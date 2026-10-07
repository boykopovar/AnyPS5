#include <pkg/PackageFile.hpp>
#include <pkg/PackageError.hpp>
#include <algorithm>
#include <array>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Pkg {

PackageFile::PackageFile(const std::filesystem::path& path) : _path(path) {
#ifdef _WIN32
    const HANDLE handle = CreateFileW(path.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) throw PackageError("Cannot open package: " + path.string());
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle, &size)) {
        CloseHandle(handle);
        throw PackageError("Cannot read package size: " + path.string());
    }
    _handle = reinterpret_cast<std::intptr_t>(handle);
    _size = static_cast<std::uint64_t>(size.QuadPart);
#else
    const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) throw PackageError("Cannot open package: " + path.string());
    struct stat status{};
    if (::fstat(descriptor, &status) != 0) {
        ::close(descriptor);
        throw PackageError("Cannot read package size: " + path.string());
    }
    _handle = descriptor;
    _size = static_cast<std::uint64_t>(status.st_size);
#endif
}

PackageFile::~PackageFile() {
#ifdef _WIN32
    CloseHandle(reinterpret_cast<HANDLE>(_handle));
#else
    ::close(static_cast<int>(_handle));
#endif
}

std::uint64_t PackageFile::Size() const {
    return _size;
}

void PackageFile::Read(std::uint64_t offset, std::uint8_t* destination, std::size_t size) const {
    if (offset > _size || size > _size - offset) throw PackageError("Package read outside the file", offset);
    while (size > 0) {
        const std::size_t request = std::min<std::size_t>(size, 0x40000000);
#ifdef _WIN32
        OVERLAPPED position{};
        position.Offset = static_cast<DWORD>(offset);
        position.OffsetHigh = static_cast<DWORD>(offset >> 32);
        DWORD transferred = 0;
        if (!ReadFile(reinterpret_cast<HANDLE>(_handle), destination, static_cast<DWORD>(request), &transferred, &position) || transferred == 0)
            throw PackageError("Cannot read package: " + _path.string(), offset);
        const std::size_t count = transferred;
#else
        const ssize_t transferred = ::pread(static_cast<int>(_handle), destination, request, static_cast<off_t>(offset));
        if (transferred <= 0) throw PackageError("Cannot read package: " + _path.string(), offset);
        const std::size_t count = static_cast<std::size_t>(transferred);
#endif
        offset += count;
        destination += count;
        size -= count;
    }
}

std::vector<std::uint8_t> PackageFile::Read(const std::uint64_t offset, const std::size_t size) const {
    std::vector<std::uint8_t> bytes(size);
    Read(offset, bytes.data(), size);
    return bytes;
}

bool IsFinalizedPackage(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::array<char, 4> magic{};
    if (!stream || !stream.read(magic.data(), magic.size())) return false;
    return static_cast<unsigned char>(magic[0]) == 0x7f && magic[1] == 'F' && magic[2] == 'I' && magic[3] == 'H';
}

}
