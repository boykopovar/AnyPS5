#include "prx/libkernel_web/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int GuestEnoent = 2;
constexpr int GuestEbadf = 9;
constexpr int GuestEfault = 14;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "KERNEL WEB FORWARDS: %s\n", message);
        std::abort();
    }
}

void RequireFailure(int result, int error, const char* message) {
    Require(result == -1 && *__error_nid_postfix() == error, message);
}

void TestDup2() {
    RequireFailure(_dup2_nid_disambig1_nid_postfix(-1, 0), GuestEbadf, "_dup2(-1, 0) did not fail with EBADF");
    RequireFailure(_dup2_nid_disambig1_nid_postfix(1000000, 1000001), GuestEbadf, "_dup2 on unopened descriptors did not fail with EBADF");

    int first[2]{-1, -1};
    int second[2]{-1, -1};
    Require(pipe_nid_postfix(first) == 0 && pipe_nid_postfix(second) == 0, "pipe creation failed");
    Require(_dup2_nid_disambig1_nid_postfix(first[0], first[0]) == first[0], "_dup2 to the same descriptor failed");
    Require(_dup2_nid_disambig1_nid_postfix(first[1], second[1]) == second[1], "_dup2 replacing a descriptor failed");
    Require(close_nid_postfix(first[1]) == 0, "closing the original write end failed");

    const char message[] = "kernel_web_dup2";
    const auto length = static_cast<std::int64_t>(sizeof(message));
    Require(write_nid_postfix(second[1], message, length) == length, "write through the duplicate failed");
    char buffer[32]{};
    Require(read_nid_postfix(first[0], buffer, sizeof(buffer)) == length, "read from the original read end failed");
    Require(std::memcmp(buffer, message, sizeof(message)) == 0, "payload mismatch");

    Require(close_nid_postfix(second[1]) == 0 && close_nid_postfix(second[0]) == 0 && close_nid_postfix(first[0]) == 0,
            "closing the pipes failed");
}

void TestExec() {
    char argument[] = "kernel_web_missing.bin";
    char* argv[] = {argument, nullptr};
    char* const emptyArgv[] = {nullptr};
    RequireFailure(_execve_nid_disambig1_nid_postfix("", nullptr, nullptr), GuestEfault, "_execve with a null argv did not fail with EFAULT");
    RequireFailure(_execve_nid_disambig1_nid_postfix("", emptyArgv, nullptr), GuestEnoent, "_execve with an empty path did not fail with ENOENT");
    RequireFailure(_execve_nid_disambig1_nid_postfix("kernel_web_missing.bin", argv, nullptr), GuestEnoent,
                   "_execve of a missing file did not fail with ENOENT");
    RequireFailure(_execvpe_nid_disambig1_nid_postfix(nullptr, argv, nullptr), GuestEfault, "_execvpe with a null name did not fail with EFAULT");
    RequireFailure(_execvpe_nid_disambig1_nid_postfix("", argv, nullptr), GuestEnoent, "_execvpe with an empty name did not fail with ENOENT");
    RequireFailure(_execvpe_nid_disambig1_nid_postfix("kernel_web_absent.bin", argv, nullptr), GuestEnoent,
                   "_execvpe of a missing program did not fail with ENOENT");
}

} // namespace

int main() {
    TestDup2();
    TestExec();
    return 0;
}
