#include "prx/libScePad/include/InputMapping.hpp"
#include "prx/libScePad/include/KeyboardMouse.hpp"
#include "PadSimulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <tuple>
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

void testSlowReportingMouse() {
    const double speed = 0.5 * Pad::MouseFullTiltCountsPerSecond;
    const double expected = expectedDeflection(defaults().settings, 0.5);
    for (const double reportHz : {125.0, 250.0, 1000.0}) {
        std::vector<TimedEvent> events{PadSimulation::MouseModeAt(0.0, true)};
        PadSimulation::AppendMotion(events, 0.0, 2000.0, speed, 0.0, reportHz);
        for (const double poll : {16.7, 8.3, 6.9, 1.0}) {
            const auto samples = Simulate(defaults(), events, poll, 2000.0);
            double low = 1.0;
            double high = 0.0;
            for (const auto& sample : samples) {
                if (sample.ms < 1000.0) continue;
                low = std::min(low, axis(sample.state.sticks[2]));
                high = std::max(high, axis(sample.state.sticks[2]));
            }
            check(high - low < 0.125, "a " + std::to_string(reportHz) + " Hz mouse at " + std::to_string(poll) + " ms polls wavers by " + std::to_string(high - low));
            check(std::abs((high + low) / 2.0 - expected) < 0.05, "a " + std::to_string(reportHz) + " Hz mouse at " + std::to_string(poll) + " ms polls stays around the expected tilt");
        }
    }
}

