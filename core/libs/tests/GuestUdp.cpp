#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <thread>
#include <chrono>
extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI fcntl_nid_postfix(int, int, ...);
int APS5_VABI setsockopt_nid_postfix(int, int, int, const void*, std::uint32_t);
int APS5_VABI getsockopt_nid_postfix(int, int, int, void*, std::uint32_t*);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI getpeername_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI ioctl_nid_postfix(int, std::uint64_t, void*);
int APS5_VABI _ioctl_nid_postfix(int, std::uint64_t, void*);
std::int64_t APS5_VABI send_nid_postfix(int, const void*, std::uint64_t, int);
std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int);
std::int64_t APS5_VABI sendto_nid_postfix(int, const void*, std::uint64_t, int, const void*, std::uint32_t);
std::int64_t APS5_VABI recvfrom_nid_postfix(int, void*, std::uint64_t, int, void*, std::uint32_t*);
int APS5_VABI close_nid_postfix(int);
const char* APS5_VABI inet_ntop_nid_postfix(int, const void*, char*, std::uint32_t);
int APS5_VABI inet_pton_nid_postfix(int, const char*, void*);
int* APS5_VABI __error_nid_postfix();
}
struct Iovec {
    void* base;
    std::uint64_t length;
};
struct Msghdr {
    void* name;
    std::uint32_t nameLength;
    Iovec* iov;
    int iovLength;
    void* control;
    std::uint32_t controlLength;
    int flags;
};
extern "C" {
std::int64_t APS5_VABI sendmsg_nid_postfix(int, const Msghdr*, int);
std::int64_t APS5_VABI recvmsg_nid_postfix(int, Msghdr*, int);
std::int64_t APS5_VABI sceKernelReadv(int, const Iovec*, int);
std::int64_t APS5_VABI sceKernelWritev(int, const Iovec*, int);
}
static void Require(bool value) { if (!value) std::abort(); }

static void CheckAddressLengths(int family) {
    const std::uint32_t addressSize = family == 2 ? 16 : 28;
    const int receiver = socket_nid_postfix(family, 2, 0);
    if (receiver < 0 && family == 28 && *__error_nid_postfix() == 47) return;
    Require(receiver >= 0);
    const int sender = socket_nid_postfix(family, 2, 0);
    Require(sender >= 0);
    std::array<unsigned char, 28> destination{};
    destination[0] = static_cast<unsigned char>(addressSize);
    destination[1] = static_cast<unsigned char>(family);
    if (family == 2) { destination[4] = 127; destination[7] = 1; }
    else destination[23] = 1;
    const int bound = bind_nid_postfix(receiver, destination.data(), addressSize);
    if (bound < 0 && family == 28 && *__error_nid_postfix() == 49) {
        Require(close_nid_postfix(sender) == 0 && close_nid_postfix(receiver) == 0);
        return;
    }
    Require(bound == 0);
    std::uint32_t size = destination.size();
    Require(getsockname_nid_postfix(receiver, destination.data(), &size) == 0 && size == addressSize);
    Require(connect_nid_postfix(sender, destination.data(), addressSize) == 0);
    std::array<unsigned char, 28> expected{};
    size = expected.size();
    Require(getsockname_nid_postfix(sender, expected.data(), &size) == 0 && size == addressSize);
    for (std::uint32_t capacity : {0u, 1u, 2u, addressSize - 1, addressSize, 32u, UINT32_MAX}) {
        const auto copied = std::min(capacity, addressSize);
        std::array<unsigned char, 32> address;
        address.fill(0xa5);
        size = capacity;
        Require(getsockname_nid_postfix(receiver, address.data(), &size) == 0 && size == copied);
        Require(std::memcmp(address.data(), destination.data(), copied) == 0);
        Require(std::all_of(address.begin() + copied, address.end(), [](auto byte) { return byte == 0xa5; }));
        const char payload[] = "address length";
        char received[sizeof(payload)]{};
        Require(sendto_nid_postfix(sender, payload, sizeof(payload), 0, nullptr, 0) == sizeof(payload));
        address.fill(0xa5);
        size = capacity;
        Require(recvfrom_nid_postfix(receiver, received, sizeof(received), 2, address.data(), &size) == sizeof(payload));
        Require(size == copied && std::memcmp(address.data(), expected.data(), copied) == 0);
        Require(std::all_of(address.begin() + copied, address.end(), [](auto byte) { return byte == 0xa5; }));
        address.fill(0xa5);
        Iovec vector{received, sizeof(received)};
        Msghdr message{address.data(), capacity, &vector, 1, nullptr, 0, -1};
        Require(recvmsg_nid_postfix(receiver, &message, 0) == sizeof(payload));
        Require(message.nameLength == copied && std::memcmp(received, payload, sizeof(payload)) == 0);
        Require(std::memcmp(address.data(), expected.data(), copied) == 0);
        Require(std::all_of(address.begin() + copied, address.end(), [](auto byte) { return byte == 0xa5; }));
    }
    Require(close_nid_postfix(sender) == 0 && close_nid_postfix(receiver) == 0);
}

