#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

constexpr int SolSocket = 0xffff;
constexpr int SoTimestamp = 0x400;
constexpr int SoBintime = 0x2000;
constexpr int ScmTimestamp = 2;
constexpr int ScmBintime = 4;
constexpr int AfInet = 2;
constexpr int AfInet6 = 28;
constexpr int SockDgram = 2;
constexpr int SockStream = 1;
constexpr int MsgPeek = 2;
constexpr int MsgCtrunc = 0x20;

constexpr int ErrorBadf = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorAfNoSupport = 47;
constexpr int ErrorAddrNotAvail = 49;

static void CheckOptionPrecedenceAndBounds() {
    int one = 1;
    Require(setsockopt_nid_postfix(-1, SolSocket, SoTimestamp, &one, sizeof(one)) == -1 && *__error_nid_postfix() == ErrorBadf);
    Require(setsockopt_nid_postfix(-1, SolSocket, SoBintime, &one, sizeof(one)) == -1 && *__error_nid_postfix() == ErrorBadf);
    int value = 0;
    std::uint32_t length = sizeof(value);
    Require(getsockopt_nid_postfix(-1, SolSocket, SoTimestamp, &value, &length) == -1 && *__error_nid_postfix() == ErrorBadf);
    Require(getsockopt_nid_postfix(-1, SolSocket, SoBintime, &value, &length) == -1 && *__error_nid_postfix() == ErrorBadf);

    const int socketFd = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(socketFd >= 0);

    Require(setsockopt_nid_postfix(socketFd, SolSocket, SoTimestamp, nullptr, sizeof(one)) == -1 && *__error_nid_postfix() == ErrorFault);
    Require(setsockopt_nid_postfix(socketFd, SolSocket, SoBintime, nullptr, sizeof(one)) == -1 && *__error_nid_postfix() == ErrorFault);
    Require(setsockopt_nid_postfix(socketFd, SolSocket, SoTimestamp, reinterpret_cast<void*>(0x10), sizeof(one)) == -1 && *__error_nid_postfix() == ErrorFault);
    Require(setsockopt_nid_postfix(socketFd, SolSocket, SoBintime, reinterpret_cast<void*>(0x10), sizeof(one)) == -1 && *__error_nid_postfix() == ErrorFault);

    Require(setsockopt_nid_postfix(socketFd, SolSocket, SoTimestamp, &one, 0) == -1 && *__error_nid_postfix() == ErrorInvalid);
    Require(setsockopt_nid_postfix(socketFd, SolSocket, SoTimestamp, &one, 3) == -1 && *__error_nid_postfix() == ErrorInvalid);
    Require(setsockopt_nid_postfix(socketFd, SolSocket, SoBintime, &one, 0) == -1 && *__error_nid_postfix() == ErrorInvalid);
    Require(setsockopt_nid_postfix(socketFd, SolSocket, SoBintime, &one, 2) == -1 && *__error_nid_postfix() == ErrorInvalid);

    length = sizeof(value);
    Require(getsockopt_nid_postfix(socketFd, SolSocket, SoTimestamp, nullptr, &length) == 0 && length == 0);
    Require(getsockopt_nid_postfix(socketFd, SolSocket, SoTimestamp, &value, nullptr) == -1 && *__error_nid_postfix() == ErrorFault);
    length = sizeof(value);
    Require(getsockopt_nid_postfix(socketFd, SolSocket, SoBintime, nullptr, &length) == 0 && length == 0);
    Require(getsockopt_nid_postfix(socketFd, SolSocket, SoBintime, &value, nullptr) == -1 && *__error_nid_postfix() == ErrorFault);
    Require(getsockopt_nid_postfix(socketFd, SolSocket, SoTimestamp, reinterpret_cast<void*>(0x10), &length) == -1 && *__error_nid_postfix() == ErrorFault);
    Require(getsockopt_nid_postfix(socketFd, SolSocket, SoTimestamp, &value, reinterpret_cast<std::uint32_t*>(0x10)) == -1 && *__error_nid_postfix() == ErrorFault);

    value = -1;
    length = sizeof(value);
    Require(getsockopt_nid_postfix(socketFd, SolSocket, SoTimestamp, &value, &length) == 0 && value == 0 && length == sizeof(int));
    value = -1;
    length = sizeof(value);
    Require(getsockopt_nid_postfix(socketFd, SolSocket, SoBintime, &value, &length) == 0 && value == 0 && length == sizeof(int));

    for (const int option : {SoTimestamp, SoBintime}) {
        int enable = 1;
        Require(setsockopt_nid_postfix(socketFd, SolSocket, option, &enable, sizeof(enable)) == 0);
        value = 0;
        length = sizeof(value);
        Require(getsockopt_nid_postfix(socketFd, SolSocket, option, &value, &length) == 0 && value == option && length == sizeof(int));

        enable = 0;
        Require(setsockopt_nid_postfix(socketFd, SolSocket, option, &enable, sizeof(enable)) == 0);
        value = -1;
        length = sizeof(value);
        Require(getsockopt_nid_postfix(socketFd, SolSocket, option, &value, &length) == 0 && value == 0 && length == sizeof(int));

        std::int64_t wide = 1;
        Require(setsockopt_nid_postfix(socketFd, SolSocket, option, &wide, sizeof(wide)) == 0);
        value = 0;
        length = sizeof(value);
        Require(getsockopt_nid_postfix(socketFd, SolSocket, option, &value, &length) == 0 && value == option && length == sizeof(int));

        wide = 0;
        Require(setsockopt_nid_postfix(socketFd, SolSocket, option, &wide, sizeof(wide)) == 0);
        value = -1;
        length = sizeof(value);
        Require(getsockopt_nid_postfix(socketFd, SolSocket, option, &value, &length) == 0 && value == 0 && length == sizeof(int));
    }

    Require(close_nid_postfix(socketFd) == 0);
    Require(close_nid_postfix(socketFd) == -1 && *__error_nid_postfix() == ErrorBadf);
}

