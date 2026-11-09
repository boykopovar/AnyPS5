#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_PORT0PRESENCE_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_PORT0PRESENCE_HPP

#include <chrono>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>

namespace Port0Presence {

enum class Mode {
    Default,
    Controller
};

constexpr std::chrono::milliseconds kHold{500};

inline Mode ParseMode(const char* text) {
    if (text == nullptr) return Mode::Default;
    const std::string value(text);
    if (value == "default") return Mode::Default;
    if (value == "controller") return Mode::Controller;
    throw std::runtime_error("ANYPS5_PAD_PORT0_STATE must be default or controller, not '" + value + "'");
}

inline Mode ConfiguredMode() {
    return ParseMode(std::getenv("ANYPS5_PAD_PORT0_STATE"));
}

class Presence {
public:
    using Clock = std::chrono::steady_clock;

    Presence(Mode mode, bool bound) : mode(mode), committed(mode == Mode::Default || bound) {}

    bool Connected() const { return committed; }

    std::optional<bool> Observe(bool bound, Clock::time_point now) {
        if (mode == Mode::Default || bound == committed) {
            waiting = false;
            return std::nullopt;
        }
        if (!waiting) {
            waiting = true;
            since = now;
            return std::nullopt;
        }
        if (now - since < kHold) return std::nullopt;
        committed = bound;
        waiting = false;
        return committed;
    }

private:
    Mode mode;
    bool committed;
    bool waiting = false;
    Clock::time_point since{};
};

}

#endif
