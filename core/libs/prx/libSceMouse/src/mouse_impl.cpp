#include "prx/libSceMouse/include/MouseState.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace {
std::mutex mouseMutex;
std::array<MouseData, MOUSE_MAX_DATA_NUM> history{};
int head = 0;
int count = 0;
bool initialized = false;
bool opened = false;
int openedUser = 0;
std::uint32_t extensionConfig = 0;
std::uint8_t extensionFlags = 0;
std::int32_t extensionCursor = 0;
std::uint32_t handType = 0;
std::uint32_t pointerSpeed = 0;
bool connected = true;
std::uint32_t buttons = 0;
constexpr std::array<std::int64_t, 4> SCREEN_WIDTHS{1280, 1920, 2560, 3840};
constexpr std::array<std::int64_t, 4> SCREEN_HEIGHTS{720, 1080, 1440, 2160};
constexpr std::array<float, 5> CURVE_SPEEDS{0.0f, 0.4300079345703125f, 1.25f, 3.8600006103515625f, 40.0f};
constexpr std::array<float, 5> CURVE_MOVES{0.0f, 1.0702667236328125f, 4.140625f, 18.984375f, 443.75f};
struct AbsoluteSample {
    std::uint64_t timestamp;
    std::uint32_t buttons;
    std::int32_t x;
    std::int32_t y;
    std::int32_t wheel;
    std::int32_t tilt;
    bool connected;
};

std::array<AbsoluteSample, MOUSE_MAX_DATA_NUM> absoluteHistory{};
int absoluteHead = 0;
int absoluteCount = 0;
std::uint32_t screenSize = 1;
std::int64_t absoluteX = 0;
std::int64_t absoluteY = 0;
std::uint8_t highPrecision = 0;
float fractionX = 0.0f;
float fractionY = 0.0f;

float curve(float speed) {
    std::size_t segment = 1;
    while (segment + 1 < CURVE_SPEEDS.size() && speed > CURVE_SPEEDS[segment]) ++segment;
    const float lowSpeed = CURVE_SPEEDS[segment - 1];
    const float lowMove = CURVE_MOVES[segment - 1];
    return lowMove + (speed - lowSpeed) * (CURVE_MOVES[segment] - lowMove) / (CURVE_SPEEDS[segment] - lowSpeed);
}

std::int64_t step(float& fraction, float delta, float scale) {
    const float total = delta * scale + fraction;
    const float whole = std::trunc(total);
    fraction = total - whole;
    return static_cast<std::int64_t>(whole);
}

void moveAbsolute(std::int32_t dx, std::int32_t dy) {
    std::int64_t moveX = dx;
    std::int64_t moveY = dy;
    if (highPrecision != 0) {
        moveX = 0;
        moveY = 0;
        const float length = std::sqrt(static_cast<float>(dx) * static_cast<float>(dx) + static_cast<float>(dy) * static_cast<float>(dy));
        if (length != 0.0f) {
            const float scale = curve(length * 120.0f / 420.0f) * 96.0f / 120.0f / length;
            moveX = step(fractionX, static_cast<float>(dx), scale);
            moveY = step(fractionY, static_cast<float>(dy), scale);
        }
    }
    absoluteX = std::clamp<std::int64_t>(absoluteX + moveX, 0, SCREEN_WIDTHS[screenSize] - 1);
    absoluteY = std::clamp<std::int64_t>(absoluteY + moveY, 0, SCREEN_HEIGHTS[screenSize] - 1);
}

void enqueueAbsolute(const AbsoluteSample& sample) {
    if (absoluteCount == MOUSE_MAX_DATA_NUM) {
        absoluteHead = (absoluteHead + 1) % MOUSE_MAX_DATA_NUM;
        --absoluteCount;
    }
    absoluteHistory[(absoluteHead + absoluteCount) % MOUSE_MAX_DATA_NUM] = sample;
    ++absoluteCount;
}

bool sharesRecord(const AbsoluteSample& first, const AbsoluteSample& second) {
    return first.connected && second.connected && first.buttons == second.buttons;
}

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
    openedUser = userId;
    extensionConfig = 0;
    extensionFlags = 0;
    extensionCursor = 0;
    buttons = 0;
    head = 0;
    count = 0;
    screenSize = 1;
    absoluteX = 0;
    absoluteY = 0;
    highPrecision = 0;
    handType = 0;
    pointerSpeed = 0;
    fractionX = 0.0f;
    fractionY = 0.0f;
    MouseData initial{};
    initial.timestamp = sceKernelGetProcessTime();
    initial.connected = connected;
    enqueue(initial);
    absoluteHead = 0;
    absoluteCount = 0;
    enqueueAbsolute(AbsoluteSample{initial.timestamp, 0, 0, 0, 0, 0, connected});
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
    absoluteHead = 0;
    absoluteCount = 0;
    return MOUSE_OK;
}

