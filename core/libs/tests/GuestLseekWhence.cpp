#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceKernelOpen(const char* path, int flags, std::uint16_t mode);
int APS5_VABI sceKernelClose(int d);
std::int64_t APS5_VABI sceKernelLseek(int d, std::int64_t offset, int whence);
std::int64_t APS5_VABI lseek_nid_postfix(int d, std::int64_t offset, int whence);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI pipe_nid_postfix(int* descriptors);
int APS5_VABI close_nid_postfix(int d);
int APS5_VABI sceKernelMkdir(const char* path, std::uint16_t mode);
std::int64_t APS5_VABI sceKernelWrite(int d, const void* buf, std::size_t nbytes);
}

namespace {

void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "lseek whence check failed at line %d\n", line);
        std::exit(1);
    }
}
#define Check(value) Require((value), __LINE__)

constexpr int GuestEbadf = 9;
constexpr int GuestEinval = 22;
constexpr int GuestEspipe = 29;
constexpr int SeekData = 3;
constexpr int SeekHole = 4;

template <typename TCall> bool NotImplemented(TCall call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::strstr(error.what(), "not implemented") != nullptr;
    }
    return false;
}

std::filesystem::path EnterFreshRoot() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-lseek-whence-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    return root;
}

void UnknownWhence() {
    { std::ofstream file("data.bin", std::ios::binary); file << "0123456789"; }
    const int fd = sceKernelOpen("data.bin", SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0);
    Check(sceKernelLseek(fd, 4, 0) == 4);
    for (const int whence : {-1, 5, 6, 0x7fffffff}) {
        Check(sceKernelLseek(fd, 0, whence) == SCE_KERNEL_ERROR_EINVAL);
        *__error_nid_postfix() = 0;
        Check(lseek_nid_postfix(fd, 0, whence) == -1 && *__error_nid_postfix() == GuestEinval);
    }
    Check(sceKernelLseek(fd, 0, 1) == 4);
    Check(sceKernelClose(fd) == 0);

    Check(sceKernelLseek(fd, 0, 9) == SCE_KERNEL_ERROR_EBADF);
    *__error_nid_postfix() = 0;
    Check(lseek_nid_postfix(fd, 0, 9) == -1 && *__error_nid_postfix() == GuestEbadf);
    int pipe[2] = {-1, -1};
    Check(pipe_nid_postfix(pipe) == 0);
    for (const int end : pipe) {
        Check(sceKernelLseek(end, 0, 9) == SCE_KERNEL_ERROR_ESPIPE);
        *__error_nid_postfix() = 0;
        Check(lseek_nid_postfix(end, 0, -1) == -1 && *__error_nid_postfix() == GuestEspipe);
    }
    Check(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0);
}

void DataAndHoles() {
    const int fd = sceKernelOpen("data.bin", SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0);
    Check(sceKernelLseek(fd, 3, SeekData) == 3 && sceKernelLseek(fd, 0, 1) == 3);
    Check(sceKernelLseek(fd, 0, SeekData) == 0);
    Check(sceKernelLseek(fd, 3, SeekHole) == 10 && sceKernelLseek(fd, 0, 1) == 10);
    Check(sceKernelLseek(fd, 9, SeekHole) == 10);
    Check(sceKernelLseek(fd, 4, 0) == 4);
    Check(sceKernelLseek(fd, 10, SeekData) == SCE_KERNEL_ERROR_ENXIO);
    Check(sceKernelLseek(fd, 10, SeekHole) == SCE_KERNEL_ERROR_ENXIO);
    Check(sceKernelLseek(fd, 1000, SeekData) == SCE_KERNEL_ERROR_ENXIO);
    Check(sceKernelLseek(fd, 0, 1) == 4);
    *__error_nid_postfix() = 0;
    Check(lseek_nid_postfix(fd, 10, SeekHole) == -1 && *__error_nid_postfix() == 6);
    Check(lseek_nid_postfix(fd, 2, SeekHole) == 10);
    Check(NotImplemented([fd] { (void)sceKernelLseek(fd, -1, SeekData); }));
    Check(NotImplemented([fd] { (void)sceKernelLseek(fd, -1, SeekHole); }));
    Check(sceKernelClose(fd) == 0);

    { std::ofstream empty("empty.bin", std::ios::binary); }
    const int none = sceKernelOpen("empty.bin", SCE_KERNEL_O_RDONLY, 0);
    Check(none >= 0);
    Check(sceKernelLseek(none, 0, SeekData) == SCE_KERNEL_ERROR_ENXIO);
    Check(sceKernelLseek(none, 0, SeekHole) == SCE_KERNEL_ERROR_ENXIO);
    Check(sceKernelClose(none) == 0);

    Check(sceKernelMkdir("folder", 0755) == 0);
    const int folder = sceKernelOpen("folder", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Check(folder >= 0);
    Check(sceKernelLseek(folder, 0, SeekData) == SCE_KERNEL_ERROR_ENOTTY);
    Check(sceKernelLseek(folder, 0, SeekHole) == SCE_KERNEL_ERROR_ENOTTY);
    Check(sceKernelClose(folder) == 0);

    int pipe[2] = {-1, -1};
    Check(pipe_nid_postfix(pipe) == 0);
    Check(sceKernelLseek(pipe[0], 0, SeekData) == SCE_KERNEL_ERROR_ESPIPE);
    Check(sceKernelLseek(pipe[1], 0, SeekHole) == SCE_KERNEL_ERROR_ESPIPE);
    Check(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0);
}

}

int main() {
    const auto root = EnterFreshRoot();
    UnknownWhence();
    DataAndHoles();
    std::error_code error;
    std::filesystem::current_path(std::filesystem::temp_directory_path(), error);
    std::filesystem::remove_all(root, error);
    return 0;
}
