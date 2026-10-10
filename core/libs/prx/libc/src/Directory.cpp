#include "prx/libc/include/GuestDirectory.hpp"
#include "prx/libc/include/General.hpp"
#include <dirent.h>
#include <cerrno>
#include <cstring>
#include <memory>
#include <mutex>

namespace {
int DirectoryError(int error) {
    switch (error) {
    case ENOENT: return 2;
    case EACCES: return 13;
    case ENOTDIR: return 20;
    case EINVAL: return 22;
    case EMFILE: return 24;
    case ENFILE: return 23;
    case ENOMEM: return 12;
    case ENAMETOOLONG: return 63;
    default: return 5;
    }
}
#ifdef _WIN32
using NativeDirectory = _WDIR;
NativeDirectory* OpenNative(const std::filesystem::path& path) { return ::_wopendir(path.c_str()); }
int CloseNative(NativeDirectory* native) { return ::_wclosedir(native); }
void RewindNative(NativeDirectory* native) { ::_wrewinddir(native); }
bool ReadNative(NativeDirectory* native, std::filesystem::path& component, std::string& name, std::uint64_t& inode) {
    auto* entry = ::_wreaddir(native);
    if (!entry) return false;
    component = entry->d_name;
    const auto utf8 = component.u8string();
    name.assign(utf8.begin(), utf8.end());
    inode = static_cast<std::uint64_t>(entry->d_ino);
    return true;
}
#else
using NativeDirectory = DIR;
NativeDirectory* OpenNative(const std::filesystem::path& path) { return ::opendir(path.c_str()); }
int CloseNative(NativeDirectory* native) { return ::closedir(native); }
void RewindNative(NativeDirectory* native) { ::rewinddir(native); }
bool ReadNative(NativeDirectory* native, std::filesystem::path& component, std::string& name, std::uint64_t& inode) {
    auto* entry = ::readdir(native);
    if (!entry) return false;
    name = entry->d_name;
    component = name;
    inode = static_cast<std::uint64_t>(entry->d_ino);
    return true;
}
#endif
struct Directory {
    NativeDirectory* native = nullptr;
    std::filesystem::path path;
    GuestDirectoryEntry entry{};
    std::mutex mutex;
    ~Directory() { if (native) CloseNative(native); }
};
std::uint8_t DirectoryType(const std::filesystem::path& path) {
    std::error_code error;
    switch (std::filesystem::symlink_status(path, error).type()) {
    case std::filesystem::file_type::regular: return 8;
    case std::filesystem::file_type::directory: return 4;
    case std::filesystem::file_type::symlink: return 10;
    case std::filesystem::file_type::block: return 6;
    case std::filesystem::file_type::character: return 2;
    case std::filesystem::file_type::fifo: return 1;
    case std::filesystem::file_type::socket: return 12;
    default: return 0;
    }
}
}

extern "C" {
void* APS5_VABI opendir_nid_postfix(const char* path) {
    if (!path) { errno = 14; return nullptr; }
    if (!*path) { errno = 2; return nullptr; }
    try {
        auto directory = std::make_unique<Directory>();
        directory->path = ResolvePath_nid_no_patch(path);
        directory->native = OpenNative(directory->path);
        if (!directory->native) { errno = DirectoryError(errno); return nullptr; }
        return directory.release();
    } catch (const std::bad_alloc&) { errno = 12; return nullptr; }
      catch (const std::filesystem::filesystem_error&) { errno = 5; return nullptr; }
}

GuestDirectoryEntry* APS5_VABI readdir_nid_postfix(void* handle) {
    if (!handle) { errno = 9; return nullptr; }
    auto& directory = *static_cast<Directory*>(handle);
    std::lock_guard lock(directory.mutex);
    const int savedError = errno;
    errno = 0;
    std::filesystem::path component;
    std::string name;
    std::uint64_t inode = 0;
    if (!ReadNative(directory.native, component, name, inode)) {
        errno = errno ? DirectoryError(errno) : savedError;
        return nullptr;
    }
    const auto length = name.size();
    if (length > 255) { errno = 63; return nullptr; }
    try {
        directory.entry = {};
        // MinGW reports no inode identity (zero); do not fabricate one.
        directory.entry.fileNumber = inode <= UINT32_MAX ? static_cast<std::uint32_t>(inode) : 0;
        directory.entry.recordLength = static_cast<std::uint16_t>(8 + ((length + 1 + 3) & ~3));
        directory.entry.nameLength = static_cast<std::uint8_t>(length);
        directory.entry.type = DirectoryType(directory.path / component);
        std::memcpy(directory.entry.name, name.c_str(), length + 1);
        errno = savedError;
        return &directory.entry;
    } catch (const std::bad_alloc&) { errno = 12; return nullptr; }
      catch (const std::filesystem::filesystem_error&) { errno = 5; return nullptr; }
}

int APS5_VABI closedir_nid_postfix(void* handle) {
    if (!handle) { errno = 9; return -1; }
    std::unique_ptr<Directory> directory(static_cast<Directory*>(handle));
    const int result = CloseNative(directory->native);
    directory->native = nullptr;
    if (result) errno = DirectoryError(errno);
    return result;
}

void APS5_VABI rewinddir_nid_postfix(void* handle) {
    if (!handle) { errno = 9; return; }
    auto& directory = *static_cast<Directory*>(handle);
    std::lock_guard lock(directory.mutex);
    RewindNative(directory.native);
}
}
