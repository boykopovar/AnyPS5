#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/InputCommandFile.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <mutex>
#include <stdexcept>

namespace {
    std::mutex stateMutex;
    PadInputState state;
    PadOutputState output;
    std::uint64_t timestamp = 0;
    std::exception_ptr failure;
    bool initialized = false;

    struct Quat { float w = 1.0f, x = 0.0f, y = 0.0f, z = 0.0f; };
    Quat orientation;
    std::uint64_t lastFuseTime = 0;
    float biasIntegral[3] = {0.0f, 0.0f, 0.0f};
    bool tiltCorrection = true;
    std::uint8_t nextTouchId = 0;
    bool prevTouchActive[2] = {false, false};
    std::uint8_t touchIds[2] = {0, 0};

    void normalize(Quat& q) {
        const float n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
        if (n < 1e-6f) { q = Quat{}; return; }
        q.w /= n; q.x /= n; q.y /= n; q.z /= n;
    }

    void fuse(float dt, const std::array<float, 3>& accel, const std::array<float, 3>& gyro) {
        float gx = gyro[0], gy = gyro[1], gz = gyro[2];
        const float an = std::sqrt(accel[0] * accel[0] + accel[1] * accel[1] + accel[2] * accel[2]);
        if (tiltCorrection && an > 1.0f) {
            const float ax = accel[0] / an, ay = accel[1] / an, az = accel[2] / an;
            const float vx = 2.0f * (orientation.x * orientation.y + orientation.w * orientation.z);
            const float vy = orientation.w * orientation.w - orientation.x * orientation.x + orientation.y * orientation.y - orientation.z * orientation.z;
            const float vz = 2.0f * (orientation.y * orientation.z - orientation.w * orientation.x);
            const float ex = ay * vz - az * vy;
            const float ey = az * vx - ax * vz;
            const float ez = ax * vy - ay * vx;
            constexpr float kp = 1.5f;
            constexpr float ki = 0.02f;
            biasIntegral[0] += ki * ex * dt;
            biasIntegral[1] += ki * ey * dt;
            biasIntegral[2] += ki * ez * dt;
            gx += kp * ex + biasIntegral[0];
            gy += kp * ey + biasIntegral[1];
            gz += kp * ez + biasIntegral[2];
        }
        const float hx = 0.5f * gx * dt, hy = 0.5f * gy * dt, hz = 0.5f * gz * dt;
        const Quat q = orientation;
        orientation.w = q.w - q.x * hx - q.y * hy - q.z * hz;
        orientation.x = q.x + q.w * hx + q.y * hz - q.z * hy;
        orientation.y = q.y + q.w * hy - q.x * hz + q.z * hx;
        orientation.z = q.z + q.w * hz + q.x * hy - q.y * hx;
        normalize(orientation);
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
        timestamp = sceKernelGetProcessTime();
        lastFuseTime = timestamp;
    }
    initialized = true;
}

PadData Pad::ReadState() {
    std::lock_guard lock(stateMutex);
    if (failure) std::rethrow_exception(failure);
    if (!initialized) throw std::runtime_error("Pad: read before initialization");
    const std::uint64_t now = sceKernelGetProcessTime();
    static InputCommandFile commands(std::getenv("APS5_PAD_INPUT_FILE"));
    static std::uint32_t lastInjected = 0;
    const auto injected = commands.Buttons(now);
    if (injected != lastInjected) {
        lastInjected = injected;
        timestamp = now;
    }
    PadData data{};
    data.buttons = state.buttons | injected;
    data.left_stick_x = state.sticks[0];
    data.left_stick_y = state.sticks[1];
    data.right_stick_x = state.sticks[2];
    data.right_stick_y = state.sticks[3];
    data.analog_buttons_l2 = std::max<std::uint8_t>(state.analogButtonsL2, (data.buttons & 0x100) != 0 ? 255 : 0);
    data.analog_buttons_r2 = std::max<std::uint8_t>(state.analogButtonsR2, (data.buttons & 0x200) != 0 ? 255 : 0);

    const bool live = state.hasMotion && output.motionEnabled;
    const std::array<float, 3> rest{0.0f, 9.80665f, 0.0f};
    const std::array<float, 3>& accel = live ? state.accel : rest;
    const std::array<float, 3> gyro = live ? state.gyro : std::array<float, 3>{0.0f, 0.0f, 0.0f};
    if (live) {
        float dt = lastFuseTime != 0 && now > lastFuseTime ? static_cast<float>(now - lastFuseTime) * 1e-6f : 0.0f;
        dt = std::min(dt, 0.1f);
        fuse(dt, accel, gyro);
    } else {
        orientation = Quat{};
        biasIntegral[0] = biasIntegral[1] = biasIntegral[2] = 0.0f;
    }
    lastFuseTime = now;
    data.orientation_x = orientation.x;
    data.orientation_y = orientation.y;
    data.orientation_z = orientation.z;
    data.orientation_w = orientation.w;
    constexpr float kGravity = 9.80665f;
    data.acceleration_x = accel[0] / kGravity;
    data.acceleration_y = accel[1] / kGravity;
    data.acceleration_z = accel[2] / kGravity;
    data.angular_velocity_x = gyro[0];
    data.angular_velocity_y = gyro[1];
    data.angular_velocity_z = gyro[2];

    data.connected = true;
    data.connected_count = 1;
    data.timestamp = timestamp;

    std::uint8_t touchNum = 0;
    for (int i = 0; i < 2; ++i) {
        const PadTouchPoint& tp = state.touch[i];
        if (!tp.active) { prevTouchActive[i] = false; continue; }
        if (!prevTouchActive[i]) touchIds[i] = static_cast<std::uint8_t>(nextTouchId++ & 0x7F);
        prevTouchActive[i] = true;
        if (touchNum == 0) {
            data.touch_data_touch0_x = tp.x; data.touch_data_touch0_y = tp.y; data.touch_data_touch0_id = touchIds[i];
        } else {
            data.touch_data_touch1_x = tp.x; data.touch_data_touch1_y = tp.y; data.touch_data_touch1_id = touchIds[i];
        }
        ++touchNum;
    }
    if (touchNum == 0 && (state.touchLeft || state.touchRight)) {
        data.buttons |= 0x100000;
        touchNum = 1;
        data.touch_data_touch0_x = state.touchRight ? 1440 : 480;
        data.touch_data_touch0_y = 471;
        data.touch_data_touch0_id = 0;
    }
    data.touch_data_touch_num = touchNum;
    return data;
}

