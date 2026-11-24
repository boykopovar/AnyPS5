#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <source_location>
#include <string>

namespace {

constexpr int FunctionUnlock = 0;
constexpr int FunctionLock = 1;
constexpr int FunctionTryLock = 2;
constexpr int FunctionTest = 3;
constexpr int GuestEinval = 22;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "lockf check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
        std::abort();
    }
}

}

int main() {
    std::random_device random;
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-lockf-test-" + std::to_string(random()));
    std::filesystem::create_directory(root);
    AddPathAlias_nid_no_patch("/anyps5-lockf-test", root.string().c_str());
    const int descriptor = open_nid_postfix("/anyps5-lockf-test/file.bin", SCE_KERNEL_O_RDWR | SCE_KERNEL_O_CREAT, 0600);
    Require(descriptor >= 0);

    Require(lockf_nid_postfix(descriptor, FunctionTest, 0) == 0);
    Require(lockf_nid_postfix(descriptor, FunctionTryLock, 10) == 0);
    Require(lockf_nid_postfix(descriptor, FunctionTest, 0) == 0);
    Require(lockf_nid_postfix(descriptor, FunctionUnlock, 10) == 0);
    Require(lockf_nid_postfix(descriptor, FunctionLock, 0) == 0);
    Require(lockf_nid_postfix(descriptor, FunctionUnlock, 0) == 0);

    *__error_nid_postfix() = 0;
    Require(lockf_nid_postfix(descriptor, 7, 0) == -1);
    Require(*__error_nid_postfix() == GuestEinval);

    Require(close_nid_postfix(descriptor) == 0);
    RemovePathAlias_nid_no_patch("/anyps5-lockf-test");
    std::filesystem::remove_all(root);
    return 0;
}
