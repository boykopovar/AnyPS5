#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
static std::vector<std::filesystem::path> longTree;
#endif
static void Require(bool value) {
    if (value) return;
#ifdef _WIN32
    for (auto entry = longTree.rbegin(); entry != longTree.rend(); ++entry) {
        const auto path = LR"(\\?\)" + entry->wstring();
        if (::RemoveDirectoryW(path.c_str()) == FALSE) ::DeleteFileW(path.c_str());
    }
#endif
    std::abort();
}
static int allocations = 0;
static int releases = 0;
static void* APS5_VABI Allocate(std::size_t bytes) { ++allocations; return GuestHeap::GuestHeapAllocate_nid_postfix(bytes); }
static void APS5_VABI Release(void* pointer) { ++releases; GuestHeap::GuestHeapFree_nid_postfix(pointer); }
static void* APS5_VABI Unused() { std::abort(); }
#ifdef _WIN32
static std::wstring Verbatim(const std::filesystem::path& path) { return LR"(\\?\)" + path.wstring(); }
static void LongDirectory(const std::filesystem::path& host) {
    std::filesystem::path relative = "anyps5-cwd-long-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    for (int level = 0; level < 5; ++level) relative /= std::string(60, 'd');
    std::vector<std::filesystem::path> levels;
    for (const auto& part : relative) levels.push_back(levels.empty() ? host / part : levels.back() / part);
    const auto directory = levels.back();
    longTree = levels;
    longTree.push_back(directory / "sample.txt");
    for (const auto& level : levels) Require(::CreateDirectoryW(Verbatim(level).c_str(), nullptr) != FALSE);
    Require(directory.native().size() > MAX_PATH);
    const HANDLE file = ::CreateFileW(Verbatim(directory / "sample.txt").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
    Require(file != INVALID_HANDLE_VALUE && ::CloseHandle(file) != FALSE);
    const auto guest = "/" + relative.generic_string();
    char path[1024];
    Require(chdir_nid_postfix(guest.c_str()) == 0);
    Require(getcwd_nid_postfix(path, sizeof(path)) == path && path == guest);
    Require(ResolvePath_nid_no_patch("sample.txt") == directory / "sample.txt");
    Require(chdir_nid_postfix((guest + "/sample.txt").c_str()) == -1 && *__error_nid_postfix() == 20);
    Require(chdir_nid_postfix((guest + "/missing").c_str()) == -1 && *__error_nid_postfix() == 2);
    Require(chdir_nid_postfix("/") == 0);
    Require(::DeleteFileW(Verbatim(directory / "sample.txt").c_str()) != FALSE);
    for (auto level = levels.rbegin(); level != levels.rend(); ++level) Require(::RemoveDirectoryW(Verbatim(*level).c_str()) != FALSE);
    longTree.clear();
}
#endif
int main() {
    const std::array<void*, 10> api{reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&Unused),
        reinterpret_cast<void*>(&Unused), reinterpret_cast<void*>(&Unused), reinterpret_cast<void*>(&Unused), reinterpret_cast<void*>(&Unused)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    const auto host = std::filesystem::canonical(std::filesystem::current_path());
    char path[1024];
    Require(getcwd_nid_postfix(path, sizeof(path)) == path && std::strcmp(path, "/") == 0);
    const auto name = "anyps5-cwd-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = host / name;
    Require(std::filesystem::create_directory(directory));
    { std::ofstream file(directory / "sample.txt"); file << "sample"; }
    Require(chdir_nid_postfix(name.c_str()) == 0);
    Require(std::filesystem::current_path() == host);
    Require(getcwd_nid_postfix(path, sizeof(path)) == path && path == "/" + name);
    Require(ResolvePath_nid_no_patch("sample.txt") == directory / "sample.txt");
    Require(ResolvePath_nid_no_patch(("/" + name + "/sample.txt").c_str()) == directory / "sample.txt");
    char tiny[] = "xyz";
    Require(getcwd_nid_postfix(tiny, 2) == nullptr && *__error_nid_postfix() == 34);
    Require(std::strcmp(tiny, "xyz") == 0);
    Require(getcwd_nid_postfix(path, 0) == nullptr && *__error_nid_postfix() == 22);
    char* allocated = getcwd_nid_postfix(nullptr, 0);
    Require(allocated && std::strcmp(allocated, path) == 0 && allocations == 1);
    ApplicationHeapFree_nid_no_patch(allocated);
    Require(releases == 1);
    for (const auto size : {std::size_t{1}, std::size_t{2}, std::size_t{1024}, std::numeric_limits<std::size_t>::max()}) {
        const int before = allocations;
        allocated = getcwd_nid_postfix(nullptr, size);
        if (!allocated) std::fputs("getcwd(NULL, size) must ignore size when allocating its result\n", stderr);
        Require(allocated && std::strcmp(allocated, path) == 0 && allocations == before + 1);
        ApplicationHeapFree_nid_no_patch(allocated);
        Require(releases == allocations);
    }
    Require(chdir_nid_postfix("sample.txt") == -1 && *__error_nid_postfix() == 20);
    Require(chdir_nid_postfix("missing") == -1 && *__error_nid_postfix() == 2);
    Require(chdir_nid_postfix("..") == 0);
    Require(getcwd_nid_postfix(path, sizeof(path)) == path && std::strcmp(path, "/") == 0);
    Require(chdir_nid_postfix("../..") == 0);
    Require(getcwd_nid_postfix(path, sizeof(path)) == path && std::strcmp(path, "/") == 0);
    std::filesystem::remove(directory / "sample.txt");
    std::filesystem::remove(directory);
    const auto vanishedName = "anyps5-cwd-vanished-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto vanished = host / vanishedName;
    Require(std::filesystem::create_directory(vanished));
    Require(chdir_nid_postfix(vanishedName.c_str()) == 0);
    Require(std::filesystem::remove(vanished));
    char one[1] = {'x'};
    Require(getcwd_nid_postfix(one, sizeof(one)) == nullptr && *__error_nid_postfix() == 34);
    Require(chdir_nid_postfix("/") == 0);
    Require(getcwd_nid_postfix(path, sizeof(path)) == path && std::strcmp(path, "/") == 0);
#ifdef _WIN32
    LongDirectory(host);
#endif
}
