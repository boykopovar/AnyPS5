#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/include/PadAudioHapticQueue.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/InputCommandFile.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <stdexcept>

namespace {
    struct Quat { float w = 1.0f, x = 0.0f, y = 0.0f, z = 0.0f; };
    constexpr std::uint64_t kLatchExpiry = 100000;
    constexpr std::uint64_t kLatchExpiryLimit = 500000;
    constexpr std::uint32_t kInterceptedButton = 0x80000000;
    std::uint64_t injectedTimestamp = 0;

    struct QueuedInput {
        PadInputState input;
        std::uint64_t timestamp = 0;
    };
    constexpr std::size_t kQueueCapacity = 64;

    struct PadSlot {
        PadInputState state;
        PadOutputState output;
        std::deque<QueuedInput> queue;
        Quat orientation;
        std::uint64_t lastFuseTime = 0;
        float biasIntegral[3] = {0.0f, 0.0f, 0.0f};
        bool tiltCorrection = true;
        bool angularVelocityDeadband = false;
        std::uint8_t nextTouchId = 0;
        bool prevTouchActive[2] = {false, false};
        std::uint8_t touchIds[2] = {0, 0};
        std::chrono::steady_clock::time_point audioHapticEnd{};
        std::array<std::uint8_t, 2> audioHapticRumble{};
        PadAudioHapticQueue legacyHaptics;
        std::uint64_t timestamp = 0;
        std::uint32_t pressedSinceRead = 0;
        std::uint8_t pressedAnalogL2 = 0;
        std::uint8_t pressedAnalogR2 = 0;
        std::uint64_t pressedAt = 0;
        std::uint64_t lastReadAt = 0;
        std::uint64_t readInterval = 0;
        bool connected = false;
    };

    std::mutex stateMutex;
    std::array<PadSlot, PAD_MAX_CONTROLLERS> slots = [] {
        std::array<PadSlot, PAD_MAX_CONTROLLERS> initialSlots{};
        initialSlots[0].connected = true;
        return initialSlots;
    }();
    std::exception_ptr failure;
    bool initialized = false;
    constexpr float kAngularVelocityDeadband = 0.017453292f;

    bool RefreshAudioHaptics(PadSlot& slot) {
        if (std::chrono::steady_clock::now() >= slot.audioHapticEnd) slot.audioHapticRumble = {};
        const auto legacy = slot.legacyHaptics.Read(sceKernelGetProcessTime());
        const auto large = std::max(slot.audioHapticRumble[0], legacy[0]);
        const auto small = std::max(slot.audioHapticRumble[1], legacy[1]);
        if (slot.output.audioVibrationLarge == large && slot.output.audioVibrationSmall == small) return false;
        slot.output.audioVibrationLarge = large;
        slot.output.audioVibrationSmall = small;
        return true;
    }

    void normalize(Quat& q) {
        const float n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
        if (n < 1e-6f) { q = Quat{}; return; }
        q.w /= n; q.x /= n; q.y /= n; q.z /= n;
    }

    void fuse(PadSlot& slot, float dt, const std::array<float, 3>& accel, const std::array<float, 3>& gyro) {
        float gx = gyro[0], gy = gyro[1], gz = gyro[2];
        const float an = std::sqrt(accel[0] * accel[0] + accel[1] * accel[1] + accel[2] * accel[2]);
        if (slot.tiltCorrection && an > 1.0f) {
            const float ax = accel[0] / an, ay = accel[1] / an, az = accel[2] / an;
            const float vx = 2.0f * (slot.orientation.x * slot.orientation.y + slot.orientation.w * slot.orientation.z);
            const float vy = slot.orientation.w * slot.orientation.w - slot.orientation.x * slot.orientation.x + slot.orientation.y * slot.orientation.y - slot.orientation.z * slot.orientation.z;
            const float vz = 2.0f * (slot.orientation.y * slot.orientation.z - slot.orientation.w * slot.orientation.x);
            const float ex = ay * vz - az * vy;
            const float ey = az * vx - ax * vz;
            const float ez = ax * vy - ay * vx;
            constexpr float kp = 1.5f;
            constexpr float ki = 0.02f;
            slot.biasIntegral[0] += ki * ex * dt;
            slot.biasIntegral[1] += ki * ey * dt;
            slot.biasIntegral[2] += ki * ez * dt;
            gx += kp * ex + slot.biasIntegral[0];
            gy += kp * ey + slot.biasIntegral[1];
            gz += kp * ez + slot.biasIntegral[2];
        }
        const float hx = 0.5f * gx * dt, hy = 0.5f * gy * dt, hz = 0.5f * gz * dt;
        const Quat q = slot.orientation;
        slot.orientation.w = q.w - q.x * hx - q.y * hy - q.z * hz;
        slot.orientation.x = q.x + q.w * hx + q.y * hz - q.z * hy;
        slot.orientation.y = q.y + q.w * hy - q.x * hz + q.z * hx;
        slot.orientation.z = q.z + q.w * hz + q.x * hy - q.y * hx;
        normalize(slot.orientation);
    }