static void CheckIpv4UdpTimestampsAndBounds() {
    const int receiver = socket_nid_postfix(AfInet, SockDgram, 0);
    const int sender = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(receiver >= 0 && sender >= 0);

    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(receiver, address.data(), address.size()) == 0);
    std::uint32_t addressLen = address.size();
    Require(getsockname_nid_postfix(receiver, address.data(), &addressLen) == 0);
    Require(connect_nid_postfix(sender, address.data(), address.size()) == 0);

    const char payload[] = "ping";
    char receivedPayload[sizeof(payload)]{};
    KernelIovec iov{receivedPayload, sizeof(receivedPayload)};

    Require(sendto_nid_postfix(sender, payload, sizeof(payload), 0, nullptr, 0) == sizeof(payload));
    std::array<unsigned char, 128> controlBuffer;
    controlBuffer.fill(0xa5);
    GuestMsghdr msg{nullptr, 0, &iov, 1, controlBuffer.data(), static_cast<std::uint32_t>(controlBuffer.size()), -1};
    Require(recvmsg_nid_postfix(receiver, &msg, 0) == sizeof(payload));
    Require(msg.controlLength == 0 && msg.flags == 0);

    for (const int opt : {SoTimestamp, SoBintime}) {
        int enable = 1;
        Require(setsockopt_nid_postfix(receiver, SolSocket, opt, &enable, sizeof(enable)) == 0);
        KernelTimespec before{};
        Require(clock_gettime_nid_postfix(0, &before) == 0);
        Require(sendto_nid_postfix(sender, payload, sizeof(payload), 0, nullptr, 0) == sizeof(payload));
        controlBuffer.fill(0xa5);
        msg = GuestMsghdr{nullptr, 0, &iov, 1, controlBuffer.data(), 64, -1};
        Require(recvmsg_nid_postfix(receiver, &msg, 0) == sizeof(payload));
        KernelTimespec after{};
        Require(clock_gettime_nid_postfix(0, &after) == 0);
        Require(msg.controlLength == 32 && msg.flags == 0);
        Require(std::all_of(controlBuffer.begin() + 32, controlBuffer.end(), [](auto byte) { return byte == 0xa5; }));

        std::uint32_t length = 0;
        int level = 0;
        int type = 0;
        std::int64_t seconds = 0;
        std::uint64_t fraction = 0;
        std::memcpy(&length, controlBuffer.data(), sizeof(length));
        std::memcpy(&level, controlBuffer.data() + 4, sizeof(level));
        std::memcpy(&type, controlBuffer.data() + 8, sizeof(type));
        std::memcpy(&seconds, controlBuffer.data() + 16, sizeof(seconds));
        std::memcpy(&fraction, controlBuffer.data() + 24, sizeof(fraction));
        Require(length == 32 && level == SolSocket);
        Require(type == (opt == SoTimestamp ? ScmTimestamp : ScmBintime));
        Require(seconds >= before.tv_sec && seconds <= after.tv_sec);
        if (opt == SoTimestamp) {
            Require(fraction < 1000000);
        }

        enable = 0;
        Require(setsockopt_nid_postfix(receiver, SolSocket, opt, &enable, sizeof(enable)) == 0);
    }

    int enable = 1;
    Require(setsockopt_nid_postfix(receiver, SolSocket, SoTimestamp, &enable, sizeof(enable)) == 0);
    Require(setsockopt_nid_postfix(receiver, SolSocket, SoBintime, &enable, sizeof(enable)) == 0);

    constexpr std::uint32_t expectedDualSize = 64;
    for (const std::uint32_t capacity : {0u, 1u, 8u, 16u, 31u, 32u, 40u, 48u, 63u, 64u, 80u, 128u}) {
        Require(sendto_nid_postfix(sender, payload, sizeof(payload), 0, nullptr, 0) == sizeof(payload));
        controlBuffer.fill(0x5a);
        msg = GuestMsghdr{nullptr, 0, &iov, 1, controlBuffer.data(), capacity, -1};
        Require(recvmsg_nid_postfix(receiver, &msg, 0) == sizeof(payload));
        const std::uint32_t copied = std::min(capacity, expectedDualSize);
        Require(msg.controlLength == copied);
        Require(msg.flags == (capacity < expectedDualSize ? MsgCtrunc : 0));
        Require(std::all_of(controlBuffer.begin() + copied, controlBuffer.end(), [](auto byte) { return byte == 0x5a; }));
        if (copied >= 32) {
            std::uint32_t length = 0;
            int level = 0;
            int type = 0;
            std::memcpy(&length, controlBuffer.data(), sizeof(length));
            std::memcpy(&level, controlBuffer.data() + 4, sizeof(level));
            std::memcpy(&type, controlBuffer.data() + 8, sizeof(type));
            Require(length == 32 && level == SolSocket && type == ScmBintime);
        }
        if (copied >= 64) {
            std::uint32_t length = 0;
            int level = 0;
            int type = 0;
            std::memcpy(&length, controlBuffer.data() + 32, sizeof(length));
            std::memcpy(&level, controlBuffer.data() + 36, sizeof(level));
            std::memcpy(&type, controlBuffer.data() + 40, sizeof(type));
            Require(length == 32 && level == SolSocket && type == ScmTimestamp);
        }
    }

    Require(sendto_nid_postfix(sender, payload, sizeof(payload), 0, nullptr, 0) == sizeof(payload));
    msg = GuestMsghdr{nullptr, 0, &iov, 1, nullptr, 0, -1};
    Require(recvmsg_nid_postfix(receiver, &msg, 0) == sizeof(payload));
    Require(msg.controlLength == 0 && msg.flags == 0);

    Require(sendto_nid_postfix(sender, payload, sizeof(payload), 0, nullptr, 0) == sizeof(payload));
    msg = GuestMsghdr{nullptr, 0, &iov, 1, reinterpret_cast<void*>(0x10), 64, -1};
    Require(recvmsg_nid_postfix(receiver, &msg, 0) == -1 && *__error_nid_postfix() == ErrorFault);

    char drain[16]{};
    Require(recvfrom_nid_postfix(receiver, drain, sizeof(drain), 0, nullptr, nullptr) == sizeof(payload));

    Require(close_nid_postfix(sender) == 0 && close_nid_postfix(receiver) == 0);
}

