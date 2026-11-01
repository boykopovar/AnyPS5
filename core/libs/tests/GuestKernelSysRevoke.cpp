#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <source_location>
#include <string>

extern "C" {
int* APS5_VABI __error_nid_postfix();
int APS5_VABI revoke_nid_postfix(const char* path);
}

namespace {

constexpr int Enoent = 2;
constexpr int Efault = 14;
constexpr int Enotdir = 20;
constexpr int Einval = 22;
constexpr int Enametoolong = 63;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

bool Fails(const std::string& path, int error) {
    return revoke_nid_postfix(path.c_str()) == -1 && Error() == error;
}

}

int main() {
    const auto root = std::filesystem::path("anyps5-revoke-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto file = (root / "file.txt").string();
    std::ofstream(file).put('x');

    Require(Fails(file, Einval));
    Require(Fails(root.string(), Einval));
    Require(Fails((root / "missing").string(), Enoent));
    Require(Fails(file + "/below", Enotdir));
    Require(Fails("", Enoent));
    Require(Fails(std::string(1024, 'a'), Enametoolong));
    Require(revoke_nid_postfix(nullptr) == -1 && Error() == Efault);

    std::filesystem::remove_all(root);
    std::puts("guest_kernel_sys_revoke passed");
    return 0;
}
