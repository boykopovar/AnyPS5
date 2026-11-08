#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <system_error>

namespace {

constexpr int TypeNotFound = -1;
constexpr int TypeRegular = 1;
constexpr int TypeDirectory = 2;
constexpr int NoSuchFile = 2;
constexpr int IsDirectory = 21;
constexpr int NotSupported = 45;
constexpr int NoSystemCall = 78;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "unversioned filesystem: %s\n", message);
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
    for (const char* name : {"f", "g", "big", "d", "link", "copy", "renamed", "new"}) std::filesystem::remove_all(name, ignored);
    Write("f", 7, 1);
    Write("g", 3, 2);
    Write("big", 200000, 3);
    std::filesystem::create_directory("d");
}

void TestStatus() {
    Prepare();
    int perms = -1;
    Require(_ZNSt10filesystem5_StatEPKcPNS_5permsE_nid_postfix("f", &perms) == TypeRegular, "_Stat of a regular file");
    Require(perms == (Status("f").st_mode & 07777), "_Stat writes the permission bits");
    Require(_ZNSt10filesystem5_StatEPKcPNS_5permsE_nid_postfix("d", nullptr) == TypeDirectory, "_Stat of a directory without a perms output");
    perms = 0x1234;
    Require(_ZNSt10filesystem5_StatEPKcPNS_5permsE_nid_postfix("missing", &perms) == TypeNotFound && perms == 0x1234, "_Stat of a missing path");
    Require(_ZNSt10filesystem5_StatEPKcPNS_5permsE_nid_postfix("f/x", &perms) == TypeNotFound && perms == 0x1234, "_Stat below a file");
    Require(_ZNSt10filesystem6_LstatEPKcPNS_5permsE_nid_postfix("f", &perms) == TypeRegular && perms == (Status("f").st_mode & 07777), "_Lstat of a regular file");
    Require(_ZNSt10filesystem6_LstatEPKcPNS_5permsE_nid_postfix("d", nullptr) == TypeDirectory, "_Lstat of a directory");
    Require(_ZNSt10filesystem6_LstatEPKcPNS_5permsE_nid_postfix("missing", nullptr) == TypeNotFound, "_Lstat of a missing path");
    std::error_code link;
    std::filesystem::create_symlink("f", "link", link);
    if (!link) Require(_ZNSt10filesystem6_LstatEPKcPNS_5permsE_nid_postfix("link", nullptr) == TypeRegular, "_Lstat follows a link as the overload with an error output does");
}

void TestSizeAndTime() {
    Prepare();
    Require(_ZNSt10filesystem10_File_sizeEPKc_nid_postfix("big") == 200000, "_File_size of a file");
    Require(_ZNSt10filesystem10_File_sizeEPKc_nid_postfix("missing") == UINT64_MAX, "_File_size of a missing file");
    const struct {
        std::int64_t microseconds, seconds;
    } times[] = {{1700000000123456, 1700000000}, {-1500001, -2}, {999999, 0}, {-1, -1}};
    for (const auto& time : times) {
        Require(_ZNSt10filesystem20_Set_last_write_timeEPKcl_nid_postfix("f", time.microseconds), "_Set_last_write_time of a file");
        Require(_ZNSt10filesystem16_Last_write_timeEPKc_nid_postfix("f") == time.seconds, "_Last_write_time returns the floored seconds");
    }
    Require(!_ZNSt10filesystem20_Set_last_write_timeEPKcl_nid_postfix("missing", 0), "_Set_last_write_time of a missing file");
    Require(_ZNSt10filesystem16_Last_write_timeEPKc_nid_postfix("missing") == -1, "_Last_write_time of a missing file");
}

void TestEntries() {
    Prepare();
    Require(_ZNSt10filesystem7_RenameEPKcS1__nid_postfix("g", "renamed") == 0 && Read("renamed").size() == 3 && !std::filesystem::exists("g"), "_Rename of a file");
    Require(_ZNSt10filesystem7_RenameEPKcS1__nid_postfix("missing", "new") == NoSuchFile, "_Rename of a missing file");
    Require(_ZNSt10filesystem7_RenameEPKcS1__nid_postfix("renamed", "d") == IsDirectory && std::filesystem::exists("renamed"), "_Rename over a directory");
    Require(_ZNSt10filesystem7_ResizeEPKcm_nid_postfix("renamed", 100000) == 0 && Status("renamed").st_size == 100000, "_Resize grows a file");
    Require(_ZNSt10filesystem7_ResizeEPKcm_nid_postfix("renamed", 2) == 0 && Status("renamed").st_size == 2, "_Resize shrinks a file");
    Require(_ZNSt10filesystem7_ResizeEPKcm_nid_postfix("missing", 0) == NoSuchFile, "_Resize of a missing file");
    const auto contents = Read("big");
    Require(_ZNSt10filesystem10_Copy_fileEPKcS1__nid_postfix("big", "copy") == 0 && Read("copy") == contents, "_Copy_file of a file larger than one block");
    Require(_ZNSt10filesystem10_Copy_fileEPKcS1__nid_postfix("missing", "new") == NoSuchFile && !std::filesystem::exists("new"), "_Copy_file of a missing source");
    Require(_ZNSt10filesystem10_Copy_fileEPKcS1__nid_postfix("f", "d") == IsDirectory, "_Copy_file onto a directory");
}

void TestUnsupported() {
    std::uint64_t space[4] = {1, 2, 3, 4};
    Require(_ZNSt10filesystem8_StatvfsEPKcRNS_10space_infoE_nid_postfix("f", space) == NoSystemCall, "_Statvfs is not supported");
    Require(space[0] == UINT64_MAX && space[1] == UINT64_MAX && space[2] == UINT64_MAX && space[3] == 4, "_Statvfs reports unknown space in three fields");
    Require(_ZNSt10filesystem13_Current_set2EPKc_nid_postfix("/") == 0, "_Current_set2 keeps the root");
    Require(_ZNSt10filesystem13_Current_set2EPKc_nid_postfix("d") == NotSupported, "_Current_set2 refuses any other directory");
}

} // namespace

int main() {
    std::error_code ignored;
    const auto directory = std::filesystem::temp_directory_path() / ("anyps5-unversioned-filesystem-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(directory);
    std::filesystem::current_path(directory);
    TestStatus();
    TestSizeAndTime();
    TestEntries();
    TestUnsupported();
    std::filesystem::current_path(std::filesystem::temp_directory_path(), ignored);
    std::filesystem::remove_all(directory, ignored);
    return 0;
}