static void CheckIpv6UdpTimestamps() {
    const int receiver = socket_nid_postfix(AfInet6, SockDgram, 0);
    if (receiver < 0 && *__error_nid_postfix() == ErrorAfNoSupport) {
        return;
    }
    Require(receiver >= 0);
    const int sender = socket_nid_postfix(AfInet6, SockDgram, 0);
    Require(sender >= 0);

    std::array<unsigned char, 28> destination{};
    destination[0] = 28;
    destination[1] = static_cast<unsigned char>(AfInet6);
    destination[23] = 1;
    const int bound = bind_nid_postfix(receiver, destination.data(), destination.size());
    if (bound < 0 && *__error_nid_postfix() == ErrorAddrNotAvail) {
        Require(close_nid_postfix(sender) == 0 && close_nid_postfix(receiver) == 0);
        return;
    }
    Require(bound == 0);
    std::uint32_t size = destination.size();
    Require(getsockname_nid_postfix(receiver, destination.data(), &size) == 0);
    Require(connect_nid_postfix(sender, destination.data(), destination.size()) == 0);

    int enable = 1;
    Require(setsockopt_nid_postfix(receiver, SolSocket, SoTimestamp, &enable, sizeof(enable)) == 0);
    Require(setsockopt_nid_postfix(receiver, SolSocket, SoBintime, &enable, sizeof(enable)) == 0);

    const char payload[] = "ipv6";
    Require(sendto_nid_postfix(sender, payload, sizeof(payload), 0, nullptr, 0) == sizeof(payload));

    char received[sizeof(payload)]{};
    KernelIovec iov{received, sizeof(received)};
    std::array<unsigned char, 128> controlBuffer;
    controlBuffer.fill(0x33);
    GuestMsghdr msg{nullptr, 0, &iov, 1, controlBuffer.data(), static_cast<std::uint32_t>(controlBuffer.size()), -1};
    Require(recvmsg_nid_postfix(receiver, &msg, 0) == sizeof(payload));
    Require(msg.controlLength == 32 && msg.flags == 0);
    Require(std::all_of(controlBuffer.begin() + 32, controlBuffer.end(), [](auto byte) { return byte == 0x33; }));

    std::uint32_t length = 0;
    int level = 0;
    int type = 0;
    std::memcpy(&length, controlBuffer.data(), sizeof(length));
    std::memcpy(&level, controlBuffer.data() + 4, sizeof(level));
    std::memcpy(&type, controlBuffer.data() + 8, sizeof(type));
    Require(length == 32 && level == SolSocket && type == ScmTimestamp);

    Require(close_nid_postfix(sender) == 0 && close_nid_postfix(receiver) == 0);
}

