#include "prx/libc/include/GuestAllocations.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <limits>
#include <iterator>
#include <map>
#include <span>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <link.h>
#include <unistd.h>
#endif

namespace GuestAllocations {
namespace {

struct Registry {
    std::mutex mutex;
    std::map<std::uint64_t, std::shared_ptr<const Range>> ranges;
    bool mainImageRegistered = false;
};

Registry& registry() {
    static Registry value;
    return value;
}

std::atomic<std::uint64_t> generation{1};
std::atomic<void (*)(std::uintptr_t, std::size_t)> invalidator{nullptr};
std::atomic<bool (*)()> pinWaiter{nullptr};

struct MappingHistory {
    struct Change {
        std::uint64_t generation = 0;
        std::uintptr_t address = 0;
        std::size_t bytes = 0;
    };
    std::mutex mutex;
    std::array<Change, 1024> changes{};
    std::size_t next = 0;
    std::uint64_t discardedThrough = 0;
};

MappingHistory& mappingHistory() {
    static MappingHistory history;
    return history;
}

void publishChanges(std::span<const std::pair<std::uintptr_t, std::size_t>> ranges) {
    auto& history = mappingHistory();
    std::lock_guard lock(history.mutex);
    const auto nextGeneration = generation.load(std::memory_order_relaxed) + 1;
    for (const auto& [address, bytes] : ranges) {
        auto& change = history.changes[history.next];
        history.discardedThrough = change.generation;
        change = {nextGeneration, address, bytes};
        history.next = (history.next + 1) % history.changes.size();
    }
    generation.store(nextGeneration, std::memory_order_release);
}

std::chrono::milliseconds pinWait() {
    static const std::chrono::milliseconds value{[] {
        const char* text = std::getenv("APS5_PIN_WAIT_MS");
        return text != nullptr ? std::strtoull(text, nullptr, 10) : 60000ull;
    }()};
    return value;
}

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

// A mutation holds the registry lock and remembers the ranges it changed, which are reported to the
// invalidator once the generation has moved on, so a cache that observed the old state meanwhile
// notices either way.
struct MutationState {
    std::unique_lock<std::mutex> lock;
    std::vector<std::pair<std::uintptr_t, std::size_t>> changed;
};

void recordChange(void* mutation, const void* pointer, std::size_t bytes) {
    if (mutation != nullptr && bytes != 0) static_cast<MutationState*>(mutation)->changed.emplace_back(reinterpret_cast<std::uintptr_t>(pointer), bytes);
}

}

void* GuestAllocationsBegin_nid_postfix() {
    return new MutationState{std::unique_lock<std::mutex>(registry().mutex), {}};
}

void GuestAllocationsEnd_nid_postfix(void* mutation) noexcept {
    auto* state = static_cast<MutationState*>(mutation);
    publishChanges(state->changed);
    if (const auto callback = invalidator.load(std::memory_order_acquire)) {
        for (const auto& [address, bytes] : state->changed) callback(address, bytes);
    }
    delete state;
}

std::uint64_t GuestAllocationsGeneration_nid_postfix() {
    return generation.load(std::memory_order_acquire);
}

std::uint64_t GuestAllocationsValidateMapping_nid_no_patch(std::uintptr_t address, std::size_t bytes, std::uint64_t since) {
    if (since == 0 || bytes == 0 || bytes > std::numeric_limits<std::uintptr_t>::max() - address) return 0;
    auto current = generation.load(std::memory_order_acquire);
    if (since == current) return current;
    auto& history = mappingHistory();
    std::lock_guard lock(history.mutex);
    current = generation.load(std::memory_order_relaxed);
    if (since > current || since < history.discardedThrough) return 0;
    const auto end = address + bytes;
    auto index = history.next;
    for (std::size_t count = 0; count < history.changes.size(); ++count) {
        index = (index + history.changes.size() - 1) % history.changes.size();
        const auto& change = history.changes[index];
        if (change.generation <= since) break;
        if (change.bytes > std::numeric_limits<std::uintptr_t>::max() - change.address || (change.address < end && address < change.address + change.bytes)) return 0;
    }
    return current;
}

void GuestAllocationsSetInvalidator_nid_postfix(void (*callback)(std::uintptr_t, std::size_t)) {
    invalidator.store(callback, std::memory_order_release);
}

void GuestAllocationsInvalidate_nid_postfix(std::uintptr_t address, std::size_t bytes) {
    const std::pair range{address, bytes};
    publishChanges(std::span(&range, 1));
    if (const auto callback = invalidator.load(std::memory_order_acquire)) callback(address, bytes);
}

void GuestAllocationsSetPinWaiter_nid_postfix(bool (*callback)()) {
    pinWaiter.store(callback, std::memory_order_release);
}

#ifdef _WIN32
void GuestAllocationsRegisterMainImage_nid_postfix(void* mutation) {
    auto& state = registry();
    if (state.mainImageRegistered) return;
    const auto image = GetModuleHandleW(nullptr);
    require(image != nullptr, "cannot locate the main guest image");
    auto replacement = state.ranges;
    auto cursor = reinterpret_cast<std::uintptr_t>(image);
    bool registered = false;
    for (;;) {
        MEMORY_BASIC_INFORMATION memory{};
        require(VirtualQuery(reinterpret_cast<const void*>(cursor), &memory, sizeof(memory)) == sizeof(memory), "cannot query the main guest image");
        if (memory.AllocationBase != image) break;
        require(memory.Type == MEM_IMAGE && (memory.State == MEM_COMMIT || memory.State == MEM_RESERVE), "unsupported guest image mapping");
        require(reinterpret_cast<std::uintptr_t>(memory.BaseAddress) == cursor && memory.RegionSize != 0 && memory.RegionSize <= std::numeric_limits<std::uintptr_t>::max() - cursor, "invalid guest image range");
        if (memory.State == MEM_COMMIT) {
            require((memory.Protect & PAGE_GUARD) == 0, "guarded guest image pages are not supported");
            const auto protection = memory.Protect & 0xffu;
            const bool writable = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
            const bool readable = writable || protection == PAGE_READONLY || protection == PAGE_EXECUTE_READ;
            require(readable || protection == PAGE_NOACCESS || protection == PAGE_EXECUTE, "unsupported guest image protection");
            const auto next = replacement.lower_bound(cursor);
            require(next == replacement.end() || cursor + memory.RegionSize <= next->first, "guest image overlaps a registered allocation");
            if (next != replacement.begin()) {
                const auto& previous = *std::prev(next)->second;
                require(previous.address + previous.bytes <= cursor, "guest image overlaps a registered allocation");
            }
            replacement.emplace(cursor, std::make_shared<const Range>(Range{cursor, memory.RegionSize, readable, writable, cursor, memory.RegionSize, false}));
            recordChange(mutation, reinterpret_cast<const void*>(cursor), memory.RegionSize);
            registered = true;
        }
        cursor += memory.RegionSize;
    }
    require(registered, "main guest image has no committed pages");
    state.ranges.swap(replacement);
    state.mainImageRegistered = true;
}
#else
void GuestAllocationsRegisterMainImage_nid_postfix(void* mutation) {
    auto& state = registry();
    if (state.mainImageRegistered) return;
    struct Page {
        bool readable = false;
        bool writable = false;
    };
    std::map<std::uint64_t, Page> pages;
    const auto pageSize = static_cast<std::uint64_t>(::sysconf(_SC_PAGESIZE));
    std::pair<std::map<std::uint64_t, Page>*, std::uint64_t> collection{&pages, pageSize};
    dl_iterate_phdr([](dl_phdr_info* image, std::size_t, void* data) {
        auto& collected = *static_cast<std::pair<std::map<std::uint64_t, Page>*, std::uint64_t>*>(data);
        for (int index = 0; index < image->dlpi_phnum; ++index) {
            const auto& header = image->dlpi_phdr[index];
            if (header.p_type != PT_LOAD || header.p_memsz == 0) continue;
            const auto start = (image->dlpi_addr + header.p_vaddr) & ~(collected.second - 1);
            const auto end = (image->dlpi_addr + header.p_vaddr + header.p_memsz + collected.second - 1) & ~(collected.second - 1);
            for (auto page = start; page < end; page += collected.second) {
                auto& entry = (*collected.first)[page];
                entry.readable = entry.readable || (header.p_flags & (PF_R | PF_W)) != 0;
                entry.writable = entry.writable || (header.p_flags & PF_W) != 0;
            }
        }
        return 1;
    }, &collection);
    require(!pages.empty(), "main guest image has no loadable segments");
    auto replacement = state.ranges;
    for (auto page = pages.begin(); page != pages.end();) {
        auto last = page;
        while (std::next(last) != pages.end() && std::next(last)->first == last->first + pageSize && std::next(last)->second.readable == page->second.readable && std::next(last)->second.writable == page->second.writable) ++last;
        const auto address = page->first;
        const auto bytes = static_cast<std::size_t>(last->first + pageSize - address);
        const auto next = replacement.lower_bound(address);
        require(next == replacement.end() || address + bytes <= next->first, "guest image overlaps a registered allocation");
        if (next != replacement.begin()) {
            const auto& previous = *std::prev(next)->second;
            require(previous.address + previous.bytes <= address, "guest image overlaps a registered allocation");
        }
        replacement.emplace(address, std::make_shared<const Range>(Range{address, bytes, page->second.readable, page->second.writable, address, bytes, false}));
        recordChange(mutation, reinterpret_cast<const void*>(address), bytes);
        page = std::next(last);
    }
    state.ranges.swap(replacement);
    state.mainImageRegistered = true;
}
#endif

void GuestAllocationsAdd_nid_postfix(void* mutation, void* pointer, std::size_t bytes, bool readable, bool writable) {
    recordChange(mutation, pointer, bytes);
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(address != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest allocation range");
    require(!writable || readable, "writable guest allocation must be readable");
    auto& ranges = registry().ranges;
    const auto next = ranges.lower_bound(address);
    require(next == ranges.end() || (next->first != address && address + bytes <= next->first), "overlapping guest allocation");
    if (next != ranges.begin()) {
        const auto& previous = *std::prev(next)->second;
        require(previous.address + previous.bytes <= address, "overlapping guest allocation");
    }
    ranges.emplace(address, std::make_shared<const Range>(Range{address, bytes, readable, writable, address, bytes}));
}

[[noreturn]] void PinnedFailure(std::uintptr_t address, std::size_t bytes, const char* why) {
    char message[160];
    std::snprintf(message, sizeof(message), "guest allocation 0x%llx+0x%llx is owned by an active GPU command (%s)", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), why);
    throw std::runtime_error(message);
}

void GuestAllocationsRequireUnpinned_nid_postfix(void* mutation, const void* pointer, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(bytes <= std::numeric_limits<std::uint64_t>::max() - address, "guest allocation range overflow");
    const auto end = address + bytes;
    auto& ranges = registry().ranges;
    // GPU work leases allocations (a lease copies the shared_ptr) until the work completed, so wait
    // for it rather than failing the guest. The wait releases the registry lock: the driver's waiter
    // finishes that work under the device lock, under which the driver's workers acquire leases from
    // this registry, so waiting with the lock held would deadlock them. The scan restarts after the
    // lock is retaken, since another mutation may have run meanwhile. Without a waiter the lease is
    // dropped by another thread (a synchronous draw or dispatch), so yielding suffices. A waiter
    auto* state = static_cast<MutationState*>(mutation);
    const auto deadline = std::chrono::steady_clock::now() + pinWait();
    int syncedRounds = 0;
    for (;;) {
        bool pinned = false;
        auto it = ranges.upper_bound(address);
        if (it != ranges.begin()) --it;
        for (; it != ranges.end(); ++it) {
            const auto& [base, range] = *it;
            if (base >= end && base != address) break;
            if (((address < base + range->bytes && base < end) || base == address) && range.use_count() != 1) {
                pinned = true;
                break;
            }
        }
        if (!pinned) return;
        if (std::chrono::steady_clock::now() >= deadline) PinnedFailure(address, bytes, ("waited " + std::to_string(pinWait().count()) + " ms for the lease (APS5_PIN_WAIT_MS)").c_str());
        const auto waiter = pinWaiter.load(std::memory_order_acquire);
        bool progressed = false;
        if (waiter != nullptr && state != nullptr && state->lock.owns_lock()) {
            state->lock.unlock();
            progressed = waiter();
            state->lock.lock();
        } else {
            std::this_thread::yield();
        }
        if (progressed) {
            // The lease's work finished, yet the range is pinned again: between the waiter's return
            // and the re-lock a driver worker took a new lease (every address-based build pins every
            // registered range). Rare per round, but each round is a GPU wait, so a repeat is reported.
            if (++syncedRounds == 8) std::fprintf(stderr, "[gpu] leased allocation 0x%llx+0x%llx still pinned after %d recorder syncs\n", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), syncedRounds);
            continue;
        }
    }
}

