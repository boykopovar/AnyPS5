#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

extern "C" {
int* APS5_VABI __error_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int enoent = 2;
constexpr int efault = 14;

using RenameFunction = int (APS5_VABI*)(const char*, const char*);

RenameFunction LoadKernelRename() {
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

RenameFunction KernelRename() {
    static const RenameFunction function = LoadKernelRename();
    Require(function != nullptr, "rename_nid_postfix is defined by libkernel");
    return function;
}

class RenameFixture {
public:
    RenameFixture()
        : root("anyps5-kernel-rename-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())),
          file((root / "file.txt").string()),
          renamed((root / "renamed.txt").string()) {
        Require(std::filesystem::create_directory(root), "create the test directory");
        std::ofstream(file) << "retained";
    }
    ~RenameFixture() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
    RenameFixture(const RenameFixture&) = delete;
    RenameFixture& operator=(const RenameFixture&) = delete;

    const std::filesystem::path root;
    const std::string file;
    const std::string renamed;
};

void RequireFailure(int result, int expectedError, const std::string& message) {
    RequireEqual(result, -1, message + " result");
    RequireEqual(*__error_nid_postfix(), expectedError, message + " errno");
}

const Case existingFile{"KernelRename_ExistingFile_MovesItToNewName", [] {
    const auto rename = KernelRename();
    const RenameFixture fixture;
    RequireEqual(rename(fixture.file.c_str(), fixture.renamed.c_str()), 0, "rename");
    Require(!std::filesystem::exists(fixture.file), "source no longer exists");
    Require(std::filesystem::exists(fixture.renamed), "destination exists");
}};

const Case samePath{"KernelRename_SameSourceAndDestination_Succeeds", [] {
    const auto rename = KernelRename();
    const RenameFixture fixture;
    RequireEqual(rename(fixture.file.c_str(), fixture.file.c_str()), 0, "rename onto itself");
    Require(std::filesystem::exists(fixture.file), "file still exists");
}};

const Case missingSource{"KernelRename_MissingSource_FailsWithEnoent", [] {
    const auto rename = KernelRename();
    const RenameFixture fixture;
    RequireEqual(rename(fixture.file.c_str(), fixture.renamed.c_str()), 0, "first rename");
    RequireFailure(rename(fixture.file.c_str(), fixture.renamed.c_str()), enoent, "rename of moved source");
}};

const Case nullPath{"KernelRename_NullPath_FailsWithEfault", [] {
    const auto rename = KernelRename();
    const RenameFixture fixture;
    RequireFailure(rename(nullptr, fixture.file.c_str()), efault, "null source");
    RequireFailure(rename(fixture.file.c_str(), nullptr), efault, "null destination");
}};

const Case emptyPath{"KernelRename_EmptyPath_FailsWithEnoent", [] {
    const auto rename = KernelRename();
    const RenameFixture fixture;
    RequireFailure(rename("", fixture.file.c_str()), enoent, "empty source");
    RequireFailure(rename(fixture.file.c_str(), ""), enoent, "empty destination");
}};

} // namespace
