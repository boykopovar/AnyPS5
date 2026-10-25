#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#ifdef __APPLE__
#include <libproc.h>
#include <unistd.h>
#endif
#endif
#include "prx/libkernel/Socket/include/SocketError.hpp"
#include "prx/libkernel/Socket/include/SocketOptions.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace GuestSockets {
namespace {

constexpr int GuestAcceptConnection = 0x2;
constexpr int GuestReuseAddress = 0x4;
constexpr int GuestKeepAlive = 0x8;
constexpr int GuestDontRoute = 0x10;
constexpr int GuestBroadcast = 0x20;
constexpr int GuestLinger = 0x80;
constexpr int GuestOobInline = 0x100;
constexpr int GuestNoSigpipe = 0x800;
constexpr int GuestSendBuffer = 0x1001;
constexpr int GuestReceiveBuffer = 0x1002;
constexpr int GuestSendLowWater = 0x1003;
constexpr int GuestReceiveLowWater = 0x1004;
constexpr int GuestSendTimeout = 0x1005;
constexpr int GuestReceiveTimeout = 0x1006;
constexpr int GuestError = 0x1007;
constexpr int GuestType = 0x1008;
constexpr int GuestNonBlocking = 0x1200;
constexpr int GuestNonBlockingState = 0x100;
constexpr int GuestUnix = 1;
constexpr int GuestTcpNoDelay = 1;
constexpr int GuestReusePort = 0x200;
constexpr int KeptFlags = 0x1 | GuestReuseAddress | 0x40 | GuestReusePort | 0x400 | 0x2000 | 0x10000;
constexpr int WriteOnlyFlags = 0x4000 | 0x8000;
constexpr int GuestTcpNoDelayFlag = 4;
constexpr int GuestTcpMaximumSegment = 2;
constexpr int GuestTcpKeepIdle = 0x100;
constexpr int GuestTcpKeepInterval = 0x200;
constexpr int GuestTcpKeepCount = 0x400;
constexpr int GuestTcpNoPush = 4;
constexpr int GuestTcpNoOptions = 8;
constexpr int GuestTcpOption80 = 0x80;
constexpr int MinimumSegment = 216;
constexpr unsigned int TicksPerSecond = 1000;
constexpr unsigned int DefaultKeepIdle = 7200;
constexpr unsigned int DefaultKeepInterval = 75;
constexpr unsigned int DefaultKeepCount = 8;
constexpr int GuestIpv6Only = 27;
constexpr int GuestIpTypeOfService = 3;
constexpr int GuestIpTimeToLive = 4;
constexpr int GuestIpMulticastTimeToLive = 10;
constexpr int GuestIpMulticastLoop = 11;
constexpr int GuestIpMulticastInterface = 9;
constexpr int GuestIpAddMembership = 12;
constexpr int GuestIpDropMembership = 13;
constexpr int GuestIpReceiveDestination = 7;
constexpr int GuestIpReceiveInterface = 20;
constexpr int GuestIpReceiveTimeToLive = 65;
constexpr std::uint32_t MulticastRequestWithIndex = 12;
constexpr int ErrorNotSupported = 45;
constexpr int ErrorAddressInUse = 48;
constexpr int ErrorAddressNotAvailable = 49;

constexpr int ErrorInvalid = 22;
constexpr int ErrorDomain = 33;
constexpr int ErrorNoProtocolOption = 42;
constexpr int ErrorNoBuffers = 55;
constexpr int MaximumReservation = 5592405;
constexpr int MaximumBuffer = 524288;
constexpr int MaximumLinger = 65535;
constexpr std::int64_t LongestTimeout = 0x7fffffffffffffffLL;

struct GuestLingerValue {
    std::int32_t enabled;
    std::int32_t seconds;
};

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

std::int64_t TimeoutOf(const GuestTimeval& time) {
    if (time.seconds > INT32_MAX) return LongestTimeout;
    return (time.seconds << 32) + static_cast<std::int64_t>((static_cast<std::uint64_t>(time.microseconds) * ((std::uint64_t{1} << 63) / 500000)) >> 32);
}

GuestTimeval TimevalOf(std::int64_t timeout) {
    return {timeout >> 32, (1000000 * static_cast<std::int64_t>(static_cast<std::uint32_t>(timeout))) >> 32};
}

#ifdef _WIN32
using NativeHandle = SOCKET;
using NativeOptionValue = const char*;
using NativeOptionOutput = char*;
int HostError() { return GuestErrorFromHost(WSAGetLastError()); }
#else
using NativeHandle = int;
using NativeOptionValue = const void*;
using NativeOptionOutput = void*;
int HostError() { return GuestErrorFromHost(errno); }
#endif

int NativeBoolean(int option) {
    switch (option) {
        case GuestKeepAlive: return SO_KEEPALIVE;
        case GuestDontRoute: return SO_DONTROUTE;
        case GuestBroadcast: return SO_BROADCAST;
        case GuestOobInline: return SO_OOBINLINE;
        default: return -1;
    }
}

void EnsureBuffers(SocketOptionState& state, int family, int type) {
    if (state.sendBuffer != 0) return;
    if (family == GuestUnix) {
        state.sendBuffer = type == 2 ? 2048 : 8192;
        state.receiveBuffer = type == 2 ? 4096 : 8192;
    } else {
        state.sendBuffer = type == 1 ? 32768 : 9216;
        state.receiveBuffer = type == 1 ? 65536 : 42080;
    }
    state.sendLowWater = std::min(2048, state.sendBuffer);
    state.receiveLowWater = 1;
}

int TcpFlagBit(int option) {
    switch (option) {
        case GuestTcpNoPush: return 0x1000;
        case GuestTcpNoOptions: return 0x8;
        case GuestTcpOption80: return 0x10000;
        default: return 0;
    }
}

int IpReceiveBit(int option) {
    switch (option) {
        case GuestIpReceiveDestination: return 1;
        case GuestIpReceiveInterface: return 2;
        case GuestIpReceiveTimeToLive: return 4;
        default: return 0;
    }
}

int Copy(void* output, std::uint32_t* length, const void* value, std::uint32_t size) {
    const std::uint32_t count = output ? std::min(*length, size) : 0;
    if (count != 0) std::memcpy(output, value, count);
    *length = count;
    return 0;
}

}

