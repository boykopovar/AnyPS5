#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI listen_nid_postfix(int, int);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI setsockopt_nid_postfix(int, int, int, const void*, std::uint32_t);
std::int64_t APS5_VABI sendto_nid_postfix(int, const void*, std::uint64_t, int, const void*, std::uint32_t);
int APS5_VABI close_nid_postfix(int);
int* APS5_VABI __error_nid_postfix();
}

namespace {

constexpr int Stream = 1;
constexpr int Datagram = 2;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorNameTooLong = 63;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

bool Fails(long result, int error) { return result == -1 && *__error_nid_postfix() == error; }

std::array<std::uint8_t, 16> Loopback() {
    std::array<std::uint8_t, 16> address{16, 2};
    address[4] = 127;
    address[7] = 1;
    return address;
}

void ShortAddressesAreInvalid() {
    const int stream = socket_nid_postfix(2, Stream, 0);
    const int datagram = socket_nid_postfix(2, Datagram, 0);
    Require(stream >= 0 && datagram >= 0, "socket failed");
    const auto address = Loopback();
    Require(Fails(connect_nid_postfix(stream, address.data(), 1), ErrorInvalid), "connect with a length below the size of sa_family must fail with EINVAL (22)");
    Require(Fails(bind_nid_postfix(stream, address.data(), 0), ErrorInvalid), "bind with length 0 must fail with EINVAL (22)");
    Require(Fails(connect_nid_postfix(stream, address.data(), 3), ErrorInvalid), "connect with an AF_INET address shorter than sockaddr_in must fail with EINVAL (22)");
    Require(Fails(bind_nid_postfix(stream, address.data(), 15), ErrorInvalid), "bind with an AF_INET address shorter than sockaddr_in must fail with EINVAL (22)");
    Require(Fails(sendto_nid_postfix(datagram, "x", 1, 0, address.data(), 8), ErrorInvalid), "sendto with an AF_INET address shorter than sockaddr_in must fail with EINVAL (22)");
    std::array<std::uint8_t, 28> v6{28, 28};
    const int stream6 = socket_nid_postfix(28, Stream, 0);
    Require(stream6 >= 0, "socket failed");
    Require(Fails(connect_nid_postfix(stream6, v6.data(), 20), ErrorInvalid), "connect with an AF_INET6 address shorter than sockaddr_in6 must fail with EINVAL (22)");
    Require(Fails(bind_nid_postfix(stream, address.data(), 256), ErrorNameTooLong), "an address longer than SOCK_MAXADDRLEN (255) must fail with ENAMETOOLONG (63)");
    Require(Fails(bind_nid_postfix(stream, nullptr, address.size()), ErrorFault), "a null address must fail with EFAULT (14)");
    Require(close_nid_postfix(stream) == 0 && close_nid_postfix(datagram) == 0 && close_nid_postfix(stream6) == 0, "close failed");
}

void ArgumentsAreCheckedBeforeTheDescriptor() {
    const auto address = Loopback();
    constexpr int ErrorBadDescriptor = 9;
    Require(Fails(bind_nid_postfix(-1, address.data(), 1), ErrorInvalid), "bind(-1) with a 1-byte address must fail with EINVAL (22) before the descriptor is looked up");
    Require(Fails(bind_nid_postfix(-1, address.data(), 256), ErrorNameTooLong), "bind(-1) with a 256-byte address must fail with ENAMETOOLONG (63)");
    Require(Fails(bind_nid_postfix(-1, nullptr, address.size()), ErrorFault), "bind(-1) with a null address must fail with EFAULT (14)");
    Require(Fails(connect_nid_postfix(-1, address.data(), 1), ErrorInvalid), "connect(-1) with a 1-byte address must fail with EINVAL (22)");
    Require(Fails(sendto_nid_postfix(-1, "x", 1, 0, address.data(), 1), ErrorInvalid), "sendto(-1) with a 1-byte address must fail with EINVAL (22)");
    Require(Fails(bind_nid_postfix(-1, address.data(), address.size()), ErrorBadDescriptor), "bind(-1) with a valid address must fail with EBADF (9)");
    Require(Fails(connect_nid_postfix(-1, address.data(), address.size()), ErrorBadDescriptor), "connect(-1) with a valid address must fail with EBADF (9)");
    Require(Fails(sendto_nid_postfix(-1, "x", 1, 0, address.data(), address.size()), ErrorBadDescriptor), "sendto(-1) with a valid address must fail with EBADF (9)");
}

void LongAddressesAndOtherFamiliesFail() {
    constexpr int ErrorFamily = 47;
    const int datagram = socket_nid_postfix(2, Datagram, 0);
    const int stream6 = socket_nid_postfix(28, Stream, 0);
    const int datagram6 = socket_nid_postfix(28, Datagram, 0);
    Require(datagram >= 0 && stream6 >= 0 && datagram6 >= 0, "socket failed");
    std::array<std::uint8_t, 255> address{16, 2};
    address[4] = 127;
    address[7] = 1;
    for (const std::uint32_t length : {17u, 28u, 255u}) {
        Require(Fails(bind_nid_postfix(datagram, address.data(), length), ErrorInvalid), "bind with an AF_INET address longer than sockaddr_in must fail with EINVAL (22)");
        Require(Fails(connect_nid_postfix(datagram, address.data(), length), ErrorInvalid), "connect with an AF_INET address longer than sockaddr_in must fail with EINVAL (22)");
        Require(Fails(sendto_nid_postfix(datagram, "x", 1, 0, address.data(), length), ErrorInvalid), "sendto with an AF_INET address longer than sockaddr_in must fail with EINVAL (22)");
    }
    std::array<std::uint8_t, 64> v6{28, 28};
    v6[23] = 1;
    Require(Fails(bind_nid_postfix(stream6, v6.data(), 29), ErrorInvalid), "bind with an AF_INET6 address longer than sockaddr_in6 must fail with EINVAL (22)");
    Require(Fails(connect_nid_postfix(stream6, v6.data(), 64), ErrorInvalid), "connect with an AF_INET6 address longer than sockaddr_in6 must fail with EINVAL (22)");
    for (const std::uint8_t family : {std::uint8_t{28}, std::uint8_t{1}, std::uint8_t{0}}) {
        address[1] = family;
        Require(Fails(bind_nid_postfix(datagram, address.data(), 16), ErrorFamily), "bind of another family on an AF_INET socket must fail with EAFNOSUPPORT (47)");
    }
    address[1] = 28;
    Require(Fails(bind_nid_postfix(datagram, address.data(), 28), ErrorFamily), "the family is checked before the length on an AF_INET socket");
    Require(Fails(sendto_nid_postfix(datagram, "x", 1, 0, address.data(), 28), ErrorInvalid), "sendto checks the length before the family");
    Require(Fails(sendto_nid_postfix(datagram, "x", 1, 0, address.data(), 16), ErrorFamily), "sendto of another family fails with EAFNOSUPPORT (47)");
    address[1] = 2;
    Require(Fails(bind_nid_postfix(datagram6, address.data(), 16), ErrorInvalid), "an AF_INET6 socket checks the length before the family");
    Require(Fails(connect_nid_postfix(datagram6, address.data(), 28), ErrorFamily), "an AF_INET address of the AF_INET6 length fails with EAFNOSUPPORT (47)");
    Require(close_nid_postfix(datagram) == 0 && close_nid_postfix(stream6) == 0 && close_nid_postfix(datagram6) == 0, "close failed");
}

void ConnectedStreamSendtoIgnoresTheAddress() {
    const int listener = socket_nid_postfix(2, Stream, 0);
    const int client = socket_nid_postfix(2, Stream, 0);
    Require(listener >= 0 && client >= 0, "socket failed");
    auto address = Loopback();
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0 && listen_nid_postfix(listener, 1) == 0, "listen failed");
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &size) == 0, "getsockname failed");
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0, "connect failed");
    std::array<std::uint8_t, 17> padded{};
    std::memcpy(padded.data(), address.data(), address.size());
    Require(sendto_nid_postfix(client, "x", 1, 0, padded.data(), padded.size()) == 1, "tcp_usr_send ignores the address of a connected stream socket, so sendto must send");
    Require(Fails(sendto_nid_postfix(client, "x", 1, 0, padded.data(), 1), ErrorInvalid), "getsockaddr still checks the length of a connected stream socket's address");
    Require(close_nid_postfix(client) == 0 && close_nid_postfix(listener) == 0, "close failed");
}

