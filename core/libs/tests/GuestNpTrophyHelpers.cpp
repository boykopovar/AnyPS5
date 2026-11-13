#include "prx/libSceNpTrophy/include/Api.hpp"
#include "prx/libSceNpTrophy/include/InventoryExports.hpp"

#include <algorithm>
#include <array>
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
    Require(configHasGroupFeature(reinterpret_cast<const std::uint8_t*>(0x10)) == 0, "an unreadable config did not answer 0");
    config[0x90] = 0x03;
    Require(configHasGroupFeature(config) == 1, "bit 0 of the feature byte was not reported");
    config[0x90] = 0x02;
    Require(configHasGroupFeature(config) == 0, "a bit other than bit 0 was reported");

    const std::array<std::uint32_t, 20> shared = {
        0x8002002du, 0x80020060u, 0x80551611u, 0x80551615u, 0x80551616u,
        0x80551618u, 0x80551627u, 0x8055168cu, 0x805516a2u, 0x805516a3u,
        0x805516a7u, 0x805516aeu, 0x805516b2u, 0x805516b7u, 0x805516b8u,
        0x805516c4u, 0x805516ceu, 0x805516e9u, 0x805516f4u, 0x805516fbu
    };
    const auto inShared = [&](std::uint32_t code) { return std::find(shared.begin(), shared.end(), code) != shared.end(); };
    const auto check = [&](std::uint32_t code) {
        const int result = static_cast<int>(code);
        Require(libSceNpTrophyUnknown_FJZW2oHUHFk_nid_no_patch(result) == (code == 0x80551612u), "FJZW2oHUHFk is true only for 0x80551612");
        Require(libSceNpTrophyUnknown__plus_O9vU1CpGZA_nid_no_patch(result) == (code == 0x8055169bu || code == 0x805516d1u), "+O9vU1CpGZA is true only for 0x8055169b and 0x805516d1");
        Require(libSceNpTrophyUnknown__plus_not13BEdVI_nid_no_patch(result) == (inShared(code) || code == 0x805516b4u), "+not13BEdVI is true only for its 21 codes");
        Require(libSceNpTrophyUnknown_n4AHGHb_minus_pfY_nid_no_patch(result) == inShared(code), "n4AHGHb-pfY is true only for its 20 codes");
    };
    for (std::uint32_t code = 0x80550000u; code <= 0x8055ffffu; ++code) check(code);
    for (std::uint32_t code = 0x80020000u; code <= 0x8002ffffu; ++code) check(code);
    for (std::uint32_t code : {0u, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu, 0x00551612u, 0x80561612u}) check(code);

    std::puts("NpTrophy helper tests passed");
    return 0;
}
