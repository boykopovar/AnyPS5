#include "prx/libSceVideoOut/include/Output.hpp"
#include "SceTypes.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;

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

void TestUnopenedHandleBounds() {
    for (const int badHandle : {-1000, -100, -1, 0, 1, 2, 8, 100, 9999, 0x7FFFFFFF}) {
        for (const int top : {-50, -1, 0, 10, 50, 100, 150, 1000}) {
            for (const int bottom : {-50, -1, 0, 10, 50, 100, 150, 1000}) {
                Require(Throws([badHandle, top, bottom] {
                    return sceVideoOutSetWindowModeMargins(badHandle, top, bottom);
                }));
            }
        }

        Require(Throws([badHandle] {
            return sceVideoOutLatencyControlWaitBeforeInput(badHandle);
        }));

        for (const uint32_t point : {0u, 1u, 2u, 10u, 100u, 0xFFFFFFFFu}) {
            Require(Throws([badHandle, point] {
                return sceVideoOutLatencyMeasureSetStartPoint(badHandle, point);
            }));
        }
    }
}

void TestOpenHandleOperations(int handle) {
    for (const int top : {-100, -10, 0, 1, 5, 10, 25, 50, 75, 100, 120, 200}) {
        for (const int bottom : {-100, -10, 0, 1, 5, 10, 25, 50, 75, 100, 120, 200}) {
            Require(sceVideoOutSetWindowModeMargins(handle, top, bottom) == 0);
        }
    }

    Require(sceVideoOutLatencyControlWaitBeforeInput(handle) == 0);

    for (const uint32_t point : {0u, 1u, 2u, 5u, 10u, 50u, 100u, 0x12345678u, 0xFFFFFFFFu}) {
        Require(sceVideoOutLatencyMeasureSetStartPoint(handle, point) == 0);
    }
}

void TestClosedHandleBounds(int handle) {
    for (const int top : {0, 10, 50, 100}) {
        Require(Throws([handle, top] {
            return sceVideoOutSetWindowModeMargins(handle, top, top);
        }));
    }

    Require(Throws([handle] {
        return sceVideoOutLatencyControlWaitBeforeInput(handle);
    }));

    for (const uint32_t point : {0u, 1u, 10u, 100u}) {
        Require(Throws([handle, point] {
            return sceVideoOutLatencyMeasureSetStartPoint(handle, point);
        }));
    }
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
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }

    Require(handle >= 0);

    TestOpenHandleOperations(handle);

    Require(sceVideoOutClose(handle) == 0);

    TestClosedHandleBounds(handle);

    return 0;
}
