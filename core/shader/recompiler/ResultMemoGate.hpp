#ifndef CORE_SHADER_RECOMPILER_RESULTMEMOGATE_HPP
#define CORE_SHADER_RECOMPILER_RESULTMEMOGATE_HPP

#include <atomic>
#include <cstdint>

namespace ShaderRecompiler {

class ResultMemoGate {
public:
    static constexpr std::uint32_t Window = 256;
    static constexpr std::uint32_t BypassCalls = 4096;

    [[nodiscard]] bool Use() {
        auto remaining = bypass.load(std::memory_order_relaxed);
        while (remaining != 0 && !bypass.compare_exchange_weak(remaining, remaining - 1, std::memory_order_relaxed)) {}
        return remaining == 0;
    }

    [[nodiscard]] bool Bypassing() const {
        return bypass.load(std::memory_order_relaxed) != 0;
    }

    void Record(bool hit) {
        if (Bypassing()) return;
        if (hit) hits.fetch_add(1, std::memory_order_relaxed);
        if (lookups.fetch_add(1, std::memory_order_relaxed) + 1 < Window) return;
        auto counted = lookups.load(std::memory_order_relaxed);
        while (counted >= Window) {
            if (lookups.compare_exchange_weak(counted, 0, std::memory_order_relaxed)) {
                const auto served = hits.exchange(0, std::memory_order_relaxed);
                if (served * 4u < Window) bypass.store(BypassCalls, std::memory_order_relaxed);
                return;
            }
        }
    }

private:
    std::atomic<std::uint32_t> bypass{0};
    std::atomic<std::uint32_t> lookups{0};
    std::atomic<std::uint32_t> hits{0};
};

}

#endif
