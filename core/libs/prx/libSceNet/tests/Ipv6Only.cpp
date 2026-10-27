#include "prx/libkernel/libkernel.hpp"
#include "prx/libSceNet/libSceNet.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "IPV6_V6ONLY check failed at line %u (errno %d)\n", location.line(), *sceNetErrnoLoc());
    std::abort();
}

static void RequireOption(int descriptor, int expected) {
    for (std::uint32_t capacity = 0; capacity <= 8; ++capacity) {
        std::array<unsigned char, 8> output;
        output.fill(0xa5);
        auto length = capacity;
        *sceNetErrnoLoc() = 1234;
        Require(sceNetGetsockopt(descriptor, 41, 27, output.data(), &length) == 0);
        Require(*sceNetErrnoLoc() == 1234 && length == std::min<std::uint32_t>(capacity, sizeof(expected)));
        Require(std::memcmp(output.data(), &expected, length) == 0);
        for (auto index = length; index < output.size(); ++index) Require(output[index] == 0xa5);
    }
}

static void Exercise(int descriptor) {
    RequireOption(descriptor, 1);
    for (const int value : {0, 1, -1, std::numeric_limits<int>::min(), 0x10}) {
        *sceNetErrnoLoc() = 1234;
        Require(sceNetSetsockopt(descriptor, 41, 27, &value, sizeof(value)) == 0);
        Require(*sceNetErrnoLoc() == 1234);
        RequireOption(descriptor, value ? 1 : 0);
    }
    const int enabled = 1;
    Require(setsockopt_nid_postfix(descriptor, 41, 27, &enabled, sizeof(enabled)) == 0);
    for (std::uint32_t length = 0; length <= 8; ++length) {
        if (length == sizeof(enabled)) continue;
        Require(sceNetSetsockopt(descriptor, 41, 27, &enabled, length) == static_cast<int>(0x80410116u) && *sceNetErrnoLoc() == 22);
        RequireOption(descriptor, 1);
    }
    Require(sceNetSetsockopt(descriptor, 41, 27, nullptr, sizeof(enabled)) == static_cast<int>(0x80410116u) && *sceNetErrnoLoc() == 22);
    RequireOption(descriptor, 1);
    std::uint32_t length = sizeof(enabled);
    int output = 0;
    Require(sceNetGetsockopt(descriptor, 41, 28, &output, &length) == static_cast<int>(0x8041012au) && *sceNetErrnoLoc() == 42);
    Require(sceNetSetsockopt(descriptor, 41, 28, &enabled, sizeof(enabled)) == static_cast<int>(0x8041012au) && *sceNetErrnoLoc() == 42);
}

static void DualStack(int receiver, int type) {
    const int disabled = 0;
    Require(sceNetSetsockopt(receiver, 41, 27, &disabled, sizeof(disabled)) == 0);
    std::array<std::uint8_t, 28> address{28, 28};
    Require(sceNetBind_nid_postfix(receiver, address.data(), address.size()) == 0);
    auto length = static_cast<std::uint32_t>(address.size());
    Require(sceNetGetsockname(receiver, address.data(), &length) == 0);
    RequireOption(receiver, 0);
    const int enabled = 1;
    Require(sceNetSetsockopt(receiver, 41, 27, &enabled, sizeof(enabled)) == static_cast<int>(0x80410116u) && *sceNetErrnoLoc() == 22);
    RequireOption(receiver, 0);
    std::array<std::uint8_t, 16> destination{16, 2, address[2], address[3], 127, 0, 0, 1};
    const int sender = sceNetSocket("ipv4-peer", 2, type, 0);
    Require(sender >= 0);
    int connection = receiver;
    if (type == 1) {
        Require(sceNetListen(receiver, 1) == 0);
        Require(sceNetConnect(sender, destination.data(), destination.size()) == 0);
        connection = sceNetAccept(receiver, nullptr, nullptr);
        Require(connection >= 0);
    }
    const int timeout = 2000000;
    Require(sceNetSetsockopt(connection, 0xffff, 0x1106, &timeout, sizeof(timeout)) == 0);
    constexpr std::array<char, 4> payload{'d', 'u', 'a', 'l'};
    if (type == 1) Require(sceNetSend(sender, payload.data(), payload.size(), 0) == payload.size());
    else Require(sceNetSendto(sender, payload.data(), payload.size(), 0, destination.data(), destination.size()) == payload.size());
    std::array<char, 4> received{};
    address.fill(0);
    length = address.size();
    Require(sceNetRecvfrom(connection, received.data(), received.size(), 0, address.data(), &length) == received.size());
    Require(received == payload);
    if (type == 2) {
        Require(length == 28 && address[1] == 28);
        Require(address[18] == 0xff && address[19] == 0xff);
        Require(address[20] == 127 && address[21] == 0 && address[22] == 0 && address[23] == 1);
    }
    if (connection != receiver) Require(sceNetSocketClose(connection) == 0);
    Require(sceNetSocketClose(sender) == 0);
}