void testReportRateSmoothing() {
    for (const double reportHz : {125.0, 250.0, 1000.0}) {
        Pad::MouseStick mouse(defaults().settings);
        check(mouse.SmoothingMs() == defaults().settings.mouseSmoothingMs, "smoothing starts at the setting");
        const double periodSeconds = 1.0 / reportHz;
        for (int report = 0; report < static_cast<int>(reportHz); ++report) {
            mouse.Move(6, 0);
            mouse.Advance(periodSeconds);
        }
        check(std::abs(mouse.ReportRate() - reportHz) < 0.1 * reportHz, "measured " + std::to_string(mouse.ReportRate()) + " reports per second for a " + std::to_string(reportHz) + " Hz mouse");
        const double smoothing = mouse.SmoothingMs();
        check(std::abs(smoothing - std::max(10.0, 3000.0 / reportHz)) < 0.15 * smoothing, "smoothing " + std::to_string(smoothing) + " ms for a " + std::to_string(reportHz) + " Hz mouse");
    }
    Pad::MouseStick slow(defaults().settings);
    for (int step = 0; step < 40; ++step) {
        slow.Move(30, 0);
        slow.Advance(0.025);
    }
    check(std::abs(slow.ReportRate() - 40.0) < 2.0 && slow.SmoothingMs() == Pad::MouseSmoothingLimitMs, "smoothing for very slow reports stops at 30 ms");
    auto off = defaults().settings;
    off.mouseSmoothingMs = 0.0;
    Pad::MouseStick unsmoothed(off);
    for (int report = 0; report < 125; ++report) {
        unsmoothed.Move(6, 0);
        unsmoothed.Advance(0.008);
    }
    check(unsmoothed.SmoothingMs() == 0.0, "a smoothing of 0 stays off for a slow mouse");
    Pad::MouseStick single(defaults().settings);
    for (int report = 0; report < 125; ++report) {
        single.Move(1, 0);
        single.Advance(0.008);
    }
    check(single.ReportRate() == 0.0 && single.SmoothingMs() == defaults().settings.mouseSmoothingMs, "single-count reports are not taken for a slow mouse");
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

Pad::InputConfiguration withModifier(double scale) {
    auto configuration = defaults();
    configuration.bindings.push_back({SDL_SCANCODE_Z, Pad::MouseButton::None, Pad::InputControl::StickModifier});
    configuration.bindings.push_back({SDL_SCANCODE_UNKNOWN, Pad::MouseButton::X2, Pad::InputControl::StickModifier});
    configuration.settings.stickModifierScale = scale;
    return configuration;
}

void testStickModifier() {
    const std::vector<TimedEvent> events{
        At(0.0, KeyEvent(SDL_SCANCODE_W, true)),
        At(10.0, KeyEvent(SDL_SCANCODE_Z, true)),
        At(20.0, KeyEvent(SDL_SCANCODE_D, true)),
        At(30.0, KeyEvent(SDL_SCANCODE_A, true)),
        At(40.0, KeyEvent(SDL_SCANCODE_Z, false)),
        At(50.0, KeyEvent(SDL_SCANCODE_A, false)),
        At(60.0, KeyEvent(SDL_SCANCODE_D, false)),
        At(70.0, PadSimulation::ButtonEvent(SDL_BUTTON_X2, true)),
        At(80.0, KeyEvent(SDL_SCANCODE_S, true)),
        At(90.0, PadSimulation::ButtonEvent(SDL_BUTTON_X2, false))};
    const auto samples = Simulate(withModifier(0.5), events, 1.0, 100.0);
    check(samples[4].state.sticks[1] == 0, "keys tilt fully before the modifier is held");
    check(samples[14].state.sticks[1] == 64 && samples[14].state.sticks[0] == 128, "the modifier halves the tilt of a held key");
    check(samples[24].state.sticks[0] == 192 && samples[24].state.sticks[1] == 64, "the modifier halves a second axis");
    check(samples[34].state.sticks[0] == 128, "opposite keys still cancel under the modifier");
    check(samples[44].state.sticks[1] == 0 && samples[44].state.sticks[0] == 128, "letting go of the modifier restores the full tilt");
    check(samples[74].state.sticks[1] == 64 && samples[74].state.sticks[0] == 128, "a modifier on a mouse button scales the held keys");
    check(samples[84].state.sticks[1] == 128, "opposite keys cancel under a mouse button modifier");
    check(samples[94].state.sticks[1] == 128, "opposite keys still cancel once the mouse button modifier is released");
    const auto alone = Simulate(withModifier(0.5), {At(0.0, KeyEvent(SDL_SCANCODE_Z, true))}, 1.0, 10.0);
    check(alone.back().state.sticks[0] == 128 && alone.back().state.sticks[1] == 128, "the modifier alone moves no stick");
    const auto quarter = Simulate(withModifier(0.25), {At(0.0, KeyEvent(SDL_SCANCODE_Z, true)), At(0.0, KeyEvent(SDL_SCANCODE_D, true))}, 1.0, 10.0);
    check(quarter.back().state.sticks[0] == 160, "the scale setting sets the tilt");
    const auto full = Simulate(withModifier(1.0), {At(0.0, KeyEvent(SDL_SCANCODE_Z, true)), At(0.0, KeyEvent(SDL_SCANCODE_D, true))}, 1.0, 10.0);
    check(full.back().state.sticks[0] == 255, "a scale of 1 tilts fully");
}

int byteDistance(std::uint8_t value, int expected) {
    return std::abs(static_cast<int>(value) - expected);
}

void testKeyboardRamp() {
    auto configuration = withModifier(0.5);
    configuration.settings.keyboardRampMs = 100.0;
    const std::vector<TimedEvent> events{At(0.0, KeyEvent(SDL_SCANCODE_W, true)), At(150.0, KeyEvent(SDL_SCANCODE_W, false))};
    const auto samples = Simulate(configuration, events, 1.0, 400.0);
    check(byteDistance(samples[20].state.sticks[1], 102) <= 1, "a ramp of 100 ms is a fifth of the way after 20 ms");
    check(byteDistance(samples[50].state.sticks[1], 64) <= 1, "a ramp of 100 ms is halfway after 50 ms");
    check(samples[119].state.sticks[1] == 0, "a ramp ends at full tilt");
    for (std::size_t index = 2; index < 119; ++index) check(samples[index].state.sticks[1] <= samples[index - 1].state.sticks[1], "the stick only moves on while the key is held");
    check(byteDistance(samples[199].state.sticks[1], 64) <= 1, "letting go ramps back at the same speed");
    for (std::size_t index = 152; index < 259; ++index) check(samples[index].state.sticks[1] >= samples[index - 1].state.sticks[1], "the stick only moves back after the key is let go");
    check(samples[259].state.sticks[1] == 128 && samples[259].state.sticks[0] == 128, "the stick ends centered");
    const std::vector<TimedEvent> modified{At(0.0, KeyEvent(SDL_SCANCODE_Z, true)), At(0.0, KeyEvent(SDL_SCANCODE_W, true)), PadSimulation::ReleaseAt(80.0)};
    const auto scaled = Simulate(configuration, modified, 1.0, 120.0);
    check(byteDistance(scaled[20].state.sticks[1], 102) <= 1, "a ramp toward a modified tilt moves at the ramp speed");
    check(scaled[70].state.sticks[1] == 64, "a ramp toward a modified tilt stops at it");
    check(scaled[99].state.sticks[1] == 128, "releasing every key centers a ramped stick at once");
    const auto held = Simulate(configuration, {At(0.0, KeyEvent(SDL_SCANCODE_Z, true)), At(0.0, KeyEvent(SDL_SCANCODE_W, true))}, 1.0, 120.0);
    check(held[99].state.sticks[1] == 64, "a ramp stops at the modified tilt");
}

void testRampKeepsAdvanceReporting() {
    auto configuration = defaults();
    configuration.settings.keyboardRampMs = 100.0;
    Pad::KeyboardMouse keyboardMouse(configuration);
    const auto start = Pad::KeyboardMouse::Clock::now();
    keyboardMouse.Advance(start);
    check(!keyboardMouse.Advance(start + std::chrono::milliseconds(5)), "nothing moves while no key is held");
    keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_W, true), start + std::chrono::milliseconds(10));
    check(keyboardMouse.Advance(start + std::chrono::milliseconds(20)), "a ramping stick asks for a new state");
    keyboardMouse.Advance(start + std::chrono::milliseconds(200));
    check(!keyboardMouse.Advance(start + std::chrono::milliseconds(210)), "a stick at its tilt asks for nothing");
}

