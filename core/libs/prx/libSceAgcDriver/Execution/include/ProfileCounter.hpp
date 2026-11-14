#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PROFILECOUNTER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PROFILECOUNTER_HPP

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "prx/libc/include/HostThreadSlot.hpp"

namespace AgcDriver {

class ProfileCounter {
public:
    static constexpr std::size_t Slots = 16;

    void Add(std::uint64_t value = 1) noexcept {
        slots[ThreadSlot()].value.fetch_add(value, std::memory_order_relaxed);
    }

    std::uint64_t Sum() const noexcept {
        std::uint64_t total = 0;
        for (const auto& slot : slots) total += slot.value.load(std::memory_order_relaxed);
        return total;
    }

    std::uint64_t Take() noexcept {
        std::uint64_t total = 0;
        for (auto& slot : slots) total += slot.value.exchange(0, std::memory_order_relaxed);
        return total;
    }

    static std::size_t ThreadSlot() noexcept {
        using Held = HostThreadSlot<std::uint32_t, ProfileCounter>;
        std::uint32_t held = Held::Get();
        if (held == 0) {
            held = static_cast<std::uint32_t>(Shared() ? 0 : NextSlot().fetch_add(1, std::memory_order_relaxed) % Slots) + 1;
            Held::Set(held);
        }
        return held - 1;
    }

    static bool Shared() noexcept {
        static const bool shared = std::getenv("APS5_SHARED_PROFILE_COUNTERS") != nullptr;
        return shared;
    }

private:
    struct alignas(64) Slot {
        std::atomic<std::uint64_t> value{0};
    };

    static std::atomic<std::size_t>& NextSlot() noexcept {
        static std::atomic<std::size_t> next{0};
        return next;
    }

    std::array<Slot, Slots> slots{};
};

}

#endif