    void encodeZones(std::uint8_t* dst, std::uint8_t mode, const std::uint8_t* strengths, int count, std::uint8_t frequency) {
        std::uint16_t zones = 0;
        std::uint32_t packed = 0;
        for (int i = 0; i < count && i < 10; ++i) {
            const std::uint8_t s = strengths[i];
            if (s == 0 || s > 8) continue;
            zones |= static_cast<std::uint16_t>(1u << i);
            packed |= static_cast<std::uint32_t>(s - 1) << (i * 3);
        }
        if (zones == 0 || (mode == 0x26 && frequency == 0)) { dst[0] = 0x05; return; }
        dst[0] = mode;
        dst[1] = static_cast<std::uint8_t>(zones & 0xFF);
        dst[2] = static_cast<std::uint8_t>(zones >> 8);
        std::memcpy(dst + 3, &packed, 4);
        dst[9] = frequency;
    }

    std::uint8_t maxOf(const std::uint8_t* v, int n) {
        std::uint8_t m = 0;
        for (int i = 0; i < n; ++i) m = std::max(m, v[i]);
        return m;
    }

    std::uint8_t scaleStrength(std::uint8_t s) {
        return s == 0 ? 0 : static_cast<std::uint8_t>(std::min(255, 32 * std::min<int>(s, 8)));
    }
}

void Pad::Initialize() {
    std::lock_guard lock(stateMutex);
    if (failure) std::rethrow_exception(failure);
    if (!initialized) {
        const std::uint64_t now = sceKernelGetProcessTime();
        for (std::size_t i = 0; i < slots.size(); ++i) {
            slots[i].timestamp = now;
            slots[i].lastFuseTime = now;
            slots[i].queue.clear();
            slots[i].pressedSinceRead = 0;
            slots[i].lastReadAt = 0;
            slots[i].readInterval = 0;
        }
    }
    initialized = true;
}

bool Pad::IsInitialized() {
    std::lock_guard lock(stateMutex);
    return initialized;
}