static void ExerciseGuestOutputs() {
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t bytes = page * 4;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0);
    void* view = nullptr;
    void* twin = nullptr;
    Require(sceKernelMapDirectMemory(&view, bytes, 3, 0, physical, 0) == 0);
    Require(sceKernelMapDirectMemory(&twin, bytes, 3, 0, physical, 0) == 0);
    Require(GuestArena::GuestArenaContains_nid_postfix(view, bytes));
    auto* data = static_cast<unsigned char*>(view);
    auto* output = data + page - sizeof(int);
    auto* length = reinterpret_cast<std::uint32_t*>(data + page * 3 - sizeof(std::uint32_t));
    auto* mirror = static_cast<unsigned char*>(twin) + page - sizeof(int);
    auto* mirrorLength = reinterpret_cast<std::uint32_t*>(static_cast<unsigned char*>(twin) + page * 3 - sizeof(std::uint32_t));
    {
        const GuestArena::HostWrite access(view, bytes);
        Require(access.Open());
        std::memset(view, 0xa5, bytes);
    }
    Require(sceKernelMprotect(data + page, page, 1) == 0);
    Require(sceKernelMprotect(data + page * 3, page, 1) == 0);
    for (const bool net : {true, false}) {
        const int descriptor = net ? sceNetSocket("guest-options", 28, 2, 0) : socket_nid_postfix(28, 2, 0);
        Require(descriptor >= 0);
        int* error = net ? sceNetErrnoLoc() : __error_nid_postfix();
        const auto get = [&](void* value, std::uint32_t* count) {
            return net ? sceNetGetsockopt(descriptor, 41, 27, value, count) :
                getsockopt_nid_postfix(descriptor, 41, 27, value, count);
        };
        const auto reset = [&](std::uint32_t capacity) {
            const GuestArena::HostWrite destination(output, sizeof(int));
            const GuestArena::HostWrite lengthOutput(length, sizeof(*length));
            Require(destination.Open() && lengthOutput.Open());
            std::memset(output, 0xa5, sizeof(int));
            *length = capacity;
        };
        for (const std::uint32_t capacity : {0u, 1u, 2u, 3u, 4u, 5u, 8u, UINT32_MAX}) {
            reset(capacity);
#ifdef _WIN32
            std::array<void*, 32> written{};
            std::size_t count = written.size();
            Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(view), bytes,
                written.data(), &count, true));
#endif
            *error = 1234;
            Require(get(output, length) == 0 && *error == 1234);
            const auto expected = std::min<std::uint32_t>(capacity, sizeof(int));
            const int enabled = 1;
            Require(*length == expected && *mirrorLength == expected);
            Require(std::memcmp(output, &enabled, expected) == 0 && std::memcmp(mirror, output, sizeof(int)) == 0);
            Require(std::all_of(output + expected, output + sizeof(int), [](unsigned char value) { return value == 0xa5; }));
            Require(output[-1] == 0xa5 && output[sizeof(int)] == 0xa5);
            Require(reinterpret_cast<unsigned char*>(length)[-1] == 0xa5 && reinterpret_cast<unsigned char*>(length)[sizeof(*length)] == 0xa5);
#ifdef _WIN32
            count = written.size();
            Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(view), bytes,
                written.data(), &count, true) && count != 0);
#endif
        }
        reset(0);
        Require(get(data + page, length) == 0 && *length == 0 && data[page] == 0xa5);
#ifdef _WIN32
        const int failure = net ? static_cast<int>(0x8041010eu) : -1;
        reset(4);
        Require(sceKernelMprotect(data, page, 1) == 0);
        {
            const GuestArena::HostWrite denied(output, sizeof(int));
            Require(!denied.Open());
        }
        Require(get(output, length) == failure && *error == 14);
        Require(*length == 4 && std::all_of(output, output + sizeof(int), [](unsigned char value) { return value == 0xa5; }));
        Require(sceKernelMprotect(data, page, 3) == 0);
        reset(8);
        Require(sceKernelMprotect(data + page * 2, page, 1) == 0);
        {
            const GuestArena::HostWrite denied(length, sizeof(*length));
            Require(!denied.Open());
        }
        Require(get(output, length) == failure && *error == 14);
        Require(*length == 8 && std::all_of(output, output + sizeof(int), [](unsigned char value) { return value == 0xa5; }));
        if (!net) Require(get(nullptr, length) == -1 && *error == 14 && *length == 8);
        Require(sceKernelMprotect(data + page * 2, page, 3) == 0);
#endif
        Require((net ? sceNetSocketClose(descriptor) : close_nid_postfix(descriptor)) == 0);
    }
    Require(sceKernelMprotect(view, bytes, 3) == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(twin), bytes) == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(view), bytes) == 0);
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0);
}

int main() {
    for (const int type : {1, 2}) {
        const int descriptor = sceNetSocket("ipv6-only", 28, type, 0);
        if (descriptor < 0 && (*sceNetErrnoLoc() == 43 || *sceNetErrnoLoc() == 47)) return 77;
        Require(descriptor >= 0);
        Exercise(descriptor);
        DualStack(descriptor, type);
        Require(sceNetSocketClose(descriptor) == 0);
    }
    ExerciseGuestOutputs();
    const int ipv4 = sceNetSocket("ipv4-only", 2, 2, 0);
    Require(ipv4 >= 0);
    const int value = 1;
    Require(sceNetSetsockopt(ipv4, 41, 27, &value, sizeof(value)) == static_cast<int>(0x80410116u) && *sceNetErrnoLoc() == 22);
    Require(sceNetSocketClose(ipv4) == 0);
}
