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
#include <stdexcept>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;
constexpr int InvalidHandle = -1;
constexpr int ZeroHandle = 0;
constexpr int UnopenedHandle = 999;
constexpr int LargeInvalidHandle = 0x7fffffff;
constexpr std::size_t VrrStatusSize = 80;
constexpr std::size_t CanaryBufferSize = 96;

constexpr int VideoOutOk = 0;
constexpr int VideoOutErrorInvalidAddress = static_cast<int>(0x80290002u);
constexpr int VideoOutErrorInvalidHandle = static_cast<int>(0x8029000Bu);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestInvalidHandleBounds() {
    std::array<std::byte, CanaryBufferSize> buffer{};
    buffer.fill(std::byte{0xaa});

    Require(sceVideoOutGetOutputStatusWithVrr(InvalidHandle, buffer.data()) == VideoOutErrorInvalidHandle);
    Require(sceVideoOutGetOutputStatusWithVrr(ZeroHandle, buffer.data()) == VideoOutErrorInvalidHandle);
    Require(sceVideoOutGetOutputStatusWithVrr(UnopenedHandle, buffer.data()) == VideoOutErrorInvalidHandle);
    Require(sceVideoOutGetOutputStatusWithVrr(LargeInvalidHandle, buffer.data()) == VideoOutErrorInvalidHandle);

    for (const auto byte : buffer) {
        Require(byte == std::byte{0xaa});
    }

    Require(sceVideoOutGetOutputStatusWithVrr(InvalidHandle, nullptr) == VideoOutErrorInvalidAddress);
    Require(sceVideoOutGetOutputStatusWithVrr(ZeroHandle, nullptr) == VideoOutErrorInvalidAddress);
    Require(sceVideoOutGetOutputStatusWithVrr(UnopenedHandle, nullptr) == VideoOutErrorInvalidAddress);
    Require(sceVideoOutGetOutputStatusWithVrr(LargeInvalidHandle, nullptr) == VideoOutErrorInvalidAddress);
}

int TestOutputStatusWithVrrLayout() {
    VideoOutTestEnvironment environment;
    int handle = 0;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) {
            throw;
        }
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == VideoOutErrorInvalidAddress);

    VideoOutOutputStatus baseline{};
    Require(sceVideoOutGetOutputStatus(handle, &baseline) == VideoOutOk);

    std::array<std::byte, CanaryBufferSize> vrr{};
    vrr.fill(std::byte{0xcc});
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrr.data()) == VideoOutOk);

    Require(std::memcmp(vrr.data(), &baseline, sizeof(baseline)) == 0);

    for (std::size_t offset = sizeof(baseline); offset < VrrStatusSize; ++offset) {
        Require(vrr[offset] == std::byte{0});
    }

    for (std::size_t offset = VrrStatusSize; offset < CanaryBufferSize; ++offset) {
        Require(vrr[offset] == std::byte{0xcc});
    }

    VideoOutOutputStatus unpacked{};
    std::memcpy(&unpacked, vrr.data(), sizeof(unpacked));
    Require(unpacked.resolution == baseline.resolution);
    Require(unpacked.dynamicRange == baseline.dynamicRange);
    Require(unpacked.refreshRate == baseline.refreshRate);
    Require(unpacked.flags == baseline.flags);
    Require(unpacked.reserved[0] == baseline.reserved[0]);
    Require(unpacked.reserved[1] == baseline.reserved[1]);
    Require(unpacked.reserved[2] == baseline.reserved[2]);

    Require(sceVideoOutClose(handle) == VideoOutOk);

    vrr.fill(std::byte{0xdd});
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrr.data()) == VideoOutErrorInvalidHandle);
    for (const auto byte : vrr) {
        Require(byte == std::byte{0xdd});
    }

    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == VideoOutErrorInvalidAddress);

    return 0;
}

} // namespace

int main() {
    TestInvalidHandleBounds();
    const int result = TestOutputStatusWithVrrLayout();
    if (result != 0) {
        return result;
    }
    LibcRunShutdown_nid_postfix();
    return 0;
}
