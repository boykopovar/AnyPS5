#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

extern "C" {
std::uint32_t* APS5_VABI sceAgcDcbPushMarker(CommandBuffer*, const char*, std::uint32_t);
std::uint32_t* APS5_VABI sceAgcDcbPushMarkerSpan(CommandBuffer*, const char*, std::uint32_t, std::uint32_t);
std::uint32_t* APS5_VABI sceAgcDcbSetMarkerSpan(CommandBuffer*, const char*, std::uint32_t, std::uint32_t);
std::uint32_t* APS5_VABI sceAgcAcbPushMarkerSpan(CommandBuffer*, const char*, std::uint32_t, std::uint32_t);
std::uint32_t* APS5_VABI sceAgcAcbSetMarkerSpan(CommandBuffer*, const char*, std::uint32_t, std::uint32_t);
}

namespace {

using SpanFunction = std::uint32_t* (APS5_VABI *)(CommandBuffer*, const char*, std::uint32_t, std::uint32_t);

constexpr std::uint32_t Sentinel = 0xabcdef01u;
constexpr std::uint32_t PushHeader = 0x0bu << 2u;
constexpr std::uint32_t PopHeader = 0x0cu << 2u;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void expectFailure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return;
    }
    throw std::runtime_error("expected invalid input to fail");
}

struct Storage {
    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};

    Storage() { words.fill(Sentinel); }
};

void testPush(SpanFunction push) {
    Storage storage;
    const char text[] = "frame-and-more";
    const auto* packet = push(&storage.buffer, text, 5, 0xff0000u);
    check(packet == storage.words.data() && packet[0] == Agc::Command::Header(0x10, 3, PushHeader), "push span header mismatch");
    check(std::strcmp(reinterpret_cast<const char*>(packet + 1), "frame") == 0, "push span text is not the span");
    check(storage.buffer.cursor_up == storage.words.data() + 3 && storage.words[3] == Sentinel, "push span cursor advance");

    Storage named;
    sceAgcDcbPushMarker(&named.buffer, "frame", 0);
    check(named.words == storage.words, "push span differs from push marker");

    Storage aligned;
    const auto* exact = push(&aligned.buffer, "abcdefgh", 8, 0);
    check(exact[0] == Agc::Command::Header(0x10, 4, PushHeader) && exact[3] == 0, "span of four-byte multiple is not terminated");
    check(std::memcmp(exact + 1, "abcdefgh", 8) == 0, "aligned span text mismatch");

    Storage empty;
    const auto* none = push(&empty.buffer, nullptr, 0, 0);
    check(none[0] == Agc::Command::Header(0x10, 2, PushHeader) && none[1] == 0, "empty span mismatch");

    Storage invalid;
    const auto before = invalid.words;
    expectFailure([&] { push(&invalid.buffer, nullptr, 3, 0); });
    expectFailure([&] { push(nullptr, "frame", 5, 0); });
    check(invalid.words == before && invalid.buffer.cursor_up == invalid.words.data(), "failed push span modified the buffer");

    const std::vector<char> large(0x10000, 'a');
    std::vector<std::uint32_t> words(0x4100, Sentinel);
    CommandBuffer wide{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    expectFailure([&] { push(&wide, large.data(), 0xfffdu + 3u, 0); });
    check(wide.cursor_up == words.data(), "rejected long span advanced the buffer");
    const auto* longest = push(&wide, large.data(), 0xfffcu, 0);
    check(longest[0] == Agc::Command::Header(0x10, 0x4001, PushHeader) && wide.cursor_up == words.data() + 0x4001, "longest span mismatch");
}

void testSet(SpanFunction set) {
    Storage storage;
    const auto* packet = set(&storage.buffer, "frame-and-more", 5, 0x00ff00u);
    check(packet == storage.words.data() && packet[0] == Agc::Command::Header(0x10, 3, PushHeader), "set span push header mismatch");
    check(std::strcmp(reinterpret_cast<const char*>(packet + 1), "frame") == 0, "set span text is not the span");
    check(storage.words[3] == Agc::Command::Header(0x10, 2, PopHeader) && storage.words[4] == 0, "set span is not followed by a pop");
    check(storage.buffer.cursor_up == storage.words.data() + 5, "set span cursor advance");
    expectFailure([&] { set(&storage.buffer, nullptr, 1, 0); });
}

}

int main() {
    try {
        testPush(sceAgcDcbPushMarkerSpan);
        testPush(sceAgcAcbPushMarkerSpan);
        testSet(sceAgcDcbSetMarkerSpan);
        testSet(sceAgcAcbSetMarkerSpan);
        std::puts("AGC marker span tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
