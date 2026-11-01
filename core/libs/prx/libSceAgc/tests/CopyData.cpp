#include "prx/libSceAgc/Acb/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgc/DcbFlow/include/Memory.hpp"

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

void testDrawQueueCopy() {
    Storage storage;
    const auto* packet = sceAgcDcbCopyData(&storage.buffer, 2, 1, 0x123456789ab0ull, 5, 2, 0xdeadbeef00000010ull, 1, 1);
    const std::array<std::uint32_t, 6> expected{0xc0044000u, 0x42114102u, 0x10u, 0xdeadbeefu, 0x56789ab0u, 0x1234u};
    check(packet == storage.words.data() && std::equal(expected.begin(), expected.end(), packet), "DCB COPY_DATA packet mismatch");
    check(storage.buffer.cursor_up == storage.words.data() + expected.size(), "DCB COPY_DATA cursor advance mismatch");
    check(sceAgcDcbCopyDataGetSize() == expected.size() * sizeof(std::uint32_t), "DCB COPY_DATA size differs from its packet");
    expectFailure(storage, [&] { sceAgcDcbCopyData(&storage.buffer, 2, 4, 0x1000, 4, 0, 0x2000, 0, 0); }, "a DCB destination cache policy above 3 was accepted");
    expectFailure(storage, [&] { sceAgcDcbCopyData(&storage.buffer, 2, 0, 0x1000, 4, 0, 0x2000, 2, 0); }, "a DCB item size above 1 was accepted");
    expectFailure(storage, [&] { sceAgcDcbCopyData(&storage.buffer, 2, 0, 0x1000, 10, 0, 0x100000000ull, 0, 0); }, "a DCB 32-bit immediate wider than 32 bits was accepted");
}

void testComputeQueueCopy() {
    Storage storage;
    const auto* packet = sceAgcAcbCopyData(&storage.buffer, 1, 0, 0x1000, 5, 0, 0x1234, 0, 0);
    const std::array<std::uint32_t, 6> expected{0xc0044000u, 0x105u, 0x1234u, 0, 0x1000u, 0};
    check(packet == storage.words.data() && std::equal(expected.begin(), expected.end(), packet), "ACB COPY_DATA packet mismatch");
    check(storage.buffer.cursor_up == storage.words.data() + expected.size(), "ACB COPY_DATA cursor advance mismatch");
    check(sceAgcAcbCopyDataGetSize() == expected.size() * sizeof(std::uint32_t), "ACB COPY_DATA size differs from its packet");
    const auto* wide = sceAgcAcbCopyData(&storage.buffer, 1, 0, 0x1000, 5, 0, 0x100000000ull, 1, 0);
    check(wide == storage.words.data() + expected.size() && wide[1] == 0x10105u && wide[2] == 0 && wide[3] == 1, "ACB 64-bit immediate COPY_DATA packet mismatch");
    expectFailure(storage, [&] { sceAgcAcbCopyData(&storage.buffer, 1, 0, 0x1000, 5, 0, 0x100000000ull, 0, 0); }, "an ACB 32-bit immediate wider than 32 bits was accepted");
    expectFailure(storage, [&] { sceAgcAcbCopyData(&storage.buffer, 1, 0, 0x1000, 5, 0, 0, 0, 2); }, "an ACB write confirm above 1 was accepted");
}

void testWriteDataSize() {
    Storage storage;
    const std::array<std::uint32_t, 3> payload{1, 2, 3};
    const auto* packet = sceAgcDcbWriteData(&storage.buffer, 2, 0, 0x2000, payload.data(), static_cast<std::uint32_t>(payload.size()), 1, 0);
    const std::array<std::uint32_t, 7> expected{0xc0053700u, 0x10100u, 0x2000u, 0, 1, 2, 3};
    check(packet == storage.words.data() && std::equal(expected.begin(), expected.end(), packet), "DCB WRITE_DATA packet mismatch");
    check(sceAgcDcbWriteDataGetSize(static_cast<std::uint32_t>(payload.size())) == expected.size() * sizeof(std::uint32_t), "DCB WRITE_DATA size differs from its packet");
    check(sceAgcDcbWriteDataGetSize(1) == 20 && sceAgcDcbWriteDataGetSize(0x3ffd) == 0x3ffd * 4 + 16, "DCB WRITE_DATA size is not four header words plus the payload");
}

}

int main() {
    try {
        testDrawQueueCopy();
        testComputeQueueCopy();
        testWriteDataSize();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
