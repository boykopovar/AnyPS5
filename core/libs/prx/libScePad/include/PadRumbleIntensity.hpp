#ifndef CORE_LIBS_PRX_LIBSCEPAD_PADRUMBLEINTENSITY_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_PADRUMBLEINTENSITY_HPP

#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace Pad {

constexpr unsigned kRumblePercentFull = 100;

constexpr std::uint16_t ScaleRumbleAmplitude(std::uint16_t amplitude, unsigned percent) {
    if (percent >= kRumblePercentFull) return amplitude;
    return static_cast<std::uint16_t>((static_cast<std::uint32_t>(amplitude) * percent + 50u) / 100u);
}

struct RumbleCommand {
    std::uint16_t low = 0;
    std::uint16_t high = 0;
    std::uint32_t durationMs = 0;

    constexpr bool Active() const { return low != 0 || high != 0; }
    constexpr bool operator==(const RumbleCommand& other) const {
        return low == other.low && high == other.high && durationMs == other.durationMs;
    }
};

constexpr RumbleCommand MakeRumbleCommand(std::uint8_t low, std::uint8_t high, unsigned percent, std::uint32_t durationMs) {
    if (percent >= kRumblePercentFull) {
        return RumbleCommand{static_cast<std::uint16_t>(low * 257), static_cast<std::uint16_t>(high * 257), (low != 0 || high != 0) ? durationMs : 0};
    }
    RumbleCommand command;
    command.low = ScaleRumbleAmplitude(static_cast<std::uint16_t>(low * 257), percent);
    command.high = ScaleRumbleAmplitude(static_cast<std::uint16_t>(high * 257), percent);
    command.durationMs = command.Active() ? durationMs : 0;
    return command;
}

template <class Sink>
bool SendRumble(Sink&& sink, std::uint8_t low, std::uint8_t high, unsigned percent, std::uint32_t durationMs) {
    const RumbleCommand command = MakeRumbleCommand(low, high, percent, durationMs);
    sink(command.low, command.high, command.durationMs);
    return command.Active();
}

inline unsigned ParseRumblePercent(const char* text) {
    if (text == nullptr) return kRumblePercentFull;
    const std::string value(text);
    bool valid = !value.empty() && value.size() <= 3;
    unsigned parsed = 0;
    for (const char digit : value) {
        if (digit < '0' || digit > '9') valid = false;
        else parsed = parsed * 10 + static_cast<unsigned>(digit - '0');
    }
    if (!valid || parsed > kRumblePercentFull) {
        throw std::runtime_error("ANYPS5_RUMBLE must be a whole number from 0 to 100, got '" + value + "'");
    }
    return parsed;
}

inline unsigned RumblePercent() {
    static const unsigned percent = ParseRumblePercent(std::getenv("ANYPS5_RUMBLE"));
    return percent;
}

}

#endif
