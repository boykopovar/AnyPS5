#include "prx/libScePad/include/PadRumbleIntensity.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "pad_rumble_intensity: %s\n", message);
    std::exit(1);
}

void SetEnvironment(const char* value) {
#ifdef _WIN32
    _putenv_s("ANYPS5_RUMBLE", value == nullptr ? "" : value);
#else
    if (value == nullptr) unsetenv("ANYPS5_RUMBLE");
    else setenv("ANYPS5_RUMBLE", value, 1);
#endif
}

unsigned ReferenceScale(unsigned amplitude, unsigned percent) {
    return (2u * amplitude * percent + 100u) / 200u;
}

void ScaleKnownValues() {
    Require(Pad::ScaleRumbleAmplitude(0, 0) == 0 && Pad::ScaleRumbleAmplitude(0, 50) == 0 && Pad::ScaleRumbleAmplitude(0, 100) == 0, "a zero amplitude stays zero");
    Require(Pad::ScaleRumbleAmplitude(65535, 100) == 65535, "100 percent keeps the full amplitude");
    Require(Pad::ScaleRumbleAmplitude(65535, 0) == 0, "0 percent silences the full amplitude");
    Require(Pad::ScaleRumbleAmplitude(65535, 1) == 655, "1 percent of 65535 is 655");
    Require(Pad::ScaleRumbleAmplitude(65535, 50) == 32768, "50 percent of 65535 rounds .5 up to 32768");
    Require(Pad::ScaleRumbleAmplitude(65535, 99) == 64880, "99 percent of 65535 is 64880");
    Require(Pad::ScaleRumbleAmplitude(1, 50) == 1, "half of 1 rounds up to 1");
    Require(Pad::ScaleRumbleAmplitude(1, 49) == 0, "49 percent of 1 rounds down to 0");
    Require(Pad::ScaleRumbleAmplitude(257, 50) == 129, "50 percent of 257 rounds .5 up to 129");
    Require(Pad::ScaleRumbleAmplitude(3, 50) == 2, "half of 3 rounds up to 2");
    Require(Pad::ScaleRumbleAmplitude(2, 25) == 1, "a quarter of 2 rounds .5 up to 1");
    Require(Pad::ScaleRumbleAmplitude(1000, 101) == 1000 && Pad::ScaleRumbleAmplitude(65535, 4000000000u) == 65535, "a percentage above 100 never amplifies");
}

void ScaleOverFullRange() {
    for (unsigned percent = 0; percent <= 100; ++percent) {
        unsigned previous = 0;
        for (unsigned amplitude = 0; amplitude <= 65535; ++amplitude) {
            const auto scaled = Pad::ScaleRumbleAmplitude(static_cast<std::uint16_t>(amplitude), percent);
            Require(scaled == ReferenceScale(amplitude, percent), "the scale matches the independent exact reference");
            Require(scaled <= amplitude, "scaling never amplifies");
            Require(scaled >= previous, "scaling is monotonic in the amplitude");
            previous = scaled;
        }
    }
    for (unsigned amplitude = 0; amplitude <= 65535; amplitude += 97) {
        unsigned previous = 0;
        for (unsigned percent = 0; percent <= 100; ++percent) {
            const auto scaled = Pad::ScaleRumbleAmplitude(static_cast<std::uint16_t>(amplitude), percent);
            Require(scaled >= previous, "scaling is monotonic in the percentage");
            previous = scaled;
        }
        Require(Pad::ScaleRumbleAmplitude(static_cast<std::uint16_t>(amplitude), 100) == amplitude, "100 percent is the identity");
        Require(Pad::ScaleRumbleAmplitude(static_cast<std::uint16_t>(amplitude), 0) == 0, "0 percent is silence");
    }
}

void FullIntensityIsTheLegacyCommandByteForByte() {
    constexpr std::uint32_t duration = 2000;
    for (unsigned low = 0; low <= 255; ++low) {
        for (unsigned high = 0; high <= 255; ++high) {
            Pad::RumbleCommand legacy;
            legacy.low = static_cast<std::uint16_t>(low * 257);
            legacy.high = static_cast<std::uint16_t>(high * 257);
            legacy.durationMs = (low != 0 || high != 0) ? duration : 0;
            for (const unsigned percent : {100u, 101u, 255u}) {
                const auto command = Pad::MakeRumbleCommand(static_cast<std::uint8_t>(low), static_cast<std::uint8_t>(high), percent, duration);
                Require(sizeof(command) == sizeof(legacy) && std::memcmp(&command, &legacy, sizeof(command)) == 0, "full intensity equals the unscaled command byte for byte");
                Require(command.Active() == (low != 0 || high != 0), "full intensity is active exactly when a motor is on");
            }
        }
    }
}