int SetSocketLevelOption(std::uintptr_t native, SocketOptionState& state, int family, int type, int option, const void* value, std::uint32_t length) {
    const auto handle = static_cast<NativeHandle>(native);
    if (option == GuestLinger) {
        if (length < sizeof(GuestLingerValue)) return ErrorInvalid;
        GuestLingerValue guest;
        std::memcpy(&guest, value, sizeof(guest));
        if (guest.seconds < 0 || guest.seconds > MaximumLinger) return ErrorDomain;
#ifdef _WIN32
        linger request{};
        request.l_onoff = guest.enabled != 0 ? 1 : 0;
        request.l_linger = static_cast<u_short>(guest.seconds);
#else
        linger request{};
        request.l_onoff = guest.enabled != 0 ? 1 : 0;
        request.l_linger = guest.seconds;
#endif
        if (::setsockopt(handle, SOL_SOCKET, SO_LINGER, reinterpret_cast<NativeOptionValue>(&request), sizeof(request))) return HostError();
        return 0;
    }
    if (option == GuestSendTimeout || option == GuestReceiveTimeout) {
        if (length < sizeof(GuestTimeval)) return ErrorInvalid;
        GuestTimeval time;
        std::memcpy(&time, value, sizeof(time));
        if (time.seconds < 0 || time.microseconds < 0 || time.microseconds >= 1000000) return ErrorDomain;
        (option == GuestSendTimeout ? state.sendTimeout : state.receiveTimeout) = TimeoutOf(time);
        return 0;
    }
    if (option == GuestNoSigpipe || option == GuestNonBlocking) {
        if (length < sizeof(int)) return ErrorInvalid;
        int number;
        std::memcpy(&number, value, sizeof(number));
        if (option == GuestNoSigpipe) {
            state.noSignal = number != 0;
            return 0;
        }
#ifdef _WIN32
        unsigned long enabled = number != 0;
        if (ioctlsocket(handle, FIONBIO, &enabled)) return HostError();
#else
        int enabled = number != 0;
        if (::ioctl(handle, FIONBIO, &enabled)) return HostError();
#endif
        state.nonblocking = enabled != 0;
        return 0;
    }
    const bool native_boolean = NativeBoolean(option) != -1;
    const bool buffer = option == GuestSendBuffer || option == GuestReceiveBuffer;
    const bool lowWater = option == GuestSendLowWater || option == GuestReceiveLowWater;
    if (native_boolean || buffer || lowWater) {
        if (length < sizeof(int)) return ErrorInvalid;
        int number;
        std::memcpy(&number, value, sizeof(number));
        if (buffer || lowWater) {
            if (number < 1) return ErrorInvalid;
            EnsureBuffers(state, family, type);
            const bool send = option == GuestSendBuffer || option == GuestSendLowWater;
            int& size = send ? state.sendBuffer : state.receiveBuffer;
            int& mark = send ? state.sendLowWater : state.receiveLowWater;
            if (lowWater) {
                mark = std::min(number, size);
                return 0;
            }
            if (number > MaximumReservation) return ErrorNoBuffers;
            const int nativeOption = send ? SO_SNDBUF : SO_RCVBUF;
            if (::setsockopt(handle, SOL_SOCKET, nativeOption, reinterpret_cast<NativeOptionValue>(&number), sizeof(number))) return HostError();
            size = std::min(number, MaximumBuffer);
            mark = std::min(mark, size);
            return 0;
        }
        const int enabled = number != 0 ? 1 : 0;
        if (::setsockopt(handle, SOL_SOCKET, NativeBoolean(option), reinterpret_cast<NativeOptionValue>(&enabled), sizeof(enabled))) return HostError();
        return 0;
    }
    if ((option & (KeptFlags | WriteOnlyFlags)) != 0 && (option & (option - 1)) == 0) {
        if (length < sizeof(int)) return ErrorInvalid;
        int number;
        std::memcpy(&number, value, sizeof(number));
        if ((option & KeptFlags) == 0) return 0;
        const int flags = number != 0 ? state.keptFlags | option : state.keptFlags & ~option;
        if (option == GuestReuseAddress || option == GuestReusePort) {
#ifdef _WIN32
            const int reuseAddress = (flags & (GuestReuseAddress | GuestReusePort)) != 0 ? 1 : 0;
#else
            const int reuseAddress = (flags & GuestReuseAddress) != 0 ? 1 : 0;
            const int reusePort = (flags & GuestReusePort) != 0 ? 1 : 0;
            if (::setsockopt(handle, SOL_SOCKET, SO_REUSEPORT, reinterpret_cast<NativeOptionValue>(&reusePort), sizeof(reusePort))) return HostError();
#endif
            if (::setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<NativeOptionValue>(&reuseAddress), sizeof(reuseAddress))) return HostError();
        }
        state.keptFlags = flags;
        return 0;
    }
    return ErrorNoProtocolOption;
}

