#include "prx/libc/include/GuestAllocations.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <limits>
#include <iterator>
#include <map>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#else
#include <link.h>
#endif
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#endif

namespace GuestAllocations {
namespace {
namespace {
namespace {
decltype(registry().ranges)::iterator firstRangeTouching(std::uintptr_t address) {
    auto& ranges = registry().ranges;
    auto it = ranges.lower_bound(address);
    if (it == ranges.begin()) return it;
    const auto& previous = *std::prev(it);
    if (previous.second->bytes != 0 && address < previous.first + previous.second->bytes) return std::prev(it);
    return it;
}

void GuestAllocationsRequireAvailable_nid_postfix(void*, const void* pointer, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(address != 0 && bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid fixed guest mapping");
    const auto first = firstRangeTouching(address);
    for (auto it = first; it != registry().ranges.end(); ++it) {
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
    const auto first = firstRangeTouching(address);
    for (auto it = first; it != registry().ranges.end(); ++it) {
        const auto& [base, range] = *it;
        if (base >= address + bytes) break;
        if (base + range->bytes > address) return true;
    }
    return false;
}

bool GuestAllocationsForRegistered_nid_postfix(void*, const void* pointer, std::size_t bytes, const std::function<void(const void*, std::size_t)>& visit) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(address != 0 && bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest allocation range");
    const auto end = address + bytes;
    const auto& ranges = registry().ranges;
    auto it = ranges.upper_bound(address);
    if (it != ranges.begin()) --it;
    std::uintptr_t first = 0;
    std::uintptr_t last = 0;
    bool any = false;
    for (; it != ranges.end() && it->first < end; ++it) {
        const auto base = std::max<std::uintptr_t>(it->first, address);
        const auto finish = std::min<std::uintptr_t>(it->first + it->second->bytes, end);
        if (finish <= base) continue;
        if (any && base == last) {
            last = finish;
            continue;
        }
        if (any) visit(reinterpret_cast<const void*>(first), last - first);
        first = base;
        last = finish;
        any = true;
    }
    if (any) visit(reinterpret_cast<const void*>(first), last - first);
    return any;
}

void GuestAllocationsGuestPages_nid_postfix(std::uintptr_t address, std::size_t bytes, std::size_t page, std::uintptr_t* first, std::uintptr_t* end) {
    std::uintptr_t bias = 0;
#ifdef __APPLE__
    struct Search {
        std::uintptr_t address;
        std::uintptr_t hostPage;
        std::uintptr_t bias;
    } search{address, static_cast<std::uintptr_t>(::sysconf(_SC_PAGESIZE)), 0};
    dl_iterate_phdr([](dl_phdr_info* image, std::size_t, void* data) {
        auto& found = *static_cast<Search*>(data);
        for (unsigned index = 0; index < image->dlpi_phnum; ++index) {
            const auto& header = image->dlpi_phdr[index];
            const auto segment = static_cast<std::uintptr_t>(image->dlpi_addr + header.p_vaddr);
            const auto start = segment & ~(found.hostPage - 1);
            const auto finish = (segment + header.p_memsz + found.hostPage - 1) & ~(found.hostPage - 1);
            if (header.p_type == PT_LOAD && found.address >= start && found.address < finish) {
                found.bias = static_cast<std::uintptr_t>(image->dlpi_addr);
                return 1;
            }
        }
        return 0;
    }, &search);
    bias = search.bias;
#endif
    const auto mask = static_cast<std::uintptr_t>(page - 1);
    *first = bias + ((address - bias) & ~mask);
    *end = bias + ((address + bytes - bias + mask) & ~mask);
}

void GuestAllocationsTrimToRegistered_nid_postfix(void*, std::size_t page, const void** pointer, std::size_t* bytes) {
#ifdef __APPLE__
    auto start = reinterpret_cast<std::uintptr_t>(*pointer);
    const auto end = start + *bytes;
    const auto& ranges = registry().ranges;
    const auto following = ranges.upper_bound(start);
    const bool startCovered = following != ranges.begin() && std::prev(following)->first + std::prev(following)->second->bytes > start;
    if (!startCovered && following != ranges.end() && following->first < end && following->first - start < page) start = following->first;
    *pointer = reinterpret_cast<const void*>(start);
    *bytes = end - start;
#else
    static_cast<void>(page);
    static_cast<void>(pointer);
    static_cast<void>(bytes);
#endif
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
        const auto base = it->first;
        const auto finish = base + it->second->bytes;
        if (finish <= cursor) continue;
        if (base > cursor || !it->second->releasable) return false;
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
        return {address, range.allocationBytes, range.readable, range.writable, address, range.allocationBytes, range.releasable, range.gpu, range.backingSerial, range.gpuWritable};
    }
    for (const auto& [base, range] : registry().ranges) {
        if (range->allocationAddress == address) {
            require(range->releasable, "guest image memory is not a releasable allocation");
            return {address, range->allocationBytes, range->readable, range->writable, address, range->allocationBytes, range->releasable, range->gpu, range->backingSerial, range->gpuWritable};
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
    } else {
        std::erase_if(ranges, [&](const auto& entry) { return entry.second->allocationAddress == range.address; });
    }
    if (const auto observer = unmapObserver.load(std::memory_order_acquire)) observer(range.address, range.bytes);
}

namespace {

struct RangePatch {
    std::vector<std::uint64_t> erased;
    std::vector<std::pair<std::uint64_t, std::shared_ptr<const Range>>> inserted;
};

RangePatch replaceRange(const void* pointer, std::size_t bytes, bool remove, bool readable, bool writable, bool gpu, bool gpuWritable) {
    const std::uint64_t address = reinterpret_cast<std::uintptr_t>(pointer);
    require(bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest protection or unmap range");
    require(!writable || readable, "writable guest allocation must be readable");
    require(!gpuWritable || gpu, "gpu-writable guest allocation must be gpu-accessible");
    const auto end = address + bytes;
    RangePatch patch;
    auto cursor = address;
    const auto& ranges = registry().ranges;
    auto it = ranges.upper_bound(address);
    if (it != ranges.begin()) --it;
    for (; it != ranges.end(); ++it) {
        const auto base = it->first;
        const auto& range = *it->second;
        const auto finish = base + range.bytes;
        if (finish <= address) continue;
        if (base >= end) break;
        require(base <= cursor, "guest protection or unmap range has a hole");
        patch.erased.push_back(base);
        const auto insert = [&](std::uint64_t first, std::uint64_t last, bool canRead, bool canWrite, bool gpuMapped, bool canGpuWrite, std::uint64_t backing) {
            if (first < last) patch.inserted.emplace_back(first, std::make_shared<const Range>(Range{first, static_cast<std::size_t>(last - first), canRead, canWrite, range.allocationAddress, range.allocationBytes, range.releasable, gpuMapped, backing, canGpuWrite}));
        };
        insert(base, std::max<std::uint64_t>(base, address), range.readable, range.writable, range.gpu, range.gpuWritable, range.backingSerial);
        if (!remove) insert(std::max<std::uint64_t>(base, address), std::min<std::uint64_t>(finish, end), readable, writable, gpu, gpuWritable, freshBacking());
        insert(std::min<std::uint64_t>(finish, end), finish, range.readable, range.writable, range.gpu, range.gpuWritable, range.backingSerial);
        cursor = std::min<std::uint64_t>(finish, end);
    }
    require(cursor == end, "guest protection or unmap range is not registered");
    return patch;
}

struct StagedPatch {
    using Ranges = decltype(Registry::ranges);
    const RangePatch& patch;
    Ranges staged;
    explicit StagedPatch(const RangePatch& source) : patch(source) {
        for (const auto& [key, range] : patch.inserted) staged.emplace(key, range);
    }
    void apply() noexcept {
        auto& ranges = registry().ranges;
        for (const auto key : patch.erased) ranges.erase(key);
        ranges.merge(staged);
    }
};

bool allocationRemains(const Range& range, const RangePatch& patch) {
    for (const auto& [key, entry] : patch.inserted) {
        if (entry->allocationAddress == range.allocationAddress) return true;
    }
    const auto& ranges = registry().ranges;
    const auto limit = range.allocationAddress + range.allocationBytes;
    for (auto it = ranges.lower_bound(range.allocationAddress); it != ranges.end() && it->first < limit; ++it) {
        if (it->second->allocationAddress != range.allocationAddress) continue;
        if (std::find(patch.erased.begin(), patch.erased.end(), it->first) == patch.erased.end()) return true;
    }
    return false;
}

}

void GuestAllocationsProtectGpu_nid_no_patch(void* mutation, const void* pointer, std::size_t bytes, bool readable, bool writable, bool gpu, bool gpuWritable, const std::function<void()>& apply) {
    GuestAllocationsRequireUnpinned_nid_postfix(mutation, pointer, bytes);
    recordChange(mutation, pointer, bytes);
    const auto patch = replaceRange(pointer, bytes, false, readable, writable, gpu, gpuWritable);
    StagedPatch staged(patch);
    apply();
    staged.apply();
}

void GuestAllocationsProtect_nid_postfix(void* mutation, const void* pointer, std::size_t bytes, bool readable, bool writable, bool gpu, const std::function<void()>& apply) {
    GuestAllocationsProtectGpu_nid_no_patch(mutation, pointer, bytes, readable, writable, gpu, gpu && writable, apply);
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
            if (cursor < candidate.address + candidate.bytes) containing = &candidate;
        }
        if (containing == nullptr) {
            if (found == registry().ranges.end() || found->first >= end) break;
            cursor = found->first;
            continue;
        }
        const auto range = *containing;
        require(range.releasable, "guest image memory cannot be unmapped");
        const auto pieceEnd = std::min<std::uint64_t>(end, range.address + range.bytes);
        const auto patch = replaceRange(reinterpret_cast<const void*>(cursor), pieceEnd - cursor, true, false, false, false, false);
        const bool last = !allocationRemains(range, patch);
        StagedPatch staged(patch);
        apply(reinterpret_cast<const void*>(cursor), pieceEnd - cursor, reinterpret_cast<const void*>(range.allocationAddress), last);
        staged.apply();
        if (const auto observer = unmapObserver.load(std::memory_order_acquire)) observer(cursor, pieceEnd - cursor);
        any = true;
        cursor = pieceEnd;
    }
    require(any, "unmap address is not registered");
    if (const auto observer = unmapObserver.load(std::memory_order_acquire)) observer(reinterpret_cast<std::uintptr_t>(pointer), bytes);
}

void GuestAllocationsSetUnmapObserver_nid_postfix(void (*callback)(std::uintptr_t, std::size_t)) {
    unmapObserver.store(callback, std::memory_order_release);
}

Lease GuestAllocationsAcquire_nid_postfix() {
    std::lock_guard lock(registry().mutex);
    if (auto shared = registry().readableLease) return Lease(std::move(shared));
    auto ranges = std::make_shared<Lease::Ranges>();
    ranges->reserve(registry().ranges.size());
    for (const auto& [address, range] : registry().ranges) {
        if (range->HostReadable() && range->bytes != 0 && !isRetiring(*range)) ranges->push_back(range);
    }
    if (registry().retiring.empty()) registry().readableLease = ranges;
    return Lease(std::move(ranges));
}

Lease GuestAllocationsAcquireMapped_nid_no_patch(const Mapped& mapped) {
    std::lock_guard lock(registry().mutex);
    auto& registered = registry().ranges;
    std::vector<decltype(registered.begin())> targets;
    targets.reserve(mapped.size());
    for (const auto& weak : mapped) {
        const auto range = weak.lock();
        if (range == nullptr || !range->HostReadable() || range->bytes == 0) continue;
        const auto found = registered.find(range->address);
        if (found != registered.end() && found->second == range) targets.push_back(found);
    }
    std::sort(targets.begin(), targets.end(), [](const auto& a, const auto& b) { return a->first < b->first; });
    auto ranges = std::make_shared<Lease::Ranges>();
    const auto continues = [](const Range& before, const Range& after) {
        return before.address + before.bytes == after.address && before.readable == after.readable && before.writable == after.writable && before.gpu == after.gpu && before.HostWritable() == after.HostWritable();
    };
    for (const auto target : targets) {
        if (!ranges->empty() && target->first < ranges->back()->address + ranges->back()->bytes) continue;
        auto first = target;
        while (first != registered.begin()) {
            auto previous = std::prev(first);
            while (previous != registered.begin() && previous->second->bytes == 0) --previous;
            if (previous->second->bytes == 0 || !continues(*previous->second, *first->second)) break;
            first = previous;
        }
        for (auto current = first;;) {
            ranges->push_back(current->second);
            auto next = std::next(current);
            while (next != registered.end() && next->second->bytes == 0) ++next;
            if (next == registered.end() || !continues(*current->second, *next->second)) break;
            current = next;
        }
    }
    return Lease(std::move(ranges));
}

Lease GuestAllocationsAcquireRegistered_nid_postfix() {
    std::lock_guard lock(registry().mutex);
    if (registry().retiring.empty()) {
        if (auto shared = registry().readableLease) return Lease(std::move(shared));
    }
    auto ranges = std::make_shared<Lease::Ranges>();
    for (const auto& [address, range] : registry().ranges) {
        if (range->HostReadable() && range->bytes != 0) ranges->push_back(range);
    }
    if (registry().retiring.empty()) registry().readableLease = ranges;
    return Lease(std::move(ranges));
}

void GuestAllocationsLookupEach_nid_postfix(const std::uintptr_t* addresses, std::size_t count, std::shared_ptr<const Range>* ranges) {
    std::lock_guard lock(registry().mutex);
    for (std::size_t index = 0; index < count; ++index) ranges[index] = lookupLocked(addresses[index], false);
}

std::shared_ptr<const Range> GuestAllocationsLookup_nid_postfix(std::uintptr_t address, bool next) {
    std::lock_guard lock(registry().mutex);
    return lookupLocked(address, next);
}

Lease GuestAllocationsAcquireAll_nid_postfix() {
    std::lock_guard lock(registry().mutex);
    if (auto shared = registry().allLease) return Lease(std::move(shared));
    auto ranges = std::make_shared<Lease::Ranges>();
    ranges->reserve(registry().ranges.size());
    for (const auto& [address, range] : registry().ranges) {
        if (range->bytes != 0 && !isRetiring(*range)) ranges->push_back(range);
    }
    if (registry().retiring.empty()) registry().allLease = ranges;
    return Lease(std::move(ranges));
}

}
}
}
}
