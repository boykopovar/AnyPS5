#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>

extern "C" {
int APS5_VABI dup_nid_postfix(int);
int APS5_VABI pipe_nid_postfix(int*);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI sceKernelClose(int);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value) { if (!value) std::abort(); }

constexpr int GuestEperm = 1;
constexpr int GuestEbadf = 9;
constexpr int SceKernelErrorEperm = static_cast<int>(0x80020001);
constexpr int SceKernelErrorEbadf = static_cast<int>(0x80020009);

static bool Open(int fd) {
    const int duplicate = dup_nid_postfix(fd);
    if (duplicate < 0) return false;
    Require(close_nid_postfix(duplicate) == 0);
    return true;
}

int main() {
    bool open[3];
    for (int fd = 0; fd < 3; ++fd) open[fd] = Open(fd);
    for (int fd = 0; fd < 3; ++fd) {
        if (!open[fd]) continue;
        Require(close_nid_postfix(fd) == -1 && *__error_nid_postfix() == GuestEperm);
        Require(sceKernelClose(fd) == SceKernelErrorEperm);
        Require(Open(fd));
    }
    int pipe[2];
    Require(pipe_nid_postfix(pipe) == 0);
    for (const int fd : pipe) Require(fd >= 3 || !open[fd]);
    Require(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0);
    Require(close_nid_postfix(pipe[0]) == -1 && *__error_nid_postfix() == GuestEbadf);
    Require(sceKernelClose(pipe[1]) == SceKernelErrorEbadf);
    Require(close_nid_postfix(-1) == -1 && *__error_nid_postfix() == GuestEbadf);
    Require(sceKernelClose(-1) == SceKernelErrorEbadf);
    return 0;
}
