#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <nid/NidCompute.hpp>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <source_location>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Kernel sys pwrite check failed at line %u\n", location.line());
        std::abort();
    }
}

static void* LoadLibraryFile(const std::filesystem::path& path) {
#ifdef _WIN32
    auto* module = reinterpret_cast<void*>(LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH));
#else
    auto* module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr);
#ifdef _WIN32
    std::wstring loaded(32768, L'\0');
    const auto length = GetModuleFileNameW(static_cast<HMODULE>(module), loaded.data(), static_cast<DWORD>(loaded.size()));
    Require(length != 0 && length < loaded.size());
    loaded.resize(length);
    Require(std::filesystem::equivalent(path, std::filesystem::path(loaded)));
#endif
    return module;
}

template <typename Function>
static Function LoadFunction(void* module, const char* name, bool patched, const char* unpatched = nullptr) {
    const auto symbol = patched ? Nid::ComputeNid(name, "") : unpatched ? std::string(unpatched) : std::string(name) + "_nid_postfix";
#ifdef _WIN32
    const auto function = reinterpret_cast<Function>(reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module), symbol.c_str())));
#else
    const auto function = reinterpret_cast<Function>(dlsym(module, symbol.c_str()));
#endif
    if (!function) std::fprintf(stderr, "Missing symbol %s\n", symbol.c_str());
    Require(function != nullptr);
    return function;
}

