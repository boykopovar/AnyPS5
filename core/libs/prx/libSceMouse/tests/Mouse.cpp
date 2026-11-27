#include "prx/libSceMouse/include/Api.hpp"
#include "prx/libSceMouse/include/mouse_structs.h"
#include "prx/libSceMouse/include/MouseState.hpp"
#include "prx/libSceVideoOut/include/MouseInput.hpp"
#include "SDL_events.h"
#include "SDL_mouse.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

extern "C" {
bool MouseIsOpen_nid_no_patch();

}

static void Require(bool value) { if (!value) std::abort(); }

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

namespace {

struct GuardedPages {
    std::uint8_t* base = nullptr;
    std::size_t pageSize = 0;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE, PAGE_NOACCESS));
        Require(base != nullptr);
        Require(VirtualAlloc(base, pageSize, MEM_COMMIT, PAGE_READWRITE) == base);
#else
        pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapping = mmap(nullptr, pageSize * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED);
        base = static_cast<std::uint8_t*>(mapping);
        Require(mprotect(base, pageSize, PROT_READ | PROT_WRITE) == 0);
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 2);
#endif
    }

    void SetWritable(bool writable) {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(base, pageSize, writable ? PAGE_READWRITE : PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(base, pageSize, writable ? (PROT_READ | PROT_WRITE) : PROT_READ) == 0);
#endif
    }

    template <typename T>
    T* AtEnd(std::size_t bytesBeforeBoundary) const {
        return reinterpret_cast<T*>(base + pageSize - bytesBeforeBoundary);
    }

    bool HasByte(std::size_t offset, std::size_t bytes, std::uint8_t value) const {
        for (std::size_t i = 0; i < bytes; ++i) {
            if (base[offset + i] != value) return false;
        }
        return true;
    }
};

void CheckGuardedRead() {
    MouseData data[64]{};
    GuardedPages guarded;
    std::memset(guarded.base, 0xA5, guarded.pageSize);
    Require(sceMouseOpen(1, 0, 0, nullptr) == MOUSE_HANDLE);
    Require(sceMouseRead(MOUSE_HANDLE, data, 64) == 1);

    MouseInputEvent buttonOne{};
    buttonOne.button = 1;
    buttonOne.pressed = true;
    MousePublishInput_nid_no_patch(buttonOne);
    errno = EINVAL;
    Require(sceMouseRead(MOUSE_HANDLE, reinterpret_cast<MouseData*>(0x10), 1) == MOUSE_ERROR_INVALID_ARG);
    Require(errno == EINVAL);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 1 && data[0].buttons == 1);

    MouseInputEvent buttonTwo{};
    buttonTwo.button = 2;
    buttonTwo.pressed = true;
    MousePublishInput_nid_no_patch(buttonTwo);
    buttonTwo.pressed = false;
    MousePublishInput_nid_no_patch(buttonTwo);

    guarded.SetWritable(false);
    errno = EACCES;
    Require(sceMouseRead(MOUSE_HANDLE, reinterpret_cast<MouseData*>(guarded.base), 2) == MOUSE_ERROR_INVALID_ARG);
    Require(errno == EACCES);
    Require(guarded.HasByte(0, sizeof(MouseData) * 2, 0xA5));
    guarded.SetWritable(true);

    MouseData* crossing = guarded.AtEnd<MouseData>(sizeof(MouseData));
    errno = EDOM;
    Require(sceMouseRead(MOUSE_HANDLE, crossing, 2) == MOUSE_ERROR_INVALID_ARG);
    Require(errno == EDOM);
    Require(guarded.HasByte(guarded.pageSize - sizeof(MouseData), sizeof(MouseData), 0xA5));

    Require(sceMouseRead(MOUSE_HANDLE, data, 4) == 2 && data[0].buttons == 3 && data[1].buttons == 1);

    MouseInputEvent buttonThree{};
    buttonThree.button = 4;
    buttonThree.pressed = true;
    MousePublishInput_nid_no_patch(buttonThree);
    Require(sceMouseRead(MOUSE_HANDLE, crossing, 2) == 1 && crossing[0].buttons == 5);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);
    Require(sceMouseClose(MOUSE_HANDLE) == MOUSE_OK);
}

}

