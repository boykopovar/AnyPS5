#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace audioin::detail {

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
