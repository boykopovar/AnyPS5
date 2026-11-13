#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/GuestDirectory.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>

extern "C" void* APS5_VABI fdopendir_nid_postfix(int fd) {
    constexpr int SetDescriptorFlags = 2;
    constexpr int CloseOnExec = 1;
    if (fcntl_nid_postfix(fd, SetDescriptorFlags, CloseOnExec) == -1) return nullptr;
    int error = 0;
    const char* path = GetDirectoryDescriptorPath_nid_no_patch(fd, &error);
    if (path == nullptr) {
        *__error_nid_postfix() = error == EBADF ? 9 : 20;
        return nullptr;
    }
    const bool virtualDirectory = IsDirectoryDescriptor_nid_no_patch(fd) != 0;
    return OpenDescriptorDirectory_nid_no_patch(path, fd, close_nid_postfix, GetDirectoryDescriptorPosition_nid_no_patch(fd),
        virtualDirectory ? ReadDirectoryDescriptorCallback_nid_no_patch : nullptr,
        virtualDirectory ? SeekDirectoryDescriptorCallback_nid_no_patch : nullptr);
}

namespace {

int APS5_VABI EmptyClose(int) { return 0; }
int APS5_VABI EmptyRead(int, char*, int) { return 0; }
std::int64_t APS5_VABI EmptySeek(int, std::int64_t, int) { return 0; }

}

extern "C" void* APS5_VABI __opendir2_nid_postfix(const char* name, int flags) {
    constexpr int ReadAll = 0x8;
    void* directory = opendir_nid_postfix(name);
    if (directory == nullptr || (flags & ReadAll) == 0) return directory;
    closedir_nid_postfix(directory);
    return OpenDescriptorDirectory_nid_no_patch(name, -1, EmptyClose, 0, EmptyRead, EmptySeek);
}

extern "C" GuestDirectoryEntry* APS5_VABI _readdir_unlocked_nid_postfix(void* handle, int skip) {
    return ReadDirectoryUnlocked_nid_no_patch(handle, skip);
}

extern "C" void APS5_VABI _seekdir_nid_postfix(void* handle, std::int64_t location) {
    SeekDirectoryUnlocked_nid_no_patch(handle, location);
}
