#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorNoSpace = 28;
constexpr int ErrorDestAddrReq = 39;
constexpr int ErrorOperationNotSupported = 45;
constexpr int ErrorAddressFamilyNotSupported = 47;
constexpr int ErrorNotConnected = 57;
constexpr int ErrorNameTooLong = 63;

constexpr int FamilyInet = 2;
constexpr int FamilyInet6 = 28;
constexpr int FamilyUnix = 1;
constexpr int SocketStream = 1;
constexpr int SocketDatagram = 2;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

bool Fails(long long result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

std::array<std::uint8_t, 16> MakeV4(std::uint8_t family = 2, std::uint16_t port = 0) {
    std::array<std::uint8_t, 16> address{};
    address[0] = 16;
    address[1] = family;
    address[2] = static_cast<std::uint8_t>(port >> 8);
    address[3] = static_cast<std::uint8_t>(port);
    address[4] = 127;
    address[7] = 1;
    return address;
}

std::array<std::uint8_t, 28> MakeV6(std::uint8_t family = 28, std::uint16_t port = 0) {
    std::array<std::uint8_t, 28> address{};
    address[0] = 28;
    address[1] = family;
    address[2] = static_cast<std::uint8_t>(port >> 8);
    address[3] = static_cast<std::uint8_t>(port);
    address[23] = 1;
    return address;
}

void TestSocketAddressBounds() {
    const int stream4 = socket_nid_postfix(FamilyInet, SocketStream, 0);
    const int dgram4 = socket_nid_postfix(FamilyInet, SocketDatagram, 0);
    const int stream6 = socket_nid_postfix(FamilyInet6, SocketStream, 0);
    const int dgram6 = socket_nid_postfix(FamilyInet6, SocketDatagram, 0);
    Require(stream4 >= 0 && dgram4 >= 0 && stream6 >= 0 && dgram6 >= 0);

    const auto v4 = MakeV4();
    const auto v6 = MakeV6();

    Require(Fails(bind_nid_postfix(stream4, v4.data(), 0), ErrorInvalid));
    Require(Fails(bind_nid_postfix(stream4, v4.data(), 1), ErrorInvalid));
    Require(Fails(bind_nid_postfix(stream4, v4.data(), 2), ErrorInvalid));
    Require(Fails(bind_nid_postfix(stream4, v4.data(), 15), ErrorInvalid));
    Require(Fails(bind_nid_postfix(stream4, v4.data(), 17), ErrorInvalid));
    Require(Fails(bind_nid_postfix(stream4, v4.data(), 28), ErrorInvalid));
    Require(Fails(bind_nid_postfix(stream4, v4.data(), 255), ErrorInvalid));
    Require(Fails(bind_nid_postfix(stream4, v4.data(), 256), ErrorNameTooLong));
    Require(Fails(bind_nid_postfix(stream4, v4.data(), 500), ErrorNameTooLong));

    Require(Fails(connect_nid_postfix(stream4, v4.data(), 0), ErrorInvalid));
    Require(Fails(connect_nid_postfix(stream4, v4.data(), 1), ErrorInvalid));
    Require(Fails(connect_nid_postfix(stream4, v4.data(), 15), ErrorInvalid));
    Require(Fails(connect_nid_postfix(stream4, v4.data(), 17), ErrorInvalid));
    Require(Fails(connect_nid_postfix(stream4, v4.data(), 28), ErrorInvalid));
    Require(Fails(connect_nid_postfix(stream4, v4.data(), 256), ErrorNameTooLong));

    Require(Fails(bind_nid_postfix(stream6, v6.data(), 0), ErrorInvalid));
    Require(Fails(bind_nid_postfix(stream6, v6.data(), 1), ErrorInvalid));
    Require(Fails(bind_nid_postfix(stream6, v6.data(), 16), ErrorInvalid));
    Require(Fails(bind_nid_postfix(stream6, v6.data(), 20), ErrorInvalid));
    Require(Fails(bind_nid_postfix(stream6, v6.data(), 27), ErrorInvalid));
    Require(Fails(bind_nid_postfix(stream6, v6.data(), 29), ErrorInvalid));
    Require(Fails(bind_nid_postfix(stream6, v6.data(), 255), ErrorInvalid));
    Require(Fails(bind_nid_postfix(stream6, v6.data(), 256), ErrorNameTooLong));

    Require(Fails(connect_nid_postfix(stream6, v6.data(), 0), ErrorInvalid));
    Require(Fails(connect_nid_postfix(stream6, v6.data(), 16), ErrorInvalid));
    Require(Fails(connect_nid_postfix(stream6, v6.data(), 27), ErrorInvalid));
    Require(Fails(connect_nid_postfix(stream6, v6.data(), 29), ErrorInvalid));
    Require(Fails(connect_nid_postfix(stream6, v6.data(), 256), ErrorNameTooLong));

    Require(Fails(sendto_nid_postfix(dgram4, "x", 1, 0, v4.data(), 0), ErrorInvalid));
    Require(Fails(sendto_nid_postfix(dgram4, "x", 1, 0, v4.data(), 1), ErrorInvalid));
    Require(Fails(sendto_nid_postfix(dgram4, "x", 1, 0, v4.data(), 15), ErrorInvalid));
    Require(Fails(sendto_nid_postfix(dgram4, "x", 1, 0, v4.data(), 256), ErrorNameTooLong));

    Require(Fails(sendto_nid_postfix(dgram6, "x", 1, 0, v6.data(), 0), ErrorInvalid));
    Require(Fails(sendto_nid_postfix(dgram6, "x", 1, 0, v6.data(), 16), ErrorInvalid));
    Require(Fails(sendto_nid_postfix(dgram6, "x", 1, 0, v6.data(), 27), ErrorInvalid));
    Require(Fails(sendto_nid_postfix(dgram6, "x", 1, 0, v6.data(), 256), ErrorNameTooLong));

    Require(Fails(sendto_nid_postfix(dgram4, "x", 1, 0, nullptr, 0), ErrorDestAddrReq));
    Require(Fails(sendto_nid_postfix(stream4, "x", 1, 0, nullptr, 0), ErrorNotConnected));

    std::uint32_t len0 = 0;
    std::array<std::uint8_t, 16> outAddr{};
    Require(getsockname_nid_postfix(stream4, outAddr.data(), &len0) == 0 && len0 == 0);
    len0 = 0;
    Require(getsockname_nid_postfix(stream4, nullptr, &len0) == 0 && len0 == 0);

    const int listener = socket_nid_postfix(FamilyInet, SocketStream, 0);
    Require(listener >= 0);
    const auto boundAddr = MakeV4(2, 0);
    Require(bind_nid_postfix(listener, boundAddr.data(), boundAddr.size()) == 0);

    std::uint32_t len4 = 4;
    std::array<std::uint8_t, 16> shortOut{};
    Require(getsockname_nid_postfix(listener, shortOut.data(), &len4) == 0 && len4 == 4);
    Require(shortOut[0] == 16 && shortOut[1] == 2);

    Require(close_nid_postfix(listener) == 0);
    Require(close_nid_postfix(stream4) == 0);
    Require(close_nid_postfix(dgram4) == 0);
    Require(close_nid_postfix(stream6) == 0);
    Require(close_nid_postfix(dgram6) == 0);
}

void TestNullBufferRejection() {
    const int stream4 = socket_nid_postfix(FamilyInet, SocketStream, 0);
    const int dgram4 = socket_nid_postfix(FamilyInet, SocketDatagram, 0);
    const int stream6 = socket_nid_postfix(FamilyInet6, SocketStream, 0);
    Require(stream4 >= 0 && dgram4 >= 0 && stream6 >= 0);

    const auto v4 = MakeV4();
    const auto v6 = MakeV6();

    Require(Fails(bind_nid_postfix(stream4, nullptr, 16), ErrorFault));
    Require(Fails(bind_nid_postfix(stream6, nullptr, 28), ErrorFault));
    Require(Fails(connect_nid_postfix(stream4, nullptr, 16), ErrorFault));
    Require(Fails(connect_nid_postfix(stream6, nullptr, 28), ErrorFault));

    Require(Fails(bind_nid_postfix(-1, nullptr, 0), ErrorInvalid));
    Require(Fails(bind_nid_postfix(-1, nullptr, 1), ErrorInvalid));
    Require(Fails(bind_nid_postfix(-1, nullptr, 256), ErrorNameTooLong));
    Require(Fails(bind_nid_postfix(-1, nullptr, 16), ErrorFault));
    Require(Fails(bind_nid_postfix(-1, v4.data(), 16), ErrorBadDescriptor));

    Require(Fails(connect_nid_postfix(-1, nullptr, 0), ErrorInvalid));
    Require(Fails(connect_nid_postfix(-1, nullptr, 1), ErrorInvalid));
    Require(Fails(connect_nid_postfix(-1, nullptr, 256), ErrorNameTooLong));
    Require(Fails(connect_nid_postfix(-1, nullptr, 16), ErrorFault));
    Require(Fails(connect_nid_postfix(-1, v4.data(), 16), ErrorBadDescriptor));

    Require(Fails(send_nid_postfix(-1, nullptr, 10, 0), ErrorBadDescriptor));
    Require(Fails(recv_nid_postfix(-1, nullptr, 10, 0), ErrorBadDescriptor));
    Require(Fails(send_nid_postfix(stream4, nullptr, 10, 0), ErrorNotConnected));
    Require(Fails(recv_nid_postfix(stream4, nullptr, 10, 0), ErrorFault));

    Require(Fails(sendto_nid_postfix(dgram4, nullptr, 10, 0, v4.data(), 16), ErrorFault));

    Require(Fails(recvfrom_nid_postfix(dgram4, nullptr, 10, 0, nullptr, nullptr), ErrorFault));
    std::array<std::uint8_t, 16> rcvAddr{};
    char buf[16]{};
    Require(Fails(recvfrom_nid_postfix(dgram4, buf, sizeof(buf), 0, rcvAddr.data(), nullptr), ErrorInvalid));
    Require(Fails(recvfrom_nid_postfix(-1, nullptr, 10, 0, nullptr, nullptr), ErrorBadDescriptor));

    std::uint32_t validLen = 16;
    Require(Fails(getsockname_nid_postfix(stream4, rcvAddr.data(), nullptr), ErrorFault));
    Require(Fails(getsockname_nid_postfix(stream4, nullptr, &validLen), ErrorFault));
    Require(Fails(getsockname_nid_postfix(-1, rcvAddr.data(), &validLen), ErrorBadDescriptor));

    Require(Fails(getpeername_nid_postfix(stream4, rcvAddr.data(), nullptr), ErrorFault));
    Require(Fails(getpeername_nid_postfix(stream4, rcvAddr.data(), &validLen), ErrorNotConnected));
    Require(Fails(getpeername_nid_postfix(-1, rcvAddr.data(), &validLen), ErrorBadDescriptor));

    Require(Fails(accept_nid_postfix(stream4, rcvAddr.data(), nullptr), ErrorFault));
    Require(Fails(accept_nid_postfix(-1, nullptr, nullptr), ErrorBadDescriptor));

    Require(Fails(socketpair_nid_postfix(FamilyUnix, SocketStream, 0, nullptr), ErrorFault));

    Require(Fails(sendmsg_nid_postfix(stream4, nullptr, 0), ErrorFault));
    Require(Fails(recvmsg_nid_postfix(stream4, nullptr, 0), ErrorFault));
    GuestMsghdr msg{};
    msg.iov = nullptr;
    msg.iovLength = 1;
    Require(Fails(sendmsg_nid_postfix(stream4, &msg, 0), ErrorFault));
    Require(Fails(recvmsg_nid_postfix(stream4, &msg, 0), ErrorFault));

    KernelIovec iov{};
    iov.base = nullptr;
    iov.length = 10;
    msg.iov = &iov;
    msg.iovLength = 1;
    Require(Fails(sendmsg_nid_postfix(stream4, &msg, 0), ErrorFault));

    Require(close_nid_postfix(stream4) == 0);
    Require(close_nid_postfix(dgram4) == 0);
    Require(close_nid_postfix(stream6) == 0);
}

void TestFamilyValidation() {
    for (const int unsupported : {-1, 0, 1, 3, 4, 16, 27, 29, 42, 99}) {
        Require(Fails(socket_nid_postfix(unsupported, SocketStream, 0), ErrorAddressFamilyNotSupported));
        Require(Fails(socket_nid_postfix(unsupported, SocketDatagram, 0), ErrorAddressFamilyNotSupported));
    }

    int sv[2]{-1, -1};
    Require(Fails(socketpair_nid_postfix(0, SocketStream, 0, sv), ErrorAddressFamilyNotSupported));
    Require(Fails(socketpair_nid_postfix(99, SocketStream, 0, sv), ErrorAddressFamilyNotSupported));
    Require(Fails(socketpair_nid_postfix(FamilyInet, SocketStream, 0, sv), ErrorOperationNotSupported));
    Require(Fails(socketpair_nid_postfix(FamilyInet6, SocketStream, 0, sv), ErrorOperationNotSupported));
    Require(socketpair_nid_postfix(FamilyUnix, SocketStream, 0, sv) == 0);
    Require(sv[0] >= 0 && sv[1] >= 0);
    Require(close_nid_postfix(sv[0]) == 0 && close_nid_postfix(sv[1]) == 0);

    const int stream4 = socket_nid_postfix(FamilyInet, SocketStream, 0);
    const int stream6 = socket_nid_postfix(FamilyInet6, SocketStream, 0);
    const int dgram4 = socket_nid_postfix(FamilyInet, SocketDatagram, 0);
    Require(stream4 >= 0 && stream6 >= 0 && dgram4 >= 0);

    for (const std::uint8_t wrongFamily : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{28}, std::uint8_t{99}}) {
        const auto addr = MakeV4(wrongFamily);
        Require(Fails(bind_nid_postfix(stream4, addr.data(), 16), ErrorAddressFamilyNotSupported));
        Require(Fails(connect_nid_postfix(stream4, addr.data(), 16), ErrorAddressFamilyNotSupported));
        Require(Fails(sendto_nid_postfix(dgram4, "x", 1, 0, addr.data(), 16), ErrorAddressFamilyNotSupported));
    }

    for (const std::uint8_t wrongFamily : {std::uint8_t{0}, std::uint8_t{2}, std::uint8_t{99}}) {
        const auto addr6 = MakeV6(wrongFamily);
        Require(Fails(bind_nid_postfix(stream6, addr6.data(), 28), ErrorAddressFamilyNotSupported));
        Require(Fails(connect_nid_postfix(stream6, addr6.data(), 28), ErrorAddressFamilyNotSupported));
    }

    const auto v6ForV4 = MakeV6(28);
    Require(Fails(bind_nid_postfix(stream4, v6ForV4.data(), 28), ErrorInvalid));
    Require(Fails(connect_nid_postfix(stream4, v6ForV4.data(), 28), ErrorInvalid));

    const auto v4ForV6 = MakeV4(2);
    Require(Fails(bind_nid_postfix(stream6, v4ForV6.data(), 16), ErrorInvalid));
    Require(Fails(connect_nid_postfix(stream6, v4ForV6.data(), 16), ErrorInvalid));

    const int udpDisconnect = socket_nid_postfix(FamilyInet, SocketDatagram, 0);
    Require(udpDisconnect >= 0);
    const auto target = MakeV4(2, 9876);
    Require(connect_nid_postfix(udpDisconnect, target.data(), target.size()) == 0);
    std::array<std::uint8_t, 16> peerAddr{};
    std::uint32_t peerLen = peerAddr.size();
    Require(getpeername_nid_postfix(udpDisconnect, peerAddr.data(), &peerLen) == 0 && peerLen == 16);

    const auto disconnectAddr = MakeV4(0, 0);
    Require(Fails(connect_nid_postfix(udpDisconnect, disconnectAddr.data(), disconnectAddr.size()), ErrorAddressFamilyNotSupported));
    Require(close_nid_postfix(udpDisconnect) == 0);

    std::uint8_t dst[16]{};
    for (const int badFamily : {-1, 0, 1, 3, 27, 29, 99}) {
        Require(Fails(__inet_pton_nid_postfix(badFamily, "127.0.0.1", dst), ErrorAddressFamilyNotSupported));
    }
    Require(__inet_pton_nid_postfix(FamilyInet, "127.0.0.1", dst) == 1);
    Require(dst[0] == 127 && dst[1] == 0 && dst[2] == 0 && dst[3] == 1);
    Require(__inet_pton_nid_postfix(FamilyInet6, "::1", dst) == 1);
    Require(dst[15] == 1);
    Require(__inet_pton_nid_postfix(FamilyInet, "999.999.999.999", dst) == 0);
    Require(__inet_pton_nid_postfix(FamilyInet6, "invalid:::v6", dst) == 0);

    char text[64]{};
    for (const int badFamily : {-1, 0, 1, 3, 27, 29, 99}) {
        *__error_nid_postfix() = 0;
        Require(__inet_ntop_nid_postfix(badFamily, dst, text, sizeof(text)) == nullptr && *__error_nid_postfix() == ErrorAddressFamilyNotSupported);
    }
    const std::uint8_t v4Bytes[4] = {127, 0, 0, 1};
    *__error_nid_postfix() = 0;
    Require(__inet_ntop_nid_postfix(FamilyInet, v4Bytes, text, 3) == nullptr && *__error_nid_postfix() == ErrorNoSpace);
    Require(__inet_ntop_nid_postfix(FamilyInet, v4Bytes, text, sizeof(text)) == text && std::strcmp(text, "127.0.0.1") == 0);

    const std::uint8_t v6Bytes[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    *__error_nid_postfix() = 0;
    Require(__inet_ntop_nid_postfix(FamilyInet6, v6Bytes, text, 2) == nullptr && *__error_nid_postfix() == ErrorNoSpace);
    Require(__inet_ntop_nid_postfix(FamilyInet6, v6Bytes, text, sizeof(text)) == text && std::strcmp(text, "::1") == 0);

    Require(close_nid_postfix(stream4) == 0);
    Require(close_nid_postfix(stream6) == 0);
    Require(close_nid_postfix(dgram4) == 0);
}

} // namespace

int main() {
    TestSocketAddressBounds();
    TestNullBufferRejection();
    TestFamilyValidation();
    return 0;
}
