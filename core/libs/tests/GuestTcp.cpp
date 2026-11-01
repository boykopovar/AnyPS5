#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

static void Require(bool condition) {
    if (!condition) std::abort();
}

static void StreamReceiveReportsNoSender() {
    const int listener = socket_nid_postfix(2, 1, 0);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0 && listen_nid_postfix(listener, 4) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &size) == 0);
    const int client = socket_nid_postfix(2, 1, 0);
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0);
    const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted >= 0);
    const char text[] = "stream";
    Require(send_nid_postfix(client, text, sizeof(text), 0) == sizeof(text));
    std::array<std::uint8_t, 28> sender;
    sender.fill(0xaa);
    char received[sizeof(text)]{};
    std::uint32_t senderSize = sender.size();
    Require(recvfrom_nid_postfix(accepted, received, sizeof(received), 0, sender.data(), &senderSize) == sizeof(text));
    Require(std::strcmp(text, received) == 0);
    Require(senderSize == 0 && sender[0] == 0xaa && sender[1] == 0xaa);
    for (const int descriptor : {client, accepted, listener}) Require(close_nid_postfix(descriptor) == 0);
}

static bool HeldByAnotherInet6Socket(const std::array<std::uint8_t, 28>& any6) {
    const int probe = socket_nid_postfix(28, 1, 0);
    Require(probe >= 0);
    const bool held = bind_nid_postfix(probe, any6.data(), any6.size()) != 0 && *__error_nid_postfix() == 48;
    Require(close_nid_postfix(probe) == 0);
    return held;
}

static void ListenersShareAPortAcrossFamilies() {
    for (int attempt = 0; attempt < 64; ++attempt) {
        const int inet = socket_nid_postfix(2, 1, 0);
        Require(inet >= 0);
        std::array<std::uint8_t, 16> any{16, 2};
        Require(bind_nid_postfix(inet, any.data(), any.size()) == 0 && listen_nid_postfix(inet, 4) == 0);
        std::uint32_t size = any.size();
        Require(getsockname_nid_postfix(inet, any.data(), &size) == 0 && size == any.size());
        const int inet6 = socket_nid_postfix(28, 1, 0);
        if (inet6 < 0) {
            Require(*__error_nid_postfix() == 47);
            Require(close_nid_postfix(inet) == 0);
            return;
        }
        std::array<std::uint8_t, 28> any6{28, 28, any[2], any[3]};
        if (bind_nid_postfix(inet6, any6.data(), any6.size()) == 0) {
            Require(listen_nid_postfix(inet6, 4) == 0);
            Require(close_nid_postfix(inet6) == 0);
            Require(close_nid_postfix(inet) == 0);
            return;
        }
        const int error = *__error_nid_postfix();
        Require(close_nid_postfix(inet6) == 0);
        Require(close_nid_postfix(inet) == 0);
        if (error == 49) return;
        if (error != 48 || !HeldByAnotherInet6Socket(any6)) {
            std::fprintf(stderr, "bind of [::]:%d failed with errno %d while an IPv4 listener holds 0.0.0.0:%d\n",
                (any[2] << 8) | any[3], error, (any[2] << 8) | any[3]);
            std::abort();
        }
    }
    std::fprintf(stderr, "another IPv6 socket held each of 64 ports an IPv4 listener took\n");
    std::abort();
}

constexpr int GetFlags = 3;
constexpr int SetFlags = 4;
constexpr int NonBlockingFlag = 4;

static int AcceptWithin(int listener) {
    for (int attempt = 0; attempt < 200; ++attempt) {
        const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
        if (accepted >= 0) return accepted;
        Require(*__error_nid_postfix() == 35);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return -1;
}

static void AcceptedSocketsFollowListenerBlocking() {
    const int listener = socket_nid_postfix(2, 1, 0);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0 && listen_nid_postfix(listener, 4) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &size) == 0);

    const int firstClient = socket_nid_postfix(2, 1, 0);
    Require(connect_nid_postfix(firstClient, address.data(), address.size()) == 0);
    const int blocking = accept_nid_postfix(listener, nullptr, nullptr);
    Require(blocking >= 0 && (fcntl_nid_postfix(blocking, GetFlags) & NonBlockingFlag) == 0);

    Require(fcntl_nid_postfix(listener, SetFlags, NonBlockingFlag) == 0);
    const int secondClient = socket_nid_postfix(2, 1, 0);
    Require(connect_nid_postfix(secondClient, address.data(), address.size()) == 0);
    const int inherited = AcceptWithin(listener);
    Require(inherited >= 0 && (fcntl_nid_postfix(inherited, GetFlags) & NonBlockingFlag) != 0);
    char byte;
    Require(recv_nid_postfix(inherited, &byte, 1, 0) == -1 && *__error_nid_postfix() == 35);
    Require(fcntl_nid_postfix(inherited, SetFlags, 0) == 0 && (fcntl_nid_postfix(inherited, GetFlags) & NonBlockingFlag) == 0);
    Require((fcntl_nid_postfix(listener, GetFlags) & NonBlockingFlag) != 0);

    for (const int descriptor : {firstClient, secondClient, blocking, inherited, listener}) Require(close_nid_postfix(descriptor) == 0);
}

