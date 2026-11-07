#include "prx/libSceAppContent/include/Api.hpp"
#include "SceTypes.hpp"
#include "ProcessParametersFixture.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr int NotInitialized = static_cast<int>(0x80D90001u);
constexpr int Parameter = static_cast<int>(0x80D90002u);
constexpr int NotFound = static_cast<int>(0x80D90005u);
constexpr std::uint32_t Untouched = 0x5a5a5a5au;

using CheckBundleLicenseFn = int (APS5_VABI*)(std::uint32_t, const NpUnifiedEntitlementLabel*);
using GetGameTrialsFlagFn = int (APS5_VABI*)(std::uint32_t*);
using GetPlayableStatusFn = int (APS5_VABI*)(std::uint32_t, const NpUnifiedEntitlementLabel*, std::uint32_t*);

template <typename Function>
Function Resolve(const char* name) {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(LoadLibraryA("libSceAppContent.prx"), name));
#else
    void* symbol = dlsym(RTLD_DEFAULT, name);
#endif
    if (!symbol) {
        std::fprintf(stderr, "%s is not exported by libSceAppContent\n", name);
        std::abort();
    }
    return reinterpret_cast<Function>(symbol);
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "AppContent license: %s\n", message);
    std::abort();
}

}

int main() {
    static std::array<std::byte, 64> process{};
    Require(ProcessParametersFixture::Install(process.data(), process.size()), "the process parameters could not be installed");
    std::uint32_t sdkVersion = 0x03500000;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));

    const auto checkBundleLicense = Resolve<CheckBundleLicenseFn>("sceAppContentCheckBundleLicenseOnDisc");
    const auto getGameTrialsFlag = Resolve<GetGameTrialsFlagFn>("sceAppContentGetGameTrialsFlag");
    const auto getPlayableStatus = Resolve<GetPlayableStatusFn>("sceAppContentGetPlayableStatus");

    NpUnifiedEntitlementLabel label{};
    std::memcpy(label.data, "TESTADDON0000001", 17);
    std::uint32_t flag = Untouched;
    std::uint32_t status = Untouched;
    Require(checkBundleLicense(0, &label) == NotInitialized, "the bundle license was checked before initialization");
    Require(getGameTrialsFlag(&flag) == NotInitialized && flag == Untouched, "the game trials flag was read before initialization");
    Require(getPlayableStatus(0, &label, &status) == NotInitialized && status == Untouched, "the playable status was read before initialization");

    const auto original = std::filesystem::current_path();
    const auto directory = original / ("appcontent-license-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(directory), "the test directory could not be created");
    std::filesystem::current_path(directory);
    std::filesystem::create_directories("app0/sce_sys");
    {
        std::ofstream param("app0/sce_sys/param.json", std::ios::binary);
        param << R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}},"downloadDataSize":0})";
        Require(static_cast<bool>(param), "param.json could not be written");
    }
    AppContentInitParam init{};
    AppContentBootParam boot{};
    Require(sceAppContentInitialize(&init, &boot) == 0, "the library could not be initialized");

    Require(checkBundleLicense(0, &label) == NotFound && checkBundleLicense(7, &label) == NotFound, "a bundle license was found for unknown content");
    Require(checkBundleLicense(0, nullptr) == Parameter, "a null label was taken by the bundle license check");
    Require(getGameTrialsFlag(&flag) == 0 && flag == 0, "the game trials flag was not 0");
    Require(getGameTrialsFlag(nullptr) == Parameter, "a null game trials flag was taken");
    Require(getPlayableStatus(0, &label, &status) == NotFound && getPlayableStatus(7, &label, &status) == NotFound && status == Untouched, "a playable status was given for unknown content");
    Require(getPlayableStatus(0, nullptr, &status) == Parameter && getPlayableStatus(0, &label, nullptr) == Parameter && status == Untouched, "a null argument was taken by the playable status");

    for (const std::uint32_t version : {0x014fffffu, 0x01500000u, 0x034fffffu, 0x03500000u}) {
        std::memcpy(process.data() + 16, &version, sizeof(version));
        for (char& padding : label.padding) {
            padding = 1;
            Require(checkBundleLicense(0, &label) == (version >= 0x03500000 ? Parameter : NotFound), "a padded label was not checked from SDK 0x03500000 by the bundle license check");
            Require(getPlayableStatus(0, &label, &status) == (version >= 0x03500000 ? Parameter : NotFound) && status == Untouched, "a padded label was not checked from SDK 0x03500000 by the playable status");
            padding = 0;
        }
    }

    std::filesystem::current_path(original);
    std::filesystem::remove_all(directory);
    std::puts("AppContent license and trials tests passed");
    return 0;
}
