#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
int APS5_VABI sceVideoOutOpen(int userId, int busType, int index, const void* param);
int APS5_VABI sceVideoOutClose(int handle);
int APS5_VABI sceVideoOutInitializeOutputOptions(VideoOutOutputOptions* options);
int APS5_VABI sceVideoOutIsOutputSupported(int handle, std::uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, std::uint64_t reserved);
int APS5_VABI sceVideoOutConfigureOutput(int handle, std::uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, std::uint64_t reserved);
}

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int INVALID_VALUE = static_cast<int>(0x80290001);
static constexpr int INVALID_ADDRESS = static_cast<int>(0x80290002);
static constexpr int INVALID_HANDLE = static_cast<int>(0x8029000B);
static constexpr int UNSUPPORTED_OUTPUT_MODE = static_cast<int>(0x80290016);
static constexpr int UNKNOWN_OUTPUT_MODE = static_cast<int>(0x8029001E);

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    std::array<std::uint8_t, sizeof(VideoOutOutputOptions)> raw{};
    raw.fill(0xAA);
    VideoOutOutputOptions options{};
    std::memcpy(&options, raw.data(), raw.size());
    Require(sceVideoOutInitializeOutputOptions(&options) == 0);
    std::memcpy(raw.data(), &options, raw.size());
    for (std::size_t index = 0; index < raw.size(); ++index) Require(raw[index] == (index == 2 ? 0xFF : 0));
    Require(sceVideoOutInitializeOutputOptions(nullptr) == INVALID_ADDRESS);

    for (int handle : {-1, 0, 1, 0x7FFFFFFF}) {
        Require(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 0) == INVALID_HANDLE);
        Require(sceVideoOutIsOutputSupported(handle, 2, nullptr, &options, 1) == INVALID_HANDLE);
    }

    const int handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    Require(handle > 0);
    Require(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 0) == 1);
    Require(sceVideoOutIsOutputSupported(handle, 1, nullptr, nullptr, 0) == 1);
    for (std::uint64_t mode : {0x4ull, 0x7ull, 0x8ull, 0xCull, 0xDull, 0xEull, 0xFull, 0x10ull, 0x11ull, 0x13ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == UNSUPPORTED_OUTPUT_MODE);
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == UNSUPPORTED_OUTPUT_MODE);
    }
    for (std::uint64_t mode : {0x0ull, 0x2ull, 0x3ull, 0x5ull, 0x6ull, 0x9ull, 0xAull, 0xBull, 0x14ull, 0x20ull, 0x3Full, 0x100ull, 0xD000000Aull, 0x8000000000000001ull, ~0ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == UNKNOWN_OUTPUT_MODE);
    }
    bool thrown = false;
    try {
        sceVideoOutIsOutputSupported(handle, 0x12, &options, nullptr, 0);
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown);

    Require(sceVideoOutIsOutputSupported(handle, 1, &options, &options, 0) == INVALID_VALUE);
    Require(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 1) == INVALID_VALUE);
    Require(sceVideoOutIsOutputSupported(handle, 2, &options, nullptr, 1) == INVALID_VALUE);
    VideoOutOutputOptions zeroed{};
    Require(sceVideoOutIsOutputSupported(handle, 1, &zeroed, nullptr, 0) == INVALID_VALUE);
    Require(sceVideoOutIsOutputSupported(handle, 2, &zeroed, nullptr, 0) == UNKNOWN_OUTPUT_MODE);
    Require(sceVideoOutIsOutputSupported(handle, 4, &zeroed, nullptr, 0) == UNSUPPORTED_OUTPUT_MODE);
    for (std::size_t word = 0; word < 16; ++word) {
        VideoOutOutputOptions changed = options;
        changed.internalData[word] ^= 1;
        Require(sceVideoOutIsOutputSupported(handle, 1, &changed, nullptr, 0) == (word == 3 ? 1 : INVALID_VALUE));
    }
    VideoOutOutputOptions freeWord = options;
    freeWord.internalData[3] = 0xFFFFFFFF;
    Require(sceVideoOutIsOutputSupported(handle, 1, &freeWord, nullptr, 0) == 1);
    Require(sceVideoOutConfigureOutput(handle, 1, &options, nullptr, 0) == 0);

    Require(sceVideoOutClose(handle) == 0);
    Require(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 0) == INVALID_HANDLE);
    LibcRunShutdown_nid_postfix();
}
