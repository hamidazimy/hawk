#include <hawk/platform/file_mapping.hpp>

#ifdef _WIN32

#include <windows.h>

#include <cstddef>
#include <stdexcept>

namespace hawk::platform {

void FileMapping::open(const std::string& path) {
    file_ = CreateFileA(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file_ == INVALID_HANDLE_VALUE)
        throw std::runtime_error("CreateFile failed");

    LARGE_INTEGER size;
    if (!GetFileSizeEx(file_, &size))
        throw std::runtime_error("GetFileSizeEx failed");

    size_ = static_cast<std::size_t>(size.QuadPart);

    mapping_ = CreateFileMappingA(
        file_,
        nullptr,
        PAGE_READONLY,
        0,
        0,
        nullptr);

    if (!mapping_)
        throw std::runtime_error("CreateFileMapping failed");

    data_ = static_cast<const char*>(
        MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0));

    if (!data_)
        throw std::runtime_error("MapViewOfFile failed");
}

FileMapping::~FileMapping() {
    if (data_)
        UnmapViewOfFile(data_);

    if (mapping_)
        CloseHandle(mapping_);

    if (file_ && file_ != INVALID_HANDLE_VALUE)
        CloseHandle(file_);
}

} // namespace hawk::platform

#endif // _WIN32
