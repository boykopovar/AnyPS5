#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestFilesystemPrimitives.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

}

int main() {
    const auto root = std::filesystem::absolute("anyps5-lstat-link-test");
    std::error_code error;
    std::filesystem::remove_all(root, error);
    Require(std::filesystem::create_directory(root, error) && !error, "create root");
    {
        std::ofstream target(root / "target.txt", std::ios::binary);
        target << "payload";
    }
    std::filesystem::create_symlink("target.txt", root / "link", error);
    Require(!error, "create file link");
    std::filesystem::create_symlink("missing.txt", root / "dangling", error);
    Require(!error, "create dangling link");
    AddPathAlias_nid_no_patch("lstat-link-test", root.string().c_str());

    int perms = 0;
    int code = -1;
    Require(_ZNSt10filesystem6_LstatEPKcPNS_5permsEPi_nid_postfix("/lstat-link-test/link", &perms, &code) == GuestFilesystem::TypeSymlink && code == 0,
        "_Lstat reports a file link as a symbolic link");
    Require(_ZNSt10filesystem6_LstatEPKcPNS_5permsEPi_nid_postfix("/lstat-link-test/dangling", &perms, &code) == GuestFilesystem::TypeSymlink && code == 0,
        "_Lstat reports a dangling link as a symbolic link");
    Require(_ZNSt10filesystem6_LstatEPKcPNS_5permsEPi_nid_postfix("/lstat-link-test/target.txt", &perms, &code) == GuestFilesystem::TypeRegular && code == 0,
        "_Lstat reports a regular file");
    Require(_ZNSt10filesystem6_LstatEPKcPNS_5permsEPi_nid_postfix("/lstat-link-test/absent", &perms, &code) == GuestFilesystem::TypeNotFound && code == 2,
        "_Lstat reports a missing entry");

    Require(_ZNSt10filesystem5_StatEPKcPNS_5permsEPi_nid_postfix("/lstat-link-test/link", &perms, &code) == GuestFilesystem::TypeRegular && code == 0,
        "_Stat follows a file link");
    Require(_ZNSt10filesystem5_StatEPKcPNS_5permsEPi_nid_postfix("/lstat-link-test/dangling", &perms, &code) == GuestFilesystem::TypeNotFound && code == 2,
        "_Stat reports a dangling link as missing");

    std::filesystem::remove_all(root, error);
    return 0;
}
