#include "prx/libSceVoice/include/Api.hpp"
#include "prx/libSceVoiceQoS/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static void Require(bool value) { if (!value) std::abort(); }

template <typename TCall>
static bool NotImplemented(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

template <typename TCall>
static bool NotImplementedMessage(TCall call, std::string_view message) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::string_view(error.what()).find(message) != std::string_view::npos;
    }
    return false;
}

int main(int argc, char** argv) {
    Require(argc == 2);
    constexpr int argumentInvalid = static_cast<int>(0x804E0902);
    constexpr int initialized = static_cast<int>(0x804E0905);
    constexpr int notInitialized = static_cast<int>(0x804E0901);
    constexpr int voiceNotInitialized = static_cast<int>(0x804E0801);
    const auto appType = static_cast<std::int32_t>(std::strtoul(argv[1], nullptr, 16));
    std::vector<std::uint8_t> memory(0x40000, 0xAA);
    Require(sceVoiceQoSEnd() == notInitialized);
    Require(sceVoiceQoSTerminate() == notInitialized);
    Require(sceVoiceQoSEnableChat() == notInitialized);
    Require(sceVoiceQoSDisableChat() == notInitialized);
    std::int32_t uninitLocal = -1;
    std::int32_t uninitRemote = -1;
    Require(sceVoiceQoSGetLocalEndpoint(1, &uninitLocal) == notInitialized);
    Require(sceVoiceQoSGetRemoteEndpoint(1, &uninitRemote) == notInitialized);
    Require(sceVoiceQoSInit(nullptr, 0x40000, appType) == argumentInvalid);
    Require(sceVoiceQoSInit(nullptr, 0, appType) == argumentInvalid);
    Require(sceVoiceQoSInitHQ(nullptr, 0x40000, appType) == argumentInvalid);
    Require(sceVoiceQoSInitHQ(nullptr, 0, appType) == argumentInvalid);
    for (std::uint32_t size : {0u, 1u, 0x100u, 0x3FFFFu, 0x40001u, 0x80000u, 0xFFFFFFFFu}) Require(sceVoiceQoSInit(memory.data(), size, appType) == argumentInvalid);
    for (std::int32_t type : {0, 1, 0x08000000, 0x20000001, 0x30000000, 0x40000000, static_cast<std::int32_t>(0x80000000), -1}) {
        Require(sceVoiceQoSInit(memory.data(), 0x40000, type) == argumentInvalid);
    }
    for (std::uint8_t byte : memory) Require(byte == 0xAA);
    Require(sceVoiceQoSInit(memory.data(), 0x40000, appType) == 0);
    VoiceInitParam voiceParam{};
    voiceParam.app_type = appType;
    Require(sceVoiceInit(&voiceParam, 100) == static_cast<int>(0x804E0802));
    Require(sceVoiceQoSInit(memory.data(), 0x40000, appType) == initialized);
    Require(sceVoiceQoSInit(nullptr, 0, 0) == initialized);
    Require(sceVoiceQoSInitHQ(memory.data(), 0x40000, appType) == initialized);
    Require(sceVoiceQoSInitHQ(nullptr, 0, 0) == initialized);

    const bool wideAppType = appType == 0x10000000;
    const std::int32_t connectionLimit = wideAppType ? 15 : 7;
    const std::int32_t bitrateCap = wideAppType ? 22533 : 7300;
    auto setBitrate = [](std::int32_t connection, std::int32_t attribute, std::int32_t bitrate) {
        return sceVoiceQoSSetConnectionAttribute(connection, attribute, &bitrate, sizeof(bitrate));
    };
    auto getBitrate = [](std::int32_t connection, std::int32_t attribute) {
        std::int32_t bitrate = -1;
        Require(sceVoiceQoSGetConnectionAttribute(connection, attribute, &bitrate, sizeof(bitrate)) == 0);
        return bitrate;
    };
    for (std::int32_t connection = 0; connection < connectionLimit; ++connection) {
        Require(getBitrate(connection, 4) == bitrateCap && getBitrate(connection, 9) == 3850);
    }
    std::int32_t bitrateOut = 77;
    for (std::int32_t connection : {-1, connectionLimit, 100}) {
        Require(sceVoiceQoSGetConnectionAttribute(connection, 9, &bitrateOut, 4) == argumentInvalid);
        Require(setBitrate(connection, 9, 5000) == argumentInvalid);
    }
    for (std::int32_t attribute : {0, 1, 3, 5, 8, 10}) {
        Require(sceVoiceQoSGetConnectionAttribute(0, attribute, &bitrateOut, 4) == argumentInvalid);
        Require(setBitrate(0, attribute, 5000) == argumentInvalid);
    }
    Require(sceVoiceQoSGetConnectionAttribute(0, 9, &bitrateOut, 8) == argumentInvalid && bitrateOut == 77);
    Require(sceVoiceQoSGetConnectionAttribute(0, 9, nullptr, 4) == argumentInvalid);
    Require(sceVoiceQoSSetConnectionAttribute(0, 9, &bitrateOut, 2) == argumentInvalid);
    Require(sceVoiceQoSSetConnectionAttribute(0, 9, nullptr, 4) == argumentInvalid);
    Require(getBitrate(0, 9) == 3850);
    Require(setBitrate(0, 9, 5000) == 0 && getBitrate(0, 9) == 4650);
    Require(setBitrate(0, 9, 7299) == 0 && getBitrate(0, 9) == 5700);
    Require(setBitrate(0, 9, 7300) == 0 && getBitrate(0, 9) == 7300);
    Require(setBitrate(0, 9, 16500) == 0 && getBitrate(0, 9) == (wideAppType ? 16000 : 7300));
    Require(setBitrate(0, 9, 100000) == 0 && getBitrate(0, 9) == bitrateCap);
    Require(setBitrate(0, 9, -5) == 0 && getBitrate(0, 9) == 3850);
    Require(getBitrate(1, 9) == 3850 && getBitrate(1, 4) == bitrateCap);
    Require(setBitrate(0, 9, 7300) == 0);
    Require(setBitrate(0, 4, 1000) == 0 && getBitrate(0, 4) == 7300);
    Require(setBitrate(0, 9, 0) == 0 && getBitrate(0, 9) == 3850);
    Require(setBitrate(0, 4, 5000) == 0 && getBitrate(0, 4) == 5000);
    Require(setBitrate(0, 9, 6000) == 0 && getBitrate(0, 9) == 4650);
    Require(setBitrate(0, 4, 100000) == 0 && getBitrate(0, 4) == bitrateCap);
    Require(setBitrate(0, 4, -1) == 0 && getBitrate(0, 4) == 4650);
    Require(setBitrate(connectionLimit - 1, 9, 5700) == 0 && getBitrate(connectionLimit - 1, 9) == 5700);

    VoicePortParam titlePortParam{};
    titlePortParam.port_type = 0;
    titlePortParam.volume = 0.25f;
    titlePortParam.device.user_id = 1;
    titlePortParam.device.type = 0;
    titlePortParam.device.index = 0;
    std::uint32_t titlePort = 255;
    Require(sceVoiceCreatePort(&titlePort, &titlePortParam) == 0 && titlePort == 0);
    float titleVolume = 0.0f;
    bool titleMute = true;
    Require(sceVoiceGetVolume(titlePort, &titleVolume) == 0 && titleVolume == 0.25f);
    Require(sceVoiceGetMuteFlag(titlePort, &titleMute) == 0 && !titleMute);
    const float otherVolume = 0.5f;
    const std::uint8_t otherMute = 1;
    Require(sceVoiceQoSSetLocalEndpointAttribute(0, 0, &otherVolume, sizeof(otherVolume)) == argumentInvalid);
    Require(sceVoiceQoSSetLocalEndpointAttribute(0, 1, &otherMute, sizeof(otherMute)) == argumentInvalid);
    Require(sceVoiceQoSGetLocalEndpointAttribute(0, 0, &titleVolume, sizeof(titleVolume)) == argumentInvalid);
    Require(sceVoiceGetVolume(titlePort, &titleVolume) == 0 && titleVolume == 0.25f);
    Require(sceVoiceGetMuteFlag(titlePort, &titleMute) == 0 && !titleMute);

    std::int32_t local = -1;
    std::int32_t remote = -1;
    std::int32_t connection = -1;
    Require(sceVoiceQoSCreateLocalEndpoint(nullptr, 1, 0, 0) == argumentInvalid);
    errno = 71;
    Require(sceVoiceQoSCreateLocalEndpoint(reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10}), 1, 0, 0) == argumentInvalid);
    Require(errno == 71);
    Require(sceVoiceQoSCreateRemoteEndpoint(reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10})) == argumentInvalid);
    Require(errno == 71);
    Require(sceVoiceQoSCreateLocalEndpoint(&local, 1, 0, 0) == 0 && local == 0);
    Require(sceVoiceQoSCreateRemoteEndpoint(&remote) == 0 && remote == 0);
    float volume = 0.0f;
    std::uint8_t mute = 1;
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 0, &volume, sizeof(volume)) == 0 && volume == 1.0f);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 1, &mute, sizeof(mute)) == 0 && mute == 0);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 2, &volume, sizeof(volume)) == 0 && volume == 1.0f);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 3, &mute, sizeof(mute)) == 0 && mute == 0);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 4, &volume, sizeof(volume)) == argumentInvalid);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 7, &volume, sizeof(volume)) == argumentInvalid);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 0, &volume, 1) == argumentInvalid);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 1, &mute, 4) == argumentInvalid);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local + 100, 0, &volume, sizeof(volume)) == argumentInvalid);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 0, nullptr, sizeof(volume)) == argumentInvalid);
    std::uint32_t attributeValue = 0;
    Require(sceVoiceQoSGetLocalEndpointAttribute(local + 1, 0, &attributeValue, sizeof(attributeValue)) == argumentInvalid);
    for (std::int32_t attribute : {-1, 4, 7, 9, 100}) Require(sceVoiceQoSGetLocalEndpointAttribute(local, attribute, &attributeValue, sizeof(attributeValue)) == argumentInvalid);
    for (std::int32_t attribute : {0, 2, 6}) {
        for (std::int32_t size : {0, 1, 2, 8, -1}) Require(sceVoiceQoSGetLocalEndpointAttribute(local, attribute, &attributeValue, size) == argumentInvalid);
    }
    for (std::int32_t attribute : {1, 3, 5}) {
        for (std::int32_t size : {0, 2, 4, 8, -1}) Require(sceVoiceQoSGetLocalEndpointAttribute(local, attribute, &attributeValue, size) == argumentInvalid);
    }
    for (std::int32_t size : {0, 1, 4, 8, -1}) Require(sceVoiceQoSGetLocalEndpointAttribute(local, 8, &attributeValue, size) == argumentInvalid);
    const float halfVolume = 0.5f;
    const std::uint8_t enabled = 1;
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 0, &halfVolume, sizeof(halfVolume)) == 0);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 0, &volume, sizeof(volume)) == 0 && volume == halfVolume);
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 1, &enabled, sizeof(enabled)) == 0);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 1, &mute, sizeof(mute)) == 0 && mute == 1);
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 2, &halfVolume, sizeof(halfVolume)) == 0);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 2, &volume, sizeof(volume)) == 0 && volume == halfVolume);
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 3, &enabled, sizeof(enabled)) == 0);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 3, &mute, sizeof(mute)) == 0 && mute == 1);
    errno = 71;
    const void* unreadableValue = reinterpret_cast<const void*>(std::uintptr_t{0x10});
    for (int attribute : {0, 2}) Require(sceVoiceQoSSetLocalEndpointAttribute(local, attribute, unreadableValue, sizeof(float)) == argumentInvalid);
    for (int attribute : {1, 3}) Require(sceVoiceQoSSetLocalEndpointAttribute(local, attribute, unreadableValue, sizeof(std::uint8_t)) == argumentInvalid);
    Require(errno == 71);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 0, &volume, sizeof(volume)) == 0 && volume == halfVolume);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 1, &mute, sizeof(mute)) == 0 && mute == enabled);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 2, &volume, sizeof(volume)) == 0 && volume == halfVolume);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 3, &mute, sizeof(mute)) == 0 && mute == enabled);
    Require(sceVoiceQoSConnect(&connection, 1, remote) == argumentInvalid);
    Require(sceVoiceQoSConnect(&connection, local, remote) == 0 && connection == 0);
    std::int32_t queriedLocal = -1;
    std::int32_t queriedRemote = -1;
    Require(sceVoiceQoSGetLocalEndpoint(connection, nullptr) == argumentInvalid);
    Require(sceVoiceQoSGetRemoteEndpoint(connection, nullptr) == argumentInvalid);
    Require(sceVoiceQoSGetLocalEndpoint(connection + 100, &queriedLocal) == argumentInvalid);
    Require(sceVoiceQoSGetRemoteEndpoint(connection + 100, &queriedRemote) == argumentInvalid);
    Require(sceVoiceQoSGetLocalEndpoint(connection, &queriedLocal) == 0 && queriedLocal == local);
    Require(sceVoiceQoSGetRemoteEndpoint(connection, &queriedRemote) == 0 && queriedRemote == remote);
    errno = 71;
    Require(sceVoiceQoSGetLocalEndpoint(connection, reinterpret_cast<std::int32_t*>(0x10)) == argumentInvalid);
    Require(sceVoiceQoSGetRemoteEndpoint(connection, reinterpret_cast<std::int32_t*>(0x10)) == argumentInvalid);
    Require(errno == 71 && queriedLocal == local && queriedRemote == remote);
    auto getStatusInt = [](std::int32_t target, std::int32_t status) {
        std::int32_t result = -7;
        Require(sceVoiceQoSGetStatus(target, status, &result, sizeof(result)) == 0);
        return result;
    };
    auto getStatusFloat = [](std::int32_t target, std::int32_t status) {
        float result = -7.0f;
        Require(sceVoiceQoSGetStatus(target, status, &result, sizeof(result)) == 0);
        return result;
    };
    auto getDebugStatusInt = [](std::int32_t target, std::int32_t status) {
        std::int32_t result = -7;
        Require(sceVoiceQoSDebugGetStatus(target, status, &result, sizeof(result)) == 0);
        return result;
    };
    auto getDebugStatusFloat = [](std::int32_t target, std::int32_t status) {
        float result = -7.0f;
        Require(sceVoiceQoSDebugGetStatus(target, status, &result, sizeof(result)) == 0);
        return result;
    };
    Require(getStatusInt(connection, 0) == bitrateCap);
    Require(getStatusInt(connection, 1) == bitrateCap);
    Require(getStatusInt(connection, 2) == bitrateCap);
    Require(getStatusFloat(connection, 3) == 0.0f);
    Require(getStatusInt(connection, 4) == 1);
    const std::int32_t unusedConnection = connectionLimit - 1;
    Require(getStatusInt(unusedConnection, 2) == bitrateCap);
    Require(getStatusFloat(unusedConnection, 3) == 0.0f);
    for (std::int32_t status : {0, 1, 4}) {
        Require(NotImplementedMessage([&] {
            std::int32_t result = 0;
            sceVoiceQoSGetStatus(unusedConnection, status, &result, sizeof(result));
        }, "unconnected connection"));
    }
    for (std::int32_t target : {connection, unusedConnection}) {
        for (std::int32_t status : {200, 201, 202, 203, 204, 205, 207}) Require(getDebugStatusInt(target, status) == 0);
        for (std::int32_t status : {206, 208, 209}) Require(getDebugStatusFloat(target, status) == 0.0f);
        Require(getDebugStatusFloat(target, 210) == 1.0f);
    }
    std::int32_t statusOut = 77;
    for (std::int32_t target : {-1, connectionLimit, 100}) {
        Require(sceVoiceQoSGetStatus(target, 2, &statusOut, 4) == argumentInvalid);
        Require(sceVoiceQoSDebugGetStatus(target, 200, &statusOut, 4) == argumentInvalid);
    }
    for (std::int32_t status : {-1, 5, 100, 200}) Require(sceVoiceQoSGetStatus(connection, status, &statusOut, 4) == argumentInvalid);
    for (std::int32_t status : {-1, 0, 4, 199, 211}) Require(sceVoiceQoSDebugGetStatus(connection, status, &statusOut, 4) == argumentInvalid);
    for (std::int32_t size : {0, 1, 3, 8, -1}) {
        Require(sceVoiceQoSGetStatus(connection, 2, &statusOut, size) == argumentInvalid);
        Require(sceVoiceQoSDebugGetStatus(connection, 200, &statusOut, size) == argumentInvalid);
    }
    Require(statusOut == 77);
    Require(sceVoiceQoSGetStatus(connection, 2, nullptr, 4) == argumentInvalid);
    Require(sceVoiceQoSDebugGetStatus(connection, 200, nullptr, 4) == argumentInvalid);
    errno = 71;
    Require(sceVoiceQoSGetStatus(connection, 2, reinterpret_cast<void*>(std::uintptr_t{0x10}), 4) == argumentInvalid);
    Require(sceVoiceQoSDebugGetStatus(connection, 200, reinterpret_cast<void*>(std::uintptr_t{0x10}), 4) == argumentInvalid);
    Require(errno == 71);
    std::int32_t duplicate = -1;
    Require(sceVoiceQoSConnect(&duplicate, local, remote) == 0 && duplicate > 0 && duplicate != connection);
    Require(sceVoiceQoSDisconnect(duplicate) == 0);

    constexpr int limitReached = static_cast<int>(0x804E0907);
    std::int32_t otherLocal = 12;
    Require(sceVoiceQoSCreateLocalEndpoint(&otherLocal, 1, 0, 0) == limitReached && otherLocal == -1);
    Require(sceVoiceQoSCreateLocalEndpoint(&otherLocal, 2, 0, 0) == limitReached && otherLocal == -1);

    const std::uint32_t level = 0x1234;
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 3, &level, sizeof(level)) == argumentInvalid);
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 3, nullptr, sizeof(level)) == argumentInvalid);
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 3, &level, 0) == argumentInvalid);
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 3, &level, -1) == argumentInvalid);
    Require(sceVoiceQoSSetLocalEndpointAttribute(local + 100, 3, &level, sizeof(level)) == argumentInvalid);

    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote, 3, &level, sizeof(level)) == argumentInvalid);
    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote, 3, nullptr, sizeof(level)) == argumentInvalid);
    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote, 3, &level, 0) == argumentInvalid);
    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote, 3, &level, -1) == argumentInvalid);
    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote + 100, 3, &level, sizeof(level)) == argumentInvalid);

    std::uint8_t remoteMute = 0xFF;
    std::uint8_t localMuteBefore = 0xFF;
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 1, &localMuteBefore, sizeof(localMuteBefore)) == 0);
    Require(sceVoiceQoSGetRemoteEndpointAttribute(remote, 3, &remoteMute, sizeof(remoteMute)) == argumentInvalid);
    Require(sceVoiceQoSGetRemoteEndpointAttribute(remote, 3, nullptr, sizeof(remoteMute)) == argumentInvalid);
    Require(sceVoiceQoSGetRemoteEndpointAttribute(remote, 3, &remoteMute, 0) == argumentInvalid);
    Require(sceVoiceQoSGetRemoteEndpointAttribute(remote, 7, &remoteMute, -1) == argumentInvalid);
    Require(sceVoiceQoSGetRemoteEndpointAttribute(-1, 7, &remoteMute, sizeof(remoteMute)) == argumentInvalid);
    Require(sceVoiceQoSGetRemoteEndpointAttribute(remote + 100, 7, &remoteMute, sizeof(remoteMute)) == argumentInvalid);
    Require(sceVoiceQoSGetRemoteEndpointAttribute(remote + 1, 7, &remoteMute, sizeof(remoteMute)) == argumentInvalid);
    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote + 1, 7, &remoteMute, sizeof(remoteMute)) == argumentInvalid);
    Require(sceVoiceQoSGetRemoteEndpointAttribute(remote, 7, &remoteMute, sizeof(remoteMute)) == 0 && remoteMute == 0);
    bool titleMuteBefore = true;
    Require(sceVoiceGetMuteFlag(titlePort, &titleMuteBefore) == 0);
    const std::uint8_t remoteMuted = 1;
    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote, 7, &remoteMuted, sizeof(remoteMuted)) == 0);
    Require(sceVoiceQoSGetRemoteEndpointAttribute(remote, 7, &remoteMute, sizeof(remoteMute)) == 0 && remoteMute != 0);
    std::uint8_t localMuteAfter = 0xFF;
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 1, &localMuteAfter, sizeof(localMuteAfter)) == 0 && localMuteAfter == localMuteBefore);
    bool titleMuteAfter = true;
    Require(sceVoiceGetMuteFlag(titlePort, &titleMuteAfter) == 0 && titleMuteAfter == titleMuteBefore);
    const std::uint8_t remoteUnmuted = 0;
    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote, 7, &remoteUnmuted, sizeof(remoteUnmuted)) == 0);
    Require(sceVoiceQoSGetRemoteEndpointAttribute(remote, 7, &remoteMute, sizeof(remoteMute)) == 0 && remoteMute == 0);

    std::vector<std::uint8_t> packet(1024, 0);
    std::uint32_t packetSize = static_cast<std::uint32_t>(packet.size());
    Require(sceVoiceQoSReadPacket(connection, packet.data(), &packetSize) == 0 && packetSize == 0);
    errno = 71;
    Require(sceVoiceQoSReadPacket(connection, packet.data(), reinterpret_cast<std::uint32_t*>(std::uintptr_t{0x10})) == argumentInvalid);
    Require(errno == 71);
    Require(sceVoiceQoSConnect(reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10}), local, remote) == argumentInvalid);
    Require(errno == 71);
    Require(sceVoiceQoSReadPacket(connection, nullptr, &packetSize) == argumentInvalid);
    Require(sceVoiceQoSReadPacket(connection, packet.data(), nullptr) == argumentInvalid);
    Require(sceVoiceQoSReadPacket(connection + 100, packet.data(), &packetSize) == argumentInvalid);

    packetSize = 100;
    Require(sceVoiceQoSWritePacket(connection, packet.data(), &packetSize) == 0);
    errno = 71;
    Require(sceVoiceQoSWritePacket(connection, packet.data(), reinterpret_cast<std::uint32_t*>(std::uintptr_t{0x10})) == argumentInvalid);
    Require(errno == 71);
    Require(sceVoiceQoSWritePacket(connection, nullptr, &packetSize) == argumentInvalid);
    Require(sceVoiceQoSWritePacket(connection, packet.data(), nullptr) == argumentInvalid);
    Require(sceVoiceQoSWritePacket(connection + 100, packet.data(), &packetSize) == argumentInvalid);
    Require(NotImplementedMessage([&] { getStatusInt(connection, 2); }, "after voice packet traffic"));
    Require(NotImplementedMessage([&] { getDebugStatusInt(connection, 200); }, "after voice packet traffic"));
    Require(sceVoiceQoSGetStatus(connection, 5, &statusOut, 4) == argumentInvalid);
    Require(sceVoiceQoSDebugGetStatus(connection, 211, &statusOut, 4) == argumentInvalid);

    Require(sceVoiceQoSDisconnect(connection) == 0);
    Require(sceVoiceQoSDisconnect(connection) == 0);
    Require(sceVoiceQoSGetLocalEndpoint(connection, &queriedLocal) == 0 && queriedLocal == -1);
    Require(sceVoiceQoSGetRemoteEndpoint(connection, &queriedRemote) == 0 && queriedRemote == -1);
    Require(sceVoiceQoSDeleteLocalEndpoint(local) == 0);
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 0, &otherVolume, sizeof(otherVolume)) == argumentInvalid);
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 1, &otherMute, sizeof(otherMute)) == argumentInvalid);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 0, &titleVolume, sizeof(titleVolume)) == argumentInvalid);
    Require(sceVoiceGetVolume(titlePort, &titleVolume) == 0 && titleVolume == 0.25f);
    Require(sceVoiceGetMuteFlag(titlePort, &titleMute) == 0 && !titleMute);
    Require(sceVoiceDeletePort(titlePort) == 0);
    Require(sceVoiceQoSDeleteLocalEndpoint(local) == argumentInvalid);
    Require(sceVoiceQoSDeleteRemoteEndpoint(remote) == 0);
    errno = 71;
    Require(sceVoiceQoSCreateRemoteEndpoint(reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10})) == argumentInvalid);
    Require(errno == 71);

    const int endpointLimit = appType == 0x10000000 ? 15 : 7;
    for (int index = 0; index < endpointLimit; ++index) Require(sceVoiceQoSCreateRemoteEndpoint(&remote) == 0 && remote == index);
    Require(sceVoiceQoSCreateRemoteEndpoint(&remote) == limitReached && remote == -1);
    errno = 71;
    Require(sceVoiceQoSCreateRemoteEndpoint(reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10})) == argumentInvalid);
    Require(errno == 71);
    Require(sceVoiceQoSDeleteRemoteEndpoint(2) == 0);
    Require(sceVoiceQoSCreateRemoteEndpoint(&remote) == 0 && remote == 2);
    Require(sceVoiceQoSEnd() == 0);
    Require(sceVoiceQoSEnd() == notInitialized);
    Require(sceVoiceQoSGetLocalEndpoint(connection, &queriedLocal) == notInitialized);
    Require(sceVoiceQoSGetRemoteEndpoint(connection, &queriedRemote) == notInitialized);
    Require(sceVoiceQoSDeleteRemoteEndpoint(remote) == notInitialized);
    Require(sceVoiceQoSCreateLocalEndpoint(&local, 1, 0, 0) == notInitialized);
    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote, 3, &level, sizeof(level)) == notInitialized);
    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote, 7, &remoteMute, sizeof(remoteMute)) == notInitialized);
    Require(sceVoiceQoSGetRemoteEndpointAttribute(remote, 7, &remoteMute, sizeof(remoteMute)) == notInitialized);
    Require(sceVoiceQoSReadPacket(connection, packet.data(), &packetSize) == notInitialized);
    Require(sceVoiceQoSWritePacket(connection, packet.data(), &packetSize) == notInitialized);
    Require(sceVoiceQoSInit(memory.data(), 0x40000, appType) == 0);
    errno = 71;
    Require(sceVoiceQoSCreateLocalEndpoint(reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10}), 1, 0, 0) == argumentInvalid);
    Require(errno == 71);
    Require(sceVoiceQoSDeleteRemoteEndpoint(remote) == argumentInvalid);
    if (GuestArena::GuestArenaAvailable_nid_postfix()) {
        constexpr std::size_t kArenaBytes = 0x10000;
        auto* const mapped = static_cast<unsigned char*>(GuestArena::GuestArenaAllocate_nid_postfix(kArenaBytes, kArenaBytes));
        Require(mapped != nullptr);
#ifdef _WIN32
        const HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, kArenaBytes, nullptr);
        Require(section != nullptr);
        GuestArena::GuestArenaMap_nid_postfix(mapped, kArenaBytes, section, 0, PAGE_READWRITE);
        CloseHandle(section);
        const bool watchedWrites = GuestArena::GuestArenaWriteWatched_nid_postfix();
