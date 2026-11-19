#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestErrno.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "KERNEL SYS APP CATEGORY: %s\n", message);
        std::abort();
    }
}

}

int main() {
    const auto original = std::filesystem::current_path();
    const auto directory = original / ("kernel-sys-app-category-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(directory), "the test directory could not be created");
    std::filesystem::current_path(directory);
    std::filesystem::create_directories("app0/sce_sys");
    {
        std::ofstream param("app0/sce_sys/param.json", std::ios::binary);
        param << R"({"titleId":"PPSA00000","applicationCategoryType":7,"localizedParameters":{"en-US":{"titleName":"Example"}}})";
        Require(static_cast<bool>(param), "param.json could not be written");
    }

    const auto getAppCategoryType = reinterpret_cast<int (APS5_VABI*)(int, std::uint32_t*)>(&sceKernelGetAppCategoryType);
    std::array<std::uint32_t, 2> output{0x5a5a5a5au, 0x5a5a5a5au};
    Require(getAppCategoryType(getpid_nid_postfix(), output.data()) == 0, "the query failed");
    Require(output[0] == 7u, "the category is not the applicationCategoryType from param.json");
    Require(output[1] == 0x5a5a5a5au, "the query wrote past the category");
    Require(getAppCategoryType(getpid_nid_postfix(), nullptr) == static_cast<int>(0x80020016), "a null output is not EINVAL");
    output[0] = 0x5a5a5a5au;
    Require(getAppCategoryType(0, output.data()) == 0 && output[0] == 7u, "pid 0 is not the calling process");
    for (const int pid : {-1, -2, static_cast<int>(0x80000000)}) {
        output[0] = 0x5a5a5a5au;
        *__error_nid_postfix() = 0;
        Require(getAppCategoryType(pid, output.data()) == static_cast<int>(0x80020003), "a negative pid is not ESRCH");
        Require(*__error_nid_postfix() == 3 && output[0] == 0x5a5a5a5au, "a negative pid set the wrong errno or wrote the output");
        Require(getAppCategoryType(pid, nullptr) == static_cast<int>(0x80020016), "the null check does not come first");
    }
    Require(getAppCategoryType(getpid_nid_postfix(), reinterpret_cast<std::uint32_t*>(0x10)) == static_cast<int>(0x8002000e),
            "an unwritable output is not EFAULT");

    std::filesystem::current_path(original);
    std::filesystem::remove_all(directory);
    std::puts("KERNEL SYS APP CATEGORY tests passed");
    return 0;
}
