#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/GuestDirectory.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
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
    return OpenDescriptorDirectory_nid_no_patch(path, fd, close_nid_postfix, GetDirectoryDescriptorPosition_nid_no_patch(fd));
}
