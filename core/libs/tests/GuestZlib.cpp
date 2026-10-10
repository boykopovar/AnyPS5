#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
std::int32_t APS5_VABI sceZlibInitialize(const void*, std::uint64_t);
std::int32_t APS5_VABI sceZlibFinalize();
std::int32_t APS5_VABI sceZlibInflate(const void*, std::uint32_t, void*, std::uint32_t, std::uint64_t*);
std::int32_t APS5_VABI sceZlibWaitForDone(std::uint64_t*, std::uint32_t*);
std::int32_t APS5_VABI sceZlibGetResult(std::uint64_t, std::uint32_t*, std::int32_t*);
}

static void Require(bool value) { if (!value) std::abort(); }

template<typename TAction>
static void RequireThrows(TAction action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    std::abort();
}

int main() {
    constexpr std::array<std::uint8_t, 77> compressed{0x78, 0x9c, 0xed, 0xca, 0xb1, 0xd, 0x80, 0x20, 0x10, 0x40, 0xd1, 0x9e, 0x29, 0x6e, 0x2, 0x3a, 0x6, 0x60, 0x3, 0x12, 0x26, 0x10, 0x30, 0x91, 0x88, 0x47, 0x71, 0xa1, 0xd0, 0xe9, 0x1d, 0x82, 0xf6, 0xbf, 0xfa, 0x45, 0x7d, 0x53, 0xe, 0xf2, 0x8d, 0x5e, 0xa4, 0x5e, 0x67, 0xbd, 0x6d, 0x3d, 0x72, 0x68, 0x93, 0x32, 0x97, 0x36, 0xf3, 0x2e, 0x12, 0x8, 0x4, 0x2, 0x81, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x81, 0x40, 0x20, 0xec, 0x85, 0x1f, 0xf5, 0xf3, 0xbd, 0x4c};
    constexpr char text[] = "AnyPS5 zlib checksum and bounds.\n";
    constexpr std::uint32_t expectedLength = (sizeof(text) - 1) * 128;
    alignas(2048) std::array<std::uint8_t, 2048 + 65536 + 2048> storage{};
    auto* output = storage.data() + 2048;
    std::uint64_t id = 0;
    std::uint32_t produced = 0;
    std::int32_t status = 0;
    Require(sceZlibFinalize() == static_cast<std::int32_t>(0x81120032));
    Require(sceZlibInflate(nullptr, 0, nullptr, 0, nullptr) == static_cast<std::int32_t>(0x81120032));
    RequireThrows([&] { sceZlibInitialize(nullptr, 1); });
    Require(sceZlibInitialize(nullptr, 0) == 0);
    Require(sceZlibInitialize(nullptr, 0) == static_cast<std::int32_t>(0x81120033));
    RequireThrows([&] { sceZlibInflate(compressed.data(), compressed.size(), output + 1, expectedLength, &id); });
    RequireThrows([&] { sceZlibInflate(compressed.data(), compressed.size(), output, 65537, &id); });
    RequireThrows([&] { sceZlibInflate(nullptr, compressed.size(), output, expectedLength, &id); });
    std::uint64_t previous = 0;
    auto decode = [&](const void* input, std::uint32_t inputLength, std::uint32_t capacity, std::int32_t expectedStatus) {
        storage.fill(0xA5);
        Require(sceZlibInflate(input, inputLength, output, capacity, &id) == 0);
        Require(id > previous);
        previous = id;
        RequireThrows([&] { sceZlibInflate(input, inputLength, output, capacity, &id); });
        RequireThrows([&] { sceZlibFinalize(); });
        std::uint32_t timeout = 0;
        RequireThrows([&] { sceZlibWaitForDone(&id, &timeout); });
        std::uint64_t completed;
        Require(sceZlibWaitForDone(&completed, nullptr) == 0);
        Require(completed == id);
        RequireThrows([&] { sceZlibGetResult(id + 1, &produced, &status); });
        Require(sceZlibGetResult(id, &produced, &status) == 0);
        Require(status == expectedStatus && produced <= capacity);
        RequireThrows([&] { sceZlibGetResult(id, &produced, &status); });
        Require(std::all_of(storage.begin(), storage.begin() + 2048, [](auto byte) { return byte == 0xA5; }));
        Require(std::all_of(storage.begin() + 2048 + capacity, storage.end(), [](auto byte) { return byte == 0xA5; }));
    };
    decode(compressed.data(), compressed.size(), expectedLength, 0);
    Require(produced == expectedLength);
    for (std::uint32_t offset = 0; offset < produced; offset += sizeof(text) - 1)
        Require(std::memcmp(output + offset, text, sizeof(text) - 1) == 0);
    decode(compressed.data(), compressed.size(), expectedLength - 1, static_cast<std::int32_t>(0x8112001C));
    auto corrupt = compressed;
    corrupt.back() ^= 1;
    decode(corrupt.data(), corrupt.size(), expectedLength, static_cast<std::int32_t>(0x811200FF));
    decode(compressed.data(), compressed.size() - 1, 65536, static_cast<std::int32_t>(0x811200FF));
    Require(sceZlibFinalize() == 0);
    Require(sceZlibGetResult(id, &produced, &status) == static_cast<std::int32_t>(0x81120032));
    std::array<std::uint8_t, 65536> scratch{};
    Require(sceZlibInitialize(scratch.data(), scratch.size()) == 0);
    decode(compressed.data(), compressed.size(), 65536, 0);
    Require(sceZlibFinalize() == 0);
}
