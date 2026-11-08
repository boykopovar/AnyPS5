#include "AppArchiveFixture.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace {

constexpr int TypeNotFound = -1;
constexpr int TypeRegular = 1;
constexpr int TypeDirectory = 2;
constexpr int NoSuchFile = 2;
constexpr int NotDirectory = 20;
constexpr int IsDirectory = 21;
constexpr int Invalid = 22;
constexpr int NameTooLong = 63;
constexpr int NoSystemCall = 78;
constexpr int NotSupported = 45;
constexpr std::uint32_t KernelNoSuchFile = 0x80020002u;
constexpr std::uint32_t KernelNotDirectory = 0x80020014u;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "sce v2 filesystem calls: %s\n", message);
    std::abort();
}

void Write(const char* path, std::size_t bytes, unsigned seed) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    for (std::size_t index = 0; index < bytes; ++index) file.put(static_cast<char>((index * 131 + seed) & 0xff));
}

std::string Read(const char* path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}

FileStat Status(const char* path) {
    FileStat status{};
    Require(stat_nid_postfix(path, &status) == 0, "stat of a test file");
    return status;
}

void Prepare() {
    std::error_code ignored;
    for (const char* name : {"f", "g", "big", "d", "e", "ro", "new", "made", "copy", "renamed"}) {
        std::filesystem::permissions(name, std::filesystem::perms::owner_all, std::filesystem::perm_options::add, ignored);
        std::filesystem::remove_all(name, ignored);
    }
    Write("f", 7, 1);
    Write("g", 3, 2);
    Write("big", 200000, 4);
    std::filesystem::create_directories("d/sub");
    Write("d/sub/h", 5, 5);
    std::filesystem::create_directory("e");
    Write("ro", 4, 6);
    std::filesystem::permissions("ro", std::filesystem::perms::owner_read | std::filesystem::perms::group_read | std::filesystem::perms::others_read);
}

void TestStatus() {
    Prepare();
    int perms = -1;
    int error = -1;
    Require(_ZNSt8__sce_v210filesystem6_LstatEPKcPNS0_5permsEPi_nid_postfix("f", &perms, &error) == TypeRegular && error == 0, "a regular file");
    Require(perms == (Status("f").st_mode & 0xfff), "the permission bits of a regular file");
    Require(_ZNSt8__sce_v210filesystem6_LstatEPKcPNS0_5permsEPi_nid_postfix("d", nullptr, nullptr) == TypeDirectory, "a directory without outputs");
    Require(_ZNSt8__sce_v210filesystem6_LstatEPKcPNS0_5permsE_nid_postfix("d/", &perms) == TypeDirectory && perms == (Status("d").st_mode & 0xfff), "a directory with a trailing slash");
    const struct {
        const char* path;
        int error;
    } missing[] = {{"missing", NoSuchFile}, {"f/x", NotDirectory}, {"", NoSuchFile}};
    for (const auto& entry : missing) {
        perms = 0x1234;
        Require(_ZNSt8__sce_v210filesystem6_LstatEPKcPNS0_5permsEPi_nid_postfix(entry.path, &perms, &error) == TypeNotFound && error == entry.error && perms == 0x1234, "a path that is not found");
        Require(_ZNSt8__sce_v210filesystem6_LstatEPKcPNS0_5permsE_nid_postfix(entry.path, &perms) == TypeNotFound && perms == 0x1234, "a path that is not found without an error output");
    }
    const std::string longName(300, 'n');
    Require(_ZNSt8__sce_v210filesystem6_LstatEPKcPNS0_5permsEPi_nid_postfix(longName.c_str(), &perms, &error) == TypeNotFound && error == NameTooLong, "a name that is too long");
}

void TestSizes() {
    Prepare();
    int error = 0x5678;
    Require(_ZNSt8__sce_v210filesystem10_File_sizeEPKcPi_nid_postfix("big", &error) == 200000 && error == 0x5678, "the size of a file leaves the error output alone");
    Require(_ZNSt8__sce_v210filesystem10_File_sizeEPKc_nid_postfix("f") == 7, "the size of a file without an error output");
    Require(_ZNSt8__sce_v210filesystem10_File_sizeEPKcPi_nid_postfix("missing", &error) == UINT64_MAX && static_cast<std::uint32_t>(error) == KernelNoSuchFile, "a missing file reports the kernel error");
    Require(_ZNSt8__sce_v210filesystem10_File_sizeEPKcPi_nid_postfix("f/x", &error) == UINT64_MAX && static_cast<std::uint32_t>(error) == KernelNotDirectory, "a file used as a directory reports the kernel error");
    Require(_ZNSt8__sce_v210filesystem10_File_sizeEPKcPi_nid_postfix("missing", nullptr) == UINT64_MAX, "a missing file without an error output");
    Require(_ZNSt8__sce_v210filesystem10_File_sizeEPKc_nid_postfix("missing") == UINT64_MAX, "a missing file through the short form");
}