#else
        GuestArena::GuestArenaCommit_nid_postfix(mapped, kArenaBytes, 0x04, kArenaBytes);
        const bool watchedWrites = GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix();
        if (watchedWrites) {
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(mapped, kArenaBytes);
        }
#endif
        const auto collectDirty = [&]() -> bool {
            if (!watchedWrites) {
                return true;
            }
#ifdef _WIN32
            std::array<void*, 16> pages{};
            std::size_t count = pages.size();
            Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), kArenaBytes, pages.data(), &count, true));
            return count != 0;
#else
            bool dirty = false;
            Require(GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), kArenaBytes,
                [](void* context, std::uintptr_t, std::uintptr_t) { *static_cast<bool*>(context) = true; }, &dirty));
            return dirty;
#endif
        };
        const auto armWatch = [&](void* target) {
            if (!watchedWrites) {
                return;
            }
            static_cast<void>(collectDirty());
            Require(!collectDirty());
#ifdef _WIN32
            MEMORY_BASIC_INFORMATION mbi{};
            Require(VirtualQuery(target, &mbi, sizeof(mbi)) == sizeof(mbi) && mbi.Protect == PAGE_READONLY);
#else
            static_cast<void>(target);
#endif
        };
        auto* const watchedId = reinterpret_cast<std::int32_t*>(mapped);
        auto* const watchedSize = reinterpret_cast<std::uint32_t*>(mapped);
        *watchedId = -1;
        armWatch(watchedId);
        errno = 71;
        Require(sceVoiceQoSCreateLocalEndpoint(watchedId, 1, 0, 0) == 0 && *watchedId == 0);
        Require(errno == 71);
        Require(collectDirty());
        *watchedId = -1;
        armWatch(watchedId);
        errno = 71;
        Require(sceVoiceQoSCreateRemoteEndpoint(watchedId) == 0 && *watchedId == 0);
        Require(errno == 71);
        Require(collectDirty());
        *watchedId = -1;
        armWatch(watchedId);
        errno = 71;
        Require(sceVoiceQoSConnect(watchedId, 0, 0) == 0 && *watchedId == 0);
        Require(errno == 71);
        Require(collectDirty());
        *watchedId = 99;
        armWatch(watchedId);
        errno = 71;
        Require(sceVoiceQoSGetLocalEndpoint(0, watchedId) == 0 && *watchedId == 0);
        Require(errno == 71);
        Require(collectDirty());
        *watchedId = 99;
        armWatch(watchedId);
        errno = 71;
        Require(sceVoiceQoSGetRemoteEndpoint(0, watchedId) == 0 && *watchedId == 0);
        Require(errno == 71);
        Require(collectDirty());
        *watchedSize = 64;
        armWatch(watchedSize);
        errno = 71;
        Require(sceVoiceQoSReadPacket(0, packet.data(), watchedSize) == 0 && *watchedSize == 0);
        Require(errno == 71);
        Require(collectDirty());
        Require(sceVoiceQoSDisconnect(0) == 0);
        Require(sceVoiceQoSDeleteLocalEndpoint(0) == 0);
        for (int index = 1; index < endpointLimit; ++index) Require(sceVoiceQoSCreateRemoteEndpoint(&remote) == 0);
        *watchedId = 99;
        armWatch(watchedId);
        errno = 71;
        Require(sceVoiceQoSCreateRemoteEndpoint(watchedId) == limitReached && *watchedId == -1);
        Require(errno == 71);
        Require(collectDirty());
        GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), kArenaBytes, 0x02);
        errno = 71;
        Require(sceVoiceQoSCreateLocalEndpoint(watchedId, 1, 0, 0) == argumentInvalid && *watchedId == -1);
        Require(errno == 71);
        errno = 71;
        Require(sceVoiceQoSCreateRemoteEndpoint(watchedId) == argumentInvalid && *watchedId == -1);
        Require(errno == 71);
        errno = 71;
        Require(sceVoiceQoSConnect(watchedId, 0, 0) == argumentInvalid && *watchedId == -1);
        Require(errno == 71);
        errno = 71;
        Require(sceVoiceQoSGetLocalEndpoint(0, watchedId) == argumentInvalid && *watchedId == -1);
        Require(errno == 71);
        errno = 71;
        Require(sceVoiceQoSGetRemoteEndpoint(0, watchedId) == argumentInvalid && *watchedId == -1);
        Require(errno == 71);
        GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), kArenaBytes, 0x04);
