#include "prx/libSceAgcDriver/Graphics/include/ParallelCompare.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>

namespace AgcDriver::Graphics {
namespace {

class ComparePool {
public:
    explicit ComparePool(unsigned count) {
        for (unsigned i = 0; i < count; ++i) std::thread([this] { work(); }).detach();
    }

    bool Run(std::span<const CompareSpan> spans, std::span<std::uint8_t> equal) {
        std::unique_lock caller(callers, std::try_to_lock);
        if (!caller.owns_lock()) return false;
        {
            std::unique_lock lock(mutex);
            finished.wait(lock, [&] { return working == 0; });
            jobSpans = spans;
            jobEqual = equal;
            next.store(0, std::memory_order_relaxed);
            done.store(0, std::memory_order_relaxed);
            ++generation;
        }
        wake.notify_all();
        drain(spans, equal);
        std::unique_lock lock(mutex);
        finished.wait(lock, [&] { return working == 0 && done.load(std::memory_order_acquire) == spans.size(); });
        jobSpans = {};
        jobEqual = {};
        return true;
    }

private:
    void drain(std::span<const CompareSpan> spans, std::span<std::uint8_t> equal) {
        std::size_t compared = 0;
        for (auto index = next.fetch_add(1, std::memory_order_relaxed); index < spans.size(); index = next.fetch_add(1, std::memory_order_relaxed)) {
            const auto& span = spans[index];
            equal[index] = std::memcmp(span.first, span.second, span.bytes) == 0 ? 1 : 0;
            ++compared;
        }
        if (compared != 0) done.fetch_add(compared, std::memory_order_acq_rel);
    }

    void work() {
        std::uint64_t seen = 0;
        std::unique_lock lock(mutex);
        for (;;) {
            wake.wait(lock, [&] { return generation != seen; });
            seen = generation;
            const auto spans = jobSpans;
            const auto equal = jobEqual;
            ++working;
            lock.unlock();
            drain(spans, equal);
            lock.lock();
            --working;
            if (working == 0) finished.notify_all();
        }
    }

    std::mutex callers;
    std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable finished;
    std::uint64_t generation = 0;
    unsigned working = 0;
    std::span<const CompareSpan> jobSpans;
    std::span<std::uint8_t> jobEqual;
    std::atomic<std::size_t> next{0};
    std::atomic<std::size_t> done{0};
};

ComparePool* sharedPool() {
    static ComparePool* const pool = new ComparePool(CompareHelpers());
    return pool;
}

}

unsigned CompareHelpers() {
    static const unsigned helpers = std::clamp(std::thread::hardware_concurrency() / 8u, 1u, 4u);
    return helpers;
}

void CompareSpansFrom(std::span<const CompareSpan> spans, std::span<std::uint8_t> equal, std::size_t parallelBytes) {
    std::size_t total = 0;
    for (const auto& span : spans) total += span.bytes;
    if (spans.size() > 1 && total >= parallelBytes) {
        if (sharedPool()->Run(spans, equal)) return;
    }
    for (std::size_t index = 0; index < spans.size(); ++index) equal[index] = std::memcmp(spans[index].first, spans[index].second, spans[index].bytes) == 0 ? 1 : 0;
}

void CompareSpans(std::span<const CompareSpan> spans, std::span<std::uint8_t> equal) {
    CompareSpansFrom(spans, equal, std::size_t{1} << 20u);
}

}
