#include "prx/libSceVideoOut/include/Output.hpp"
#include "SceTypes.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <source_location>
#include <stdexcept>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;

constexpr int ErrorInvalidAddress = static_cast<int>(0x80290002u);
constexpr int ErrorInvalidHandle = static_cast<int>(0x8029000Bu);

void Require(bool value, const char* expr = "", std::source_location loc = std::source_location::current()) {
    if (!value) {
        std::cerr << loc.file_name() << ":" << loc.line() << ": requirement failed: " << expr << std::endl;
        std::abort();
    }
}
#define REQUIRE(x) Require((x), #x)

void TestUnopenedHandleBounds() {
    std::array<std::uint8_t, sizeof(VideoOutFlipStatus)> untouched{};
    untouched.fill(0xAA);

    for (const int badHandle : {-1000, -100, -1, 0, 1, 2, 8, 100, 9999, 0x7FFFFFFF}) {
        REQUIRE(sceVideoOutGetFlipStatus(badHandle, reinterpret_cast<VideoOutFlipStatus*>(untouched.data())) == ErrorInvalidHandle);
        REQUIRE(std::all_of(untouched.begin(), untouched.end(), [](std::uint8_t b) { return b == 0xAA; }));

        REQUIRE(sceVideoOutIsFlipPending(badHandle) == ErrorInvalidHandle);
    }
}

void TestOpenHandleBounds(int handle) {
    REQUIRE(sceVideoOutGetFlipStatus(handle, nullptr) == ErrorInvalidAddress);

    REQUIRE(sceVideoOutIsFlipPending(handle) == 0);

    static_assert(sizeof(VideoOutFlipStatus) == 128);

    std::array<std::uint8_t, sizeof(VideoOutFlipStatus)> rawBuffer{};
    rawBuffer.fill(0x5A);

    REQUIRE(sceVideoOutGetFlipStatus(handle, reinterpret_cast<VideoOutFlipStatus*>(rawBuffer.data())) == 0);

    VideoOutFlipStatus status{};
    std::memcpy(&status, rawBuffer.data(), sizeof(status));

    REQUIRE(status.count == 0);
    REQUIRE(status.processTime == 0);
    REQUIRE(status.reserved0 == 0);
    REQUIRE(status.flipArg == -1);
    REQUIRE(status.reserved1 == 0);
    REQUIRE(status.processTimeCounter == 0);
    REQUIRE(status.gcQueueNum == 0);
    REQUIRE(status.flipPendingNum == 0);
    REQUIRE(status.currentBuffer == -1);
    REQUIRE(status.reserved2 == 0);
    REQUIRE(status.submitProcessTimeCounter == 0);
    for (std::size_t i = 0; i < 7; ++i) {
        REQUIRE(status.reserved3[i] == 0);
    }

    for (std::size_t i = 0; i < 24; ++i) {
        REQUIRE(rawBuffer[i] == 0x00);
    }
    for (std::size_t i = 24; i < 32; ++i) {
        REQUIRE(rawBuffer[i] == 0xFF);
    }
    for (std::size_t i = 32; i < 48; ++i) {
        REQUIRE(rawBuffer[i] == 0x00);
    }
    for (std::size_t i = 48; i < 56; ++i) {
        REQUIRE(rawBuffer[i] == 0x00);
    }
    for (std::size_t i = 56; i < 60; ++i) {
        REQUIRE(rawBuffer[i] == 0xFF);
    }
    for (std::size_t i = 60; i < 128; ++i) {
        REQUIRE(rawBuffer[i] == 0x00);
    }

    REQUIRE(sceVideoOutIsFlipPending(handle) == 0);
}

void TestClosedHandleBounds(int handle) {
    std::array<std::uint8_t, sizeof(VideoOutFlipStatus)> untouched{};
    untouched.fill(0x33);

    REQUIRE(sceVideoOutGetFlipStatus(handle, reinterpret_cast<VideoOutFlipStatus*>(untouched.data())) == ErrorInvalidHandle);
    REQUIRE(std::all_of(untouched.begin(), untouched.end(), [](std::uint8_t b) { return b == 0x33; }));

    REQUIRE(sceVideoOutIsFlipPending(handle) == ErrorInvalidHandle);
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;

    TestUnopenedHandleBounds();

    int handle = -1;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        return 77;
    }

    REQUIRE(handle >= 0);

    TestOpenHandleBounds(handle);

    REQUIRE(sceVideoOutClose(handle) == 0);

    TestClosedHandleBounds(handle);

    return 0;
}
