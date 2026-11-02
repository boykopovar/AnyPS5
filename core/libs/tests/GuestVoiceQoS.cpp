#include "prx/libSceVoice/include/Api.hpp"
#include "prx/libSceVoiceQoS/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

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

int main(int argc, char** argv) {
    Require(argc == 2);
    constexpr int argumentInvalid = static_cast<int>(0x804E0902);
    constexpr int initialized = static_cast<int>(0x804E0905);
    constexpr int notInitialized = static_cast<int>(0x804E0901);
    constexpr int voiceNotInitialized = static_cast<int>(0x804E0801);
    const auto appType = static_cast<std::int32_t>(std::strtoul(argv[1], nullptr, 16));
    std::vector<std::uint8_t> memory(0x40000, 0xAA);
    Require(sceVoiceQoSEnd() == notInitialized);
    std::int32_t uninitLocal = -1;
    std::int32_t uninitRemote = -1;
    Require(sceVoiceQoSGetLocalEndpoint(1, &uninitLocal) == notInitialized);
    Require(sceVoiceQoSGetRemoteEndpoint(1, &uninitRemote) == notInitialized);
    Require(sceVoiceQoSInit(nullptr, 0x40000, appType) == argumentInvalid);
    Require(sceVoiceQoSInit(nullptr, 0, appType) == argumentInvalid);
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

    std::vector<std::uint8_t> packet(1024, 0);
    std::uint32_t packetSize = static_cast<std::uint32_t>(packet.size());
    Require(sceVoiceQoSReadPacket(connection, packet.data(), &packetSize) == 0 && packetSize == 0);
    Require(sceVoiceQoSReadPacket(connection, nullptr, &packetSize) == argumentInvalid);
    Require(sceVoiceQoSReadPacket(connection, packet.data(), nullptr) == argumentInvalid);
    Require(sceVoiceQoSReadPacket(connection + 100, packet.data(), &packetSize) == argumentInvalid);

    packetSize = 100;
    Require(sceVoiceQoSWritePacket(connection, packet.data(), &packetSize) == 0);
    Require(sceVoiceQoSWritePacket(connection, nullptr, &packetSize) == argumentInvalid);
    Require(sceVoiceQoSWritePacket(connection, packet.data(), nullptr) == argumentInvalid);
    Require(sceVoiceQoSWritePacket(connection + 100, packet.data(), &packetSize) == argumentInvalid);

    Require(NotImplemented([&] { sceVoiceQoSSetRemoteEndpointAttribute(remote, 7, &level, 1); }));
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

    const int endpointLimit = appType == 0x10000000 ? 15 : 7;
    for (int index = 0; index < endpointLimit; ++index) Require(sceVoiceQoSCreateRemoteEndpoint(&remote) == 0 && remote == index);
    Require(sceVoiceQoSCreateRemoteEndpoint(&remote) == limitReached && remote == -1);
    Require(sceVoiceQoSDeleteRemoteEndpoint(2) == 0);
    Require(sceVoiceQoSCreateRemoteEndpoint(&remote) == 0 && remote == 2);
    Require(sceVoiceQoSEnd() == 0);
    Require(sceVoiceQoSEnd() == notInitialized);
    Require(sceVoiceQoSGetLocalEndpoint(connection, &queriedLocal) == notInitialized);
    Require(sceVoiceQoSGetRemoteEndpoint(connection, &queriedRemote) == notInitialized);
    Require(sceVoiceQoSDeleteRemoteEndpoint(remote) == notInitialized);
    Require(sceVoiceQoSCreateLocalEndpoint(&local, 1, 0, 0) == notInitialized);
    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote, 3, &level, sizeof(level)) == notInitialized);
    Require(sceVoiceQoSReadPacket(connection, packet.data(), &packetSize) == notInitialized);
    Require(sceVoiceQoSWritePacket(connection, packet.data(), &packetSize) == notInitialized);
    Require(sceVoiceQoSInit(memory.data(), 0x40000, appType) == 0);
    Require(sceVoiceQoSDeleteRemoteEndpoint(remote) == argumentInvalid);
    Require(sceVoiceQoSEnd() == 0);
    Require(sceVoiceQoSEnd() == notInitialized);
    Require(sceVoiceQoSInit(nullptr, 0x40000, appType) == argumentInvalid);

    Require(sceVoiceQoSGetLocalEndpointAttribute(0, 0, nullptr, 0) == notInitialized);
    Require(NotImplemented([] { sceVoiceQoSGetRemoteEndpointAttribute(1, 1, nullptr, 0); }));
    Require(NotImplemented([] { sceVoiceQoSGetConnectionAttribute(1, 1, nullptr, 0); }));
    Require(NotImplemented([] { sceVoiceQoSEnableChat(); }));
    Require(NotImplemented([] { sceVoiceQoSDisableChat(); }));
    Require(NotImplemented([] { sceVoiceQoSGetStatus(1, 1, nullptr, 0); }));
    Require(NotImplemented([] { sceVoiceQoSDebugGetStatus(0, 200, nullptr, 4); }));
    Require(NotImplemented([] { sceVoiceQoSInitHQ(nullptr, 0xc0000, 0x80000); }));
    Require(NotImplemented([] { sceVoiceQoSSetMode(); }));
    Require(NotImplemented([] { sceVoiceQoSTerminate(); }));
    Require(sceVoiceEnd_nid_postfix() == voiceNotInitialized);
}
