#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <string>
#include <system_error>

namespace {

constexpr int NoSuchFile = 2;
constexpr int TypeNotFound = -1;
constexpr int TypeRegular = 1;
constexpr int TypeDirectory = 2;
constexpr int TypeUnknown = 8;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "filesystem: %s\n", message);
    std::abort();
}

std::string root;

std::string At(const char* name) {
    return root + "/" + name;
}

void Write(const std::string& path, std::size_t bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << std::string(bytes, 'x');
}

void TestStat() {
    Write(At("file"), 7);
    int perms = -1;
    int error = -1;
    Require(_ZNSt10filesystem5_StatEPKcPNS_5permsEPi_nid_postfix(At("file").c_str(), &perms, &error) == TypeRegular && error == 0, "regular file");
    FileStat status{};
    Require(stat_nid_postfix(At("file").c_str(), &status) == 0 && perms == (status.st_mode & 07777), "permission bits");
    Require(_ZNSt10filesystem5_StatEPKcPNS_5permsEPi_nid_postfix(root.c_str(), nullptr, &error) == TypeDirectory && error == 0, "directory");
    perms = 1234;
    Require(_ZNSt10filesystem5_StatEPKcPNS_5permsEPi_nid_postfix(At("missing").c_str(), &perms, &error) == TypeNotFound && error == NoSuchFile && perms == 1234, "missing path");
    Require(_ZNSt10filesystem5_StatEPKcPNS_5permsEPi_nid_postfix(At("file/below").c_str(), nullptr, &error) == TypeNotFound, "path below a file");
    Require(_ZNSt10filesystem6_LstatEPKcPNS_5permsEPi_nid_postfix(At("file").c_str(), nullptr, &error) == TypeRegular && error == 0, "lstat of a regular file");
    std::error_code link;
    std::filesystem::create_symlink("file", At("link"), link);
    if (!link) Require(_ZNSt10filesystem6_LstatEPKcPNS_5permsEPi_nid_postfix(At("link").c_str(), nullptr, &error) == TypeRegular, "lstat follows a link as the console does");
}

void TestSizeAndTime() {
    int error = 0;
    Require(_ZNSt10filesystem10_File_sizeEPKcPi_nid_postfix(At("file").c_str(), &error) == 7 && error == 0, "file size");
    error = 0;
    Require(_ZNSt10filesystem10_File_sizeEPKcPi_nid_postfix(At("missing").c_str(), &error) == UINT64_MAX && error == NoSuchFile, "size of a missing file");
    std::int64_t seconds = 0;
    std::int64_t nanoseconds = -1;
    FileStat status{};
    Require(stat_nid_postfix(At("file").c_str(), &status) == 0, "stat for time");
    Require(_ZNSt10filesystem16_Last_write_timeEPKcPlS2__nid_postfix(At("file").c_str(), &seconds, &nanoseconds) == 0, "last write time");
    Require(seconds == status.st_mtim.tv_sec && nanoseconds == status.st_mtim.tv_nsec, "last write time matches stat");
    seconds = 5;
    Require(_ZNSt10filesystem16_Last_write_timeEPKcPlS2__nid_postfix(At("missing").c_str(), &seconds, &nanoseconds) == NoSuchFile && seconds == 5, "time of a missing file");
}

void TestDirectories() {
    Require(_ZNSt10filesystem9_Make_dirEPKcS1__nid_postfix(At("made").c_str(), nullptr) == 1, "make a directory");
    Require(_ZNSt10filesystem9_Make_dirEPKcS1__nid_postfix(At("made").c_str(), nullptr) == 0, "make an existing directory");
    Require(_ZNSt10filesystem9_Make_dirEPKcS1__nid_postfix(At("absent/made").c_str(), nullptr) == -NoSuchFile, "make below a missing directory");
    Require(_ZNSt10filesystem9_Make_dirEPKcS1__nid_postfix(At("copy").c_str(), At("missing").c_str()) == -NoSuchFile, "copy the mode of a missing directory");
    Require(_ZNSt10filesystem9_Make_dirEPKcS1__nid_postfix(At("copy").c_str(), At("made").c_str()) == 1, "copy the mode of a directory");
    Require(_ZNSt10filesystem11_Remove_dirEPKc_nid_postfix(At("copy").c_str()), "remove a directory");
    Require(!_ZNSt10filesystem11_Remove_dirEPKc_nid_postfix(At("copy").c_str()), "remove a missing directory");
    char name[260];
    int error = -1;
    int type = -1;
    Require(_ZNSt10filesystem9_Open_dirERA260_cPKcRiRNS_9file_typeE_nid_postfix(name, At("made").c_str(), &error, &type) == nullptr && error == 0 && name[0] == '\0', "an empty directory gives no handle");
    Require(_ZNSt10filesystem9_Open_dirERA260_cPKcRiRNS_9file_typeE_nid_postfix(name, At("missing").c_str(), &error, &type) == nullptr && error == NoSuchFile, "a missing directory");
    Write(At("made/one"), 1);
    Write(At("made/two"), 2);
    void* handle = _ZNSt10filesystem9_Open_dirERA260_cPKcRiRNS_9file_typeE_nid_postfix(name, At("made").c_str(), &error, &type);
    Require(handle != nullptr && error == 0 && type == TypeUnknown, "open a directory");
    std::set<std::string> names{name};
    type = 0;
    Require(_ZNSt10filesystem9_Read_dirERA260_cPvRNS_9file_typeE_nid_postfix(name, handle, &type) == name && type == TypeUnknown, "read a directory");
    names.insert(name);
    Require(names == std::set<std::string>{"one", "two"}, "entries without dot entries");
    Require(_ZNSt10filesystem9_Read_dirERA260_cPvRNS_9file_typeE_nid_postfix(name, handle, &type)[0] == '\0' && type == TypeUnknown, "end of the directory");
    Require(_ZNSt10filesystem10_Close_dirEPv_nid_postfix(handle) == 0, "close a directory");
}

void TestFiles() {
    Require(_ZNSt10filesystem6_ChmodEPKcNS_5permsE_nid_postfix(At("file").c_str(), 0600) == 0, "chmod");
    Require(_ZNSt10filesystem6_ChmodEPKcNS_5permsE_nid_postfix(At("missing").c_str(), 0600) == -1, "chmod of a missing file");
    Require(_ZNSt10filesystem7_UnlinkEPKc_nid_postfix(At("made/one").c_str()) == 0, "unlink");
    Require(_ZNSt10filesystem7_UnlinkEPKc_nid_postfix(At("made/one").c_str()) == NoSuchFile, "unlink a missing file");
    char current[260];
    char expected[256];
    Require(getcwd_nid_postfix(expected, sizeof(expected)) != nullptr, "getcwd");
    Require(_ZNSt10filesystem12_Current_getERA260_c_nid_postfix(current) && std::strcmp(current, expected) == 0, "current directory");
}

}

int main() {
    root = "anyps5-libc-filesystem-" + std::to_string(std::random_device{}());
    std::filesystem::create_directory(root);
    TestStat();
    TestSizeAndTime();
    TestDirectories();
    TestFiles();
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return 0;
}
