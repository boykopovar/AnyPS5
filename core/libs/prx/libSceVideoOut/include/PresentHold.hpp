#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_PRESENTHOLD_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_PRESENTHOLD_HPP

#include <atomic>
#include <cstdint>

class PresentHold {
public:
    bool Held() const { return held.load(std::memory_order_acquire); }
    std::uint64_t Epoch() const { return epoch.load(std::memory_order_acquire); }

    bool Set(bool value) {
        if (held.load(std::memory_order_relaxed) == value) return false;
        epoch.fetch_add(1, std::memory_order_acq_rel);
        held.store(value, std::memory_order_release);
        return true;
    }

    bool Overlapped(std::uint64_t epochBefore) const { return Held() || Epoch() != epochBefore; }

private:
    std::atomic<bool> held{false};
    std::atomic<std::uint64_t> epoch{0};
};

#endif
