#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace audioin::detail {

class CaptureMuteGenerations {
public:
    bool Observe(std::uint32_t device, std::uint64_t generation) {
        std::lock_guard lock(mutex);
        const auto [position, inserted] = generations.try_emplace(device, generation);
        if (inserted || position->second == generation) return false;
        position->second = generation;
        return true;
    }

    void Forget(std::uint32_t device) {
        std::lock_guard lock(mutex);
        generations.erase(device);
    }

private:
    std::mutex mutex;
    std::unordered_map<std::uint32_t, std::uint64_t> generations;
};

inline std::size_t filterCapturedSamples(std::uint8_t* destination, std::size_t bytes, std::size_t captured, bool muted) {
    const std::size_t available = std::min(captured, bytes);
    if (muted) {
        std::fill_n(destination, bytes, std::uint8_t{});
        return 0;
    }
    std::fill_n(destination + available, bytes - available, std::uint8_t{});
    return available;
}

template <typename TDequeue>
void discardQueuedAudio(std::size_t excess, std::uint8_t* discard, std::size_t discardSize, TDequeue&& dequeue) {
    while (excess > 0) {
        const auto chunk = static_cast<std::uint32_t>(std::min(excess, discardSize));
        const auto dequeued = dequeue(discard, chunk);
        if (dequeued == 0) break;
        excess -= std::min(excess, static_cast<std::size_t>(dequeued));
    }
}

}
