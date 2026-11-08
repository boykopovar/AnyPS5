#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
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
constexpr int IpLevel = 0;
constexpr int IpMulticastInterface = 9;
constexpr int ErrorAddressNotAvailable = 49;
constexpr unsigned char GuestAfInet = 2;
constexpr unsigned char GuestAfLink = 18;
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

static std::uint32_t LoopbackInterfaceIndex() {
    GuestInterfaceAddress* list = nullptr;
    Require(getifaddrs_nid_postfix(&list) == 0, "getifaddrs lists the interfaces");
    std::string loopback;
    for (const GuestInterfaceAddress* item = list; item != nullptr && loopback.empty(); item = item->next) {
        const auto* address = static_cast<const unsigned char*>(item->address);
        if (address != nullptr && address[1] == GuestAfInet && address[4] == 127 && address[5] == 0 && address[6] == 0 && address[7] == 1)
            loopback = item->name;
    }
    std::uint32_t index = 0;
    for (const GuestInterfaceAddress* item = list; item != nullptr && index == 0; item = item->next) {
        const auto* address = static_cast<const unsigned char*>(item->address);
        if (address == nullptr || address[1] != GuestAfLink || loopback != item->name) continue;
        std::uint16_t small = 0;
        std::memcpy(&small, address + 2, sizeof small);
        index = small;
    }
    freeifaddrs_nid_postfix(list);
    return index;
}

static void SetsMulticastInterfaceByIndex() {
    const int datagram = socket_nid_postfix(2, 2, 0);
    Require(datagram >= 0, "a socket for IP_MULTICAST_IF");
    const std::uint32_t index = LoopbackInterfaceIndex();
    Require(index != 0, "the loopback interface has an index");
    unsigned char request[12]{};
    const std::int32_t known = static_cast<std::int32_t>(index);
    std::memcpy(request + 8, &known, sizeof known);
    Require(setsockopt_nid_postfix(datagram, IpLevel, IpMulticastInterface, request, sizeof request) == 0,
        "IP_MULTICAST_IF accepts the loopback interface index");
    const std::int32_t unknown = 0x7fffffff;
    std::memcpy(request + 8, &unknown, sizeof unknown);
    Require(setsockopt_nid_postfix(datagram, IpLevel, IpMulticastInterface, request, sizeof request) == -1
            && *__error_nid_postfix() == ErrorAddressNotAvailable, "an interface index that is not on the host is refused");
    close_nid_postfix(datagram);
}

int main() {
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
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

    SetsMulticastInterfaceByIndex();
    close_nid_postfix(guest);
    close_nid_postfix(peer);
    return 0;
}
