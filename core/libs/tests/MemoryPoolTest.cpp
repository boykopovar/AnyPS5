#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/DirectMemory/MemoryPool.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

bool DirectMemoryCheckedFree(int64_t start, size_t len);
void DirectMemoryRetype(int64_t start, size_t len, int memoryType);
size_t DirectMemoryFreeRun(uint64_t offset, uint64_t limit);

namespace {

constexpr uint64_t kPage = static_cast<uint64_t>(PS5_PAGE_SIZE);
constexpr uint64_t kPool = static_cast<uint64_t>(DIRECT_MEMORY_SIZE);

enum class Fault { None, BackingBadAlloc, BackingRuntimeError, ForgetRuntimeError };
Fault g_fault = Fault::None;
long g_newBudget = -1;

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
    {"retype_splits_and_skips_gaps", retype_splits_and_skips_gaps},
    {"alloc_backing_bad_alloc_rolls_back", alloc_backing_bad_alloc_rolls_back},
    {"alloc_node_bad_alloc_rolls_back", alloc_node_bad_alloc_rolls_back},
    {"alloc_unexpected_exception_propagates", alloc_unexpected_exception_propagates},
    {"free_node_bad_alloc_rolls_back", free_node_bad_alloc_rolls_back},
    {"retype_node_bad_alloc_rolls_back", retype_node_bad_alloc_rolls_back},
    {"forget_exception_propagates_and_rolls_back", forget_exception_propagates_and_rolls_back},
};

}

void CreateDirectMemoryBacking(int64_t, size_t, int) {
    if (g_fault == Fault::BackingBadAlloc) throw std::bad_alloc();
    if (g_fault == Fault::BackingRuntimeError) throw std::runtime_error("backing");
}

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
