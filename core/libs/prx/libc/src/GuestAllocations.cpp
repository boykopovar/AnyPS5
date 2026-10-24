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
    std::map<std::uintptr_t, std::uintptr_t> registeredImages;
    std::shared_ptr<const Lease::Ranges> readableLease;
    std::shared_ptr<const Lease::Ranges> allLease;
    std::uint64_t backingSerials = 0;
};

Registry& registry() {
    static auto* value = new Registry;
    return *value;
}

std::uint64_t freshBacking() {
    return ++registry().backingSerials;
}

void renewBacking(std::uintptr_t address, std::size_t bytes) {
    auto& ranges = registry().ranges;
    auto it = ranges.upper_bound(address);
    if (it != ranges.begin()) --it;
    for (; it != ranges.end() && it->first < address + bytes; ++it) {
        if (it->first + it->second->bytes <= address) continue;
        auto renewed = *it->second;
        renewed.backingSerial = freshBacking();
        it->second = std::make_shared<const Range>(renewed);
    }
}

std::atomic<std::uint64_t> generation{1};
std::atomic<void (*)(std::uintptr_t, std::size_t)> invalidator{nullptr};
std::atomic<bool (*)(std::uintptr_t, std::size_t)> pinWaiter{nullptr};
std::atomic<void (*)(const Mapped&, std::uint64_t)> gpuMapObserver{nullptr};
std::atomic<void (*)(std::uintptr_t, std::size_t)> unmapObserver{nullptr};
std::atomic<std::uint32_t> gpuMapObserverCalls{0};

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
    std::vector<std::pair<std::uintptr_t, std::size_t>> gpuMapped;
    std::vector<std::pair<std::uintptr_t, std::size_t>> unpinned;
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

#ifdef ANYPS5_ENABLE_ASAN
[[gnu::no_sanitize("address")]] void copyImagePages(void* target, const void* source, std::size_t bytes) {
    auto* to = static_cast<volatile std::uintptr_t*>(target);
    const auto* from = static_cast<const volatile std::uintptr_t*>(source);
    for (std::size_t word = 0; word < bytes / sizeof(std::uintptr_t); ++word) to[word] = from[word];
}
#endif

bool backWritableImageAnonymously() {
    for (const auto& [address, bytes] : fileBackedWritableImage()) {
        auto* image = reinterpret_cast<void*>(address);
        auto* copy = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (copy == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "mmap of an anonymous main image copy failed");
#ifdef ANYPS5_ENABLE_ASAN
        copyImagePages(copy, image, bytes);
#else
        std::memcpy(copy, image, bytes);
#endif
        if (mremap(copy, bytes, bytes, MREMAP_MAYMOVE | MREMAP_FIXED, image) == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "mremap of an anonymous main image copy failed");
    }
    return true;
}

[[maybe_unused]] const bool writableImageAnonymous = backWritableImageAnonymously();
#endif

}

void* GuestAllocationsBegin_nid_postfix() {
    auto* state = new MutationState{std::unique_lock<std::mutex>(registry().mutex), {}};
    registry().readableLease.reset();
    registry().allLease.reset();
    return state;
}

