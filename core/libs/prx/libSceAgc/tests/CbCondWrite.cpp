#include "prx/libSceAgc/Cb/include/Branch.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

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

void testConsoleArguments() {
    struct Case {
        std::uint32_t compareFunction;
        std::uint32_t writeSpace;
        std::uintptr_t writeAddress;
        std::uint32_t writeData;
        std::uintptr_t pollAddress;
        std::array<std::uint32_t, 9> packet;
    };
    const Case cases[] = {
        {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u, 0x55555555u, {0xc0074500u, 0x211u, 0x55555555u, 0, 0, 1, 0x33333333u, 0, 0x44444444u}},
        {1, 2, 3, 4, 5, {0xc0074500u, 0x211u, 5, 0, 0, 1, 3, 0, 4}},
        {0x22222222u, 0x44444444u, 0, 0, 0, {0xc0074500u, 0x12u, 0, 0, 0, 1, 0, 0, 0}},
    };
    for (const Case& entry : cases) {
        Storage storage;
        const auto* packet = sceAgcCbCondWrite(&storage.buffer, entry.compareFunction, entry.writeSpace, at(entry.writeAddress), entry.writeData, at(entry.pollAddress), 0, 1);
        check(packet == storage.words.data(), "console COND_WRITE arguments were refused");
        for (std::size_t index = 0; index < entry.packet.size(); ++index) check(packet[index] == entry.packet[index], "console COND_WRITE packet mismatch");
        check(storage.buffer.cursor_up == storage.words.data() + 9 && storage.words[9] == Sentinel, "console COND_WRITE cursor advance");
    }
}

void testInvalid() {
    Storage storage;
    const auto before = storage.words;
    const auto poll = at(0x1000u);
    const auto write = at(0x2000u);
    expectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 1, at(0x0001000000000000u), 0, poll, 0, 0); });
    expectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 1, write, 0, at(0x0001000000000000u), 0, 0); });
    check(storage.words == before && storage.buffer.cursor_up == storage.words.data(), "failed COND_WRITE modified the buffer");
}
}

int main() {
    try {
        testPacket();
        testConsoleArguments();
        testInvalid();
        std::puts("AGC COND_WRITE tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
