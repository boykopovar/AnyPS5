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

struct Registry {
    std::mutex mutex;
    std::map<std::uint64_t, std::shared_ptr<const Range>> ranges;
    bool mainImageRegistered = false;
    std::map<std::uintptr_t, std::uintptr_t> registeredImages;
    std::shared_ptr<const Lease::Ranges> readableLease;
    std::shared_ptr<const Lease::Ranges> allLease;
    std::uint64_t backingSerials = 0;
    std::vector<std::pair<std::uintptr_t, std::uintptr_t>> retiring;
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

std::shared_ptr<const Range> lookupLocked(std::uintptr_t address, bool next) {
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

bool isRetiring(const Range& range) {
    for (const auto& [begin, end] : registry().retiring) {
        if (range.address < end && begin < range.address + range.bytes) return true;
    }
    return false;
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

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
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
                if (range->gpu && range->HostReadable() && range->bytes != 0 && range->address + range->bytes > address) mapped.push_back(range);
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

#ifdef __APPLE__
void addSegmentPages(ImagePages& collected, std::uintptr_t address, std::uint64_t bytes, bool readable, bool writable) {
    const auto start = address & ~(collected.pageSize - 1);
    const auto end = (address + bytes + collected.pageSize - 1) & ~(collected.pageSize - 1);
    for (auto page = start; page < end; page += collected.pageSize) {
        auto& entry = collected.pages[page];
        entry.readable = entry.readable || readable;
        entry.writable = entry.writable || writable;
    }
}

void collectMachImagePages(ImagePages& collected) {
    const auto count = _dyld_image_count();
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto* header = reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(index));
        if (header == nullptr || header->magic != MH_MAGIC_64) continue;
        if (collected.main ? header->filetype != MH_EXECUTE : reinterpret_cast<std::uintptr_t>(header) != collected.base) continue;
        collected.found = true;
        const auto slide = static_cast<std::uintptr_t>(_dyld_get_image_vmaddr_slide(index));
        const auto* command = reinterpret_cast<const load_command*>(header + 1);
        for (std::uint32_t loaded = 0; loaded < header->ncmds; ++loaded) {
            if (command->cmd == LC_SEGMENT_64) {
                const auto& segment = *reinterpret_cast<const segment_command_64*>(command);
                if (segment.vmsize != 0 && segment.initprot != VM_PROT_NONE)
                    addSegmentPages(collected, slide + segment.vmaddr, segment.vmsize, (segment.initprot & (VM_PROT_READ | VM_PROT_WRITE)) != 0, (segment.initprot & VM_PROT_WRITE) != 0);
            }
            command = reinterpret_cast<const load_command*>(reinterpret_cast<const std::uint8_t*>(command) + command->cmdsize);
        }
        return;
    }
}
#else
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
#endif

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
#ifdef __APPLE__
    collectMachImagePages(collected);
#else
    dl_iterate_phdr(collectImagePages, &collected);
#endif
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
#ifdef __APPLE__
    collectMachImagePages(collected);
#else
    dl_iterate_phdr(collectImagePages, &collected);
#endif
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

void GuestAllocationsAddGpu_nid_no_patch(void* mutation, void* pointer, std::size_t bytes, bool readable, bool writable, bool gpu, bool gpuWritable) {
    recordChange(mutation, pointer, bytes);
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(address != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest allocation range");
    require(!writable || readable, "writable guest allocation must be readable");
    require(!gpuWritable || gpu, "gpu-writable guest allocation must be gpu-accessible");
    auto& ranges = registry().ranges;
    const auto next = ranges.lower_bound(address);
    require(next == ranges.end() || (next->first != address && address + bytes <= next->first), "overlapping guest allocation");
    if (next != ranges.begin()) {
        const auto& previous = *std::prev(next)->second;
        require(previous.address + previous.bytes <= address, "overlapping guest allocation");
    }
    ranges.emplace(address, std::make_shared<const Range>(Range{address, bytes, readable, writable, address, bytes, true, gpu, freshBacking(), gpuWritable}));
}

void GuestAllocationsAdd_nid_postfix(void* mutation, void* pointer, std::size_t bytes, bool readable, bool writable, bool gpu) {
    GuestAllocationsAddGpu_nid_no_patch(mutation, pointer, bytes, readable, writable, gpu, gpu && writable);
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
    bool marked = false;
    const auto unmark = [&] {
        if (!marked) return;
        if (!state->lock.owns_lock()) state->lock.lock();
        auto& retiring = registry().retiring;
        retiring.erase(std::find(retiring.begin(), retiring.end(), std::pair<std::uintptr_t, std::uintptr_t>{address, end}));
        registry().readableLease.reset();
        registry().allLease.reset();
        marked = false;
    };
    struct Unmark {
        const decltype(unmark)& release;
        ~Unmark() { release(); }
    } unmarkOnExit{unmark};
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
            unmark();
            if (state != nullptr && bytes != 0) state->unpinned.emplace_back(address, bytes);
            return;
        }
        if (!marked && state != nullptr && state->lock.owns_lock()) {
            registry().retiring.emplace_back(address, end);
            marked = true;
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

}