void GuestAllocationsEnd_nid_postfix(void* mutation) noexcept {
    auto* state = static_cast<MutationState*>(mutation);
    for (const auto& [address, bytes] : state->unpinned) renewBacking(address, bytes);
    const auto ended = generation.fetch_add(1, std::memory_order_release) + 1;
    if (const auto callback = invalidator.load(std::memory_order_acquire)) {
        for (const auto& [address, bytes] : state->changed) callback(address, bytes);
    }
    if (!state->gpuMapped.empty()) gpuMapObserverCalls.fetch_add(1);
    const auto observer = state->gpuMapped.empty() ? nullptr : gpuMapObserver.load();
    Lease::Ranges mapped;
    if (observer != nullptr) {
        const auto& ranges = registry().ranges;
        for (const auto& [address, bytes] : state->gpuMapped) {
            auto it = ranges.upper_bound(address);
            if (it != ranges.begin()) --it;
            for (; it != ranges.end() && it->first < address + bytes; ++it) {
                const auto& range = it->second;
                if (range->gpu && range->readable && range->bytes != 0 && range->address + range->bytes > address) mapped.push_back(range);
            }
        }
        std::sort(mapped.begin(), mapped.end(), [](const auto& left, const auto& right) { return left->address < right->address; });
        mapped.erase(std::unique(mapped.begin(), mapped.end()), mapped.end());
    }
    const Mapped weak(mapped.begin(), mapped.end());
    mapped.clear();
    const bool noted = !state->gpuMapped.empty();
    registry().readableLease.reset();
    registry().allLease.reset();
    delete state;
    if (!weak.empty()) observer(weak, ended);
    if (noted) gpuMapObserverCalls.fetch_sub(1);
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

void GuestAllocationsSetGpuMapObserver_nid_postfix(void (*callback)(const Mapped& ranges, std::uint64_t generation)) {
    gpuMapObserver.store(callback);
    while (gpuMapObserverCalls.load() != 0) std::this_thread::yield();
}

void GuestAllocationsNoteGpuMapping_nid_postfix(void* mutation, const void* pointer, std::size_t bytes) {
    if (mutation != nullptr && bytes != 0) static_cast<MutationState*>(mutation)->gpuMapped.emplace_back(reinterpret_cast<std::uintptr_t>(pointer), bytes);
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
            replacement.emplace(cursor, std::make_shared<const Range>(Range{cursor, memory.RegionSize, readable, writable, cursor, memory.RegionSize, false, false, freshBacking()}));
            registered = true;
        }
        cursor += memory.RegionSize;
    }
    require(registered, "guest image has no committed pages");
    state.ranges.swap(replacement);
    state.registeredImages.emplace(reinterpret_cast<std::uintptr_t>(image), cursor);
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
namespace {
struct ImagePages {
    struct Page {
        bool readable = false;
        bool writable = false;
    };
    std::uintptr_t base = 0;
    bool main = false;
    bool found = false;
    std::uint64_t pageSize = 0;
    std::map<std::uint64_t, Page> pages;
};

int collectImagePages(dl_phdr_info* image, std::size_t, void* data) {
    auto& collected = *static_cast<ImagePages*>(data);
    if (!collected.main && image->dlpi_addr != collected.base) return 0;
    collected.found = true;
    for (int index = 0; index < image->dlpi_phnum; ++index) {
        const auto& header = image->dlpi_phdr[index];
        if (header.p_type != PT_LOAD || header.p_memsz == 0) continue;
        const auto start = (image->dlpi_addr + header.p_vaddr) & ~(collected.pageSize - 1);
        const auto end = (image->dlpi_addr + header.p_vaddr + header.p_memsz + collected.pageSize - 1) & ~(collected.pageSize - 1);
        for (auto page = start; page < end; page += collected.pageSize) {
            auto& entry = collected.pages[page];
            entry.readable = entry.readable || (header.p_flags & (PF_R | PF_W)) != 0;
            entry.writable = entry.writable || (header.p_flags & PF_W) != 0;
        }
    }
    return 1;
}

std::uintptr_t registerImagePages(const ImagePages& collected) {
    auto& state = registry();
    const auto pageSize = collected.pageSize;
    const auto& pages = collected.pages;
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
        replacement.emplace(address, std::make_shared<const Range>(Range{address, bytes, page->second.readable, page->second.writable, address, bytes, false, false, freshBacking()}));
        page = std::next(last);
    }
    state.ranges.swap(replacement);
    return static_cast<std::uintptr_t>(std::prev(pages.end())->first + pageSize);
}
}

