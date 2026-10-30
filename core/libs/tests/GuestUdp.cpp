#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <thread>
#include <chrono>

static void Require(bool value) { if (!value) std::abort(); }

static void CheckUnreachablePortIgnored() {
    const int closing = socket_nid_postfix(2, 2, 0);
    Require(closing >= 0);
    std::array<unsigned char, 16> unreachable{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(closing, unreachable.data(), unreachable.size()) == 0);
    std::uint32_t size = unreachable.size();
    Require(getsockname_nid_postfix(closing, unreachable.data(), &size) == 0 && size == unreachable.size());
    const int receiver = socket_nid_postfix(2, 2 | 0x20000000, 0);
    const int sender = socket_nid_postfix(2, 2, 0);
    Require(receiver >= 0 && sender >= 0);
    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(receiver, address.data(), address.size()) == 0);
    size = address.size();
    Require(getsockname_nid_postfix(receiver, address.data(), &size) == 0);
    Require(close_nid_postfix(closing) == 0);
    Require(sendto_nid_postfix(receiver, "x", 1, 0, unreachable.data(), unreachable.size()) == 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    char received[4]{};
    for (int attempt = 0; attempt < 3; ++attempt) {
        Require(recvfrom_nid_postfix(receiver, received, sizeof(received), 0, nullptr, nullptr) == -1 && *__error_nid_postfix() == 35);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    Require(sendto_nid_postfix(sender, "ok", 2, 0, address.data(), address.size()) == 2);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(recvfrom_nid_postfix(receiver, received, sizeof(received), 0, nullptr, nullptr) == 2 && received[0] == 'o');
    Require(close_nid_postfix(sender) == 0 && close_nid_postfix(receiver) == 0);
}

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
        KernelIovec vector{received, sizeof(received)};
        GuestMsghdr message{address.data(), capacity, &vector, 1, nullptr, 0, -1};
        Require(recvmsg_nid_postfix(receiver, &message, 0) == sizeof(payload));
        Require(message.nameLength == copied && std::memcmp(received, payload, sizeof(payload)) == 0);
        Require(std::memcmp(address.data(), expected.data(), copied) == 0);
        Require(std::all_of(address.begin() + copied, address.end(), [](auto byte) { return byte == 0xa5; }));
    }
    Require(close_nid_postfix(sender) == 0 && close_nid_postfix(receiver) == 0);
}

