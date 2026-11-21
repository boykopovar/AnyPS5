#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/DirectMemory/MemoryPool.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <map>
#include <mutex>
#include <new>
#include <random>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

bool DirectMemoryCheckedFree(int64_t start, size_t len);
bool DirectMemoryClaimRelease(int64_t start, size_t len, bool requireAllocated);
void DirectMemoryAbandonRelease(int64_t start, size_t len) noexcept;
void DirectMemoryRetype(int64_t start, size_t len, int memoryType);
size_t DirectMemoryFreeRun(uint64_t offset, uint64_t limit);

namespace {

constexpr uint64_t kPage = static_cast<uint64_t>(PS5_PAGE_SIZE);
constexpr uint64_t kPool = static_cast<uint64_t>(DIRECT_MEMORY_SIZE);

enum class Fault { None, BackingBadAlloc, BackingRuntimeError, ForgetRuntimeError };
Fault g_fault = Fault::None;
long g_newBudget = -1;
std::map<uint64_t, uint64_t> g_releaseClaims;
std::mutex g_mapHoldLock;
std::vector<std::pair<uint64_t, uint64_t>> g_mapHolds;

int g_failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("  FAILED line %d: %s\n", __LINE__, #cond);              \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

struct NewBudget {
    explicit NewBudget(long n) { g_newBudget = n; }
    ~NewBudget() { g_newBudget = -1; }
};

bool BlockAt(uint64_t offset, int64_t& s, int64_t& e, int& t) {
    return DirectMemoryFind(static_cast<int64_t>(offset), false, &s, &e, &t);
}

bool BlockIs(uint64_t offset, uint64_t start, uint64_t end, int type) {
    int64_t s = 0, e = 0;
    int t = 0;
    return BlockAt(offset, s, e, t) && s == static_cast<int64_t>(start) && e == static_cast<int64_t>(end) && t == type;
}

bool PoolEmpty() {
    int64_t s, e;
    int t;
    return !DirectMemoryFind(0, true, &s, &e, &t) && DirectMemoryFreeRun(0, kPool) == kPool;
}

void alloc_unplaceable_requests() {
    int64_t out = -1;
    CHECK(DirectMemoryAlloc(0, kPool, 0, 0, 1, &out) == SCE_KERNEL_ERROR_EINVAL);
    CHECK(DirectMemoryAlloc(-1, kPool, kPage, 0, 1, &out) == SCE_KERNEL_ERROR_EAGAIN);
    CHECK(DirectMemoryAlloc(0, kPool, SIZE_MAX, 0, 1, &out) == SCE_KERNEL_ERROR_EAGAIN);
    CHECK(DirectMemoryAlloc(0, kPool, kPool + kPage, 0, 1, &out) == SCE_KERNEL_ERROR_EAGAIN);
    CHECK(DirectMemoryAlloc(kPage, kPage, kPage, 0, 1, &out) == SCE_KERNEL_ERROR_EAGAIN);
    CHECK(DirectMemoryAlloc(2 * kPage, kPage, kPage, 0, 1, &out) == SCE_KERNEL_ERROR_EAGAIN);
    CHECK(DirectMemoryAlloc(kPool, kPool + kPage, kPage, 0, 1, &out) == SCE_KERNEL_ERROR_EAGAIN);
    CHECK(out == -1);
    CHECK(PoolEmpty());
}

void alloc_null_out() {
    CHECK(DirectMemoryAlloc(0, kPool, kPage, 0, 1, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    CHECK(PoolEmpty());
}

void alloc_rounding_alignment_and_clamping() {
    int64_t a = -1, b = -1;
    CHECK(DirectMemoryAlloc(0, kPool, 1, 1, 1, &a) == 0 && a == 0);
    CHECK(DirectMemoryAlloc(0, kPool, kPage, 0, 1, &b) == 0 && b == static_cast<int64_t>(kPage));
    DirectMemoryFree(0, kPool);

    CHECK(DirectMemoryAlloc(0, -1, kPage, 0, 1, &a) == 0 && a == 0);
    DirectMemoryFree(0, kPage);
    CHECK(DirectMemoryAlloc(0, INT64_MAX, kPage, 0, 1, &a) == 0 && a == 0);
    DirectMemoryFree(0, kPage);

    CHECK(DirectMemoryAlloc(1, kPool, kPage, 0, 1, &a) == 0 && a == static_cast<int64_t>(kPage));
    DirectMemoryFree(0, kPool);

    CHECK(DirectMemoryAlloc(0, kPool, kPage, static_cast<size_t>(1) << 62, 1, &a) == 0 && a == 0);
    CHECK(DirectMemoryAlloc(0, kPool, kPage, static_cast<size_t>(1) << 62, 1, &b) == SCE_KERNEL_ERROR_EAGAIN);
    DirectMemoryFree(0, kPool);
    CHECK(PoolEmpty());
}

void alloc_non_power_of_two_alignment() {
    int64_t a = -1, b = -1, c = -1;
    CHECK(DirectMemoryAlloc(kPage, kPool, kPage, 3 * kPage, 1, &a) == 0 && a == static_cast<int64_t>(3 * kPage));
    CHECK(DirectMemoryAlloc(kPage, kPool, kPage, 3 * kPage, 1, &b) == 0 && b == static_cast<int64_t>(6 * kPage));
    CHECK(DirectMemoryAlloc(0, kPool, kPage, 3 * kPage, 1, &c) == 0 && c == 0);
    CHECK(BlockIs(3 * kPage, 3 * kPage, 4 * kPage, 1));
    DirectMemoryFree(0, kPool);
    CHECK(PoolEmpty());
}

void alloc_exhaustion_and_gap_reuse() {
    int64_t a, b, c;
    CHECK(DirectMemoryAlloc(0, 3 * kPage, kPage, 0, 1, &a) == 0 && a == 0);
    CHECK(DirectMemoryAlloc(0, 3 * kPage, kPage, 0, 1, &b) == 0 && b == static_cast<int64_t>(kPage));
    CHECK(DirectMemoryAlloc(0, 3 * kPage, kPage, 0, 1, &c) == 0 && c == static_cast<int64_t>(2 * kPage));
    CHECK(DirectMemoryAlloc(0, 3 * kPage, kPage, 0, 1, &c) == SCE_KERNEL_ERROR_EAGAIN);
    DirectMemoryFree(static_cast<int64_t>(kPage), kPage);
    CHECK(DirectMemoryAlloc(0, 3 * kPage, kPage, 0, 1, &c) == 0 && c == static_cast<int64_t>(kPage));
    DirectMemoryFree(0, kPool);
}

void free_out_of_range_is_ignored() {
    int64_t a;
    CHECK(DirectMemoryAlloc(0, kPool, 4 * kPage, 0, 7, &a) == 0 && a == 0);
    DirectMemoryFree(0, SIZE_MAX);
    DirectMemoryFree(static_cast<int64_t>(kPool), kPage);
    DirectMemoryFree(static_cast<int64_t>(kPool - kPage), 2 * kPage);
    DirectMemoryFree(INT64_MAX, kPage);
    CHECK(!DirectMemoryCheckedFree(0, SIZE_MAX));
    CHECK(!DirectMemoryCheckedFree(static_cast<int64_t>(kPool), kPage));
    CHECK(BlockIs(0, 0, 4 * kPage, 7));
    DirectMemoryFree(0, kPool);
    CHECK(PoolEmpty());
}

void retype_out_of_range_is_ignored() {
    int64_t a;
    CHECK(DirectMemoryAlloc(0, kPool, 4 * kPage, 0, 7, &a) == 0 && a == 0);
    DirectMemoryRetype(0, SIZE_MAX, 9);
    DirectMemoryRetype(static_cast<int64_t>(kPool), kPage, 9);
    DirectMemoryRetype(static_cast<int64_t>(kPool - kPage), 2 * kPage, 9);
    CHECK(BlockIs(0, 0, 4 * kPage, 7));
    DirectMemoryFree(0, kPool);
}

void free_partial_and_spanning() {
    int64_t a, b;
    CHECK(DirectMemoryAlloc(0, kPool, 4 * kPage, 0, 1, &a) == 0 && a == 0);
    CHECK(DirectMemoryAlloc(0, kPool, 4 * kPage, 0, 2, &b) == 0 && b == static_cast<int64_t>(4 * kPage));
    DirectMemoryFree(static_cast<int64_t>(2 * kPage), 4 * kPage);
    CHECK(BlockIs(0, 0, 2 * kPage, 1));
    CHECK(BlockIs(6 * kPage, 6 * kPage, 8 * kPage, 2));
    CHECK(DirectMemoryFreeRun(2 * kPage, kPool) == 4 * kPage);
    CHECK(!DirectMemoryCheckedFree(0, 4 * kPage));
    CHECK(DirectMemoryCheckedFree(0, 2 * kPage));
    DirectMemoryFree(0, kPool);
    CHECK(PoolEmpty());
}

void alloc_skips_a_claimed_release() {
    int64_t a = -1, b = -1;
    CHECK(DirectMemoryAlloc(0, kPool, kPage, 0, 1, &a) == 0 && a == 0);
    CHECK(DirectMemoryClaimRelease(0, 4 * kPage, false));
    CHECK(DirectMemoryAlloc(0, 4 * kPage, kPage, 0, 1, &b) == SCE_KERNEL_ERROR_EAGAIN);
    CHECK(DirectMemoryAlloc(0, 5 * kPage, kPage, 0, 1, &b) == 0 && b == static_cast<int64_t>(4 * kPage));
    DirectMemoryAbandonRelease(0, 4 * kPage);
    CHECK(DirectMemoryAlloc(0, 4 * kPage, kPage, 0, 1, &b) == 0 && b == static_cast<int64_t>(kPage));
    DirectMemoryFree(0, kPool);
    CHECK(PoolEmpty());
}

void alloc_skips_a_long_used_run() {
    constexpr uint64_t kRun = 16384;
    int64_t a = -1;
    bool placed = true;
    for (uint64_t i = 0; i < kRun; ++i) placed = placed && DirectMemoryAlloc(0, kPool, kPage, 0, 1, &a) == 0 && a == static_cast<int64_t>(i * kPage);
    CHECK(placed);
    CHECK(DirectMemoryAlloc(0, kPool, 4 * kPage, 0, 1, &a) == 0 && a == static_cast<int64_t>(kRun * kPage));
    DirectMemoryFree(static_cast<int64_t>(100 * kPage), 3 * kPage);
    CHECK(DirectMemoryAlloc(0, kPool, 4 * kPage, 0, 1, &a) == 0 && a == static_cast<int64_t>((kRun + 4) * kPage));
    CHECK(DirectMemoryAlloc(0, kPool, 4 * kPage, 5 * kPage, 1, &a) == 0 && a == static_cast<int64_t>((kRun + 11) * kPage));
    CHECK(DirectMemoryAlloc(0, kPool, 3 * kPage, 0, 1, &a) == 0 && a == static_cast<int64_t>(100 * kPage));
    CHECK(DirectMemoryAlloc(0, static_cast<int64_t>(kRun * kPage), kPage, 0, 1, &a) == SCE_KERNEL_ERROR_EAGAIN);
    CHECK(DirectMemoryAlloc(0, static_cast<int64_t>(kRun * kPage + kPage / 2), kPage, 0, 1, &a) == SCE_KERNEL_ERROR_EAGAIN);
    DirectMemoryFree(0, kPool);
    CHECK(PoolEmpty());

    CHECK(DirectMemoryAlloc(static_cast<int64_t>(kPool - kRun * kPage), INT64_MAX, kRun * kPage, 0, 1, &a) == 0 && a == static_cast<int64_t>(kPool - kRun * kPage));
    CHECK(DirectMemoryAlloc(static_cast<int64_t>(kPool - kRun * kPage), INT64_MAX, kPage, 0, 1, &a) == SCE_KERNEL_ERROR_EAGAIN);
    CHECK(DirectMemoryAlloc(static_cast<int64_t>(kPool - (kRun + 3) * kPage), INT64_MAX, kPage, kPage + 1, 1, &a) == 0 && a == static_cast<int64_t>(kPool - (kRun + 2) * kPage));
    DirectMemoryFree(0, kPool);
    CHECK(PoolEmpty());
}

struct FirstFitModel {
    std::vector<bool> used = std::vector<bool>(static_cast<size_t>(kPool / kPage), false);
    std::map<uint64_t, uint64_t> claims;

    bool Claimed(uint64_t lo, uint64_t hi) const {
        return std::any_of(claims.begin(), claims.end(), [&](const auto& claim) { return claim.first < hi && lo < claim.second; });
    }

    int64_t Find(uint64_t start, uint64_t end, uint64_t len, uint64_t align) const {
        for (uint64_t cur = (start + align - 1) / align * align; cur <= end && len <= end - cur; cur += align) {
            bool fits = !Claimed(cur, cur + len);
            for (uint64_t page = cur / kPage; fits && page < (cur + len) / kPage; ++page) fits = !used[static_cast<size_t>(page)];
            if (fits) return static_cast<int64_t>(cur);
        }
        return -1;
    }

    void Mark(uint64_t lo, uint64_t hi, bool value) {
        for (uint64_t page = lo / kPage; page < hi / kPage; ++page) used[static_cast<size_t>(page)] = value;
    }
};

void alloc_matches_a_first_fit_model() {
    constexpr uint64_t kWindowPages = 512;
    const uint64_t alignments[] = {0, kPage / 2, kPage, kPage + 1, 2 * kPage, 3 * kPage - 1, 3 * kPage, 4 * kPage, 16 * kPage};
    for (const uint64_t seed : {1u, 2u, 3u}) {
        std::mt19937_64 rng(seed);
        const auto pick = [&](uint64_t lo, uint64_t hi) { return std::uniform_int_distribution<uint64_t>(lo, hi)(rng); };
        FirstFitModel model;
        int mismatches = 0;
        for (int op = 0; op < 4000 && mismatches == 0; ++op) {
            const uint64_t kind = pick(0, 9);
            if (kind < 5) {
                const uint64_t len = pick(1, 8) * kPage;
                const uint64_t alignment = alignments[pick(0, std::size(alignments) - 1)];
                const uint64_t align = (std::max(alignment, kPage) + kPage - 1) / kPage * kPage;
                const uint64_t start = pick(0, kWindowPages) * kPage + (pick(0, 3) == 0 ? kPage / 2 : 0);
                const uint64_t end = pick(0, 7) == 0 ? (pick(0, 1) == 0 ? kPool : static_cast<uint64_t>(INT64_MAX)) : start + pick(1, kWindowPages) * kPage;
                const int64_t expected = model.Find(start, std::min(end, kPool), len, align);
                int64_t out = -1;
                const int result = DirectMemoryAlloc(static_cast<int64_t>(start), static_cast<int64_t>(end), len, alignment, 1, &out);
                if (expected < 0 ? result != SCE_KERNEL_ERROR_EAGAIN : (result != 0 || out != expected)) {
                    std::printf("  seed %llu op %d: alloc start 0x%llx end 0x%llx len 0x%llx alignment 0x%llx returned %d at 0x%llx, the model expects 0x%llx\n", static_cast<unsigned long long>(seed), op, static_cast<unsigned long long>(start), static_cast<unsigned long long>(end), static_cast<unsigned long long>(len), static_cast<unsigned long long>(alignment), result, static_cast<unsigned long long>(out), static_cast<unsigned long long>(expected));
                    ++mismatches;
                }
                if (expected >= 0 && result == 0) model.Mark(static_cast<uint64_t>(out), static_cast<uint64_t>(out) + len, true);
            } else if (kind < 8) {
                const uint64_t lo = pick(0, kWindowPages) * kPage;
                const uint64_t len = pick(1, 16) * kPage;
                DirectMemoryFree(static_cast<int64_t>(lo), len);
                model.Mark(lo, lo + len, false);
            } else if (kind < 9) {
                const uint64_t lo = pick(0, kWindowPages) * kPage;
                const uint64_t len = pick(1, 8) * kPage;
                if (model.Claimed(lo, lo + len)) continue;
                if (DirectMemoryReleaseClaimed(lo, lo + len, nullptr)) {
                    std::printf("  seed %llu op %d: 0x%llx+0x%llx is claimed, the model has no claim there\n", static_cast<unsigned long long>(seed), op, static_cast<unsigned long long>(lo), static_cast<unsigned long long>(len));
                    ++mismatches;
                    continue;
                }
                CHECK(DirectMemoryClaimRelease(static_cast<int64_t>(lo), len, false));
                model.claims.emplace(lo, lo + len);
            } else if (!model.claims.empty()) {
                const auto claim = std::next(model.claims.begin(), static_cast<std::ptrdiff_t>(pick(0, model.claims.size() - 1)));
                DirectMemoryAbandonRelease(static_cast<int64_t>(claim->first), claim->second - claim->first);
                model.claims.erase(claim);
            }
        }
        CHECK(mismatches == 0);
        for (const auto& claim : model.claims) DirectMemoryAbandonRelease(static_cast<int64_t>(claim.first), claim.second - claim.first);
        DirectMemoryFree(0, kPool);
        CHECK(PoolEmpty());
    }
}

void HoldMap(uint64_t first, uint64_t last) {
    std::lock_guard<std::mutex> lock(g_mapHoldLock);
    g_mapHolds.emplace_back(first, last);
}

void EndMapHold(uint64_t first, uint64_t last) {
    {
        std::lock_guard<std::mutex> lock(g_mapHoldLock);
        g_mapHolds.erase(std::find(g_mapHolds.begin(), g_mapHolds.end(), std::make_pair(first, last)));
    }
    DirectMemoryWakeReleaseClaims();
}

void claims_wait_for_map_holds() {
    int64_t a = -1;
    CHECK(DirectMemoryAlloc(0, kPool, 4 * kPage, 0, 1, &a) == 0 && a == 0);
    for (const bool requireAllocated : {false, true}) {
        HoldMap(kPage, 2 * kPage);
        std::atomic<bool> claimed{false};
        bool result = false;
        std::thread claimer([&] {
            result = DirectMemoryClaimRelease(0, 4 * kPage, requireAllocated);
            claimed = true;
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        CHECK(!claimed);
        CHECK(!DirectMemoryReleaseClaimed(0, 4 * kPage, nullptr));
        EndMapHold(kPage, 2 * kPage);
        claimer.join();
        CHECK(result && DirectMemoryReleaseClaimed(0, 4 * kPage, nullptr));
        DirectMemoryAbandonRelease(0, 4 * kPage);
    }
    HoldMap(4 * kPage, 5 * kPage);
    CHECK(DirectMemoryClaimRelease(0, 4 * kPage, false));
    DirectMemoryAbandonRelease(0, 4 * kPage);
    EndMapHold(4 * kPage, 5 * kPage);
    DirectMemoryFree(0, kPool);
    CHECK(PoolEmpty());
}

void retype_splits_and_skips_gaps() {
    int64_t a;
    CHECK(DirectMemoryAlloc(2 * kPage, kPool, 4 * kPage, 0, 1, &a) == 0 && a == static_cast<int64_t>(2 * kPage));
    DirectMemoryRetype(static_cast<int64_t>(3 * kPage), kPage, 9);
    CHECK(BlockIs(2 * kPage, 2 * kPage, 3 * kPage, 1));
    CHECK(BlockIs(3 * kPage, 3 * kPage, 4 * kPage, 9));
    CHECK(BlockIs(4 * kPage, 4 * kPage, 6 * kPage, 1));
    DirectMemoryRetype(0, 8 * kPage, 5);
    CHECK(BlockIs(2 * kPage, 2 * kPage, 3 * kPage, 5));
    CHECK(BlockIs(5 * kPage, 4 * kPage, 6 * kPage, 5));
    int64_t s, e;
    int t;
    CHECK(!BlockAt(0, s, e, t) && !BlockAt(6 * kPage, s, e, t));
    DirectMemoryFree(0, kPool);
}

void retype_locked_needs_the_pool_lock() {
    int64_t a;
    CHECK(DirectMemoryAlloc(0, kPool, 2 * kPage, 0, 1, &a) == 0 && a == 0);
    std::mutex other;
    bool refused = false;
    try {
        std::unique_lock<std::mutex> foreign(other);
        DirectMemoryRetypeLocked(foreign, 0, kPage, 9);
    } catch (const std::logic_error&) {
        refused = true;
    }
    CHECK(refused && BlockIs(0, 0, 2 * kPage, 1));
    {
        const auto pool = DirectMemoryPoolLock();
        DirectMemoryRetypeLocked(pool, static_cast<int64_t>(kPage), kPage, 9);
    }
    CHECK(BlockIs(0, 0, kPage, 1) && BlockIs(kPage, kPage, 2 * kPage, 9));
    DirectMemoryFree(0, kPool);
    CHECK(PoolEmpty());
}

void alloc_backing_bad_alloc_rolls_back() {
    int64_t out = -1;
    g_fault = Fault::BackingBadAlloc;
    CHECK(DirectMemoryAlloc(0, kPool, 4 * kPage, 0, 1, &out) == SCE_KERNEL_ERROR_ENOMEM);
    g_fault = Fault::None;
    CHECK(out == -1);
    CHECK(PoolEmpty());
    CHECK(DirectMemoryAlloc(0, kPool, 4 * kPage, 0, 1, &out) == 0 && out == 0);
    DirectMemoryFree(0, kPool);
}

void alloc_node_bad_alloc_rolls_back() {
    int64_t out = -1;
    {
        NewBudget budget(0);
        CHECK(DirectMemoryAlloc(0, kPool, 4 * kPage, 0, 1, &out) == SCE_KERNEL_ERROR_ENOMEM);
    }
    CHECK(out == -1);
    CHECK(PoolEmpty());
    CHECK(DirectMemoryAlloc(0, kPool, 4 * kPage, 0, 1, &out) == 0 && out == 0);
    DirectMemoryFree(0, kPool);
}

void alloc_unexpected_exception_propagates() {
    int64_t out = -1;
    g_fault = Fault::BackingRuntimeError;
    bool threw = false;
    try {
        (void)DirectMemoryAlloc(0, kPool, 4 * kPage, 0, 1, &out);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    g_fault = Fault::None;
    CHECK(threw);
    CHECK(PoolEmpty());
}

void free_node_bad_alloc_rolls_back() {
    int64_t a;
    CHECK(DirectMemoryAlloc(0, kPool, 8 * kPage, 0, 3, &a) == 0 && a == 0);
    for (long budget = 0; budget < 2; ++budget) {
        bool threw = false;
        {
            NewBudget guard(budget);
            try {
                DirectMemoryFree(static_cast<int64_t>(2 * kPage), 2 * kPage);
            } catch (const std::bad_alloc&) {
                threw = true;
            }
        }
        CHECK(threw);
        CHECK(BlockIs(0, 0, 8 * kPage, 3));
        CHECK(DirectMemoryFreeRun(2 * kPage, 4 * kPage) == 0);
    }
    DirectMemoryFree(static_cast<int64_t>(2 * kPage), 2 * kPage);
    CHECK(BlockIs(0, 0, 2 * kPage, 3));
    CHECK(BlockIs(4 * kPage, 4 * kPage, 8 * kPage, 3));
    CHECK(DirectMemoryFreeRun(2 * kPage, 4 * kPage) == 2 * kPage);
    DirectMemoryFree(0, kPool);
}

void retype_node_bad_alloc_rolls_back() {
    int64_t a;
    CHECK(DirectMemoryAlloc(0, kPool, 8 * kPage, 0, 3, &a) == 0 && a == 0);
    for (long budget = 0; budget < 2; ++budget) {
        bool threw = false;
        {
            NewBudget guard(budget);
            try {
                DirectMemoryRetype(static_cast<int64_t>(2 * kPage), 2 * kPage, 9);
            } catch (const std::bad_alloc&) {
                threw = true;
            }
        }
        CHECK(threw);
        CHECK(BlockIs(0, 0, 8 * kPage, 3));
    }
    DirectMemoryRetype(static_cast<int64_t>(2 * kPage), 2 * kPage, 9);
    CHECK(BlockIs(2 * kPage, 2 * kPage, 4 * kPage, 9));
    DirectMemoryFree(0, kPool);
}

void forget_exception_propagates_and_rolls_back() {
    int64_t a;
    CHECK(DirectMemoryAlloc(0, kPool, 8 * kPage, 0, 3, &a) == 0 && a == 0);
    g_fault = Fault::ForgetRuntimeError;
    bool threwFree = false, threwChecked = false;
    try {
        DirectMemoryFree(static_cast<int64_t>(2 * kPage), 2 * kPage);
    } catch (const std::runtime_error&) {
        threwFree = true;
    }
    try {
        (void)DirectMemoryCheckedFree(static_cast<int64_t>(2 * kPage), 2 * kPage);
    } catch (const std::runtime_error&) {
        threwChecked = true;
    }
    g_fault = Fault::None;
    CHECK(threwFree);
    CHECK(threwChecked);
    CHECK(BlockIs(0, 0, 8 * kPage, 3));
    DirectMemoryFree(0, kPool);
    CHECK(PoolEmpty());
}

struct Case {
    const char* name;
    void (*fn)();
};

const Case kCases[] = {
    {"alloc_unplaceable_requests", alloc_unplaceable_requests},
    {"alloc_null_out", alloc_null_out},
    {"alloc_rounding_alignment_and_clamping", alloc_rounding_alignment_and_clamping},
    {"alloc_non_power_of_two_alignment", alloc_non_power_of_two_alignment},
    {"alloc_exhaustion_and_gap_reuse", alloc_exhaustion_and_gap_reuse},
    {"free_out_of_range_is_ignored", free_out_of_range_is_ignored},
    {"retype_out_of_range_is_ignored", retype_out_of_range_is_ignored},
    {"free_partial_and_spanning", free_partial_and_spanning},
    {"alloc_skips_a_claimed_release", alloc_skips_a_claimed_release},
    {"alloc_skips_a_long_used_run", alloc_skips_a_long_used_run},
    {"alloc_matches_a_first_fit_model", alloc_matches_a_first_fit_model},
    {"claims_wait_for_map_holds", claims_wait_for_map_holds},
    {"retype_splits_and_skips_gaps", retype_splits_and_skips_gaps},
    {"retype_locked_needs_the_pool_lock", retype_locked_needs_the_pool_lock},
    {"alloc_backing_bad_alloc_rolls_back", alloc_backing_bad_alloc_rolls_back},
    {"alloc_node_bad_alloc_rolls_back", alloc_node_bad_alloc_rolls_back},
    {"alloc_unexpected_exception_propagates", alloc_unexpected_exception_propagates},
    {"free_node_bad_alloc_rolls_back", free_node_bad_alloc_rolls_back},
    {"retype_node_bad_alloc_rolls_back", retype_node_bad_alloc_rolls_back},
    {"forget_exception_propagates_and_rolls_back", forget_exception_propagates_and_rolls_back},
};

}

void CreateDirectMemoryBacking(int64_t, size_t) {
    if (g_fault == Fault::BackingBadAlloc) throw std::bad_alloc();
    if (g_fault == Fault::BackingRuntimeError) throw std::runtime_error("backing");
}

bool DirectMemoryReleaseClaimed(std::uint64_t first, std::uint64_t last, std::uint64_t* claimEnd) {
    auto claim = g_releaseClaims.lower_bound(last);
    if (claim == g_releaseClaims.begin()) return false;
    --claim;
    if (claim->second <= first) return false;
    if (claimEnd) *claimEnd = claim->second;
    return true;
}

bool DirectMemoryTryAddReleaseClaim(std::uint64_t first, std::uint64_t last) {
    std::lock_guard<std::mutex> lock(g_mapHoldLock);
    if (std::any_of(g_mapHolds.begin(), g_mapHolds.end(), [&](const auto& hold) { return hold.first < last && first < hold.second; })) return false;
    g_releaseClaims.emplace(first, last);
    return true;
}

void DirectMemoryRemoveReleaseClaim(std::uint64_t first) { g_releaseClaims.erase(first); }

void ForgetDirectMemory(int64_t, size_t) {
    if (g_fault == Fault::ForgetRuntimeError) throw std::runtime_error("forget");
}

void* operator new(size_t n) {
    if (g_newBudget == 0) throw std::bad_alloc();
    if (g_newBudget > 0) --g_newBudget;
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--list") == 0) {
        for (const Case& c : kCases) std::printf("%s\n", c.name);
        return 0;
    }
    int ran = 0;
    for (const Case& c : kCases) {
        if (argc == 2 && std::strcmp(argv[1], c.name) != 0) continue;
        std::printf("[ RUN ] %s\n", c.name);
        const int before = g_failures;
        try {
            c.fn();
        } catch (...) {
            std::printf("  FAILED: unexpected exception escaped\n");
            ++g_failures;
        }
        std::printf("[ %s ] %s\n", g_failures == before ? " OK " : "FAIL", c.name);
        ++ran;
    }
    if (ran == 0) {
        std::printf("no such case\n");
        return 2;
    }
    return g_failures == 0 ? 0 : 1;
}
