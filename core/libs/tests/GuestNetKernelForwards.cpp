#include <array>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include "prx/libSceNet/libSceNet.hpp"
#include "prx/libkernel/libkernel.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#define Require(condition) RequireAt((condition), __LINE__)

static void RequireAt(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "failed at line %d, errno %d\n", line, *__error_nid_postfix());
    std::abort();
}

static bool Failed(std::int64_t result, int error) {
    const bool matched = result == -1 && *__error_nid_postfix() == error;
    *__error_nid_postfix() = 0;
    return matched;
}

static int Open(bool net, int type) {
    const int protocol = type == 1 ? 6 : 17;
    return net ? sceNetSocket("forward", 2, type, protocol) : socket_nid_postfix(2, type, protocol);
}

static int Close(bool net, int descriptor) {
    return net ? sceNetSocketClose(descriptor) : close_nid_postfix(descriptor);
}

static char* UnreadablePage() {
#ifdef _WIN32
    return static_cast<char*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
#else
    return static_cast<char*>(mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
#endif
}

static void CheckDatagram(bool net) {
    const int receiver = Open(net, 2);
    const int sender = Open(net, 2);
    Require(receiver >= 0 && sender >= 0);
    std::array<unsigned char, 16> local{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(receiver, local.data(), local.size()) == 0);
    Require(bind_nid_postfix(sender, local.data(), local.size()) == 0);
    std::array<unsigned char, 16> receiverAddress{};
    std::array<unsigned char, 16> senderAddress{};
    std::uint32_t size = receiverAddress.size();
    Require(getsockname_nid_postfix(receiver, receiverAddress.data(), &size) == 0 && size == receiverAddress.size());
    Require((receiverAddress[2] | receiverAddress[3]) != 0);
    size = senderAddress.size();
    Require(getsockname_nid_postfix(sender, senderAddress.data(), &size) == 0 && size == senderAddress.size());
    char received[8]{};
    Require(recvfrom_nid_postfix(receiver, received, 0, 0, nullptr, nullptr) == 0);
    const char payload[] = "ping";
    for (const std::uint32_t capacity : {16u, 4u, 0u}) {
        Require(sendto_nid_postfix(sender, payload, sizeof(payload), 0, receiverAddress.data(), receiverAddress.size()) == sizeof(payload));
        std::array<unsigned char, 16> from{};
        size = capacity;
        Require(recvfrom_nid_postfix(receiver, received, sizeof(received), 0, from.data(), &size) == sizeof(payload));
        Require(std::strcmp(received, payload) == 0 && size == capacity);
        if (capacity == from.size()) Require(from[2] == senderAddress[2] && from[3] == senderAddress[3]);
    }
    Require(sendto_nid_postfix(sender, payload, sizeof(payload), 0, receiverAddress.data(), receiverAddress.size()) == sizeof(payload));
    size = 123;
    Require(recvfrom_nid_postfix(receiver, received, sizeof(received), 0, nullptr, &size) == sizeof(payload));
    Require(size == 123 && std::strcmp(received, payload) == 0);
    Require(Close(net, receiver) == 0 && Close(net, sender) == 0);
}

static void CheckArguments(bool net) {
    const int socket = Open(net, 2);
    Require(socket >= 0);
    char* const unreadable = UnreadablePage();
    Require(unreadable != nullptr);
    std::array<unsigned char, 32> local{16, 2, 0, 0, 127, 0, 0, 1};
    std::array<unsigned char, 16> otherFamily{16, 28};
    std::array<unsigned char, 256> longAddress{};
    Require(Failed(bind_nid_postfix(socket, nullptr, 16), 14));
    Require(Failed(bind_nid_postfix(socket, local.data(), 1), 22));
    Require(Failed(bind_nid_postfix(socket, local.data(), 17), 22));
    Require(Failed(bind_nid_postfix(socket, longAddress.data(), 256), 63));
    Require(Failed(bind_nid_postfix(socket, otherFamily.data(), 16), 47));
    Require(bind_nid_postfix(socket, local.data(), 16) == 0);
    std::uint32_t size = 16;
    Require(Failed(getsockname_nid_postfix(socket, nullptr, &size), 14));
    Require(Failed(getsockname_nid_postfix(socket, local.data(), nullptr), 14));
    Require(Failed(sendto_nid_postfix(socket, nullptr, 1, 0, local.data(), 16), 14));
    Require(Failed(sendto_nid_postfix(socket, "x", 1, 0, local.data(), 1), 22));
    Require(Failed(sendto_nid_postfix(socket, "x", 1, 0, local.data(), 17), 22));
    Require(Failed(sendto_nid_postfix(socket, "x", 1, 0, longAddress.data(), 256), 63));
    Require(Failed(sendto_nid_postfix(socket, "x", 1, 0, otherFamily.data(), 16), 47));
    Require(Failed(sendto_nid_postfix(socket, "x", 1, 0, unreadable, 16), 14));
    Require(Failed(sendto_nid_postfix(socket, "x", 1, 0, nullptr, 16), 22));
    Require(Failed(sendto_nid_postfix(socket, "x", 1, 0x2, local.data(), 16), 45));
    Require(Failed(sendto_nid_postfix(socket, "x", std::uint64_t{INT_MAX} + 1, 0, local.data(), 16), 40));
    char received[8]{};
    Require(Failed(recvfrom_nid_postfix(socket, nullptr, 1, 0, nullptr, nullptr), 14));
    Require(Failed(recvfrom_nid_postfix(socket, received, sizeof(received), 0, local.data(), nullptr), 14));
    Require(Failed(recvfrom_nid_postfix(socket, received, sizeof(received), 0, nullptr, reinterpret_cast<std::uint32_t*>(unreadable)), 14));
    Require(Failed(recvfrom_nid_postfix(socket, received, sizeof(received), 0, local.data(), reinterpret_cast<std::uint32_t*>(unreadable)), 14));
    Require(Failed(recvfrom_nid_postfix(socket, received, sizeof(received), 0x1, nullptr, nullptr), 45));
    Require(Failed(recvfrom_nid_postfix(socket, received, std::uint64_t{INT_MAX} + 1, 0, nullptr, nullptr), 40));
    Require(Failed(recvfrom_nid_postfix(socket, received, sizeof(received), 0x80, nullptr, nullptr), 35));
    Require(Close(net, socket) == 0);
}

struct Stream {
    int listener;
    int client;
    int accepted;
};

static Stream OpenStream(bool net) {
    Stream stream{Open(net, 1), Open(net, 1), -1};
    Require(stream.listener >= 0 && stream.client >= 0);
    std::array<unsigned char, 16> local{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(stream.listener, local.data(), local.size()) == 0);
    std::uint32_t size = local.size();
    Require(getsockname_nid_postfix(stream.listener, local.data(), &size) == 0);
    Require((net ? sceNetListen(stream.listener, 1) : listen_nid_postfix(stream.listener, 1)) == 0);
    Require((net ? sceNetConnect(stream.client, local.data(), 16) : connect_nid_postfix(stream.client, local.data(), 16)) == 0);
    stream.accepted = net ? sceNetAccept(stream.listener, nullptr, nullptr) : accept_nid_postfix(stream.listener, nullptr, nullptr);
    Require(stream.accepted >= 0);
    return stream;
}

static void CheckStreamWaitAll(bool net) {
    const int unconnected = Open(net, 1);
    char unused = 0;
    Require(unconnected >= 0 && Failed(recvfrom_nid_postfix(unconnected, &unused, 0, 0, nullptr, nullptr), 57));
    Require(Close(net, unconnected) == 0);
    const auto stream = OpenStream(net);
    constexpr int WaitAll = 0x40;
    std::thread writer([&] {
        Require(sendto_nid_postfix(stream.client, "ab", 2, 0, nullptr, 0) == 2);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Require(sendto_nid_postfix(stream.client, "cd", 2, 0, nullptr, 0) == 2);
    });
    char received[4]{};
    std::array<unsigned char, 16> from;
    from.fill(0xa5);
    std::uint32_t size = from.size();
    Require(recvfrom_nid_postfix(stream.accepted, received, sizeof(received), WaitAll, from.data(), &size) == 4);
    writer.join();
    Require(std::memcmp(received, "abcd", 4) == 0 && size == 0 && from[0] == 0xa5);
    Require(sendto_nid_postfix(stream.client, "ef", 2, 0, nullptr, 0) == 2);
    char peeked[2]{};
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(recvfrom_nid_postfix(stream.accepted, peeked, 0, 0, nullptr, nullptr) == 0);
    const int peek = net ? WaitAll | 0x2 : 0x2;
    Require(recvfrom_nid_postfix(stream.accepted, peeked, sizeof(peeked), peek, nullptr, nullptr) == 2);
    Require(std::memcmp(peeked, "ef", 2) == 0);
    Require(sendto_nid_postfix(stream.client, "g", 1, 0, nullptr, 0) == 1);
    Require((net ? sceNetShutdown(stream.client, 1) : shutdown_nid_postfix(stream.client, 1)) == 0);
    char rest[8]{};
    Require(recvfrom_nid_postfix(stream.accepted, rest, sizeof(rest), WaitAll, nullptr, nullptr) == 3);
    Require(std::memcmp(rest, "efg", 3) == 0);
    Require(Close(net, stream.accepted) == 0 && Close(net, stream.client) == 0 && Close(net, stream.listener) == 0);
}

static void CheckUnknownDescriptor() {
    constexpr int unknown = 0x7ffffff0;
    std::array<unsigned char, 16> local{16, 2, 0, 0, 127, 0, 0, 1};
    std::uint32_t size = local.size();
    char byte = 0;
    Require(Failed(bind_nid_postfix(unknown, local.data(), local.size()), 9));
    Require(Failed(getsockname_nid_postfix(unknown, local.data(), &size), 9));
    Require(Failed(sendto_nid_postfix(unknown, &byte, 1, 0, local.data(), local.size()), 9));
    Require(Failed(recvfrom_nid_postfix(unknown, &byte, 1, 0, nullptr, nullptr), 9));
}

int main() {
    for (const bool net : {false, true}) {
        CheckDatagram(net);
        CheckArguments(net);
        CheckStreamWaitAll(net);
    }
    CheckUnknownDescriptor();
    return 0;
}
