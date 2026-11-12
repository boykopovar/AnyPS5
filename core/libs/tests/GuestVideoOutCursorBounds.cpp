#include "SceTypes.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstdlib>
#include <exception>
#include <initializer_list>

static constexpr int INVALID_HANDLE = static_cast<int>(0x8029000B);
static constexpr int INVALID_ADDRESS = static_cast<int>(0x80290002);

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

template <typename F>
static void RequireThrows(F&& f) {
    bool threw = false;
    try {
        f();
    } catch (const std::exception&) {
        threw = true;
    }
    if (!threw) {
        std::abort();
    }
}

int main() {
    for (int handle : {-1, 0, 1, 9999, -100, 0x7FFFFFFF}) {
        Require(sceVideoOutCursorSet2xMagnify(handle, 0, true) == INVALID_HANDLE);
        Require(sceVideoOutCursorSet2xMagnify(handle, 0, false) == INVALID_HANDLE);
        Require(sceVideoOutCursorDisable(handle, 0) == INVALID_HANDLE);
        Require(sceVideoOutCursorSetPosition(handle, 0, 0, 0) == INVALID_HANDLE);
        Require(sceVideoOutCursorSetPosition(handle, 0, 100, 100) == INVALID_HANDLE);
        Require(sceVideoOutCursorSetPosition(handle, 0, 1920, 1080) == INVALID_HANDLE);
        Require(sceVideoOutCursorEnable(handle, 0, nullptr) == INVALID_HANDLE);
        char dummy = 0;
        Require(sceVideoOutCursorEnable(handle, 0, &dummy) == INVALID_HANDLE);
        Require(sceVideoOutCursorSetImageAddress(handle, 0, nullptr) == INVALID_HANDLE);
        Require(sceVideoOutCursorSetImageAddress(handle, 0, &dummy) == INVALID_HANDLE);
    }

    for (int badIndex : {-1, 1, 2, 99}) {
        RequireThrows([&] { sceVideoOutCursorSet2xMagnify(-1, badIndex, true); });
        RequireThrows([&] { sceVideoOutCursorDisable(-1, badIndex); });
        RequireThrows([&] { sceVideoOutCursorSetPosition(-1, badIndex, 0, 0); });
        RequireThrows([&] { sceVideoOutCursorEnable(-1, badIndex, nullptr); });
        RequireThrows([&] { sceVideoOutCursorSetImageAddress(-1, badIndex, nullptr); });
    }

    VideoOutTestEnvironment environment;
    int handle = 0;
    try {
        handle = sceVideoOutOpen(255, 0, 0, nullptr);
    } catch (const std::exception&) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        return 0;
    }

    if (handle > 0) {
        Require(sceVideoOutCursorSet2xMagnify(handle, 0, true) == 0);
        Require(sceVideoOutCursorSet2xMagnify(handle, 0, false) == 0);
        Require(sceVideoOutCursorDisable(handle, 0) == 0);
        Require(sceVideoOutCursorSetPosition(handle, 0, 0, 0) == 0);
        Require(sceVideoOutCursorSetPosition(handle, 0, 100, 100) == 0);
        Require(sceVideoOutCursorSetPosition(handle, 0, 3840, 2160) == 0);

        Require(sceVideoOutCursorEnable(handle, 0, nullptr) == INVALID_ADDRESS);
        Require(sceVideoOutCursorSetImageAddress(handle, 0, nullptr) == INVALID_ADDRESS);

        char dummy = 0;
        RequireThrows([&] { sceVideoOutCursorEnable(handle, 0, &dummy); });
        RequireThrows([&] { sceVideoOutCursorSetImageAddress(handle, 0, &dummy); });

        for (int badIndex : {-1, 1, 2, 99}) {
            RequireThrows([&] { sceVideoOutCursorSet2xMagnify(handle, badIndex, true); });
            RequireThrows([&] { sceVideoOutCursorDisable(handle, badIndex); });
            RequireThrows([&] { sceVideoOutCursorSetPosition(handle, badIndex, 100, 100); });
            RequireThrows([&] { sceVideoOutCursorEnable(handle, badIndex, nullptr); });
            RequireThrows([&] { sceVideoOutCursorSetImageAddress(handle, badIndex, nullptr); });
        }

        Require(sceVideoOutClose(handle) == 0);

        Require(sceVideoOutCursorSet2xMagnify(handle, 0, true) == INVALID_HANDLE);
        Require(sceVideoOutCursorSet2xMagnify(handle, 0, false) == INVALID_HANDLE);
        Require(sceVideoOutCursorDisable(handle, 0) == INVALID_HANDLE);
        Require(sceVideoOutCursorSetPosition(handle, 0, 100, 100) == INVALID_HANDLE);
        Require(sceVideoOutCursorEnable(handle, 0, nullptr) == INVALID_HANDLE);
        Require(sceVideoOutCursorSetImageAddress(handle, 0, nullptr) == INVALID_HANDLE);
    }

    return 0;
}
