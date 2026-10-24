#pragma once
#include <cstdint>
#include <vector>

namespace GuestSockets {

struct SocketOptionState {
    int reportedError = 0;
    bool nonblocking = false;
    bool noSignal = false;
    std::int64_t sendTimeout = 0;
    std::int64_t receiveTimeout = 0;
    int sendBuffer = 0;
    int receiveBuffer = 0;
    int sendLowWater = 0;
    int receiveLowWater = 0;
    int ipTimeToLive = 64;
    int ipTypeOfService = 0;
    int multicastTimeToLive = 1;
    bool multicastLoop = true;
    int tcpMaximumSegment = 536;
    unsigned int tcpKeepIdle = 0;
    unsigned int tcpKeepInterval = 0;
    unsigned int tcpKeepCount = 0;
    int keptFlags = 0;
    std::uint32_t multicastInterface = 0;
    std::vector<std::uint64_t> memberships;
};

bool NativeListening(std::uintptr_t native, bool& listening);
int SetSocketLevelOption(std::uintptr_t native, SocketOptionState& state, int family, int type, int option, const void* value, std::uint32_t length);
int GetSocketLevelOption(std::uintptr_t native, SocketOptionState& state, int family, int type, int option, void* value, std::uint32_t* length);
int SetTcpLevelOption(std::uintptr_t native, SocketOptionState& state, bool tcp, int option, const void* value, std::uint32_t length);
int GetTcpLevelOption(std::uintptr_t native, SocketOptionState& state, bool tcp, int option, void* value, std::uint32_t* length);
int SetIpLevelOption(std::uintptr_t native, SocketOptionState& state, bool ipv4, bool datagram, int option, const void* value, std::uint32_t length);
int GetIpLevelOption(SocketOptionState& state, bool ipv4, bool datagram, int option, void* value, std::uint32_t* length);
int SetIpv6LevelOption(std::uintptr_t native, bool ipv6, int option, const void* value, std::uint32_t length);
int GetIpv6LevelOption(std::uintptr_t native, bool ipv6, int option, void* value, std::uint32_t* length);

}
