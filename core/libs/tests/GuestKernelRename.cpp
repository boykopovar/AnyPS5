#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using RenameFunction = int (APS5_VABI*)(const char*, const char*);
static RenameFunction LoadKernelRename() {
#ifdef _WIN32
    const HMODULE module = LoadLibraryA(KERNEL_PRX_PATH);
    if (module == nullptr) return nullptr;
    const auto function = reinterpret_cast<RenameFunction>(reinterpret_cast<void*>(GetProcAddress(module, "rename_nid_postfix")));
    HMODULE owner = nullptr;
    if (function == nullptr || !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(function), &owner) || owner != module) return nullptr;
    return function;
#else
    void* module = dlopen(KERNEL_PRX_PATH, RTLD_NOW | RTLD_LOCAL);
    if (module == nullptr) return nullptr;
    void* function = dlsym(module, "rename_nid_postfix");
    Dl_info owner{};
    void* self = dlsym(module, "sceKernelRename");
    Dl_info expected{};
    if (function == nullptr || self == nullptr || !dladdr(function, &owner) || !dladdr(self, &expected) ||
        owner.dli_fname == nullptr || expected.dli_fname == nullptr || std::strcmp(owner.dli_fname, expected.dli_fname) != 0) return nullptr;
    return reinterpret_cast<RenameFunction>(function);
#endif
}
static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Kernel rename check failed at line %d (guest errno %d)\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)
int main() {
    const RenameFunction rename_nid_postfix = LoadKernelRename();
    if (rename_nid_postfix == nullptr) { std::fputs("rename_nid_postfix is not defined by libkernel\n", stderr); return 1; }
    const auto root = std::filesystem::path("anyps5-kernel-rename-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto file = root / "file.txt";
    const auto renamed = root / "renamed.txt";
    { std::ofstream stream(file); stream << "retained"; }
    Require(rename_nid_postfix(file.string().c_str(), renamed.string().c_str()) == 0);
    Require(!std::filesystem::exists(file) && std::filesystem::exists(renamed));
    Require(rename_nid_postfix(renamed.string().c_str(), renamed.string().c_str()) == 0);
    Require(rename_nid_postfix(file.string().c_str(), renamed.string().c_str()) == -1 && *__error_nid_postfix() == 2);
    Require(rename_nid_postfix(nullptr, renamed.string().c_str()) == -1 && *__error_nid_postfix() == 14);
    Require(rename_nid_postfix(renamed.string().c_str(), nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(rename_nid_postfix("", renamed.string().c_str()) == -1 && *__error_nid_postfix() == 2);
    Require(rename_nid_postfix(renamed.string().c_str(), "") == -1 && *__error_nid_postfix() == 2);
    const auto dir = root / "dir";
    const auto file2 = root / "file2.txt";
    Require(std::filesystem::create_directory(dir));
    { std::ofstream stream(dir / "inner.txt"); stream << "inner"; }
    { std::ofstream stream(file2); stream << "x"; }
    const auto sep = std::string(1, std::filesystem::path::preferred_separator);
    const auto dirSlash = (root / "newdir").string() + sep;
    Require(rename_nid_postfix(dir.string().c_str(), dirSlash.c_str()) == 0);
    Require(!std::filesystem::exists(dir) && std::filesystem::is_directory(root / "newdir"));
    Require(std::filesystem::exists(root / "newdir" / "inner.txt"));
    const auto doubleSlash = (root / "deeper").string() + sep + sep;
    Require(rename_nid_postfix((root / "newdir").string().c_str(), doubleSlash.c_str()) == 0);
    Require(std::filesystem::is_directory(root / "deeper"));
    Require(std::filesystem::exists(root / "deeper" / "inner.txt"));
    const auto fileSlash = (root / "moved").string() + sep;
    Require(rename_nid_postfix(file2.string().c_str(), fileSlash.c_str()) == -1 && *__error_nid_postfix() == 2);
    Require(std::filesystem::exists(file2));
    const auto emptyDir = root / "emptydir";
    Require(std::filesystem::create_directory(emptyDir));
    const auto emptySlash = emptyDir.string() + sep;
    Require(rename_nid_postfix((root / "deeper").string().c_str(), emptySlash.c_str()) == 0);
    Require(std::filesystem::is_directory(emptyDir));
    Require(std::filesystem::exists(emptyDir / "inner.txt"));
    const auto nonemptyDir = root / "nonempty";
    Require(std::filesystem::create_directories(nonemptyDir / "inner"));
    const auto nonemptySlash = nonemptyDir.string() + sep;
    Require(rename_nid_postfix(emptyDir.string().c_str(), nonemptySlash.c_str()) == -1 && *__error_nid_postfix() == 66);
    Require(std::filesystem::exists(emptyDir / "inner.txt") && std::filesystem::is_directory(nonemptyDir / "inner"));
    std::filesystem::remove_all(root);
    return 0;
}
