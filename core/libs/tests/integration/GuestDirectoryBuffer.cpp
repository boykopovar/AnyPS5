#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
int APS5_VABI sceKernelGetdents(int, char*, int);
int APS5_VABI sceKernelGetdirentries(int, char*, int, std::int64_t*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Directory buffer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-directory-buffer-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const std::string name = "a-long-directory-entry-name.bin";
    { std::ofstream file(root / name); Require(static_cast<bool>(file)); }
    const int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    std::array<char, 256> buffer;
    buffer.fill('x');
    std::int64_t base = -1;
    Require(sceKernelGetdirentries(directory, buffer.data(), 8, &base) == SCE_KERNEL_ERROR_EINVAL);
    Require(std::all_of(buffer.begin(), buffer.end(), [](char byte) { return byte == 'x'; }));
    Require(sceKernelGetdents(directory, buffer.data(), 8) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelGetdents(directory, nullptr, 256) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelGetdents(directory, buffer.data(), 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelGetdents(directory, buffer.data(), -1) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelGetdents(directory, buffer.data(), 20) == 12);
    Require(std::strcmp(buffer.data() + 8, ".") == 0);
    Require(sceKernelGetdents(directory, buffer.data(), 12) == 12);
    Require(std::strcmp(buffer.data() + 8, "..") == 0);
    buffer.fill('x');
    Require(sceKernelGetdents(directory, buffer.data(), 12) == SCE_KERNEL_ERROR_EINVAL);
    Require(std::all_of(buffer.begin(), buffer.end(), [](char byte) { return byte == 'x'; }));
    const int recordSize = static_cast<int>((8 + name.size() + 1 + 3) & ~std::size_t{3});
    Require(sceKernelGetdirentries(directory, buffer.data(), recordSize, &base) == recordSize);
    Require(std::strcmp(buffer.data() + 8, name.c_str()) == 0);
    Require(sceKernelGetdents(directory, buffer.data(), 8) == 0);
    Require(sceKernelGetdents(directory, buffer.data(), 256) == 0);
    Require(sceKernelClose(directory) == 0);
    Require(sceKernelGetdents(-1, buffer.data(), 256) == SCE_KERNEL_ERROR_ENOTDIR);
    std::filesystem::remove_all(root);
}