static void CheckTcpStreamNoTimestamps() {
    const int listener = socket_nid_postfix(AfInet, SockStream, 0);
    const int client = socket_nid_postfix(AfInet, SockStream, 0);
    Require(listener >= 0 && client >= 0);

    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    Require(listen_nid_postfix(listener, 1) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &size) == 0);
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0);

    const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted >= 0);

    int enable = 1;
    Require(setsockopt_nid_postfix(accepted, SolSocket, SoTimestamp, &enable, sizeof(enable)) == 0);
    Require(setsockopt_nid_postfix(accepted, SolSocket, SoBintime, &enable, sizeof(enable)) == 0);

    const char payload[] = "streamdata";
    Require(send_nid_postfix(client, payload, sizeof(payload), 0) == sizeof(payload));

    char received[sizeof(payload)]{};
    KernelIovec iov{received, sizeof(received)};
    std::array<unsigned char, 128> controlBuffer;
    controlBuffer.fill(0x77);
    GuestMsghdr msg{nullptr, 0, &iov, 1, controlBuffer.data(), static_cast<std::uint32_t>(controlBuffer.size()), -1};
    Require(recvmsg_nid_postfix(accepted, &msg, 0) == sizeof(payload));
    Require(msg.controlLength == 0 && msg.flags == 0);
    Require(std::all_of(controlBuffer.begin(), controlBuffer.end(), [](auto byte) { return byte == 0x77; }));

    Require(close_nid_postfix(accepted) == 0);
    Require(close_nid_postfix(client) == 0);
    Require(close_nid_postfix(listener) == 0);
}

static void CheckMsgPeekTimestamps() {
    const int receiver = socket_nid_postfix(AfInet, SockDgram, 0);
    const int sender = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(receiver >= 0 && sender >= 0);

    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(receiver, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(receiver, address.data(), &size) == 0);
    Require(connect_nid_postfix(sender, address.data(), address.size()) == 0);

    int enable = 1;
    Require(setsockopt_nid_postfix(receiver, SolSocket, SoTimestamp, &enable, sizeof(enable)) == 0);

    const char payload[] = "peekdatagram";
    Require(sendto_nid_postfix(sender, payload, sizeof(payload), 0, nullptr, 0) == sizeof(payload));

    char received[sizeof(payload)]{};
    KernelIovec iov{received, sizeof(received)};
    std::array<unsigned char, 64> controlBuffer;
    controlBuffer.fill(0);
    GuestMsghdr peekMsg{nullptr, 0, &iov, 1, controlBuffer.data(), static_cast<std::uint32_t>(controlBuffer.size()), -1};
    Require(recvmsg_nid_postfix(receiver, &peekMsg, MsgPeek) == sizeof(payload));
    Require(peekMsg.controlLength == 32 && peekMsg.flags == MsgPeek);
    Require(std::memcmp(received, payload, sizeof(payload)) == 0);

    std::memset(received, 0, sizeof(received));
    controlBuffer.fill(0);
    GuestMsghdr realMsg{nullptr, 0, &iov, 1, controlBuffer.data(), static_cast<std::uint32_t>(controlBuffer.size()), -1};
    Require(recvmsg_nid_postfix(receiver, &realMsg, 0) == sizeof(payload));
    Require(realMsg.controlLength == 32 && realMsg.flags == 0);
    Require(std::memcmp(received, payload, sizeof(payload)) == 0);

    Require(close_nid_postfix(sender) == 0 && close_nid_postfix(receiver) == 0);
}

int main() {
    CheckOptionPrecedenceAndBounds();
    CheckIpv4UdpTimestampsAndBounds();
    CheckIpv6UdpTimestamps();
    CheckTcpStreamNoTimestamps();
    CheckMsgPeekTimestamps();
    return 0;
}