static void CheckSendToPortZero() {
    const int receiver = socket_nid_postfix(2, 2, 0);
    const int sender = socket_nid_postfix(2, 2, 0);
    Require(receiver >= 0 && sender >= 0);
    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(receiver, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(receiver, address.data(), &size) == 0);
    std::array<unsigned char, 16> portZero{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sendto_nid_postfix(sender, "x", 1, 0, portZero.data(), portZero.size()) == -1 && *__error_nid_postfix() == 49);
    char buffer[4];
    Require(recv_nid_postfix(receiver, buffer, sizeof(buffer), 0x80) == -1 && *__error_nid_postfix() == 35);
    Require(sendto_nid_postfix(sender, "x", 1, 0, address.data(), address.size()) == 1);
    Require(recv_nid_postfix(receiver, buffer, sizeof(buffer), 0) == 1);
    const int bound = socket_nid_postfix(2, 2, 0);
    Require(bound >= 0);
    std::array<unsigned char, 16> bindAddress{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(bound, bindAddress.data(), bindAddress.size()) == 0);
    Require(sendto_nid_postfix(bound, "x", 1, 0, portZero.data(), portZero.size()) == 1);
    Require(recv_nid_postfix(receiver, buffer, sizeof(buffer), 0x80) == -1 && *__error_nid_postfix() == 35);
    std::array<unsigned char, 16> anyDestination{16, 2, 0, 0, 0, 0, 0, 0};
    Require(sendto_nid_postfix(bound, "x", 1, 0, anyDestination.data(), anyDestination.size()) == -1 && *__error_nid_postfix() == 49);
    Require(close_nid_postfix(bound) == 0 && close_nid_postfix(receiver) == 0 && close_nid_postfix(sender) == 0);
}

static void CheckDontWait() {
    const int receiver = socket_nid_postfix(2, 2, 0);
    const int sender = socket_nid_postfix(2, 2, 0);
    Require(receiver >= 0 && sender >= 0);
    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(receiver, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(receiver, address.data(), &size) == 0);
    char buffer[16]{};
    Iovec vector{buffer, sizeof(buffer)};
    Msghdr message{nullptr, 0, &vector, 1, nullptr, 0, 0};
    const auto start = std::chrono::steady_clock::now();
    Require(recv_nid_postfix(receiver, buffer, sizeof(buffer), 0x80) == -1 && *__error_nid_postfix() == 35);
    Require(recvfrom_nid_postfix(receiver, buffer, sizeof(buffer), 0x80, nullptr, nullptr) == -1 && *__error_nid_postfix() == 35);
    Require(recvmsg_nid_postfix(receiver, &message, 0x80) == -1 && *__error_nid_postfix() == 35);
    Require(recv_nid_postfix(receiver, buffer, sizeof(buffer), 0x82) == -1 && *__error_nid_postfix() == 35);
    Require(std::chrono::steady_clock::now() - start < std::chrono::seconds(1));
    const char datagram[] = "nowait";
    Require(sendto_nid_postfix(sender, datagram, sizeof(datagram), 0x80, address.data(), address.size()) == sizeof(datagram));
    Require(recvfrom_nid_postfix(receiver, buffer, sizeof(buffer), 2, nullptr, nullptr) == sizeof(datagram));
    std::memset(buffer, 0, sizeof(buffer));
    Require(recv_nid_postfix(receiver, buffer, sizeof(buffer), 0x82) == sizeof(datagram) && std::strcmp(buffer, datagram) == 0);
    std::memset(buffer, 0, sizeof(buffer));
    Require(recv_nid_postfix(receiver, buffer, sizeof(buffer), 0x80) == sizeof(datagram) && std::strcmp(buffer, datagram) == 0);
    Require(connect_nid_postfix(sender, address.data(), address.size()) == 0);
    Require(send_nid_postfix(sender, datagram, sizeof(datagram), 0x80 | 0x20000) == sizeof(datagram));
    std::memset(buffer, 0, sizeof(buffer));
    Require(recvmsg_nid_postfix(receiver, &message, 0) == sizeof(datagram) && std::strcmp(buffer, datagram) == 0);
    Require(recv_nid_postfix(receiver, buffer, sizeof(buffer), 0x80) == -1 && *__error_nid_postfix() == 35);
    Require(recv_nid_postfix(receiver, buffer, sizeof(buffer), 0x1) == -1 && *__error_nid_postfix() == 45);
    Require(close_nid_postfix(receiver) == 0 && close_nid_postfix(sender) == 0);
}