void GuestAllocationsRegisterMainImage_nid_postfix(void*) {
    auto& state = registry();
    if (state.mainImageRegistered) return;
    ImagePages collected{.main = true, .pageSize = static_cast<std::uint64_t>(::sysconf(_SC_PAGESIZE))};
    dl_iterate_phdr(collectImagePages, &collected);
    require(!collected.pages.empty(), "main guest image has no loadable segments");
    registerImagePages(collected);
    state.mainImageRegistered = true;
}

void GuestAllocationsRegisterImage_nid_postfix(void*, const void* image) {
    require(image != nullptr, "cannot register a guest image without a base");
    auto& state = registry();
    const auto base = reinterpret_cast<std::uintptr_t>(image);
    if (state.registeredImages.contains(base)) return;
    ImagePages collected{.base = base, .pageSize = static_cast<std::uint64_t>(::sysconf(_SC_PAGESIZE))};
    dl_iterate_phdr(collectImagePages, &collected);
    require(collected.found, "cannot locate a guest image");
    require(!collected.pages.empty(), "guest image has no loadable segments");
    require(collected.pages.begin()->first == base, "guest image does not start at its base");
    state.registeredImages.emplace(base, registerImagePages(collected));
}
#endif

void GuestAllocationsUnregisterImage_nid_postfix(void* mutation, const void* image) {
    auto& state = registry();
    const auto found = state.registeredImages.find(reinterpret_cast<std::uintptr_t>(image));
    if (found == state.registeredImages.end()) return;
    const auto [begin, end] = *found;
    GuestAllocationsRequireUnpinned_nid_postfix(mutation, image, end - begin);
    recordChange(mutation, image, end - begin);
    std::erase_if(state.ranges, [&](const auto& entry) { return !entry.second->releasable && entry.second->allocationAddress >= begin && entry.second->allocationAddress < end; });
    state.registeredImages.erase(found);
}