void GuestAllocationsRequireAvailable_nid_postfix(void*, const void* pointer, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(address != 0 && bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid fixed guest mapping");
    const auto& ranges = registry().ranges;
    auto it = ranges.upper_bound(address);
    if (it != ranges.begin()) --it;
    for (; it != ranges.end(); ++it) {
        const auto& [base, range] = *it;
        if (base >= address + bytes) break;
        if (base + range->bytes > address) {
            char message[160];
            std::snprintf(message, sizeof(message), "fixed mapping 0x%llx+0x%llx overlaps a registered guest allocation 0x%llx+0x%llx", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), static_cast<unsigned long long>(base), static_cast<unsigned long long>(range->bytes));
            throw std::runtime_error(message);
        }
    }
}

bool GuestAllocationsOverlaps_nid_postfix(void*, const void* pointer, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(address != 0 && bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest allocation range");
    for (const auto& [base, range] : registry().ranges) {
        if (base >= address + bytes) break;
        if (base + range->bytes > address) return true;
    }
    return false;
}

bool GuestAllocationsCovers_nid_postfix(void*, const void* pointer, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(address != 0 && bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest allocation range");
    const auto end = address + bytes;
    auto cursor = address;
    const auto& ranges = registry().ranges;
    auto it = ranges.upper_bound(address);
    if (it != ranges.begin()) --it;
    for (; it != ranges.end(); ++it) {
        const auto& [base, range] = *it;
        const auto finish = base + range->bytes;
        if (finish <= cursor) continue;
        if (base > cursor || !range->releasable) return false;
        cursor = finish;
        if (cursor >= end) return true;
    }
    return false;
}

Range GuestAllocationsFind_nid_postfix(void*, const void* pointer) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const auto exact = registry().ranges.find(address);
    if (exact != registry().ranges.end() && exact->second->allocationAddress == address && exact->second->allocationBytes == exact->second->bytes) {
        const auto& range = *exact->second;
        require(range.releasable, "guest image memory is not a releasable allocation");
        return {address, range.allocationBytes, range.readable, range.writable, address, range.allocationBytes, range.releasable};
    }
    for (const auto& [base, range] : registry().ranges) {
        if (range->allocationAddress == address) {
            require(range->releasable, "guest image memory is not a releasable allocation");
            return {address, range->allocationBytes, range->readable, range->writable, address, range->allocationBytes, range->releasable};
        }
    }
    throw std::runtime_error("guest allocation is not registered");
}

void GuestAllocationsRemove_nid_postfix(void* mutation, const void* pointer) {
    const auto range = GuestAllocationsFind_nid_postfix(mutation, pointer);
    GuestAllocationsRequireUnpinned_nid_postfix(mutation, pointer, range.bytes);
    recordChange(mutation, pointer, range.bytes);
    auto& ranges = registry().ranges;
    const auto exact = ranges.find(range.address);
    if (exact != ranges.end() && exact->second->allocationBytes == exact->second->bytes) {
        ranges.erase(exact);
        return;
    }
    std::erase_if(ranges, [&](const auto& entry) { return entry.second->allocationAddress == range.address; });
}

namespace {

using RangeMap = std::map<std::uint64_t, std::shared_ptr<const Range>>;

struct RangeReplacement {
    RangeMap::iterator first;
    RangeMap::iterator last;
    RangeMap fragments;

    void Commit() {
        auto& ranges = registry().ranges;
        ranges.erase(first, last);
        ranges.merge(fragments);
    }
};

RangeReplacement replaceRange(const void* pointer, std::size_t bytes, bool remove, bool readable, bool writable) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest protection or unmap range");
    require(!writable || readable, "writable guest allocation must be readable");
    const auto end = address + bytes;
    auto& ranges = registry().ranges;
    auto first = ranges.upper_bound(address);
    if (first != ranges.begin() && std::prev(first)->first + std::prev(first)->second->bytes > address) --first;
    RangeReplacement replacement{first, ranges.lower_bound(end), {}};
    auto cursor = address;
    for (auto it = replacement.first; it != replacement.last; ++it) {
        const auto& [base, entry] = *it;
        const auto& range = *entry;
        const auto finish = base + range.bytes;
        require(base <= cursor, "guest protection or unmap range has a hole");
        const auto insert = [&](std::uint64_t first, std::uint64_t last, bool canRead, bool canWrite) {
            if (first < last) replacement.fragments.emplace(first, std::make_shared<const Range>(Range{first, static_cast<std::size_t>(last - first), canRead, canWrite, range.allocationAddress, range.allocationBytes, range.releasable}));
        };
        insert(base, std::max(base, address), range.readable, range.writable);
        if (!remove) insert(std::max(base, address), std::min(finish, end), readable, writable);
        insert(std::min(finish, end), finish, range.readable, range.writable);
        cursor = std::min(finish, end);
    }
    require(cursor == end, "guest protection or unmap range is not registered");
    return replacement;
}

}

