#ifndef CORE_LIBS_PRX_LIBSCEPAD_PADIDENTIFY_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_PADIDENTIFY_HPP

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "prx/libScePad/include/PadRumbleIntensity.hpp"

namespace Pad {

enum class IdentifyMode : std::uint8_t { Off, Connect, Always };

inline IdentifyMode ParseIdentifyMode(const char* text) {
    if (text == nullptr) return IdentifyMode::Off;
    const std::string value(text);
    if (value == "off") return IdentifyMode::Off;
    if (value == "connect") return IdentifyMode::Connect;
    if (value == "always") return IdentifyMode::Always;
    throw std::runtime_error("ANYPS5_PAD_IDENTIFY must be off, connect or always, got '" + value + "'");
}

inline IdentifyMode IdentifyModeFromEnvironment() {
    static const IdentifyMode mode = ParseIdentifyMode(std::getenv("ANYPS5_PAD_IDENTIFY"));
    return mode;
}

constexpr unsigned kIdentifyMaxPlayer = 4;
constexpr std::uint32_t kIdentifyPulseMs = 150;
constexpr std::uint32_t kIdentifyPeriodMs = 400;
constexpr std::uint8_t kIdentifyStrength = 255;

struct IdentifyPulse {
    std::uint32_t startMs = 0;
    std::uint32_t durationMs = 0;
    std::uint8_t strength = 0;
};

struct IdentifyPattern {
    std::array<IdentifyPulse, kIdentifyMaxPlayer> pulses{};
    unsigned count = 0;
};

constexpr IdentifyPattern MakeIdentifyPattern(unsigned player) {
    IdentifyPattern pattern;
    if (player < 1 || player > kIdentifyMaxPlayer) return pattern;
    pattern.count = player;
    for (unsigned index = 0; index < player; ++index) {
        pattern.pulses[index] = IdentifyPulse{index * kIdentifyPeriodMs, kIdentifyPulseMs, kIdentifyStrength};
    }
    return pattern;
}

constexpr std::chrono::milliseconds kIdentifyQuiet{500};
constexpr std::chrono::seconds kIdentifyRepeat{30};

class IdentifyScheduler {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    void Bind(TimePoint now, unsigned player, IdentifyMode identifyMode) {
        pattern = MakeIdentifyPattern(player);
        mode = identifyMode;
        armed = pattern.count > 0 && mode != IdentifyMode::Off;
        quietSince = now;
        lastEnd = now;
        running = false;
        finished = false;
        nextPulse = 0;
    }

    template <class Sink>
    bool Tick(TimePoint now, bool titleActive, unsigned percent, Sink&& sink) {
        if (!armed) return false;
        if (titleActive) {
            quietSince = now;
            if (running) {
                running = false;
                finished = true;
                lastEnd = now;
            }
            return false;
        }
        if (percent == 0) return false;
        if (!running) {
            if (!Due(now)) return false;
            running = true;
            patternStart = now;
            nextPulse = 0;
        }
        const IdentifyPulse& pulse = pattern.pulses[nextPulse];
        if (now - patternStart < std::chrono::milliseconds(pulse.startMs)) return false;
        ++nextPulse;
        const bool sent = SendRumble(sink, pulse.strength, pulse.strength, percent, pulse.durationMs);
        if (nextPulse == pattern.count) {
            running = false;
            finished = true;
            lastEnd = now;
        }
        return sent;
    }

private:
    bool Due(TimePoint now) const {
        if (now - quietSince < kIdentifyQuiet) return false;
        if (!finished) return true;
        return mode == IdentifyMode::Always && now - lastEnd >= kIdentifyRepeat;
    }

    IdentifyPattern pattern{};
    IdentifyMode mode = IdentifyMode::Off;
    bool armed = false;
    bool running = false;
    bool finished = false;
    unsigned nextPulse = 0;
    TimePoint quietSince{};
    TimePoint patternStart{};
    TimePoint lastEnd{};
};

}

#endif
