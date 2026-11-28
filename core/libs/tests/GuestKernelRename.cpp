#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
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
#ifdef _WIN32
static std::filesystem::path ExtendedPath(const std::filesystem::path& path) {
    return std::filesystem::path(L"\\\\?\\" + std::filesystem::absolute(path).lexically_normal().make_preferred().native());
}

static void CreateLongJunction(const std::filesystem::path& link, const std::filesystem::path& target) {
    Require(CreateDirectoryW(ExtendedPath(link).c_str(), nullptr) != 0);
    const auto printName = std::filesystem::absolute(target).lexically_normal().make_preferred().wstring();
    const auto substituteName = L"\\??\\" + printName;
    struct MountPoint {
        ULONG tag;
        USHORT dataLength;
        USHORT reserved;
        USHORT substituteOffset;
        USHORT substituteLength;
        USHORT printOffset;
        USHORT printLength;
        wchar_t names[2048];
    } data{};
    Require(substituteName.size() + printName.size() + 2 <= std::size(data.names));
    data.tag = IO_REPARSE_TAG_MOUNT_POINT;
    data.substituteLength = static_cast<USHORT>(substituteName.size() * sizeof(wchar_t));
    data.printOffset = static_cast<USHORT>((substituteName.size() + 1) * sizeof(wchar_t));
    data.printLength = static_cast<USHORT>(printName.size() * sizeof(wchar_t));
    data.dataLength = static_cast<USHORT>(8 + data.printOffset + data.printLength + sizeof(wchar_t));
    std::memcpy(data.names, substituteName.c_str(), data.substituteLength + sizeof(wchar_t));
    std::memcpy(reinterpret_cast<char*>(data.names) + data.printOffset, printName.c_str(), data.printLength + sizeof(wchar_t));
    const auto handle = CreateFileW(ExtendedPath(link).c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    Require(handle != INVALID_HANDLE_VALUE);
    DWORD returned = 0;
    const auto result = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, &data, 8 + data.dataLength, nullptr, 0, &returned, nullptr);
    Require(CloseHandle(handle) != 0);
    Require(result != 0);
}

static void CreateLongDirectories(const std::filesystem::path& base, const std::filesystem::path& deep) {
    auto directory = base;
    Require(CreateDirectoryW(ExtendedPath(base).c_str(), nullptr) != 0);
    for (const auto& component : deep.lexically_relative(base)) {
        directory /= component;
        Require(CreateDirectoryW(ExtendedPath(directory).c_str(), nullptr) != 0);
    }
}

static void RemoveLongDirectories(const std::filesystem::path& base, const std::filesystem::path& deep) {
    for (auto path = deep;; path = path.parent_path()) {
        Require(RemoveDirectoryW(ExtendedPath(path).c_str()) != 0);
        if (path == base) break;
    }
}
#endif
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
#ifdef _WIN32
    const auto juncTarget = root / "junc-target";
    const auto junc = root / "junc";
    const auto srcDir = root / "junc-src";
    Require(std::filesystem::create_directory(juncTarget));
    Require(std::filesystem::create_directory(srcDir));
    CreateLongJunction(junc, juncTarget);
    const auto juncSlash = junc.string() + sep;
    const auto juncFwdSlash = junc.string() + "/";
    Require(rename_nid_postfix(srcDir.string().c_str(), juncSlash.c_str()) == -1 && *__error_nid_postfix() == 20);
    Require(::rename_nid_postfix(srcDir.string().c_str(), juncFwdSlash.c_str()) == -1 && *__error_nid_postfix() == 20);
    const DWORD juncAttrs = GetFileAttributesW(ExtendedPath(junc).c_str());
    Require(juncAttrs != INVALID_FILE_ATTRIBUTES && (juncAttrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
    Require(std::filesystem::is_directory(srcDir));
    Require(RemoveDirectoryW(ExtendedPath(junc).c_str()) != 0);

    const auto longRoot = std::filesystem::absolute(root) / "long";
    const auto longTarget = std::filesystem::absolute(root) / "long-target";
    Require(CreateDirectoryW(ExtendedPath(longTarget).c_str(), nullptr) != 0);
    auto deep = longRoot;
    while (deep.native().size() < MAX_PATH + 16) deep /= std::wstring(40, L'a');
    CreateLongDirectories(longRoot, deep);
    const auto longJunc = deep / "junc";
    const auto longFile = deep / "file.txt";
    const auto longMoved = deep / "moved";
    Require(longJunc.string().size() >= MAX_PATH && longFile.string().size() >= MAX_PATH && longMoved.string().size() >= MAX_PATH);
    CreateLongJunction(longJunc, longTarget);
    const auto fileHandle = CreateFileW(ExtendedPath(longFile).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(fileHandle != INVALID_HANDLE_VALUE && CloseHandle(fileHandle) != 0);
    Require(rename_nid_postfix(longJunc.string().c_str(), longFile.string().c_str()) == 0);
    Require(GetFileAttributesW(ExtendedPath(longJunc).c_str()) == INVALID_FILE_ATTRIBUTES);
    const DWORD replacedAttrs = GetFileAttributesW(ExtendedPath(longFile).c_str());
    Require(replacedAttrs != INVALID_FILE_ATTRIBUTES && (replacedAttrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
    Require(rename_nid_postfix(longFile.string().c_str(), longMoved.string().c_str()) == 0);
    Require(GetFileAttributesW(ExtendedPath(longFile).c_str()) == INVALID_FILE_ATTRIBUTES);
    const DWORD movedAttrs = GetFileAttributesW(ExtendedPath(longMoved).c_str());
    Require(movedAttrs != INVALID_FILE_ATTRIBUTES && (movedAttrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
    Require(::rename_nid_postfix(longMoved.string().c_str(), longFile.string().c_str()) == 0);
    Require(GetFileAttributesW(ExtendedPath(longMoved).c_str()) == INVALID_FILE_ATTRIBUTES);
    const DWORD restoredAttrs = GetFileAttributesW(ExtendedPath(longFile).c_str());
    Require(restoredAttrs != INVALID_FILE_ATTRIBUTES && (restoredAttrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
    Require(RemoveDirectoryW(ExtendedPath(longFile).c_str()) != 0);
    RemoveLongDirectories(longRoot, deep);
    Require(RemoveDirectoryW(ExtendedPath(longTarget).c_str()) != 0);
#endif
    std::filesystem::remove_all(root);
    return 0;
}
