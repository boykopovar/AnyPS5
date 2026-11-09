#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

namespace {

constexpr int GuestEbadf = 9;
constexpr int GuestEinval = 22;
constexpr int LockSh = 1;
constexpr int LockEx = 2;
constexpr int LockNb = 4;
constexpr int LockUn = 8;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FLOCK EXCLUSIVE: %s\n", message);
        std::abort();
    }
}

} // namespace

int main() {
    Require(flock_nid_postfix(-1, LockEx | LockNb) == -1 && *__error_nid_postfix() == GuestEbadf,
            "flock on invalid fd -1 did not fail with EBADF");
    Require(flock_nid_postfix(999999, LockEx | LockNb) == -1 && *__error_nid_postfix() == GuestEbadf,
            "flock on unopened descriptor did not fail with EBADF");

    Require(flock_nid_postfix(-1, 0x7fff) == -1 && *__error_nid_postfix() == GuestEinval,
            "flock with invalid operation flags did not fail with EINVAL");

    char tempName[] = "aps5_flock_test_XXXXXX";
    int fd = mkstemp(tempName);
    Require(fd >= 0, "failed to create temporary file for flock testing");

    Require(flock_nid_postfix(fd, LockEx | LockNb) == 0, "flock exclusive non-blocking failed on valid file");
    Require(flock_nid_postfix(fd, LockSh | LockNb) == 0, "flock downgrade to shared non-blocking failed");

    int fd2 = open(tempName, O_RDWR);
    Require(fd2 >= 0, "failed to open second handle to temp file");
    Require(flock_nid_postfix(fd2, LockEx | LockNb) == -1, "flock exclusive non-blocking on locked file did not fail");

    Require(flock_nid_postfix(fd, LockUn) == 0, "flock unlock failed");
    Require(flock_nid_postfix(fd2, LockEx | LockNb) == 0, "flock exclusive on now-unlocked second handle failed");

    Require(flock_nid_postfix(fd2, LockUn) == 0, "flock unlock on second handle failed");
    Require(close(fd) == 0, "close temp file failed");
    Require(close(fd2) == 0, "close second handle failed");
    unlink(tempName);

    std::puts("FLOCK EXCLUSIVE audit passed successfully");
    return 0;
}
