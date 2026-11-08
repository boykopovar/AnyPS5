#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

constexpr int GuestEbadf = 9;
constexpr int GuestEwouldblock = 35;

constexpr int LockSh = 1;
constexpr int LockEx = 2;
constexpr int LockNb = 4;
constexpr int LockUn = 8;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "KERNEL FLOCK: %s\n", message);
        std::abort();
    }
}

} // namespace

int main() {
    Require(flock_nid_postfix(-1, LockEx) == -1 && *__error_nid_postfix() == GuestEbadf, "flock(-1, LOCK_EX) did not fail with EBADF");
    Require(flock_nid_postfix(1000000, LockSh | LockNb) == -1 && *__error_nid_postfix() == GuestEbadf, "flock on invalid fd did not fail with EBADF");

    const std::string path = (std::filesystem::temp_directory_path() / ("anyps5-flock-bounds-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin")).string();
    const int fd = open_nid_postfix(path.c_str(), 0x602, 0644);
    Require(fd >= 0, "failed to create test file");

    Require(flock_nid_postfix(fd, LockEx) == 0, "failed to apply LOCK_EX");
    Require(flock_nid_postfix(fd, LockUn) == 0, "failed to unlock LOCK_EX");

    Require(flock_nid_postfix(fd, LockSh | LockNb) == 0, "failed to apply LOCK_SH | LOCK_NB");
    Require(flock_nid_postfix(fd, LockEx | LockNb) == -1 && *__error_nid_postfix() == GuestEwouldblock, "LOCK_EX on shared locked fd did not fail with EWOULDBLOCK");
    Require(flock_nid_postfix(fd, LockUn) == 0, "failed to unlock LOCK_SH");

    Require(flock_nid_postfix(fd, LockEx) == 0, "failed to apply LOCK_EX");
    Require(flock_nid_postfix(fd, LockSh | LockNb) == -1 && *__error_nid_postfix() == GuestEwouldblock, "LOCK_SH on exclusively locked fd did not fail with EWOULDBLOCK");
    Require(flock_nid_postfix(fd, LockUn) == 0, "failed to unlock LOCK_EX");

    Require(close_nid_postfix(fd) == 0, "failed to close fd");
    Require(std::filesystem::remove(path), "failed to remove temp file");

    std::puts("guest_kernel_flock_bounds tests passed");
    return 0;
}
