#ifndef HAWK_LOG_FILE_HPP
#define HAWK_LOG_FILE_HPP

#include <hawk/core/types.hpp>
#include <hawk/platform/file_mapping.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace hawk {

struct LogMetadata {
    std::uint64_t byte_size = 0;
    std::uint64_t line_count = 0;
    LineEnding line_ending = LineEnding::UNKNOWN;
};

class LogFile {
public:
    explicit LogFile(const std::string& path);
    ~LogFile() = default;

    LogFile(const LogFile&) = delete;
    LogFile& operator=(const LogFile&) = delete;

    LogFile(LogFile&&) noexcept;
    LogFile& operator=(LogFile&&) noexcept;

public:
    const LogMetadata& metadata() const noexcept { return metadata_; }

    std::uint64_t row_count() const noexcept { return metadata_.line_count; }

    std::string_view get_line(std::uint64_t idx) const;

    std::vector<std::string> sample_lines(std::size_t max_samples) const;

private:
    enum class StorageMode { InMemory, Mapped };

private:
    void open_file();
    void detect_storage_mode();
    void load_into_memory();
    void map_file_and_build_index();
    void detect_line_ending();

private:
    std::string path_;
    StorageMode mode_ = StorageMode::Mapped;
    LogMetadata metadata_;
    std::uint64_t file_size_ = 0;
    bool has_crlf_ = false;

    // small-file InMemory mode
    std::string file_buffer_;
    std::vector<std::string_view> lines_;

    // large-file Mapped mode
    platform::FileMapping mapping_;
    std::vector<std::uint64_t> line_offsets_;
};

} // namespace hawk

#endif // HAWK_LOG_FILE_HPP
