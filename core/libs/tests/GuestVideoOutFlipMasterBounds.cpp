#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

#define Require(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "assertion failed at %s:%d\n", __FILE__, __LINE__); \
            std::abort(); \
        } \
    } while (false)

namespace {

constexpr int SYSTEM_USER = 255;
constexpr int MAIN_BUS = 0;
constexpr int VIDEO_OUT_ERROR_INVALID_ADDRESS = static_cast<int>(0x80290002u);
constexpr int VIDEO_OUT_ERROR_INVALID_HANDLE = static_cast<int>(0x8029000Bu);
constexpr int VIDEO_OUT_ERROR_0x8029001C = static_cast<int>(0x8029001Cu);

void TestBufferAttributeBounds() {
    VideoOutBufferAttribute attr{};
    std::memset(&attr, 0x5A, sizeof(attr));
    sceVideoOutSetBufferAttribute(&attr, 0x80000000u, 1, 0, 1920, 1080, 1920);
    Require(attr.pixel_format == 0x80000000u);
    Require(attr.tiling_mode == 1);
    Require(attr.aspect_ratio == 0);
    Require(attr.width == 1920);
    Require(attr.height == 1080);
    Require(attr.pitch_in_pixel == 1920);
    Require(attr.reserved0 == 0);
    Require(attr.reserved1 == 0);

    VideoOutBufferAttribute2 attr2{};
    std::memset(&attr2, 0x5A, sizeof(attr2));
    sceVideoOutSetBufferAttribute2(&attr2, 0x8000000000000000ull, 1, 1920, 1080, 0x10ull, 0x208u, 0x11223344ull);
    Require(attr2.reserved0 == 0);
    Require(attr2.tiling_mode == 1);
    Require(attr2.aspect_ratio == 0);
    Require(attr2.width == 1920);
    Require(attr2.height == 1080);
    Require(attr2.pitch_in_pixel == 0);
    Require(attr2.option == 0x10ull);
    Require(attr2.pixel_format == 0x8000000000000000ull);
    Require(attr2.dcc_cb_register_clear_color == 0x11223344ull);
    Require(attr2.dcc_control == 0x208u);
    Require(attr2.pad0 == 0);
    Require(attr2.reserved1[0] == 0);
    Require(attr2.reserved1[1] == 0);
    Require(attr2.reserved1[2] == 0);

    sceVideoOutAddBufferAttributeOption(&attr2, 8);
    Require(attr2.option == 0x18ull);

    sceVideoOutRemoveBufferAttributeOption(&attr2, 0x10);
    Require(attr2.option == 8ull);

    sceVideoOutRemoveBufferAttributeOption(&attr2, 8);
    Require(attr2.option == 0ull);
}

void TestSubmitChangeBufferAttributeUnopened() {
    VideoOutBufferAttribute attr{};
    sceVideoOutSetBufferAttribute(&attr, 0x80000000u, 1, 0, 1920, 1080, 1920);

    VideoOutBufferAttribute2 attr2{};
    sceVideoOutSetBufferAttribute2(&attr2, 0x8000000000000000ull, 0, 1920, 1080, 0, 0, 0);

    for (const int unopened : {-1, 0, 1, 2, 42, 9999, -9999, 0x7FFFFFFF}) {
        Require(sceVideoOutSubmitChangeBufferAttribute(unopened, 0, nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS);
        Require(sceVideoOutSubmitChangeBufferAttribute(unopened, 0, &attr) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutSubmitChangeBufferAttribute2(unopened, 0, &attr2, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutSubmitChangeBufferAttribute2(unopened, 0, nullptr, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    }
}

} // namespace

int main() {
    TestBufferAttributeBounds();
    TestSubmitChangeBufferAttributeUnopened();

    VideoOutTestEnvironment environment;

    int handle = 0;
    try {
        const int unopenedHandles[] = {-1, 0, 1, 2, 42, 9999, -9999, 0x7FFFFFFF};
        for (const int unopened : unopenedHandles) {
            for (const int master : {-1, 0, 1, 2, 3, 42, 0x7FFFFFFF}) {
                Require(sceVideoOutSetFlipMaster(unopened, master) == VIDEO_OUT_ERROR_INVALID_HANDLE);
            }
            Require(sceVideoOutClearFlipMaster(unopened) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        }
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    Require(sceVideoOutSetFlipMaster(handle, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutSetFlipMaster(handle, 1) == VIDEO_OUT_ERROR_INVALID_HANDLE);

    Require(sceVideoOutClearFlipMaster(handle) == VIDEO_OUT_ERROR_0x8029001C);
    Require(sceVideoOutClearFlipMaster(handle) == VIDEO_OUT_ERROR_0x8029001C);
    Require(sceVideoOutSetFlipMaster(handle, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutSetFlipMaster(handle, 1) == VIDEO_OUT_ERROR_INVALID_HANDLE);

    std::vector<std::byte> allocation(65536 + 65536);
    const auto address = reinterpret_cast<std::uintptr_t>(allocation.data());
    const auto aligned = (address + 65535u) & ~static_cast<std::uintptr_t>(65535u);
    VideoOutBuffers buffer{reinterpret_cast<void*>(aligned), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 64, 64, 0, 0, 0);
    Require(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr) == 0);

    Require(sceVideoOutUnregisterBuffers(handle, 0) == 0);

    Require(sceVideoOutClose(handle) == 0);

    VideoOutBufferAttribute postCloseAttr{};
    sceVideoOutSetBufferAttribute(&postCloseAttr, 0x80000000u, 1, 0, 1920, 1080, 1920);
    Require(sceVideoOutSubmitChangeBufferAttribute(handle, 0, nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(sceVideoOutSubmitChangeBufferAttribute(handle, 0, &postCloseAttr) == VIDEO_OUT_ERROR_INVALID_HANDLE);

    for (const int master : {-1, 0, 1, 2, 3, 42, 0x7FFFFFFF}) {
        Require(sceVideoOutSetFlipMaster(handle, master) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutSetFlipMaster(-1, master) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    }
    Require(sceVideoOutClearFlipMaster(handle) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutClearFlipMaster(-1) == VIDEO_OUT_ERROR_INVALID_HANDLE);

    LibcRunShutdown_nid_postfix();
    return 0;
}