void GuestAllocationsProtect_nid_postfix(void* mutation, const void* pointer, std::size_t bytes, bool readable, bool writable, const std::function<void()>& apply) {
    GuestAllocationsRequireUnpinned_nid_postfix(mutation, pointer, bytes);
    recordChange(mutation, pointer, bytes);
    auto replacement = replaceRange(pointer, bytes, false, readable, writable);
    apply();
    replacement.Commit();
}

void GuestAllocationsUnmap_nid_postfix(void* mutation, const void* pointer, std::size_t bytes, const std::function<void(const void*, std::size_t, const void*, bool)>& apply) {
    GuestAllocationsRequireUnpinned_nid_postfix(mutation, pointer, bytes);
    recordChange(mutation, pointer, bytes);
    const auto end = reinterpret_cast<std::uintptr_t>(pointer) + bytes;
    auto cursor = reinterpret_cast<std::uintptr_t>(pointer);
    bool any = false;
    while (cursor < end) {
        const auto found = registry().ranges.upper_bound(cursor);
        const Range* containing = nullptr;
        if (found != registry().ranges.begin()) {
            const auto& candidate = *std::prev(found)->second;
            if (cursor < candidate.allocationAddress + candidate.allocationBytes) containing = &candidate;
        }
        if (containing == nullptr) {
            if (found == registry().ranges.end() || found->first >= end) break;
            cursor = found->first;
            continue;
        }
        const auto range = *containing;
        require(range.releasable, "guest image memory cannot be unmapped");
        const auto pieceEnd = std::min<std::uint64_t>(end, range.allocationAddress + range.allocationBytes);
        auto replacement = replaceRange(reinterpret_cast<const void*>(cursor), pieceEnd - cursor, true, false, false);
        bool last = true;
        const auto& ranges = registry().ranges;
        const auto allocationEnd = range.allocationAddress + range.allocationBytes;
        for (auto it = ranges.lower_bound(range.allocationAddress); it != ranges.end() && it->first < allocationEnd; ++it) {
            const auto& [base, entry] = *it;
            if (entry->allocationAddress == range.allocationAddress && (base < cursor || base + entry->bytes > pieceEnd)) {
                last = false;
                break;
            }
        }
        apply(reinterpret_cast<const void*>(cursor), pieceEnd - cursor, reinterpret_cast<const void*>(range.allocationAddress), last);
        replacement.Commit();
        any = true;
        cursor = pieceEnd;
    }
    require(any, "unmap address is not registered");
}

