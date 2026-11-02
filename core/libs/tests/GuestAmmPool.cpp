#include "prx/libkernel/Apr/include/AmmPool.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <new>
#include <string>

static std::atomic<long> failingAllocation{-1};

void* operator new(std::size_t size) {
    if (failingAllocation.load(std::memory_order_relaxed) >= 0 && failingAllocation.fetch_sub(1, std::memory_order_relaxed) == 0) throw std::bad_alloc();
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

static void RequireAt(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "GuestAmmPool.cpp:%d: requirement failed\n", line);
    std::abort();
}

#define Require(value) RequireAt((value), __LINE__)

namespace {

constexpr std::uint64_t page = AmmPool::PageBytes;
constexpr std::uint64_t address = 0x7000000000;

bool Equal(const AmmPool::Records& a, const AmmPool::Records& b) {
    const auto samePage = [](const AmmPool::Page& x, const AmmPool::Page& y) { return x.physical == y.physical && x.type == y.type && x.protection == y.protection && x.pooled == y.pooled; };
    if (a.pool != b.pool || a.pooledUses != b.pooledUses || a.pages.size() != b.pages.size()) return false;
    for (auto x = a.pages.begin(), y = b.pages.begin(); x != a.pages.end(); ++x, ++y) {
        if (x->first != y->first || !samePage(x->second, y->second)) return false;
    }
    return true;
}

std::uint64_t FreeBytes(const AmmPool::Records& records) {
    std::uint64_t free = 0;
    for (const auto& [offset, size] : records.pool) free += size;
    return free;
}

void RequireConsistent(const AmmPool::Records& records) {
    std::map<std::uint64_t, std::uint32_t> uses;
    for (const auto& [at, entry] : records.pages) {
        if (entry.pooled) ++uses[entry.physical];
    }
    Require(uses == records.pooledUses);
    std::uint64_t previousEnd = 0;
    bool first = true;
    for (const auto& [offset, size] : records.pool) {
        Require(size != 0 && offset % page == 0 && size % page == 0);
        Require(first || offset > previousEnd);
        previousEnd = offset + size;
        first = false;
    }
    for (const auto& [physical, count] : uses) {
        for (const auto& [offset, size] : records.pool) Require(physical >= offset + size || physical + page <= offset);
    }
}

AmmPool::Records Fragmented() {
    AmmPool::Records records;
    records.pool.emplace(0, page);
    records.pool.emplace(2 * page, page);
    records.pool.emplace(4 * page, page);
    records.pages.emplace(address - 2 * page, AmmPool::Page{page, 0, 3, true});
    records.pooledUses.emplace(page, 1);
    records.pages.emplace(address - page, AmmPool::Page{3 * page, 0, 3, true});
    records.pooledUses.emplace(3 * page, 1);
    return records;
}

void Map(AmmPool::Records& records, std::uint64_t bytes, const std::function<void(std::uint64_t, std::uint64_t, std::uint64_t)>& mapRun) {
    AmmPool::MapScattered(records, address, bytes, 1, 3, mapRun);
}

void TestScatteredMap() {
    auto records = Fragmented();
    std::map<std::uint64_t, std::uint64_t> runs;
    Map(records, 3 * page, [&](std::uint64_t at, std::uint64_t physical, std::uint64_t size) { runs.emplace(at, physical); Require(size == page); });
    Require(records.pool.empty());
    Require(runs.size() == 3 && runs.at(address) == 0 && runs.at(address + page) == 2 * page && runs.at(address + 2 * page) == 4 * page);
    for (std::uint64_t index = 0; index < 3; ++index) {
        const auto found = records.pages.find(address + index * page);
        Require(found != records.pages.end() && found->second.physical == 2 * index * page && found->second.pooled && found->second.type == 1 && found->second.protection == 3);
        Require(records.pooledUses.at(2 * index * page) == 1);
    }
    RequireConsistent(records);
}

void TestSplitRun() {
    AmmPool::Records records;
    records.pool.emplace(0, page);
    records.pool.emplace(2 * page, 3 * page);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
    Map(records, 3 * page, [&](std::uint64_t at, std::uint64_t physical, std::uint64_t size) { runs.emplace_back(physical, size); });
    Require(runs.size() == 2 && runs[0] == std::make_pair(std::uint64_t{0}, page) && runs[1] == std::make_pair(2 * page, 2 * page));
    Require(records.pool.size() == 1 && records.pool.at(4 * page) == page);
    Require(records.pages.size() == 3 && records.pooledUses.size() == 3);
    RequireConsistent(records);
}

void TestRefusals() {
    auto records = Fragmented();
    const auto before = records;
    bool refused = false;
    try {
        Map(records, 4 * page, [&](std::uint64_t, std::uint64_t, std::uint64_t) { Require(false); });
    } catch (const std::runtime_error& error) {
        refused = std::string(error.what()).starts_with("AMM: no free run of ");
    }
    Require(refused && Equal(records, before));
}

void TestHostMapFailsAfterEachRun() {
    for (int failAt = 0; failAt < 3; ++failAt) {
        auto records = Fragmented();
        const auto before = records;
        int mapped = 0;
        bool failed = false;
        try {
            Map(records, 3 * page, [&](std::uint64_t, std::uint64_t, std::uint64_t) {
                if (mapped++ == failAt) throw std::runtime_error("AMM: map failed");
            });
        } catch (const std::runtime_error&) {
            failed = true;
        }
        Require(failed && Equal(records, before));
        RequireConsistent(records);
        Map(records, 3 * page, [&](std::uint64_t, std::uint64_t, std::uint64_t) {});
        Require(FreeBytes(records) == 0);
        RequireConsistent(records);
    }
}

void TestEveryAllocationFails() {
    long failures = 0;
    for (long allocation = 0;; ++allocation) {
        Require(allocation < 1000);
        auto records = Fragmented();
        const auto before = records;
        bool failed = false;
        failingAllocation.store(allocation);
        try {
            Map(records, 3 * page, [&](std::uint64_t, std::uint64_t, std::uint64_t) {});
        } catch (const std::bad_alloc&) {
            failed = true;
        }
        failingAllocation.store(-1);
        if (!failed) {
            Require(records.pool.empty() && records.pages.size() == 5 && records.pooledUses.size() == 5);
            RequireConsistent(records);
            break;
        }
        ++failures;
        Require(Equal(records, before));
        RequireConsistent(records);
        Map(records, 3 * page, [&](std::uint64_t, std::uint64_t, std::uint64_t) {});
        Require(FreeBytes(records) == 0);
        RequireConsistent(records);
    }
    Require(failures >= 8);
}

void TestEveryAllocationFailsWithASplit() {
    long failures = 0;
    for (long allocation = 0;; ++allocation) {
        Require(allocation < 1000);
        AmmPool::Records records;
        records.pool.emplace(0, page);
        records.pool.emplace(2 * page, 3 * page);
        const auto before = records;
        bool failed = false;
        failingAllocation.store(allocation);
        try {
            Map(records, 3 * page, [&](std::uint64_t, std::uint64_t, std::uint64_t) {});
        } catch (const std::bad_alloc&) {
            failed = true;
        }
        failingAllocation.store(-1);
        if (!failed) {
            Require(records.pool.size() == 1 && records.pool.at(4 * page) == page && records.pages.size() == 3);
            RequireConsistent(records);
            break;
        }
        ++failures;
        Require(Equal(records, before));
        RequireConsistent(records);
        Map(records, 4 * page, [&](std::uint64_t, std::uint64_t, std::uint64_t) {});
        Require(FreeBytes(records) == 0);
    }
    Require(failures >= 8);
}

void TestPrtAllocationShape() {
    for (int failAt = 0; failAt < 3; ++failAt) {
        AmmPool::Records records;
        records.pool.emplace(0, 2 * page);
        records.pool.emplace(3 * page, page);
        records.pool.emplace(5 * page, 2 * page);
        const auto before = records;
        int mapped = 0;
        bool failed = false;
        try {
            Map(records, 5 * page, [&](std::uint64_t, std::uint64_t, std::uint64_t) {
                if (mapped++ == failAt) throw std::runtime_error("AMM: map failed");
            });
        } catch (const std::runtime_error&) {
            failed = true;
        }
        Require(failed && Equal(records, before));
        RequireConsistent(records);
        std::vector<std::uint64_t> sizes;
        Map(records, 5 * page, [&](std::uint64_t, std::uint64_t, std::uint64_t size) { sizes.push_back(size); });
        Require(sizes.size() == 3 && sizes[0] == 2 * page && sizes[1] == page && sizes[2] == 2 * page);
        Require(FreeBytes(records) == 0 && records.pages.size() == 5);
        RequireConsistent(records);
    }
}

void TestAddPageFailures() {
    long failures = 0;
    for (long allocation = 0;; ++allocation) {
        Require(allocation < 100);
        AmmPool::Records records;
        records.pages.emplace(address, AmmPool::Page{0, 0, 3, true});
        records.pooledUses.emplace(0, 1);
        const auto before = records;
        bool failed = false;
        failingAllocation.store(allocation);
        try {
            AmmPool::AddPage(records, address + page, AmmPool::Page{page, 0, 3, true});
        } catch (const std::bad_alloc&) {
            failed = true;
        }
        failingAllocation.store(-1);
        if (!failed) {
            Require(records.pages.size() == 2 && records.pooledUses.at(page) == 1);
            RequireConsistent(records);
            break;
        }
        ++failures;
        Require(Equal(records, before));
        RequireConsistent(records);
    }
    Require(failures >= 1);
}

}

int main() {
    try {
        TestScatteredMap();
        TestSplitRun();
        TestRefusals();
        TestHostMapFailsAfterEachRun();
        TestEveryAllocationFails();
        TestEveryAllocationFailsWithASplit();
        TestPrtAllocationShape();
        TestAddPageFailures();
        std::puts("AMM scattered pool pages published exactly once");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
