#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "fpathconf check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int GuestEbadf = 9;
constexpr int GuestEinval = 22;

constexpr int PcNameMax = 4;
constexpr int PcPathMax = 5;
constexpr int PcPipeBuf = 6;

extern "C" {
std::int64_t APS5_VABI fpathconf_nid_postfix(int fd, int name);
std::int64_t APS5_VABI pathconf_nid_postfix(const char* path, int name);
void APS5_VABI AddPathAlias_nid_no_patch(const char* alias, const char* path);
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-fpathconf-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    {
        std::ofstream file(root / "file.txt", std::ios::binary);
        file << "test";
        Require(static_cast<bool>(file));
    }

    AddPathAlias_nid_no_patch("/anyps5-fpathconf-test", root.string().c_str());
    const int fd = sceKernelOpen("/anyps5-fpathconf-test/file.txt", SCE_KERNEL_O_RDONLY, 0);
    Require(fd >= 0);

    *__error_nid_postfix() = 0;
    Require(fpathconf_nid_postfix(fd, PcNameMax) == 255);
    Require(*__error_nid_postfix() == 0);

    *__error_nid_postfix() = 0;
    Require(fpathconf_nid_postfix(fd, PcPathMax) == 1024);
    Require(*__error_nid_postfix() == 0);

    *__error_nid_postfix() = 0;
    Require(fpathconf_nid_postfix(fd, PcPipeBuf) == 512);
    Require(*__error_nid_postfix() == 0);

    *__error_nid_postfix() = 0;
    Require(fpathconf_nid_postfix(fd, 999999) == -1);
    Require(*__error_nid_postfix() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(fpathconf_nid_postfix(-1, PcNameMax) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);

    *__error_nid_postfix() = 0;
    Require(fpathconf_nid_postfix(99999, PcNameMax) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);

    Require(sceKernelClose(fd) == 0);

    *__error_nid_postfix() = 0;
    Require(fpathconf_nid_postfix(fd, PcNameMax) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);

    std::filesystem::remove_all(root);
}
