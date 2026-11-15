#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <string_view>

namespace {

constexpr std::uint32_t Sentinel = 0xA5A5A5A5u;
constexpr char FlagName[] = "SceUserServiceCommonEventFlag";
constexpr std::uint32_t SharedFlag = 0x100;

void Require(bool value) { if (!value) std::abort(); }

std::uint32_t Read() {
    std::uint32_t setting = Sentinel;
    Require(sceSystemServiceGetPlatformPrivacySetting(&setting) == 0);
    return setting;
}

std::uint32_t ReadWithFlag(std::uint64_t pattern) {
    KernelEventFlag flag = nullptr;
    Require(sceKernelCreateEventFlag(&flag, FlagName, SharedFlag, pattern, nullptr) == 0);
    const std::uint32_t first = Read();
    Require(Read() == first);
    Require(sceKernelDeleteEventFlag(flag) == 0);
    return first;
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    const std::string_view mode = argv[1];
    if (mode == "ps5") {
        Require(Read() == Sentinel);
        Require(ReadWithFlag(1) == Sentinel);
        return 0;
    }
    Require(mode == "ps4");
    Require(Read() == 0);
    Require(ReadWithFlag(0) == 0);
    Require(ReadWithFlag(2) == 0);
    Require(ReadWithFlag(1) == 1);
    Require(ReadWithFlag(3) == 1);
    Require(Read() == 0);
    KernelEventFlag flag = nullptr;
    Require(sceKernelCreateEventFlag(&flag, FlagName, SharedFlag, 0, nullptr) == 0);
    Require(Read() == 0);
    Require(sceKernelSetEventFlag(flag, 1) == 0);
    Require(Read() == 1);
    Require(sceKernelClearEventFlag(flag, ~std::uint64_t{1}) == 0);
    Require(Read() == 0);
    Require(sceKernelDeleteEventFlag(flag) == 0);
}