namespace {
int checkRead(std::int32_t handle, const void* data, std::int32_t num) {
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    if (data == nullptr || num <= 0 || num > MOUSE_MAX_DATA_NUM) return MOUSE_ERROR_INVALID_ARG;
    return MOUSE_OK;
}
}

int CheckRead(std::int32_t handle, const void* data, std::int32_t num) {
    std::lock_guard lock(mouseMutex);
    return checkRead(handle, data, num);
}

int CheckHandle(std::int32_t handle) {
    std::lock_guard lock(mouseMutex);
    return handle == MOUSE_HANDLE && opened ? MOUSE_OK : MOUSE_ERROR_INVALID_HANDLE;
}

bool IsInitialized() {
    std::lock_guard lock(mouseMutex);
    return initialized;
}

int GetHandle(int userId, std::int32_t type) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    return opened && openedUser == userId && type == 0 ? MOUSE_HANDLE : MOUSE_ERROR_NO_HANDLE;
}

int SetExtensionConfig(std::int32_t handle, const std::uint8_t* config) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (config == nullptr) return MOUSE_ERROR_INVALID_ARG;
    std::array<std::uint8_t, MOUSE_EXTENSION_CONFIG_SIZE> local{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(local.data(), config, local.size());
    errno = savedErrno;
    if (!copied) return MOUSE_ERROR_INVALID_ARG;
    for (std::size_t offset = 5; offset < MOUSE_EXTENSION_CONFIG_SIZE; ++offset) {
        if (local[offset] != 0) return MOUSE_ERROR_INVALID_ARG;
    }
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    std::memcpy(&extensionConfig, local.data(), sizeof(extensionConfig));
    extensionFlags = local[4];
    return MOUSE_OK;
}

int SetCursor(std::int32_t handle, std::int32_t cursor) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    extensionCursor = cursor;
    return MOUSE_OK;
}

int GetCursor(std::int32_t handle, std::int32_t* cursor) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (cursor == nullptr) return MOUSE_ERROR_INVALID_ARG;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    const int savedErrno = errno;
    const bool written = GuestCopyOut_nid_no_patch(cursor, &extensionCursor, sizeof(extensionCursor));
    errno = savedErrno;
    return written ? MOUSE_OK : MOUSE_ERROR_INVALID_ARG;
}

int SetHandType(std::int32_t handle, std::uint32_t type) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    if (type > MOUSE_MAX_HAND_TYPE) return MOUSE_ERROR_INVALID_ARG;
    handType = type;
    return MOUSE_OK;
}

int SetPointerSpeed(std::int32_t handle, std::uint32_t speed) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    if (speed > MOUSE_MAX_POINTER_SPEED) return MOUSE_ERROR_INVALID_ARG;
    pointerSpeed = speed;
    return MOUSE_OK;
}

int Read(std::int32_t handle, MouseData* data, std::int32_t num) {
    std::lock_guard lock(mouseMutex);
    if (const int error = checkRead(handle, data, num)) return error;
    const int available = std::min(count, num);
    for (int i = 0; i < available; ++i) {
        data[i] = history[head];
        head = (head + 1) % MOUSE_MAX_DATA_NUM;
    }
    count -= available;
    return available;
}

int SetAbsoluteVirtualScreenSize(std::int32_t handle, std::int32_t size) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    const auto next = static_cast<std::uint32_t>(size);
    if (next >= SCREEN_WIDTHS.size()) return MOUSE_ERROR_INVALID_ARG;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    absoluteX = absoluteX * SCREEN_WIDTHS[next] / SCREEN_WIDTHS[screenSize];
    absoluteY = absoluteY * SCREEN_HEIGHTS[next] / SCREEN_HEIGHTS[screenSize];
    screenSize = next;
    return MOUSE_OK;
}

int SetAbsoluteHighPrecisionMode(std::int32_t handle, std::int32_t mode) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    highPrecision = static_cast<std::uint8_t>(mode);
    return MOUSE_OK;
}

