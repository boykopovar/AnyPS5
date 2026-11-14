#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_RECYCLEDSHARED_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_RECYCLEDSHARED_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace AgcDriver::DriverDetail {

template <typename TObject>
class RecycledShared {
public:
    RecycledShared(std::size_t capacity, std::size_t probes) : capacity(capacity), probes(probes) {}

    template <typename TReset>
    std::shared_ptr<TObject> Take(TReset&& reset) {
        for (std::size_t probe = 0; probe < probes && probe < pool.size(); ++probe) {
            auto& candidate = pool[cursor];
            advance();
            if (candidate.use_count() != 1) continue;
            std::atomic_thread_fence(std::memory_order_acquire);
            reset(*candidate);
            ++reused;
            return candidate;
        }
        auto made = std::make_shared<TObject>();
        ++fresh;
        if (pool.size() < capacity) {
            pool.push_back(made);
        } else {
            pool[cursor] = made;
            advance();
        }
        return made;
    }

    std::uint64_t Reused() const {
        return reused;
    }

    std::uint64_t Fresh() const {
        return fresh;
    }

private:
    void advance() {
        cursor = cursor + 1 >= pool.size() ? 0 : cursor + 1;
    }

    std::vector<std::shared_ptr<TObject>> pool;
    std::size_t cursor = 0;
    std::size_t capacity;
    std::size_t probes;
    std::uint64_t reused = 0;
    std::uint64_t fresh = 0;
};

}

#endif
