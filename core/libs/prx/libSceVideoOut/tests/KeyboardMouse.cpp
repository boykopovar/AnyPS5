#include "prx/libScePad/include/InputMapping.hpp"
#include "prx/libScePad/include/KeyboardMouse.hpp"
#include "PadSimulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using PadSimulation::At;
using PadSimulation::KeyEvent;
using PadSimulation::Sample;
using PadSimulation::Simulate;
using PadSimulation::TimedEvent;

constexpr double Polls[] = {33.0, 8.0, 1.0};

void check(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

Pad::InputConfiguration defaults() {
    return Pad::InputConfiguration{{Pad::InputMapping.begin(), Pad::InputMapping.end()}, {}};
}

Pad::InputConfiguration withSettings(const std::function<void(Pad::InputSettings&)>& change) {
    auto configuration = defaults();
    change(configuration.settings);
    return configuration;
}

double axis(std::uint8_t value) {
    return value < 128 ? (static_cast<double>(value) - 128.0) / 128.0 : (static_cast<double>(value) - 128.0) / 127.0;
}

double rightMagnitude(const Sample& sample) {
    return std::hypot(axis(sample.state.sticks[2]), axis(sample.state.sticks[3]));
}

double meanRightX(const std::vector<Sample>& samples, double fromMs, double toMs) {
    double sum = 0.0;
    int count = 0;
    for (const auto& sample : samples) {
        if (sample.ms <= fromMs || sample.ms > toMs) continue;
        sum += axis(sample.state.sticks[2]);
        ++count;
    }
    check(count > 0, "no samples in window");
    return sum / count;
}

double heldMeanRightX(const std::vector<Sample>& samples, double pollMs, double fromMs, double toMs) {
    double sum = 0.0;
    for (const auto& sample : samples) {
        if (sample.ms >= fromMs - 1e-9 && sample.ms < toMs - 1e-9) sum += axis(sample.state.sticks[2]) * pollMs;
    }
    return sum / (toMs - fromMs);
}

std::vector<TimedEvent> mouseOnThenMotion(double durationMs, double countsPerSecondX, double countsPerSecondY = 0.0) {
    std::vector<TimedEvent> events{PadSimulation::MouseModeAt(0.0, true)};
    PadSimulation::AppendMotion(events, 0.0, durationMs, countsPerSecondX, countsPerSecondY);
    return events;
}

double expectedDeflection(const Pad::InputSettings& settings, double normalizedSpeed) {
    const double clamped = std::min(normalizedSpeed, settings.mouseMaxSpeed);
    return settings.mouseDeadzoneOffset + (1.0 - settings.mouseDeadzoneOffset) * std::pow(std::min(clamped, 1.0), settings.mouseCurve);
}

void testCenteredWithoutMotion() {
    for (const double poll : Polls) {
        const auto samples = Simulate(defaults(), {PadSimulation::MouseModeAt(0.0, true)}, poll, 300.0);
        for (const auto& sample : samples) {
            check(sample.state.sticks[2] == 128 && sample.state.sticks[3] == 128, "a resting mouse leaves the stick centered");
        }
    }
}

void testSteadyDeflectionFollowsCurve(const Pad::InputConfiguration& configuration, const char* name) {
    const auto& settings = configuration.settings;
    for (const double speed : {0.05, 0.25, 0.5, 0.75, 0.95, 3.0}) {
        const double countsPerSecond = speed * Pad::MouseFullTiltCountsPerSecond / settings.mouseSensitivity;
        const double expected = expectedDeflection(settings, speed);
        for (const double poll : Polls) {
            const auto samples = Simulate(configuration, mouseOnThenMotion(600.0, countsPerSecond), poll, 600.0);
            const double measured = meanRightX(samples, 300.0, 600.0);
            if (std::abs(measured - expected) > 0.02) {
                throw std::runtime_error(std::string(name) + ": speed " + std::to_string(speed) + " at " + std::to_string(poll) + " ms polls gives " +
                    std::to_string(measured) + ", expected " + std::to_string(expected));
            }
        }
    }
}

void testDeflectionCurve() {
    testSteadyDeflectionFollowsCurve(defaults(), "defaults");
    testSteadyDeflectionFollowsCurve(withSettings([](auto& s) { s.mouseCurve = 2.0; s.mouseDeadzoneOffset = 0.1; s.mouseSensitivity = 2.0; }), "quadratic");
    testSteadyDeflectionFollowsCurve(withSettings([](auto& s) { s.mouseCurve = 0.5; s.mouseDeadzoneOffset = 0.0; }), "square root");
}

void testDeadzoneOffset() {
    const auto configuration = defaults();
    for (const double poll : Polls) {
        const auto samples = Simulate(configuration, mouseOnThenMotion(400.0, 40.0), poll, 400.0);
        bool moved = false;
        for (const auto& sample : samples) {
            const double magnitude = rightMagnitude(sample);
            if (magnitude == 0.0) continue;
            moved = true;
            check(magnitude >= configuration.settings.mouseDeadzoneOffset - 0.01, "the slowest movement still clears the deadzone offset at " + std::to_string(poll) + " ms polls");
        }
        check(moved, "slow movement moves the stick");
    }
    const auto samples = Simulate(withSettings([](auto& s) { s.mouseDeadzoneOffset = 0.0; }), mouseOnThenMotion(400.0, 40.0), 8.0, 400.0);
    check(meanRightX(samples, 200.0, 400.0) < 0.05, "without an offset slow movement stays small");
}

double centeredAfter(const std::vector<Sample>& samples, double stopMs) {
    double lastMoving = stopMs;
    double previous = 2.0;
    for (const auto& sample : samples) {
        if (sample.ms <= stopMs) continue;
        const double magnitude = rightMagnitude(sample);
        check(magnitude <= previous + 1e-9, "the stick never grows after the mouse stops");
        previous = magnitude;
        if (magnitude != 0.0) lastMoving = sample.ms;
    }
    return lastMoving - stopMs;
}

void testDecay() {
    const auto configuration = defaults();
    for (const double speed : {0.3, 1.0}) {
        for (const double poll : Polls) {
            const auto samples = Simulate(configuration, mouseOnThenMotion(300.0, speed * Pad::MouseFullTiltCountsPerSecond), poll, 600.0);
            const double tail = centeredAfter(samples, 300.0);
            check(tail < configuration.settings.mouseDecayMs + poll, "stick centered within decay plus one poll at " + std::to_string(poll) + " ms polls, took " + std::to_string(tail));
            check(samples.back().state.sticks[2] == 128 && samples.back().state.sticks[3] == 128, "stick returns exactly to center");
        }
    }
    const auto slow = withSettings([](auto& s) { s.mouseDecayMs = 120.0; });
    const auto samples = Simulate(slow, mouseOnThenMotion(300.0, Pad::MouseFullTiltCountsPerSecond), 1.0, 600.0);
    const double tail = centeredAfter(samples, 300.0);
    check(tail >= 100.0 && tail < 121.0, "a longer decay keeps the stick moving longer");
}

void testMaxSpeedClamp() {
    const auto configuration = defaults();
    constexpr double StopMs = 264.0;
    for (const double poll : Polls) {
        const auto flick = Simulate(configuration, mouseOnThenMotion(StopMs, 10.0 * Pad::MouseFullTiltCountsPerSecond), poll, 500.0);
        const auto full = Simulate(configuration, mouseOnThenMotion(StopMs, 1.5 * Pad::MouseFullTiltCountsPerSecond), poll, 500.0);
        check(flick[flick.size() / 3].state.sticks[2] == 255, "a fast flick reaches full tilt");
        check(centeredAfter(flick, StopMs) <= centeredAfter(full, StopMs) + 1e-9, "a flick faster than full tilt does not extend the tail");
        double flickSum = 0.0;
        double fullSum = 0.0;
        for (std::size_t index = 0; index < flick.size(); ++index) {
            if (flick[index].ms <= StopMs) continue;
            flickSum += rightMagnitude(flick[index]);
            fullSum += rightMagnitude(full[index]);
        }
        check(std::abs(flickSum - fullSum) <= 0.02 * std::max(1.0, fullSum), "the stick decays from a flick like from full tilt at " + std::to_string(poll) + " ms polls");
    }
    const auto headroom = withSettings([](auto& s) { s.mouseMaxSpeed = 3.0; });
    const auto flick = Simulate(headroom, mouseOnThenMotion(StopMs, 10.0 * Pad::MouseFullTiltCountsPerSecond), 1.0, 500.0);
    const auto full = Simulate(defaults(), mouseOnThenMotion(StopMs, 10.0 * Pad::MouseFullTiltCountsPerSecond), 1.0, 500.0);
    check(rightMagnitude(flick[275]) > rightMagnitude(full[275]), "a higher max speed lets a flick hold full tilt longer");
}

void testSmoothing() {
    const auto instant = Simulate(withSettings([](auto& s) { s.mouseSmoothingMs = 0.0; }), mouseOnThenMotion(100.0, Pad::MouseFullTiltCountsPerSecond / 2.0), 8.0, 100.0);
    check(std::abs(axis(instant[1].state.sticks[2]) - expectedDeflection(defaults().settings, 0.5)) < 0.02, "without smoothing the stick follows at once");
    const auto smoothed = Simulate(defaults(), mouseOnThenMotion(100.0, Pad::MouseFullTiltCountsPerSecond / 2.0), 1.0, 100.0);
    const double early = (axis(smoothed[9].state.sticks[2]) - 0.2) / 0.8;
    check(early > 0.5 * 0.55 && early < 0.5 * 0.72, "smoothing reaches about 63 percent after one time constant");
}

std::vector<TimedEvent> waveMotion(double base, double swing) {
    constexpr double Pi = 3.14159265358979323846;
    std::vector<TimedEvent> events{PadSimulation::MouseModeAt(0.0, true)};
    for (int segment = 0; segment < 200; ++segment) {
        const double speed = (base + swing * std::sin(2.0 * Pi * segment * 8.0 / 800.0)) * Pad::MouseFullTiltCountsPerSecond;
        PadSimulation::AppendMotion(events, segment * 8.0, 8.0, speed, 0.3 * speed);
    }
    PadSimulation::AppendMotion(events, 1700.0, 200.0, 250.0, -120.0);
    return events;
}

double integratedRightMagnitude(const std::vector<Sample>& samples, double pollMs) {
    double sum = 0.0;
    for (const auto& sample : samples) sum += rightMagnitude(sample) * pollMs / 1000.0;
    return sum;
}

void testPollRateIndependence() {
    for (const auto& [name, events] : {std::pair{"one-way wave", waveMotion(0.5, 0.3)}, std::pair{"back-and-forth wave", waveMotion(0.0, 0.6)}}) {
        std::vector<std::vector<Sample>> runs;
        for (const double poll : Polls) runs.push_back(Simulate(defaults(), events, poll, 2112.0));
        const double reference = integratedRightMagnitude(runs[2], Polls[2]);
        for (std::size_t run = 0; run < runs.size(); ++run) {
            const double integrated = integratedRightMagnitude(runs[run], Polls[run]);
            check(std::abs(integrated - reference) <= 0.02 * reference, std::string(name) + ": total deflection at " + std::to_string(Polls[run]) + " ms polls is " +
                std::to_string(integrated) + ", 1 ms gives " + std::to_string(reference));
            const double tolerance = 0.03 + 0.0025 * Polls[run];
            for (double window = 0.0; window + 264.0 <= 2112.0; window += 264.0) {
                const double mean = heldMeanRightX(runs[run], Polls[run], window, window + 264.0);
                const double expected = heldMeanRightX(runs[2], Polls[2], window, window + 264.0);
                check(std::abs(mean - expected) < tolerance, std::string(name) + ": held deflection at " + std::to_string(Polls[run]) + " ms polls from " +
                    std::to_string(window) + " ms is " + std::to_string(mean) + ", 1 ms gives " + std::to_string(expected));
            }
        }
    }
}

void testVerticalScale() {
    const auto configuration = withSettings([](auto& s) { s.mouseVerticalScale = 0.5; s.mouseDeadzoneOffset = 0.0; });
    const auto samples = Simulate(configuration, mouseOnThenMotion(300.0, 0.0, 0.8 * Pad::MouseFullTiltCountsPerSecond), 8.0, 300.0);
    const double y = axis(samples.back().state.sticks[3]);
    check(std::abs(y - 0.4) < 0.02, "vertical scale halves the vertical deflection");
}

void testMouseModeOff() {
    std::vector<TimedEvent> events;
    PadSimulation::AppendMotion(events, 0.0, 300.0, Pad::MouseFullTiltCountsPerSecond, 0.0);
    for (const auto& sample : Simulate(defaults(), events, 8.0, 300.0)) {
        check(sample.state.sticks[2] == 128, "motion without mouse mode leaves the stick alone");
    }
    events.insert(events.begin(), At(0.0, PadSimulation::ButtonEvent(SDL_BUTTON_MIDDLE, true)));
    events.insert(events.begin() + 1, At(0.0, PadSimulation::ButtonEvent(SDL_BUTTON_MIDDLE, false)));
    const auto samples = Simulate(defaults(), events, 8.0, 300.0);
    check(samples.back().state.sticks[2] == 255, "middle click turns mouse mode on");
}

void testKeyboardSticksUnchanged() {
    const std::vector<TimedEvent> events{
        At(0.0, KeyEvent(SDL_SCANCODE_W, true)),
        At(10.0, KeyEvent(SDL_SCANCODE_D, true)),
        At(20.0, KeyEvent(SDL_SCANCODE_A, true)),
        At(30.0, KeyEvent(SDL_SCANCODE_W, false)),
        At(30.0, KeyEvent(SDL_SCANCODE_D, false)),
        At(40.0, KeyEvent(SDL_SCANCODE_A, false))};
    const auto samples = Simulate(defaults(), events, 1.0, 50.0);
    check(samples[4].state.sticks[1] == 0 && samples[4].state.sticks[0] == 128, "W tilts the left stick fully up");
    check(samples[14].state.sticks[0] == 255, "D tilts the left stick fully right");
    check(samples[24].state.sticks[0] == 128, "A and D together cancel");
    check(samples[34].state.sticks[0] == 0 && samples[34].state.sticks[1] == 128, "A alone tilts fully left");
    check(samples[44].state.sticks[0] == 128, "released keys center the stick");
}

void testWheelPress() {
    const auto samples = Simulate(defaults(), {At(0.0, PadSimulation::WheelEvent(1))}, 1.0, 200.0);
    const auto up = static_cast<std::uint32_t>(Pad::PadButton::Up);
    check((samples[40].state.buttons & up) != 0, "wheel up presses Up");
    check((samples[120].state.buttons & up) == 0, "wheel press releases after its duration");
}


void testWheelFocusLoss() {
    const auto samples = Simulate(defaults(), {At(0.0, PadSimulation::WheelEvent(1)), At(10.0, KeyEvent(SDL_SCANCODE_W, true)), At(20.0, PadSimulation::FocusLostEvent())}, 1.0, 40.0);
    check(samples[15].state.sticks[1] == 0, "a held key tilts the stick");
    check(samples[30].state.sticks[1] == 128, "losing focus lets go of held keys");
    check((samples[30].state.buttons & static_cast<std::uint32_t>(Pad::PadButton::Up)) == 0, "losing focus lets go of the wheel press");
}

void testFocusLossReleasesMouse() {
    Pad::KeyboardMouse keyboardMouse(defaults());
    keyboardMouse.SetMouseMode(true);
    const auto requests = keyboardMouse.HandleEvent(PadSimulation::FocusLostEvent(), Pad::KeyboardMouse::Clock::now());
    check(requests.releaseMouse, "losing focus asks to release the captured mouse");
    keyboardMouse.SetMouseMode(false);
    check(!keyboardMouse.HandleEvent(PadSimulation::FocusLostEvent(), Pad::KeyboardMouse::Clock::now()).releaseMouse, "nothing to release without capture");
}

void testToggleRequests() {
    Pad::KeyboardMouse keyboardMouse(defaults());
    const auto now = Pad::KeyboardMouse::Clock::now();
    check(keyboardMouse.HandleEvent(PadSimulation::ButtonEvent(SDL_BUTTON_MIDDLE, true), now).toggleMouse, "middle click asks to toggle the mouse");
    check(!keyboardMouse.HandleEvent(PadSimulation::ButtonEvent(SDL_BUTTON_MIDDLE, false), now).toggleMouse, "releasing it asks for nothing");
    check(keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_F11, true), now).toggleFullscreen, "F11 asks to toggle fullscreen");
    const auto overlay = keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_F10, true), now);
    check(overlay.togglePerformanceOverlay && !overlay.toggleFullscreen, "F10 asks to toggle the performance overlay");
    check(!keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_F10, false), now).togglePerformanceOverlay, "releasing F10 asks for nothing");
}

}

int main() {
    try {
        const std::pair<const char*, std::function<void()>> tests[] = {
            {"centered without motion", testCenteredWithoutMotion},
            {"deflection curve", testDeflectionCurve},
            {"deadzone offset", testDeadzoneOffset},
            {"decay", testDecay},
            {"max speed clamp", testMaxSpeedClamp},
            {"smoothing", testSmoothing},
            {"poll rate independence", testPollRateIndependence},
            {"vertical scale", testVerticalScale},
            {"mouse mode off", testMouseModeOff},
            {"keyboard sticks unchanged", testKeyboardSticksUnchanged},
            {"wheel press", testWheelPress},
            {"wheel and keys on focus loss", testWheelFocusLoss},
            {"focus loss releases the mouse", testFocusLossReleasesMouse},
            {"toggle requests", testToggleRequests},
        };
        for (const auto& [name, test] : tests) {
            test();
            std::printf("pad_keyboard_mouse: %s passed\n", name);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "pad_keyboard_mouse: %s\n", error.what());
        return 1;
    }
    return 0;
}
