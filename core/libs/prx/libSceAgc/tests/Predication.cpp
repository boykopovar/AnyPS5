#include "prx/libSceAgc/Predication/include/Predication.hpp"
#include "prx/libSceAgc/DcbState/include/Predication.hpp"
#include "prx/libSceAgc/DcbDraw/include/IndexBuffer.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "Testing.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

struct Storage {
    std::array<std::uint32_t, 32> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
};

void testSizes() {
    Storage storage;
    alignas(16) static std::uint64_t query = 0;
    auto* packet = sceAgcDcbSetPredication(&storage.buffer, 1, 1, 0, &query, 0);
    const auto bytes = static_cast<std::uint32_t>((storage.buffer.cursor_up - packet) * sizeof(std::uint32_t));
    Require(bytes == 16, "SET_PREDICATION is not four dwords");
    Require(sceAgcDcbSetZPassPredicationEnableGetSize() == bytes, "z-pass predication size");
    Require(sceAgcDcbSetPredicationDisableGetSize() == bytes, "predication disable size");
    Require(sceAgcDcbSetBoolPredicationEnableGetSize() == bytes, "bool predication size");
}

void testConsoleArguments() {
    struct Case {
        std::uint8_t condition;
        std::uint8_t operation;
        std::uint8_t waitOperation;
        std::uintptr_t address;
        std::uint32_t count;
        std::array<std::uint32_t, 4> words;
    };
    const std::array<Case, 3> cases{{
        {0x11, 0x22, 0x33, 0x44444444u, 0x55555555u, {0xc0022000u, 0x00021100u, 0x44444440u, 0u}},
        {1, 2, 3, 4u, 5u, {0xc0022000u, 0x00021100u, 0u, 0u}},
        {0x22, 0x44, 0, 0u, 0u, {0xc0022000u, 0x00040000u, 0u, 0u}},
    }};
    for (const auto& item : cases) {
        Storage storage;
        const auto* packet = sceAgcDcbSetPredication(&storage.buffer, item.condition, item.operation, item.waitOperation, reinterpret_cast<const volatile void*>(item.address), item.count);
        Require(packet == storage.words.data() && std::equal(item.words.begin(), item.words.end(), packet), "SET_PREDICATION does not match the console's packet");
        Require(storage.buffer.cursor_up == storage.words.data() + item.words.size(), "SET_PREDICATION cursor mismatch");
    }
}

void testPacket() {
    Storage storage;
    auto* packet = sceAgcDcbSetIndexCount(&storage.buffer, 7);
    const auto header = packet[0];
    Require(sceAgcSetPacketPredication(packet, 1) == 0 && packet[0] == (header | 1u) && packet[1] == 7, "predication bit not set");
    Require(sceAgcSetPacketPredication(packet, 0) == 0 && packet[0] == header, "predication bit not cleared");
    ExpectErr<std::runtime_error>([&] { sceAgcSetPacketPredication(packet, 2); });
    ExpectErr<std::runtime_error>([&] { sceAgcSetPacketPredication(nullptr, 1); });
    std::uint32_t filler = 0x80000000u;
    ExpectErr<std::runtime_error>([&] { sceAgcSetPacketPredication(&filler, 1); });
    Require(packet[0] == header && filler == 0x80000000u, "rejected calls modified a packet");
}

void testRange() {
    Storage storage;
    auto* first = sceAgcDcbSetIndexCount(&storage.buffer, 3);
    *storage.buffer.cursor_up++ = 0x80000000u;
    *storage.buffer.cursor_up++ = 0xffff1000u;
    auto* second = sceAgcDcbSetIndexBuffer(&storage.buffer, 0x1000);
    auto* third = sceAgcDcbSetIndexCount(&storage.buffer, 5);
    const std::array headers{first[0], second[0], third[0]};
    Require(sceAgcSetRangePredication(first, storage.buffer.cursor_up, 1) == 0, "range predication failed");
    Require(first[0] == (headers[0] | 1u) && second[0] == (headers[1] | 1u) && third[0] == (headers[2] | 1u), "range did not predicate every packet");
    Require(first[2] == 0x80000000u && first[3] == 0xffff1000u && second[1] == 0x1000u && third[1] == 5u, "range predication changed a payload, filler or pad");
    Require(sceAgcSetRangePredication(second, third, 0) == 0 && second[0] == headers[1] && third[0] == (headers[2] | 1u), "range end is not exclusive");
    Require(sceAgcSetRangePredication(first, first, 0) == 0 && first[0] == (headers[0] | 1u), "empty range changed a packet");
    ExpectErr<std::runtime_error>([&] { sceAgcSetRangePredication(first, first + 1, 1); });
    ExpectErr<std::runtime_error>([&] { sceAgcSetRangePredication(second, first, 1); });
    ExpectErr<std::runtime_error>([&] { sceAgcSetRangePredication(first, third, 2); });
    std::array<std::uint32_t, 2> invalid{0x40000000u, 0};
    ExpectErr<std::runtime_error>([&] { sceAgcSetRangePredication(invalid.data(), invalid.data() + invalid.size(), 1); });
}

}

int main() {
    try {
        testSizes();
        testConsoleArguments();
        testPacket();
        testRange();
        std::puts("agc predication tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
