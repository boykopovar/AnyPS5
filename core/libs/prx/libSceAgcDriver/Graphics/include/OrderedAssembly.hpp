#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_ORDEREDASSEMBLY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_ORDEREDASSEMBLY_HPP

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace AgcDriver::Graphics {

template<typename T>
class OrderedAssembly {
    static_assert(std::is_nothrow_move_constructible_v<T>);

public:
    std::uint64_t Reserve() {
        const auto sequence = next.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard lock(mutex);
        ready.emplace(sequence, std::nullopt);
        return sequence;
    }

    void Submit(std::uint64_t sequence, T result) noexcept {
        std::lock_guard lock(mutex);
        ready.find(sequence)->second.emplace(std::move(result));
        cv.notify_all();
    }

    T Take(std::uint64_t sequence) {
        std::unique_lock lock(mutex);
        cv.wait(lock, [&] { return ready.find(sequence)->second.has_value(); });
        auto node = ready.extract(sequence);
        return std::move(*node.mapped());
    }

    bool Ready(std::uint64_t sequence) const {
        std::lock_guard lock(mutex);
        const auto found = ready.find(sequence);
        return found != ready.end() && found->second.has_value();
    }

private:
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::atomic<std::uint64_t> next{0};
    std::unordered_map<std::uint64_t, std::optional<T>> ready;
};

}

#endif