void testTriggerRamp() {
    const auto r2 = static_cast<std::uint32_t>(Pad::PadButton::R2);
    const std::vector<TimedEvent> events{At(0.0, PadSimulation::ButtonEvent(SDL_BUTTON_RIGHT, true)), At(150.0, PadSimulation::ButtonEvent(SDL_BUTTON_RIGHT, false))};
    const auto instant = Simulate(defaults(), events, 1.0, 10.0);
    check(instant[0].state.analogButtonsR2 == 255 && (instant[0].state.buttons & r2) != 0, "without a ramp a press is full at once");
    auto configuration = defaults();
    configuration.settings.triggerRampMs = 100.0;
    const auto samples = Simulate(configuration, events, 1.0, 400.0);
    check(samples[0].state.analogButtonsR2 == 0 && (samples[0].state.buttons & r2) == 0, "a ramped press starts at 0");
    check(byteDistance(samples[50].state.analogButtonsR2, 128) <= 2 && (samples[50].state.buttons & r2) != 0, "a ramp of 100 ms is halfway after 50 ms");
    for (std::size_t index = 2; index < 119; ++index) check(samples[index].state.analogButtonsR2 >= samples[index - 1].state.analogButtonsR2, "the trigger only presses on while the button is held");
    check(samples[119].state.analogButtonsR2 == 255, "a ramp ends at a full press");
    check(byteDistance(samples[199].state.analogButtonsR2, 128) <= 2, "letting go ramps back at the same speed");
    check(samples[259].state.analogButtonsR2 == 0 && (samples[259].state.buttons & r2) == 0, "the trigger ends released");
    check(samples[119].state.analogButtonsL2 == 0, "the other trigger is left alone");
    Pad::KeyboardMouse keyboardMouse(configuration);
    const auto start = Pad::KeyboardMouse::Clock::now();
    keyboardMouse.Advance(start);
    keyboardMouse.HandleEvent(PadSimulation::ButtonEvent(SDL_BUTTON_RIGHT, true), start);
    keyboardMouse.Advance(start + std::chrono::milliseconds(30));
    PadInputState controller;
    controller.analogButtonsR2 = 200;
    check(keyboardMouse.Compose(controller).analogButtonsR2 == 200, "a controller trigger above the ramp is passed on");
    controller.analogButtonsR2 = 10;
    check(byteDistance(keyboardMouse.Compose(controller).analogButtonsR2, 77) <= 1, "a ramp above a controller trigger is passed on");
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

void testShake() {
    const auto samples = Simulate(defaults(), {At(0.0, KeyEvent(SDL_SCANCODE_X, true)), At(200.0, KeyEvent(SDL_SCANCODE_X, false))}, 10.0, 300.0);
    const auto& first = samples[1].state;
    const auto& second = samples[6].state;
    check(first.hasMotion && second.hasMotion, "a held shake key reports motion");
    check(std::fabs(first.accel[0]) == Pad::ShakeAcceleration && second.accel[0] == -first.accel[0], "the shake acceleration flips every half period");
    check(second.gyro[2] == -first.gyro[2] && std::fabs(first.gyro[2]) == Pad::ShakeAngularVelocity, "the shake angular velocity flips with it");
    check(!samples[25].state.hasMotion && samples[25].state.accel == PadInputState{}.accel, "releasing the shake key stops the motion");
    Pad::KeyboardMouse keyboardMouse(defaults());
    const auto now = Pad::KeyboardMouse::Clock::now();
    keyboardMouse.Advance(now);
    keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_X, true), now);
    check(keyboardMouse.Advance(now + Pad::ShakeHalfPeriod), "a held shake key keeps the state changing");
}