int main(int argc, char** argv) {
    Require(argc == 3);
    const auto directory = std::filesystem::absolute(argv[1]);
    const bool patched = std::strcmp(argv[2], "patched") == 0;
    auto* sys = LoadLibraryFile(directory / "libkernel_sys.prx");
    auto* kernel = LoadLibraryFile(directory / "libkernel.prx");
    auto* libc = LoadLibraryFile(directory / "libc.prx");
    using Pwrite = std::int64_t (APS5_VABI*)(int, const void*, std::size_t, std::int64_t);
    const auto writeAt = LoadFunction<Pwrite>(sys, "pwrite", patched);
    const auto kernelWriteAt = LoadFunction<Pwrite>(kernel, "pwrite", patched, "pwrite_nid_disambig1_nid_postfix");
    Require(writeAt != kernelWriteAt);
    const auto open = LoadFunction<int (APS5_VABI*)(const char*, int, int)>(kernel, "open", patched);
    const auto close = LoadFunction<int (APS5_VABI*)(int)>(kernel, "close", patched);
    const auto duplicate = LoadFunction<int (APS5_VABI*)(int)>(kernel, "dup", patched);
    const auto duplicateTo = LoadFunction<int (APS5_VABI*)(int, int)>(kernel, "dup2", patched);
    const auto write = LoadFunction<std::int64_t (APS5_VABI*)(int, const char*, std::int64_t)>(kernel, "write", patched);
    const auto seek = LoadFunction<std::int64_t (APS5_VABI*)(int, std::int64_t, int)>(kernel, "lseek", patched);
    const auto readAt = LoadFunction<std::int64_t (APS5_VABI*)(int, void*, std::size_t, std::int64_t)>(kernel, "pread", patched);
    const auto error = LoadFunction<int* (APS5_VABI*)()>(libc, "__error", patched);
    const auto shmOpen = LoadFunction<int (APS5_VABI*)(const char*, int, int)>(kernel, "shm_open", patched);
    const auto truncate = LoadFunction<int (APS5_VABI*)(int, std::int64_t)>(kernel, "ftruncate", patched);
    const auto pipe = LoadFunction<int (APS5_VABI*)(int*)>(kernel, "pipe", patched);
    const auto socket = LoadFunction<int (APS5_VABI*)(int, int, int)>(kernel, "socket", patched);
    const auto root = std::filesystem::path("anyps5-kernel-sys-pwrite-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto path = (root / "data").string();
    { std::ofstream stream(path, std::ios::binary); stream << "abcdefgh"; Require(stream.good()); }
#ifdef _WIN32
    const auto sparseFile = CreateFileW(std::filesystem::path(path).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr);
    Require(sparseFile != INVALID_HANDLE_VALUE);
    DWORD returned = 0;
    Require(DeviceIoControl(sparseFile, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned, nullptr) != FALSE);
    Require(CloseHandle(sparseFile) != FALSE);
#endif
    const int descriptor = open(path.c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(descriptor >= 0);
    Require(seek(descriptor, 3, SEEK_SET) == 3);
    *error() = 13;
    Require(writeAt(descriptor, "XY", 2, 5) == 2 && *error() == 13);
    Require(seek(descriptor, 0, SEEK_CUR) == 3);
    char contents[8]{};
    Require(readAt(descriptor, contents, sizeof(contents), 0) == 8 && std::memcmp(contents, "abcdeXYh", 8) == 0);
    constexpr std::int64_t largeOffset = (std::int64_t{1} << 32) + 17;
    Require(writeAt(descriptor, "Z", 1, largeOffset) == 1);
    Require(std::filesystem::file_size(path) == largeOffset + 1);
    Require(readAt(descriptor, contents, 2, largeOffset - 1) == 2 && contents[0] == 0 && contents[1] == 'Z');
    Require(readAt(descriptor, contents, 8, 0) == 8 && std::memcmp(contents, "abcdeXYh", 8) == 0);
    Require(writeAt(descriptor, nullptr, 0, 0) == 0);
    Require(writeAt(descriptor, nullptr, 1, 0) == -1 && *error() == 14);
    Require(writeAt(descriptor, "x", 1, -1) == -1 && *error() == 22);
    Require(writeAt(descriptor, nullptr, 0, -5) == -1 && *error() == 22);
    Require(writeAt(descriptor, nullptr, 1, -1) == -1 && *error() == 22);
    Require(writeAt(-1, nullptr, 1, -1) == -1 && *error() == 9);
    Require(writeAt(-1, "x", 1, 0) == -1 && *error() == 9);
    Require(writeAt(-1, nullptr, 0, 0) == -1 && *error() == 9);
    const auto oversized = static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) + 1;
    Require(writeAt(descriptor, "x", oversized, 0) == -1 && *error() == 22);
    Require(seek(descriptor, 0, SEEK_CUR) == 3);
    Require(truncate(descriptor, 8) == 0);
    Require(close(descriptor) == 0);
#ifndef _WIN32
    const int append = open(path.c_str(), SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_APPEND, 0);
    Require(append >= 0 && seek(append, 2, SEEK_SET) == 2);
    Require(writeAt(append, "!", 1, 0) == 1 && seek(append, 0, SEEK_CUR) == 2);
    Require(std::filesystem::file_size(path) == 9);
    Require(close(append) == 0);
#endif
    const int readOnly = open(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(readOnly >= 0);
    Require(writeAt(readOnly, "x", 1, 0) == -1 && *error() == 9);
    Require(writeAt(readOnly, nullptr, 1, -1) == -1 && *error() == 9);
#ifndef _WIN32
    Require(readAt(readOnly, contents, 1, 8) == 1 && contents[0] == '!');
#endif
    Require(close(readOnly) == 0);
    int descriptors[2]{};
    Require(pipe(descriptors) == 0);
    Require(writeAt(descriptors[1], "x", 1, 0) == -1 && *error() == 29);
    Require(close(descriptors[0]) == 0 && close(descriptors[1]) == 0);
    const int sock = socket(2, 1, 0);
    Require(sock >= 0);
    Require(writeAt(sock, "x", 1, 0) == -1 && *error() == 29);
    Require(close(sock) == 0);
    Require(writeAt(sock, "x", 1, 0) == -1 && *error() == 9);
    const int shared = shmOpen(reinterpret_cast<const char*>(1), 0x2, 0600);
    Require(shared >= 0 && truncate(shared, 0x4000) == 0);
    Require(seek(shared, 7, SEEK_SET) == 7);
    Require(writeAt(shared, "shared", 6, 0x1000) == 6);
    Require(readAt(shared, contents, 6, 0x1000) == 6 && std::memcmp(contents, "shared", 6) == 0);
    Require(seek(shared, 0, SEEK_CUR) == 7);
    Require(writeAt(shared, "tail", 4, 0x3ffe) == 2);
    Require(readAt(shared, contents, 4, 0x3ffe) == 2 && std::memcmp(contents, "ta", 2) == 0);
    Require(seek(shared, 0, SEEK_CUR) == 7);
    Require(seek(shared, 0, SEEK_END) == 0x4000);
    const int copy = duplicate(shared);
    Require(copy >= 0 && close(shared) == 0);
    Require(kernelWriteAt(copy, "edge", 4, 0x3ffe) == 2);
    Require(writeAt(copy, "x", 1, 0x4000) == 0 && seek(copy, 0, SEEK_END) == 0x4000);
    Require(seek(copy, 0x3fff, SEEK_SET) == 0x3fff);
    Require(write(copy, "ab", 2) == 1 && write(copy, "x", 1) == 0);
    Require(seek(copy, 0, SEEK_END) == 0x4000);
    Require(close(copy) == 0);
    const int ordinary = open(path.c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(ordinary >= 0 && duplicateTo(ordinary, shared) == shared);
    Require(writeAt(shared, "grow", 4, 16) == 4 && seek(shared, 0, SEEK_END) == 20);
    Require(close(shared) == 0);
    if (ordinary != shared) Require(close(ordinary) == 0);
    std::filesystem::remove_all(root);
    return 0;
}
