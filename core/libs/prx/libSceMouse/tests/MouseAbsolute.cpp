#include "prx/libSceMouse/include/Api.hpp"
#include "prx/libSceMouse/include/MouseState.hpp"
#include "prx/libSceMouse/include/mouse_structs.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

struct AbsoluteRecord {
    std::uint64_t timestamp;
    std::uint8_t connected;
    std::uint8_t padding[3];
    std::uint32_t buttons;
    std::uint16_t x;
    std::uint16_t y;
    std::int32_t wheel;
    std::int32_t tilt;
    std::uint8_t reserved[12];
};
static_assert(sizeof(AbsoluteRecord) == 40);

using SetValueFn = int (APS5_VABI*)(std::int32_t, std::int32_t);
using ReadAbsoluteFn = int (APS5_VABI*)(std::int32_t, AbsoluteRecord*, std::int32_t);

template <typename Function>
Function Resolve(const char* name) {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("libSceMouse.prx"), name));
#else
    void* symbol = dlsym(RTLD_DEFAULT, name);
#endif
    if (!symbol) {
        std::fprintf(stderr, "%s is not exported by libSceMouse\n", name);
        std::abort();
    }
    return reinterpret_cast<Function>(symbol);
}

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "mouse absolute position: %s\n", what);
    std::abort();
}

void Motion(std::int32_t x, std::int32_t y) {
    MouseInputEvent event{};
    event.x = x;
    event.y = y;
    MousePublishInput_nid_no_patch(event);
}

void Button(std::uint32_t button, bool pressed) {
    MouseInputEvent event{};
    event.button = button;
    event.pressed = pressed;
    MousePublishInput_nid_no_patch(event);
}

}

