#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/InventoryExports.hpp"
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

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int VIDEO_OUT_ERROR_INVALID_ADDRESS = static_cast<int>(0x80290002u);
static constexpr int VIDEO_OUT_ERROR_INVALID_HANDLE = static_cast<int>(0x8029000Bu);
static constexpr int VIDEO_OUT_BUFFER_NUM_MAX = 16;

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static bool ThrowsRuntimeError(auto&& action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    VideoOutTestEnvironment env;

    for (int unopened : {-1, 0, 1, 9999, 0x7FFFFFFF}) {
        Require(sceVideoOutGetBufferLabelAddress(unopened, nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS);
        std::uintptr_t canaryAddress = 0xDEADBEEFu;
        Require(sceVideoOutGetBufferLabelAddress(unopened, &canaryAddress) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(canaryAddress == 0xDEADBEEFu);
        Require(ThrowsRuntimeError([=] { VideoOutBufferLabelAddress_nid_postfix(unopened); }));
    }

    Require(ThrowsRuntimeError([] { sceVideoOutDriverGetBufferLabelAddress(); }));
    Require(ThrowsRuntimeError([] { sceVideoOutDriverIncrementBufferLabel(); }));
    Require(ThrowsRuntimeError([] { sceVideoOutDriverDecrementBufferLabel(); }));
    Require(ThrowsRuntimeError([] { sceVideoOutDriverResetBufferLabel(); }));

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    Require(sceVideoOutGetBufferLabelAddress(handle, nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS);

    std::uintptr_t labelAddress = 0;
    Require(sceVideoOutGetBufferLabelAddress(handle, &labelAddress) == VIDEO_OUT_BUFFER_NUM_MAX);
    Require(labelAddress != 0);
    Require((labelAddress % alignof(std::uint64_t)) == 0);

    const auto* labels = reinterpret_cast<const std::uint64_t*>(labelAddress);
    for (std::size_t i = 0; i < VIDEO_OUT_BUFFER_NUM_MAX; ++i) {
        Require(labels[i] == 0u);
    }

    Require(VideoOutBufferLabelAddress_nid_postfix(handle) == labelAddress);

    alignas(std::uintptr_t) std::array<std::byte, sizeof(std::uintptr_t) * 2> unalignedBuf{};
    unalignedBuf.fill(std::byte{0x7C});
    auto* unalignedPtr = reinterpret_cast<std::uintptr_t*>(unalignedBuf.data() + 1);
    Require(sceVideoOutGetBufferLabelAddress(handle, unalignedPtr) == VIDEO_OUT_BUFFER_NUM_MAX);
    std::uintptr_t readUnaligned = 0;
    std::memcpy(&readUnaligned, unalignedPtr, sizeof(readUnaligned));
    Require(readUnaligned == labelAddress);

    auto* mutableLabels = reinterpret_cast<std::uint64_t*>(labelAddress);
    for (std::size_t i = 0; i < VIDEO_OUT_BUFFER_NUM_MAX; ++i) {
        mutableLabels[i] = static_cast<std::uint64_t>(0xA000u + i);
    }
    std::uintptr_t repeatedAddress = 0;
    Require(sceVideoOutGetBufferLabelAddress(handle, &repeatedAddress) == VIDEO_OUT_BUFFER_NUM_MAX);
    Require(repeatedAddress == labelAddress);
    const auto* repeatedLabels = reinterpret_cast<const std::uint64_t*>(repeatedAddress);
    for (std::size_t i = 0; i < VIDEO_OUT_BUFFER_NUM_MAX; ++i) {
        Require(repeatedLabels[i] == static_cast<std::uint64_t>(0xA000u + i));
    }

    Require(sceVideoOutClose(handle) == 0);

    std::uintptr_t postCloseAddress = 0xCAFEBABE;
    Require(sceVideoOutGetBufferLabelAddress(handle, &postCloseAddress) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(postCloseAddress == 0xCAFEBABE);
    Require(sceVideoOutGetBufferLabelAddress(handle, nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(ThrowsRuntimeError([=] { VideoOutBufferLabelAddress_nid_postfix(handle); }));

    int reopenedHandle = 0;
    try {
        reopenedHandle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(reopenedHandle > 0);

    std::uintptr_t reopenedAddress = 0;
    Require(sceVideoOutGetBufferLabelAddress(reopenedHandle, &reopenedAddress) == VIDEO_OUT_BUFFER_NUM_MAX);
    Require(reopenedAddress != 0);
    Require((reopenedAddress % alignof(std::uint64_t)) == 0);
    const auto* reopenedLabels = reinterpret_cast<const std::uint64_t*>(reopenedAddress);
    for (std::size_t i = 0; i < VIDEO_OUT_BUFFER_NUM_MAX; ++i) {
        Require(reopenedLabels[i] == 0u);
    }

    Require(sceVideoOutClose(reopenedHandle) == 0);

    LibcRunShutdown_nid_postfix();
    return 0;
}
