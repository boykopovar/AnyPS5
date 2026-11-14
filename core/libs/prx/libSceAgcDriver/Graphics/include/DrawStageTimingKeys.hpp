#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DRAWSTAGETIMINGKEYS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DRAWSTAGETIMINGKEYS_HPP

#include <algorithm>
#include <cstdint>
#include <map>
#include <mutex>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

class DrawStageTimingKeys {
public:
    using Stage = std::pair<std::uint32_t, std::uint64_t>;
    static constexpr std::uint64_t MaxKey = (std::uint64_t{1} << 62u) - 1;

    explicit DrawStageTimingKeys(std::uint64_t capacity = MaxKey) : capacity(capacity) {
        if (capacity > MaxKey) throw std::invalid_argument("draw-stage timing capacity exceeds the key space");
    }

    std::pair<std::uint64_t, bool> Intern(std::span<const Stage> stages) {
        std::vector<Stage> key(stages.begin(), stages.end());
        std::sort(key.begin(), key.end());
        std::lock_guard lock(mutex);
        if (auto found = keys.find(key); found != keys.end()) return {found->second, false};
        if (nextKey > capacity) throw std::overflow_error("draw-stage timing key space exhausted");
        const auto value = nextKey;
        keys.emplace(std::move(key), value);
        ++nextKey;
        return {value, true};
    }

private:
    std::mutex mutex;
    std::map<std::vector<Stage>, std::uint64_t> keys;
    std::uint64_t nextKey = 1;
    std::uint64_t capacity;
};

}

#endif