int main() {
    MouseData data[64]{};
    std::int32_t deviceId = 0;
    Require(!MouseIsOpen_nid_no_patch());
    Require(sceMouseDebugGetDeviceId(MOUSE_HANDLE, &deviceId) == MOUSE_ERROR_NOT_INITIALIZED);
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 959, 539) == MOUSE_ERROR_NOT_INITIALIZED);
    Require(sceMouseOpen(1, 0, 0, nullptr) == MOUSE_ERROR_NOT_INITIALIZED);
    Require(sceMouseOpen(1, 0, 0, reinterpret_cast<const void*>(0x10)) == MOUSE_ERROR_NOT_INITIALIZED);
    Require(sceMouseConnectPort() == MOUSE_ERROR_NOT_INITIALIZED);
    Require(sceMouseDeviceOpen() == MOUSE_ERROR_NOT_INITIALIZED);
    Require(sceMouseDisconnectDevice() == MOUSE_ERROR_NOT_INITIALIZED);
    Require(sceMouseDisconnectPort() == MOUSE_ERROR_NOT_INITIALIZED);
    Require(sceMouseGetDeviceInfo(0, nullptr) == MOUSE_ERROR_NOT_INITIALIZED);
    Require(sceMouseMbusInit() == MOUSE_ERROR_NOT_INITIALIZED);
    Require(sceMouseSetHandType(0, 0) == MOUSE_ERROR_NOT_INITIALIZED);
    Require(sceMouseSetPointerSpeed(0, 0) == MOUSE_ERROR_NOT_INITIALIZED);
    Require(sceMouseSetProcessPrivilege() == MOUSE_ERROR_NOT_INITIALIZED);
    Require(sceMouseInit() == MOUSE_OK);
    Require(sceMouseInit() == MOUSE_OK);
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 959, 539) == MOUSE_ERROR_INVALID_HANDLE);
    Require(sceMouseOpen(1, 0, 1, nullptr) == MOUSE_ERROR_INVALID_ARG);
    MouseOpenParam unsupported{};
    unsupported.behaviorFlag = 2;
    Require(sceMouseOpen(1, 0, 0, &unsupported) == MOUSE_ERROR_INVALID_ARG);
    Require(sceMouseOpen(1, 0, 0, reinterpret_cast<const void*>(0x10)) == MOUSE_ERROR_INVALID_ARG);
    Require(sceMouseOpen(1, 0, 0, nullptr) == MOUSE_HANDLE);
    Require(MouseIsOpen_nid_no_patch());
    Require(sceMouseOpen(1, 0, 0, nullptr) == MOUSE_ERROR_ALREADY_OPENED);
    Require(sceMouseResetAbsolutePosition(42, 959, 539) == MOUSE_ERROR_INVALID_HANDLE);
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 959, 539) == MOUSE_OK);
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 1919, 1079) == MOUSE_OK);
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 1920, 1079) == MOUSE_ERROR_INVALID_ARG);
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 1919, 1080) == MOUSE_ERROR_INVALID_ARG);
    Require(sceMouseRead(42, data, 1) == MOUSE_ERROR_INVALID_HANDLE);
    Require(sceMouseRead(MOUSE_HANDLE, nullptr, 1) == MOUSE_ERROR_INVALID_ARG);
    Require(sceMouseRead(MOUSE_HANDLE, data, 65) == MOUSE_ERROR_INVALID_ARG);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 1);
    Require(data[0].connected && data[0].buttons == 0);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);

    MouseInput input;
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION;
    event.motion.windowID = 7;
    event.motion.xrel = 9;
    event.motion.yrel = -4;
    input.HandleEvent(event, 8);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);
    input.HandleEvent(event, 7);

    event = {};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.windowID = 7;
    event.button.button = SDL_BUTTON_RIGHT;
    input.HandleEvent(event, 7);
    event = {};
    event.type = SDL_MOUSEWHEEL;
    event.wheel.windowID = 7;
    event.wheel.x = 2;
    event.wheel.y = -3;
    event.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 3) == 3);
    Require(data[0].x_axis == 9 && data[0].y_axis == -4 && data[0].buttons == 0);
    Require(data[1].buttons == 2);
    Require(data[2].buttons == 2 && data[2].wheel == 3 && data[2].tilt == -2);
    Require(data[0].timestamp <= data[2].timestamp);
    event = {};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.windowID = 7;
    event.button.button = SDL_BUTTON_X1;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);

    event = {};
    event.type = SDL_WINDOWEVENT;
    event.window.windowID = 7;
    event.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 1);
    Require(data[0].connected && data[0].buttons == 0);
    event.type = SDL_MOUSEMOTION;
    event.motion.xrel = 5;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);
    event.type = SDL_WINDOWEVENT;
    event.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);
    event.window.event = SDL_WINDOWEVENT_CLOSE;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 1 && !data[0].connected);
    event.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 1 && data[0].connected);

    event = {};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.windowID = 7;
    event.button.button = SDL_BUTTON_LEFT;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 1 && data[0].buttons == 1);
    input.SetPadOwnsMouse(true);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 1 && data[0].buttons == 0);
    input.HandleEvent(event, 7);
    event = {};
    event.type = SDL_MOUSEMOTION;
    event.motion.windowID = 7;
    event.motion.xrel = 3;
    input.HandleEvent(event, 7);
    event = {};
    event.type = SDL_MOUSEWHEEL;
    event.wheel.windowID = 7;
    event.wheel.y = 1;
    input.HandleEvent(event, 7);
    input.SetPadOwnsMouse(true);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);
    input.SetPadOwnsMouse(false);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);
    event = {};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.windowID = 7;
    event.button.button = SDL_BUTTON_LEFT;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 1 && data[0].buttons == 1);
    input.SetPadOwnsMouse(true);
    event.type = SDL_MOUSEBUTTONUP;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 2) == 2 && data[0].buttons == 0 && data[1].buttons == 0);
    input.SetPadOwnsMouse(false);

    for (int i = 0; i < 300; ++i) {
        MouseInputEvent motion{};
        motion.x = -1;
        motion.y = i % 2;
        MousePublishInput_nid_no_patch(motion);
    }
    Require(sceMouseRead(MOUSE_HANDLE, data, 8) == 1);
    Require(data[0].x_axis == -300 && data[0].y_axis == 150 && data[0].buttons == 0);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);

    MouseInputEvent still{};
    MousePublishInput_nid_no_patch(still);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);
    for (int i = 0; i < 300; ++i) {
        MouseInputEvent alternating{};
        alternating.x = i % 2;
        MousePublishInput_nid_no_patch(alternating);
    }
    Require(sceMouseRead(MOUSE_HANDLE, data, 8) == 1);
    Require(data[0].x_axis == 150 && data[0].y_axis == 0);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);

    MouseInputEvent zero{};
    MouseInputEvent two{};
    two.x = 2;
    MouseInputEvent three{};
    three.x = 3;
    MouseInputEvent down{};
    down.button = 1;
    down.pressed = true;
    MouseInputEvent up{};
    up.button = 1;
    for (const MouseInputEvent& report : {two, zero, down, zero, three, zero, up, zero}) MousePublishInput_nid_no_patch(report);
    Require(sceMouseRead(MOUSE_HANDLE, data, 64) == 4);
    Require(data[0].x_axis == 2 && data[0].buttons == 0);
    Require(data[1].x_axis == 0 && data[1].buttons == 1);
    Require(data[2].x_axis == 3 && data[2].buttons == 1);
    Require(data[3].x_axis == 0 && data[3].buttons == 0);

    MouseInputEvent step{};
    step.x = 3;
    MousePublishInput_nid_no_patch(step);
    MousePublishInput_nid_no_patch(step);
    MouseInputEvent press{};
    press.button = 1;
    press.pressed = true;
    MousePublishInput_nid_no_patch(press);
    MousePublishInput_nid_no_patch(step);
    MousePublishInput_nid_no_patch(step);
    MouseInputEvent wheel{};
    wheel.wheel = 1;
    MousePublishInput_nid_no_patch(wheel);
    MousePublishInput_nid_no_patch(step);
    MouseInputEvent release{};
    release.button = 1;
    MousePublishInput_nid_no_patch(release);
    MousePublishInput_nid_no_patch(step);
    Require(sceMouseRead(MOUSE_HANDLE, data, 64) == 7);
    Require(data[0].x_axis == 6 && data[0].buttons == 0);
    Require(data[1].x_axis == 0 && data[1].buttons == 1);
    Require(data[2].x_axis == 6 && data[2].buttons == 1);
    Require(data[3].wheel == 1 && data[3].x_axis == 0 && data[3].buttons == 1);
    Require(data[4].x_axis == 3 && data[4].wheel == 0 && data[4].buttons == 1);
    Require(data[5].x_axis == 0 && data[5].buttons == 0);
    Require(data[6].x_axis == 3 && data[6].buttons == 0);
    Require(data[2].timestamp >= data[1].timestamp && data[6].timestamp >= data[5].timestamp);

    MouseInputEvent large{};
    large.x = INT32_MAX;
    MousePublishInput_nid_no_patch(large);
    MousePublishInput_nid_no_patch(large);
    Require(sceMouseRead(MOUSE_HANDLE, data, 8) == 1 && data[0].x_axis == INT32_MAX);

    for (int i = 0; i < 70; ++i) {
        MouseInputEvent click{};
        click.button = 1;
        click.pressed = (i % 2) == 0;
        MousePublishInput_nid_no_patch(click);
    }
    Require(sceMouseRead(MOUSE_HANDLE, data, 64) == 64);
    Require(data[0].buttons == 1 && data[63].buttons == 0 && data[62].buttons == 1);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);
    Require(sceMouseClose(MOUSE_HANDLE) == MOUSE_OK);
    Require(!MouseIsOpen_nid_no_patch());
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == MOUSE_ERROR_INVALID_HANDLE);
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 959, 539) == MOUSE_ERROR_INVALID_HANDLE);
    Require(sceMouseClose(MOUSE_HANDLE) == MOUSE_ERROR_INVALID_HANDLE);
    MouseOpenParam merged{};
    merged.behaviorFlag = MOUSE_OPEN_PARAM_MERGED;
    Require(sceMouseOpen(1, 0, 0, &merged) == MOUSE_HANDLE);
    Require(MouseIsOpen_nid_no_patch());
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 959, 539) == MOUSE_OK);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 1 && data[0].buttons == 0);

    Require(sceMouseDebugGetDeviceId(MOUSE_HANDLE, nullptr) == MOUSE_ERROR_INVALID_ARG);
    Require(sceMouseDebugGetDeviceId(MOUSE_HANDLE, reinterpret_cast<std::int32_t*>(0x10)) == MOUSE_ERROR_INVALID_ARG);
    Require(sceMouseDebugGetDeviceId(42, &deviceId) == MOUSE_ERROR_INVALID_HANDLE && deviceId == -1);
    Require(sceMouseDebugGetDeviceId(MOUSE_HANDLE, &deviceId) == MOUSE_OK && deviceId == MOUSE_DEVICE_ID);
    MouseInputEvent disconnectEvent{};
    disconnectEvent.connectionChange = true;
    disconnectEvent.connected = false;
    MousePublishInput_nid_no_patch(disconnectEvent);
    Require(sceMouseDebugGetDeviceId(MOUSE_HANDLE, &deviceId) == MOUSE_OK && deviceId == -1);
    disconnectEvent.connected = true;
    MousePublishInput_nid_no_patch(disconnectEvent);
    Require(sceMouseDebugGetDeviceId(MOUSE_HANDLE, &deviceId) == MOUSE_OK && deviceId == MOUSE_DEVICE_ID);

    Require(sceMouseClose(MOUSE_HANDLE) == MOUSE_OK);
    Require(!MouseIsOpen_nid_no_patch());
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 959, 539) == MOUSE_ERROR_INVALID_HANDLE);

    using ReadAbsolute = int (APS5_VABI*)(std::int32_t, AbsoluteRecord*, std::int32_t);
    const auto readAbsolute = reinterpret_cast<ReadAbsolute>(reinterpret_cast<void (*)()>(&sceMouseReadAbsolutePosition));
    AbsoluteRecord absolute[MOUSE_MAX_DATA_NUM]{};
    Require(sceMouseOpen(1, 0, 0, nullptr) == MOUSE_HANDLE);
    Require(sceMouseRead(MOUSE_HANDLE, data, 64) == 1);
    Require(readAbsolute(MOUSE_HANDLE, absolute, MOUSE_MAX_DATA_NUM) == 1);
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 100, 100) == MOUSE_OK);
    input.SetPresentation(3840, 2160, 1920, 1080);
    event = {};
    event.type = SDL_MOUSEMOTION;
    event.motion.windowID = 7;
    event.motion.xrel = 10;
    event.motion.yrel = 5;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 1 && data[0].x_axis == 20 && data[0].y_axis == 10);
    Require(readAbsolute(MOUSE_HANDLE, absolute, 1) == 1 && absolute[0].x == 110 && absolute[0].y == 105);
    Require(sceMouseClose(MOUSE_HANDLE) == MOUSE_OK);
    Require(sceMouseDebugGetDeviceId(MOUSE_HANDLE, &deviceId) == MOUSE_ERROR_INVALID_HANDLE && deviceId == -1);
    CheckGuardedRead();
}
