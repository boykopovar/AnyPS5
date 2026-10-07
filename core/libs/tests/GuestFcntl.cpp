#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
extern "C" {
int APS5_VABI sceKernelOpen(const char* path, int flags, std::uint16_t mode);
int APS5_VABI sceKernelClose(int d);
std::int64_t APS5_VABI sceKernelWrite(int d, const void* buf, std::size_t nbytes);
int APS5_VABI sceKernelFcntl(int d, int cmd, std::intptr_t arg);
int APS5_VABI open_nid_postfix(const char* path, int flags, int mode);
int APS5_VABI close_nid_postfix(int d);
int APS5_VABI pipe_nid_postfix(int* descriptors);
int APS5_VABI socket_nid_postfix(int family, int type, int protocol);
}
static void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Fcntl check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)
static bool Throws(int d, int cmd, std::intptr_t arg) {
    try { sceKernelFcntl(d, cmd, arg); } catch (const std::runtime_error&) { return true; }
    return false;
}
int main() {
    constexpr int GetFd = 1;
    constexpr int SetFd = 2;
    constexpr int GetFl = 3;
    constexpr int SetFl = 4;
    const auto root = std::filesystem::path("anyps5-fcntl-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(std::filesystem::create_directory(root));
    const auto file = (root / "data.bin").string();
    { std::ofstream stream(file, std::ios::binary); stream << "0123"; }

    const int fd = sceKernelOpen(file.c_str(), SCE_KERNEL_O_RDWR | SCE_KERNEL_O_APPEND | SCE_KERNEL_O_CREAT, 0666);
    Check(fd >= 0);
    Check(sceKernelFcntl(fd, GetFd, 0) == 0);
    Check(sceKernelFcntl(fd, GetFl, 0) == (SCE_KERNEL_O_RDWR | SCE_KERNEL_O_APPEND));
    Check(sceKernelFcntl(fd, SetFd, 1) == 0);
    Check(sceKernelFcntl(fd, GetFd, 0) == 1);
    Check(sceKernelFcntl(fd, SetFd, 6) == 0);
    Check(sceKernelFcntl(fd, GetFd, 0) == 0);
    Check(sceKernelFcntl(fd, SetFl, SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_APPEND | SCE_KERNEL_O_NONBLOCK | SCE_KERNEL_O_TRUNC) == 0);
    Check(sceKernelFcntl(fd, GetFl, 0) == (SCE_KERNEL_O_RDWR | SCE_KERNEL_O_APPEND | SCE_KERNEL_O_NONBLOCK));
    Check(sceKernelFcntl(fd, SetFl, SCE_KERNEL_O_APPEND | SCE_KERNEL_O_DIRECT) == 0);
    Check(sceKernelFcntl(fd, GetFl, 0) == (SCE_KERNEL_O_RDWR | SCE_KERNEL_O_APPEND | SCE_KERNEL_O_DIRECT));
    Check(Throws(fd, SetFl, 0));
    Check(Throws(fd, SetFl, SCE_KERNEL_O_APPEND | SCE_KERNEL_O_FSYNC));
    Check(Throws(fd, SetFl, SCE_KERNEL_O_APPEND | SCE_KERNEL_O_ASYNC));
    Check(sceKernelFcntl(fd, GetFl, 0) == (SCE_KERNEL_O_RDWR | SCE_KERNEL_O_APPEND | SCE_KERNEL_O_DIRECT));
    Check(Throws(fd, 0, 0));
    Check(Throws(fd, 11, 0));
    Check(sceKernelWrite(fd, "45", 2) == 2);
    Check(sceKernelClose(fd) == 0);
    Check(std::filesystem::file_size(file) == 6);
    Check(Throws(fd, GetFl, 0));

    const int closeOnExec = sceKernelOpen(file.c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_CLOEXEC, 0);
    Check(closeOnExec >= 0);
    Check(sceKernelFcntl(closeOnExec, GetFd, 0) == 1);
    Check(sceKernelFcntl(closeOnExec, GetFl, 0) == SCE_KERNEL_O_RDONLY);
    Check(sceKernelClose(closeOnExec) == 0);

    const int posix = open_nid_postfix(file.c_str(), SCE_KERNEL_O_WRONLY, 0);
    Check(posix >= 0);
    Check(sceKernelFcntl(posix, GetFl, 0) == SCE_KERNEL_O_WRONLY);
    Check(close_nid_postfix(posix) == 0);
    Check(Throws(posix, GetFl, 0));

    const int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Check(directory >= 0);
    Check(sceKernelFcntl(directory, GetFl, 0) == SCE_KERNEL_O_RDONLY);
    Check(sceKernelClose(directory) == 0);

    int pipes[2] = {-1, -1};
    Check(pipe_nid_postfix(pipes) == 0);
    Check(Throws(pipes[0], GetFl, 0));
    Check(close_nid_postfix(pipes[0]) == 0);
    Check(close_nid_postfix(pipes[1]) == 0);

    const int socket = socket_nid_postfix(2, 2, 0);
    Check(socket >= 0);
    Check(sceKernelFcntl(socket, GetFl, 0) == SCE_KERNEL_O_RDWR);
    Check(sceKernelFcntl(socket, SetFl, SCE_KERNEL_O_NONBLOCK) == 0);
    Check(sceKernelFcntl(socket, GetFl, 0) == (SCE_KERNEL_O_RDWR | SCE_KERNEL_O_NONBLOCK));
    Check(sceKernelFcntl(socket, SetFl, 0) == 0);
    Check(sceKernelFcntl(socket, GetFl, 0) == SCE_KERNEL_O_RDWR);
    Check(sceKernelFcntl(socket, SetFl, SCE_KERNEL_O_APPEND) == SCE_KERNEL_ERROR_EOPNOTSUPP);
    Check(sceKernelFcntl(socket, GetFd, 0) == SCE_KERNEL_ERROR_EINVAL);
    Check(close_nid_postfix(socket) == 0);
    Check(sceKernelFcntl(socket, GetFl, 0) == SCE_KERNEL_ERROR_EBADF);

    std::filesystem::remove_all(root);
    return 0;
}