void testToggleRequests() {
    Pad::KeyboardMouse keyboardMouse(defaults());
    const auto now = Pad::KeyboardMouse::Clock::now();
    check(keyboardMouse.HandleEvent(PadSimulation::ButtonEvent(SDL_BUTTON_MIDDLE, true), now).toggleMouse, "middle click asks to toggle the mouse");
    check(!keyboardMouse.HandleEvent(PadSimulation::ButtonEvent(SDL_BUTTON_MIDDLE, false), now).toggleMouse, "releasing it asks for nothing");
    check(keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_F7, true), now).toggleMouse, "F7 asks to toggle the mouse");
    check(!keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_F7, false), now).toggleMouse, "releasing F7 asks for nothing");
    check(keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_F11, true), now).toggleFullscreen, "F11 asks to toggle fullscreen");
    const auto overlay = keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_F10, true), now);
    check(overlay.togglePerformanceOverlay && !overlay.toggleFullscreen, "F10 asks to toggle the performance overlay");
    check(!keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_F10, false), now).togglePerformanceOverlay, "releasing F10 asks for nothing");
    const auto screenshot = keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_F12, true), now);
    check(screenshot.screenshot && !screenshot.toggleFullscreen && !screenshot.togglePerformanceOverlay, "F12 asks for a screenshot");
    auto repeated = KeyEvent(SDL_SCANCODE_F12, true);
    repeated.key.repeat = 1;
    check(!keyboardMouse.HandleEvent(repeated, now).screenshot, "a repeated F12 asks for nothing");
    check(!keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_F12, false), now).screenshot, "releasing F12 asks for nothing");
}

