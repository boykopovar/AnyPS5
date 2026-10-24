#ifndef CORE_LIBS_PRX_LIBSCEPAD_PADAUDIOHAPTICQUEUE_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_PADAUDIOHAPTICQUEUE_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <stdexcept>

class PadAudioHapticQueue {
    struct Block {
        std::uint64_t begin;
        std::uint64_t end;
        std::array<std::uint8_t, 2> rumble;
    };
    std::map<int, std::deque<Block>> sources;

public:
    void Queue(int source, std::uint64_t begin, std::uint64_t end, std::uint8_t large, std::uint8_t small) {
        if (source <= 0 || begin >= end) throw std::invalid_argument("Pad: invalid legacy haptic interval");
        auto& blocks = sources[source];
        if (!blocks.empty() && begin < blocks.back().end) throw std::invalid_argument("Pad: overlapping legacy haptic intervals");
        blocks.push_back({begin, end, {large, small}});
    }

    void Clear(int source) { sources.erase(source); }

    std::array<std::uint8_t, 2> Read(std::uint64_t now) {
        std::array<std::uint8_t, 2> result{};
        for (auto source = sources.begin(); source != sources.end();) {
            auto& blocks = source->second;
            while (!blocks.empty() && blocks.front().end <= now) blocks.pop_front();
            if (blocks.empty()) {
                source = sources.erase(source);
                continue;
            }
            if (blocks.front().begin <= now) {
                for (std::size_t channel = 0; channel < result.size(); ++channel) result[channel] = std::max(result[channel], blocks.front().rumble[channel]);
            }
            ++source;
        }
        return result;
    }
};

#endif
