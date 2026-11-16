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
constexpr int VIDEO_OUT_ERROR_INVALID_HANDLE = static_cast<int>(0x8029000Bu);
constexpr int VIDEO_OUT_ERROR_0x8029001C = static_cast<int>(0x8029001Cu);

} // namespace

int main() {
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

    for (const int master : {-1, 0, 1, 2, 3, 42, 0x7FFFFFFF}) {
        Require(sceVideoOutSetFlipMaster(handle, master) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutSetFlipMaster(-1, master) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    }
    Require(sceVideoOutClearFlipMaster(handle) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutClearFlipMaster(-1) == VIDEO_OUT_ERROR_INVALID_HANDLE);

    LibcRunShutdown_nid_postfix();
    return 0;
}
