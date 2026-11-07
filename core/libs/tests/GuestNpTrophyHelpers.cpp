#include "prx/libSceNpTrophy/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

using WordsFn = int (APS5_VABI*)(const std::uint32_t*);
using BytesFn = int (APS5_VABI*)(const std::uint8_t*);
using FlagArrayFn = int (APS5_VABI*)(const std::uint8_t*, std::uint8_t*);

template <typename Function>
Function Resolve(const char* name) {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(LoadLibraryA("libSceNpTrophy.prx"), name));
#else
    void* symbol = dlsym(RTLD_DEFAULT, name);
#endif
    if (!symbol) {
        std::fprintf(stderr, "%s is not exported by libSceNpTrophy\n", name);
        std::abort();
    }
    return reinterpret_cast<Function>(symbol);
}

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "NpTrophy helpers: %s\n", what);
    std::abort();
}

}

int main() {
    const auto numInfoGetTotal = Resolve<WordsFn>("sceNpTrophyNumInfoGetTotal");
    const auto groupArrayGetNum = Resolve<WordsFn>("sceNpTrophyGroupArrayGetNum");
    const auto setInfoGetTrophyNum = Resolve<BytesFn>("sceNpTrophySetInfoGetTrophyNum");
    const auto setInfoGetTrophyFlagArray = Resolve<FlagArrayFn>("sceNpTrophySetInfoGetTrophyFlagArray");
    const auto configHasGroupFeature = Resolve<BytesFn>("sceNpTrophyConfigHasGroupFeature");

    const std::uint32_t counts[4] = {1, 20, 300, 4000};
    Require(numInfoGetTotal(counts) == 4321, "the four counts were not summed");
    const std::uint32_t wrapping[4] = {0xFFFFFFFFu, 2, 0, 0};
    Require(numInfoGetTotal(wrapping) == 1, "the sum did not wrap at 32 bits");

    const std::uint32_t groups[4] = {0x12345678u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
    Require(groupArrayGetNum(groups) == 13, "the group count read more than the first word");
    const std::uint32_t allGroups[4] = {0xFFFFFFFFu, 0, 0, 0};
    Require(groupArrayGetNum(allGroups) == 32, "32 groups were not counted");

    std::uint8_t setInfo[128];
    std::memset(setInfo, 0x0F, 64);
    std::memset(setInfo + 64, 0xFF, 64);
    Require(setInfoGetTrophyNum(setInfo) == 256, "the trophy count read past the four flag arrays");

    std::memset(setInfo, 0, sizeof(setInfo));
    std::memset(setInfo + 32, 0xFF, 16);
    std::uint8_t flags[32];
    std::memset(flags, 0xA5, sizeof(flags));
    Require(setInfoGetTrophyFlagArray(setInfo, flags) == 128, "a full flag array did not return 128 set bits");
    for (std::size_t index = 0; index < 16; ++index) Require(flags[index] == 0xFF, "one full flag array did not fill the output");
    for (std::size_t index = 16; index < 32; ++index) Require(flags[index] == 0xA5, "bytes past the 16-byte output were written");
    std::memset(setInfo, 0, sizeof(setInfo));
    setInfo[0] = 0x01;
    setInfo[16] = 0x02;
    setInfo[32] = 0x04;
    setInfo[48 + 15] = 0x80;
    Require(setInfoGetTrophyFlagArray(setInfo, flags) == 4 && flags[0] == 0x07 && flags[15] == 0x80 && flags[1] == 0, "the four flag arrays were not combined with OR, or the set bits were not counted");
    std::memset(setInfo, 0, sizeof(setInfo));
    Require(setInfoGetTrophyFlagArray(setInfo, flags) == 0 && flags[0] == 0, "an empty set did not return 0");
    setInfo[0] = 0x01;
    Require(setInfoGetTrophyFlagArray(setInfo, flags) == 1, "a single trophy did not return 1");

    std::uint8_t config[0x100] = {};
    Require(configHasGroupFeature(nullptr) == 0, "a null config did not answer 0");
    config[0x90] = 0x03;
    Require(configHasGroupFeature(config) == 1, "bit 0 of the feature byte was not reported");
    config[0x90] = 0x02;
    Require(configHasGroupFeature(config) == 0, "a bit other than bit 0 was reported");

    std::puts("NpTrophy helper tests passed");
    return 0;
}
