#include <hawk/platform/file_mapping.hpp>

#ifndef _WIN32

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace hawk::platform {

void FileMapping::open(const std::string& path) {
    fd_ = ::open(path.c_str(), O_RDONLY);
    if (fd_ == -1)
        throw std::runtime_error("Failed to open file");

    struct stat st{};
    if (fstat(fd_, &st) != 0)
        throw std::runtime_error("Failed to stat file");

    size_ = static_cast<std::size_t>(st.st_size);

    data_ = static_cast<const char*>(
        mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0));

    if (data_ == MAP_FAILED)
        throw std::runtime_error("mmap failed");
}

FileMapping::~FileMapping() {
    if (data_)
        munmap((void*)data_, size_);

    if (fd_ != -1)
        close(fd_);
}

} // namespace hawk::platform

#endif // !_WIN32