bool NativeListening(std::uintptr_t native, bool& listening) {
#ifdef __APPLE__
    socket_fdinfo info{};
    if (proc_pidfdinfo(getpid(), static_cast<int>(native), PROC_PIDFDSOCKETINFO, &info, sizeof(info)) != static_cast<int>(sizeof(info))) return false;
    listening = (info.psi.soi_options & SO_ACCEPTCONN) != 0;
#else
    int accepting = 0;
    socklen_t size = sizeof(accepting);
    if (::getsockopt(static_cast<NativeHandle>(native), SOL_SOCKET, SO_ACCEPTCONN, reinterpret_cast<NativeOptionOutput>(&accepting), &size)) return false;
    listening = accepting != 0;
#endif
    return true;
}

int GetSocketLevelOption(std::uintptr_t native, SocketOptionState& state, int family, int type, int option, void* value, std::uint32_t* length) {
    const auto handle = static_cast<NativeHandle>(native);
    if (option == GuestLinger) {
        linger current{};
        socklen_t size = sizeof(current);
        if (::getsockopt(handle, SOL_SOCKET, SO_LINGER, reinterpret_cast<NativeOptionOutput>(&current), &size)) return HostError();
        const GuestLingerValue guest{current.l_onoff != 0 ? GuestLinger : 0, static_cast<std::int32_t>(current.l_linger)};
        return Copy(value, length, &guest, sizeof(guest));
    }
    if (option == GuestSendTimeout || option == GuestReceiveTimeout) {
        const GuestTimeval time = TimevalOf(option == GuestSendTimeout ? state.sendTimeout : state.receiveTimeout);
        return Copy(value, length, &time, sizeof(time));
    }
    int number = 0;
    if (const int nativeOption = NativeBoolean(option); nativeOption != -1) {
        int enabled = 0;
        socklen_t size = sizeof(enabled);
        if (::getsockopt(handle, SOL_SOCKET, nativeOption, reinterpret_cast<NativeOptionOutput>(&enabled), &size)) return HostError();
        number = enabled != 0 ? option : 0;
    } else if (option == GuestAcceptConnection) {
        bool listening = false;
        if (!NativeListening(native, listening)) return HostError();
        number = listening ? GuestAcceptConnection : 0;
    } else if (option == GuestType) {
        number = type;
    } else if ((option & KeptFlags) != 0 && (option & (option - 1)) == 0) {
        number = state.keptFlags & option;
    } else if (option == GuestNoSigpipe) {
        number = state.noSignal ? GuestNoSigpipe : 0;
    } else if (option == GuestNonBlocking) {
        number = state.nonblocking ? GuestNonBlockingState : 0;
    } else if (option == GuestError) {
        int pending = 0;
        socklen_t size = sizeof(pending);
        if (::getsockopt(handle, SOL_SOCKET, SO_ERROR, reinterpret_cast<NativeOptionOutput>(&pending), &size)) return HostError();
#ifdef _WIN32
        if (pending != 0 && pending == state.reportedError) pending = 0;
        else if (pending != 0) state.reportedError = pending;
#endif
        number = pending == 0 ? 0 : GuestErrorFromHost(pending);
    } else if (option == GuestSendBuffer || option == GuestReceiveBuffer || option == GuestSendLowWater || option == GuestReceiveLowWater) {
        EnsureBuffers(state, family, type);
        number = option == GuestSendBuffer ? state.sendBuffer : option == GuestReceiveBuffer ? state.receiveBuffer
            : option == GuestSendLowWater ? state.sendLowWater : state.receiveLowWater;
    } else {
        return ErrorNoProtocolOption;
    }
    return Copy(value, length, &number, sizeof(number));
}

