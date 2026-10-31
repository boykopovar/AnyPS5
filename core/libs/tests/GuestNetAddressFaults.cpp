#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceNet/include/Net.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

static void Failed(int result, int error) {
    Require(result == static_cast<int>(0x80410100u | static_cast<unsigned>(error)));
    Require(*sceNetErrnoLoc() == error);
}

int main(int argc, char** argv) {
    Require(argc == 3);
    const bool connect = std::strcmp(argv[1], "connect") == 0;
    const int readable = std::atoi(argv[2]);
    constexpr std::size_t page = 16384;
    Require(sceNetInit_nid_postfix() == 0);
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &physical) == 0);
    void* mapping = nullptr;
    Require(sceKernelMapDirectMemory(&mapping, page * 2, 3, 0, physical, 0) == 0);
    for (const int family : {2, 28}) {
        const std::uint32_t size = family == 2 ? 16 : 28;
        std::array<std::uint8_t, 28> address{};
        address[0] = size;
        address[1] = family;
        if (family == 2) {
            address[4] = 127;
            address[7] = 1;
        } else {
            address[23] = 1;
        }
        const int peer = sceNetSocket("address-peer", family, 2, 17);
        const int socket = sceNetSocket("address-fault", family, 2, 17);
        Require(peer >= 0 && socket >= 0);
        Require(sceNetBind_nid_postfix(peer, address.data(), size) == 0);
        std::uint32_t actual = size;
        Require(sceNetGetsockname(peer, address.data(), &actual) == 0);
        const auto call = [&](int descriptor, const void* pointer, std::uint32_t length) {
            return connect ? sceNetConnect(descriptor, pointer, length) : sceNetBind_nid_postfix(descriptor, pointer, length);
        };
        if (connect) Require(call(socket, address.data(), size) == 0);
        auto* boundary = static_cast<std::uint8_t*>(mapping) + page;
        const int prefix = readable == 8 ? size - 1 : readable;
        auto* fault = boundary - prefix;
        std::memcpy(fault, address.data(), size);
        Require(sceKernelMprotect(boundary, page, 0) == 0);
        Failed(call(-1, fault, size), 9);
        Failed(call(socket, nullptr, size), 22);
        Failed(call(socket, fault, 0), 22);
        Failed(call(socket, fault, 1), 22);
        Failed(call(socket, fault, size), 14);
        if (connect) {
            if (prefix >= 2) {
                fault[1] = 0;
                Failed(call(socket, fault, size), 14);
            }
            std::array<std::uint8_t, 28> connected{};
            actual = size;
            Require(sceNetGetpeername(socket, connected.data(), &actual) == 0);
            Require(std::memcmp(connected.data(), address.data(), size) == 0);
        }
        Require(sceKernelMprotect(boundary, page, 3) == 0);
        if (!connect) {
            address[2] = 0;
            address[3] = 0;
        }
        Require(call(socket, address.data(), size) == 0);
        Require(sceNetSocketClose(socket) == 0);
        Require(sceNetSocketClose(peer) == 0);
    }
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), page * 2) == 0);
    Require(sceKernelReleaseDirectMemory(physical, page * 2) == 0);
}
