#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "KERNEL SYS APP CATEGORY: %s\n", message);
        std::abort();
    }
}

}

int main() {
    const auto getAppCategoryType = reinterpret_cast<int (APS5_VABI*)(int, std::uint32_t*)>(&sceKernelGetAppCategoryType);
    for (const int pid : {getpid_nid_postfix(), 0}) {
        std::array<std::uint8_t, 0x58> info{};
        Require(sceKernelGetAppInfo(pid, info.data()) == 0, "sceKernelGetAppInfo failed");
        std::uint32_t expected = 0;
        std::memcpy(&expected, info.data() + 0x50, sizeof(expected));

        std::array<std::uint32_t, 2> output{0x5a5a5a5au, 0x5a5a5a5au};
        Require(getAppCategoryType(pid, output.data()) == 0, "the query failed");
        Require(output[0] == expected, "the category is not the field at offset 0x50 of the app info record");
        Require(output[1] == 0x5a5a5a5au, "the query wrote past the category");
        Require(getAppCategoryType(pid, nullptr) == static_cast<int>(0x80020016), "a null output is not EINVAL");
    }

    std::puts("KERNEL SYS APP CATEGORY tests passed");
    return 0;
}
