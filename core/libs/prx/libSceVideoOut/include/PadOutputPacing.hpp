#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_PADOUTPUTPACING_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_PADOUTPUTPACING_HPP

#include <array>
#include <chrono>
#include <cstdint>

class LightBarPacer {
public:
    using Clock = std::chrono::steady_clock;
    using Color = std::array<std::uint8_t, 3>;
    static constexpr std::chrono::milliseconds Interval{30};

    bool Pending(const Color& color) const {
        return !sent || color != last;
    }

    bool Due(const Color& color, Clock::time_point now) const {
        return Pending(color) && now >= next;
    }

    void Result(const Color& color, Clock::time_point now, bool succeeded) {
        next = now + Interval;
        if (!succeeded) return;
        last = color;
        sent = true;
    }

    void Reset() {
        sent = false;
        next = {};
    }

private:
    Color last{};
    bool sent = false;
    Clock::time_point next{};
};

class TriggerEffectCache {
public:
    using Effect = std::array<std::uint8_t, 47>;

    bool NeedsSend(const Effect& effect) const {
        return !sent || effect != last;
    }

    void Result(const Effect& effect, bool succeeded) {
        if (!succeeded) return;
        last = effect;
        sent = true;
    }

    void Reset() {
        sent = false;
    }

private:
    Effect last{};
    bool sent = false;
};

#endif
