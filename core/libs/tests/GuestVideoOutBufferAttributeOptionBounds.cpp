#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static bool ThrowsAddOption(VideoOutBufferAttribute2* attribute, uint64_t option) {
    try {
        sceVideoOutAddBufferAttributeOption(attribute, option);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsRemoveOption(VideoOutBufferAttribute2* attribute, uint64_t option) {
    try {
        sceVideoOutRemoveBufferAttributeOption(attribute, option);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsSetAttribute(VideoOutBufferAttribute2* attribute, uint64_t pixelFormat, uint32_t tilingMode, uint32_t width, uint32_t height, uint64_t option, uint32_t dccControl, uint64_t dccClearColor) {
    try {
        sceVideoOutSetBufferAttribute2(attribute, pixelFormat, tilingMode, width, height, option, dccControl, dccClearColor);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsRegisterBuffers2(int handle, int setIndex, int bufferIndexStart, const VideoOutBuffers* buffers, int bufferNum, const VideoOutBufferAttribute2* attribute, int category, void* option) {
    try {
        sceVideoOutRegisterBuffers2(handle, setIndex, bufferIndexStart, buffers, bufferNum, attribute, category, option);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    VideoOutTestEnvironment environment;

    Require(ThrowsAddOption(nullptr, 0));
    Require(ThrowsAddOption(nullptr, 8));
    Require(ThrowsAddOption(nullptr, 0x20));
    Require(ThrowsRemoveOption(nullptr, 0));
    Require(ThrowsRemoveOption(nullptr, 8));
    Require(ThrowsRemoveOption(nullptr, 0x20));
    Require(ThrowsSetAttribute(nullptr, 0x8000000000000000ull, 0, 64, 64, 0, 0, 0));

    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 64, 64, 0, 0, 0);
    Require(attribute.option == 0);
    Require(attribute.width == 64);
    Require(attribute.height == 64);

    sceVideoOutAddBufferAttributeOption(&attribute, 8);
    Require(attribute.option == 8);
    Require(attribute.width == 64);

    sceVideoOutAddBufferAttributeOption(&attribute, 8);
    Require(attribute.option == 8);

    sceVideoOutAddBufferAttributeOption(&attribute, 0x20);
    Require(attribute.option == 0x28);

    sceVideoOutRemoveBufferAttributeOption(&attribute, 0x20);
    Require(attribute.option == 8);

    sceVideoOutRemoveBufferAttributeOption(&attribute, 0x40);
    Require(attribute.option == 8);

    sceVideoOutRemoveBufferAttributeOption(&attribute, 8);
    Require(attribute.option == 0);

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    std::vector<std::byte> allocation(65536 + 65536);
    const auto address = reinterpret_cast<std::uintptr_t>(allocation.data());
    const auto aligned = (address + 65535u) & ~static_cast<std::uintptr_t>(65535u);
    VideoOutBuffers buffer{reinterpret_cast<void*>(aligned), nullptr, {nullptr, nullptr}};

    const std::uint64_t unsupportedOptions[] = {1ull, 2ull, 4ull, 16ull, 0x20ull, 0x28ull, 32ull, 40ull, 64ull, 128ull, 0x1000ull, ~0ull};
    for (const std::uint64_t badOption : unsupportedOptions) {
        sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 64, 64, badOption, 0, 0);
        Require(ThrowsRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr));
    }

    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 64, 64, 8, 0, 0);
    Require(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr) == 0);
    Require(sceVideoOutUnregisterBuffers(handle, 0) == 0);

    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 64, 64, 0, 0, 0);
    Require(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr) == 0);
    Require(sceVideoOutUnregisterBuffers(handle, 0) == 0);

    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 64, 64, 0, 0, 0);
    sceVideoOutAddBufferAttributeOption(&attribute, 8);
    Require(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr) == 0);
    Require(sceVideoOutUnregisterBuffers(handle, 0) == 0);

    sceVideoOutAddBufferAttributeOption(&attribute, 0x20);
    Require(ThrowsRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr));

    sceVideoOutRemoveBufferAttributeOption(&attribute, 0x20);
    Require(attribute.option == 8);
    Require(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr) == 0);
    Require(sceVideoOutUnregisterBuffers(handle, 0) == 0);

    sceVideoOutRemoveBufferAttributeOption(&attribute, 8);
    Require(attribute.option == 0);
    Require(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr) == 0);
    Require(sceVideoOutUnregisterBuffers(handle, 0) == 0);

    Require(sceVideoOutClose(handle) == 0);

    LibcRunShutdown_nid_postfix();
    return 0;
}