void FullAddressesStillWork() {
    const int stream = socket_nid_postfix(2, Stream, 0);
    Require(stream >= 0, "socket failed");
    const auto address = Loopback();
    Require(bind_nid_postfix(stream, address.data(), address.size()) == 0, "bind of a full sockaddr_in failed");
    Require(close_nid_postfix(stream) == 0, "close failed");
}

std::array<std::uint8_t, 255> PaddedLoopback(int family) {
    std::array<std::uint8_t, 255> address{};
    if (family == 2) {
        const auto v4 = Loopback();
        std::memcpy(address.data(), v4.data(), v4.size());
    } else {
        address[0] = 28;
        address[1] = 28;
        address[23] = 1;
    }
    return address;
}

void LongAddressesAreInvalid(int family, int type) {
    const std::uint32_t size = family == 2 ? 16 : 28;
    auto address = PaddedLoopback(family);
    const int descriptor = socket_nid_postfix(family, type, 0);
    const int listener = type == Stream ? socket_nid_postfix(family, Stream, 0) : descriptor;
    Require(descriptor >= 0 && listener >= 0, "socket failed");
    for (std::uint32_t length = size + 1; length <= address.size(); ++length) {
        Require(Fails(bind_nid_postfix(descriptor, address.data(), length), ErrorInvalid), "bind with an address longer than sockaddr_in or sockaddr_in6 must fail with EINVAL (22)");
    }
    Require(bind_nid_postfix(listener, address.data(), size) == 0, "bind with an address of exactly the size of sockaddr_in or sockaddr_in6 failed");
    if (type == Stream) Require(listen_nid_postfix(listener, 1) == 0, "listen failed");
    std::uint32_t named = size;
    Require(getsockname_nid_postfix(listener, address.data(), &named) == 0 && named == size, "getsockname failed");
    for (std::uint32_t length = size + 1; length <= address.size(); ++length) {
        Require(Fails(connect_nid_postfix(descriptor, address.data(), length), ErrorInvalid), "connect with an address longer than sockaddr_in or sockaddr_in6 must fail with EINVAL (22)");
    }
    if (family == 28 && type == Datagram) {
        for (std::uint32_t length = size + 1; length <= address.size(); ++length) {
            Require(Fails(sendto_nid_postfix(descriptor, "x", 1, 0, address.data(), length), ErrorInvalid), "sendto on an AF_INET6 datagram socket with an address longer than sockaddr_in6 must fail with EINVAL (22)");
        }
        Require(sendto_nid_postfix(descriptor, "x", 1, 0, address.data(), size) == 1, "sendto with an address of exactly the size of sockaddr_in6 failed");
    }
    Require(connect_nid_postfix(descriptor, address.data(), size) == 0, "connect with an address of exactly the size of sockaddr_in or sockaddr_in6 failed");
    Require(close_nid_postfix(descriptor) == 0, "close failed");
    if (listener != descriptor) Require(close_nid_postfix(listener) == 0, "close failed");
}

