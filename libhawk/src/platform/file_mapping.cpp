#include <hawk/platform/file_mapping.hpp>

#include <utility>

namespace hawk::platform {

FileMapping::FileMapping(FileMapping&& other) noexcept {
    *this = std::move(other);
}

FileMapping& FileMapping::operator=(FileMapping&& other) noexcept {
    if (this == &other)
        return *this;

    this->~FileMapping();

    data_ = other.data_;
    size_ = other.size_;

#ifdef _WIN32
    file_ = other.file_;
    mapping_ = other.mapping_;

    other.file_ = nullptr;
    other.mapping_ = nullptr;
#else
    fd_ = other.fd_;
    other.fd_ = -1;
#endif

    other.data_ = nullptr;
    other.size_ = 0;

    return *this;
}

} // namespace hawk::platform