#ifndef _WIN32
        if (watchedWrites) {
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(mapped, kArenaBytes);
        }
#endif
        GuestArena::GuestArenaReset_nid_postfix(mapped, kArenaBytes);
        GuestArena::GuestArenaRelease_nid_postfix(mapped, kArenaBytes);
    }
    Require(sceVoiceQoSEnd() == 0);
    Require(sceVoiceQoSEnd() == notInitialized);
    Require(sceVoiceQoSTerminate() == notInitialized);
    Require(sceVoiceQoSEnableChat() == notInitialized);
    Require(sceVoiceQoSDisableChat() == notInitialized);
    Require(sceVoiceQoSInitHQ(nullptr, 0x40000, appType) == argumentInvalid);
    Require(sceVoiceQoSInitHQ(memory.data(), 0x40000, appType) == 0);
    Require(getBitrate(0, 4) == bitrateCap && getBitrate(0, 9) == 3850 && getBitrate(connectionLimit - 1, 9) == 3850);
    Require(sceVoiceQoSEnd() == 0);
    Require(sceVoiceQoSEnd() == notInitialized);
    Require(sceVoiceQoSInitHQ(memory.data(), 0x40000, appType) == 0);
    Require(sceVoiceQoSTerminate() == 0);
    Require(sceVoiceQoSTerminate() == notInitialized);
    Require(sceVoiceQoSEnableChat() == notInitialized);
    Require(sceVoiceQoSDisableChat() == notInitialized);
    Require(sceVoiceQoSInit(nullptr, 0x40000, appType) == argumentInvalid);

    Require(sceVoiceQoSGetLocalEndpointAttribute(0, 0, nullptr, 0) == notInitialized);
    Require(sceVoiceQoSGetConnectionAttribute(1, 9, &bitrateOut, 4) == notInitialized);
    Require(setBitrate(1, 9, 5000) == notInitialized);
    Require(sceVoiceQoSGetStatus(1, 1, nullptr, 0) == notInitialized);
    Require(sceVoiceQoSDebugGetStatus(0, 200, nullptr, 4) == notInitialized);
    Require(NotImplemented([] { sceVoiceQoSSetMode(); }));
    Require(sceVoiceEnd_nid_postfix() == voiceNotInitialized);
}
