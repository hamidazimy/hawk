#include <hawk/core/log_file.hpp>

#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace hawk {

namespace {

#ifdef TEST_BUILD
constexpr std::uint64_t MEMORY_THRESHOLD = 1024ull; // 1 KB for testing
#else
constexpr std::uint64_t MEMORY_THRESHOLD = 64ull * 1024ull * 1024ull; // 64 MB
#endif

}

LogFile::LogFile(const std::string& path)
    : path_(path)
{
    open_file();
    detect_storage_mode();

    if (mode_ == StorageMode::InMemory) {
        load_into_memory();
    } else {
        map_file_and_build_index();
    }

    detect_line_ending();
}

LogFile::LogFile(LogFile&& other) noexcept {
    *this = std::move(other);
}

LogFile& LogFile::operator=(LogFile&& other) noexcept {
    if (this == &other)
        return *this;

    path_ = std::move(other.path_);
    mode_ = other.mode_;
    metadata_ = other.metadata_;
    has_crlf_ = other.has_crlf_;

    file_buffer_ = std::move(other.file_buffer_);
    lines_ = std::move(other.lines_);

    mapping_ = std::move(other.mapping_);
    line_offsets_ = std::move(other.line_offsets_);

    if (mode_ == StorageMode::Mapped)
        file_size_ = mapping_.size();
    else
        file_size_ = other.file_size_;

    other.file_size_ = 0;

    return *this;
}

std::string_view LogFile::get_line(std::uint64_t idx) const {
    if (mode_ == StorageMode::InMemory) return lines_[idx];

    const char* data = mapping_.data();
    const auto start = line_offsets_[idx];

    std::uint64_t end = (idx + 1 < line_offsets_.size()) ? line_offsets_[idx + 1] : file_size_;

    // Skip newline and optional CR
    if (end > start) {
        --end; // skip '\n'
        if (has_crlf_ && end > start && data[end - 1] == '\r')
            --end;
    }

    return std::string_view(data + start, end - start);
}

std::vector<std::string> LogFile::sample_lines(std::size_t max_samples) const {
    std::size_t count = std::min(max_samples, static_cast<std::size_t>(metadata_.line_count));

    std::vector<std::string> samples;
    samples.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        samples.emplace_back(get_line(i));
    }

    return samples;
}

void LogFile::open_file() {
    std::ifstream file(path_, std::ios::binary | std::ios::ate);

    if (!file)
        throw std::runtime_error("Failed to open log file");

    file_size_ = static_cast<std::uint64_t>(file.tellg());
    metadata_.byte_size = file_size_;
}

void LogFile::detect_storage_mode() {
    if (file_size_ <= MEMORY_THRESHOLD)
        mode_ = StorageMode::InMemory;
    else
        mode_ = StorageMode::Mapped;
}

void LogFile::load_into_memory() {
   std::ifstream file(path_, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Failed to open log file");

    file_size_ = static_cast<std::uint64_t>(file.tellg());

    file_buffer_.resize(file_size_);

    file.seekg(0);
    if (!file.read(file_buffer_.data(), file_size_))
        throw std::runtime_error("Failed to read log file");

    lines_.clear();
    lines_.reserve(file_size_ / 80);

    std::size_t start = 0;

    if (file_buffer_.size() >= 3 &&
        static_cast<unsigned char>(file_buffer_[0]) == 0xEF &&
        static_cast<unsigned char>(file_buffer_[1]) == 0xBB &&
        static_cast<unsigned char>(file_buffer_[2]) == 0xBF)
    {
        start = 3;
    }

    for (std::size_t i = start; i < file_buffer_.size(); ++i) {
        if (file_buffer_[i] == '\n') {
            std::size_t len = i - start;

            if (len && file_buffer_[start + len - 1] == '\r')
                --len;

            lines_.emplace_back(file_buffer_.data() + start, len);

            start = i + 1;
        }
    }

    if (start < file_buffer_.size()) {
        lines_.emplace_back(
            file_buffer_.data() + start,
            file_buffer_.size() - start);
    }

    metadata_.line_count = lines_.size();
}

void LogFile::map_file_and_build_index() {
    mapping_.open(path_);

    file_size_ = mapping_.size();

    if (file_size_ == 0) {
        metadata_.line_count = 0;
        return;
    }

    const char* data = mapping_.data();
    const char* end  = data + file_size_;

    std::size_t start_offset = 0;

    if (file_size_ >= 3 &&
        static_cast<unsigned char>(data[0]) == 0xEF &&
        static_cast<unsigned char>(data[1]) == 0xBB &&
        static_cast<unsigned char>(data[2]) == 0xBF)
    {
        start_offset = 3;
    }

    line_offsets_.clear();
    line_offsets_.reserve(file_size_ / 80);

    line_offsets_.push_back(start_offset);

    const char* p = data + start_offset;

    while (p < end) {
        const void* pos = std::memchr(p, '\n',
            static_cast<std::size_t>(end - p));

        if (!pos)
            break;

        const char* newline = static_cast<const char*>(pos);

        std::uint64_t offset =
            static_cast<std::uint64_t>(newline - data);

        std::uint64_t next = offset + 1;

        if (next < file_size_)
            line_offsets_.push_back(next);

        p = newline + 1;
    }

    metadata_.line_count = line_offsets_.size();
}

void LogFile::detect_line_ending() {
    if (mode_ == StorageMode::InMemory) {

        for (const auto& line : lines_) {
            if (!line.empty() && line.back() == '\r') {
                metadata_.line_ending = LineEnding::CRLF;
                has_crlf_ = true;
                return;
            }
        }

        metadata_.line_ending = LineEnding::LF;
        return;
    }

    const char* data = mapping_.data();

    for (std::uint64_t i = 0; i < file_size_; ++i) {
        if (data[i] == '\n') {

            if (i > 0 && data[i - 1] == '\r') {
                metadata_.line_ending = LineEnding::CRLF;
                has_crlf_ = true;
            }
            else {
                metadata_.line_ending = LineEnding::LF;
            }

            return;
        }
    }

    metadata_.line_ending = LineEnding::UNKNOWN;
}

} // namespace hawk