void GuestAllocationsAdd_nid_postfix(void* mutation, void* pointer, std::size_t bytes, bool readable, bool writable, bool gpu) {
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
    ranges.emplace(address, std::make_shared<const Range>(Range{address, bytes, readable, writable, address, bytes, true, gpu, freshBacking()}));
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
        registry().readableLease.reset();
        registry().allLease.reset();
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
        if (!pinned) {
            if (state != nullptr && bytes != 0) state->unpinned.emplace_back(address, bytes);
            return;
        }
        if (std::chrono::steady_clock::now() >= deadline) PinnedFailure(address, bytes, ("waited " + std::to_string(pinWait().count()) + " ms for the lease (APS5_PIN_WAIT_MS)").c_str());
        const auto waiter = pinWaiter.load(std::memory_order_acquire);
        bool progressed = false;
        if (state != nullptr && state->lock.owns_lock()) {
            state->lock.unlock();
            try {
                if (waiter != nullptr) progressed = waiter(address, bytes);
                else std::this_thread::yield();
            } catch (...) {
                state->lock.lock();
                throw;
            }
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
        return {address, range.allocationBytes, range.readable, range.writable, address, range.allocationBytes, range.releasable, range.gpu};
    }
    for (const auto& [base, range] : registry().ranges) {
        if (range->allocationAddress == address) {
            require(range->releasable, "guest image memory is not a releasable allocation");
            return {address, range->allocationBytes, range->readable, range->writable, address, range->allocationBytes, range->releasable, range->gpu};
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

RangePatch replaceRange(const void* pointer, std::size_t bytes, bool remove, bool readable, bool writable, bool gpu) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest protection or unmap range");
    require(!writable || readable, "writable guest allocation must be readable");
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
        const auto insert = [&](std::uint64_t first, std::uint64_t last, bool canRead, bool canWrite, bool gpuMapped, std::uint64_t backing) {
            if (first < last) patch.inserted.emplace_back(first, std::make_shared<const Range>(Range{first, static_cast<std::size_t>(last - first), canRead, canWrite, range.allocationAddress, range.allocationBytes, range.releasable, gpuMapped, backing}));
        };
        insert(base, std::max<std::uint64_t>(base, address), range.readable, range.writable, range.gpu, range.backingSerial);
        if (!remove) insert(std::max<std::uint64_t>(base, address), std::min<std::uint64_t>(finish, end), readable, writable, gpu, freshBacking());
        insert(std::min<std::uint64_t>(finish, end), finish, range.readable, range.writable, range.gpu, range.backingSerial);
        cursor = std::min<std::uint64_t>(finish, end);
    }
    require(cursor == end, "guest protection or unmap range is not registered");
    return patch;
}

void applyPatch(const RangePatch& patch) {
    auto& ranges = registry().ranges;
    for (const auto key : patch.erased) ranges.erase(key);
    for (const auto& [key, range] : patch.inserted) ranges.emplace(key, range);
}

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

void GuestAllocationsProtect_nid_postfix(void* mutation, const void* pointer, std::size_t bytes, bool readable, bool writable, bool gpu, const std::function<void()>& apply) {
    GuestAllocationsRequireUnpinned_nid_postfix(mutation, pointer, bytes);
    recordChange(mutation, pointer, bytes);
    const auto patch = replaceRange(pointer, bytes, false, readable, writable, gpu);
    apply();
    applyPatch(patch);
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
        const auto patch = replaceRange(reinterpret_cast<const void*>(cursor), pieceEnd - cursor, true, false, false, false);
        const bool last = !allocationRemains(range, patch);
        apply(reinterpret_cast<const void*>(cursor), pieceEnd - cursor, reinterpret_cast<const void*>(range.allocationAddress), last);
        applyPatch(patch);
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
    for (const auto& [address, range] : registry().ranges) {
        if (range->readable && range->bytes != 0) ranges->push_back(range);
    }
    registry().readableLease = ranges;
    return Lease(std::move(ranges));
}

Lease GuestAllocationsAcquireMapped_nid_no_patch(const Mapped& mapped) {
    std::lock_guard lock(registry().mutex);
    auto& registered = registry().ranges;
    std::vector<decltype(registered.begin())> targets;
    targets.reserve(mapped.size());
    for (const auto& weak : mapped) {
        const auto range = weak.lock();
        if (range == nullptr || !range->readable || range->bytes == 0) continue;
        const auto found = registered.find(range->address);
        if (found != registered.end() && found->second == range) targets.push_back(found);
    }
    std::sort(targets.begin(), targets.end(), [](const auto& a, const auto& b) { return a->first < b->first; });
    auto ranges = std::make_shared<Lease::Ranges>();
    const auto continues = [](const Range& before, const Range& after) {
        return before.address + before.bytes == after.address && before.readable && after.readable && before.writable == after.writable && before.gpu == after.gpu;
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

std::shared_ptr<const Range> GuestAllocationsLookup_nid_postfix(std::uintptr_t address, bool next) {
    std::lock_guard lock(registry().mutex);
    const auto& ranges = registry().ranges;
    auto it = ranges.upper_bound(address);
    if (it != ranges.begin()) {
        const auto& previous = std::prev(it)->second;
        if (previous->bytes != 0 && address < previous->address + previous->bytes) return previous;
    }
    if (next) {
        for (; it != ranges.end(); ++it) {
            if (it->second->bytes != 0) return it->second;
        }
    }
    return nullptr;
}

Lease GuestAllocationsAcquireAll_nid_postfix() {
    std::lock_guard lock(registry().mutex);
    if (auto shared = registry().allLease) return Lease(std::move(shared));
    auto ranges = std::make_shared<Lease::Ranges>();
    ranges->reserve(registry().ranges.size());
    for (const auto& [address, range] : registry().ranges) {
        if (range->bytes != 0) ranges->push_back(range);
    }
    registry().allLease = ranges;
    return Lease(std::move(ranges));
}

}