void Pad::SetVibration(std::uint8_t large, std::uint8_t small) {
    std::lock_guard lock(stateMutex);
    if (output.vibrationLarge == large && output.vibrationSmall == small) return;
    output.vibrationLarge = large;
    output.vibrationSmall = small;
    ++output.sequence;
}

void Pad::SetVibrationMode(int mode) {
    std::lock_guard lock(stateMutex);
    if (output.vibrationMode == mode) return;
    output.vibrationMode = mode;
    ++output.sequence;
}

void Pad::SetLightBar(bool valid, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    std::lock_guard lock(stateMutex);
    output.lightBarValid = valid;
    output.lightBar[0] = r;
    output.lightBar[1] = g;
    output.lightBar[2] = b;
    ++output.sequence;
}

void Pad::SetTriggerCommand(int trigger, const std::uint8_t* command) {
    if (trigger < 0 || trigger > 1 || command == nullptr) return;
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
    output.trigger[trigger] = req;
    output.triggerTouched = true;
    ++output.sequence;
}

void Pad::ResetOrientation() {
    std::lock_guard lock(stateMutex);
    orientation = Quat{};
    biasIntegral[0] = biasIntegral[1] = biasIntegral[2] = 0.0f;
}

void Pad::SetTiltCorrection(bool enabled) {
    std::lock_guard lock(stateMutex);
    tiltCorrection = enabled;
    biasIntegral[0] = biasIntegral[1] = biasIntegral[2] = 0.0f;
}

void Pad::SetMotionEnabled(bool enabled) {
    std::lock_guard lock(stateMutex);
    if (output.motionEnabled == enabled) return;
    output.motionEnabled = enabled;
    ++output.sequence;
}

extern "C" void PadPublishInput_nid_postfix(const PadInputState& input) {
    std::lock_guard lock(stateMutex);
    if (failure) std::rethrow_exception(failure);
    if (state.buttons == input.buttons && state.sticks == input.sticks &&
        state.analogButtonsL2 == input.analogButtonsL2 && state.analogButtonsR2 == input.analogButtonsR2 &&
        state.touchLeft == input.touchLeft && state.touchRight == input.touchRight &&
        state.hasMotion == input.hasMotion && state.accel == input.accel && state.gyro == input.gyro &&
        state.touch == input.touch && state.deviceKind == input.deviceKind) return;
    state = input;
    timestamp = sceKernelGetProcessTime();
}

extern "C" void PadReportInputFailure_nid_postfix(std::exception_ptr error) {
    if (!error) throw std::invalid_argument("Pad: missing input failure");
    std::lock_guard lock(stateMutex);
    if (!failure) failure = error;
}

extern "C" bool PadFetchOutput_nid_postfix(std::uint32_t* seenSequence, PadOutputState* out) {
    std::lock_guard lock(stateMutex);
    if (seenSequence == nullptr || out == nullptr || *seenSequence == output.sequence) return false;
    *seenSequence = output.sequence;
    *out = output;
    return true;
}
