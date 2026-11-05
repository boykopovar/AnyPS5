#include "prx/libkernel/File/include/FileLock.hpp"
#include "prx/libc/include/FileLock.hpp"

#ifdef _WIN32

namespace File {

int Flock(int fd, int operation) {
    return GuestFileLock_nid_no_patch(fd, operation);
}

void ForgetFileLock(int fd) {
    GuestFileLockForget_nid_no_patch(fd);
}

}

#endif