namespace {
PadData BuildData(PadSlot& slot, const PadInputState& state, std::uint64_t changeTime, std::uint64_t now, const Pad::InputCommandFile::Input& injected, int port) {
    PadData data{};
    const bool intercepted = state.intercepted;
    data.buttons = intercepted ? kInterceptedButton : state.buttons | injected.buttons;
    const auto& sticks = !intercepted && injected.hasSticks ? injected.sticks : state.sticks;
    data.left_stick_x = sticks[0];
    data.left_stick_y = sticks[1];
    data.right_stick_x = sticks[2];
    data.right_stick_y = sticks[3];
    const auto trigger = [&](std::uint8_t analog, std::uint32_t bit) -> std::uint8_t {
        return (!intercepted && (injected.buttons & bit) != 0) || (analog == 0 && (state.buttons & bit) != 0) ? 255 : analog;
    };
    data.analog_buttons_l2 = trigger(state.analogButtonsL2, 0x100);
    data.analog_buttons_r2 = trigger(state.analogButtonsR2, 0x200);

    const bool live = state.hasMotion && slot.output.motionEnabled;
    const std::array<float, 3> rest{0.0f, 9.80665f, 0.0f};
    const std::array<float, 3>& accel = live ? state.accel : rest;
    std::array<float, 3> gyro = live ? state.gyro : std::array<float, 3>{0.0f, 0.0f, 0.0f};
    if (slot.angularVelocityDeadband) {
        for (float& axis : gyro) {
            if (axis < kAngularVelocityDeadband && axis > -kAngularVelocityDeadband) axis = 0.0f;
        }
    }
    if (live) {
        float dt = slot.lastFuseTime != 0 && now > slot.lastFuseTime ? static_cast<float>(now - slot.lastFuseTime) * 1e-6f : 0.0f;
        dt = std::min(dt, 0.1f);
        fuse(slot, dt, accel, gyro);
    } else {
        slot.orientation = Quat{};
        slot.biasIntegral[0] = slot.biasIntegral[1] = slot.biasIntegral[2] = 0.0f;
    }
    slot.lastFuseTime = now;
    data.orientation_x = slot.orientation.x;
    data.orientation_y = slot.orientation.y;
    data.orientation_z = slot.orientation.z;
    data.orientation_w = slot.orientation.w;
    constexpr float kGravity = 9.80665f;
    data.acceleration_x = accel[0] / kGravity;
    data.acceleration_y = accel[1] / kGravity;
    data.acceleration_z = accel[2] / kGravity;
    data.angular_velocity_x = gyro[0];
    data.angular_velocity_y = gyro[1];
    data.angular_velocity_z = gyro[2];

    std::uint8_t connectedCount = 0;
    for (const auto& s : slots) {
        if (s.connected) ++connectedCount;
    }
    data.connected = slot.connected;
    data.connected_count = connectedCount > 0 ? connectedCount : 1;
    data.timestamp = changeTime;

    std::uint8_t touchNum = 0;
    for (int i = 0; i < 2; ++i) {
        const PadTouchPoint& tp = state.touch[i];
        if (!tp.active) { slot.prevTouchActive[i] = false; continue; }
        if (!slot.prevTouchActive[i]) slot.touchIds[i] = static_cast<std::uint8_t>(slot.nextTouchId++ & 0x7F);
        slot.prevTouchActive[i] = true;
        if (touchNum == 0) {
            data.touch_data_touch0_x = tp.x; data.touch_data_touch0_y = tp.y; data.touch_data_touch0_id = slot.touchIds[i];
        } else {
            data.touch_data_touch1_x = tp.x; data.touch_data_touch1_y = tp.y; data.touch_data_touch1_id = slot.touchIds[i];
        }
        ++touchNum;
    }
    const bool injectedTouch = !intercepted && (injected.touchLeft || injected.touchRight);
    if (touchNum == 0 && (state.touchLeft || state.touchRight || injectedTouch)) {
        data.buttons |= 0x100000;
        touchNum = 1;
        data.touch_data_touch0_x = state.touchRight || (!intercepted && injected.touchRight) ? kTouchPadWidth * 3 / 4 : kTouchPadWidth / 4;
        data.touch_data_touch0_y = kTouchPadHeight / 2;
        data.touch_data_touch0_id = 0;
    }
    data.touch_data_touch_num = touchNum;
    return data;
}

Pad::InputCommandFile::Input InjectedInput(std::uint64_t now) {
    static Pad::InputCommandFile commands([] {
#ifdef _WIN32
        const auto* path = _wgetenv(L"APS5_PAD_INPUT_FILE");
#else
        const auto* path = std::getenv("APS5_PAD_INPUT_FILE");
#endif
        return path ? std::filesystem::path(path) : std::filesystem::path{};
    }());
    static Pad::InputCommandFile::Input lastInjected;
    const auto injected = commands.Poll(now);
    if (injected != lastInjected) {
        lastInjected = injected;
        slots[0].timestamp = now;
        injectedTimestamp = now;
    }
    return injected;
}

std::uint64_t LatchExpiry(const PadSlot& slot) {
    return std::clamp(slot.readInterval + slot.readInterval / 2, kLatchExpiry, kLatchExpiryLimit);
}
}

PadData Pad::ReadState(int port) {
    if (port < 0 || port >= PAD_MAX_CONTROLLERS) throw std::invalid_argument("Pad: invalid port");
    std::lock_guard lock(stateMutex);
    if (failure) std::rethrow_exception(failure);
    if (!initialized) throw std::runtime_error("Pad: read before initialization");
    const std::uint64_t now = sceKernelGetProcessTime();
    const auto injected = port == 0 ? InjectedInput(now) : Pad::InputCommandFile::Input{};
    auto& slot = slots[port];
    PadData data = BuildData(slot, slot.state, slot.timestamp, now, injected, port);
    if (slot.pressedSinceRead != 0 && now - slot.pressedAt <= LatchExpiry(slot)) {
        data.buttons |= slot.pressedSinceRead;
        if (slot.pressedSinceRead & 0x100u) data.analog_buttons_l2 = std::max(data.analog_buttons_l2, slot.pressedAnalogL2);
        if (slot.pressedSinceRead & 0x200u) data.analog_buttons_r2 = std::max(data.analog_buttons_r2, slot.pressedAnalogR2);
    }
    slot.pressedSinceRead = 0;
    if (slot.lastReadAt != 0) slot.readInterval = now - slot.lastReadAt;
    slot.lastReadAt = now;
    return data;
}

