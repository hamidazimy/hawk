#include <hawk/core/log_file.hpp>

#include <hawk/core/types.hpp>
#include <hawk/platform/file_mapping.hpp>

#include <stdexcept>
#include <utility>

namespace hawk {

LogFile::LogFile(const std::string& path)
    : path_(path)
{
    mapping_.open(path_);
    file_size_ = mapping_.size();
    if (detect_utf8_bom()) {
        bom_offset_ = 3;
    }
    has_crlf_ = detect_crlf();
}

LogFile::LogFile(LogFile&& other) noexcept
    : path_(std::move(other.path_))
    , mapping_(std::move(other.mapping_))
    , file_size_(other.file_size_)
    , bom_offset_(other.bom_offset_)
    , has_crlf_(other.has_crlf_)
{
    other.file_size_ = 0;
    other.bom_offset_ = 0;
    other.has_crlf_ = false;
}

LogFile& LogFile::operator=(LogFile&& other) noexcept {
    if (this == &other)
        return *this;

    path_ = std::move(other.path_);
    mapping_ = std::move(other.mapping_);
    file_size_ = other.file_size_;
    bom_offset_ = other.bom_offset_;
    has_crlf_ = other.has_crlf_;

    other.file_size_ = 0;
    other.bom_offset_ = 0;
    other.has_crlf_ = false;

    return *this;
}

bool LogFile::detect_utf8_bom() const noexcept {
    const char* raw_data = mapping_.data();
    if (file_size_ >= 3
        && static_cast<unsigned char>(raw_data[0]) == 0xEF
        && static_cast<unsigned char>(raw_data[1]) == 0xBB
        && static_cast<unsigned char>(raw_data[2]) == 0xBF
    )
    {
        return true;
    }
    return false;
}

bool LogFile::detect_crlf() const noexcept {
    const char* raw_data = mapping_.data();
    for (FileOffset i = 1; i < file_size_; ++i) {
        if (raw_data[i] == '\n') {
            if (raw_data[i - 1] == '\r')
                return true;
            break;
        }
    }
    return false;
}

std::string_view LogFile::bytes() const noexcept {
    if (size() == 0)
        return {};
    return std::string_view(mapping_.data() + bom_offset_, size());
}

std::string_view LogFile::read_bytes(FileOffset start, FileOffset length) const {
    const char* data = mapping_.data() + bom_offset_;
    if (start > size() || length > size() - start)
        throw std::out_of_range("LogFile::read_bytes out of range");
    return std::string_view(data + start, length);
}

} // namespace hawk
