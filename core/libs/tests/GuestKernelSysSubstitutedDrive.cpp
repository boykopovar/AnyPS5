#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <source_location>
#include <string>
#include <windows.h>

namespace {

std::filesystem::path g_base;
std::wstring g_drive;

void Cleanup() {
    if (!g_base.empty()) ::SetCurrentDirectoryW(g_base.parent_path().c_str());
    if (!g_drive.empty()) ::DefineDosDeviceW(DDD_NO_BROADCAST_SYSTEM | DDD_REMOVE_DEFINITION, g_drive.c_str(), nullptr);
    g_drive.clear();
    std::error_code error;
    if (!g_base.empty()) std::filesystem::remove_all(g_base, error);
}

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "substituted drive check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
    Cleanup();
    std::abort();
}

std::wstring FreeDrive() {
    const DWORD used = ::GetLogicalDrives();
    for (wchar_t letter = L'Z'; letter >= L'D'; --letter) {
        if ((used & (1u << (letter - L'A'))) == 0) return std::wstring{letter, L':'};
    }
    return {};
}

}

int main() {
    g_base = std::filesystem::temp_directory_path() /
        ("anyps5-subst-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(g_base / "dir");
    const auto drive = FreeDrive();
    if (drive.empty() || !::DefineDosDeviceW(DDD_NO_BROADCAST_SYSTEM, drive.c_str(), g_base.c_str())) {
        Cleanup();
        std::fputs("no drive letter could be substituted\n", stderr);
        return 77;
    }
    g_drive = drive;
    Require(::SetCurrentDirectoryW((drive + L"\\").c_str()) != FALSE);
    char path[1024];
    Require(getcwd_nid_postfix(path, sizeof(path)) == path && std::strcmp(path, "/") == 0);
    const int directory = sceKernelOpen("dir", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    Require(fchdir_nid_postfix(directory) == 0);
    Require(getcwd_nid_postfix(path, sizeof(path)) == path && std::strcmp(path, "/dir") == 0);
    Require(chdir_nid_postfix("/") == 0);
    Require(sceKernelClose(directory) == 0);
    Cleanup();
    return 0;
}