int Pad::Read(PadData* data, int num, int port) {
    if (data == nullptr || num <= 0) throw std::invalid_argument("Pad: invalid read buffer");
    if (port < 0 || port >= PAD_MAX_CONTROLLERS) throw std::invalid_argument("Pad: invalid port");
    std::lock_guard lock(stateMutex);
    if (failure) std::rethrow_exception(failure);
    if (!initialized) throw std::runtime_error("Pad: read before initialization");
    auto& slot = slots[port];
    slot.pressedSinceRead = 0;
    const std::uint64_t now = sceKernelGetProcessTime();
    const auto injected = port == 0 ? InjectedInput(now) : Pad::InputCommandFile::Input{};
    if (slot.queue.empty()) {
        data[0] = BuildData(slot, slot.state, slot.timestamp, now, injected, port);
        return 1;
    }
    int count = 0;
    while (count < num && !slot.queue.empty()) {
        const QueuedInput& entry = slot.queue.front();
        data[count++] = BuildData(slot, entry.input, port == 0 ? std::max(entry.timestamp, injectedTimestamp) : entry.timestamp, now, injected, port);
        slot.queue.pop_front();
    }
    return count;
}

void Pad::SetVibration(int port, std::uint8_t large, std::uint8_t small) {
    if (port < 0 || port >= PAD_MAX_CONTROLLERS) return;
    std::lock_guard lock(stateMutex);
    auto& output = slots[port].output;
    if (output.vibrationLarge == large && output.vibrationSmall == small) return;
    output.vibrationLarge = large;
    output.vibrationSmall = small;
    ++output.sequence;
}

void Pad::SetVibration(std::uint8_t large, std::uint8_t small) {
    SetVibration(0, large, small);
}

void Pad::SetVibrationMode(int port, int mode) {
    if (port < 0 || port >= PAD_MAX_CONTROLLERS) return;
    std::lock_guard lock(stateMutex);
    auto& output = slots[port].output;
    if (output.vibrationMode == mode) return;
    output.vibrationMode = mode;
    ++output.sequence;
}

void Pad::SetVibrationMode(int mode) {
    SetVibrationMode(0, mode);
}

void Pad::SetLightBar(int port, bool valid, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    if (port < 0 || port >= PAD_MAX_CONTROLLERS) return;
    std::lock_guard lock(stateMutex);
    auto& output = slots[port].output;
    output.lightBarValid = valid;
    output.lightBar[0] = r;
    output.lightBar[1] = g;
    output.lightBar[2] = b;
    ++output.sequence;
}

void Pad::SetLightBar(bool valid, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    SetLightBar(0, valid, r, g, b);
}