void testShowPadStatus() {
    const auto now = Pad::KeyboardMouse::Clock::now();
    Pad::KeyboardMouse keyboardMouse(defaults());
    const auto pressed = keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_F9, true), now);
    check(pressed.showPadStatus, "F9 asks to show the pad status");
    check(!pressed.toggleFullscreen && !pressed.togglePerformanceOverlay && !pressed.screenshot && !pressed.toggleMouse, "F9 asks for more than the pad status");
    check(keyboardMouse.Compose(PadInputState{}).buttons == 0, "F9 presses a pad button");
    auto repeated = KeyEvent(SDL_SCANCODE_F9, true);
    repeated.key.repeat = 1;
    check(!keyboardMouse.HandleEvent(repeated, now).showPadStatus, "a repeated F9 asks again");
    check(!keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_F9, false), now).showPadStatus, "releasing F9 asks for the pad status");
    check(keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_F9, true), now).showPadStatus, "pressing F9 again does not ask again");
    keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_F9, false), now);
    for (const auto key : {SDL_SCANCODE_F10, SDL_SCANCODE_F11, SDL_SCANCODE_F12, SDL_SCANCODE_F7, SDL_SCANCODE_F8, SDL_SCANCODE_F6, SDL_SCANCODE_F5}) {
        check(!keyboardMouse.HandleEvent(KeyEvent(key, true), now).showPadStatus, "another function key asks for the pad status");
        keyboardMouse.HandleEvent(KeyEvent(key, false), now);
    }
    check(!keyboardMouse.HandleEvent(PadSimulation::ButtonEvent(SDL_BUTTON_MIDDLE, true), now).showPadStatus, "a mouse button asks for the pad status");
    auto moved = defaults();
    std::erase_if(moved.bindings, [](const Pad::InputBinding& binding) { return binding.control == Pad::InputControl::ShowPadStatus; });
    moved.bindings.push_back({SDL_SCANCODE_F4, Pad::MouseButton::None, Pad::InputControl::ShowPadStatus});
    Pad::KeyboardMouse rebound(moved);
    check(!rebound.HandleEvent(KeyEvent(SDL_SCANCODE_F9, true), now).showPadStatus, "the old key still asks after a rebind");
    check(rebound.HandleEvent(KeyEvent(SDL_SCANCODE_F4, true), now).showPadStatus, "the new key does not ask after a rebind");
    auto shared = defaults();
    shared.bindings.push_back({SDL_SCANCODE_F10, Pad::MouseButton::None, Pad::InputControl::ShowPadStatus});
    Pad::KeyboardMouse conflicting(shared);
    const auto both = conflicting.HandleEvent(KeyEvent(SDL_SCANCODE_F10, true), now);
    check(both.showPadStatus && both.togglePerformanceOverlay, "a key shared with the overlay does not do both");
}

void testDefaultL2() {
    const auto l2 = static_cast<std::uint32_t>(Pad::PadButton::L2);
    const auto r2 = static_cast<std::uint32_t>(Pad::PadButton::R2);
    const auto now = Pad::KeyboardMouse::Clock::now();
    for (const auto& [name, press, release] : {
             std::tuple{"R", KeyEvent(SDL_SCANCODE_R, true), KeyEvent(SDL_SCANCODE_R, false)},
             std::tuple{"mouse button 4", PadSimulation::ButtonEvent(SDL_BUTTON_X1, true), PadSimulation::ButtonEvent(SDL_BUTTON_X1, false)}}) {
        Pad::KeyboardMouse keyboardMouse(defaults());
        keyboardMouse.HandleEvent(press, now);
        const auto held = keyboardMouse.Compose(PadInputState{});
        check((held.buttons & l2) != 0 && held.analogButtonsL2 == 255, std::string(name) + " presses L2 fully");
        check((held.buttons & r2) == 0 && held.analogButtonsR2 == 0, std::string(name) + " leaves R2 alone");
        keyboardMouse.HandleEvent(release, now);
        const auto released = keyboardMouse.Compose(PadInputState{});
        check((released.buttons & l2) == 0 && released.analogButtonsL2 == 0, std::string(name) + " released lets go of L2");
    }
}

