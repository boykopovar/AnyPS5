#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestHeap.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

extern "C" {
int APS5_VABI chdir_nid_postfix(const char*);
char* APS5_VABI getcwd_nid_postfix(char*, std::size_t);
int* APS5_VABI __error_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::Fail;
using Testing::Require;
using Testing::RequireEqual;

constexpr int enoent = 2;
constexpr int enotdir = 20;
constexpr int einval = 22;
constexpr int erange = 34;

std::atomic<int> allocations{0};
std::atomic<int> releases{0};

void* APS5_VABI Allocate(std::size_t bytes) {
    ++allocations;
    return GuestHeap::GuestHeapAllocate_nid_postfix(bytes);
}

void APS5_VABI Release(void* pointer) {
    ++releases;
    GuestHeap::GuestHeapFree_nid_postfix(pointer);
}

void* APS5_VABI Unused() {
    Fail("unexpected application heap callback");
}

void RegisterCountingHeap() {
    const std::array<void*, 10> api{reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release),
        reinterpret_cast<void*>(&Unused), reinterpret_cast<void*>(&Unused), reinterpret_cast<void*>(&Unused),
        reinterpret_cast<void*>(&Unused), reinterpret_cast<void*>(&Unused)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

class WorkingDirectoryFixture {
public:
    WorkingDirectoryFixture()
        : host(std::filesystem::canonical(std::filesystem::current_path())),
          name("anyps5-cwd-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())),
          directory(host / name),
          file(directory / "sample.txt") {
        Require(std::filesystem::create_directory(directory), "create the test directory");
        std::ofstream(file) << "sample";
    }

    ~WorkingDirectoryFixture() {
        chdir_nid_postfix("/");
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }

    WorkingDirectoryFixture(const WorkingDirectoryFixture&) = delete;
    WorkingDirectoryFixture& operator=(const WorkingDirectoryFixture&) = delete;

    void Enter() const {
        RequireEqual(chdir_nid_postfix(name.c_str()), 0, "enter the test directory");
    }

    const std::filesystem::path host;
    const std::string name;
    const std::filesystem::path directory;
    const std::filesystem::path file;
};

class HeapString {
public:
    explicit HeapString(char* pointer) : pointer(pointer) {}
    ~HeapString() { ApplicationHeapFree_nid_no_patch(pointer); }
    HeapString(const HeapString&) = delete;
    HeapString& operator=(const HeapString&) = delete;

    char* Get() const { return pointer; }

    void Free() {
        ApplicationHeapFree_nid_no_patch(pointer);
        pointer = nullptr;
    }

private:
    char* pointer;
};

std::string GuestWorkingDirectory() {
    char path[1024];
    Require(getcwd_nid_postfix(path, sizeof(path)) == path, "getcwd returns the supplied buffer");
    return path;
}

void RequireFailure(int result, int expectedError, const char* message) {
    RequireEqual(result, -1, message);
    RequireEqual(*__error_nid_postfix(), expectedError, message);
}

const Case initialDirectory{"Getcwd_WithoutChdir_ReturnsGuestRoot", [] {
    const WorkingDirectoryFixture fixture;
    RequireEqual(GuestWorkingDirectory(), std::string("/"), "initial guest directory");
}};

const Case chdirKeepsHost{"Chdir_RelativeDirectory_KeepsHostWorkingDirectory", [] {
    const WorkingDirectoryFixture fixture;
    fixture.Enter();
    RequireEqual(std::filesystem::current_path(), fixture.host, "host working directory");
}};

const Case getcwdAfterChdir{"Getcwd_AfterChdir_ReturnsGuestDirectory", [] {
    const WorkingDirectoryFixture fixture;
    fixture.Enter();
    RequireEqual(GuestWorkingDirectory(), "/" + fixture.name, "guest directory");
}};

const Case resolveRelative{"ResolvePath_RelativePathAfterChdir_ResolvesInsideGuestDirectory", [] {
    const WorkingDirectoryFixture fixture;
    fixture.Enter();
    RequireEqual(ResolvePath_nid_no_patch("sample.txt"), fixture.file, "relative path");
}};

const Case resolveAbsolute{"ResolvePath_AbsoluteGuestPath_ResolvesFromGuestRoot", [] {
    const WorkingDirectoryFixture fixture;
    fixture.Enter();
    RequireEqual(ResolvePath_nid_no_patch(("/" + fixture.name + "/sample.txt").c_str()), fixture.file, "absolute path");
}};

const Case tinyBuffer{"Getcwd_BufferTooSmall_FailsWithErangeWithoutWriting", [] {
    const WorkingDirectoryFixture fixture;
    fixture.Enter();
    char tiny[] = "xyz";
    Require(getcwd_nid_postfix(tiny, 2) == nullptr, "getcwd returns null");
    RequireEqual(*__error_nid_postfix(), erange, "errno");
    RequireEqual(std::string_view(tiny), std::string_view("xyz"), "buffer untouched");
}};

const Case zeroSize{"Getcwd_ZeroSizeBuffer_FailsWithEinval", [] {
    const WorkingDirectoryFixture fixture;
    fixture.Enter();
    char path[1024];
    Require(getcwd_nid_postfix(path, 0) == nullptr, "getcwd returns null");
    RequireEqual(*__error_nid_postfix(), einval, "errno");
}};

const Case nullBuffer{"Getcwd_NullBuffer_AllocatesFromApplicationHeap", [] {
    RegisterCountingHeap();
    const WorkingDirectoryFixture fixture;
    fixture.Enter();
    const std::string expected = GuestWorkingDirectory();
    const int allocationsBefore = allocations;
    const int releasesBefore = releases;
    HeapString allocated(getcwd_nid_postfix(nullptr, 0));
    Require(allocated.Get() != nullptr, "getcwd allocates a buffer");
    RequireEqual(std::string(allocated.Get()), expected, "allocated directory");
    RequireEqual(allocations - allocationsBefore, 1, "application heap allocations");
    allocated.Free();
    RequireEqual(releases - releasesBefore, 1, "application heap releases");
}};

const Case chdirFile{"Chdir_RegularFile_FailsWithEnotdir", [] {
    const WorkingDirectoryFixture fixture;
    fixture.Enter();
    RequireFailure(chdir_nid_postfix("sample.txt"), enotdir, "chdir into a file");
}};

const Case chdirMissing{"Chdir_MissingDirectory_FailsWithEnoent", [] {
    const WorkingDirectoryFixture fixture;
    fixture.Enter();
    RequireFailure(chdir_nid_postfix("missing"), enoent, "chdir into a missing directory");
}};

const Case chdirParent{"Chdir_ParentOfTopLevelDirectory_ReturnsToGuestRoot", [] {
    const WorkingDirectoryFixture fixture;
    fixture.Enter();
    RequireEqual(chdir_nid_postfix(".."), 0, "chdir ..");
    RequireEqual(GuestWorkingDirectory(), std::string("/"), "guest directory");
}};

const Case chdirAboveRoot{"Chdir_AboveGuestRoot_StaysAtGuestRoot", [] {
    const WorkingDirectoryFixture fixture;
    RequireEqual(chdir_nid_postfix("../.."), 0, "chdir ../..");
    RequireEqual(GuestWorkingDirectory(), std::string("/"), "guest directory");
}};

} // namespace
