#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;

constexpr int VideoOutErrorInvalidValue = static_cast<int>(0x80290001u);
constexpr int VideoOutErrorInvalidAddress = static_cast<int>(0x80290002u);
constexpr int VideoOutErrorInvalidIndex = static_cast<int>(0x80290003u);
constexpr int VideoOutErrorInvalidHandle = static_cast<int>(0x8029000Bu);
constexpr int VideoOutErrorFlipQueueFull = static_cast<int>(0x80290012u);

constexpr int BufferIndexBlank = -1;
constexpr int BufferIndexBlack = -2;
constexpr std::size_t FlipQueueCapacity = 16;

void Require(bool value) {
    if (!value) std::abort();
}

template <typename Fn>
bool Throws(Fn&& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestUnopenedHandleFlipBounds() {
    VideoOutFlipStatus status{};
    std::uint32_t token = 0;

    for (const int badHandle : {-1000, -100, -1, 0, 1, 2, 8, 10, 100, 9999, 0x7FFFFFFF}) {
        Require(Throws([badHandle] {
            return sceVideoOutSubmitFlip(badHandle, BufferIndexBlack, 1, 0);
        }));

        Require(sceVideoOutSubmitEopFlip(badHandle, BufferIndexBlack, 1, 0, nullptr) == VideoOutErrorInvalidAddress);
        Require(sceVideoOutSubmitEopFlip(badHandle, BufferIndexBlack, 1, 0, &token) == VideoOutErrorInvalidHandle);

        Require(sceVideoOutIsFlipPending(badHandle) == VideoOutErrorInvalidHandle);

        Require(Throws([badHandle] {
            return sceVideoOutGetFlipStatus(badHandle, nullptr);
        }));
        Require(sceVideoOutGetFlipStatus(badHandle, &status) == VideoOutErrorInvalidHandle);

        for (const int rate : {-1, 0, 1, 2, 3}) {
            Require(Throws([badHandle, rate] {
                return sceVideoOutSetFlipRate(badHandle, rate);
            }));
        }

        Require(sceVideoOutSetFlipMaster(badHandle, 0) == VideoOutErrorInvalidHandle);
        Require(sceVideoOutSetFlipMaster(badHandle, 1) == VideoOutErrorInvalidHandle);
        Require(sceVideoOutClearFlipMaster(badHandle) == VideoOutErrorInvalidHandle);
    }
}

void TestOpenHandleFlipParameterBounds(int handle) {
    VideoOutFlipStatus status{};
    std::memset(&status, 0x5A, sizeof(status));

    Require(sceVideoOutGetFlipStatus(handle, nullptr) == VideoOutErrorInvalidAddress);
    Require(sceVideoOutGetFlipStatus(handle, &status) == 0);
    Require(status.count == 0);
    Require(status.flipPendingNum == 0);

    Require(sceVideoOutIsFlipPending(handle) == 0);

    for (const int badRate : {-1000, -100, -1, 3, 4, 10, 60, 100, 0x7FFFFFFF}) {
        Require(sceVideoOutSetFlipRate(handle, badRate) == VideoOutErrorInvalidValue);
    }

    for (const int validRate : {0, 1, 2}) {
        Require(sceVideoOutSetFlipRate(handle, validRate) == 0);
    }

    for (const int badMode : {-100, -1, 0, 7, 8, 10, 100, 0x7FFFFFFF}) {
        Require(Throws([handle, badMode] {
            return sceVideoOutSubmitFlip(handle, BufferIndexBlack, badMode, 0);
        }));
    }

    for (const int badIndex : {-100, -10, -3, 16, 17, 32, 100, 0x7FFFFFFF}) {
        Require(Throws([handle, badIndex] {
            return sceVideoOutSubmitFlip(handle, badIndex, 1, 0);
        }));
    }

    for (const int unregIndex : {0, 1, 2, 5, 10, 15}) {
        Require(Throws([handle, unregIndex] {
            return sceVideoOutSubmitFlip(handle, unregIndex, 1, 0);
        }));
    }

    std::uint32_t token = 0;
    Require(sceVideoOutSubmitEopFlip(handle, BufferIndexBlack, 1, 0, nullptr) == VideoOutErrorInvalidAddress);

    for (const int badMode : {-10, -1, 0, 7, 8, 100}) {
        Require(sceVideoOutSubmitEopFlip(handle, BufferIndexBlack, badMode, 0, &token) == VideoOutErrorInvalidValue);
    }

    for (const int badIndex : {-10, -5, -3, 16, 20, 100}) {
        Require(sceVideoOutSubmitEopFlip(handle, badIndex, 1, 0, &token) == VideoOutErrorInvalidIndex);
    }

    for (const int unregIndex : {0, 1, 2, 8, 15}) {
        Require(sceVideoOutSubmitEopFlip(handle, unregIndex, 1, 0, &token) == VideoOutErrorInvalidIndex);
    }

    Require(sceVideoOutSetFlipMaster(handle, 0) == VideoOutErrorInvalidHandle);
    Require(sceVideoOutSetFlipMaster(handle, 1) == VideoOutErrorInvalidHandle);
    Require(sceVideoOutClearFlipMaster(handle) != 0);
}

void TestEopFlipQueueCapacity(int handle) {
    std::vector<std::uint32_t> tokens;
    tokens.reserve(FlipQueueCapacity + 4);

    for (std::size_t i = 0; i < FlipQueueCapacity; ++i) {
        std::uint32_t token = 0;
        const int result = sceVideoOutSubmitEopFlip(handle, BufferIndexBlack, 1, static_cast<int64_t>(i), &token);
        Require(result == 0);
        Require(token > 0);
        tokens.push_back(token);
    }

    Require(tokens.size() == FlipQueueCapacity);
    for (std::size_t i = 1; i < tokens.size(); ++i) {
        Require(tokens[i] > tokens[i - 1]);
    }

    Require(sceVideoOutIsFlipPending(handle) == static_cast<int>(FlipQueueCapacity));

    VideoOutFlipStatus status{};
    Require(sceVideoOutGetFlipStatus(handle, &status) == 0);
    Require(status.flipPendingNum == static_cast<int>(FlipQueueCapacity));

    std::uint32_t overflowToken = 0x5A5A5A5Au;
    const int overflowResult = sceVideoOutSubmitEopFlip(handle, BufferIndexBlack, 1, 999, &overflowToken);
    Require(overflowResult == VideoOutErrorFlipQueueFull);
    Require(overflowToken == 0x5A5A5A5Au);

    const int blankOverflowResult = sceVideoOutSubmitEopFlip(handle, BufferIndexBlank, 1, 999, &overflowToken);
    Require(blankOverflowResult == VideoOutErrorFlipQueueFull);
    Require(overflowToken == 0x5A5A5A5Au);
}

void TestClosedHandleFlipBounds(int handle) {
    VideoOutFlipStatus status{};
    std::uint32_t token = 0;

    Require(sceVideoOutIsFlipPending(handle) == VideoOutErrorInvalidHandle);
    Require(sceVideoOutGetFlipStatus(handle, &status) == VideoOutErrorInvalidHandle);

    Require(Throws([handle] {
        return sceVideoOutSetFlipRate(handle, 0);
    }));

    Require(Throws([handle] {
        return sceVideoOutSubmitFlip(handle, BufferIndexBlack, 1, 0);
    }));

    Require(sceVideoOutSubmitEopFlip(handle, BufferIndexBlack, 1, 0, &token) == VideoOutErrorInvalidHandle);
    Require(sceVideoOutSetFlipMaster(handle, 0) == VideoOutErrorInvalidHandle);
    Require(sceVideoOutClearFlipMaster(handle) == VideoOutErrorInvalidHandle);
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;

    TestUnopenedHandleFlipBounds();

    int handle = -1;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }

    Require(handle >= 0);

    TestOpenHandleFlipParameterBounds(handle);
    TestEopFlipQueueCapacity(handle);

    Require(sceVideoOutClose(handle) == 0);

    TestClosedHandleFlipBounds(handle);

    LibcRunShutdown_nid_postfix();
    return 0;
}
