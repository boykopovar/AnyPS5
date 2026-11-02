#include "prx/libc/include/GuestAllocations.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/mman.h>

namespace {

std::atomic<long> failAt{0};

void* Allocate(std::size_t bytes) {
    long remaining = failAt.load(std::memory_order_relaxed);
    while (remaining > 0) {
        if (failAt.compare_exchange_weak(remaining, remaining - 1, std::memory_order_relaxed)) {
            if (remaining == 1) throw std::bad_alloc();
            break;
        }
    }
    if (void* pointer = std::malloc(bytes != 0 ? bytes : 1)) return pointer;
    throw std::bad_alloc();
}

constexpr std::size_t Page = 0x4000;
constexpr std::size_t Pages = 4;

struct Snapshot {
    std::array<std::optional<GuestAllocations::Range>, Pages> pages;
};

bool Same(const std::optional<GuestAllocations::Range>& left, const std::optional<GuestAllocations::Range>& right) {
    if (!left || !right) return left.has_value() == right.has_value();
    return left->address == right->address && left->bytes == right->bytes && left->readable == right->readable && left->writable == right->writable && left->allocationAddress == right->allocationAddress && left->allocationBytes == right->allocationBytes && left->gpu == right->gpu;
}

Snapshot Take(std::uintptr_t base) {
    Snapshot snapshot;
    for (std::size_t page = 0; page < Pages; ++page) {
        const auto found = GuestAllocations::GuestAllocationsLookup_nid_postfix(base + page * Page, false);
        if (found != nullptr) snapshot.pages[page] = *found;
    }
    return snapshot;
}

bool Equal(const Snapshot& left, const Snapshot& right) {
    for (std::size_t page = 0; page < Pages; ++page) {
        if (!Same(left.pages[page], right.pages[page])) return false;
    }
    return true;
}

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint8_t* Reserve() {
    void* raw = mmap(nullptr, Page * Pages, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(raw != MAP_FAILED, "cannot reserve the test range");
    return static_cast<std::uint8_t*>(raw);
}

void Release(std::uint8_t* base) {
    munmap(base, Page * Pages);
}

struct Window {
    explicit Window(long failure) { failAt.store(failure, std::memory_order_relaxed); }
    ~Window() { failAt.store(0, std::memory_order_relaxed); }
};

struct Attempt {
    bool threw = false;
    bool applied = false;
};

Attempt Protect(std::uintptr_t base, long failure) {
    Attempt attempt;
    try {
        GuestAllocations::Mutation mutation;
        const Window window(failure);
        mutation.Protect(reinterpret_cast<const void*>(base + Page), Page, true, false, false, [&] { attempt.applied = true; });
    } catch (const std::bad_alloc&) {
        attempt.threw = true;
    }
    failAt.store(0, std::memory_order_relaxed);
    return attempt;
}

Attempt Unmap(std::uintptr_t base, long failure) {
    Attempt attempt;
    try {
        GuestAllocations::Mutation mutation;
        const Window window(failure);
        mutation.Unmap(reinterpret_cast<const void*>(base + Page), Page, [&](const void*, std::size_t, const void*, bool) { attempt.applied = true; });
    } catch (const std::bad_alloc&) {
        attempt.threw = true;
    }
    failAt.store(0, std::memory_order_relaxed);
    return attempt;
}

template<typename TOperation>
std::size_t Sweep(const char* name, TOperation operation) {
    std::size_t injected = 0;
    for (long failure = 1; failure <= 64; ++failure) {
        auto* region = Reserve();
        const auto base = reinterpret_cast<std::uintptr_t>(region);
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(region, Page * Pages, true, true, false);
        }
        Attempt attempt;
        {
            const auto before = Take(base);
            attempt = operation(base, failure);
            const auto after = Take(base);
            if (attempt.threw) {
                ++injected;
                Require(!attempt.applied, std::string(name) + ": an allocation failure after allocation " + std::to_string(failure) + " happened after the host mapping changed");
                Require(Equal(before, after), std::string(name) + ": an allocation failure after allocation " + std::to_string(failure) + " left the registry edited");
            } else {
                Require(attempt.applied, std::string(name) + ": the operation finished without changing the host mapping");
                Require(!Equal(before, after), std::string(name) + ": the operation finished without changing the registry");
            }
        }
        {
            GuestAllocations::Mutation mutation;
            mutation.Unmap(region, Page * Pages, [](const void*, std::size_t, const void*, bool) {});
        }
        Release(region);
        if (!attempt.threw) break;
    }
    return injected;
}

}

void* operator new(std::size_t bytes) { return Allocate(bytes); }
void* operator new[](std::size_t bytes) { return Allocate(bytes); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

int main() {
    try {
        const auto protectInjected = Sweep("protect", Protect);
        const auto unmapInjected = Sweep("unmap", Unmap);
        Require(protectInjected > 0 && unmapInjected > 0, "no allocation failure was injected");
        std::printf("guest allocation registry kept its state through %zu protect and %zu unmap allocation failures\n", protectInjected, unmapInjected);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
