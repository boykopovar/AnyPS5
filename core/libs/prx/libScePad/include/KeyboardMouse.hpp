#ifndef CORE_LIBS_PRX_LIBSCEPAD_KEYBOARDMOUSE_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_KEYBOARDMOUSE_HPP

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>
#include "SDL_events.h"

#include "InputMapping.hpp"
#include "PadState.hpp"

namespace Pad {

inline constexpr double MouseFullTiltCountsPerSecond = 3000.0;
inline constexpr double MouseMinimumStepSeconds = 0.0005;
inline constexpr double MouseRateWindowSeconds = 0.25;
inline constexpr double MouseRateGapSeconds = 0.05;
inline constexpr double MouseSmoothingReports = 3.0;
inline constexpr double MouseSmoothingLimitMs = 30.0;
inline constexpr std::chrono::milliseconds ShakeHalfPeriod{50};
inline constexpr std::chrono::milliseconds TurboPeriod{100};
inline constexpr float ShakeAcceleration = 2.0f * 9.80665f;
inline constexpr float ShakeAngularVelocity = 6.0f;
inline constexpr double MouseGyroRadiansPerCount = 0.001;

class MouseStick {
public:
    explicit MouseStick(const InputSettings& settings);
    void Move(int deltaX, int deltaY);
    void Advance(double seconds);
    void Reset();
    double Speed() const;
    double ReportRate() const;
    double SmoothingMs() const;
    std::array<double, 2> Deflection() const;

private:
    void measureReportRate(double step, bool moved);

    InputSettings settings;
    double pendingX = 0.0;
    double pendingY = 0.0;
    double pendingSeconds = 0.0;
    double pendingCounts = 0.0;
    int pendingReports = 0;
    double speedX = 0.0;
    double speedY = 0.0;
    double idleSeconds = 0.0;
    double rateSeconds = 0.0;
    double rateCounts = 0.0;
    int rateReports = 0;
    double reportRate = 0.0;
};

struct InputRequests {
    bool toggleFullscreen = false;
    bool togglePerformanceOverlay = false;
    bool screenshot = false;
    bool showPadStatus = false;
    bool volumeUp = false;
    bool volumeDown = false;
    bool toggleMute = false;
    bool toggleMouse = false;
    bool reloadInputMapping = false;
    bool releaseMouse = false;
};

class KeyboardMouse {
public:
    using Clock = std::chrono::steady_clock;

    explicit KeyboardMouse(InputConfiguration configuration);
    void Replace(InputConfiguration next);
    InputRequests HandleEvent(const SDL_Event& event, Clock::time_point now);
    InputRequests PadChordRequests(std::uint32_t buttons);
    bool Advance(Clock::time_point now);
    void SetMouseMode(bool enabled);
    void SetMouseTouchpad(bool enabled);
    void SetMouseGyro(bool enabled);
    void SetViewport(int width, int height);
    void SetTitleReadsMouse(bool reads);
    void Release();
    bool MouseMode() const;
    PadInputState Compose(const PadInputState& controller) const;
    const InputConfiguration& Configuration() const;

private:
    struct StickInput {
        std::array<double, 4> targets{};
        std::array<bool, 4> held{};
    };

    StickInput stickInput() const;
    std::array<double, 2> triggerTargets() const;
    bool advanceAxes(Clock::time_point now);
    bool active(std::size_t index) const;

    InputConfiguration configuration;
    std::vector<bool> pressed;
    std::vector<bool> latched;
    std::vector<Clock::time_point> pressedAt;
    std::vector<Clock::time_point> wheelReleaseTimes;
    MouseStick mouse;
    Clock::time_point lastAdvance{};
    Clock::time_point lastAxisAdvance{};
    std::array<double, 4> stickAxes{};
    std::array<double, 2> triggerAxes{};
    bool advanced = false;
    bool axesAdvanced = false;
    bool mouseMode = false;
    bool titleReadsMouse = false;
    bool mouseTouchpad = false;
    bool mouseGyro = false;
    double gyroCountsX = 0.0;
    double gyroCountsY = 0.0;
    std::array<double, 2> gyroRate{};
    int viewportWidth = 0;
    int viewportHeight = 0;
    std::optional<PadTouchPoint> mouseTouch;
    std::optional<PadTouchPoint> touchAt(int x, int y) const;
    std::uint32_t padButtons = 0;
};

std::uint8_t StickByte(double deflection);

}

#endif
