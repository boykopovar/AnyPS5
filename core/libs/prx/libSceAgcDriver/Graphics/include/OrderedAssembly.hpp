#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_ORDEREDASSEMBLY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_ORDEREDASSEMBLY_HPP

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace AgcDriver::Graphics {

template<typename T>
class OrderedAssembly {
public:
    std::uint64_t Reserve() { return next.fetch_add(1, std::memory_order_relaxed); }

    void Submit(std::uint64_t sequence, T result) {
        std::lock_guard lock(mutex);
        ready.emplace(sequence, std::move(result));
        cv.notify_all();
    }

    T Take(std::uint64_t sequence) {
        std::unique_lock lock(mutex);
        cv.wait(lock, [&] { return ready.find(sequence) != ready.end(); });
        auto node = ready.extract(sequence);
        return std::move(node.mapped());
    }

    bool Ready(std::uint64_t sequence) const {
        std::lock_guard lock(mutex);
        return ready.find(sequence) != ready.end();
    }

private:
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::atomic<std::uint64_t> next{0};
    std::unordered_map<std::uint64_t, T> ready;
};

}

#endif