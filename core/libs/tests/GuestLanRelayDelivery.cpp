#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s (errno %d)\n", message, *__error_nid_postfix());
        std::exit(1);
    }
}

using Address = std::array<unsigned char, 16>;

constexpr int SocketLevel = 0xffff;
constexpr int BroadcastOption = 0x20;
constexpr short PollIn = 1;
constexpr std::uint16_t RelayPort = 47411;
constexpr std::uint16_t PeerPort = 47412;
constexpr std::uint16_t ReceiverPort = 47413;
constexpr std::uint16_t SenderPort = 47414;
constexpr std::uint16_t BroadcastPort = 47415;
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

static std::uint32_t ReadU32(const unsigned char* at) {
    return (static_cast<std::uint32_t>(ReadU16(at)) << 16) | ReadU16(at + 2);
}

static std::vector<unsigned char> Frame(std::uint32_t instance, std::uint16_t source, std::uint16_t destination,
    const std::vector<unsigned char>& payload) {
    std::vector<unsigned char> datagram{FrameVersion};
    for (int shift = 24; shift >= 0; shift -= 8) datagram.push_back(static_cast<unsigned char>(instance >> shift));
    datagram.push_back(static_cast<unsigned char>(source >> 8));
    datagram.push_back(static_cast<unsigned char>(source));
    datagram.push_back(static_cast<unsigned char>(destination >> 8));
    datagram.push_back(static_cast<unsigned char>(destination));
    datagram.push_back(static_cast<unsigned char>(payload.size() >> 8));
    datagram.push_back(static_cast<unsigned char>(payload.size()));
    datagram.insert(datagram.end(), payload.begin(), payload.end());
    return datagram;
}

static int Bound(unsigned char a, unsigned char b, unsigned char c, unsigned char d, std::uint16_t port) {
    const int descriptor = socket_nid_postfix(2, 2, 0);
    const Address address = Endpoint(a, b, c, d, port);
    return descriptor >= 0 && bind_nid_postfix(descriptor, address.data(), address.size()) == 0 ? descriptor : -1;
}

int main() {
    const int peer = Bound(127, 0, 0, 1, PeerPort);
    const int receiver = Bound(0, 0, 0, 0, ReceiverPort);
    const int sender = Bound(0, 0, 0, 0, SenderPort);
    Require(peer >= 0 && receiver >= 0 && sender >= 0, "the peer, the receiver and the sender bind");
    int broadcast = 1;
    Require(setsockopt_nid_postfix(sender, SocketLevel, BroadcastOption, &broadcast, sizeof broadcast) == 0, "the sender sets SO_BROADCAST");

    const unsigned char first[1] = {0x11};
    const Address limited = Endpoint(255, 255, 255, 255, BroadcastPort);
    sendto_nid_postfix(sender, first, sizeof first, 0, limited.data(), limited.size());
    Require(Readable(peer, 2000), "the peer receives the relayed broadcast");
    unsigned char relayed[64]{};
    Address from{};
    std::uint32_t fromLength = from.size();
    Require(recvfrom_nid_postfix(peer, relayed, sizeof relayed, 0, from.data(), &fromLength) == static_cast<std::int64_t>(FrameHeaderSize + 1),
        "the peer receives one frame");
    Require(relayed[0] == FrameVersion, "the relayed frame carries the frame version");
    const std::uint32_t own = ReadU32(relayed + 1);
    Require(own != 0, "the relayed frame carries the relay instance");

    const Address relay = Endpoint(127, 0, 0, 1, RelayPort);
    const auto echoed = Frame(own, PeerPort, ReceiverPort, {0x22});
    sendto_nid_postfix(peer, echoed.data(), echoed.size(), 0, relay.data(), relay.size());
    Require(!Readable(receiver, 300), "a frame that carries this relay's instance is not delivered");
    Require(!Readable(peer, 0), "a frame from the peer is not sent back to the peer");

    const auto other = Frame(own ^ 1u, PeerPort, ReceiverPort, {0x33});
    sendto_nid_postfix(peer, other.data(), other.size(), 0, relay.data(), relay.size());
    Require(Readable(receiver, 2000), "a frame from another instance is delivered to the local receiver");
    unsigned char delivered[16]{};
    Address source{};
    std::uint32_t sourceLength = source.size();
    Require(recvfrom_nid_postfix(receiver, delivered, sizeof delivered, 0, source.data(), &sourceLength) == 1 && delivered[0] == 0x33,
        "the local receiver gets the payload of the frame");
    Require(!Readable(peer, 300), "a delivered frame is not forwarded to the peers");

    close_nid_postfix(sender);
    close_nid_postfix(receiver);
    close_nid_postfix(peer);
    return 0;
}