void TestTimes() {
    Prepare();
    const struct {
        std::int64_t microseconds, seconds, nanoseconds;
    } times[] = {{-1500001, -2, 499999000}, {1700000000123456, 1700000000, 123456000}, {999999, 0, 999999000}, {-1000000, -1, 0}, {-1, -1, 999999000}};
    for (const auto& time : times) {
        Require(_ZNSt8__sce_v210filesystem20_Set_last_write_timeEPKcl_nid_postfix("f", time.microseconds), "set the write time of a file");
        std::int64_t seconds = 0;
        std::int64_t nanoseconds = 0;
        Require(_ZNSt8__sce_v210filesystem16_Last_write_timeEPKcPlS3__nid_postfix("f", &seconds, &nanoseconds) == 0, "read the write time of a file");
        Require(seconds == time.seconds && nanoseconds == time.nanoseconds, "the write time is floored to whole seconds and microseconds");
        Require(_ZNSt8__sce_v210filesystem16_Last_write_timeEPKc_nid_postfix("f") == time.seconds, "the write time in seconds");
    }
    Require(_ZNSt8__sce_v210filesystem20_Set_last_write_timeEPKcl_nid_postfix("d", 1), "set the write time of a directory");
    Require(!_ZNSt8__sce_v210filesystem20_Set_last_write_timeEPKcl_nid_postfix("missing", 0), "a missing file has no write time to set");
    std::int64_t seconds = 0x1111;
    std::int64_t nanoseconds = 0x2222;
    Require(_ZNSt8__sce_v210filesystem16_Last_write_timeEPKcPlS3__nid_postfix("missing", &seconds, &nanoseconds) == NoSuchFile && seconds == 0x1111 && nanoseconds == 0x2222, "a missing file leaves the time outputs alone");
    Require(_ZNSt8__sce_v210filesystem16_Last_write_timeEPKc_nid_postfix("missing") == -1, "a missing file has no write time");
}

void TestDirectories() {
    Prepare();
    Require(_ZNSt8__sce_v210filesystem9_Make_dirEPKcS2__nid_postfix("made", nullptr) == 1 && std::filesystem::is_directory("made"), "make a directory");
    Require(_ZNSt8__sce_v210filesystem9_Make_dirEPKcS2__nid_postfix("made", nullptr) == 0, "an existing directory is not made again");
    Require(_ZNSt8__sce_v210filesystem9_Make_dirEPKcS2__nid_postfix("f", nullptr) == 0, "an existing file is reported as existing");
    Require(_ZNSt8__sce_v210filesystem9_Make_dirEPKcS2__nid_postfix("missing/x", nullptr) == -NoSuchFile, "a missing parent");
    Require(_ZNSt8__sce_v210filesystem9_Make_dirEPKcS2__nid_postfix("f/x", nullptr) == -NotDirectory, "a file used as a parent");
    Require(_ZNSt8__sce_v210filesystem9_Make_dirEPKcS2__nid_postfix("new", "missing") == -NoSuchFile && !std::filesystem::exists("new"), "a missing template stops before making the directory");
    Require(_ZNSt8__sce_v210filesystem9_Make_dirEPKcS2__nid_postfix("new", "d") == 1 && std::filesystem::is_directory("new"), "make a directory from a template");
    const std::string longName(300, 'n');
    Require(_ZNSt8__sce_v210filesystem9_Make_dirEPKcS2__nid_postfix(longName.c_str(), nullptr) == -NameTooLong, "a name that is too long");
    Require(_ZNSt8__sce_v210filesystem11_Remove_dirEPKc_nid_postfix("made") && !std::filesystem::exists("made"), "remove an empty directory");
    Require(!_ZNSt8__sce_v210filesystem11_Remove_dirEPKc_nid_postfix("d") && std::filesystem::exists("d/sub/h"), "a directory that is not empty stays");
    Require(!_ZNSt8__sce_v210filesystem11_Remove_dirEPKc_nid_postfix("f") && std::filesystem::exists("f"), "a file is not removed as a directory");
    Require(!_ZNSt8__sce_v210filesystem11_Remove_dirEPKc_nid_postfix("missing"), "a missing directory");
}

