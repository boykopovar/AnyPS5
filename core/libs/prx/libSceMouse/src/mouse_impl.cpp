#include "prx/libSceMouse/include/MouseState.hpp"
#include "prx/libkernel/Time/include/Time.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>

namespace {
std::mutex mouseMutex;
std::array<MouseData, MOUSE_MAX_DATA_NUM> history{};
int head = 0;
int count = 0;
bool initialized = false;
bool opened = false;
bool connected = true;
std::uint32_t buttons = 0;

void enqueue(const MouseData& data) {
    if (count == MOUSE_MAX_DATA_NUM) {
        head = (head + 1) % MOUSE_MAX_DATA_NUM;
        --count;
    }
    history[(head + count) % MOUSE_MAX_DATA_NUM] = data;
    ++count;
}

bool motionOnly(const MouseData& data) {
    return data.wheel == 0 && data.tilt == 0 && (data.x_axis != 0 || data.y_axis != 0);
}

std::int32_t saturatedSum(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(static_cast<std::int64_t>(a) + b, INT32_MIN, INT32_MAX));
}

bool mergeMotion(const MouseData& data) {
    if (count == 0 || !motionOnly(data)) return false;
    MouseData& tail = history[(head + count - 1) % MOUSE_MAX_DATA_NUM];
    if (!motionOnly(tail) || tail.buttons != data.buttons || tail.connected != data.connected) return false;
    tail.timestamp = data.timestamp;
    tail.x_axis = saturatedSum(tail.x_axis, data.x_axis);
    tail.y_axis = saturatedSum(tail.y_axis, data.y_axis);
    return true;
}
}

namespace Mouse {

bool IsOpen() {
    std::lock_guard lock(mouseMutex);
    return opened;
}

int Initialize() {
    std::lock_guard lock(mouseMutex);
    initialized = true;
    return MOUSE_OK;
}

int Open(int userId, std::int32_t type, std::int32_t index, const MouseOpenParam* param) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (userId < 0 || type != 0 || index != 0 ||
        (param != nullptr && (param->behaviorFlag & ~MOUSE_OPEN_PARAM_MERGED) != 0)) {
        return MOUSE_ERROR_INVALID_ARG;
    }
    if (opened) return MOUSE_ERROR_ALREADY_OPENED;
    opened = true;
    buttons = 0;
    head = 0;
    count = 0;
    MouseData initial{};
    initial.timestamp = sceKernelGetProcessTime();
    initial.connected = connected;
    enqueue(initial);
    return MOUSE_HANDLE;
}

int Close(std::int32_t handle) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    opened = false;
    buttons = 0;
    head = 0;
    count = 0;
    return MOUSE_OK;
}

int Read(std::int32_t handle, MouseData* data, std::int32_t num) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    if (data == nullptr || num <= 0 || num > MOUSE_MAX_DATA_NUM) return MOUSE_ERROR_INVALID_ARG;
    const int available = std::min(count, num);
    for (int i = 0; i < available; ++i) {
        data[i] = history[head];
        head = (head + 1) % MOUSE_MAX_DATA_NUM;
    }
    count -= available;
    return available;
}

int ResetAbsolutePosition(std::int32_t handle, std::uint16_t x, std::uint16_t y) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    if (x >= 1920 || y >= 1080) return MOUSE_ERROR_INVALID_ARG;
    return MOUSE_OK;
}

void Publish(const MouseInputEvent& event) {
    std::lock_guard lock(mouseMutex);
    if (event.connectionChange) {
        if (connected == event.connected) return;
        connected = event.connected;
        if (!connected) buttons = 0;
    } else if (!connected) {
        return;
    }
    if (!opened) return;
    if (event.button == 0 && !event.resetButtons && !event.connectionChange && event.x == 0 && event.y == 0 && event.wheel == 0 && event.tilt == 0) return;
    if (event.resetButtons) buttons = 0;
    if (event.button != 0) {
        if (event.pressed) buttons |= event.button;
        else buttons &= ~event.button;
    }
    MouseData data{};
    data.timestamp = sceKernelGetProcessTime();
    data.connected = connected;
    data.buttons = buttons;
    data.x_axis = event.x;
    data.y_axis = event.y;
    data.wheel = event.wheel;
    data.tilt = event.tilt;
    if (event.button == 0 && !event.resetButtons && mergeMotion(data)) return;
    enqueue(data);
}

}
