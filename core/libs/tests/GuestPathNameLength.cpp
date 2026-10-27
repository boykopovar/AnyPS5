#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

constexpr int ErrorNoEntry = 2;
constexpr int ErrorNameTooLong = 63;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

bool Fails(long result, int error) { return result == -1 && *__error_nid_postfix() == error; }

bool TooLong(const std::string& path) {
    unsigned char status[256];
    return Fails(stat_nid_postfix(path.c_str(), reinterpret_cast<FileStat*>(status)), ErrorNameTooLong);
}

}

int main() {
    unsigned char status[256];
    const std::string longName(256, 'n');
    const std::string longPath(1100, 'p');
    const std::string maxName(255, 'm');
    Require(Fails(stat_nid_postfix(longName.c_str(), reinterpret_cast<FileStat*>(status)), ErrorNameTooLong), "stat of a name above NAME_MAX (255) must fail with ENAMETOOLONG (63)");
    rmdir_nid_postfix("anyps5_name_length");

    Require(mkdir_nid_postfix("anyps5_name_length", 0755) == 0, "mkdir of the test directory failed");
    Require(Fails(stat_nid_postfix(("anyps5_name_length/" + longName).c_str(), reinterpret_cast<FileStat*>(status)), ErrorNameTooLong), "stat of a long final component below an existing directory must fail with ENAMETOOLONG (63)");
    Require(Fails(stat_nid_postfix(("anyps5_name_length/missing/" + longName).c_str(), reinterpret_cast<FileStat*>(status)), ErrorNoEntry), "stat of a long component below a missing directory must fail with ENOENT (2), the lookup stops there first");
    const std::string below = "anyps5_name_length/";
    const std::string name255(255, 'n');
    Require(!TooLong(below + name255), "a final component of exactly 255 bytes must not be too long");
    Require(!TooLong(below + name255 + "/x"), "a middle component of exactly 255 bytes must not be too long");
    Require(TooLong(below + longName + "/x"), "a middle component of 256 bytes must fail with ENAMETOOLONG (63)");
    Require(TooLong(below + longName + "/"), "a long component followed by a trailing slash must fail with ENAMETOOLONG (63)");
    Require(!TooLong(below + std::string(200, 'n') + "\\" + std::string(100, 'n')), "a backslash separates components here, so 200 + 100 bytes around it are accepted (FreeBSD would refuse the 301-byte name)");
    const std::string block = name255 + "/";
#ifndef __APPLE__
    Require(!TooLong(block + block + block + name255), "a path of exactly 1023 bytes must not be too long");
#endif
    Require(TooLong(block + block + block + block), "a path of exactly 1024 bytes must fail with ENAMETOOLONG (63)");
    Require(rmdir_nid_postfix("anyps5_name_length") == 0, "rmdir of the test directory failed");
    Require(Fails(stat_nid_postfix((longName + "/x").c_str(), reinterpret_cast<FileStat*>(status)), ErrorNameTooLong), "stat of a long first component must fail with ENAMETOOLONG (63)");
    Require(Fails(stat_nid_postfix(longPath.c_str(), reinterpret_cast<FileStat*>(status)), ErrorNameTooLong), "stat of a path of 1100 bytes must fail with ENAMETOOLONG (63)");
    Require(Fails(access_nid_postfix(longName.c_str(), 0), ErrorNameTooLong), "access of a long name must fail with ENAMETOOLONG (63)");
    Require(Fails(chflags_nid_postfix(longName.c_str(), 0), ErrorNameTooLong), "chflags of a long name must fail with ENAMETOOLONG (63)");
    Require(Fails(unlink_nid_postfix(longName.c_str()), ErrorNameTooLong), "unlink of a long name must fail with ENAMETOOLONG (63)");
    Require(Fails(mkdir_nid_postfix(longName.c_str(), 0755), ErrorNameTooLong), "mkdir of a long name must fail with ENAMETOOLONG (63)");
    Require(Fails(rmdir_nid_postfix(longName.c_str()), ErrorNameTooLong), "rmdir of a long name must fail with ENAMETOOLONG (63)");
    Require(Fails(open_nid_postfix(longName.c_str(), 0, 0), ErrorNameTooLong), "open of a long name must fail with ENAMETOOLONG (63)");
    Require(Fails(rename_nid_postfix(longName.c_str(), "anyps5_name_length_target"), ErrorNameTooLong), "rename from a long name must fail with ENAMETOOLONG (63)");
    Require(Fails(rename_nid_postfix("anyps5_name_length_source", longName.c_str()), ErrorNameTooLong), "rename to a long name must fail with ENAMETOOLONG (63)");

    Require(Fails(stat_nid_postfix(maxName.c_str(), reinterpret_cast<FileStat*>(status)), ErrorNoEntry), "a name of exactly 255 bytes is not too long");
    return 0;
}
