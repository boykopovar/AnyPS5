#include "prx/libSceAgc/Misc/include/PacketInfo.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <tuple>

extern "C" int APS5_VABI sceAgcGetDataPacketPayloadRange(SceAgcMemoryRange* range, std::uint32_t* cmd, int type);
extern "C" int APS5_VABI sceAgcGetDataPacketPayloadAddress_0090(std::uint32_t** addr, std::uint32_t* cmd, int type);

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

void testRanges() {
    const std::array<std::tuple<std::uint32_t, int, std::ptrdiff_t, std::uint64_t>, 10> rows{{
        {0xc0047600u, 1, 2, 16}, {0xc0001000u, 1, 2, 0}, {0xffff7600u, 1, 2, 0xfffc}, {0xc0047600u, -1, 2, 16},
        {0xc0047600u, 7, 2, 16}, {0xc0021000u, 0, 1, 12}, {0xc0001000u, 0, 1, 4}, {0xfffe1000u, 0, 1, 0xfffc},
        {0xffff1000u, 0, -1, 0}, {0xffff7600u, 0, -1, 0}}};
    for (const auto& [header, type, offset, size] : rows) {
        std::array<std::uint32_t, 2> words{header, 0x12345678u};
        SceAgcMemoryRange range{reinterpret_cast<void*>(std::uintptr_t{0x1000}), 0xdeadu};
        check(sceAgcGetDataPacketPayloadRange(&range, words.data(), type) == 0, "payload range failed");
        void* expected = offset < 0 ? nullptr : static_cast<void*>(words.data() + offset);
        check(range.base == expected, "payload range base mismatch");
        check(range.size == size, "payload range size mismatch");
        check(words[0] == header && words[1] == 0x12345678u, "payload range query modified the packet");
        std::uint32_t* address = nullptr;
        check(sceAgcGetDataPacketPayloadAddress_0090(&address, words.data(), type) == 0 && address == range.base,
              "payload range base differs from the payload address");
    }
}

void testRejections() {
    std::array<std::uint32_t, 2> words{0xc0047600u, 0u};
    SceAgcMemoryRange range{};
    expectFailure([&] { sceAgcGetDataPacketPayloadRange(nullptr, words.data(), 1); });
    expectFailure([&] { sceAgcGetDataPacketPayloadRange(&range, nullptr, 1); });
    auto* misaligned = reinterpret_cast<std::uint32_t*>(reinterpret_cast<unsigned char*>(words.data()) + 1);
    expectFailure([&] { sceAgcGetDataPacketPayloadRange(&range, misaligned, 0); });
    std::array<unsigned char, sizeof(SceAgcMemoryRange) + 8> storage{};
    auto* misalignedRange = reinterpret_cast<SceAgcMemoryRange*>(storage.data() + 4);
    if (reinterpret_cast<std::uintptr_t>(misalignedRange) % alignof(SceAgcMemoryRange) == 0) {
        misalignedRange = reinterpret_cast<SceAgcMemoryRange*>(storage.data() + 2);
    }
    expectFailure([&] { sceAgcGetDataPacketPayloadRange(misalignedRange, words.data(), 1); });
}

}

int main() {
    try {
        testRanges();
        testRejections();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC payload range tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
