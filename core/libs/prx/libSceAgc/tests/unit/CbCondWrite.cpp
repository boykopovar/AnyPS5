#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>

extern "C" {
std::uint32_t* APS5_VABI sceAgcCbCondWrite(CommandBuffer*, std::uint32_t, std::uint32_t, const volatile void*, std::uint32_t, const volatile void*, std::uint32_t, std::uint32_t);
std::uint32_t APS5_VABI sceAgcCbCondWriteGetSize();
}

namespace {

constexpr std::uint32_t Sentinel = 0xabcdef01u;

using Testing::Require;

template<typename TAction>
void ExpectFailure(TAction action) {
    Testing::RequireThrows<std::runtime_error>(action, "expected invalid input to fail");
}

const volatile void* at(std::uintptr_t address) {
    return reinterpret_cast<const volatile void*>(address);
}

struct Storage {
    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};

    Storage() { words.fill(Sentinel); }
};

void VerifyPacket() {
    for (std::uint32_t compareFunction = 0; compareFunction <= 6; ++compareFunction) {
        Storage storage;
        const auto* packet = sceAgcCbCondWrite(&storage.buffer, compareFunction, 1, at(0x0000123456789abcu), 0xdeadbeefu, at(0x0000fedcba987654u), 0x11223344u, 0xff00ff00u);
        Require(packet == storage.words.data() && packet[0] == 0xc0074500u, "COND_WRITE header mismatch");
        Require(packet[1] == (0x110u | compareFunction), "COND_WRITE control mismatch");
        Require(packet[2] == 0xba987654u && packet[3] == 0x0000fedcu, "COND_WRITE poll address mismatch");
        Require(packet[4] == 0x11223344u && packet[5] == 0xff00ff00u, "COND_WRITE reference or mask mismatch");
        Require(packet[6] == 0x56789abcu && packet[7] == 0x00001234u, "COND_WRITE write address mismatch");
        Require(packet[8] == 0xdeadbeefu, "COND_WRITE data mismatch");
        Require(storage.buffer.cursor_up == storage.words.data() + 9 && storage.words[9] == Sentinel, "COND_WRITE cursor advance");
    }
    Require(sceAgcCbCondWriteGetSize() == 9 * sizeof(std::uint32_t), "COND_WRITE size mismatch");
}

void VerifyInvalid() {
    Storage storage;
    const auto before = storage.words;
    const auto poll = at(0x1000u);
    const auto write = at(0x2000u);
    ExpectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 7, 1, write, 0, poll, 0, 0); });
    ExpectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 0, write, 0, poll, 0, 0); });
    ExpectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 2, write, 0, poll, 0, 0); });
    ExpectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 1, nullptr, 0, poll, 0, 0); });
    ExpectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 1, at(0x2002u), 0, poll, 0, 0); });
    ExpectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 1, write, 0, nullptr, 0, 0); });
    ExpectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 1, write, 0, at(0x1001u), 0, 0); });
    ExpectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 1, at(0x0001000000000000u), 0, poll, 0, 0); });
    ExpectFailure([&] { sceAgcCbCondWrite(&storage.buffer, 3, 1, write, 0, at(0x0001000000000000u), 0, 0); });
    Require(storage.words == before && storage.buffer.cursor_up == storage.words.data(), "failed COND_WRITE modified the buffer");
}

}

namespace {

const Testing::Case packet{"CbCondWrite_EveryCompareFunction_WritesPacket", [] {
    VerifyPacket();
}};

const Testing::Case invalid{"CbCondWrite_InvalidArguments_RejectsWithoutWriting", [] {
    VerifyInvalid();
}};

} // namespace