void Pad::SetTriggerCommand(int port, int trigger, const std::uint8_t* command) {
    if (port < 0 || port >= PAD_MAX_CONTROLLERS || trigger < 0 || trigger > 1 || command == nullptr) return;
    std::uint32_t mode = 0;
    std::memcpy(&mode, command, 4);
    const std::uint8_t* p = command + 8;
    PadTriggerRequest req;
    req.mode = mode;
    req.effect[0] = 0x05;
    switch (mode) {
        case 1: {
            std::uint8_t strengths[10] = {};
            if (p[0] <= 9 && p[1] >= 1 && p[1] <= 8) for (int i = p[0]; i < 10; ++i) strengths[i] = p[1];
            encodeZones(req.effect, 0x21, strengths, 10, 0);
            req.fallback = scaleStrength(p[1]);
            break;
        }
        case 2: {
            if (p[0] >= 2 && p[0] <= 7 && p[1] > p[0] && p[1] <= 8 && p[2] >= 1 && p[2] <= 8) {
                const std::uint16_t zones = static_cast<std::uint16_t>((1u << p[0]) | (1u << p[1]));
                req.effect[0] = 0x25;
                req.effect[1] = static_cast<std::uint8_t>(zones & 0xFF);
                req.effect[2] = static_cast<std::uint8_t>(zones >> 8);
                req.effect[3] = static_cast<std::uint8_t>(p[2] - 1);
            }
            req.fallback = scaleStrength(p[2]);
            break;
        }
        case 3: {
            std::uint8_t strengths[10] = {};
            if (p[0] <= 9 && p[1] >= 1 && p[1] <= 8) for (int i = p[0]; i < 10; ++i) strengths[i] = p[1];
            encodeZones(req.effect, 0x26, strengths, 10, p[2]);
            req.fallback = scaleStrength(p[1]);
            break;
        }
        case 4:
            encodeZones(req.effect, 0x21, p, 10, 0);
            req.fallback = scaleStrength(maxOf(p, 10));
            break;
        case 5: {
            std::uint8_t strengths[10] = {};
            if (p[0] <= 8 && p[1] > p[0] && p[1] <= 9 && p[2] >= 1 && p[2] <= 8 && p[3] >= 1 && p[3] <= 8) {
                const int dist = p[1] - p[0];
                for (int i = p[0]; i < 10; ++i)
                    strengths[i] = i <= p[1] ? static_cast<std::uint8_t>(std::lround(p[2] + (p[3] - p[2]) * (i - p[0]) / static_cast<double>(dist))) : p[3];
            }
            encodeZones(req.effect, 0x21, strengths, 10, 0);
            req.fallback = scaleStrength(std::max(p[2], p[3]));
            break;
        }
        case 6:
            encodeZones(req.effect, 0x26, p + 1, 10, p[0]);
            req.fallback = p[0] == 0 ? 0 : scaleStrength(maxOf(p + 1, 10));
            break;
        default: break;
    }
    req.valid = req.effect[0] != 0x05;
    if (!req.valid) req.fallback = 0;
    std::lock_guard lock(stateMutex);
    auto& output = slots[port].output;
    output.trigger[trigger] = req;
    output.triggerTouched = true;
    ++output.sequence;
}

void Pad::SetTriggerCommand(int trigger, const std::uint8_t* command) {
    SetTriggerCommand(0, trigger, command);
}

void Pad::ResetOrientation(int port) {
    if (port < 0 || port >= PAD_MAX_CONTROLLERS) return;
    std::lock_guard lock(stateMutex);
    slots[port].orientation = Quat{};
    slots[port].biasIntegral[0] = slots[port].biasIntegral[1] = slots[port].biasIntegral[2] = 0.0f;
}

void Pad::SetTiltCorrection(int port, bool enabled) {
    if (port < 0 || port >= PAD_MAX_CONTROLLERS) return;
    std::lock_guard lock(stateMutex);
    slots[port].tiltCorrection = enabled;
    slots[port].biasIntegral[0] = slots[port].biasIntegral[1] = slots[port].biasIntegral[2] = 0.0f;
}

void Pad::SetTiltCorrection(bool enabled) {
    SetTiltCorrection(0, enabled);
}

void Pad::SetAngularVelocityDeadband(int port, bool enabled) {
    if (port < 0 || port >= PAD_MAX_CONTROLLERS) return;
    std::lock_guard lock(stateMutex);
    slots[port].angularVelocityDeadband = enabled;
}

void Pad::SetAngularVelocityDeadband(bool enabled) {
    SetAngularVelocityDeadband(0, enabled);
}

void Pad::SetMotionEnabled(int port, bool enabled) {
    if (port < 0 || port >= PAD_MAX_CONTROLLERS) return;
    std::lock_guard lock(stateMutex);
    auto& output = slots[port].output;
    if (output.motionEnabled == enabled) return;
    output.motionEnabled = enabled;
    ++output.sequence;
}

void Pad::SetMotionEnabled(bool enabled) {
    SetMotionEnabled(0, enabled);
}

void Pad::SetConnected(int port, bool connected) {
    if (port < 0 || port >= PAD_MAX_CONTROLLERS) return;
    std::lock_guard lock(stateMutex);
    slots[port].connected = connected;
}

bool Pad::IsConnected(int port) {
    if (port < 0 || port >= PAD_MAX_CONTROLLERS) return false;
    std::lock_guard lock(stateMutex);
    return slots[port].connected;
}

