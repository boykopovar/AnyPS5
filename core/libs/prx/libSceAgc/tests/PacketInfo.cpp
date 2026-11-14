#include "prx/libSceAgc/Misc/include/PacketInfo.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <utility>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void expectFailure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        check(error.what()[0] != '\0', "empty exception message");
        return;
    }
    throw std::runtime_error("expected an exception");
}

void testPacketSize() {
    const std::array<std::pair<std::uint32_t, std::uint32_t>, 7> sizes{{
        {0xc0047600u, 6}, {0xc0001000u, 2}, {0xfffe1000u, 0x4000}, {0xffff7600u, 0x4001},
        {0xffff1000u, 1}, {0xffff1001u, 1}, {0xffff10fcu, 1}}};
    for (const auto& [header, size] : sizes) {
        std::array<std::uint32_t, 2> words{header, 0xffff1000u};
        check(sceAgcGetPacketSize(words.data()) == size, "packet size mismatch");
        check(words[0] == header && words[1] == 0xffff1000u, "packet size query modified the packet");
    }
}



void testSystemSoftwareVersion() {
    std::array<std::uint32_t, 3> packet{0xc0020000u, 0xbeefcafeu, 0};
    std::uint32_t version = 0;
    check(sceAgcDcbGetSystemSoftwareVersion(&version, packet.data()) == 0, "assigned system software version");
    check(version == 0xbeefcafeu, "system software version uses the second packet word");
    check(packet[0] == 0xc0020000u && packet[1] == 0xbeefcafeu, "query does not modify the packet");

    std::array<std::uint32_t, 2> tooShort{0xc0000000u, 0xbeefcafeu};
    expectFailure([&] { sceAgcDcbGetSystemSoftwareVersion(&version, tooShort.data()); });
    expectFailure([&] { sceAgcDcbGetSystemSoftwareVersion(nullptr, packet.data()); });
    expectFailure([&] { sceAgcDcbGetSystemSoftwareVersion(&version, nullptr); });
    expectFailure([&] { sceAgcDcbGetSystemSoftwareVersion(reinterpret_cast<std::uint32_t*>(std::uintptr_t{0x10000}), packet.data()); });
    for (const auto header : {0x80000000u, 0x3fff1000u, 0x40001000u, 0x00027600u}) {
        std::array<std::uint32_t, 3> invalidHeader{header, 0xbeefcafeu, 0};
        expectFailure([&] { sceAgcDcbGetSystemSoftwareVersion(&version, invalidHeader.data()); });
    }
}

void testDataPacketPayloadAddress() {
    std::array<std::uint32_t, 4> packet{0xc0020000u, 0x11111111u, 0x22222222u, 0x33333333u};
    std::uint32_t* addr = nullptr;
    check(sceAgcGetDataPacketPayloadAddress_0090(&addr, packet.data(), 1) == 0, "type 1 query succeeded");
    check(addr == packet.data() + 2, "type 1 payload address points to word 2");
    addr = nullptr;
    check(sceAgcGetDataPacketPayloadAddress(&addr, packet.data(), 1) == 0, "alias query succeeded");
    check(addr == packet.data() + 2, "alias payload address matches versioned");

    addr = nullptr;
    check(sceAgcGetDataPacketPayloadAddress(&addr, packet.data(), 0) == 0, "type 0 query succeeded");
    check(addr == packet.data() + 1, "type 0 payload address points to word 1 when mask nonzero");

    std::array<std::uint32_t, 2> zeroMaskPacket{0xffff0000u, 0x11111111u};
    addr = packet.data();
    check(sceAgcGetDataPacketPayloadAddress(&addr, zeroMaskPacket.data(), 0) == 0, "zero mask query succeeded");
    check(addr == nullptr, "zero mask type 0 payload address is null");

    expectFailure([&] { sceAgcGetDataPacketPayloadAddress(nullptr, packet.data(), 1); });
    expectFailure([&] { sceAgcGetDataPacketPayloadAddress(&addr, nullptr, 1); });
    expectFailure([&] { sceAgcGetDataPacketPayloadAddress(reinterpret_cast<std::uint32_t**>(std::uintptr_t{0x10000}), packet.data(), 1); });
    expectFailure([&] { sceAgcGetDataPacketPayloadAddress_0090(reinterpret_cast<std::uint32_t**>(std::uintptr_t{0x10000}), packet.data(), 1); });
}

void testDataPacketPayloadRange() {
    std::array<std::uint32_t, 4> packet{0xc0040000u, 0x11111111u, 0x22222222u, 0x33333333u};
    SceAgcMemoryRange range{};
    check(sceAgcGetDataPacketPayloadRange(&range, packet.data(), 1) == 0, "type 1 payload range query succeeded");
    check(range.base == packet.data() + 2 && range.size == ((packet[0] >> 14u) & 0xfffcu), "type 1 payload range populated");

    range = {};
    check(sceAgcGetDataPacketPayloadRange(&range, packet.data(), 0) == 0, "type 0 payload range query succeeded");
    check(range.base == packet.data() + 1 && range.size == (((packet[0] >> 14u) & 0xfffcu) + sizeof(std::uint32_t)), "type 0 payload range populated");

    std::array<std::uint32_t, 2> zeroMaskPacket{0xffff0000u, 0x11111111u};
    range = {packet.data(), 128};
    check(sceAgcGetDataPacketPayloadRange(&range, zeroMaskPacket.data(), 0) == 0, "zero mask payload range query succeeded");
    check(range.base == nullptr && range.size == 0, "zero mask payload range empty");

    expectFailure([&] { sceAgcGetDataPacketPayloadRange(nullptr, packet.data(), 1); });
    expectFailure([&] { sceAgcGetDataPacketPayloadRange(&range, nullptr, 1); });
    expectFailure([&] { sceAgcGetDataPacketPayloadRange(reinterpret_cast<SceAgcMemoryRange*>(std::uintptr_t{0x10000}), packet.data(), 1); });
}

void testRejections() {
    for (const auto header : {0x80000000u, 0x3fff1000u, 0x40001000u, 0x00047600u}) {
        std::uint32_t word = header;
        expectFailure([&] { sceAgcGetPacketSize(&word); });
    }
    expectFailure([] { sceAgcGetPacketSize(nullptr); });
    std::array<std::uint32_t, 2> words{0x00100000u, 0x000000c0u};
    auto* misaligned = reinterpret_cast<std::uint32_t*>(reinterpret_cast<unsigned char*>(words.data()) + 1);
    expectFailure([&] { sceAgcGetPacketSize(misaligned); });
}

}

int main() {
    try {
        testPacketSize();
        testRejections();
        testSystemSoftwareVersion();
        testDataPacketPayloadAddress();
        testDataPacketPayloadRange();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC packet info tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