static void UnboundSocketNameIsZero() {
    for (const int family : {2, 28}) {
        for (const int type : {1, 2}) {
            const int descriptor = socket_nid_postfix(family, type, 0);
            Require(descriptor >= 0);
            const std::uint32_t expected = family == 2 ? 16 : 28;
            std::array<std::uint8_t, 32> name;
            name.fill(0xaa);
            std::uint32_t size = name.size();
            Require(getsockname_nid_postfix(descriptor, name.data(), &size) == 0);
            Require(size == expected && name[0] == expected && name[1] == family);
            for (std::uint32_t index = 2; index < expected; ++index) Require(name[index] == 0);
            Require(name[expected] == 0xaa);
            Require(close_nid_postfix(descriptor) == 0);
        }
    }
}

static void WaitAllReceivesTheWholeRequest() {
    constexpr int WaitAll = 0x40;
    const int listener = socket_nid_postfix(2, 1, 0);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0 && listen_nid_postfix(listener, 4) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &size) == 0);
    const int client = socket_nid_postfix(2, 1, 0);
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0);
    const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted >= 0);

    Require(send_nid_postfix(client, "abcd", 4, 0) == 4);
    std::thread sender([client] {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        Require(send_nid_postfix(client, "efgh", 4, 0) == 4);
    });
    char received[8]{};
    Require(recv_nid_postfix(accepted, received, sizeof(received), WaitAll) == 8);
    sender.join();
    Require(std::memcmp(received, "abcdefgh", 8) == 0);

    Require(send_nid_postfix(client, "xy", 2, 0) == 2 && shutdown_nid_postfix(client, 1) == 0);
    char tail[8]{};
    Require(recvfrom_nid_postfix(accepted, tail, sizeof(tail), WaitAll, nullptr, nullptr) == 2);
    Require(std::memcmp(tail, "xy", 2) == 0);

    const int datagram = socket_nid_postfix(2, 2, 0);
    Require(datagram >= 0);
    std::array<std::uint8_t, 16> local{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(datagram, local.data(), local.size()) == 0);
    size = local.size();
    Require(getsockname_nid_postfix(datagram, local.data(), &size) == 0);
    Require(sendto_nid_postfix(datagram, "dgram", 5, 0, local.data(), local.size()) == 5);
    char record[16]{};
    Require(recv_nid_postfix(datagram, record, sizeof(record), WaitAll) == 5 && std::memcmp(record, "dgram", 5) == 0);
    for (const int descriptor : {datagram, client, accepted, listener}) Require(close_nid_postfix(descriptor) == 0);
}

static void ReceiveAnsweredWithoutTheHost() {
    constexpr int Peek = 0x02;
    constexpr int DontWait = 0x80;
    constexpr std::uint64_t ReadableBytes = 0x4004667f;
    const int listener = socket_nid_postfix(2, 1, 0);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0 && listen_nid_postfix(listener, 4) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &size) == 0);
    const int unconnected = socket_nid_postfix(2, 1, 0);
    char byte;
    Require(recv_nid_postfix(unconnected, &byte, 0, DontWait) == -1 && *__error_nid_postfix() == 57);
    const int client = socket_nid_postfix(2, 1, 0);
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0);
    const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted >= 0);

    Require(recv_nid_postfix(accepted, &byte, 0, DontWait) == 0);
    Require(recv_nid_postfix(accepted, &byte, 0, 0) == 0);
    std::array<std::uint8_t, 16> sender;
    sender.fill(0xaa);
    std::uint32_t senderSize = sender.size();
    Require(recvfrom_nid_postfix(accepted, &byte, 0, 0, sender.data(), &senderSize) == 0 && senderSize == 0);

    char received[4];
    Require(send_nid_postfix(client, "x", 1, 0) == 1);
    Require(recv_nid_postfix(accepted, received, 1, Peek) == 1);
    Require(shutdown_nid_postfix(accepted, 0) == 0);
    int queued = -1;
    Require(ioctl_nid_postfix(accepted, ReadableBytes, &queued) == 0 && queued == 0);
    Require(recv_nid_postfix(accepted, received, sizeof(received), 0) == 0);
    Require(recv_nid_postfix(accepted, received, sizeof(received), DontWait) == 0);
    senderSize = sender.size();
    Require(recvfrom_nid_postfix(accepted, received, sizeof(received), 0, sender.data(), &senderSize) == 0 && senderSize == 0);
    KernelIovec vector{received, sizeof(received)};
    GuestMsghdr message{sender.data(), static_cast<std::uint32_t>(sender.size()), &vector, 1, nullptr, 0, -1};
    Require(recvmsg_nid_postfix(accepted, &message, 0) == 0 && message.nameLength == 0 && message.flags == 0);
    for (const int descriptor : {unconnected, client, accepted, listener}) Require(close_nid_postfix(descriptor) == 0);
}