extern "C" void PadPublishInputForPort_nid_postfix(int port, const PadInputState& input) {
    if (port < 0 || port >= PAD_MAX_CONTROLLERS) return;
    std::lock_guard lock(stateMutex);
    if (failure) std::rethrow_exception(failure);
    auto& slot = slots[port];
    if (slot.state.buttons == input.buttons && slot.state.sticks == input.sticks &&
        slot.state.analogButtonsL2 == input.analogButtonsL2 && slot.state.analogButtonsR2 == input.analogButtonsR2 &&
        slot.state.touchLeft == input.touchLeft && slot.state.touchRight == input.touchRight &&
        slot.state.hasMotion == input.hasMotion && slot.state.accel == input.accel && slot.state.gyro == input.gyro &&
        slot.state.touch == input.touch && slot.state.deviceKind == input.deviceKind && slot.state.intercepted == input.intercepted) return;
    if (input.intercepted) {
        slot.pressedSinceRead = 0;
        if (!slot.state.intercepted) slot.queue.clear();
    }
    const std::uint32_t pressed = input.buttons & ~slot.state.buttons;
    slot.timestamp = sceKernelGetProcessTime();
    if (pressed != 0) {
        if (slot.pressedSinceRead != 0 && slot.timestamp - slot.pressedAt > LatchExpiry(slot)) slot.pressedSinceRead = 0;
        slot.pressedSinceRead |= pressed;
        slot.pressedAt = slot.timestamp;
        if (pressed & 0x100u) slot.pressedAnalogL2 = input.analogButtonsL2;
        if (pressed & 0x200u) slot.pressedAnalogR2 = input.analogButtonsR2;
    }
    slot.state = input;
    if (slot.queue.size() == kQueueCapacity) slot.queue.pop_front();
    slot.queue.push_back({input, slot.timestamp});
}

extern "C" void PadPublishInput_nid_postfix(const PadInputState& input) {
    PadPublishInputForPort_nid_postfix(0, input);
}

extern "C" void PadReportInputFailure_nid_postfix(std::exception_ptr error) {
    if (!error) throw std::invalid_argument("Pad: missing input failure");
    std::lock_guard lock(stateMutex);
    if (!failure) failure = error;
}

extern "C" bool PadFetchOutputForPort_nid_postfix(int port, std::uint32_t* seenSequence, PadOutputState* out) {
    if (port < 0 || port >= PAD_MAX_CONTROLLERS) return false;
    std::lock_guard lock(stateMutex);
    auto& slot = slots[port];
    if (RefreshAudioHaptics(slot)) ++slot.output.sequence;
    if (seenSequence == nullptr || out == nullptr || *seenSequence == slot.output.sequence) return false;
    *seenSequence = slot.output.sequence;
    *out = slot.output;
    return true;
}

extern "C" bool PadFetchOutput_nid_postfix(std::uint32_t* seenSequence, PadOutputState* out) {
    return PadFetchOutputForPort_nid_postfix(0, seenSequence, out);
}

extern "C" void PadSubmitAudioHapticsForPort_nid_postfix(int port, std::uint8_t large, std::uint8_t small) {
    if (port < 0 || port >= PAD_MAX_CONTROLLERS) return;
    std::lock_guard lock(stateMutex);
    if (large == 0 && small == 0) return;
    const auto now = std::chrono::steady_clock::now();
    auto& slot = slots[port];
    if (now >= slot.audioHapticEnd) slot.audioHapticRumble = {};
    slot.audioHapticRumble[0] = std::max(slot.audioHapticRumble[0], large);
    slot.audioHapticRumble[1] = std::max(slot.audioHapticRumble[1], small);
    slot.audioHapticEnd = now + std::chrono::milliseconds(80);
    static_cast<void>(RefreshAudioHaptics(slot));
    ++slot.output.sequence;
}

extern "C" void PadQueueLegacyAudioHaptics_nid_postfix(int source, std::uint64_t begin, std::uint64_t end, std::uint8_t large, std::uint8_t small) {
    std::lock_guard lock(stateMutex);
    slots[0].legacyHaptics.Queue(source, begin, end, large, small);
    if (RefreshAudioHaptics(slots[0])) ++slots[0].output.sequence;
}

extern "C" void PadClearLegacyAudioHaptics_nid_postfix(int source) {
    std::lock_guard lock(stateMutex);
    slots[0].legacyHaptics.Clear(source);
    if (RefreshAudioHaptics(slots[0])) ++slots[0].output.sequence;
}

extern "C" void PadSubmitAudioHaptics_nid_postfix(std::uint8_t large, std::uint8_t small) {
    PadSubmitAudioHapticsForPort_nid_postfix(0, large, small);
}

extern "C" void PadSetPortConnected_nid_postfix(int port, bool connected) {
    Pad::SetConnected(port, connected);
}
