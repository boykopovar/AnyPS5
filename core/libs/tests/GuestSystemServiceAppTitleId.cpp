#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <string_view>

extern "C" int APS5_VABI sceSystemServiceGetAppIdOfRunningBigApp(void);
extern "C" int APS5_VABI sceSystemServiceGetAppId(const char* titleId);
extern "C" int APS5_VABI sceSystemServiceGetAppTitleId(int appId, char* titleId);
extern "C" int APS5_VABI sceLncUtilGetAppTitleId(std::int32_t appId, char* titleId);

namespace {

constexpr char Sentinel = 0x5A;

struct GuardedBuffer {
    char bytes[10 + 6];
    GuardedBuffer() { std::memset(bytes, Sentinel, sizeof(bytes)); }
    bool Untouched() const {
        for (const char byte : bytes) {
            if (byte != Sentinel) return false;
        }
        return true;
    }
    bool GuardIntact() const {
        for (std::size_t index = 10; index < sizeof(bytes); ++index) {
            if (bytes[index] != Sentinel) return false;
        }
        return true;
    }
};

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

bool GetAppIdThrows(const char* titleId) {
    return Throws([&] { static_cast<void>(sceSystemServiceGetAppId(titleId)); });
}

bool GetAppTitleIdThrows(int appId, GuardedBuffer& buffer) {
    return Throws([&] { static_cast<void>(sceSystemServiceGetAppTitleId(appId, buffer.bytes)); }) && buffer.Untouched();
}

bool LncGetAppTitleIdThrows(std::uint32_t appId, GuardedBuffer& buffer) {
    return Throws([&] { static_cast<void>(sceLncUtilGetAppTitleId(static_cast<std::int32_t>(appId), buffer.bytes)); }) && buffer.Untouched();
}

void CheckOtherApplications(int runningAppId) {
    const int otherAppIds[] = {runningAppId + 1, runningAppId - 1, 0, -1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()};
    for (const int appId : otherAppIds) {
        GuardedBuffer buffer;
        Require(GetAppTitleIdThrows(appId, buffer));
        Require(LncGetAppTitleIdThrows(static_cast<std::uint32_t>(appId), buffer));
    }
    GuardedBuffer buffer;
    Require(LncGetAppTitleIdThrows(static_cast<std::uint32_t>(runningAppId) | 0x80000000u, buffer));
    Require(LncGetAppTitleIdThrows(std::numeric_limits<std::uint32_t>::max(), buffer));
}

void CheckNullBuffers(int runningAppId) {
    Require(Throws([&] { static_cast<void>(sceSystemServiceGetAppTitleId(runningAppId, nullptr)); }));
    Require(Throws([&] { static_cast<void>(sceLncUtilGetAppTitleId(runningAppId, nullptr)); }));
    Require(Throws([&] { static_cast<void>(sceSystemServiceGetAppTitleId(runningAppId + 1, nullptr)); }));
    Require(GetAppIdThrows(nullptr));
}

void CheckKnownTitle(int runningAppId) {
    constexpr std::string_view titleId = "PPSA00000";
    Require(runningAppId >= 0);
    Require(sceSystemServiceGetAppId("PPSA00000") == runningAppId);
    Require(sceSystemServiceGetAppId("PPSA00000") == runningAppId);
    const char* const otherTitles[] = {"PPSA00001", "PPSA00010", "ppsa00000", "PPSA0000", "PPSA000000", "PPSA00000 ", " PPSA00000", "CUSA00000", ""};
    for (const char* other : otherTitles) {
        Require(GetAppIdThrows(other));
    }

    GuardedBuffer buffer;
    Require(sceSystemServiceGetAppTitleId(runningAppId, buffer.bytes) == 0);
    Require(std::memcmp(buffer.bytes, "PPSA00000", titleId.size() + 1) == 0);
    Require(buffer.GuardIntact());
    Require(sceSystemServiceGetAppId(buffer.bytes) == runningAppId);

    GuardedBuffer lncBuffer;
    Require(sceLncUtilGetAppTitleId(runningAppId, lncBuffer.bytes) == 0);
    Require(std::memcmp(lncBuffer.bytes, "PPSA00000", titleId.size() + 1) == 0);
    Require(lncBuffer.GuardIntact());

    GuardedBuffer dirty;
    std::memset(dirty.bytes, 'x', 10);
    Require(sceSystemServiceGetAppTitleId(runningAppId, dirty.bytes) == 0);
    Require(std::strcmp(dirty.bytes, "PPSA00000") == 0 && dirty.GuardIntact());

    CheckOtherApplications(runningAppId);
    CheckNullBuffers(runningAppId);
}

void CheckLongTitle(int runningAppId) {
    Require(sceSystemServiceGetAppId("PPSA000000") == runningAppId);
    Require(GetAppIdThrows("PPSA00000"));
    Require(GetAppIdThrows("PPSA0000000"));
    GuardedBuffer buffer;
    Require(GetAppTitleIdThrows(runningAppId, buffer));
    Require(LncGetAppTitleIdThrows(static_cast<std::uint32_t>(runningAppId), buffer));
}

void CheckMissingMetadata(int runningAppId) {
    Require(GetAppIdThrows("PPSA00000"));
    Require(GetAppIdThrows(nullptr));
    GuardedBuffer buffer;
    Require(GetAppTitleIdThrows(runningAppId, buffer));
    Require(LncGetAppTitleIdThrows(static_cast<std::uint32_t>(runningAppId), buffer));
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    const std::string_view mode = argv[1];
    const int runningAppId = sceSystemServiceGetAppIdOfRunningBigApp();
    if (mode == "known") CheckKnownTitle(runningAppId);
    else if (mode == "long") CheckLongTitle(runningAppId);
    else if (mode == "missing") CheckMissingMetadata(runningAppId);
    else Require(false);
}