int main() {
    StreamReceiveReportsNoSender();
    ListenersShareAPortAcrossFamilies();
    AcceptedSocketsFollowListenerBlocking();
    WaitAllReceivesTheWholeRequest();
    ReceiveAnsweredWithoutTheHost();
    UnboundSocketNameIsZero();
    const int listener = socket_nid_postfix(2, 1, 6);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    Require(bind_nid_postfix(listener, address.data(), address.size()) == -1 && *__error_nid_postfix() == 22);
    Require(listen_nid_postfix(listener, 4) == 0);
    std::uint32_t address_size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &address_size) == 0);
    Require(address_size == 16 && (address[2] != 0 || address[3] != 0));

    const int client = socket_nid_postfix(2, 1, 0);
    Require(client >= 0);
    Require(accept_nid_postfix(client, nullptr, nullptr) == -1 && *__error_nid_postfix() == 22);
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0);
    std::array<std::uint8_t, 16> peer{};
    address_size = peer.size();
    const int accepted = accept_nid_postfix(listener, peer.data(), &address_size);
    Require(accepted >= 0 && address_size == 16 && peer[1] == 2);

    std::array<std::uint8_t, 16> connected_peer{};
    address_size = connected_peer.size();
    Require(getpeername_nid_postfix(accepted, connected_peer.data(), &address_size) == 0);
    Require(address_size == 16 && connected_peer[1] == 2);

    for (auto query : {getsockname_nid_postfix, getpeername_nid_postfix}) {
        std::array<std::uint8_t, 16> expected{};
        address_size = expected.size();
        Require(query(accepted, expected.data(), &address_size) == 0 && address_size == expected.size());
        for (std::uint32_t capacity : {0u, 1u, 2u, 15u, 16u, 28u, UINT32_MAX}) {
            std::array<std::uint8_t, 28> actual;
            actual.fill(0xa5);
            address_size = capacity;
            const auto copied = std::min(capacity, std::uint32_t{16});
            Require(query(accepted, actual.data(), &address_size) == 0 && address_size == copied);
            Require(std::memcmp(actual.data(), expected.data(), copied) == 0);
            Require(std::all_of(actual.begin() + copied, actual.end(), [](auto byte) { return byte == 0xa5; }));
        }
    }
    const int shortClient = socket_nid_postfix(2, 1, 0);
    Require(shortClient >= 0 && connect_nid_postfix(shortClient, address.data(), address.size()) == 0);
    peer.fill(0xa5);
    address_size = 1;
    const int shortAccepted = accept_nid_postfix(listener, peer.data(), &address_size);
    Require(shortAccepted >= 0 && address_size == 1 && peer[0] == 16);
    Require(std::all_of(peer.begin() + 1, peer.end(), [](auto byte) { return byte == 0xa5; }));
    Require(close_nid_postfix(shortAccepted) == 0 && close_nid_postfix(shortClient) == 0);

    const char request[] = "guest TCP loopback";
    char received[sizeof(request)]{};
    Require(send_nid_postfix(client, request, sizeof(request), 0x1) == -1 && *__error_nid_postfix() == 45);
    Require(send_nid_postfix(client, request, sizeof(request), 0x20000) == sizeof(request));
    Require(recv_nid_postfix(accepted, received, sizeof(received), 0) == sizeof(received));
    Require(std::strcmp(request, received) == 0);
    Require(shutdown_nid_postfix(client, 1) == 0);
    Require(send_nid_postfix(client, request, sizeof(request), 0x20000) == -1 && *__error_nid_postfix() == 32);

    Require(close_nid_postfix(accepted) == 0);
    std::int64_t sent = 0;
    for (int i = 0; i < 100 && sent >= 0; ++i) sent = send_nid_postfix(client, request, sizeof(request), 0x20000);
    Require(sent == -1 && *__error_nid_postfix() == 32);
    Require(close_nid_postfix(client) == 0);
    Require(close_nid_postfix(listener) == 0);
    Require(close_nid_postfix(listener) == -1 && *__error_nid_postfix() == 9);
}
