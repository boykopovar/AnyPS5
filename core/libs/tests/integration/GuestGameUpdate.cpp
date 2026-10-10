#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceGameUpdateInitialize(void);
int APS5_VABI sceGameUpdateTerminate(void);
int APS5_VABI sceGameUpdateGetAddcontLatestVersion(std::uint32_t, const void*, GameUpdateAddcontVersionInfo*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr int notInitialized = static_cast<int>(0x80412801);
constexpr int invalidArgument = static_cast<int>(0x80412803);
constexpr int invalidSize = static_cast<int>(0x80412804);

static_assert(sizeof(GameUpdateAddcontVersionInfo) == 0x30);

const std::uint8_t label[16] = {};

GameUpdateAddcontVersionInfo SizedInfo(std::size_t size) {
    GameUpdateAddcontVersionInfo info{};
    info.size = size;
    return info;
}

class InitializedGameUpdate {
public:
    InitializedGameUpdate() {
        RequireEqual(sceGameUpdateInitialize(), 0, "initialize");
    }

    ~InitializedGameUpdate() {
        if (initialized) sceGameUpdateTerminate();
    }

    InitializedGameUpdate(const InitializedGameUpdate&) = delete;
    InitializedGameUpdate& operator=(const InitializedGameUpdate&) = delete;

    int Terminate() {
        initialized = false;
        return sceGameUpdateTerminate();
    }

private:
    bool initialized = true;
};

const Case beforeInitialize{"GetAddcontLatestVersion_BeforeInitialize_ReturnsNotInitialized", [] {
    auto info = SizedInfo(sizeof(GameUpdateAddcontVersionInfo));
    RequireEqual(sceGameUpdateGetAddcontLatestVersion(0, label, &info), notInitialized, "query before initialize");
}};

const Case nullInfo{"GetAddcontLatestVersion_NullInfo_ReturnsInvalidArgument", [] {
    const InitializedGameUpdate gameUpdate;
    RequireEqual(sceGameUpdateGetAddcontLatestVersion(0, label, nullptr), invalidArgument, "query with a null info");
}};

const Case nullLabel{"GetAddcontLatestVersion_NullLabel_ThrowsRuntimeError", [] {
    const InitializedGameUpdate gameUpdate;
    auto info = SizedInfo(sizeof(GameUpdateAddcontVersionInfo));
    RequireThrows<std::runtime_error>([&] { sceGameUpdateGetAddcontLatestVersion(0, nullptr, &info); }, "query with a null label");
}};

const Case undersizedInfo{"GetAddcontLatestVersion_InfoSizeTooSmall_ReturnsInvalidSize", [] {
    const InitializedGameUpdate gameUpdate;
    auto info = SizedInfo(sizeof(GameUpdateAddcontVersionInfo) - 1);
    RequireEqual(sceGameUpdateGetAddcontLatestVersion(0, label, &info), invalidSize, "query with an undersized info");
}};

const Case oversizedInfo{"GetAddcontLatestVersion_LargerInfo_ReportsNotFoundAndZeroesFields", [] {
    const InitializedGameUpdate gameUpdate;
    GameUpdateAddcontVersionInfo info{};
    std::memset(&info, 0xAB, sizeof(info));
    info.size = sizeof(info) + 8;
    RequireEqual(sceGameUpdateGetAddcontLatestVersion(0, label, &info), 0, "query result");
    RequireEqual(info.size, sizeof(info) + 8, "size field preserved");
    Require(!info.found, "the add-on content was reported as found");
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&info);
    for (std::size_t index = sizeof(info.size); index < sizeof(info); ++index) {
        RequireEqual(bytes[index], std::uint8_t{0}, "info byte " + std::to_string(index));
    }
}};

const Case afterTerminate{"GetAddcontLatestVersion_AfterTerminate_ReturnsNotInitialized", [] {
    InitializedGameUpdate gameUpdate;
    RequireEqual(gameUpdate.Terminate(), 0, "terminate");
    auto info = SizedInfo(sizeof(GameUpdateAddcontVersionInfo));
    RequireEqual(sceGameUpdateGetAddcontLatestVersion(0, label, &info), notInitialized, "query after terminate");
}};

} // namespace
