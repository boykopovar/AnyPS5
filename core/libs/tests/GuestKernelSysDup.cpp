#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int GuestEbadf = 9;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "KERNEL SYS DUP: %s\n", message);
        std::abort();
    }
}

template<typename TAction>
void RequireThrows(TAction action, const char* message) {
    bool threw = false;
    try {
        action();
    } catch (...) {
        threw = true;
    }
    Require(threw, message);
}

} // namespace

int main() {
    Require(_dup2_nid_postfix(-1, 0) == -1 && *__error_nid_postfix() == GuestEbadf, "_dup2(-1, 0) did not fail with EBADF");
    Require(_dup2_nid_postfix(0, -1) == -1 && *__error_nid_postfix() == GuestEbadf, "_dup2(0, -1) did not fail with EBADF");
    Require(_dup2_nid_postfix(1000000, 1000001) == -1 && *__error_nid_postfix() == GuestEbadf, "_dup2 on unopened descriptors did not fail with EBADF");

    int pipeFds1[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds1) == 0, "pipe 1 creation failed");

    int pipeFds2[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds2) == 0, "pipe 2 creation failed");

    Require(_dup2_nid_postfix(pipeFds1[0], pipeFds1[0]) == pipeFds1[0], "_dup2 to same fd failed");

    Require(_dup2_nid_postfix(-1, pipeFds2[1]) == -1 && *__error_nid_postfix() == GuestEbadf, "invalid source did not fail with EBADF");

    const char initialMessage[] = "existing_target";
    const auto initialLength = static_cast<std::int64_t>(sizeof(initialMessage));
    Require(write_nid_postfix(pipeFds2[1], initialMessage, initialLength) == initialLength, "write to existing target failed");

    char initialBuffer[32]{};
    Require(read_nid_postfix(pipeFds2[0], initialBuffer, sizeof(initialBuffer)) == initialLength, "read from pipe 2 failed");
    Require(std::memcmp(initialBuffer, initialMessage, sizeof(initialMessage)) == 0, "initial payload mismatch");

    Require(_dup2_nid_postfix(pipeFds1[1], pipeFds2[1]) == pipeFds2[1], "_dup2 replace target failed");

    Require(close_nid_postfix(pipeFds1[1]) == 0, "close pipe 1 write-end failed");

    const char message[] = "hello_dup2";
    const auto length = static_cast<std::int64_t>(sizeof(message));
    Require(write_nid_postfix(pipeFds2[1], message, length) == length, "write to surviving alias failed");

    char buffer[32]{};
    Require(read_nid_postfix(pipeFds1[0], buffer, sizeof(buffer)) == length, "read from pipe 1 read-end failed");
    Require(std::memcmp(buffer, message, sizeof(message)) == 0, "payload mismatch");

    Require(close_nid_postfix(pipeFds2[1]) == 0, "close replaced target failed");
    Require(close_nid_postfix(pipeFds2[0]) == 0, "close pipe 2 read-end failed");
    Require(close_nid_postfix(pipeFds1[0]) == 0, "close pipe 1 read-end failed");

    constexpr int SceKernelErrorEbadf = static_cast<int>(0x80020009);

    Require(sceKernelRdup(0, -1) == SceKernelErrorEbadf, "sceKernelRdup(0, -1) did not fail with EBADF");
    Require(sceKernelRdup(getpid_nid_postfix(), -1) == SceKernelErrorEbadf, "sceKernelRdup(own_pid, -1) did not fail with EBADF");
    Require(sceKernelRdup(0, 1000000) == SceKernelErrorEbadf, "sceKernelRdup on unopened descriptor did not fail with EBADF");

    int rdupPipe[2]{-1, -1};
    Require(pipe_nid_postfix(rdupPipe) == 0, "rdup pipe creation failed");
    const int rdupCopy = sceKernelRdup(0, rdupPipe[0]);
    Require(rdupCopy >= 0 && rdupCopy != rdupPipe[0], "sceKernelRdup(0, fd) failed to duplicate");
    Require(close_nid_postfix(rdupCopy) == 0, "close rdup copy failed");

    const int rdupOwnCopy = sceKernelRdup(getpid_nid_postfix(), rdupPipe[1]);
    Require(rdupOwnCopy >= 0 && rdupOwnCopy != rdupPipe[1], "sceKernelRdup(own_pid, fd) failed to duplicate");
    Require(close_nid_postfix(rdupOwnCopy) == 0, "close rdup own copy failed");

    Require(close_nid_postfix(rdupPipe[0]) == 0, "close rdup pipe 0 failed");
    Require(close_nid_postfix(rdupPipe[1]) == 0, "close rdup pipe 1 failed");

    RequireThrows([] { sceKernelRdup(1234567, 0); }, "sceKernelRdup for another process did not throw");

    Require(__sys_rdup_nid_postfix(0, -1) == -1 && *__error_nid_postfix() == GuestEbadf, "__sys_rdup(0, -1) did not fail with EBADF");
    Require(__sys_rdup_nid_postfix(getpid_nid_postfix(), -1) == -1 && *__error_nid_postfix() == GuestEbadf, "__sys_rdup(own_pid, -1) did not fail with EBADF");
    Require(__sys_rdup_nid_postfix(0, 1000000) == -1 && *__error_nid_postfix() == GuestEbadf, "__sys_rdup on unopened descriptor did not fail with EBADF");
    RequireThrows([] { __sys_rdup_nid_postfix(1234567, 0); }, "__sys_rdup for another process did not throw");

    int sysRdupPipe[2]{-1, -1};
    Require(pipe_nid_postfix(sysRdupPipe) == 0, "sys_rdup pipe creation failed");
    const int sysRdupCopy = __sys_rdup_nid_postfix(0, sysRdupPipe[0]);
    Require(sysRdupCopy >= 0 && sysRdupCopy != sysRdupPipe[0], "__sys_rdup(0, fd) failed to duplicate");
    const char sysRdupMsg[] = "sys_rdup_payload";
    const auto sysRdupLen = static_cast<std::int64_t>(sizeof(sysRdupMsg));
    Require(write_nid_postfix(sysRdupPipe[1], sysRdupMsg, sysRdupLen) == sysRdupLen, "write to pipe failed");
    char sysRdupBuf[32]{};
    Require(read_nid_postfix(sysRdupCopy, sysRdupBuf, sizeof(sysRdupBuf)) == sysRdupLen, "read from sys_rdup copy failed");
    Require(std::memcmp(sysRdupBuf, sysRdupMsg, sizeof(sysRdupMsg)) == 0, "sys_rdup payload mismatch");

    Require(close_nid_postfix(sysRdupCopy) == 0, "close sys_rdup copy failed");
    Require(close_nid_postfix(sysRdupPipe[0]) == 0, "close sys_rdup pipe 0 failed");
    Require(close_nid_postfix(sysRdupPipe[1]) == 0, "close sys_rdup pipe 1 failed");

    std::puts("KERNEL SYS DUP tests passed");
    return 0;
}
