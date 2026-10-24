#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI fcntl_nid_postfix(int, int, ...);
std::int64_t APS5_VABI sendto_nid_postfix(int, const void*, std::uint64_t, int, const void*, std::uint32_t);
std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int);
std::int64_t APS5_VABI recvfrom_nid_postfix(int, void*, std::uint64_t, int, void*, std::uint32_t*);
int APS5_VABI close_nid_postfix(int);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sceNetInit_nid_postfix();
int APS5_VABI sceNetSocket(const char*, int, int, int);
int APS5_VABI sceNetBind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI sceNetConnect(int, const void*, std::uint32_t);
int APS5_VABI sceNetGetsockname(int, void*, std::uint32_t*);
int APS5_VABI sceNetSetsockopt(int, int, int, const void*, std::uint32_t);
std::int64_t APS5_VABI sceNetSendto(int, const void*, std::size_t, int, const void*, std::uint32_t);
std::int64_t APS5_VABI sceNetRecv(int, void*, std::size_t, int);
std::int64_t APS5_VABI sceNetRecvfrom(int, void*, std::size_t, int, void*, std::uint32_t*);
int APS5_VABI sceNetSocketClose(int);
int* APS5_VABI sceNetErrnoLoc();
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

static void Check(bool net, bool from, int family) {
    int* error = net ? sceNetErrnoLoc() : __error_nid_postfix();
    const auto create = [&] { return net ? sceNetSocket(nullptr, family, 2, 17) : socket_nid_postfix(family, 2, 17); };
    const auto closeSocket = [&](int descriptor) { return net ? sceNetSocketClose(descriptor) : close_nid_postfix(descriptor); };
    const int receiver = create();
    if (receiver < 0 && family == 28 && *error == 47) return;
    const int sender = create();
    Require(receiver >= 0 && sender >= 0, "create UDP sockets");
    std::array<unsigned char, 28> destination{};
    const std::uint32_t addressSize = family == 2 ? 16 : 28;
    destination[0] = static_cast<unsigned char>(addressSize);
    destination[1] = static_cast<unsigned char>(family);
    if (family == 2) { destination[4] = 127; destination[7] = 1; }
    else destination[23] = 1;
    const auto bind = [&](int descriptor) {
        return net ? sceNetBind_nid_postfix(descriptor, destination.data(), addressSize) :
            bind_nid_postfix(descriptor, destination.data(), addressSize);
    };
    const int bound = bind(receiver);
    if (bound < 0 && family == 28 && *error == 49) {
        Require(closeSocket(sender) == 0 && closeSocket(receiver) == 0, "close sockets without IPv6 loopback");
        return;
    }
    Require(bound == 0 && bind(sender) == 0, "bind loopback sockets");
    auto senderAddress = destination;
    std::uint32_t size = addressSize;
    Require((net ? sceNetGetsockname(sender, senderAddress.data(), &size) :
        getsockname_nid_postfix(sender, senderAddress.data(), &size)) == 0, "get sender address");
    size = addressSize;
    Require((net ? sceNetGetsockname(receiver, destination.data(), &size) :
        getsockname_nid_postfix(receiver, destination.data(), &size)) == 0, "get receiver address");
    Require((net ? sceNetConnect(receiver, senderAddress.data(), addressSize) :
        connect_nid_postfix(receiver, senderAddress.data(), addressSize)) == 0, "connect receiver");
    const int enabled = 1;
    Require((net ? sceNetSetsockopt(receiver, 0xffff, 0x1200, &enabled, sizeof(enabled)) :
        fcntl_nid_postfix(receiver, 4, 6)) == 0, "make receiver nonblocking");
    const auto receive = [&](void* buffer, std::size_t length, int flags, void* address, std::uint32_t* addressLength) {
        if (net) return from ? sceNetRecvfrom(receiver, buffer, length, flags, address, addressLength) :
            sceNetRecv(receiver, buffer, length, flags);
        return from ? recvfrom_nid_postfix(receiver, buffer, length, flags, address, addressLength) :
            recv_nid_postfix(receiver, buffer, length, flags);
    };
    const auto send = [&](const char* data, std::size_t length) {
        Require((net ? sceNetSendto(sender, data, length, 0, destination.data(), addressSize) :
            sendto_nid_postfix(sender, data, length, 0, destination.data(), addressSize)) ==
            static_cast<std::int64_t>(length), "send datagram");
    };
    const char message[] = "UDP header/body";
    for (const std::size_t capacity : {std::size_t{4}, sizeof(message), sizeof(message) + 4, std::size_t{0}}) {
        send(message, sizeof(message));
        std::array<char, 32> buffer{};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        std::int64_t result;
        do {
            result = receive(buffer.data(), buffer.size(), 2, nullptr, nullptr);
            if (result >= 0 || *error != 35) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);
        Require(result == sizeof(message), "wait for complete datagram");
        for (const int flags : {2, 0}) {
            buffer.fill('!');
            std::array<unsigned char, 32> address{};
            address.fill(0xa5);
            size = address.size();
            *error = 123;
            result = receive(buffer.data() + 1, capacity, flags, address.data(), &size);
            const auto expected = std::min(capacity, sizeof(message));
            if (result != static_cast<std::int64_t>(expected)) {
                std::fprintf(stderr, "%s%s IPv%d capacity=%zu flags=%d: expected %zu, got %lld errno=%d\n",
                    net ? "sceNetRecv" : "recv", from ? "from" : "", family == 2 ? 4 : 6,
                    capacity, flags, expected, static_cast<long long>(result), *error);
                std::exit(1);
            }
            Require(*error == 123, "successful receive preserves errno");
            Require(buffer[0] == '!' && std::memcmp(buffer.data() + 1, message, expected) == 0 &&
                std::all_of(buffer.begin() + 1 + expected, buffer.end(), [](char value) { return value == '!'; }),
                "copied prefix and buffer guards");
            if (from) {
                Require(size == addressSize && std::memcmp(address.data(), senderAddress.data(), addressSize) == 0,
                    "source address on truncated receive");
                Require(std::all_of(address.begin() + addressSize, address.end(), [](unsigned char value) { return value == 0xa5; }),
                    "source address guards");
            }
            if (flags == 2) {
                Require(receive(buffer.data(), buffer.size(), 2, nullptr, nullptr) == sizeof(message) &&
                    std::memcmp(buffer.data(), message, sizeof(message)) == 0, "short peek preserves complete packet");
            }
        }
        Require(receive(buffer.data(), buffer.size(), 0, nullptr, nullptr) < 0 && *error == 35,
            "consuming receive discards remainder");
    }
    send("", 0);
    char byte = '!';
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    std::int64_t result;
    do {
        result = receive(&byte, 1, 0, nullptr, nullptr);
        if (result >= 0 || *error != 35) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    Require(result == 0 && byte == '!', "empty datagram");
    Require(closeSocket(receiver) == 0, "close receiver");
    Require(closeSocket(sender) == 0, "close sender");
}

int main(int argc, char** argv) {
    Require(argc == 2, "expected receive API argument");
    const bool net = std::strncmp(argv[1], "sce", 3) == 0;
    const bool from = std::strstr(argv[1], "from") != nullptr;
    if (net) Require(sceNetInit_nid_postfix() == 0, "initialize libSceNet");
    Check(net, from, 2);
    Check(net, from, 28);
}
