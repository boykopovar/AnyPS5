#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <string>

namespace {

constexpr int NoSuchFile = 2;
constexpr int TypeNotFound = -1;
constexpr int TypeRegular = 1;
constexpr int TypeDirectory = 2;
constexpr int TypeUnknown = 8;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "sce v2 filesystem: %s\n", message);
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
    Require(_ZNSt8__sce_v210filesystem5_StatEPKcPNS0_5permsEPi_nid_postfix(At("file").c_str(), &perms, &error) == TypeRegular && error == 0, "regular file");
    FileStat status{};
    Require(stat_nid_postfix(At("file").c_str(), &status) == 0 && perms == (status.st_mode & 07777), "permission bits");
    Require(_ZNSt8__sce_v210filesystem5_StatEPKcPNS0_5permsEPi_nid_postfix(root.c_str(), nullptr, &error) == TypeDirectory && error == 0, "directory");
    perms = 1234;
    Require(_ZNSt8__sce_v210filesystem5_StatEPKcPNS0_5permsEPi_nid_postfix(At("missing").c_str(), &perms, &error) == TypeNotFound && error == NoSuchFile && perms == 1234, "missing path");
}

void TestDirectories() {
    std::filesystem::create_directory(At("made"));
    char name[260];
    int error = -1;
    int type = -1;
    Require(_ZNSt8__sce_v210filesystem9_Open_dirERA260_cPKcRiRNS0_9file_typeE_nid_postfix(name, At("made").c_str(), &error, &type) == nullptr && error == 0 && name[0] == '\0', "an empty directory gives no handle");
    Require(_ZNSt8__sce_v210filesystem9_Open_dirERA260_cPKcRiRNS0_9file_typeE_nid_postfix(name, At("missing").c_str(), &error, &type) == nullptr && error == NoSuchFile, "a missing directory");
    Write(At("made/one"), 1);
    Write(At("made/two"), 2);
    void* handle = _ZNSt8__sce_v210filesystem9_Open_dirERA260_cPKcRiRNS0_9file_typeE_nid_postfix(name, At("made").c_str(), &error, &type);
    Require(handle != nullptr && error == 0 && type == TypeUnknown, "open a directory");
    std::set<std::string> names{name};
    type = 0;
    Require(_ZNSt8__sce_v210filesystem9_Read_dirERA260_cPvRNS0_9file_typeE_nid_postfix(name, handle, &type) == name && type == TypeUnknown, "read a directory");
    names.insert(name);
    Require(names == std::set<std::string>{"one", "two"}, "entries without dot entries");
    Require(_ZNSt8__sce_v210filesystem9_Read_dirERA260_cPvRNS0_9file_typeE_nid_postfix(name, handle, &type)[0] == '\0' && type == TypeUnknown, "end of the directory");
    Require(_ZNSt8__sce_v210filesystem10_Close_dirEPv_nid_postfix(handle) == 0, "close a directory");
}

void TestCurrent() {
    char current[260];
    std::memset(current, 0x5a, sizeof(current));
    Require(_ZNSt8__sce_v210filesystem12_Current_getERA260_c_nid_postfix(current), "current directory");
    Require(std::strcmp(current, "/") == 0, "current directory is the sandbox root");
    Require(static_cast<unsigned char>(current[2]) == 0x5a, "current directory writes two bytes");
}

}

int main() {
    root = "anyps5-sce-v2-filesystem-" + std::to_string(std::random_device{}());
    std::filesystem::create_directory(root);
    TestStat();
    TestDirectories();
    TestCurrent();
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return 0;
}
