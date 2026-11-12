#include "prx/libSceAgcVsh/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

constexpr std::uint32_t Sentinel = 0xabcdef01u;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::array<std::uint32_t, 39> Expected{
    0xc0032200u, 0xf8110000u, 0x0000000fu, 0x00000000u, 0x22000000u,
    0xc0001200u, 0x00000002u,
    0xc0033700u, 0x038f048eu, 0x070cc102u, 0x00000008u, 0x00000000u,
    0xc0044000u, 0x038f048eu, 0x0c02b303u, 0x0c410807u, 0x00001006u, 0x0000000fu,
    0xc0044000u, 0x100e4100u, 0x0d430286u, 0x07834d06u, 0x058d068cu, 0x0000000fu,
    0xc0044000u, 0x0000002cu, 0x00000624u, 0xffff0458u, 0x000002ccu, 0x0000000fu,
    0xc0071e00u, 0x100e4100u, 0x0d430286u, 0x07834d06u, 0x058d068cu, 0x038f048eu, 0x0c02a803u, 0x0c410807u, 0x00001006u,
};

void testNullBuffer() {
    check(sceAgcVshUnknown_Aozdh0bCcO0(nullptr) == nullptr, "null buffer did not return nullptr");
}

void testStream() {
    std::array<std::uint32_t, 64> words{};
    words.fill(Sentinel);
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    auto* packet = sceAgcVshUnknown_Aozdh0bCcO0(&buffer);
    check(packet == words.data(), "the stream did not start at the cursor");
    check(std::equal(Expected.begin(), Expected.end(), packet), "incorrect Vsh packet stream");
    check(buffer.cursor_up == words.data() + Expected.size(), "the cursor did not advance by the stream size");
    check(std::all_of(words.begin() + Expected.size(), words.end(), [](std::uint32_t word) { return word == Sentinel; }), "the stream wrote past its thirty-nine words");
}

bool APS5_VABI growInto(CommandBuffer* buffer, std::uint32_t count, void* userData) {
    auto& grown = *static_cast<std::array<std::uint32_t, 64>*>(userData);
    check(count == 39, "incorrect stream allocation request");
    buffer->bottom = grown.data();
    buffer->top = grown.data() + grown.size();
    buffer->cursor_up = grown.data();
    buffer->cursor_down = grown.data() + grown.size();
    return true;
}

bool APS5_VABI growFail(CommandBuffer*, std::uint32_t, void*) {
    return false;
}

void testGrow() {
    std::array<std::uint32_t, 8> small{};
    std::array<std::uint32_t, 64> large{};
    small.fill(Sentinel);
    large.fill(Sentinel);
    CommandBuffer buffer{small.data(), small.data() + small.size(), small.data(), small.data() + small.size(), growInto, &large, 0};
    auto* packet = sceAgcVshUnknown_Aozdh0bCcO0(&buffer);
    check(packet == large.data(), "the grown stream did not start at the new cursor");
    check(std::equal(Expected.begin(), Expected.end(), packet), "incorrect Vsh packet stream after growth");
    check(buffer.cursor_up == large.data() + Expected.size(), "the cursor did not advance after growth");
}

void testGrowFailure() {
    std::array<std::uint32_t, 8> small{};
    small.fill(Sentinel);
    CommandBuffer buffer{small.data(), small.data() + small.size(), small.data(), small.data() + small.size(), growFail, nullptr, 0};
    auto* packet = sceAgcVshUnknown_Aozdh0bCcO0(&buffer);
    check(packet == nullptr, "a failed grow did not return nullptr");
    check(buffer.cursor_up == small.data(), "a failed grow still advanced the cursor");
    check(std::all_of(small.begin(), small.end(), [](std::uint32_t word) { return word == Sentinel; }), "a failed grow still wrote");
}

}

int main() {
    try {
        testNullBuffer();
        testStream();
        testGrow();
        testGrowFailure();
        std::puts("AGC Vsh unknown packet writer tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