void LongMappedAddressesBindOnlyDualStackDatagramSockets() {
    const int datagram = socket_nid_postfix(28, Datagram, 0);
    Require(datagram >= 0, "socket failed");
    std::array<std::uint8_t, 29> mapped{29, 28};
    mapped[18] = 0xff;
    mapped[19] = 0xff;
    mapped[20] = 127;
    mapped[23] = 1;
    Require(Fails(bind_nid_postfix(datagram, mapped.data(), mapped.size()), ErrorInvalid), "bind of a long IPv4-mapped address with IPV6_V6ONLY set must fail with EINVAL (22)");
    const int off = 0;
    Require(setsockopt_nid_postfix(datagram, 41, 27, &off, sizeof(off)) == 0, "clearing IPV6_V6ONLY failed");
    Require(bind_nid_postfix(datagram, mapped.data(), mapped.size()) == 0, "udp6_bind passes a long IPv4-mapped address to in_pcbbind without IPV6_V6ONLY, so bind must succeed");
    Require(Fails(sendto_nid_postfix(datagram, "x", 1, 0, mapped.data(), mapped.size()), ErrorInvalid), "udp6_send checks the length before the IPv4-mapped case, so sendto must fail with EINVAL (22)");
    mapped[3] = 9;
    Require(connect_nid_postfix(datagram, mapped.data(), mapped.size()) == 0, "udp6_connect passes a long IPv4-mapped address to in_pcbconnect without IPV6_V6ONLY, so connect must succeed");
    Require(close_nid_postfix(datagram) == 0, "close failed");
}

}

int main() {
    ShortAddressesAreInvalid();
    ArgumentsAreCheckedBeforeTheDescriptor();
    LongAddressesAndOtherFamiliesFail();
    ConnectedStreamSendtoIgnoresTheAddress();
    FullAddressesStillWork();
    LongAddressesAreInvalid(2, Stream);
    LongAddressesAreInvalid(2, Datagram);
    LongAddressesAreInvalid(28, Stream);
    LongAddressesAreInvalid(28, Datagram);
    LongMappedAddressesBindOnlyDualStackDatagramSockets();
    return 0;
}