void testTitleReadsMouse() {
    const auto square = static_cast<std::uint32_t>(Pad::PadButton::Square);
    const auto r2 = static_cast<std::uint32_t>(Pad::PadButton::R2);
    const auto up = static_cast<std::uint32_t>(Pad::PadButton::Up);
    const auto down = static_cast<std::uint32_t>(Pad::PadButton::Down);
    const auto wheelDirections = up | down;
    auto now = Pad::KeyboardMouse::Clock::now();
    Pad::KeyboardMouse keyboardMouse(defaults());
    keyboardMouse.HandleEvent(PadSimulation::ButtonEvent(SDL_BUTTON_LEFT, true), now);
    check((keyboardMouse.Compose(PadInputState{}).buttons & square) != 0, "a left click presses Square while the title does not read the mouse");
    keyboardMouse.SetTitleReadsMouse(true);
    keyboardMouse.HandleEvent(PadSimulation::ButtonEvent(SDL_BUTTON_LEFT, false), now);
    check((keyboardMouse.Compose(PadInputState{}).buttons & square) == 0, "releasing a button pressed before the title read the mouse left Square held");
    keyboardMouse.HandleEvent(PadSimulation::ButtonEvent(SDL_BUTTON_LEFT, true), now);
    keyboardMouse.HandleEvent(PadSimulation::ButtonEvent(SDL_BUTTON_RIGHT, true), now);
    const auto clicked = keyboardMouse.Compose(PadInputState{});
    check((clicked.buttons & (square | r2)) == 0 && clicked.analogButtonsR2 == 0, "a click while the title reads the mouse pressed a pad button");
    keyboardMouse.HandleEvent(PadSimulation::WheelEvent(1), now);
    check((keyboardMouse.Compose(PadInputState{}).buttons & wheelDirections) == 0, "wheel up pressed a pad direction while the title reads the mouse");
    keyboardMouse.HandleEvent(PadSimulation::WheelEvent(-1), now);
    check((keyboardMouse.Compose(PadInputState{}).buttons & wheelDirections) == 0, "wheel down pressed a pad direction while the title reads the mouse");
    check(!keyboardMouse.HandleEvent(PadSimulation::ButtonEvent(SDL_BUTTON_MIDDLE, true), now).toggleMouse, "a middle click while the title reads the mouse asked to toggle mouse mode");
    keyboardMouse.HandleEvent(KeyEvent(SDL_SCANCODE_W, true), now);
    check(keyboardMouse.Compose(PadInputState{}).sticks[1] == 0, "keys stopped reaching the pad while the title reads the mouse");
    keyboardMouse.SetMouseMode(true);
    keyboardMouse.Advance(now);
    for (int step = 1; step <= 40; ++step) {
        now += std::chrono::milliseconds(8);
        keyboardMouse.HandleEvent(PadSimulation::MotionEvent(200, 0), now);
        keyboardMouse.Advance(now);
    }
    check(keyboardMouse.Compose(PadInputState{}).sticks[2] == 128, "motion tilted the right stick while the title reads the mouse");
    keyboardMouse.SetTitleReadsMouse(false);
    keyboardMouse.HandleEvent(PadSimulation::WheelEvent(1), now);
    check((keyboardMouse.Compose(PadInputState{}).buttons & up) != 0, "wheel bindings did not resume after the title stopped reading the mouse");
    for (int step = 1; step <= 40; ++step) {
        now += std::chrono::milliseconds(8);
        keyboardMouse.HandleEvent(PadSimulation::MotionEvent(200, 0), now);
        keyboardMouse.Advance(now);
    }
    check(keyboardMouse.Compose(PadInputState{}).sticks[2] > 128, "motion in mouse mode no longer tilts the stick once the title stops reading the mouse");
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
            {"slow reporting mouse", testSlowReportingMouse},
            {"report rate smoothing", testReportRateSmoothing},
            {"poll rate independence", testPollRateIndependence},
            {"vertical scale", testVerticalScale},
            {"mouse mode off", testMouseModeOff},
            {"keyboard sticks unchanged", testKeyboardSticksUnchanged},
            {"stick modifier", testStickModifier},
            {"keyboard ramp", testKeyboardRamp},
            {"trigger ramp", testTriggerRamp},
            {"ramp keeps advance reporting", testRampKeepsAdvanceReporting},
            {"wheel press", testWheelPress},
            {"wheel and keys on focus loss", testWheelFocusLoss},
            {"focus loss releases the mouse", testFocusLossReleasesMouse},
            {"shake", testShake},
            {"toggle requests", testToggleRequests},
            {"show pad status", testShowPadStatus},
            {"default L2", testDefaultL2},
            {"title reads the mouse", testTitleReadsMouse},
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
