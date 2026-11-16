#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#define Require(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "%s:%d: Require(%s) failed\n", __FILE__, __LINE__, #condition); \
            std::abort(); \
        } \
    } while (false)

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;
constexpr int ErrorInvalidHandle = static_cast<int>(0x8029000Bu);
constexpr int ErrorInvalidAddress = static_cast<int>(0x80290002u);

void TestUnopenedHandleBounds() {
    for (const int h : {-1000, -1, 0, 1, 2, 42, 9999}) {
        Require(sceVideoOutCursorDisable(h, 0) == ErrorInvalidHandle);
        Require(sceVideoOutCursorSetPosition(h, 0, 100, 100) == ErrorInvalidHandle);
        Require(sceVideoOutCursorSet2xMagnify(h, 0, false) == ErrorInvalidHandle);
        Require(sceVideoOutCursorEnable(h, 0, nullptr) == ErrorInvalidHandle);
        Require(sceVideoOutCursorSetImageAddress(h, 0, nullptr) == ErrorInvalidHandle);
    }
}

void TestNullAddressOnOpenHandle(int handle) {
    Require(sceVideoOutCursorEnable(handle, 0, nullptr) == ErrorInvalidAddress);
    Require(sceVideoOutCursorSetImageAddress(handle, 0, nullptr) == ErrorInvalidAddress);
}

void TestOpenHandleNullFreeOperations(int handle) {
    Require(sceVideoOutCursorDisable(handle, 0) == 0);
    Require(sceVideoOutCursorSetPosition(handle, 0, 0, 0) == 0);
    Require(sceVideoOutCursorSetPosition(handle, 0, 100, 100) == 0);
    Require(sceVideoOutCursorSetPosition(handle, 0, 3840, 2160) == 0);
    Require(sceVideoOutCursorSet2xMagnify(handle, 0, false) == 0);
    Require(sceVideoOutCursorSet2xMagnify(handle, 0, true) == 0);
}

void TestClosedHandleBounds(int handle) {
    Require(sceVideoOutCursorDisable(handle, 0) == ErrorInvalidHandle);
    Require(sceVideoOutCursorSetPosition(handle, 0, 100, 100) == ErrorInvalidHandle);
    Require(sceVideoOutCursorSet2xMagnify(handle, 0, false) == ErrorInvalidHandle);
    Require(sceVideoOutCursorEnable(handle, 0, nullptr) == ErrorInvalidHandle);
    Require(sceVideoOutCursorSetImageAddress(handle, 0, nullptr) == ErrorInvalidHandle);
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;

    TestUnopenedHandleBounds();

    int handle = -1;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error&) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        return 77;
    }

    Require(handle >= 0);

    TestNullAddressOnOpenHandle(handle);
    TestOpenHandleNullFreeOperations(handle);

    Require(sceVideoOutClose(handle) == 0);

    TestClosedHandleBounds(handle);

    return 0;
}
