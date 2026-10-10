#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
int APS5_VABI sceGameUpdateInitialize(void);
int APS5_VABI sceGameUpdateTerminate(void);
int APS5_VABI sceGameUpdateGetAddcontLatestVersion(std::uint32_t, const void*, GameUpdateAddcontVersionInfo*);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr int notInitialized = static_cast<int>(0x80412801);
    constexpr int invalidArgument = static_cast<int>(0x80412803);
    constexpr int invalidSize = static_cast<int>(0x80412804);

    static_assert(sizeof(GameUpdateAddcontVersionInfo) == 0x30);
    const std::uint8_t label[16] = {};
    GameUpdateAddcontVersionInfo info{};
    info.size = sizeof(info);

    Require(sceGameUpdateGetAddcontLatestVersion(0, label, &info) == notInitialized);
    Require(sceGameUpdateInitialize() == 0);
    Require(sceGameUpdateGetAddcontLatestVersion(0, label, nullptr) == invalidArgument);

    bool threw = false;
    try { sceGameUpdateGetAddcontLatestVersion(0, nullptr, &info); }
    catch (const std::runtime_error&) { threw = true; }
    Require(threw);

    info.size = sizeof(info) - 1;
    Require(sceGameUpdateGetAddcontLatestVersion(0, label, &info) == invalidSize);

    std::memset(&info, 0xAB, sizeof(info));
    info.size = sizeof(info) + 8;
    Require(sceGameUpdateGetAddcontLatestVersion(0, label, &info) == 0);
    Require(info.size == sizeof(info) + 8 && !info.found);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&info);
    for (std::size_t i = sizeof(info.size); i < sizeof(info); ++i) Require(bytes[i] == 0);

    Require(sceGameUpdateTerminate() == 0);
    info.size = sizeof(info);
    Require(sceGameUpdateGetAddcontLatestVersion(0, label, &info) == notInitialized);
}
