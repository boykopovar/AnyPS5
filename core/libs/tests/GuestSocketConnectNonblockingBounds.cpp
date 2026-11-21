#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int GuestAfInet = 2;
constexpr int GuestAfInet6 = 28;
constexpr int GuestSockStream = 1;
constexpr int GuestSockDgram = 2;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorInProgress = 36;
constexpr int ErrorAlready = 37;
constexpr int ErrorNotSocket = 38;
constexpr int ErrorNotSupported = 45;
constexpr int ErrorAddressFamilyNotSupported = 47;
constexpr int ErrorIsConnected = 56;
constexpr int ErrorConnectionRefused = 61;
constexpr int ErrorNameTooLong = 63;

constexpr int FcntlGetFlags = 3;
constexpr int FcntlSetFlags = 4;
constexpr int OpenNonBlocking = 4;

constexpr short PollIn = 0x0001;
constexpr short PollOut = 0x0004;

constexpr int SocketLevel = 0xffff;
constexpr int SocketOptionError = 0x1007;

void RequireImpl(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "assertion failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(condition) RequireImpl((condition), __LINE__)

std::array<std::uint8_t, 16> ClosedLoopbackPort() {
    std::array<std::uint8_t, 16> address{16, GuestAfInet, 0, 0, 127, 0, 0, 1};
    const int descriptor = socket_nid_postfix(GuestAfInet, GuestSockStream, 0);
    Require(descriptor >= 0);
    Require(bind_nid_postfix(descriptor, address.data(), address.size()) == 0);
    std::uint32_t length = address.size();
    Require(getsockname_nid_postfix(descriptor, address.data(), &length) == 0);
    Require(close_nid_postfix(descriptor) == 0);
    return address;
}

} // namespace

