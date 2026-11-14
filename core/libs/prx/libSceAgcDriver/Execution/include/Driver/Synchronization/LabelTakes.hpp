#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SYNCHRONIZATION_LABELTAKES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SYNCHRONIZATION_LABELTAKES_HPP

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <utility>

namespace AgcDriver::DriverDetail {

class LabelTakes {
public:
    void Pass(std::uint32_t queue, std::uint64_t dword, std::uint64_t serial) {
        const auto key = queueDword(queue, dword);
        auto& entry = entries[key];
        if (entry.passed == 0) passCounts[slot(key)].fetch_add(1, std::memory_order_relaxed);
        entry.passed = serial;
        passes[queue].emplace_back(serial, key);
    }

    bool MayHavePass(std::uint32_t queue, std::uint64_t dword) const {
        return passCounts[slot(queueDword(queue, dword))].load(std::memory_order_relaxed) != 0;
    }

    void Take(std::uint32_t queue, std::uint64_t dword) {
        const auto key = queueDword(queue, dword);
        const auto found = entries.find(key);
        if (found == entries.end() || found->second.passed == 0) return;
        found->second.taken = found->second.passed;
        found->second.passed = 0;
        passCounts[slot(key)].fetch_sub(1, std::memory_order_relaxed);
    }

    std::uint64_t Taken(std::uint32_t queue, std::uint64_t dword) const {
        const auto found = entries.find(queueDword(queue, dword));
        return found == entries.end() ? 0 : found->second.taken;
    }

    void Retire(std::uint32_t queue, std::uint64_t received) {
        const auto queued = passes.find(queue);
        if (queued == passes.end()) return;
        auto& order = queued->second;
        while (!order.empty() && order.front().first <= received) {
            const auto [serial, key] = order.front();
            order.pop_front();
            const auto found = entries.find(key);
            if (found == entries.end()) continue;
            if (found->second.passed == serial) {
                found->second.passed = 0;
                passCounts[slot(key)].fetch_sub(1, std::memory_order_relaxed);
            }
            if (found->second.taken == serial) found->second.taken = 0;
            if (found->second.passed == 0 && found->second.taken == 0) entries.erase(found);
        }
        if (order.empty()) passes.erase(queued);
    }

    bool Empty() const {
        return entries.empty() && passes.empty();
    }

private:
    struct Entry {
        std::uint64_t passed = 0;
        std::uint64_t taken = 0;
    };

    static constexpr std::size_t PassSlotBits = 12;

    static std::uint64_t queueDword(std::uint32_t queue, std::uint64_t dword) {
        return dword | (static_cast<std::uint64_t>(queue) << 56u);
    }

    static std::size_t slot(std::uint64_t key) {
        return static_cast<std::size_t>(((key >> 2u) * 0x9e3779b97f4a7c15ull) >> (64u - PassSlotBits));
    }

    std::unordered_map<std::uint64_t, Entry> entries;
    std::unordered_map<std::uint32_t, std::deque<std::pair<std::uint64_t, std::uint64_t>>> passes;
    std::array<std::atomic<std::uint32_t>, std::size_t{1} << PassSlotBits> passCounts{};
};

}

#endif
