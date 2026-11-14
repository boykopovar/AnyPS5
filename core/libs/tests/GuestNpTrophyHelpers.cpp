#include "prx/libSceNpTrophy/include/Api.hpp"
#include "prx/libSceNpTrophy/include/InventoryExports.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cerrno>
#include <stdexcept>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
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

template <typename TValue, std::size_t TCount, typename TFunction>
void CheckCountMemory(TFunction function, const std::array<TValue, TCount>& input, int expected, const char* name) {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t page = info.dwPageSize;
    auto* memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(memory != nullptr, "allocation failed");
    DWORD previous;
    Require(VirtualProtect(memory + page, page, PAGE_NOACCESS, &previous) != 0, "guard failed");
#else
    const long pageSize = sysconf(_SC_PAGESIZE);
    Require(pageSize > 0, "page size unavailable");
    const auto page = static_cast<std::size_t>(pageSize);
    auto* memory = static_cast<unsigned char*>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(memory != MAP_FAILED, "allocation failed");
    Require(mprotect(memory + page, page, PROT_NONE) == 0, "guard failed");
#endif
    constexpr auto extent = TCount * sizeof(TValue);
    for (const auto* pointer : {static_cast<const TValue*>(nullptr), reinterpret_cast<const TValue*>(memory + page), reinterpret_cast<const TValue*>(memory + page - extent / 2)}) {
        bool threw = false;
        try {
            function(pointer);
        } catch (const std::runtime_error& error) {
            threw = std::string(error.what()) == std::string(name) + " not implemented";
        }
        Require(threw, "unreadable input did not take the named unsupported path");
    }
    auto* exact = reinterpret_cast<TValue*>(memory + page - extent);
    std::memcpy(exact, input.data(), extent);
#ifdef _WIN32
    Require(VirtualProtect(memory, page, PAGE_READONLY, &previous) != 0, "read-only protection failed");
#else
    Require(mprotect(memory, page, PROT_READ) == 0, "read-only protection failed");
#endif
    errno = EDOM;
    Require(function(exact) == expected, "exact read-only input extent failed");
    Require(errno == EDOM, "successful query changed errno");
    Require(std::memcmp(exact, input.data(), extent) == 0, "query changed its input");
#ifdef _WIN32
    Require(VirtualFree(memory, 0, MEM_RELEASE) != 0, "release failed");
#else
    Require(munmap(memory, page * 2) == 0, "release failed");
#endif
}

}

int main(int argc, char** argv) {
    const auto numInfoGetTotal = Resolve<WordsFn>("sceNpTrophyNumInfoGetTotal");
    const auto groupArrayGetNum = Resolve<WordsFn>("sceNpTrophyGroupArrayGetNum");
    const auto setInfoGetTrophyNum = Resolve<BytesFn>("sceNpTrophySetInfoGetTrophyNum");
    const auto setInfoGetTrophyFlagArray = Resolve<FlagArrayFn>("sceNpTrophySetInfoGetTrophyFlagArray");
    const auto configHasGroupFeature = Resolve<BytesFn>("sceNpTrophyConfigHasGroupFeature");

    if (argc == 2) {
        if (std::strcmp(argv[1], "group-memory") == 0) {
            CheckCountMemory(groupArrayGetNum, std::array<std::uint32_t, 1>{0xffffffffu}, 32, "sceNpTrophyGroupArrayGetNum");
        } else if (std::strcmp(argv[1], "sum-memory") == 0) {
            CheckCountMemory(numInfoGetTotal, std::array<std::uint32_t, 4>{0xffffffffu, 2, 0, 0}, 1, "sceNpTrophyNumInfoGetTotal");
        } else if (std::strcmp(argv[1], "set-memory") == 0) {
            std::array<std::uint8_t, 64> input{};
            input.fill(0x0f);
            CheckCountMemory(setInfoGetTrophyNum, input, 256, "sceNpTrophySetInfoGetTrophyNum");
        } else {
            Require(false, "unknown test mode");
        }
        return 0;
    }

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
