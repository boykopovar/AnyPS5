#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

extern "C" {
std::uint32_t* APS5_VABI sceAgcCbCondWrite(CommandBuffer*, std::uint32_t, std::uint32_t, const volatile void*, std::uint32_t, const volatile void*, std::uint32_t, std::uint32_t);
std::uint32_t APS5_VABI sceAgcCbCondWriteGetSize();
}

namespace {

constexpr std::uint32_t Sentinel = 0xabcdef01u;

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

const volatile void* at(std::uintptr_t address) {
    return reinterpret_cast<const volatile void*>(address);
}

struct Storage {
    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};

    Storage() { words.fill(Sentinel); }
};

void testPacket() {
    for (std::uint32_t compareFunction = 0; compareFunction <= 6; ++compareFunction) {
        Storage storage;
        const auto* packet = sceAgcCbCondWrite(&storage.buffer, compareFunction, 1, at(0x0000123456789abcu), 0xdeadbeefu, at(0x0000fedcba987654u), 0x11223344u, 0xff00ff00u);
        check(packet == storage.words.data() && packet[0] == 0xc0074500u, "COND_WRITE header mismatch");
        check(packet[1] == (0x110u | compareFunction), "COND_WRITE control mismatch");
        check(packet[2] == 0xba987654u && packet[3] == 0x0000fedcu, "COND_WRITE poll address mismatch");
        check(packet[4] == 0x11223344u && packet[5] == 0xff00ff00u, "COND_WRITE reference or mask mismatch");
        check(packet[6] == 0x56789abcu && packet[7] == 0x00001234u, "COND_WRITE write address mismatch");
        check(packet[8] == 0xdeadbeefu, "COND_WRITE data mismatch");
        check(storage.buffer.cursor_up == storage.words.data() + 9 && storage.words[9] == Sentinel, "COND_WRITE cursor advance");
    }
    check(sceAgcCbCondWriteGetSize() == 9 * sizeof(std::uint32_t), "COND_WRITE size mismatch");
}

void testInvalid() {
    Storage storage;
    const auto before = storage.words;
    const auto poll = at(0x1000u);
    const auto write = at(0x2000u);
    expectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 7, 1, write, 0, poll, 0, 0); });
    expectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 0, write, 0, poll, 0, 0); });
    expectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 2, write, 0, poll, 0, 0); });
    expectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 1, nullptr, 0, poll, 0, 0); });
    expectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 1, at(0x2002u), 0, poll, 0, 0); });
    expectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 1, write, 0, nullptr, 0, 0); });
    expectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 1, write, 0, at(0x1001u), 0, 0); });
    expectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 1, at(0x0001000000000000u), 0, poll, 0, 0); });
    expectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 1, write, 0, at(0x0001000000000000u), 0, 0); });
    check(storage.words == before && storage.buffer.cursor_up == storage.words.data(), "failed COND_WRITE modified the buffer");
}

}

int main() {
    try {
        testPacket();
        testInvalid();
        std::puts("AGC COND_WRITE tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
