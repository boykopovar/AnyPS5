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
constexpr int ErrorBig = 27;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

std::string MakeTestPath() {
    return "anyps5_fallocate_test_" + std::to_string(getpid_nid_postfix()) + ".tmp";
}

void TestFallocateBoundaries() {
    const std::string path = MakeTestPath();
    const int fd = open_nid_postfix(path.c_str(), 0x0002 | 0x0200 | 0x0400, 0600);
    Require(fd >= 0, "open test file failed");

    Require(posix_fallocate_nid_postfix(-1, 0, 1024) == ErrorBadDescriptor, "posix_fallocate with invalid fd must return EBADF");
    Require(posix_fallocate_nid_postfix(fd, -1, 1024) == ErrorInvalidArgument, "posix_fallocate with negative offset must return EINVAL");
    Require(posix_fallocate_nid_postfix(fd, 0, 0) == ErrorInvalidArgument, "posix_fallocate with zero length must return EINVAL");
    Require(posix_fallocate_nid_postfix(fd, 0, -1024) == ErrorInvalidArgument, "posix_fallocate with negative length must return EINVAL");

    const std::int64_t maxOff = 0x7FFFFFFFFFFFFFFFLL;
    Require(posix_fallocate_nid_postfix(fd, maxOff, 1024) == ErrorBig, "posix_fallocate with out of range offset must return EFBIG");

    Require(posix_fallocate_nid_postfix(fd, 0, 4096) == 0, "posix_fallocate valid range failed");

    Require(close_nid_postfix(fd) == 0, "close test file failed");
    Require(unlink_nid_postfix(path.c_str()) == 0, "unlink test file failed");
}

} // namespace

int main() {
    TestFallocateBoundaries();
    return 0;
}
