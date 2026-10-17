#pragma once
#include <cstdint>

namespace GuestSockets {

struct SocketOptionState {
    int reportedError = 0;
    bool nonblocking = false;
    bool noSignal = false;
    std::int64_t sendTimeout = 0;
    std::int64_t receiveTimeout = 0;
};

int SetSocketLevelOption(std::uintptr_t native, SocketOptionState& state, int option, const void* value, std::uint32_t length);
int GetSocketLevelOption(std::uintptr_t native, SocketOptionState& state, int type, int option, void* value, std::uint32_t* length);
int SetTcpLevelOption(std::uintptr_t native, bool tcp, int option, const void* value, std::uint32_t length);
int GetTcpLevelOption(std::uintptr_t native, bool tcp, int option, void* value, std::uint32_t* length);
int SetIpv6LevelOption(std::uintptr_t native, bool ipv6, int option, const void* value, std::uint32_t length);
int GetIpv6LevelOption(std::uintptr_t native, bool ipv6, int option, void* value, std::uint32_t* length);

}