Lease GuestAllocationsAcquire_nid_postfix() {
    std::lock_guard lock(registry().mutex);
    Lease result;
    for (const auto& [address, range] : registry().ranges) {
        if (range->readable && range->bytes != 0) result.push_back(range);
    }
    return result;
}

Lease GuestAllocationsAcquireRange_nid_no_patch(std::uintptr_t address, std::size_t bytes) {
    if (address == 0 || bytes == 0 || bytes > std::numeric_limits<std::uintptr_t>::max() - address) return {};
    std::lock_guard lock(registry().mutex);
    const auto& ranges = registry().ranges;
    const auto next = ranges.upper_bound(address);
    if (next == ranges.begin()) return {};
    const auto& range = std::prev(next)->second;
    if (!range->readable || address - range->address >= range->bytes || bytes > range->bytes - (address - range->address)) return {};
    return {range};
}

Lease GuestAllocationsAcquireSpan_nid_no_patch(std::uintptr_t address, std::size_t bytes) {
    if (address == 0 || bytes == 0 || bytes > std::numeric_limits<std::uintptr_t>::max() - address) return {};
    std::lock_guard lock(registry().mutex);
    const auto& ranges = registry().ranges;
    auto it = ranges.upper_bound(address);
    if (it == ranges.begin()) return {};
    --it;
    Lease result;
    const auto end = address + bytes;
    for (auto cursor = address; cursor < end; ++it) {
        if (it == ranges.end() || it->first > cursor) return {};
        const auto& range = it->second;
        if (!range->readable || cursor - range->address >= range->bytes) return {};
        result.push_back(range);
        cursor = range->address + range->bytes;
    }
    return result;
}

}