void TestEntries() {
    Prepare();
    Require(_ZNSt8__sce_v210filesystem7_UnlinkEPKc_nid_postfix("f/x") == NotDirectory, "unlink below a file");
    Require(_ZNSt8__sce_v210filesystem7_UnlinkEPKc_nid_postfix("f") == 0 && !std::filesystem::exists("f"), "unlink a file");
    Require(_ZNSt8__sce_v210filesystem7_UnlinkEPKc_nid_postfix("missing") == NoSuchFile, "unlink a missing file");
    Require(_ZNSt8__sce_v210filesystem7_RenameEPKcS2__nid_postfix("g", "renamed") == 0 && Read("renamed").size() == 3 && !std::filesystem::exists("g"), "rename a file");
    Require(_ZNSt8__sce_v210filesystem7_RenameEPKcS2__nid_postfix("missing", "new") == NoSuchFile, "rename a missing file");
    Require(_ZNSt8__sce_v210filesystem7_RenameEPKcS2__nid_postfix("renamed", "d") == IsDirectory && std::filesystem::exists("renamed"), "rename a file over a directory");
    Require(_ZNSt8__sce_v210filesystem7_RenameEPKcS2__nid_postfix("renamed", "missing/x") == NoSuchFile, "rename into a missing directory");
    Require(_ZNSt8__sce_v210filesystem7_ResizeEPKcm_nid_postfix("renamed", 100000) == 0 && Status("renamed").st_size == 100000, "grow a file");
    Require(_ZNSt8__sce_v210filesystem7_ResizeEPKcm_nid_postfix("renamed", 2) == 0 && Status("renamed").st_size == 2, "shrink a file");
    Require(_ZNSt8__sce_v210filesystem7_ResizeEPKcm_nid_postfix("renamed", 0x8000000000000000ull) == Invalid, "a size above the signed range");
    Require(_ZNSt8__sce_v210filesystem7_ResizeEPKcm_nid_postfix("d", 0) == IsDirectory, "resize a directory");
    Require(_ZNSt8__sce_v210filesystem7_ResizeEPKcm_nid_postfix("missing", 0) == NoSuchFile, "resize a missing file");
    Require(_ZNSt8__sce_v210filesystem6_ChmodEPKcNS0_5permsE_nid_postfix("renamed", 0444) == 0 && (Status("renamed").st_mode & 0222) == 0, "make a file read only");
    Require(_ZNSt8__sce_v210filesystem6_ChmodEPKcNS0_5permsE_nid_postfix("renamed", 0x1ffff) == 0 && (Status("renamed").st_mode & 0200) != 0, "only the low sixteen permission bits are passed");
    Require(_ZNSt8__sce_v210filesystem6_ChmodEPKcNS0_5permsE_nid_postfix("missing", 0444) == NoSuchFile, "change a missing file");
}

void TestCopy() {
    Prepare();
    const auto contents = Read("big");
    Require(_ZNSt8__sce_v210filesystem10_Copy_fileEPKcS2__nid_postfix("big", "copy") == 0 && Read("copy") == contents, "copy a file larger than one block");
    Require(_ZNSt8__sce_v210filesystem10_Copy_fileEPKcS2__nid_postfix("g", "copy") == 0 && Read("copy") == Read("g"), "copy over a longer file truncates it");
    Require(_ZNSt8__sce_v210filesystem10_Copy_fileEPKcS2__nid_postfix("ro", "new") == 0 && Read("new") == Read("ro"), "copy a read-only file");
    Require((Status("new").st_mode & 07777) == (Status("ro").st_mode & 07777), "the copy takes the permissions of its source");
    Require(_ZNSt8__sce_v210filesystem10_Copy_fileEPKcS2__nid_postfix("missing", "made") == NoSuchFile && !std::filesystem::exists("made"), "a missing source creates nothing");
    Require(_ZNSt8__sce_v210filesystem10_Copy_fileEPKcS2__nid_postfix("f", "missing/x") == NoSuchFile, "a target in a missing directory");
    Require(_ZNSt8__sce_v210filesystem10_Copy_fileEPKcS2__nid_postfix("f", "d") == IsDirectory, "a directory target");
    Require(_ZNSt8__sce_v210filesystem10_Copy_fileEPKcS2__nid_postfix("d", "made") == IsDirectory, "a directory source");
}

void TestUnsupported() {
    std::uint64_t space[4] = {1, 2, 3, 4};
    Require(_ZNSt8__sce_v210filesystem8_StatvfsEPKcRNS0_10space_infoE_nid_postfix("f", space) == NoSystemCall, "statvfs is not supported");
    Require(space[0] == UINT64_MAX && space[1] == UINT64_MAX && space[2] == UINT64_MAX && space[3] == 4, "statvfs reports unknown space and writes three fields");
    Require(_ZNSt8__sce_v210filesystem13_Current_set2EPKc_nid_postfix("/") == 0, "the root stays the current directory");
    for (const char* path : {"", ".", "d", "//", "/d", "f/"}) Require(_ZNSt8__sce_v210filesystem13_Current_set2EPKc_nid_postfix(path) == NotSupported, "any other current directory is not supported");
}

}

int main() {
    const auto directory = AppArchiveFixture::EnterFreshRoot("anyps5-sce-v2-filesystem-calls");
    TestStatus();
    TestSizes();
    TestTimes();
    TestDirectories();
    TestEntries();
    TestCopy();
    TestUnsupported();
    Prepare();
    std::error_code ignored;
    std::filesystem::current_path(std::filesystem::temp_directory_path(), ignored);
    std::filesystem::remove_all(directory, ignored);
    return 0;
}