int SetTcpLevelOption(std::uintptr_t native, SocketOptionState& state, bool tcp, int option, const void* value, std::uint32_t length) {
    if (!tcp) return ErrorInvalid;
    if (option != GuestTcpNoDelay && option != GuestTcpMaximumSegment && option != GuestTcpKeepIdle && option != GuestTcpKeepInterval &&
        option != GuestTcpKeepCount && TcpFlagBit(option) == 0)
        return ErrorNoProtocolOption;
    if (length < sizeof(int)) return ErrorInvalid;
    int number;
    std::memcpy(&number, value, sizeof(number));
    if (const int bit = TcpFlagBit(option); bit != 0) {
        state.tcpFlags = number != 0 ? state.tcpFlags | bit : state.tcpFlags & ~bit;
        return 0;
    }
    const auto unsignedNumber = static_cast<unsigned int>(number);
    if (option == GuestTcpMaximumSegment) {
        if (number <= 0 || number > state.tcpMaximumSegment || number + 40 < MinimumSegment) return ErrorInvalid;
        state.tcpMaximumSegment = number;
        return 0;
    }
    if (option == GuestTcpKeepCount) {
        state.tcpKeepCount = unsignedNumber;
        return 0;
    }
    if (option == GuestTcpKeepIdle || option == GuestTcpKeepInterval) {
        if (unsignedNumber > UINT32_MAX / TicksPerSecond) return ErrorInvalid;
        (option == GuestTcpKeepIdle ? state.tcpKeepIdle : state.tcpKeepInterval) = unsignedNumber;
        return 0;
    }
    const int enabled = number != 0 ? 1 : 0;
    if (::setsockopt(static_cast<NativeHandle>(native), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<NativeOptionValue>(&enabled), sizeof(enabled))) return HostError();
    return 0;
}

