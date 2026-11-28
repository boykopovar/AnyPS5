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
#include <source_location>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;
constexpr int VideoOutErrorInvalidHandle = static_cast<int>(0x8029000bu);
constexpr int BlackBufferIndex = -1;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "GuestVideoOutFlipRatePrivilegeQueueBounds check failed at line %u\n", location.line());
    std::abort();
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;

    std::uint32_t canaryToken = 0x5a5a5a5au;
    Require(sceVideoOutSubmitEopFlip(-1, BlackBufferIndex, 1, 0, &canaryToken) == VideoOutErrorInvalidHandle);
    Require(canaryToken == 0x5a5a5a5au);
    Require(sceVideoOutSubmitEopFlip(2, BlackBufferIndex, 1, 0, &canaryToken) == VideoOutErrorInvalidHandle);
    Require(canaryToken == 0x5a5a5a5au);

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        LibcRunShutdown_nid_postfix();
        return 77;
    }
    Require(handle > 0);

    std::array<std::uint32_t, 16> reservedTokens{};
    for (std::size_t i = 0; i < reservedTokens.size(); ++i) {
        Require(sceVideoOutSubmitEopFlip(handle, BlackBufferIndex, 1, 0, &reservedTokens[i]) == 0);
        Require(reservedTokens[i] >= 0x04000000u && reservedTokens[i] <= 0x07ffffffu);
        if (i > 0) {
            Require(reservedTokens[i] > reservedTokens[i - 1]);
        }
    }

    Require(sceVideoOutClose(handle) == 0);

    Require(sceVideoOutSubmitEopFlip(handle, BlackBufferIndex, 1, 0, &canaryToken) == VideoOutErrorInvalidHandle);

    LibcRunShutdown_nid_postfix();
    return 0;
}
