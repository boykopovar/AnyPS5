#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "pathconf check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int GuestEnoent = 2;
constexpr int GuestEinval = 22;

constexpr int PcNameMax = 4;
constexpr int PcPathMax = 5;

extern "C" {
std::int64_t APS5_VABI pathconf_nid_postfix(const char* path, int name);
void APS5_VABI AddPathAlias_nid_no_patch(const char* alias, const char* path);
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-pathconf-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));

    AddPathAlias_nid_no_patch("/anyps5-pathconf-test", root.string().c_str());

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix("/anyps5-pathconf-test", PcNameMax) == 255);
    Require(*__error_nid_postfix() == 0);

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix("/anyps5-pathconf-test", PcPathMax) == 1024);
    Require(*__error_nid_postfix() == 0);

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix("/anyps5-pathconf-test/nonexistent", PcNameMax) == -1);
    Require(*__error_nid_postfix() == GuestEnoent);

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix("/anyps5-pathconf-test", 999999) == -1);
    Require(*__error_nid_postfix() == GuestEinval);

    std::filesystem::remove_all(root);
}
