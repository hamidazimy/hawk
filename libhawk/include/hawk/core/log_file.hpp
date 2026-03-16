#ifndef HAWK_LOG_FILE_HPP
#define HAWK_LOG_FILE_HPP

#include <hawk/core/types.hpp>
#include <hawk/platform/file_mapping.hpp>

#include <string>
#include <string_view>

namespace hawk {

class LogFile {
public:
    explicit LogFile(const std::string& path);

    LogFile(const LogFile&) = delete;
    LogFile& operator=(const LogFile&) = delete;

    LogFile(LogFile&&) noexcept;
    LogFile& operator=(LogFile&&) noexcept;

    FileOffset size() const noexcept { return file_size_ - bom_offset_; }
    bool has_crlf() const noexcept { return has_crlf_; }

    std::string_view bytes() const noexcept;
    std::string_view read_bytes(FileOffset start, FileOffset length) const;

private:
    bool detect_utf8_bom() const noexcept;
    bool detect_crlf() const noexcept;

private:
    std::string path_;
    platform::FileMapping mapping_;
    FileOffset file_size_ = 0;   // physical size
    FileOffset bom_offset_ = 0;
    bool has_crlf_ = false;
};

} // namespace hawk

#endif // HAWK_LOG_FILE_HPP