int GetTcpLevelOption(std::uintptr_t native, SocketOptionState& state, bool tcp, int option, void* value, std::uint32_t* length) {
    if (!tcp) return ErrorInvalid;
    if (option == GuestTcpMaximumSegment || option == GuestTcpKeepIdle || option == GuestTcpKeepInterval || option == GuestTcpKeepCount) {
        const unsigned int number = option == GuestTcpMaximumSegment ? static_cast<unsigned int>(state.tcpMaximumSegment)
            : option == GuestTcpKeepIdle ? (state.tcpKeepIdle != 0 ? state.tcpKeepIdle : DefaultKeepIdle)
            : option == GuestTcpKeepInterval ? (state.tcpKeepInterval != 0 ? state.tcpKeepInterval : DefaultKeepInterval)
            : (state.tcpKeepCount != 0 ? state.tcpKeepCount : DefaultKeepCount);
        return Copy(value, length, &number, sizeof(number));
    }
    if (const int bit = TcpFlagBit(option); bit != 0) {
        const int number = state.tcpFlags & bit;
        return Copy(value, length, &number, sizeof(number));
    }
    if (option != GuestTcpNoDelay) return ErrorNoProtocolOption;
    int enabled = 0;
    socklen_t size = sizeof(enabled);
    if (::getsockopt(static_cast<NativeHandle>(native), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<NativeOptionOutput>(&enabled), &size)) return HostError();
    const int number = enabled != 0 ? GuestTcpNoDelayFlag : 0;
    return Copy(value, length, &number, sizeof(number));
}

bool MulticastOption(int option) {
    return option == GuestIpMulticastInterface || option == GuestIpMulticastTimeToLive || option == GuestIpMulticastLoop ||
        option == GuestIpAddMembership || option == GuestIpDropMembership;
}

int SetIpLevelOption(std::uintptr_t native, SocketOptionState& state, bool ipv4, bool datagram, int option, const void* value, std::uint32_t length) {
    if (!ipv4) return ErrorInvalid;
    if (MulticastOption(option) && !datagram) return ErrorNotSupported;
    const auto handle = static_cast<NativeHandle>(native);
    if (option == GuestIpAddMembership || option == GuestIpDropMembership) {
        if (length < sizeof(ip_mreq)) return ErrorInvalid;
        ip_mreq request{};
        std::memcpy(&request, value, sizeof(request));
        const auto* group = reinterpret_cast<const unsigned char*>(&request.imr_multiaddr);
        if ((group[0] & 0xf0) != 0xe0) return ErrorInvalid;
        std::uint32_t groupBits;
        std::uint32_t interfaceBits;
        std::memcpy(&groupBits, &request.imr_multiaddr, sizeof(groupBits));
        std::memcpy(&interfaceBits, &request.imr_interface, sizeof(interfaceBits));
        const std::uint64_t key = (std::uint64_t{groupBits} << 32) | interfaceBits;
        const bool join = option == GuestIpAddMembership;
        const auto found = join || interfaceBits != 0
            ? std::find(state.memberships.begin(), state.memberships.end(), key)
            : std::find_if(state.memberships.begin(), state.memberships.end(), [groupBits](std::uint64_t joined) { return joined >> 32 == groupBits; });
        if (join && found != state.memberships.end()) return ErrorAddressInUse;
        if (!join && found == state.memberships.end()) return ErrorAddressNotAvailable;
        if (!join) {
            const auto joinedInterface = static_cast<std::uint32_t>(*found);
            std::memcpy(&request.imr_interface, &joinedInterface, sizeof(joinedInterface));
        }
        if (::setsockopt(handle, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, reinterpret_cast<NativeOptionValue>(&request), sizeof(request)))
            return HostError();
        if (join) state.memberships.push_back(key);
        else state.memberships.erase(found);
        return 0;
    }
    if (option == GuestIpMulticastInterface) {
        if (length < sizeof(in_addr)) return ErrorInvalid;
        in_addr address{};
        if (length == MulticastRequestWithIndex) {
            std::int32_t index;
            std::memcpy(&index, static_cast<const unsigned char*>(value) + 8, sizeof(index));
            if (index != 0) throw std::runtime_error("IP_MULTICAST_IF: an interface index is not supported");
            const auto* addresses = static_cast<const unsigned char*>(value);
            if (std::any_of(addresses, addresses + 8, [](unsigned char byte) { return byte != 0; }))
                throw std::runtime_error("IP_MULTICAST_IF: an ip_mreqn with a non-zero address is not supported");
        } else {
            std::memcpy(&address, value, sizeof(address));
        }
        if (::setsockopt(handle, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<NativeOptionValue>(&address), sizeof(address))) return HostError();
        std::memcpy(&state.multicastInterface, &address, sizeof(address));
        return 0;
    }
    if (const int bit = IpReceiveBit(option); bit != 0) {
        if (length < sizeof(int)) return ErrorInvalid;
        int number;
        std::memcpy(&number, value, sizeof(number));
        state.ipReceiveOptions = number != 0 ? state.ipReceiveOptions | bit : state.ipReceiveOptions & ~bit;
        return 0;
    }
    if (option == GuestIpTimeToLive || option == GuestIpTypeOfService) {
        if (length < sizeof(int)) return ErrorInvalid;
        int number;
        std::memcpy(&number, value, sizeof(number));
        const int stored = number & 0xff;
        if (option == GuestIpTypeOfService) {
            state.ipTypeOfService = stored;
            return 0;
        }
        if (stored != 0 && ::setsockopt(handle, IPPROTO_IP, IP_TTL, reinterpret_cast<NativeOptionValue>(&stored), sizeof(stored))) return HostError();
        state.ipTimeToLive = stored;
        return 0;
    }
    if (option == GuestIpMulticastTimeToLive || option == GuestIpMulticastLoop) {
        unsigned int number;
        if (length == 1) {
            unsigned char byte;
            std::memcpy(&byte, value, 1);
            number = byte;
        } else {
            if (length < sizeof(number)) return ErrorInvalid;
            std::memcpy(&number, value, sizeof(number));
            if (option == GuestIpMulticastTimeToLive && number > 255) return ErrorInvalid;
        }
        const int host = option == GuestIpMulticastLoop ? ((number & 0xff) != 0 ? 1 : 0) : static_cast<int>(number);
        const int nativeOption = option == GuestIpMulticastLoop ? IP_MULTICAST_LOOP : IP_MULTICAST_TTL;
        if (::setsockopt(handle, IPPROTO_IP, nativeOption, reinterpret_cast<NativeOptionValue>(&host), sizeof(host))) return HostError();
        if (option == GuestIpMulticastLoop) state.multicastLoop = host != 0;
        else state.multicastTimeToLive = host;
        return 0;
    }
    return ErrorNoProtocolOption;
}

