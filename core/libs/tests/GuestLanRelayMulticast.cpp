#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s (errno %d)\n", message, *__error_nid_postfix());
        std::exit(1);
    }
}

using Address = std::array<unsigned char, 16>;

constexpr short PollIn = 1;
constexpr std::uint16_t PeerPort = 47422;
constexpr std::uint16_t SourcePort = 47423;
constexpr std::uint16_t GroupPort = 47424;
constexpr std::uint8_t FrameVersion = 1;
constexpr std::size_t FrameHeaderSize = 11;

static Address Endpoint(unsigned char a, unsigned char b, unsigned char c, unsigned char d, std::uint16_t port) {
    Address address{};
    address[0] = 16;
    address[1] = 2;
    address[2] = static_cast<unsigned char>(port >> 8);
    address[3] = static_cast<unsigned char>(port);
    address[4] = a;
    address[5] = b;
    address[6] = c;
    address[7] = d;
    return address;
}

static bool Readable(int descriptor, int timeout) {
    GuestPollDescriptor entry{descriptor, PollIn, 0};
    return poll_nid_postfix(&entry, 1, timeout) == 1 && (entry.revents & PollIn) != 0;
}

static std::uint16_t ReadU16(const unsigned char* at) {
    return static_cast<std::uint16_t>((at[0] << 8) | at[1]);
}

int main() {
    const int peer = socket_nid_postfix(2, 2, 0);
    const Address peerAddress = Endpoint(127, 0, 0, 1, PeerPort);
    Require(peer >= 0 && bind_nid_postfix(peer, peerAddress.data(), peerAddress.size()) == 0, "the LAN peer binds its port");

    const int guest = socket_nid_postfix(2, 2, 0);
    const Address guestAddress = Endpoint(0, 0, 0, 0, SourcePort);
    Require(guest >= 0 && bind_nid_postfix(guest, guestAddress.data(), guestAddress.size()) == 0, "the guest binds its source port");

    const Address group = Endpoint(239, 1, 2, 3, GroupPort);
    const unsigned char payload[2] = {0x44, 0x55};
    sendto_nid_postfix(guest, payload, sizeof payload, 0, group.data(), group.size());
    Require(Readable(peer, 2000), "the LAN peer receives the multicast send without SO_BROADCAST");
    unsigned char datagram[64]{};
    Address from{};
    std::uint32_t fromLength = from.size();
    Require(recvfrom_nid_postfix(peer, datagram, sizeof datagram, 0, from.data(), &fromLength) == static_cast<std::int64_t>(FrameHeaderSize + sizeof payload),
        "the frame is the header and the payload");
    Require(datagram[0] == FrameVersion, "the frame carries the frame version");
    Require(ReadU16(datagram + 5) == SourcePort, "the frame carries the guest source port");
    Require(ReadU16(datagram + 7) == GroupPort, "the frame carries the group port");
    Require(ReadU16(datagram + 9) == sizeof payload, "the frame carries the payload length");
    Require(std::memcmp(datagram + FrameHeaderSize, payload, sizeof payload) == 0, "the frame carries the payload");

    close_nid_postfix(guest);
    close_nid_postfix(peer);
    return 0;
}
