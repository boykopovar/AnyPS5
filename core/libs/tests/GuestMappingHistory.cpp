#include "prx/libc/include/GuestAllocations.hpp"
#include <array>
#include <barrier>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
#include <source_location>
#include <thread>
#include <vector>

using namespace GuestAllocations;

namespace {

void Require(bool value, std::source_location where = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "Mapping history check failed at %s:%u\n", where.file_name(), where.line());
    std::abort();
}

auto Generation() { return GuestAllocationsGeneration_nid_postfix(); }
auto Validate(std::uintptr_t address, std::size_t bytes, std::uint64_t since) {
    return GuestAllocationsValidateMapping_nid_no_patch(address, bytes, since);
}

void MappingChanges() {
    constexpr std::uintptr_t base = 0x700000000;
    constexpr std::size_t bytes = 65536;
    auto* pointer = reinterpret_cast<void*>(base);
    const auto initial = Generation();
    {
        Mutation mutation;
        mutation.Add(pointer, bytes, true, true);
    }
    Require(Validate(base, bytes, initial) == 0);
    const auto mapped = Generation();
    Require(Validate(base, bytes, mapped) == mapped);
    Require(Validate(base - 1, 1, initial) == mapped);
    Require(Validate(base + bytes, 1, initial) == mapped);
    Require(Validate(base - 1, 2, initial) == 0);
    Require(Validate(base + bytes - 1, 2, initial) == 0);
    {
        Mutation mutation;
        mutation.Protect(reinterpret_cast<void*>(base + 4096), 4096, true, false, [] {});
    }
    Require(Validate(base, 4096, mapped) == Generation());
    Require(Validate(base + 4096, 1, mapped) == 0);
    Require(Validate(base, bytes, mapped) == 0);
    const auto beforeUnmap = Generation();
    {
        Mutation mutation;
        mutation.Unmap(reinterpret_cast<void*>(base + 8192), 4096, [](const void*, std::size_t, const void*, bool) {});
    }
    Require(Validate(base, 8192, beforeUnmap) == Generation());
    Require(Validate(base + 8192, 1, beforeUnmap) == 0);
    const auto protectedAt = Generation();
    {
        Mutation mutation;
    }
    Require(Validate(base, bytes, protectedAt) == Generation());
    GuestAllocationsInvalidate_nid_postfix(base + 3 * bytes, bytes);
    Require(Validate(base, bytes, protectedAt) == Generation());
    GuestAllocationsInvalidate_nid_postfix(base + bytes - 1, 1);
    Require(Validate(base, bytes, protectedAt) == 0);
    const auto beforeReplace = Generation();
    {
        Mutation mutation;
        mutation.Remove(pointer);
        mutation.Add(pointer, bytes, true, true);
    }
    Require(Validate(base, bytes, beforeReplace) == 0);
    const auto beforeRemove = Generation();
    {
        Mutation mutation;
        mutation.Remove(pointer);
    }
    Require(Validate(base, bytes, beforeRemove) == 0);
    Require(Validate(base, bytes, 0) == 0);
    Require(Validate(base, 0, Generation()) == 0);
    Require(Validate(std::numeric_limits<std::uintptr_t>::max(), 2, Generation()) == 0);
    Require(Validate(base, bytes, Generation() + 1) == 0);
}

void HistoryOverflow() {
    constexpr std::uintptr_t base = 0x710000000;
    const auto before = Generation();
    for (unsigned i = 0; i < 2048; ++i) GuestAllocationsInvalidate_nid_postfix(base, 1);
    Require(Validate(base + 1, 1, before) == 0);
    const auto recent = Generation();
    GuestAllocationsInvalidate_nid_postfix(base, 1);
    Require(Validate(base + 1, 1, recent) == Generation());
    Require(Validate(base, 1, recent) == 0);
    const auto batch = Generation();
    {
        Mutation mutation;
        for (unsigned i = 0; i < 2048; ++i) mutation.Add(reinterpret_cast<void*>(base + i * 4096), 4096, true, true);
    }
    Require(Validate(base, 1, batch) == 0);
    Require(Validate(base + 2047 * 4096, 1, batch) == 0);
    Require(Validate(base, 2048 * 4096, Generation()) == Generation());
    {
        Mutation mutation;
        for (unsigned i = 0; i < 2048; ++i) mutation.Remove(reinterpret_cast<void*>(base + i * 4096));
    }
}

void ReferenceHistory() {
    struct Change { std::uint64_t generation; std::uintptr_t begin; std::size_t bytes; };
    std::vector<Change> changes;
    std::mt19937 random(0x72c94);
    constexpr std::uintptr_t base = 0x720000000;
    for (unsigned i = 0; i < 12000; ++i) {
        const auto address = base + random() % 1048576;
        const auto bytes = 1 + random() % 16384;
        GuestAllocationsInvalidate_nid_postfix(address, bytes);
        changes.push_back({Generation(), address, bytes});
        for (unsigned query = 0; query < 8; ++query) {
            const auto index = changes.size() - 1 - random() % std::min<std::size_t>(changes.size(), 64);
            const auto since = changes[index].generation;
            const auto begin = base + random() % 1048576;
            const auto size = 1 + random() % 32768;
            bool unchanged = true;
            for (auto j = index + 1; j < changes.size(); ++j) {
                const auto& change = changes[j];
                if (change.begin < begin + size && begin < change.begin + change.bytes) unchanged = false;
            }
            Require(Validate(begin, size, since) == (unchanged ? Generation() : 0));
        }
    }
}

void ConcurrentChanges() {
    constexpr std::size_t workers = 8;
    std::barrier gate(static_cast<std::ptrdiff_t>(workers));
    std::array<std::thread, workers> threads;
    const auto initial = Generation();
    constexpr std::uintptr_t base = 0x730000000;
    for (std::size_t worker = 0; worker < workers; ++worker) {
        threads[worker] = std::thread([&, worker] {
            for (unsigned pass = 0; pass < 200; ++pass) {
                const auto since = Generation();
                gate.arrive_and_wait();
                GuestAllocationsInvalidate_nid_postfix(base + worker * 4096, 4096);
                gate.arrive_and_wait();
                Require(Validate(base + worker * 4096, 4096, since) == 0);
                Require(Validate(base + workers * 4096, 4096, since) != 0);
                gate.arrive_and_wait();
            }
        });
    }
    for (auto& thread : threads) thread.join();
    Require(Generation() - initial == workers * 200);
}

}

int main() {
    MappingChanges();
    HistoryOverflow();
    ReferenceHistory();
    ConcurrentChanges();
    std::puts("Mapping history preserves range identity, boundaries, replacement, overflow and concurrent publication");
}
