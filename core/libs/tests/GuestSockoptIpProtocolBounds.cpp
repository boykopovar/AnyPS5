#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int GuestAfInet = 2;
constexpr int GuestAfInet6 = 28;
constexpr int GuestSockStream = 1;
constexpr int GuestSockDgram = 2;

constexpr int LevelIp = 0;
constexpr int LevelIpv6 = 41;

constexpr int IpTypeOfService = 3;
constexpr int IpTimeToLive = 4;
constexpr int IpReceiveDestination = 7;
constexpr int IpMulticastInterface = 9;
constexpr int IpMulticastTimeToLive = 10;
constexpr int IpMulticastLoop = 11;
constexpr int IpReceiveInterface = 20;
constexpr int IpReceiveTimeToLive = 65;
constexpr int Ipv6Only = 27;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorNotSocket = 38;
constexpr int ErrorNoProtocolOption = 42;
constexpr int ErrorNotSupported = 45;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

} // namespace

int main() {
    int optionValue = 16;
    std::uint32_t optLength = sizeof(optionValue);

    const int v4Stream = socket_nid_postfix(GuestAfInet, GuestSockStream, 0);
    Require(v4Stream >= 0);

    const int v4Dgram = socket_nid_postfix(GuestAfInet, GuestSockDgram, 0);
    Require(v4Dgram >= 0);

    const int v6Stream = socket_nid_postfix(GuestAfInet6, GuestSockStream, 0);
    Require(v6Stream >= 0);

    const int v6Dgram = socket_nid_postfix(GuestAfInet6, GuestSockDgram, 0);
    Require(v6Dgram >= 0);

    int pipeDescriptors[2]{-1, -1};
    Require(pipe_nid_postfix(pipeDescriptors) == 0);

    const int closedDescriptor = socket_nid_postfix(GuestAfInet, GuestSockStream, 0);
    Require(closedDescriptor >= 0);
    Require(close_nid_postfix(closedDescriptor) == 0);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(-1, LevelIp, IpTimeToLive, nullptr, 4) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(9999, LevelIp, IpTimeToLive, nullptr, 4) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(closedDescriptor, LevelIp, IpTimeToLive, nullptr, 4) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(pipeDescriptors[0], LevelIp, IpTimeToLive, nullptr, 4) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Stream, LevelIp, IpTimeToLive, nullptr, 4) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(-1, LevelIp, IpTimeToLive, &optionValue, static_cast<std::uint32_t>(-1)) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(9999, LevelIp, IpTimeToLive, &optionValue, static_cast<std::uint32_t>(-1)) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(closedDescriptor, LevelIp, IpTimeToLive, &optionValue, static_cast<std::uint32_t>(-1)) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(pipeDescriptors[0], LevelIp, IpTimeToLive, &optionValue, static_cast<std::uint32_t>(-1)) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Stream, LevelIp, IpTimeToLive, &optionValue, static_cast<std::uint32_t>(-1)) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(-1, LevelIp, IpTimeToLive, &optionValue, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(9999, LevelIp, IpTimeToLive, &optionValue, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(closedDescriptor, LevelIp, IpTimeToLive, &optionValue, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(pipeDescriptors[0], LevelIp, IpTimeToLive, &optionValue, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(v4Stream, LevelIp, IpTimeToLive, &optionValue, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(-1, LevelIp, IpTimeToLive, &optionValue, sizeof(optionValue)) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(-100, LevelIp, IpTimeToLive, &optionValue, sizeof(optionValue)) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(9999, LevelIp, IpTimeToLive, &optionValue, sizeof(optionValue)) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(closedDescriptor, LevelIp, IpTimeToLive, &optionValue, sizeof(optionValue)) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(pipeDescriptors[0], LevelIp, IpTimeToLive, &optionValue, sizeof(optionValue)) == -1);
    Require(*__error_nid_postfix() == ErrorNotSocket);

    optLength = sizeof(optionValue);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(-1, LevelIp, IpTimeToLive, &optionValue, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    optLength = sizeof(optionValue);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(9999, LevelIp, IpTimeToLive, &optionValue, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    optLength = sizeof(optionValue);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(closedDescriptor, LevelIp, IpTimeToLive, &optionValue, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    optLength = sizeof(optionValue);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(pipeDescriptors[0], LevelIp, IpTimeToLive, &optionValue, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorNotSocket);

    Require(close_nid_postfix(pipeDescriptors[0]) == 0);
    Require(close_nid_postfix(pipeDescriptors[1]) == 0);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Stream, LevelIp, IpTypeOfService, &optionValue, 0) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Stream, LevelIp, IpTypeOfService, &optionValue, 3) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    optionValue = 0x10;
    Require(setsockopt_nid_postfix(v4Stream, LevelIp, IpTypeOfService, &optionValue, sizeof(optionValue)) == 0);

    int readbackTos = 0;
    optLength = sizeof(readbackTos);
    Require(getsockopt_nid_postfix(v4Stream, LevelIp, IpTypeOfService, &readbackTos, &optLength) == 0);
    Require(optLength == sizeof(readbackTos));
    Require(readbackTos == 0x10);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Stream, LevelIp, IpTimeToLive, &optionValue, 2) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    optionValue = 64;
    Require(setsockopt_nid_postfix(v4Stream, LevelIp, IpTimeToLive, &optionValue, sizeof(optionValue)) == 0);

    int readbackTtl = 0;
    optLength = sizeof(readbackTtl);
    Require(getsockopt_nid_postfix(v4Stream, LevelIp, IpTimeToLive, &readbackTtl, &optLength) == 0);
    Require(optLength == sizeof(readbackTtl));
    Require(readbackTtl == 64);

    readbackTtl = 0;
    optLength = 0;
    Require(getsockopt_nid_postfix(v4Stream, LevelIp, IpTimeToLive, &readbackTtl, &optLength) == 0);
    Require(optLength == 0 && readbackTtl == 0);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Stream, LevelIp, IpReceiveTimeToLive, &optionValue, 1) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    int receiveTtl = 1;
    Require(setsockopt_nid_postfix(v4Stream, LevelIp, IpReceiveTimeToLive, &receiveTtl, sizeof(receiveTtl)) == 0);
    int readbackRecvTtl = 0;
    optLength = sizeof(readbackRecvTtl);
    Require(getsockopt_nid_postfix(v4Stream, LevelIp, IpReceiveTimeToLive, &readbackRecvTtl, &optLength) == 0);
    Require(optLength == sizeof(readbackRecvTtl) && readbackRecvTtl == 1);

    receiveTtl = 0;
    Require(setsockopt_nid_postfix(v4Stream, LevelIp, IpReceiveTimeToLive, &receiveTtl, sizeof(receiveTtl)) == 0);
    readbackRecvTtl = -1;
    optLength = sizeof(readbackRecvTtl);
    Require(getsockopt_nid_postfix(v4Stream, LevelIp, IpReceiveTimeToLive, &readbackRecvTtl, &optLength) == 0);
    Require(readbackRecvTtl == 0);

    int mttl = 5;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Stream, LevelIp, IpMulticastTimeToLive, &mttl, sizeof(mttl)) == -1);
    Require(*__error_nid_postfix() == ErrorNotSupported);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(v4Stream, LevelIp, IpMulticastTimeToLive, &mttl, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorNotSupported);

    int mloop = 1;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Stream, LevelIp, IpMulticastLoop, &mloop, sizeof(mloop)) == -1);
    Require(*__error_nid_postfix() == ErrorNotSupported);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(v4Stream, LevelIp, IpMulticastLoop, &mloop, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorNotSupported);

    std::uint8_t byteMttl = 8;
    Require(setsockopt_nid_postfix(v4Dgram, LevelIp, IpMulticastTimeToLive, &byteMttl, 1) == 0);

    std::uint8_t readbackByteMttl = 0;
    optLength = 1;
    Require(getsockopt_nid_postfix(v4Dgram, LevelIp, IpMulticastTimeToLive, &readbackByteMttl, &optLength) == 0);
    Require(optLength == 1 && readbackByteMttl == 8);

    int intMttl = 12;
    Require(setsockopt_nid_postfix(v4Dgram, LevelIp, IpMulticastTimeToLive, &intMttl, sizeof(intMttl)) == 0);

    int readbackIntMttl = 0;
    optLength = sizeof(readbackIntMttl);
    Require(getsockopt_nid_postfix(v4Dgram, LevelIp, IpMulticastTimeToLive, &readbackIntMttl, &optLength) == 0);
    Require(optLength == sizeof(readbackIntMttl) && readbackIntMttl == 12);

    int invalidMttl = 256;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Dgram, LevelIp, IpMulticastTimeToLive, &invalidMttl, sizeof(invalidMttl)) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    std::uint8_t byteLoop = 1;
    Require(setsockopt_nid_postfix(v4Dgram, LevelIp, IpMulticastLoop, &byteLoop, 1) == 0);
    std::uint8_t readbackByteLoop = 0;
    optLength = 1;
    Require(getsockopt_nid_postfix(v4Dgram, LevelIp, IpMulticastLoop, &readbackByteLoop, &optLength) == 0);
    Require(optLength == 1 && readbackByteLoop == 1);

    int intLoop = 0;
    Require(setsockopt_nid_postfix(v4Dgram, LevelIp, IpMulticastLoop, &intLoop, sizeof(intLoop)) == 0);
    int readbackIntLoop = -1;
    optLength = sizeof(readbackIntLoop);
    Require(getsockopt_nid_postfix(v4Dgram, LevelIp, IpMulticastLoop, &readbackIntLoop, &optLength) == 0);
    Require(optLength == sizeof(readbackIntLoop) && readbackIntLoop == 0);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Stream, LevelIpv6, Ipv6Only, &optionValue, sizeof(optionValue)) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(v4Stream, LevelIpv6, Ipv6Only, &optionValue, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Dgram, LevelIpv6, Ipv6Only, &optionValue, sizeof(optionValue)) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(v4Dgram, LevelIpv6, Ipv6Only, &optionValue, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v6Stream, LevelIp, IpTimeToLive, &optionValue, sizeof(optionValue)) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(v6Stream, LevelIp, IpTimeToLive, &optionValue, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v6Dgram, LevelIp, IpTimeToLive, &optionValue, sizeof(optionValue)) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(v6Dgram, LevelIp, IpTimeToLive, &optionValue, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    int v6OnlyVal = 1;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v6Stream, LevelIpv6, Ipv6Only, &v6OnlyVal, 1) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v6Stream, LevelIpv6, Ipv6Only, &v6OnlyVal, 8) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    Require(setsockopt_nid_postfix(v6Stream, LevelIpv6, Ipv6Only, &v6OnlyVal, sizeof(v6OnlyVal)) == 0);
    int readbackV6Only = 0;
    optLength = sizeof(readbackV6Only);
    Require(getsockopt_nid_postfix(v6Stream, LevelIpv6, Ipv6Only, &readbackV6Only, &optLength) == 0);
    Require(optLength == sizeof(readbackV6Only) && readbackV6Only == 1);

    Require(setsockopt_nid_postfix(v6Dgram, LevelIpv6, Ipv6Only, &v6OnlyVal, sizeof(v6OnlyVal)) == 0);
    readbackV6Only = 0;
    optLength = sizeof(readbackV6Only);
    Require(getsockopt_nid_postfix(v6Dgram, LevelIpv6, Ipv6Only, &readbackV6Only, &optLength) == 0);
    Require(optLength == sizeof(readbackV6Only) && readbackV6Only == 1);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Stream, LevelIp, 9999, &optionValue, sizeof(optionValue)) == -1);
    Require(*__error_nid_postfix() == ErrorNoProtocolOption);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(v4Stream, LevelIp, 9999, &optionValue, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorNoProtocolOption);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v6Stream, LevelIpv6, 9999, &optionValue, sizeof(optionValue)) == -1);
    Require(*__error_nid_postfix() == ErrorNoProtocolOption);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(v6Stream, LevelIpv6, 9999, &optionValue, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorNoProtocolOption);

    optionValue = 48;
    Require(_setsockopt_nid_postfix(v4Stream, LevelIp, IpTimeToLive, &optionValue, sizeof(optionValue)) == 0);
    int twinTtl = 0;
    optLength = sizeof(twinTtl);
    Require(_getsockopt_nid_postfix(v4Stream, LevelIp, IpTimeToLive, &twinTtl, &optLength) == 0);
    Require(optLength == sizeof(twinTtl) && twinTtl == 48);

    *__error_nid_postfix() = 0;
    Require(_setsockopt_nid_postfix(-1, LevelIp, IpTimeToLive, nullptr, 4) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(_getsockopt_nid_postfix(-1, LevelIp, IpTimeToLive, &twinTtl, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    Require(close_nid_postfix(v4Stream) == 0);
    Require(close_nid_postfix(v4Dgram) == 0);
    Require(close_nid_postfix(v6Stream) == 0);
    Require(close_nid_postfix(v6Dgram) == 0);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Stream, LevelIp, IpTimeToLive, &optionValue, sizeof(optionValue)) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    optLength = sizeof(optionValue);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(v4Stream, LevelIp, IpTimeToLive, &optionValue, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    return 0;
}
