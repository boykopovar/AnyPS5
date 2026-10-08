#include "prx/libc/include/GuestAllocations.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <limits>
#include <iterator>
#include <map>
#include <set>
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
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace GuestAllocations {
namespace {

struct Registry {
    std::mutex mutex;
    std::map<std::uint64_t, std::shared_ptr<const Range>> ranges;
    bool mainImageRegistered = false;
    std::set<std::uintptr_t> registeredImages;
};

Registry& registry() {
    static Registry value;
    return value;
}

std::atomic<std::uint64_t> generation{1};
std::atomic<void (*)(std::uintptr_t, std::size_t)> invalidator{nullptr};
std::atomic<bool (*)(std::uintptr_t, std::size_t)> pinWaiter{nullptr};

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

#ifndef _WIN32
std::vector<std::pair<std::uintptr_t, std::size_t>> fileBackedWritableImage() {
    const auto image = std::filesystem::read_symlink("/proc/self/exe").string();
    std::ifstream maps("/proc/self/maps");
    std::vector<std::pair<std::uintptr_t, std::size_t>> mappings;
    for (std::string line; std::getline(maps, line);) {
        std::istringstream fields(line);
        std::string span, permissions, offset, device, inode, path;
        fields >> span >> permissions >> offset >> device >> inode >> std::ws;
        std::getline(fields, path);
        if (permissions != "rw-p" || path != image) continue;
        const auto dash = span.find('-');
        const auto begin = std::stoull(span.substr(0, dash), nullptr, 16);
        mappings.emplace_back(begin, std::stoull(span.substr(dash + 1), nullptr, 16) - begin);
    }
    return mappings;
}

bool backWritableImageAnonymously() {
    for (const auto& [address, bytes] : fileBackedWritableImage()) {
        auto* image = reinterpret_cast<void*>(address);
        auto* copy = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (copy == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "mmap of an anonymous main image copy failed");
        std::memcpy(copy, image, bytes);
        if (mremap(copy, bytes, bytes, MREMAP_MAYMOVE | MREMAP_FIXED, image) == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "mremap of an anonymous main image copy failed");
    }
    return true;
}

[[maybe_unused]] const bool writableImageAnonymous = backWritableImageAnonymously();
#endif

}

void* GuestAllocationsBegin_nid_postfix() {
    return new MutationState{std::unique_lock<std::mutex>(registry().mutex), {}};
}

void GuestAllocationsEnd_nid_postfix(void* mutation) noexcept {
    auto* state = static_cast<MutationState*>(mutation);
    generation.fetch_add(1, std::memory_order_release);
    if (const auto callback = invalidator.load(std::memory_order_acquire)) {
        for (const auto& [address, bytes] : state->changed) callback(address, bytes);
    }
    delete state;
}

std::uint64_t GuestAllocationsGeneration_nid_postfix() {
    return generation.load(std::memory_order_acquire);
}

void GuestAllocationsSetInvalidator_nid_postfix(void (*callback)(std::uintptr_t, std::size_t)) {
    invalidator.store(callback, std::memory_order_release);
}

void GuestAllocationsInvalidate_nid_postfix(std::uintptr_t address, std::size_t bytes) {
    generation.fetch_add(1, std::memory_order_release);
    if (const auto callback = invalidator.load(std::memory_order_acquire)) callback(address, bytes);
}

void GuestAllocationsSetPinWaiter_nid_postfix(bool (*callback)(std::uintptr_t, std::size_t)) {
    pinWaiter.store(callback, std::memory_order_release);
}

#ifdef _WIN32
namespace {
void registerImage(const void* image) {
    auto& state = registry();
    if (state.registeredImages.contains(reinterpret_cast<std::uintptr_t>(image))) return;
    auto replacement = state.ranges;
    auto cursor = reinterpret_cast<std::uintptr_t>(image);
    bool registered = false;
    for (;;) {
        MEMORY_BASIC_INFORMATION memory{};
        require(VirtualQuery(reinterpret_cast<const void*>(cursor), &memory, sizeof(memory)) == sizeof(memory), "cannot query a guest image");
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
            registered = true;
        }
        cursor += memory.RegionSize;
    }
    require(registered, "guest image has no committed pages");
    state.ranges.swap(replacement);
    state.registeredImages.insert(reinterpret_cast<std::uintptr_t>(image));
}
}

void GuestAllocationsRegisterMainImage_nid_postfix(void*) {
    auto& state = registry();
    if (state.mainImageRegistered) return;
    const auto image = GetModuleHandleW(nullptr);
    require(image != nullptr, "cannot locate the main guest image");
    registerImage(image);
    state.mainImageRegistered = true;
}

void GuestAllocationsRegisterImage_nid_postfix(void*, const void* image) {
    require(image != nullptr, "cannot register a guest image without a base");
    registerImage(image);
}
#else
void GuestAllocationsRegisterMainImage_nid_postfix(void*) {
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
    std::snprintf(message, sizeof(message), "guest allocation 0x%llx+0x%llx is still pinned by GPU work or an import (%s)", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), why);
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
        if (state != nullptr && state->lock.owns_lock()) {
            state->lock.unlock();
            if (waiter != nullptr) progressed = waiter(address, bytes);
            else std::this_thread::yield();
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
    for (const auto& [base, range] : registry().ranges) {
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
    for (const auto& [base, range] : registry().ranges) {
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

std::map<std::uint64_t, std::shared_ptr<const Range>> replaceRange(const void* pointer, std::size_t bytes, bool remove, bool readable, bool writable) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest protection or unmap range");
    require(!writable || readable, "writable guest allocation must be readable");
    const auto end = address + bytes;
    auto replacement = registry().ranges;
    auto cursor = address;
    for (const auto& [base, entry] : registry().ranges) {
        const auto& range = *entry;
        const auto finish = base + range.bytes;
        if (finish <= address) continue;
        if (base >= end) break;
        require(base <= cursor, "guest protection or unmap range has a hole");
        replacement.erase(base);
        const auto insert = [&](std::uint64_t first, std::uint64_t last, bool canRead, bool canWrite) {
            if (first < last) replacement.emplace(first, std::make_shared<const Range>(Range{first, static_cast<std::size_t>(last - first), canRead, canWrite, range.allocationAddress, range.allocationBytes, range.releasable}));
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
    registry().ranges.swap(replacement);
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
        for (const auto& [base, entry] : replacement) {
            if (entry->allocationAddress == range.allocationAddress) last = false;
        }
        apply(reinterpret_cast<const void*>(cursor), pieceEnd - cursor, reinterpret_cast<const void*>(range.allocationAddress), last);
        registry().ranges.swap(replacement);
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

Lease GuestAllocationsAcquireAll_nid_postfix() {
    std::lock_guard lock(registry().mutex);
    Lease result;
    result.reserve(registry().ranges.size());
    for (const auto& [address, range] : registry().ranges) {
        if (range->bytes != 0) result.push_back(range);
    }
    return result;
}

}
