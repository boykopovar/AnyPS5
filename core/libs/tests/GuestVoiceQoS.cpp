#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <vector>

extern "C" {
int APS5_VABI sceVoiceQoSInit(void*, std::uint32_t, std::int32_t);
int APS5_VABI sceVoiceQoSEnd();
int APS5_VABI sceVoiceQoSCreateLocalEndpoint(std::int32_t*, std::int32_t, std::int32_t, std::int32_t);
int APS5_VABI sceVoiceQoSDeleteLocalEndpoint(std::int32_t);
int APS5_VABI sceVoiceQoSCreateRemoteEndpoint(std::int32_t*);
int APS5_VABI sceVoiceQoSDeleteRemoteEndpoint(std::int32_t);
int APS5_VABI sceVoiceQoSConnect(std::int32_t*, std::int32_t, std::int32_t);
int APS5_VABI sceVoiceQoSDisconnect(std::int32_t);
int APS5_VABI sceVoiceQoSSetLocalEndpointAttribute(std::int32_t, std::int32_t, const void*, std::int32_t);
int APS5_VABI sceVoiceQoSSetRemoteEndpointAttribute(std::int32_t, std::int32_t, const void*, std::int32_t);
}

static void Require(bool value) { if (!value) std::abort(); }

int main(int argc, char** argv) {
    Require(argc == 2);
    constexpr int argumentInvalid = static_cast<int>(0x804E0902);
    constexpr int initialized = static_cast<int>(0x804E0905);
    constexpr int notInitialized = static_cast<int>(0x804E0901);
    const auto appType = static_cast<std::int32_t>(std::strtoul(argv[1], nullptr, 16));
    std::vector<std::uint8_t> memory(0x40000, 0xAA);
    Require(sceVoiceQoSEnd() == notInitialized);
    Require(sceVoiceQoSInit(nullptr, 0x40000, appType) == argumentInvalid);
    Require(sceVoiceQoSInit(nullptr, 0, appType) == argumentInvalid);
    for (std::uint32_t size : {0u, 1u, 0x100u, 0x3FFFFu, 0x40001u, 0x80000u, 0xFFFFFFFFu}) Require(sceVoiceQoSInit(memory.data(), size, appType) == argumentInvalid);
    for (std::int32_t type : {0, 1, 0x08000000, 0x20000001, 0x30000000, 0x40000000, static_cast<std::int32_t>(0x80000000), -1}) {
        Require(sceVoiceQoSInit(memory.data(), 0x40000, type) == argumentInvalid);
    }
    for (std::uint8_t byte : memory) Require(byte == 0xAA);
    Require(sceVoiceQoSInit(memory.data(), 0x40000, appType) == 0);
    Require(sceVoiceQoSInit(memory.data(), 0x40000, appType) == initialized);
    Require(sceVoiceQoSInit(nullptr, 0, 0) == initialized);

    std::int32_t local = -1;
    std::int32_t remote = -1;
    std::int32_t connection = -1;
    Require(sceVoiceQoSCreateLocalEndpoint(nullptr, 1, 0, 0) == argumentInvalid);
    Require(sceVoiceQoSCreateLocalEndpoint(&local, 1, 0, 0) == 0 && local > 0);
    Require(sceVoiceQoSCreateRemoteEndpoint(&remote) == 0 && remote > 0 && remote != local);
    Require(sceVoiceQoSConnect(&connection, remote, local) == argumentInvalid);
    Require(sceVoiceQoSConnect(&connection, local, remote) == 0 && connection > 0);
    std::int32_t duplicate = -1;
    Require(sceVoiceQoSConnect(&duplicate, local, remote) == 0 && duplicate > 0 && duplicate != connection);
    Require(sceVoiceQoSDisconnect(duplicate) == 0);

    std::int32_t otherLocal = -1;
    std::int32_t otherConnection = -1;
    Require(sceVoiceQoSCreateLocalEndpoint(&otherLocal, 1, 0, 0) == 0);
    Require(sceVoiceQoSConnect(&otherConnection, otherLocal, remote) == 0);
    Require(sceVoiceQoSDeleteLocalEndpoint(otherLocal) == 0);
    Require(sceVoiceQoSDeleteLocalEndpoint(otherLocal) == argumentInvalid);
    Require(sceVoiceQoSDisconnect(otherConnection) == 0);

    const std::uint32_t level = 0x1234;
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 3, &level, sizeof(level)) == 0);
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 3, nullptr, sizeof(level)) == argumentInvalid);
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 3, &level, 0) == 0);
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 3, &level, -1) == 0);
    Require(sceVoiceQoSSetLocalEndpointAttribute(local + 100, 3, &level, sizeof(level)) == argumentInvalid);

    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote, 3, &level, sizeof(level)) == 0);
    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote, 3, nullptr, sizeof(level)) == argumentInvalid);
    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote, 3, &level, 0) == 0);
    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote, 3, &level, -1) == 0);
    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote + 100, 3, &level, sizeof(level)) == argumentInvalid);

    Require(sceVoiceQoSDisconnect(connection) == 0);
    Require(sceVoiceQoSDisconnect(connection) == argumentInvalid);
    Require(sceVoiceQoSDeleteLocalEndpoint(local) == 0);
    Require(sceVoiceQoSDeleteLocalEndpoint(local) == argumentInvalid);
    Require(sceVoiceQoSDeleteRemoteEndpoint(remote) == 0);

    Require(sceVoiceQoSCreateRemoteEndpoint(&remote) == 0);
    Require(sceVoiceQoSEnd() == 0);
    Require(sceVoiceQoSEnd() == notInitialized);
    Require(sceVoiceQoSDeleteRemoteEndpoint(remote) == notInitialized);
    Require(sceVoiceQoSCreateLocalEndpoint(&local, 1, 0, 0) == notInitialized);
    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote, 3, &level, sizeof(level)) == notInitialized);
    Require(sceVoiceQoSInit(memory.data(), 0x40000, appType) == 0);
    Require(sceVoiceQoSDeleteRemoteEndpoint(remote) == argumentInvalid);
    Require(sceVoiceQoSEnd() == 0);
    Require(sceVoiceQoSEnd() == notInitialized);
    Require(sceVoiceQoSInit(nullptr, 0x40000, appType) == argumentInvalid);
}
