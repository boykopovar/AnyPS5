#include "prx/libSceVideoOut/include/Output.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int HMD2_MAIN_BUS = 0x20;
static constexpr int INVALID_VALUE = static_cast<int>(0x80290001);
static constexpr int INVALID_ADDRESS = static_cast<int>(0x80290002);
static constexpr int INVALID_HANDLE = static_cast<int>(0x8029000B);
static constexpr int RESOURCE_BUSY = static_cast<int>(0x80290009);
static constexpr int UNSUPPORTED_OUTPUT_MODE = static_cast<int>(0x80290016);
static constexpr int UNKNOWN_OUTPUT_MODE = static_cast<int>(0x8029001E);

static void Require(bool value, const std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Check failed at %s:%u in %s\n", location.file_name(), location.line(), location.function_name());
        std::fflush(stderr);
        std::abort();
    }
}

int main() {
    VideoOutTestEnvironment environment;
    std::array<std::uint8_t, sizeof(VideoOutOutputOptions)> raw{};
    raw.fill(0xAA);
    VideoOutOutputOptions options{};
    std::memcpy(&options, raw.data(), raw.size());
    Require(sceVideoOutInitializeOutputOptions(&options) == 0);
    std::memcpy(raw.data(), &options, raw.size());
    for (std::size_t index = 0; index < raw.size(); ++index) Require(raw[index] == (index == 2 ? 0xFF : 0));
    Require(sceVideoOutInitializeOutputOptions(nullptr) == INVALID_ADDRESS);
    errno = 71;
    Require(sceVideoOutInitializeOutputOptions(reinterpret_cast<VideoOutOutputOptions*>(0x10)) == INVALID_ADDRESS);
    Require(errno == 71);

    std::array<std::uint8_t, 64> zeroedOpenParam{};
    Require(sceVideoOutOpen(SYSTEM_USER, HMD2_MAIN_BUS, 0, nullptr) == INVALID_VALUE);
    Require(sceVideoOutOpen(SYSTEM_USER, HMD2_MAIN_BUS, 0, zeroedOpenParam.data()) == INVALID_VALUE);
    Require(sceVideoOutOpen(0, HMD2_MAIN_BUS, 0, nullptr) == INVALID_VALUE);

    for (int badHandle : {-1000, -100, -1, 0, 4, 10, 100, 9999, 0x7FFFFFFF}) {
        Require(sceVideoOutClose(badHandle) == INVALID_HANDLE);
    }

    int handle = 0;
    try {
        for (int unopened : {-1, 0, 1, 0x7FFFFFFF}) {
            Require(sceVideoOutIsOutputSupported(unopened, 1, &options, nullptr, 0) == INVALID_HANDLE);
            Require(sceVideoOutIsOutputSupported(unopened, 2, nullptr, &options, 1) == INVALID_HANDLE);
            Require(sceVideoOutConfigureOutput(unopened, 1, &options, nullptr, 0) == INVALID_HANDLE);
            Require(sceVideoOutConfigureOutput(unopened, 2, nullptr, &options, 1) == INVALID_HANDLE);
        }
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);
    Require(sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr) == RESOURCE_BUSY);
    Require(sceVideoOutOpen(0, MAIN_BUS, 0, nullptr) == RESOURCE_BUSY);
    Require(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 0) == 1);
    Require(sceVideoOutIsOutputSupported(handle, 1, nullptr, nullptr, 0) == 1);
    const int highFrameRate = sceVideoOutIsOutputSupported(handle, 15, &options, nullptr, 0);
    Require(highFrameRate == 1 || highFrameRate == 0);
    Require(sceVideoOutConfigureOutput(handle, 15, &options, nullptr, 0) == UNSUPPORTED_OUTPUT_MODE);
    for (std::uint64_t mode : {0x4ull, 0x7ull, 0x8ull, 0xCull, 0xDull, 0xEull, 0x10ull, 0x11ull, 0x13ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == UNSUPPORTED_OUTPUT_MODE);
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == UNSUPPORTED_OUTPUT_MODE);
    }
    for (std::uint64_t mode : {0x0ull, 0x2ull, 0x3ull, 0x5ull, 0x6ull, 0x9ull, 0xAull, 0xBull, 0x12ull, 0x14ull, 0x20ull, 0x3Full, 0x100ull, 0xD000000Aull, 0x8000000000000001ull, ~0ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == UNKNOWN_OUTPUT_MODE);
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == UNKNOWN_OUTPUT_MODE);
    }
    for (std::uint64_t mode : {0x20001ull, 0x2000Cull, 0x2000Dull, 0x2000Eull, 0x2000Full, 0x20010ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, nullptr, nullptr, 0) == UNKNOWN_OUTPUT_MODE);
        Require(sceVideoOutConfigureOutput(handle, mode, nullptr, nullptr, 0) == UNKNOWN_OUTPUT_MODE);
    }

    Require(sceVideoOutIsOutputSupported(handle, 1, &options, &options, 0) == INVALID_VALUE);
    Require(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 1) == INVALID_VALUE);
    Require(sceVideoOutIsOutputSupported(handle, 2, &options, nullptr, 1) == INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, 1, &options, &options, 0) == INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, 1, &options, nullptr, 1) == INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, 2, &options, nullptr, 1) == INVALID_VALUE);
    VideoOutOutputOptions zeroed{};
    Require(sceVideoOutIsOutputSupported(handle, 1, &zeroed, nullptr, 0) == INVALID_VALUE);
    Require(sceVideoOutIsOutputSupported(handle, 2, &zeroed, nullptr, 0) == UNKNOWN_OUTPUT_MODE);
    Require(sceVideoOutIsOutputSupported(handle, 4, &zeroed, nullptr, 0) == UNSUPPORTED_OUTPUT_MODE);
    Require(sceVideoOutConfigureOutput(handle, 1, &zeroed, nullptr, 0) == INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, 2, &zeroed, nullptr, 0) == UNKNOWN_OUTPUT_MODE);
    Require(sceVideoOutConfigureOutput(handle, 4, &zeroed, nullptr, 0) == UNSUPPORTED_OUTPUT_MODE);
    for (std::size_t word = 0; word < 16; ++word) {
        VideoOutOutputOptions changed = options;
        changed.internalData[word] ^= 1;
        Require(sceVideoOutIsOutputSupported(handle, 1, &changed, nullptr, 0) == (word == 3 ? 1 : INVALID_VALUE));
        Require(sceVideoOutConfigureOutput(handle, 1, &changed, nullptr, 0) == (word == 3 ? 0 : INVALID_VALUE));
    }
    VideoOutOutputOptions freeWord = options;
    freeWord.internalData[3] = 0xFFFFFFFF;
    Require(sceVideoOutIsOutputSupported(handle, 1, &freeWord, nullptr, 0) == 1);
    Require(sceVideoOutConfigureOutput(handle, 1, &freeWord, nullptr, 0) == 0);
    Require(sceVideoOutConfigureOutput(handle, 1, &options, nullptr, 0) == 0);

    VideoOutOutputStatus modeStatus{};
    VideoOutResolutionStatus modeResolution{};
    Require(sceVideoOutGetOutputStatus(handle, &modeStatus) == 0);
    Require(sceVideoOutGetResolutionStatus(handle, &modeResolution) == 0);
    Require(modeStatus.refreshRate == 3);
    Require(modeResolution.refreshRate == 3);
    Require(modeResolution.fullWidth == 1920 && modeResolution.fullHeight == 1080);
    Require(modeStatus.resolution == 1 || modeStatus.resolution == 2);
    Require(sceVideoOutConfigureOutput(handle, 1, nullptr, nullptr, 0) == 0);

    Require(sceVideoOutClose(handle) == 0);
    Require(sceVideoOutClose(handle) == INVALID_HANDLE);
    Require(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 0) == INVALID_HANDLE);
    Require(sceVideoOutConfigureOutput(handle, 1, &options, nullptr, 0) == INVALID_HANDLE);

    const int userZero = sceVideoOutOpen(0, MAIN_BUS, 0, nullptr);
    Require(userZero > 0);
    Require(sceVideoOutClose(userZero) == 0);

    struct VideoOutOpenParam {
        std::uint32_t firstWord = 16;
        std::uint32_t setPriority = 0;
        std::int32_t priority = 0;
        std::uint32_t setAffinity = 0;
        std::uint64_t affinity = 0;
    };
    static_assert(offsetof(VideoOutOpenParam, affinity) == 16);

    VideoOutOpenParam p1{};
    p1.setPriority = 1;
    p1.priority = 256;
    const int h1 = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, &p1);
    Require(h1 > 0);
    Require(sceVideoOutClose(h1) == 0);

    VideoOutOpenParam p2{};
    p2.setPriority = 1;
    p2.priority = 767;
    const int h2 = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, &p2);
    Require(h2 > 0);
    Require(sceVideoOutClose(h2) == 0);

    VideoOutOpenParam p3{};
    p3.setAffinity = 1;
    p3.affinity = 0x1FFF;
    const int h3 = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, &p3);
    Require(h3 > 0);
    Require(sceVideoOutClose(h3) == 0);

    VideoOutOpenParam p4{};
    p4.setPriority = 1;
    p4.priority = 512;
    p4.setAffinity = 1;
    p4.affinity = 0x3E;
    const int h4 = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, &p4);
    Require(h4 > 0);
    Require(sceVideoOutClose(h4) == 0);

    LibcRunShutdown_nid_postfix();
    return 0;
}
