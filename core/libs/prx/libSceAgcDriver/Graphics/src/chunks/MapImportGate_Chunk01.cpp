#include "prx/libSceAgcDriver/Graphics/include/MapImportGate.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <shared_mutex>
#include <string>
#include <thread>

namespace AgcDriver::Graphics::MapImportGate {

namespace {

struct alignas(64) Stripe {
    std::shared_mutex mutex;
};

std::array<Stripe, StripeCount>& StripeLocks() {
    static std::array<Stripe, StripeCount> stripes;
    return stripes;
}

alignas(64) std::atomic<std::uint32_t> fastPublishers{0};
alignas(64) std::atomic<std::uint32_t> activeImports{0};
alignas(64) std::atomic<std::uint32_t> waitingPublishers{0};
std::atomic<std::uint32_t> drainingImports{0};
std::atomic<std::uint64_t> fastPublications{0};
std::atomic<std::uint64_t> stripedPublications{0};
std::atomic<std::uint64_t> waitedPublications{0};
std::atomic<std::uint64_t> importHolds{0};
std::atomic<std::uint64_t> drainWaits{0};
std::atomic<std::uint64_t> retries{0};
std::atomic<void (*)()> unlockedHookForTest{nullptr};

struct PublisherState {
    std::uint32_t depth = 0;
    bool fast = false;
    Stripes held;
};

thread_local PublisherState publisher;
thread_local std::uint32_t leafLocksHeld = 0;

std::uint32_t Lowest(const Stripes& stripes) {
    for (std::uint32_t index = 0; index < StripeCount; ++index) {
        if (stripes[index]) return index;
    }
    return StripeCount;
}

std::uint32_t Highest(const Stripes& stripes) {
    for (std::uint32_t index = StripeCount; index-- > 0;) {
        if (stripes[index]) return index;
    }
    return 0;
}

void LockShared(const Stripes& stripes) {
    auto& locks = StripeLocks();
    for (std::uint32_t index = 0; index < StripeCount; ++index) {
        if (!stripes[index]) continue;
        if (locks[index].mutex.try_lock_shared()) continue;
        waitingPublishers.fetch_add(1, std::memory_order_acq_rel);
        locks[index].mutex.lock_shared();
        waitingPublishers.fetch_sub(1, std::memory_order_acq_rel);
        waitedPublications.fetch_add(1, std::memory_order_relaxed);
    }
}

void UnlockShared(const Stripes& stripes) {
    auto& locks = StripeLocks();
    for (std::uint32_t index = StripeCount; index-- > 0;) {
        if (stripes[index]) locks[index].mutex.unlock_shared();
    }
}

void Backoff(std::uint32_t round) {
    if (round < 16) {
        std::this_thread::yield();
        return;
    }
    const auto shift = std::min<std::uint32_t>(round - 16, 6);
    std::this_thread::sleep_for(std::chrono::microseconds(16u << shift));
}

}

Stripes StripesOf(std::uint64_t begin, std::uint64_t end) {
    Stripes stripes;
    if (end <= begin) return stripes;
    const auto first = begin >> StripeShift;
    const auto last = (end - 1) >> StripeShift;
    if (last - first + 1 >= StripeCount) return stripes.set();
    for (auto index = first; index <= last; ++index) stripes.set(static_cast<std::size_t>(index % StripeCount));
    return stripes;
}

bool LockOrderChecked() {
#ifdef NDEBUG
    static const bool checked = std::getenv("APS5_ASSERT_GPU_LOCK") != nullptr;
    return checked;
#else
    return true;
#endif
}

void NoteLeafLock() noexcept {
    if (LockOrderChecked()) ++leafLocksHeld;
}

void NoteLeafUnlock() noexcept {
    if (LockOrderChecked()) --leafLocksHeld;
}

void RequireNoLeafLockHeld(const char* where) {
    if (!LockOrderChecked()) return;
    if (leafLocksHeld == 0 && !ImportsHeldByThisThread()) return;
    std::fprintf(stderr, "[lock] ASSERT: %s waits for the map-time import gate while this thread holds %s\n", where, leafLocksHeld != 0 ? "the live or pending storage image mutex" : "the host imports mutex");
    std::abort();
}

PublishHold::PublishHold(std::uint64_t address, std::uint64_t bytes) {
    if (bytes == 0) return;
    const auto wanted = StripesOf(address, address + bytes);
    if (publisher.depth != 0) {
        if (!publisher.fast) {
            const auto missing = wanted & ~publisher.held;
            if (missing.any()) {
                Require(Lowest(missing) > Highest(publisher.held), "a nested storage image publication needs a map-time import gate stripe below one its thread already holds");
                RequireNoLeafLockHeld("a storage image publication");
                LockShared(missing);
                publisher.held |= missing;
                added = missing;
            }
        }
        ++publisher.depth;
        return;
    }
    RequireNoLeafLockHeld("a storage image publication");
    publisher.depth = 1;
    fastPublishers.fetch_add(1, std::memory_order_seq_cst);
    if (activeImports.load(std::memory_order_seq_cst) == 0) {
        publisher.fast = true;
        mode = Mode::Fast;
        fastPublications.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    fastPublishers.fetch_sub(1, std::memory_order_release);
    try {
        LockShared(wanted);
    } catch (...) {
        publisher.depth = 0;
        throw;
    }
    publisher.held = wanted;
    added = wanted;
    mode = Mode::Striped;
    stripedPublications.fetch_add(1, std::memory_order_relaxed);
}

PublishHold::~PublishHold() {
    switch (mode) {
    case Mode::Fast:
        publisher.fast = false;
        publisher.depth = 0;
        fastPublishers.fetch_sub(1, std::memory_order_release);
        return;
    case Mode::Striped:
        UnlockShared(added);
        publisher.held.reset();
        publisher.depth = 0;
        return;
    case Mode::Nested:
        if (publisher.depth == 0) return;
        --publisher.depth;
        if (added.none()) return;
        UnlockShared(added);
        publisher.held &= ~added;
        return;
    }
}

ImportHold::ImportHold(const Stripes& stripes) : held(stripes) {
    Require(publisher.depth == 0, "a map-time import without the GPU lock runs inside a storage image publication of its own thread");
    activeImports.fetch_add(1, std::memory_order_seq_cst);
    if (fastPublishers.load(std::memory_order_seq_cst) != 0) {
        drainWaits.fetch_add(1, std::memory_order_relaxed);
        drainingImports.fetch_add(1, std::memory_order_acq_rel);
        for (std::uint32_t round = 0; fastPublishers.load(std::memory_order_seq_cst) != 0; ++round) Backoff(round);
        drainingImports.fetch_sub(1, std::memory_order_acq_rel);
    }
    auto& locks = StripeLocks();
    for (std::uint32_t index = 0; index < StripeCount; ++index) {
        if (held[index]) locks[index].mutex.lock();
    }
    importHolds.fetch_add(1, std::memory_order_relaxed);
}

ImportHold::~ImportHold() {
    auto& locks = StripeLocks();
    for (std::uint32_t index = StripeCount; index-- > 0;) {
        if (held[index]) locks[index].mutex.unlock();
    }
    activeImports.fetch_sub(1, std::memory_order_release);
}

bool ImportHold::Covers(std::uint64_t begin, std::uint64_t end) const {
    const auto wanted = StripesOf(begin, end);
    return (wanted & ~held).none();
}

Counters CountersForTest() {
    Counters counters;
    counters.fastPublications = fastPublications.load(std::memory_order_relaxed);
    counters.stripedPublications = stripedPublications.load(std::memory_order_relaxed);
    counters.waitedPublications = waitedPublications.load(std::memory_order_relaxed);
    counters.importHolds = importHolds.load(std::memory_order_relaxed);
    counters.drainWaits = drainWaits.load(std::memory_order_relaxed);
    counters.retries = retries.load(std::memory_order_relaxed);
    return counters;
}

std::uint32_t WaitingPublishersForTest() {
    return waitingPublishers.load(std::memory_order_acquire);
}

std::uint32_t DrainingImportsForTest() {
    return drainingImports.load(std::memory_order_acquire);
}

void CountRetry() noexcept {
    retries.fetch_add(1, std::memory_order_relaxed);
}

void SetUnlockedHookForTest(void (*hook)()) {
    unlockedHookForTest.store(hook, std::memory_order_release);
}

void RunUnlockedHookForTest() {
    if (const auto hook = unlockedHookForTest.load(std::memory_order_acquire)) hook();
}

}