int GetIpLevelOption(SocketOptionState& state, bool ipv4, bool datagram, int option, void* value, std::uint32_t* length) {
    if (!ipv4) return ErrorInvalid;
    if (MulticastOption(option) && option != GuestIpAddMembership && option != GuestIpDropMembership && !datagram) return ErrorNotSupported;
    if (option == GuestIpMulticastInterface) {
        if (*length == MulticastRequestWithIndex) throw std::runtime_error("IP_MULTICAST_IF: reading an ip_mreqn is not supported");
        return Copy(value, length, &state.multicastInterface, sizeof(state.multicastInterface));
    }
    int number;
    if (const int bit = IpReceiveBit(option); bit != 0) number = (state.ipReceiveOptions & bit) != 0 ? 1 : 0;
    else if (option == GuestIpTimeToLive) number = state.ipTimeToLive;
    else if (option == GuestIpTypeOfService) number = state.ipTypeOfService;
    else if (option == GuestIpMulticastTimeToLive) number = state.multicastTimeToLive;
    else if (option == GuestIpMulticastLoop) number = state.multicastLoop ? 1 : 0;
    else return ErrorNoProtocolOption;
    if ((option == GuestIpMulticastTimeToLive || option == GuestIpMulticastLoop) && *length == 1) {
        const auto byte = static_cast<unsigned char>(number);
        return Copy(value, length, &byte, 1);
    }
    return Copy(value, length, &number, sizeof(number));
}

int SetIpv6LevelOption(std::uintptr_t native, bool ipv6, int option, const void* value, std::uint32_t length) {
    if (!ipv6) return ErrorInvalid;
    if (option != GuestIpv6Only) return ErrorNoProtocolOption;
    if (length != sizeof(int)) return ErrorInvalid;
    int number;
    std::memcpy(&number, value, sizeof(number));
    const int enabled = number != 0 ? 1 : 0;
    if (::setsockopt(static_cast<NativeHandle>(native), IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<NativeOptionValue>(&enabled), sizeof(enabled))) return HostError();
    return 0;
}

int GetIpv6LevelOption(std::uintptr_t native, bool ipv6, int option, void* value, std::uint32_t* length) {
    if (!ipv6) return ErrorInvalid;
    if (option != GuestIpv6Only) return ErrorNoProtocolOption;
    int enabled = 0;
    socklen_t size = sizeof(enabled);
    if (::getsockopt(static_cast<NativeHandle>(native), IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<NativeOptionOutput>(&enabled), &size)) return HostError();
    const int number = enabled != 0 ? 1 : 0;
    return Copy(value, length, &number, sizeof(number));
}

}
