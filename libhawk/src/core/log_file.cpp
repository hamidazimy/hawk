#include <hawk/core/log_file.hpp>

#include <fstream>
#include <stdexcept>
#include <algorithm>

namespace hawk {

namespace {

constexpr std::uint64_t MEMORY_THRESHOLD = 64ull * 1024ull * 1024ull; // 64 MB

}

LogFile::LogFile(const std::string& path)
    : path_(path)
{
    open_file();
    detect_storage_mode();

    if (mode_ == StorageMode::InMemory) {
        load_into_memory();
    } else {
        map_file();
        build_index();
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

    lines_ = std::move(other.lines_);
    line_offsets_ = std::move(other.line_offsets_);

    mapping_ = std::move(other.mapping_);

    data_ = other.data_;
    file_size_ = other.file_size_;

    other.data_ = nullptr;
    other.file_size_ = 0;

    return *this;
}

std::string_view LogFile::get_line(std::uint64_t idx) const {
    if (idx >= metadata_.line_count)
        throw std::out_of_range("LogFile line index out of range");

    if (mode_ == StorageMode::InMemory)
        return lines_[idx];

    std::uint64_t start = line_offsets_[idx];

    std::uint64_t end;
    if (idx + 1 < line_offsets_.size())
        end = line_offsets_[idx + 1];
    else
        end = file_size_;

    if (end > start && data_[end - 1] == '\n')
        --end;

    if (metadata_.line_ending == LineEnding::CRLF &&
        end > start && data_[end - 1] == '\r')
        --end;

    return std::string_view(data_ + start, end - start);
}

std::vector<std::string> LogFile::sample_lines(std::size_t max_samples) const {
    std::vector<std::string> samples;
    samples.reserve(std::min(max_samples, static_cast<std::size_t>(metadata_.line_count)));

    for (std::size_t i = 0; i < samples.capacity(); ++i) {
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
    std::ifstream file(path_, std::ios::binary);

    if (!file)
        throw std::runtime_error("Failed to open log file");

    std::string buffer;
    buffer.resize(file_size_);

    file.read(buffer.data(), file_size_);

    std::size_t start = 0;

    for (std::size_t i = 0; i < buffer.size(); ++i) {
        if (buffer[i] == '\n') {
            std::size_t len = i - start;

            if (len && buffer[start + len - 1] == '\r')
                --len;

            lines_.emplace_back(buffer.data() + start, len);
            start = i + 1;
        }
    }

    if (start < buffer.size()) {
        lines_.emplace_back(buffer.data() + start,
                            buffer.size() - start);
    }

    metadata_.line_count = lines_.size();
}

void LogFile::map_file() {
    mapping_.open(path_);

    data_ = mapping_.data();
    file_size_ = mapping_.size();
}

void LogFile::build_index() {
    line_offsets_.clear();
    line_offsets_.reserve(file_size_ / 40); // heuristic

    line_offsets_.push_back(0);

    for (std::uint64_t i = 0; i < file_size_; ++i) {
        if (data_[i] == '\n') {
            std::uint64_t next = i + 1;
            if (next < file_size_)
                line_offsets_.push_back(next);
        }
    }

    metadata_.line_count = line_offsets_.size();
}

void LogFile::detect_line_ending() {
    if (mode_ == StorageMode::InMemory) {

        for (const auto& line : lines_) {
            if (!line.empty() && line.back() == '\r') {
                metadata_.line_ending = LineEnding::CRLF;
                return;
            }
        }

        metadata_.line_ending = LineEnding::LF;
        return;
    }

    for (std::uint64_t i = 0; i < file_size_; ++i) {
        if (data_[i] == '\n') {

            if (i > 0 && data_[i - 1] == '\r')
                metadata_.line_ending = LineEnding::CRLF;
            else
                metadata_.line_ending = LineEnding::LF;

            return;
        }
    }

    metadata_.line_ending = LineEnding::UNKNOWN;
}

} // namespace hawk
