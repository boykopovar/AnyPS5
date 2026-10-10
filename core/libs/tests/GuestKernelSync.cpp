#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, unsigned short);
int APS5_VABI sceKernelClose(int);
std::int64_t APS5_VABI sceKernelWrite(int, const void*, std::size_t);
int APS5_VABI sceKernelMkdir(const char*, unsigned short);
int APS5_VABI sceKernelRename(const char*, const char*);
int APS5_VABI sceKernelUnlink(const char*);
int APS5_VABI sceKernelChmod_nid_postfix(const char*, std::uint16_t);
void APS5_VABI sceKernelSync();
void* APS5_VABI fopen_nid_postfix(const char*, const char*);
int APS5_VABI fclose_nid_postfix(void*);
}
static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Kernel sync check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)
static bool Recorded(const char* guest) {
    const auto paths = WrittenPaths_nid_no_patch();
    return std::find(paths.begin(), paths.end(), ResolvePath_nid_no_patch(guest).lexically_normal()) != paths.end();
}
int main() {
    std::filesystem::remove_all("kernel_sync_probe");
    Require(sceKernelMkdir("", 0777) == static_cast<int>(0x80020002u));
    Require(sceKernelMkdir("/", 0777) == static_cast<int>(0x80020011u));
    Require(sceKernelMkdir("kernel_sync_probe", 0777) == 0);
    Require(Recorded("kernel_sync_probe"));
    std::ofstream("kernel_sync_probe/existing.bin") << "existing";
    Require(sceKernelMkdir("kernel_sync_probe/nested/", 0777) == 0);
    Require(std::filesystem::is_directory("kernel_sync_probe/nested") && Recorded("kernel_sync_probe/nested"));
    Require(sceKernelMkdir("kernel_sync_probe/nested/", 0777) == static_cast<int>(0x80020011u));
    Require(sceKernelMkdir("kernel_sync_probe/./", 0777) == static_cast<int>(0x80020011u));
    Require(sceKernelMkdir("kernel_sync_probe/existing.bin/child", 0777) == static_cast<int>(0x80020014u));
    const int reader = sceKernelOpen("kernel_sync_probe/existing.bin", 0x0, 0);
    Require(reader >= 0 && sceKernelClose(reader) == 0);
    void* stream = fopen_nid_postfix("kernel_sync_probe/existing.bin", "r");
    Require(stream != nullptr && fclose_nid_postfix(stream) == 0);
    Require(!Recorded("kernel_sync_probe/existing.bin"));
    stream = fopen_nid_postfix("kernel_sync_probe/existing.bin", "a");
    Require(stream != nullptr && fclose_nid_postfix(stream) == 0);
    Require(Recorded("kernel_sync_probe/existing.bin"));
    const int writer = sceKernelOpen("kernel_sync_probe/data.bin", 0x1 | 0x200 | 0x400, 0644);
    Require(writer >= 0 && sceKernelWrite(writer, "data", 4) == 4 && sceKernelClose(writer) == 0);
    Require(Recorded("kernel_sync_probe/data.bin"));
    Require(sceKernelRename("kernel_sync_probe/data.bin", "kernel_sync_probe/moved.bin") == 0);
    Require(Recorded("kernel_sync_probe/moved.bin"));
    Require(sceKernelUnlink("kernel_sync_probe/moved.bin") == 0);
    sceKernelSync();
    Require(!Recorded("kernel_sync_probe/data.bin") && !Recorded("kernel_sync_probe/moved.bin"));
    sceKernelSync();
    const int locked = sceKernelOpen("kernel_sync_probe/read_only.bin", 0x1 | 0x200, 0644);
    Require(locked >= 0 && sceKernelWrite(locked, "data", 4) == 4 && sceKernelClose(locked) == 0);
    Require(sceKernelChmod_nid_postfix("kernel_sync_probe/read_only.bin", 0444) == 0);
    Require(Recorded("kernel_sync_probe/read_only.bin"));
    sceKernelSync();
    Require(sceKernelChmod_nid_postfix("kernel_sync_probe/read_only.bin", 0644) == 0);
#ifdef _WIN32
    const auto exclusive = CreateFileW(ResolvePath_nid_no_patch("kernel_sync_probe/read_only.bin").c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(exclusive != INVALID_HANDLE_VALUE);
    sceKernelSync();
    CloseHandle(exclusive);
#endif
    sceKernelSync();
    std::filesystem::remove_all("kernel_sync_probe");
}
