#include "prx/libc/include/GuestAllocations.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace {

using Spans = std::vector<std::pair<std::uint64_t, std::uint64_t>>;

constexpr std::size_t RegionBytes = 0x40000;
constexpr std::size_t Alignment = 0x10000;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest allocation changes: %s\n", message);
    std::exit(1);
}

std::uint64_t Generation() {
    return GuestAllocations::GuestAllocationsGeneration_nid_postfix();
}

bool Since(std::uint64_t since, Spans& spans, std::uint64_t& current) {
    spans.clear();
    return GuestAllocations::GuestAllocationsChangesSince_nid_no_patch(since, spans, current);
}

bool Covers(Spans spans, std::uint64_t begin, std::uint64_t end) {
    std::sort(spans.begin(), spans.end());
    auto reached = begin;
    for (const auto& [first, last] : spans) {
        if (first > reached) break;
        reached = std::max(reached, last);
        if (reached >= end) return true;
    }
    return reached >= end;
}

struct Region {
    std::unique_ptr<unsigned char[]> storage{new unsigned char[RegionBytes + Alignment]};
    unsigned char* base = reinterpret_cast<unsigned char*>((reinterpret_cast<std::uintptr_t>(storage.get()) + Alignment - 1) & ~(Alignment - 1));
    std::uint64_t At(std::size_t offset) const { return reinterpret_cast<std::uintptr_t>(base) + offset; }
};

void Logged(const Region& region) {
    Spans spans;
    std::uint64_t current = 0;
    Require(!Since(0, spans, current), "a generation older than the log is answered");
    const auto start = Generation();
    Require(Since(start, spans, current) && spans.empty() && current == start, "the current generation reports changes");
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(region.base, 0x10000, true, true, false);
    }
    Require(Since(start, spans, current) && current == start + 1 && Covers(spans, region.At(0), region.At(0x10000)), "an added range is not logged");
    const auto added = current;
    {
        GuestAllocations::Mutation mutation;
        mutation.Protect(region.base + 0x4000, 0x4000, true, false, false, [] {});
    }
    Require(Since(added, spans, current) && Covers(spans, region.At(0), region.At(0x10000)), "a protection does not log the pieces it re-creates");
    Require(!Covers(spans, region.At(0x10000), region.At(0x10001)), "a protection logs memory outside its ranges");
    const auto protectedAt = current;
    {
        GuestAllocations::Mutation mutation;
        mutation.RequireUnpinned(region.base + 0x9000, 0x1000);
    }
    Require(Since(protectedAt, spans, current) && current == protectedAt + 1 && Covers(spans, region.At(0x8000), region.At(0x10000)), "a renewed backing is not logged over its whole range");
    const auto renewed = current;
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(region.base);
    }
    Require(Since(renewed, spans, current) && Covers(spans, region.At(0), region.At(0x10000)), "a removed allocation is not logged");
    const auto removed = current;
    GuestAllocations::GuestAllocationsInvalidate_nid_postfix(region.At(0x20000), 0x1000);
    Require(Since(removed, spans, current) && current == removed + 1 && Covers(spans, region.At(0x20000), region.At(0x21000)), "an invalidation is not logged");
    Require(Since(start, spans, current) && Covers(spans, region.At(0), region.At(0x10000)) && Covers(spans, region.At(0x20000), region.At(0x21000)), "older changes are not reported with newer ones");
}

void Overflow(const Region& region) {
    Spans spans;
    std::uint64_t current = 0;
    const auto old = Generation();
    for (std::size_t index = 0; index < 20000; ++index) GuestAllocations::GuestAllocationsInvalidate_nid_postfix(region.At(0x30000), 0x10);
    Require(!Since(old, spans, current), "a generation whose changes were dropped is answered");
    const auto recent = Generation();
    GuestAllocations::GuestAllocationsInvalidate_nid_postfix(region.At(0x31000), 0x10);
    Require(Since(recent, spans, current) && spans.size() == 1 && Covers(spans, region.At(0x31000), region.At(0x31010)), "a recent generation is refused after an overflow");
}

void Retiring(const Region& region) {
    Spans spans;
    std::uint64_t current = 0;
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(region.base, 0x10000, true, true, false);
    }
    const auto before = Generation();
    auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    Require(std::any_of(lease.begin(), lease.end(), [&](const auto& range) { return range->address == region.At(0); }), "the leased registry misses the range");
    std::atomic<bool> done{false};
    std::thread remover([&] {
        GuestAllocations::Mutation mutation;
        mutation.Remove(region.base);
        done.store(true);
    });
    bool refused = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!refused && std::chrono::steady_clock::now() < deadline) {
        refused = !Since(before, spans, current);
        if (!refused) std::this_thread::yield();
    }
    Require(refused, "changes are answered while a range waits for its lease");
    Require(!done.load(), "a leased range was removed");
    lease.clear();
    remover.join();
    Require(Since(before, spans, current) && Covers(spans, region.At(0), region.At(0x10000)), "the removal after the lease is not logged");
}

void Untracked() {
    Spans spans;
    std::uint64_t current = 0;
    const auto before = Generation();
    {
        GuestAllocations::Mutation mutation;
        mutation.RegisterMainImage();
    }
    Require(!Since(before, spans, current), "changes are answered across an image registration");
    const auto after = Generation();
    Require(Since(after, spans, current) && spans.empty(), "generations after an image registration are refused");
}

}

int main() {
    const Region region;
    Logged(region);
    Overflow(region);
    Retiring(region);
    Untracked();
    return 0;
}
