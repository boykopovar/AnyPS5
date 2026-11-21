#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorInvalidArgument = 22;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno=%d)\n", message, *__error_nid_postfix());
    std::abort();
}

void TestMmapFlags() {
    *__error_nid_postfix() = 0;
    void* ptrZeroLen = mmap_nid_postfix(nullptr, 0, 0x1, 0x0002, -1, 0);
    Require(ptrZeroLen == reinterpret_cast<void*>(-1), "mmap with zero len must fail");
    Require(*__error_nid_postfix() == ErrorInvalidArgument, "mmap with zero len must set EINVAL");

    *__error_nid_postfix() = 0;
    void* ptrBadFd = mmap_nid_postfix(nullptr, 4096, 0x1, 0x0002, -1, 0);
    Require(ptrBadFd == reinterpret_cast<void*>(-1), "mmap with anonymous/invalid fd and MAP_SHARED must fail");
    Require(*__error_nid_postfix() == ErrorBadDescriptor || *__error_nid_postfix() == ErrorInvalidArgument, "mmap bad fd must fail with EBADF or EINVAL");

    const std::string path = "anyps5_mmap_test_" + std::to_string(getpid_nid_postfix()) + ".tmp";
    const int fd = open_nid_disambig1_nid_postfix(path.c_str(), 0x0002 | 0x0200 | 0x0400, 0600);
    Require(fd >= 0, "open test file failed");
    Require(ftruncate_nid_postfix(fd, 4096) == 0, "ftruncate test file failed");

    *__error_nid_postfix() = 0;
    void* ptrInvalidFlags = mmap_nid_postfix(nullptr, 4096, 0x1, 0x0000, fd, 0);
    Require(ptrInvalidFlags == reinterpret_cast<void*>(-1), "mmap without MAP_SHARED or MAP_PRIVATE must fail");
    Require(*__error_nid_postfix() == ErrorInvalidArgument, "mmap without sharing flags must set EINVAL");

    void* ptrShared = mmap_nid_postfix(nullptr, 4096, 0x1 | 0x2, 0x0001, fd, 0);
    Require(ptrShared != reinterpret_cast<void*>(-1) && ptrShared != nullptr, "mmap MAP_SHARED failed");
    Require(munmap_nid_postfix(ptrShared, 4096) == 0, "munmap MAP_SHARED failed");

    void* ptrPrivate = mmap_nid_postfix(nullptr, 4096, 0x1 | 0x2, 0x0002, fd, 0);
    Require(ptrPrivate != reinterpret_cast<void*>(-1) && ptrPrivate != nullptr, "mmap MAP_PRIVATE failed");
    Require(munmap_nid_postfix(ptrPrivate, 4096) == 0, "munmap MAP_PRIVATE failed");

    Require(close_nid_postfix(fd) == 0, "close test file failed");
    Require(unlink_nid_postfix(path.c_str()) == 0, "unlink test file failed");
}

} // namespace

int main() {
    TestMmapFlags();
    return 0;
}