int ReadAbsolutePosition(std::int32_t handle, MouseAbsoluteData* data, std::int32_t num) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (data == nullptr || num <= 0 || num > MOUSE_MAX_DATA_NUM) return MOUSE_ERROR_INVALID_ARG;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    if (!connected) {
        data[0] = MouseAbsoluteData{};
        return 1;
    }
    std::array<AbsoluteSample, MOUSE_MAX_DATA_NUM> samples{};
    std::array<bool, MOUSE_MAX_DATA_NUM> endsRecord{};
    const int pending = absoluteCount;
    for (int i = 0; i < pending; ++i) {
        samples[i] = absoluteHistory[(absoluteHead + i) % MOUSE_MAX_DATA_NUM];
        endsRecord[i] = true;
    }
    absoluteHead = 0;
    absoluteCount = 0;
    int records = pending;
    for (int i = 0; i + 1 < pending && records > num; ++i) {
        if (!sharesRecord(samples[i], samples[i + 1])) continue;
        endsRecord[i] = false;
        --records;
    }
    int first = pending;
    for (int kept = 0; first > 0 && kept < num;) {
        --first;
        if (endsRecord[first]) ++kept;
        while (first > 0 && !endsRecord[first - 1]) --first;
    }
    int written = 0;
    std::int32_t wheel = 0;
    std::int32_t tilt = 0;
    for (int i = first; i < pending; ++i) {
        const AbsoluteSample& sample = samples[i];
        if (!sample.connected) continue;
        moveAbsolute(sample.x, sample.y);
        wheel += sample.wheel;
        tilt += sample.tilt;
        if (!endsRecord[i]) continue;
        MouseAbsoluteData& record = data[written++];
        record.timestamp = sample.timestamp;
        record.connected = 1;
        record.buttons = sample.buttons;
        record.x = static_cast<std::uint16_t>(absoluteX);
        record.y = static_cast<std::uint16_t>(absoluteY);
        record.wheel = wheel;
        record.tilt = tilt;
        wheel = 0;
        tilt = 0;
    }
    return written;
}

int ResetAbsolutePosition(std::int32_t handle, std::uint16_t x, std::uint16_t y) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    if (x >= SCREEN_WIDTHS[screenSize] || y >= SCREEN_HEIGHTS[screenSize]) return MOUSE_ERROR_INVALID_ARG;
    absoluteX = x;
    absoluteY = y;
    return MOUSE_OK;
}

int DebugGetDeviceId(std::int32_t handle, std::int32_t* deviceId) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (deviceId == nullptr) return MOUSE_ERROR_INVALID_ARG;
    const std::int32_t initialId = -1;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(deviceId, &initialId, sizeof(initialId));
    errno = savedErrno;
    if (!copied) return MOUSE_ERROR_INVALID_ARG;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    if (!connected) return MOUSE_OK;
    const std::int32_t connectedId = MOUSE_DEVICE_ID;
    const int savedErrno2 = errno;
    GuestCopyOut_nid_no_patch(deviceId, &connectedId, sizeof(connectedId));
    errno = savedErrno2;
    return MOUSE_OK;
}

void Publish(const MouseInputEvent& event) {
    std::lock_guard lock(mouseMutex);
    if (event.connectionChange) {
        if (connected == event.connected) return;
        connected = event.connected;
        if (!connected) {
            buttons = 0;
            absoluteHead = 0;
            absoluteCount = 0;
        }
    } else if (!connected) {
        return;
    }
    if (!opened) return;
    const bool countsGiven = event.countsX != 0 || event.countsY != 0;
    const bool noReport = event.button == 0 && !event.resetButtons && !event.connectionChange && event.wheel == 0 && event.tilt == 0;
    if (noReport && event.x == 0 && event.y == 0 && !countsGiven) return;
    if (event.resetButtons) buttons = 0;
    if (event.button != 0) {
        if (event.pressed) buttons |= event.button;
        else buttons &= ~event.button;
    }
    const std::uint64_t timestamp = sceKernelGetProcessTime();
    enqueueAbsolute(AbsoluteSample{timestamp, buttons, countsGiven ? event.countsX : event.x, countsGiven ? event.countsY : event.y, event.wheel, event.tilt, connected});
    if (noReport && event.x == 0 && event.y == 0) return;
    MouseData data{};
    data.timestamp = timestamp;
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