int main() {
    const auto setSize = Resolve<SetValueFn>("sceMouseSetAbsoluteVirtualScreenSize");
    const auto setMode = Resolve<SetValueFn>("sceMouseSetAbsoluteHighPrecisionMode");
    const auto readAbsolute = Resolve<ReadAbsoluteFn>("sceMouseReadAbsolutePosition");
    AbsoluteRecord records[64];

    Require(setSize(MOUSE_HANDLE, 9) == MOUSE_ERROR_NOT_INITIALIZED, "the screen size before initialization did not return NOT_INITIALIZED first");
    Require(setMode(MOUSE_HANDLE, 1) == MOUSE_ERROR_NOT_INITIALIZED, "the precision mode before initialization did not return NOT_INITIALIZED");
    Require(readAbsolute(MOUSE_HANDLE, nullptr, 0) == MOUSE_ERROR_NOT_INITIALIZED, "a read before initialization did not return NOT_INITIALIZED first");
    Require(sceMouseInit() == MOUSE_OK, "initialization failed");
    Require(setSize(MOUSE_HANDLE, 4) == MOUSE_ERROR_INVALID_ARG && setSize(MOUSE_HANDLE, -1) == MOUSE_ERROR_INVALID_ARG, "a size past 3 was not refused before the handle");
    Require(setSize(MOUSE_HANDLE, 1) == MOUSE_ERROR_INVALID_HANDLE, "the screen size of a closed handle was set");
    Require(setMode(MOUSE_HANDLE, 1) == MOUSE_ERROR_INVALID_HANDLE, "the precision mode of a closed handle was set");
    Require(readAbsolute(MOUSE_HANDLE, nullptr, 1) == MOUSE_ERROR_INVALID_ARG && readAbsolute(MOUSE_HANDLE, records, 0) == MOUSE_ERROR_INVALID_ARG && readAbsolute(MOUSE_HANDLE, records, 65) == MOUSE_ERROR_INVALID_ARG, "a bad read was not refused before the handle");
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == MOUSE_ERROR_INVALID_HANDLE, "a closed handle was read");
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 5000, 0) == MOUSE_ERROR_INVALID_HANDLE, "a reset with a bad handle and a bad position did not return INVALID_HANDLE");

    Require(sceMouseOpen(1, 0, 0, nullptr) == MOUSE_HANDLE, "open failed");
    std::memset(records, 0xA5, sizeof(records));
    Require(readAbsolute(MOUSE_HANDLE, records, 64) == 1, "the sample queued at open was not read");
    Require(records[0].connected == 1 && records[0].x == 0 && records[0].y == 0, "the position does not start at the top-left corner");
    Require(records[0].padding[0] == 0xA5 && records[0].padding[2] == 0xA5 && records[0].reserved[0] == 0xA5 && records[0].reserved[11] == 0xA5, "bytes the console leaves alone were written");
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 0, "a read with no samples returned a record");

    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 1920, 0) == MOUSE_ERROR_INVALID_ARG, "x 1920 was taken at 1920x1080");
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 1000, 500) == MOUSE_OK, "the reset failed");
    Require(setSize(MOUSE_HANDLE, 3) == MOUSE_OK, "the size 3840x2160 was refused");
    Button(1, true);
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 2000 && records[0].y == 1000 && records[0].buttons == 1, "the position was not scaled to 3840x2160");
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 3839, 2159) == MOUSE_OK, "the corner of 3840x2160 was refused");
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 2000, 1000) == MOUSE_OK, "the reset at 3840x2160 failed");
    Require(setSize(MOUSE_HANDLE, 0) == MOUSE_OK, "the size 1280x720 was refused");
    Button(1, false);
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 666 && records[0].y == 333 && records[0].buttons == 0, "the position was not scaled to 1280x720 with truncation");
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 1280, 0) == MOUSE_ERROR_INVALID_ARG, "x 1280 was taken at 1280x720");

    Require(setSize(MOUSE_HANDLE, 1) == MOUSE_OK && sceMouseResetAbsolutePosition(MOUSE_HANDLE, 100, 100) == MOUSE_OK, "returning to 1920x1080 failed");
    Motion(5, -3);
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 105 && records[0].y == 97, "motion did not move the position one for one");
    Motion(-5000, 5000);
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 0 && records[0].y == 1079, "the position was not clamped to the screen");
    MouseInputEvent wheel{};
    wheel.wheel = 2;
    wheel.tilt = -1;
    MousePublishInput_nid_no_patch(wheel);
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].wheel == 2 && records[0].tilt == -1, "the wheel and tilt were not copied");
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 100, 100) == MOUSE_OK, "the reset before the merge checks failed");
    Motion(1, 0);
    Motion(2, 0);
    Motion(4, 0);
    Require(readAbsolute(MOUSE_HANDLE, records, 3) == 3 && records[0].x == 101 && records[1].x == 103 && records[2].x == 107, "three samples read with num 3 did not give one record each");
    Motion(1, 0);
    Motion(2, 0);
    Motion(4, 0);
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 114, "three samples read with num 1 did not share one record with all their motion");

    Require(setMode(MOUSE_HANDLE, 0x100) == MOUSE_OK && sceMouseResetAbsolutePosition(MOUSE_HANDLE, 100, 100) == MOUSE_OK, "the precision mode was refused");
    Motion(1, 0);
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 101, "mode 0x100 did not keep its low byte 0, which is off");
    Require(setMode(MOUSE_HANDLE, 1) == MOUSE_OK && sceMouseResetAbsolutePosition(MOUSE_HANDLE, 100, 100) == MOUSE_OK, "turning high precision on failed");
    Motion(1, 0);
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 100, "a delta of 1 moved a whole pixel in high precision");
    Motion(1, 0);
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 101, "the kept fraction was not added to the next delta");
    Motion(10, 0);
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 111, "a delta of 10 did not follow the curve");
    Motion(100, 0);
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 359 && records[0].y == 100, "a delta of 100 did not follow the curve");

    MouseData relative[64];
    const auto reopen = [&](const char* what) {
        Require(sceMouseClose(MOUSE_HANDLE) == MOUSE_OK && sceMouseOpen(1, 0, 0, nullptr) == MOUSE_HANDLE, what);
        Require(readAbsolute(MOUSE_HANDLE, records, 64) == 1 && sceMouseRead(MOUSE_HANDLE, relative, 64) == 1, what);
    };
    reopen("reopening for the queued precision check failed");
    Require(setMode(MOUSE_HANDLE, 1) == MOUSE_OK && sceMouseResetAbsolutePosition(MOUSE_HANDLE, 100, 100) == MOUSE_OK, "turning high precision on failed");
    for (int i = 0; i < 10; ++i) Motion(1, 0);
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 105, "ten queued deltas of 1 did not each go through the curve");
    reopen("reopening for the incremental precision check failed");
    Require(setMode(MOUSE_HANDLE, 1) == MOUSE_OK && sceMouseResetAbsolutePosition(MOUSE_HANDLE, 100, 100) == MOUSE_OK, "turning high precision on failed");
    for (int i = 0; i < 10; ++i) {
        Motion(1, 0);
        Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1, "a delta read on its own gave no record");
    }
    Require(records[0].x == 105, "ten deltas of 1 read one at a time did not end where the queued ones did");

    Require(setMode(MOUSE_HANDLE, 0) == MOUSE_OK && sceMouseResetAbsolutePosition(MOUSE_HANDLE, 0, 100) == MOUSE_OK, "the edge reset failed");
    Motion(-5, 0);
    Motion(5, 0);
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 5, "queued motion at the left edge was not clamped per sample");
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 0, 100) == MOUSE_OK, "the edge reset failed");
    Motion(-5, 0);
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 0, "motion past the left edge was not clamped");
    Motion(5, 0);
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 5, "motion read one at a time at the edge did not end where the queued motion did");

    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 100, 100) == MOUSE_OK, "the reset before the size check failed");
    Motion(10, 0);
    Require(setSize(MOUSE_HANDLE, 3) == MOUSE_OK && readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 210 && records[0].y == 200, "a queued sample was not applied at the size in force when it was read");
    Require(setSize(MOUSE_HANDLE, 1) == MOUSE_OK && sceMouseResetAbsolutePosition(MOUSE_HANDLE, 100, 100) == MOUSE_OK, "returning to 1920x1080 failed");
    Motion(10, 0);
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 50, 50) == MOUSE_OK && readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 60 && records[0].y == 50, "a queued sample did not move the position a later reset set");

    Require(sceMouseRead(MOUSE_HANDLE, relative, 64) >= 0, "draining sceMouseRead failed");
    Motion(7, 0);
    Require(sceMouseRead(MOUSE_HANDLE, relative, 1) == 1 && relative[0].x_axis == 7, "sceMouseRead did not get the motion");
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 67, "a relative read took the sample from the absolute reads");
    Motion(3, 0);
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == 1 && records[0].x == 70, "the absolute read missed the motion");
    Require(sceMouseRead(MOUSE_HANDLE, relative, 1) == 1 && relative[0].x_axis == 3, "an absolute read took the sample from sceMouseRead");

    MouseInputEvent disconnect{};
    disconnect.connectionChange = true;
    disconnect.connected = false;
    MousePublishInput_nid_no_patch(disconnect);
    std::memset(records, 0xA5, sizeof(records));
    Require(readAbsolute(MOUSE_HANDLE, records, 4) == 1, "a disconnected mouse did not return one record");
    const AbsoluteRecord zero{};
    Require(std::memcmp(&records[0], &zero, sizeof(zero)) == 0 && records[1].connected == 0xA5, "a disconnected mouse did not return one zeroed record");

    Require(sceMouseClose(MOUSE_HANDLE) == MOUSE_OK, "close failed");
    Require(readAbsolute(MOUSE_HANDLE, records, 1) == MOUSE_ERROR_INVALID_HANDLE, "a closed handle was read");
    std::puts("mouse absolute position tests passed");
    return 0;
}
