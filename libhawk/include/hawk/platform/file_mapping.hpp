#ifndef HAWK_PLATFORM_FILE_MAPPING_HPP
#define HAWK_PLATFORM_FILE_MAPPING_HPP

#include <cstdint>
#include <string>

namespace hawk::platform {

class FileMapping {
public:
    FileMapping() = default;
    ~FileMapping();

    FileMapping(const FileMapping&) = delete;
    FileMapping& operator=(const FileMapping&) = delete;

    FileMapping(FileMapping&&) noexcept;
    FileMapping& operator=(FileMapping&&) noexcept;

    void open(const std::string& path);

    const char* data() const { return data_; }
    std::uint64_t size() const { return size_; }

private:
    const char* data_ = nullptr;
    std::uint64_t size_ = 0;

#ifdef _WIN32
    void* file_ = nullptr;
    void* mapping_ = nullptr;
#else
    int fd_ = -1;
#endif
};

} // namespace hawk::platform

#endif // HAWK_PLATFORM_FILE_MAPPING_HPP