static void CheckDissolvedName(int family, int binding) {
    const std::uint32_t addressSize = family == 2 ? 16 : 28;
    const int peer = socket_nid_postfix(family, 2, 0);
    if (peer < 0 && family == 28 && *__error_nid_postfix() == 47) return;
    Require(peer >= 0);
    std::array<unsigned char, 28> peerAddress{};
    peerAddress[0] = static_cast<unsigned char>(addressSize);
    peerAddress[1] = static_cast<unsigned char>(family);
    if (family == 2) { peerAddress[4] = 127; peerAddress[7] = 1; }
    else peerAddress[23] = 1;
    const int bound = bind_nid_postfix(peer, peerAddress.data(), addressSize);
    if (bound < 0 && family == 28 && *__error_nid_postfix() == 49) {
        Require(close_nid_postfix(peer) == 0);
        return;
    }
    Require(bound == 0);
    std::uint32_t size = peerAddress.size();
    Require(getsockname_nid_postfix(peer, peerAddress.data(), &size) == 0 && size == addressSize);
    int dissolved = socket_nid_postfix(family, 2, 0);
    Require(dissolved >= 0);
    if (binding != 0) {
        auto local = peerAddress;
        local[2] = local[3] = 0;
        Require(bind_nid_postfix(dissolved, local.data(), addressSize) == 0);
        if (binding == 2) {
            size = local.size();
            Require(getsockname_nid_postfix(dissolved, local.data(), &size) == 0 && size == addressSize);
            Require(close_nid_postfix(dissolved) == 0);
            dissolved = socket_nid_postfix(family, 2, 0);
            Require(dissolved >= 0);
            Require(bind_nid_postfix(dissolved, local.data(), addressSize) == 0);
        }
    }
    const int alias = dup_nid_postfix(dissolved);
    Require(alias >= 0);
    Require(connect_nid_postfix(dissolved, peerAddress.data(), addressSize) == 0);
    std::array<unsigned char, 28> connected{};
    size = connected.size();
    Require(getsockname_nid_postfix(alias, connected.data(), &size) == 0 && size == addressSize);
    Require(connected[2] || connected[3]);
    const auto begin = family == 2 ? 4 : 8;
    const auto end = family == 2 ? 8 : 24;
    Require(std::equal(connected.begin() + begin, connected.begin() + end, peerAddress.begin() + begin));
    for (const bool portZero : {false, true}) {
        auto disconnect = peerAddress;
        if (portZero) disconnect[2] = disconnect[3] = 0;
        else disconnect[1] = 0;
        Require(connect_nid_postfix(alias, disconnect.data(), addressSize) == -1 && *__error_nid_postfix() == (portZero ? 49 : 47));
        for (const int descriptor : {dissolved, alias}) {
            std::array<unsigned char, 28> name{};
            size = name.size();
            Require(getpeername_nid_postfix(descriptor, name.data(), &size) == -1 && *__error_nid_postfix() == 57);
            size = name.size();
            Require(getsockname_nid_postfix(descriptor, name.data(), &size) == 0 && size == addressSize);
            Require(name[2] == connected[2] && name[3] == connected[3]);
            Require(std::all_of(name.begin() + begin, name.begin() + end, [](auto byte) { return byte == 0; }));
            if (family == 28) Require(std::all_of(name.begin() + 24, name.end(), [](auto byte) { return byte == 0; }));
            for (const std::uint32_t capacity : {0u, 1u, 6u, addressSize}) {
                std::array<unsigned char, 32> truncated;
                truncated.fill(0xa5);
                size = capacity;
                Require(getsockname_nid_postfix(descriptor, truncated.data(), &size) == 0 && size == capacity);
                Require(std::equal(name.begin(), name.begin() + size, truncated.begin()));
                Require(std::all_of(truncated.begin() + size, truncated.end(), [](auto byte) { return byte == 0xa5; }));
            }
        }
        Require(connect_nid_postfix(dissolved, peerAddress.data(), addressSize) == 0);
        std::array<unsigned char, 28> reconnected{};
        size = reconnected.size();
        Require(getsockname_nid_postfix(alias, reconnected.data(), &size) == 0 && size == addressSize);
        Require(std::equal(connected.begin(), connected.begin() + addressSize, reconnected.begin()));
        Require(send_nid_postfix(alias, "x", 1, 0) == 1);
        char payload = 0;
        Require(recv_nid_postfix(peer, &payload, 1, 0) == 1 && payload == 'x');
    }
    Require(close_nid_postfix(alias) == 0 && close_nid_postfix(dissolved) == 0 && close_nid_postfix(peer) == 0);
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

static void CheckShutReadUnconnected() {
    const int receiver = socket_nid_postfix(2, 2, 0);
    const int sender = socket_nid_postfix(2, 2, 0);
    Require(receiver >= 0 && sender >= 0);
    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(receiver, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(receiver, address.data(), &size) == 0);
    Require(sendto_nid_postfix(sender, "x", 1, 0, address.data(), address.size()) == 1);
    char buffer[4];
    Require(recv_nid_postfix(receiver, buffer, sizeof(buffer), 0x02) == 1);
    *__error_nid_postfix() = 0;
    Require(shutdown_nid_postfix(receiver, 0) == -1 && *__error_nid_postfix() == 57);
    int queued = -1;
    Require(ioctl_nid_postfix(receiver, 0x4004667f, &queued) == 0 && queued == 0);
    Require(recv_nid_postfix(receiver, buffer, sizeof(buffer), 0) == 0);
    Require(close_nid_postfix(receiver) == 0 && close_nid_postfix(sender) == 0);
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
    KernelIovec vector{buffer, sizeof(buffer)};
    GuestMsghdr message{nullptr, 0, &vector, 1, nullptr, 0, 0};
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

static void CheckArrivalTimestamps(int family) {
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
    Require(getsockname_nid_postfix(receiver, destination.data(), &size) == 0);
    Require(connect_nid_postfix(sender, destination.data(), addressSize) == 0);
    for (const int options : {0, 0x400, 0x2000, 0x2400}) {
        for (const int option : {0x400, 0x2000}) {
            const int enabled = (options & option) != 0 ? 1 : 0;
            Require(setsockopt_nid_postfix(receiver, 0xffff, option, &enabled, sizeof(enabled)) == 0);
        }
        const bool binary = family == 2 && (options & 0x2000) != 0;
        const bool micro = (options & 0x400) != 0;
        const std::uint32_t expected = (binary ? 32u : 0u) + (micro ? 32u : 0u);
        for (const std::uint32_t capacity : {0u, 8u, 16u, 40u, 64u, 80u}) {
            const char payload[] = "stamped";
            Require(sendto_nid_postfix(sender, payload, sizeof(payload), 0, nullptr, 0) == sizeof(payload));
            std::array<unsigned char, 80> control;
            control.fill(0xa5);
            char received[sizeof(payload)]{};
            KernelIovec vector{received, sizeof(received)};
            GuestMsghdr message{nullptr, 0, &vector, 1, control.data(), capacity, -1};
            KernelTimespec before{};
            Require(clock_gettime_nid_postfix(0, &before) == 0);
            Require(recvmsg_nid_postfix(receiver, &message, 0) == sizeof(payload));
            KernelTimespec after{};
            Require(clock_gettime_nid_postfix(0, &after) == 0);
            Require(std::memcmp(received, payload, sizeof(payload)) == 0);
            const std::uint32_t copied = std::min(capacity, expected);
            Require(message.controlLength == copied);
            Require(message.flags == (capacity < expected ? 0x20 : 0));
            Require(std::all_of(control.begin() + copied, control.end(), [](auto byte) { return byte == 0xa5; }));
            std::array<unsigned char, 64> whole{};
            std::memcpy(whole.data(), control.data(), copied);
            std::uint32_t at = 0;
            for (const int type : {4, 2}) {
                if ((type == 4 && !binary) || (type == 2 && !micro)) continue;
                if (at + 32 > copied) break;
                std::uint32_t length = 0;
                int level = 0;
                int kind = 0;
                std::int64_t seconds = 0;
                std::uint64_t fraction = 0;
                std::memcpy(&length, whole.data() + at, sizeof(length));
                std::memcpy(&level, whole.data() + at + 4, sizeof(level));
                std::memcpy(&kind, whole.data() + at + 8, sizeof(kind));
                std::memcpy(&seconds, whole.data() + at + 16, sizeof(seconds));
                std::memcpy(&fraction, whole.data() + at + 24, sizeof(fraction));
                Require(length == 32 && level == 0xffff && kind == type);
                Require(seconds >= before.tv_sec && seconds <= after.tv_sec);
                if (type == 2) Require(fraction < 1000000);
                at += 32;
            }
            if (copied >= 8) {
                std::uint32_t length = 0;
                std::memcpy(&length, whole.data(), sizeof(length));
                Require(length == 32);
            }
        }
        const char payload[] = "unstamped";
        Require(sendto_nid_postfix(sender, payload, sizeof(payload), 0, nullptr, 0) == sizeof(payload));
        char received[sizeof(payload)]{};
        KernelIovec vector{received, sizeof(received)};
        GuestMsghdr message{nullptr, 0, &vector, 1, nullptr, 0, -1};
        Require(recvmsg_nid_postfix(receiver, &message, 0) == sizeof(payload));
        Require(message.controlLength == 0 && message.flags == 0);
    }
    Require(close_nid_postfix(sender) == 0 && close_nid_postfix(receiver) == 0);
}

int main() {
    CheckAddressLengths(2);
    CheckAddressLengths(28);
    for (const int family : {2, 28})
        for (int binding = 0; binding < 3; ++binding) CheckDissolvedName(family, binding);
    CheckArrivalTimestamps(2);
    CheckArrivalTimestamps(28);
    CheckDontWait();
    CheckSendToPortZero();
    CheckShutReadUnconnected();
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
    KernelIovec vectorOutput[]{{vectorHead, sizeof(vectorHead) - 1}, {vectorTail, sizeof(vectorTail)}};
    Require(sceKernelWritev(sender, vectorOutput, 2) == sizeof(vectorHead) + sizeof(vectorTail) - 1);
    char vectorFirst[sizeof(vectorHead) - 1]{};
    char vectorSecond[sizeof(vectorTail)]{};
    KernelIovec vectorInput[]{{vectorFirst, sizeof(vectorFirst)}, {vectorSecond, sizeof(vectorSecond)}};
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
    KernelIovec parts[2]{{head, 8}, {tail, 6}};
    GuestMsghdr outgoing{nullptr, 0, parts, 2, nullptr, 0, 0};
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
    KernelIovec pieces[2]{{first, sizeof(first)}, {second, sizeof(second)}};
    source.fill(0);
    unsigned char control[8]{};
    GuestMsghdr incoming{source.data(), static_cast<std::uint32_t>(source.size()), pieces, 2, control, sizeof(control), -1};
    Require(recvmsg_nid_postfix(receiver, &incoming, 2) == 14 && incoming.flags == 2);
    incoming.nameLength = static_cast<std::uint32_t>(source.size());
    incoming.controlLength = sizeof(control);
    Require(recvmsg_nid_postfix(receiver, &incoming, 0) == 14);
    Require(std::memcmp(first, "scat", 4) == 0 && std::memcmp(second, "ter gather", 10) == 0);
    Require(incoming.flags == 0 && incoming.controlLength == 0 && incoming.nameLength == 16 && source[1] == 2 && source[4] == 127);
    Require(sendmsg_nid_postfix(sender, &outgoing, 0) == 14);
    wait();
    char shortBuffer[5]{};
    KernelIovec single{shortBuffer, sizeof(shortBuffer)};
    GuestMsghdr truncated{nullptr, 0, &single, 1, nullptr, 0, 0};
    Require(recvmsg_nid_postfix(receiver, &truncated, 2) == 5 && truncated.flags == 0x12);
    Require(recvmsg_nid_postfix(receiver, &truncated, 0x80) == 5);
    Require(std::memcmp(shortBuffer, "scatt", 5) == 0 && truncated.flags == 0x90);
    Require(recvmsg_nid_postfix(receiver, &truncated, 0) == -1 && *__error_nid_postfix() == 35);
    KernelIovec none{shortBuffer, 0};
    GuestMsghdr empty{nullptr, 0, &none, 1, nullptr, 0, -1};
    Require(fcntl_nid_postfix(receiver, 4, 2) == 0);
    Require(recvmsg_nid_postfix(receiver, &empty, 0) == 0 && empty.flags == 0);
    Require(fcntl_nid_postfix(receiver, 4, 6) == 0);
    GuestMsghdr oversized{nullptr, 0, parts, 1025, nullptr, 0, 0};
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
    Require(localName[4] == 0 && localName[5] == 0 && localName[6] == 0 && localName[7] == 0);
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
    CheckUnreachablePortIgnored();
}
