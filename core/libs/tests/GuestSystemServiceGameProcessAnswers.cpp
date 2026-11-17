#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <limits>

namespace {

constexpr std::uint8_t Fill = 0xA5;
constexpr int SystemServiceRejected = static_cast<int>(0x80A10005u);

void Require(bool value) { if (!value) std::abort(); }

template<typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

using Buffer = std::array<std::uint8_t, 0x1000>;

Buffer Filled() {
    Buffer buffer{};
    buffer.fill(Fill);
    return buffer;
}

bool Untouched(const Buffer& buffer) {
    for (const std::uint8_t byte : buffer) {
        if (byte != Fill) return false;
    }
    return true;
}

void CheckAppType() {
    const std::int32_t others[] = {SYSTEM_SERVICE_RUNNING_APP_ID + 1, 0, -1, LNC_UTIL_ERROR_GAME_PROCESS,
                                   std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()};
    for (const std::int32_t appId : others) {
        std::int32_t type = 0x5A5A5A5A;
        Require(sceSystemServiceGetAppType(appId, &type) == LNC_UTIL_ERROR_GAME_PROCESS && type == 0x5A5A5A5A);
    }
    std::int32_t type = 0;
    Require(Throws([&] { static_cast<void>(sceSystemServiceGetAppType(SYSTEM_SERVICE_RUNNING_APP_ID, &type)); }));
    Require(Throws([] { static_cast<void>(sceSystemServiceGetAppType(-1, nullptr)); }));
}

void CheckFocusedAppStatus() {
    Buffer buffer = Filled();
    Require(sceSystemServiceGetAppFocusedAppStatus(reinterpret_cast<SystemServiceAppStatus*>(buffer.data())) == LNC_UTIL_ERROR_GAME_PROCESS);
    Require(Untouched(buffer));
    Require(Throws([] { static_cast<void>(sceSystemServiceGetAppFocusedAppStatus(nullptr)); }));
}

void CheckDbgExecutablePath() {
    const std::uint64_t selectors[] = {0, 1, 2, std::numeric_limits<std::uint64_t>::max()};
    for (const std::uint64_t selector : selectors) {
        Buffer buffer = Filled();
        Require(sceSystemServiceGetDbgExecutablePath(reinterpret_cast<char*>(buffer.data()), selector) == SystemServiceRejected);
        Require(Untouched(buffer));
    }
    Require(Throws([] { static_cast<void>(sceSystemServiceGetDbgExecutablePath(nullptr, 0)); }));
}

void CheckPrivacyDefinitions() {
    Buffer version = Filled();
    Buffer data = Filled();
    Require(sceSystemServiceGetPlatformPrivacyDefinitionVersion("", reinterpret_cast<SystemServicePrivacyDefinitionVersion*>(version.data())) == SYSTEM_SERVICE_ERROR_INTERNAL);
    Require(sceSystemServiceGetPlatformPrivacyDefinitionData("", reinterpret_cast<SystemServicePrivacyDefinitionData*>(data.data())) == SYSTEM_SERVICE_ERROR_INTERNAL);
    Require(Untouched(version) && Untouched(data));
    Require(Throws([&] { static_cast<void>(sceSystemServiceGetPlatformPrivacyDefinitionVersion("key", reinterpret_cast<SystemServicePrivacyDefinitionVersion*>(version.data()))); }));
    Require(Throws([&] { static_cast<void>(sceSystemServiceGetPlatformPrivacyDefinitionData("key", reinterpret_cast<SystemServicePrivacyDefinitionData*>(data.data()))); }));
    Require(Throws([&] { static_cast<void>(sceSystemServiceGetPlatformPrivacyDefinitionVersion(nullptr, reinterpret_cast<SystemServicePrivacyDefinitionVersion*>(version.data()))); }));
    Require(Throws([&] { static_cast<void>(sceSystemServiceGetPlatformPrivacyDefinitionData(nullptr, reinterpret_cast<SystemServicePrivacyDefinitionData*>(data.data()))); }));
    Require(Throws([] { static_cast<void>(sceSystemServiceGetPlatformPrivacyDefinitionVersion("", nullptr)); }));
    Require(Throws([] { static_cast<void>(sceSystemServiceGetPlatformPrivacyDefinitionData("", nullptr)); }));
    Require(Throws([] { static_cast<void>(sceSystemServiceGetPlatformPrivacyDefinitionVersion(reinterpret_cast<const char*>(0x10), reinterpret_cast<SystemServicePrivacyDefinitionVersion*>(0x1000))); }));
}

}

int main() {
    CheckAppType();
    CheckFocusedAppStatus();
    CheckDbgExecutablePath();
    CheckPrivacyDefinitions();
}