int main() {
    CheckAddressLengths(2);
    CheckAddressLengths(28);
    CheckDontWait();
    CheckSendToPortZero();
    const int receiver = socket_nid_postfix(2, 2, 0);
    const int sender = socket_nid_postfix(2, 2, 17);
    Require(receiver >= 0 && sender >= 0 && receiver != sender);
    std::array<unsigned char, 16> destination{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(receiver, destination.data(), destination.size()) == 0);
    std::uint32_t size = destination.size();
    Require(getsockname_nid_postfix(receiver, destination.data(), &size) == 0 && size == 16);
    Require(destination[2] || destination[3]);
    Require(connect_nid_postfix(sender, destination.data(), destination.size()) == 0);
    char vectorHead[] = "atomic ";
    char vectorTail[] = "datagram";
    Iovec vectorOutput[]{{vectorHead, sizeof(vectorHead) - 1}, {vectorTail, sizeof(vectorTail)}};
    Require(sceKernelWritev(sender, vectorOutput, 2) == sizeof(vectorHead) + sizeof(vectorTail) - 1);
    char vectorFirst[sizeof(vectorHead) - 1]{};
    char vectorSecond[sizeof(vectorTail)]{};
    Iovec vectorInput[]{{vectorFirst, sizeof(vectorFirst)}, {vectorSecond, sizeof(vectorSecond)}};
    Require(sceKernelReadv(receiver, vectorInput, 2) == sizeof(vectorHead) + sizeof(vectorTail) - 1);
    Require(std::memcmp(vectorFirst, "atomic ", sizeof(vectorFirst)) == 0 && std::strcmp(vectorSecond, "datagram") == 0);
    int enabled = 1;
    Require(setsockopt_nid_postfix(sender, 0xffff, 0x20, &enabled, sizeof(enabled)) == 0);
    int option = 0;
    std::uint32_t optionSize = sizeof(option);
    Require(getsockopt_nid_postfix(sender, 0xffff, 0x20, &option, &optionSize) == 0 && option != 0);
    for (const std::int64_t wide : {0, 1}) {
        Require(setsockopt_nid_postfix(sender, 0xffff, 0x20, &wide, sizeof(wide)) == 0);
        optionSize = sizeof(option);
        Require(getsockopt_nid_postfix(sender, 0xffff, 0x20, &option, &optionSize) == 0 && (option != 0) == (wide != 0));
    }
    Require(setsockopt_nid_postfix(sender, 0xffff, 0x20, &enabled, 2) == -1 && *__error_nid_postfix() == 22);
    Require(setsockopt_nid_postfix(sender, 0xffff, 12345, &enabled, sizeof(enabled)) == -1);
    Require(*__error_nid_postfix() == 42);
    const int unconnected = socket_nid_postfix(2, 2, 0);
    Require(unconnected >= 0);
    Require(send_nid_postfix(unconnected, "x", 1, 0) == -1);
    Require(*__error_nid_postfix() == 39);
    Require(sendto_nid_postfix(unconnected, "x", 1, 0, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 39);
    Require(close_nid_postfix(unconnected) == 0);
    const int unsized = socket_nid_postfix(2, 2, 0);
    std::array<unsigned char, 16> unsizedAddress{0, 2, 0, 0, 127, 0, 0, 1};
    Require(unsized >= 0 && bind_nid_postfix(unsized, unsizedAddress.data(), unsizedAddress.size()) == 0);
    std::uint32_t unsizedLength = unsizedAddress.size();
    Require(getsockname_nid_postfix(unsized, unsizedAddress.data(), &unsizedLength) == 0 && unsizedAddress[0] == 16);
    Require(close_nid_postfix(unsized) == 0);
    Require(ioctl_nid_postfix(receiver, 0x8004667e, &enabled) == 0);
    Require(fcntl_nid_postfix(receiver, 3) == 6);
    Require(fcntl_nid_postfix(receiver, 4, 2) == 0);
    Require(fcntl_nid_postfix(receiver, 3) == 2);
    Require(fcntl_nid_postfix(receiver, 4, 6) == 0);
    Require(fcntl_nid_postfix(receiver, 3) == 6);
    Require(fcntl_nid_postfix(receiver, 4, 0x8000) == -1);
    Require(fcntl_nid_postfix(receiver, 3) == 6);
    char buffer[64]{};
    Require(recvfrom_nid_postfix(receiver, buffer, sizeof(buffer), 0, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == 35);
    const char message[] = "guest UDP loopback";
    Require(send_nid_postfix(sender, message, sizeof(message), 0x20000) == sizeof(message));
    int queued = 0;
    for (int i = 0; i < 100 && queued == 0; ++i) {
        Require(ioctl_nid_postfix(receiver, 0x4004667f, &queued) == 0);
        if (!queued) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(queued == sizeof(message));
    Require(recvfrom_nid_postfix(receiver, buffer, sizeof(buffer), 2, nullptr, nullptr) == sizeof(message));
    std::array<unsigned char, 16> source{};
    size = source.size();
    Require(recvfrom_nid_postfix(receiver, buffer, sizeof(buffer), 0, source.data(), &size) == sizeof(message));
    Require(std::strcmp(buffer, message) == 0 && source[1] == 2 && source[4] == 127);
    Require(ioctl_nid_postfix(receiver, 0x4004667f, &queued) == 0 && queued == 0);
    Require(ioctl_nid_postfix(receiver, 123, &queued) == -1 && *__error_nid_postfix() == 25);
    queued = -1;
    Require(_ioctl_nid_postfix(receiver, 0x4004667f, &queued) == 0 && queued == 0);
    Require(_ioctl_nid_postfix(receiver, 123, &queued) == -1 && *__error_nid_postfix() == 25);
    char head[] = "scatter ";
    char tail[] = "gather";
    Iovec parts[2]{{head, 8}, {tail, 6}};
    Msghdr outgoing{nullptr, 0, parts, 2, nullptr, 0, 0};
    Require(sendmsg_nid_postfix(sender, &outgoing, 0) == 14);
    const auto wait = [&] {
        queued = 0;
        for (int i = 0; i < 100 && queued == 0; ++i) {
            Require(ioctl_nid_postfix(receiver, 0x4004667f, &queued) == 0);
            if (!queued) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Require(queued != 0);
    };
    wait();
    char first[4]{}, second[16]{};
    Iovec pieces[2]{{first, sizeof(first)}, {second, sizeof(second)}};
    source.fill(0);
    unsigned char control[8]{};
    Msghdr incoming{source.data(), static_cast<std::uint32_t>(source.size()), pieces, 2, control, sizeof(control), -1};
    Require(recvmsg_nid_postfix(receiver, &incoming, 0) == 14);
    Require(std::memcmp(first, "scat", 4) == 0 && std::memcmp(second, "ter gather", 10) == 0);
    Require(incoming.flags == 0 && incoming.controlLength == 0 && incoming.nameLength == 16 && source[1] == 2 && source[4] == 127);
    Require(sendmsg_nid_postfix(sender, &outgoing, 0) == 14);
    wait();
    char shortBuffer[5]{};
    Iovec single{shortBuffer, sizeof(shortBuffer)};
    Msghdr truncated{nullptr, 0, &single, 1, nullptr, 0, 0};
    Require(recvmsg_nid_postfix(receiver, &truncated, 0) == 5);
    Require(std::memcmp(shortBuffer, "scatt", 5) == 0 && truncated.flags == 0x10);
    Require(recvmsg_nid_postfix(receiver, &truncated, 0) == -1 && *__error_nid_postfix() == 35);
    Msghdr oversized{nullptr, 0, parts, 1025, nullptr, 0, 0};
    Require(sendmsg_nid_postfix(sender, &oversized, 0) == -1 && *__error_nid_postfix() == 40);
    Require(recvmsg_nid_postfix(receiver, nullptr, 0) == -1 && *__error_nid_postfix() == 14);
    Require(close_nid_postfix(receiver) == 0);
    Require(close_nid_postfix(receiver) == -1 && *__error_nid_postfix() == 9);
    Require(ioctl_nid_postfix(receiver, 0x4004667f, &queued) == -1);
    Require(_ioctl_nid_postfix(receiver, 0x4004667f, &queued) == -1 && *__error_nid_postfix() == 9);
    Require(close_nid_postfix(sender) == 0);
    const int peer = socket_nid_postfix(2, 2, 0);
    const int other = socket_nid_postfix(2, 2, 0);
    const int dissolved = socket_nid_postfix(2, 2, 0);
    Require(peer >= 0 && other >= 0 && dissolved >= 0);
    std::array<unsigned char, 16> peerAddress{16, 2, 0, 0, 127, 0, 0, 1};
    std::array<unsigned char, 16> dissolvedAddress{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(peer, peerAddress.data(), peerAddress.size()) == 0);
    Require(bind_nid_postfix(dissolved, dissolvedAddress.data(), dissolvedAddress.size()) == 0);
    size = peerAddress.size();
    Require(getsockname_nid_postfix(peer, peerAddress.data(), &size) == 0);
    size = dissolvedAddress.size();
    Require(getsockname_nid_postfix(dissolved, dissolvedAddress.data(), &size) == 0);
    Require(connect_nid_postfix(dissolved, peerAddress.data(), peerAddress.size()) == 0);
    std::array<unsigned char, 16> peerName{};
    size = peerName.size();
    Require(getpeername_nid_postfix(dissolved, peerName.data(), &size) == 0 && peerName[2] == peerAddress[2] && peerName[3] == peerAddress[3]);
    const std::array<unsigned char, 16> unspecified{16, 0};
    Require(connect_nid_postfix(dissolved, unspecified.data(), unspecified.size()) == -1 && *__error_nid_postfix() == 47);
    size = peerName.size();
    Require(getpeername_nid_postfix(dissolved, peerName.data(), &size) == -1 && *__error_nid_postfix() == 57);
    std::array<unsigned char, 16> localName{};
    size = localName.size();
    Require(getsockname_nid_postfix(dissolved, localName.data(), &size) == 0);
    Require(localName[2] == dissolvedAddress[2] && localName[3] == dissolvedAddress[3]);
#ifndef _WIN32
    Require(localName[4] == 0 && localName[5] == 0 && localName[6] == 0 && localName[7] == 0);
#endif
    Require(send_nid_postfix(dissolved, "x", 1, 0) == -1 && *__error_nid_postfix() == 39);
    const char unrelated[] = "unrelated sender";
    Require(sendto_nid_postfix(other, unrelated, sizeof(unrelated), 0, dissolvedAddress.data(), dissolvedAddress.size()) == sizeof(unrelated));
    queued = 0;
    for (int i = 0; i < 1000 && queued == 0; ++i) {
        Require(ioctl_nid_postfix(dissolved, 0x4004667f, &queued) == 0);
        if (!queued) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(queued == sizeof(unrelated));
    Require(recvfrom_nid_postfix(dissolved, buffer, sizeof(buffer), 0, nullptr, nullptr) == sizeof(unrelated));
    Require(std::strcmp(buffer, unrelated) == 0);
    for (const int descriptor : {peer, other, dissolved}) Require(close_nid_postfix(descriptor) == 0);
    unsigned char parsed[16]{};
    Require(inet_pton_nid_postfix(2, "127.0.0.1", parsed) == 1);
    Require(parsed[0] == 127 && parsed[1] == 0 && parsed[2] == 0 && parsed[3] == 1);
    char text[64]{};
    Require(inet_ntop_nid_postfix(2, parsed, text, sizeof(text)) == text);
    Require(std::strcmp(text, "127.0.0.1") == 0);
    Require(inet_pton_nid_postfix(28, "::1", parsed) == 1);
    Require(inet_ntop_nid_postfix(28, parsed, text, sizeof(text)) == text);
    Require(std::strcmp(text, "::1") == 0);
    Require(inet_pton_nid_postfix(99, "127.0.0.1", parsed) == -1 && *__error_nid_postfix() == 47);
    Require(inet_pton_nid_postfix(2, "not-an-address", parsed) == 0);
    Require(inet_ntop_nid_postfix(2, parsed, text, 4) == nullptr && *__error_nid_postfix() == 28);
    const int stream = socket_nid_postfix(2, 1, 0);
    Require(stream >= 0);
    Require(close_nid_postfix(stream) == 0);
    const int nonblocking = socket_nid_postfix(2, 2 | 0x20000000, 0);
    Require(nonblocking >= 0 && fcntl_nid_postfix(nonblocking, 3) == 6);
    std::array<unsigned char, 16> nonblockingAddress{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(nonblocking, nonblockingAddress.data(), nonblockingAddress.size()) == 0);
    Require(recvfrom_nid_postfix(nonblocking, buffer, sizeof(buffer), 0, nullptr, nullptr) == -1 && *__error_nid_postfix() == 35);
    Require(close_nid_postfix(nonblocking) == 0);
    const int closeOnExec = socket_nid_postfix(2, 1 | 0x10000000, 6);
    Require(closeOnExec >= 0 && fcntl_nid_postfix(closeOnExec, 3) == 2);
    Require(close_nid_postfix(closeOnExec) == 0);
    Require(socket_nid_postfix(2, 0x20000000, 0) == -1 && *__error_nid_postfix() == 43);
}
