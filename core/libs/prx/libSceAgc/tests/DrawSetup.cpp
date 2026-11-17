#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgc/DcbDraw/include/DrawIndexed.hpp"
#include "prx/libSceAgc/DcbDraw/include/DrawNonIndexed.hpp"
#include "prx/libSceAgc/DcbDraw/include/IndexBuffer.hpp"
#include "prx/libSceAgc/DcbDraw/include/Instancing.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

struct Storage {
    std::array<std::uint32_t, 64> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
};

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void expectFailure(Storage& storage, TAction action, const char* message) {
    const auto before = storage.words;
    const auto* cursor = storage.buffer.cursor_up;
    try {
        action();
    } catch (const std::runtime_error&) {
        check(storage.words == before && storage.buffer.cursor_up == cursor, message);
        return;
    }
    throw std::runtime_error(message);
}

template <std::size_t N>
void expectPacket(const Storage& storage, const std::uint32_t* packet, const std::uint32_t* start, const std::array<std::uint32_t, N>& expected, std::uint32_t size, const char* message) {
    check(packet == start && std::equal(expected.begin(), expected.end(), packet), message);
    check(storage.buffer.cursor_up == start + N, message);
    check(size == N * sizeof(std::uint32_t), message);
}

void testNumInstances() {
    Storage storage;
    const auto* packet = sceAgcDcbSetNumInstances(&storage.buffer, 7);
    expectPacket(storage, packet, storage.words.data(), std::array<std::uint32_t, 2>{0xc0002f00u, 7}, sceAgcDcbSetNumInstancesGetSize(), "NUM_INSTANCES packet mismatch");
}

void testIndexSize() {
    Storage storage;
    const auto* packet = sceAgcDcbSetIndexSize(&storage.buffer, 1, 2);
    expectPacket(storage, packet, storage.words.data(), std::array<std::uint32_t, 3>{0xc0017a00u, 0x20000243u, 0x481u}, sceAgcDcbSetIndexSizeGetSize(), "index size packet mismatch");
    const auto* objectId = sceAgcDcbSetIndexSize_0600(&storage.buffer, 2, 3, 1);
    check(objectId == storage.words.data() + 3 && objectId[0] == 0xc0017a00u && objectId[1] == 0x20000243u && objectId[2] == (0x4c2u | 0x4000u), "index size packet with a per-instance object id mismatch");
    expectFailure(storage, [&] { sceAgcDcbSetIndexSize(&storage.buffer, 3, 0); }, "an index size above 2 was accepted");
    expectFailure(storage, [&] { sceAgcDcbSetIndexSize(&storage.buffer, 0, 4); }, "a cache policy above 3 was accepted");
    expectFailure(storage, [&] { sceAgcDcbSetIndexSize_0600(&storage.buffer, 0, 0, 2); }, "a per-instance object id above 1 was accepted");
}

void testDrawIndexOffset() {
    Storage storage;
    const auto* packet = sceAgcDcbDrawIndexOffset(&storage.buffer, 12, 36, 0);
    expectPacket(storage, packet, storage.words.data(), std::array<std::uint32_t, 5>{0xc0033500u, 36, 12, 36, 0}, sceAgcDcbDrawIndexOffsetGetSize(), "DRAW_INDEX_OFFSET_2 packet mismatch");
    const auto* empty = sceAgcDcbDrawIndexOffset(&storage.buffer, 0, 0, 0);
    check(empty == storage.words.data() + 5 && empty[1] == 1 && empty[3] == 0, "an empty indexed offset draw did not keep a maximum size of one");
}

void testDrawIndirect() {
    Storage storage;
    const auto* packet = sceAgcDcbDrawIndirect(&storage.buffer, 0x40, 0);
    expectPacket(storage, packet, storage.words.data(), std::array<std::uint32_t, 5>{0xc0032400u, 0x40, 0x280, 0x280, 2}, sceAgcDcbDrawIndirectGetSize(), "DRAW_INDIRECT packet mismatch");
    expectFailure(storage, [&] { sceAgcDcbDrawIndirect(&storage.buffer, 2, 0); }, "a misaligned indirect argument offset was accepted");
}

}

int main() {
    try {
        testNumInstances();
        testIndexSize();
        testDrawIndexOffset();
        testDrawIndirect();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