int main() {
    std::array<std::uint8_t, 16> ipv4Address{16, GuestAfInet, 0, 0, 127, 0, 0, 1};
    std::array<std::uint8_t, 28> ipv6Address{28, GuestAfInet6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

    const int streamSocket = socket_nid_postfix(GuestAfInet, GuestSockStream, 0);
    Require(streamSocket >= 0);

    int pipeDescriptors[2]{-1, -1};
    Require(pipe_nid_postfix(pipeDescriptors) == 0);

    const int closedSocket = socket_nid_postfix(GuestAfInet, GuestSockStream, 0);
    Require(closedSocket >= 0);
    Require(close_nid_postfix(closedSocket) == 0);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(-1, ipv4Address.data(), 256) == -1);
    Require(*__error_nid_postfix() == ErrorNameTooLong);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(9999, ipv4Address.data(), 1024) == -1);
    Require(*__error_nid_postfix() == ErrorNameTooLong);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(-1, nullptr, 256) == -1);
    Require(*__error_nid_postfix() == ErrorNameTooLong);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(streamSocket, ipv4Address.data(), 256) == -1);
    Require(*__error_nid_postfix() == ErrorNameTooLong);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(pipeDescriptors[0], ipv4Address.data(), 300) == -1);
    Require(*__error_nid_postfix() == ErrorNameTooLong);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(-1, ipv4Address.data(), 0) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(-1, ipv4Address.data(), 1) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(-1, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(-1, nullptr, 1) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(streamSocket, ipv4Address.data(), 0) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(streamSocket, ipv4Address.data(), 1) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(pipeDescriptors[0], ipv4Address.data(), 0) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(pipeDescriptors[0], ipv4Address.data(), 1) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(-1, nullptr, 16) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(-1, nullptr, 28) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(-1, nullptr, 2) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(9999, nullptr, 16) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(closedSocket, nullptr, 16) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(pipeDescriptors[0], nullptr, 16) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(streamSocket, nullptr, 16) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(-1, ipv4Address.data(), ipv4Address.size()) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(-100, ipv4Address.data(), ipv4Address.size()) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(9999, ipv4Address.data(), ipv4Address.size()) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(closedSocket, ipv4Address.data(), ipv4Address.size()) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(pipeDescriptors[0], ipv4Address.data(), ipv4Address.size()) == -1);
    Require(*__error_nid_postfix() == ErrorNotSocket);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(pipeDescriptors[1], ipv4Address.data(), ipv4Address.size()) == -1);
    Require(*__error_nid_postfix() == ErrorNotSocket);

    Require(close_nid_postfix(pipeDescriptors[0]) == 0);
    Require(close_nid_postfix(pipeDescriptors[1]) == 0);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(streamSocket, ipv4Address.data(), 15) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(streamSocket, ipv4Address.data(), 17) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(streamSocket, ipv4Address.data(), 28) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    std::array<std::uint8_t, 16> wrongFamily4{16, 28, 0, 0, 127, 0, 0, 1};
    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(streamSocket, wrongFamily4.data(), wrongFamily4.size()) == -1);
    Require(*__error_nid_postfix() == ErrorAddressFamilyNotSupported);

    const int stream6Socket = socket_nid_postfix(GuestAfInet6, GuestSockStream, 0);
    Require(stream6Socket >= 0);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(stream6Socket, ipv6Address.data(), 16) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(stream6Socket, ipv6Address.data(), 27) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(stream6Socket, ipv6Address.data(), 29) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    std::array<std::uint8_t, 28> wrongFamily6{28, 2};
    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(stream6Socket, wrongFamily6.data(), wrongFamily6.size()) == -1);
    Require(*__error_nid_postfix() == ErrorAddressFamilyNotSupported);

    Require(close_nid_postfix(stream6Socket) == 0);
    Require(close_nid_postfix(streamSocket) == 0);

    const int listener = socket_nid_postfix(GuestAfInet, GuestSockStream, 0);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> listenerAddress{16, GuestAfInet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, listenerAddress.data(), listenerAddress.size()) == 0);
    std::uint32_t listenerLength = listenerAddress.size();
    Require(getsockname_nid_postfix(listener, listenerAddress.data(), &listenerLength) == 0);
    Require(listen_nid_postfix(listener, 8) == 0);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(listener, listenerAddress.data(), 256) == -1);
    Require(*__error_nid_postfix() == ErrorNameTooLong);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(listener, listenerAddress.data(), 1) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(listener, nullptr, listenerAddress.size()) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(listener, listenerAddress.data(), listenerAddress.size()) == -1);
    Require(*__error_nid_postfix() == ErrorNotSupported);

    const int client = socket_nid_postfix(GuestAfInet, GuestSockStream, 0);
    Require(client >= 0);

    const int initialFlags = fcntl_nid_postfix(client, FcntlGetFlags);
    Require(initialFlags >= 0 && (initialFlags & OpenNonBlocking) == 0);

    Require(fcntl_nid_postfix(client, FcntlSetFlags, OpenNonBlocking) == 0);
    const int nonblockingFlags = fcntl_nid_postfix(client, FcntlGetFlags);
    Require((nonblockingFlags & OpenNonBlocking) != 0);

    *__error_nid_postfix() = 0;
    int connectResult = connect_nid_postfix(client, listenerAddress.data(), listenerAddress.size());
    if (connectResult == -1) {
        Require(*__error_nid_postfix() == ErrorInProgress);
        *__error_nid_postfix() = 0;
        int secondConnect = connect_nid_postfix(client, listenerAddress.data(), listenerAddress.size());
        if (secondConnect == -1) {
            Require(*__error_nid_postfix() == ErrorAlready || *__error_nid_postfix() == ErrorIsConnected);
        }
    }

    GuestPollDescriptor pollFd{client, PollOut, 0};
    int pollCount = poll_nid_postfix(&pollFd, 1, 5000);
    Require(pollCount > 0);
    Require((pollFd.revents & PollOut) != 0);

    int socketError = 0;
    std::uint32_t optLen = sizeof(socketError);
    Require(getsockopt_nid_postfix(client, SocketLevel, SocketOptionError, &socketError, &optLen) == 0);
    Require(socketError == 0);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(client, listenerAddress.data(), listenerAddress.size()) == -1);
    Require(*__error_nid_postfix() == ErrorIsConnected || *__error_nid_postfix() == ErrorAlready);

    const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted >= 0);

    const char sendByte = '\x5a';
    Require(write_nid_postfix(client, &sendByte, 1) == 1);
    char recvByte = 0;
    Require(read_nid_postfix(accepted, &recvByte, 1) == 1);
    Require(recvByte == sendByte);

    Require(close_nid_postfix(client) == 0);
    Require(close_nid_postfix(accepted) == 0);

    const auto closedAddress = ClosedLoopbackPort();
    const int closedPortClient = socket_nid_postfix(GuestAfInet, GuestSockStream, 0);
    Require(closedPortClient >= 0);
    Require(fcntl_nid_postfix(closedPortClient, FcntlSetFlags, OpenNonBlocking) == 0);

    *__error_nid_postfix() = 0;
    int closedConnectResult = connect_nid_postfix(closedPortClient, closedAddress.data(), closedAddress.size());
    if (closedConnectResult == -1) {
        const int firstError = *__error_nid_postfix();
        Require(firstError == ErrorInProgress || firstError == ErrorConnectionRefused);
        if (firstError == ErrorInProgress) {
            GuestPollDescriptor closedPollFd{closedPortClient, static_cast<short>(PollIn | PollOut), 0};
            int pollRet = poll_nid_postfix(&closedPollFd, 1, 2000);
            Require(pollRet > 0);
            Require((closedPollFd.revents & (PollIn | PollOut)) != 0);
            int pending = 0;
            std::uint32_t pendingLen = sizeof(pending);
            getsockopt_nid_postfix(closedPortClient, SocketLevel, SocketOptionError, &pending, &pendingLen);
            *__error_nid_postfix() = 0;
            int retryResult = connect_nid_postfix(closedPortClient, closedAddress.data(), closedAddress.size());
            Require(pending == ErrorConnectionRefused);
            if (retryResult == -1) {
                const int retryError = *__error_nid_postfix();
                Require(retryError == ErrorConnectionRefused || retryError == ErrorInProgress || retryError == ErrorAlready || retryError == ErrorInvalid);
            }
        }
    }
    Require(close_nid_postfix(closedPortClient) == 0);

    const int udpSocket = socket_nid_postfix(GuestAfInet, GuestSockDgram, 0);
    Require(udpSocket >= 0);
    Require(fcntl_nid_postfix(udpSocket, FcntlSetFlags, OpenNonBlocking) == 0);

    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(udpSocket, listenerAddress.data(), listenerAddress.size()) == 0);

    std::array<std::uint8_t, 16> peerAddress{};
    std::uint32_t peerLength = peerAddress.size();
    Require(getpeername_nid_postfix(udpSocket, peerAddress.data(), &peerLength) == 0);
    Require(peerLength == listenerAddress.size());


    Require(close_nid_postfix(udpSocket) == 0);

    *__error_nid_postfix() = 0;
    Require(_connect_nid_postfix(-1, ipv4Address.data(), 256) == -1);
    Require(*__error_nid_postfix() == ErrorNameTooLong);

    *__error_nid_postfix() = 0;
    Require(_connect_nid_postfix(-1, ipv4Address.data(), 1) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(_connect_nid_postfix(-1, nullptr, 16) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(_connect_nid_postfix(-1, ipv4Address.data(), ipv4Address.size()) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    const int client2 = socket_nid_postfix(GuestAfInet, GuestSockStream, 0);
    Require(client2 >= 0);
    Require(fcntl_nid_postfix(client2, FcntlSetFlags, OpenNonBlocking) == 0);

    *__error_nid_postfix() = 0;
    int underscoreConnectResult = _connect_nid_postfix(client2, listenerAddress.data(), listenerAddress.size());
    if (underscoreConnectResult == -1) {
        Require(*__error_nid_postfix() == ErrorInProgress);
    }

    GuestPollDescriptor pollFd2{client2, PollOut, 0};
    Require(poll_nid_postfix(&pollFd2, 1, 5000) > 0);
    Require((pollFd2.revents & PollOut) != 0);

    *__error_nid_postfix() = 0;
    Require(_connect_nid_postfix(client2, listenerAddress.data(), listenerAddress.size()) == -1);
    Require(*__error_nid_postfix() == ErrorIsConnected || *__error_nid_postfix() == ErrorAlready);

    const int accepted2 = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted2 >= 0);
    Require(close_nid_postfix(client2) == 0);
    Require(close_nid_postfix(accepted2) == 0);

    Require(close_nid_postfix(listener) == 0);
    return 0;
}
