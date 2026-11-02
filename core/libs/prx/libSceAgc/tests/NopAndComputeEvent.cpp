#include "prx/libSceAgc/Acb/include/Sync.hpp"
#include "prx/libSceAgc/Cb/include/Sync.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

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

void testNop() {
    Storage storage;
    storage.words.fill(0xaaaaaaaau);
    const auto* single = sceAgcCbNop_nid_postfix(&storage.buffer, 1);
    check(single == storage.words.data() && single[0] == 0xffff1000u && storage.words[1] == 0xaaaaaaaau, "one-dword NOP packet mismatch");
    check(sceAgcCbNopGetSize(1) == sizeof(std::uint32_t), "one-dword NOP size mismatch");
    const auto* padding = sceAgcCbNop_nid_postfix(&storage.buffer, 4);
    const std::array<std::uint32_t, 4> expected{0xc0021000u, 0, 0, 0};
    check(padding == storage.words.data() + 1 && std::equal(expected.begin(), expected.end(), padding), "four-dword NOP packet mismatch");
    check(storage.buffer.cursor_up == storage.words.data() + 5 && storage.words[5] == 0xaaaaaaaau, "NOP cursor advance mismatch");
    check(sceAgcCbNopGetSize(4) == expected.size() * sizeof(std::uint32_t), "four-dword NOP size mismatch");
    expectFailure(storage, [&] { sceAgcCbNop_nid_postfix(&storage.buffer, 0); }, "an empty NOP was accepted");
}

void testComputeEventWrite() {
    Storage storage;
    const auto* packet = sceAgcAcbEventWrite(&storage.buffer, 0x16, nullptr);
    check(packet == storage.words.data() && packet[0] == 0xc0004600u && packet[1] == 0x16u, "ACB EVENT_WRITE packet mismatch");
    check(sceAgcAcbEventWriteGetSize(0x16) == 2 * sizeof(std::uint32_t), "ACB EVENT_WRITE size differs from its packet");
    const auto* flush = sceAgcAcbEventWrite(&storage.buffer, 7, nullptr);
    check(flush == storage.words.data() + 2 && flush[0] == 0xc0004600u && flush[1] == 0x407u, "ACB CS_PARTIAL_FLUSH does not carry event index 4");
    check(storage.buffer.cursor_up == storage.words.data() + 4, "ACB EVENT_WRITE cursor advance mismatch");
    expectFailure(storage, [&] { sceAgcAcbEventWrite(&storage.buffer, 0x40, nullptr); }, "an ACB event type above 0x3f was accepted");
}

}

int main() {
    try {
        testNop();
        testComputeEventWrite();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
