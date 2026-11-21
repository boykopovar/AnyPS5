#include "prx/libSceVideoOut/include/Output.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

constexpr int SYSTEM_USER = 255;
constexpr int MAIN_BUS = 0;
constexpr uint64_t MODE_DEFAULT = 1;
constexpr uint64_t MODE_119_88HZ = 15;
constexpr int VIDEO_OUT_ERROR_INVALID_VALUE = static_cast<int>(0x80290001u);
constexpr int VIDEO_OUT_ERROR_INVALID_ADDRESS = static_cast<int>(0x80290002u);
constexpr int VIDEO_OUT_ERROR_INVALID_HANDLE = static_cast<int>(0x8029000Bu);
constexpr int VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE = static_cast<int>(0x80290016u);
constexpr int VIDEO_OUT_ERROR_UNAVAILABLE_OUTPUT_MODE = static_cast<int>(0x80290019u);
constexpr int VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE = static_cast<int>(0x8029001Eu);

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "%s:%u: check failed in %s\n", location.file_name(), location.line(), location.function_name());
        std::fflush(stderr);
        std::abort();
    }
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        LibcRunShutdown_nid_postfix();
        return 77;
    }
    Require(handle > 0);

    Require(sceVideoOutInitializeOutputOptions(nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS);

    VideoOutOutputOptions options{};
    std::memset(&options, 0x7E, sizeof(options));
    Require(sceVideoOutInitializeOutputOptions(&options) == 0);
    Require(options.internalData[0] == 0x00FF0000u);
    for (std::size_t i = 1; i < 16; ++i) {
        Require(options.internalData[i] == 0);
    }

    const int unopenedHandles[] = {-1, 0, handle + 10, handle + 100, 0x7FFFFFFF};
    for (const int unopened : unopenedHandles) {
        Require(sceVideoOutConfigureOutput(unopened, MODE_DEFAULT, &options, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutConfigureOutput(unopened, MODE_DEFAULT, nullptr, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutConfigureOutput(unopened, MODE_119_88HZ, &options, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutConfigureOutput(unopened, 4, &options, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutConfigureOutput(unopened, 0, &options, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutConfigureOutput(unopened, 0x12, &options, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutConfigureOutput(unopened, MODE_DEFAULT, &options, &options, 1) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    }

    int dummy = 0;
    Require(sceVideoOutConfigureOutput(handle, MODE_DEFAULT, &options, &dummy, 0) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, MODE_DEFAULT, &options, nullptr, 1) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, MODE_DEFAULT, &options, &dummy, 1) == VIDEO_OUT_ERROR_INVALID_VALUE);

    Require(sceVideoOutConfigureOutput(handle, MODE_DEFAULT, nullptr, nullptr, 0) == 0);
    Require(sceVideoOutConfigureOutput(handle, MODE_DEFAULT, &options, nullptr, 0) == 0);

    const uint32_t freeWordValues[] = {0u, 1u, 0x12345678u, 0x80000000u, 0xFFFFFFFFu};
    for (const uint32_t freeVal : freeWordValues) {
        VideoOutOutputOptions freeOpt = options;
        freeOpt.internalData[3] = freeVal;
        Require(sceVideoOutConfigureOutput(handle, MODE_DEFAULT, &freeOpt, nullptr, 0) == 0);
    }

    for (std::size_t word = 0; word < 16; ++word) {
        if (word == 3) continue;
        VideoOutOutputOptions badOpt = options;
        badOpt.internalData[word] ^= 1u;
        Require(sceVideoOutConfigureOutput(handle, MODE_DEFAULT, &badOpt, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_VALUE);
    }

    VideoOutOutputOptions badWord0 = options;
    badWord0.internalData[0] = 0;
    Require(sceVideoOutConfigureOutput(handle, MODE_DEFAULT, &badWord0, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_VALUE);

    const int mode15Result = sceVideoOutConfigureOutput(handle, MODE_119_88HZ, &options, nullptr, 0);
    Require(mode15Result == VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE || mode15Result == VIDEO_OUT_ERROR_UNAVAILABLE_OUTPUT_MODE);

    const uint64_t unsupportedModes[] = {0x4ull, 0x7ull, 0x8ull, 0xCull, 0xDull, 0xEull, 0x10ull, 0x11ull, 0x13ull};
    for (const uint64_t unsupp : unsupportedModes) {
        Require(sceVideoOutConfigureOutput(handle, unsupp, &options, nullptr, 0) == VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE);
        Require(sceVideoOutConfigureOutput(handle, unsupp, nullptr, nullptr, 0) == VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE);
    }

    const uint64_t unknownModes[] = {0x0ull, 0x2ull, 0x3ull, 0x5ull, 0x6ull, 0x9ull, 0xAull, 0xBull, 0x12ull, 0x14ull, 0x20ull, 0x100ull, 0xD000000Aull, 0x20001ull, 0x2000Full, ~0ull};
    for (const uint64_t unk : unknownModes) {
        Require(sceVideoOutConfigureOutput(handle, unk, &options, nullptr, 0) == VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE);
        Require(sceVideoOutConfigureOutput(handle, unk, nullptr, nullptr, 0) == VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE);
    }

    VideoOutResolutionStatus resStatus{};
    Require(sceVideoOutGetResolutionStatus(handle, &resStatus) == 0);
    Require(resStatus.refreshRate == 59 || resStatus.refreshRate == 60 || resStatus.refreshRate == 3);

    Require(sceVideoOutClose(handle) == 0);

    Require(sceVideoOutConfigureOutput(handle, MODE_DEFAULT, &options, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutConfigureOutput(handle, MODE_119_88HZ, &options, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutConfigureOutput(handle, 4, &options, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutConfigureOutput(handle, 0x12, &options, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutConfigureOutput(handle, 0, &options, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);

    LibcRunShutdown_nid_postfix();
    return 0;
}