void OffNeverReachesTheHost() {
    std::vector<Pad::RumbleCommand> calls;
    const auto sink = [&calls](std::uint16_t low, std::uint16_t high, std::uint32_t duration) { calls.push_back({low, high, duration}); };
    for (unsigned low = 0; low <= 255; ++low) {
        for (unsigned high = 0; high <= 255; ++high) {
            calls.clear();
            const bool active = Pad::SendRumble(sink, static_cast<std::uint8_t>(low), static_cast<std::uint8_t>(high), 0, 2000);
            Require(calls.size() == 1, "an applied output reaches the sink exactly once, so a running rumble is stopped");
            Require(calls[0].low == 0 && calls[0].high == 0 && calls[0].durationMs == 0, "0 percent only ever asks the host to stop");
            Require(!active, "0 percent is never reported as rumbling");
        }
    }
}

void StopOnApplyAfterFullRumble() {
    std::vector<Pad::RumbleCommand> calls;
    const auto sink = [&calls](std::uint16_t low, std::uint16_t high, std::uint32_t duration) { calls.push_back({low, high, duration}); };
    Pad::SendRumble(sink, 255, 255, 100, 2000);
    Require(calls.size() == 1 && calls[0].low == 65535 && calls[0].high == 65535 && calls[0].durationMs == 2000, "before the setting applies, the host rumbles at full strength");
    Pad::SendRumble(sink, 255, 255, 0, 2000);
    Require(calls.size() == 2 && calls[1].low == 0 && calls[1].high == 0 && calls[1].durationMs == 0, "applying 0 percent stops the running rumble");
    Pad::SendRumble(sink, 0, 0, 0, 2000);
    Require(calls.size() == 3 && !calls[2].Active() && calls[2].durationMs == 0, "a title stopping its own vibration sends the same stop");
}

void IntermediateIntensityScalesBothMotors() {
    const auto half = Pad::MakeRumbleCommand(255, 128, 50, 2000);
    Require(half.low == 32768 && half.high == 16448 && half.durationMs == 2000, "50 percent halves each motor with .5 rounded up");
    const auto left = Pad::MakeRumbleCommand(200, 0, 25, 2000);
    Require(left.low == 12850 && left.high == 0 && left.durationMs == 2000, "a motor that is off stays off");
    const auto idle = Pad::MakeRumbleCommand(0, 0, 50, 2000);
    Require(idle.low == 0 && idle.high == 0 && idle.durationMs == 0, "no motor on means a stop with no duration");
    for (unsigned percent = 1; percent <= 99; ++percent) {
        for (unsigned level = 1; level <= 255; ++level) {
            const auto command = Pad::MakeRumbleCommand(static_cast<std::uint8_t>(level), 0, percent, 2000);
            Require(command.Active() && command.durationMs == 2000, "any nonzero request stays audible at any nonzero percentage");
            Require(command.low <= level * 257u && command.high == 0, "an intermediate percentage never amplifies");
        }
    }
}

bool Refuses(const char* text, const char* expectedValue) {
    try {
        Pad::ParseRumblePercent(text);
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        return message.find("ANYPS5_RUMBLE") != std::string::npos && message.find(expectedValue) != std::string::npos;
    }
    return false;
}

void ParserAcceptsAndRefuses() {
    Require(Pad::ParseRumblePercent(nullptr) == 100, "an unset variable keeps full intensity");
    const struct { const char* text; unsigned expected; } accepted[] = {{"0", 0}, {"1", 1}, {"50", 50}, {"99", 99}, {"100", 100}, {"007", 7}, {"000", 0}};
    for (const auto& entry : accepted) Require(Pad::ParseRumblePercent(entry.text) == entry.expected, "a whole number from 0 to 100 is accepted");
    const char* refused[] = {"", "101", "999", "1000", "-1", "+5", " 5", "5 ", "1.5", "abc", "0x10", "50%", "5a", "1e1", "\n"};
    for (const char* text : refused) Require(Refuses(text, text), "an invalid value is refused with an error naming the variable and the value");
}

void EnvironmentIsReadOnceAndInvalidValuesKeepFailing() {
    SetEnvironment("abc");
    bool first = false;
    bool second = false;
    try { Pad::RumblePercent(); } catch (const std::runtime_error&) { first = true; }
    try { Pad::RumblePercent(); } catch (const std::runtime_error&) { second = true; }
    Require(first && second, "an invalid value fails at every use, not only the first");
    SetEnvironment("30");
    Require(Pad::RumblePercent() == 30, "a valid value is read");
    SetEnvironment("80");
    Require(Pad::RumblePercent() == 30, "the value is read once for the lifetime of the process");
}

}

int main() {
    ScaleKnownValues();
    ScaleOverFullRange();
    FullIntensityIsTheLegacyCommandByteForByte();
    OffNeverReachesTheHost();
    StopOnApplyAfterFullRumble();
    IntermediateIntensityScalesBothMotors();
    ParserAcceptsAndRefuses();
    EnvironmentIsReadOnceAndInvalidValuesKeepFailing();
    return 0;
}
