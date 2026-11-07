#include "prx/libc/include/GuestStacks.hpp"

#ifdef _WIN32
#include "prx/libc/include/GuestArena.hpp"
#include <atomic>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <windows.h>
#endif

namespace GuestStacks {
namespace {

#ifdef _WIN32
struct StackBounds {
    std::uintptr_t base;
    std::uintptr_t limit;
    std::uintptr_t deallocation;
};

constexpr std::size_t DeallocationStackOffset = 0x1478;

std::mutex rangesLock;
std::map<std::uintptr_t, std::uintptr_t> ranges;

StackBounds CurrentBounds() {
    auto* tib = reinterpret_cast<NT_TIB*>(NtCurrentTeb());
    auto* teb = reinterpret_cast<std::uint8_t*>(tib);
    return {reinterpret_cast<std::uintptr_t>(tib->StackBase), reinterpret_cast<std::uintptr_t>(tib->StackLimit),
            *reinterpret_cast<std::uintptr_t*>(teb + DeallocationStackOffset)};
}

void ApplyBounds(const StackBounds& bounds) {
    auto* tib = reinterpret_cast<NT_TIB*>(NtCurrentTeb());
    auto* teb = reinterpret_cast<std::uint8_t*>(tib);
    tib->StackBase = reinterpret_cast<void*>(bounds.base);
    tib->StackLimit = reinterpret_cast<void*>(bounds.limit);
    *reinterpret_cast<std::uintptr_t*>(teb + DeallocationStackOffset) = bounds.deallocation;
}

bool RegisteredBounds(std::uintptr_t stackPointer, StackBounds& bounds) {
    std::lock_guard lock(rangesLock);
    const auto next = ranges.upper_bound(stackPointer);
    if (next == ranges.begin()) return false;
    const auto& [low, high] = *std::prev(next);
    if (stackPointer >= high) return false;
    bounds = {high, low, low};
    return true;
}

bool HostBounds(std::uintptr_t stackPointer, StackBounds& bounds) {
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(reinterpret_cast<const void*>(stackPointer), &memory, sizeof(memory)) != sizeof(memory) || memory.State != MEM_COMMIT) return false;
    const auto base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
    bounds = {base + memory.RegionSize, base, reinterpret_cast<std::uintptr_t>(memory.AllocationBase)};
    return true;
}
#endif

}

void GuestStackRegister_nid_no_patch(const void* pointer, std::size_t bytes) {
#ifdef _WIN32
    const auto low = reinterpret_cast<std::uintptr_t>(pointer);
    const auto high = low + bytes;
    std::lock_guard lock(rangesLock);
    const auto same = ranges.find(low);
    if (same != ranges.end() && same->second == high) return;
    auto overlap = ranges.upper_bound(low);
    if (overlap != ranges.begin() && std::prev(overlap)->second > low) --overlap;
    while (overlap != ranges.end() && overlap->first < high) {
        GuestArena::GuestArenaUnpinWritable_nid_postfix(reinterpret_cast<const void*>(overlap->first), overlap->second - overlap->first);
        overlap = ranges.erase(overlap);
    }
    GuestArena::GuestArenaPinWritable_nid_postfix(pointer, bytes);
    ranges.emplace(low, high);
#else
    (void)pointer;
    (void)bytes;
#endif
}

void APS5_VABI GuestStackSwitch_nid_no_patch(std::uintptr_t stackPointer) {
#ifdef _WIN32
    const auto current = CurrentBounds();
    if (stackPointer >= current.deallocation && stackPointer < current.base) return;
    StackBounds target{};
    if (RegisteredBounds(stackPointer, target) || HostBounds(stackPointer, target)) ApplyBounds(target);
#else
    (void)stackPointer;
#endif
}

#ifdef _WIN32
[[noreturn]] void EnterGuestMainStack_nid_no_patch(void (*entry)()) {
    if (entry == nullptr) throw std::invalid_argument("Guest main stack requires an entry point");
    const auto host = CurrentBounds();
    constexpr std::size_t guardBytes = 0x10000;
    if (host.base <= host.deallocation || host.base - host.deallocation > std::numeric_limits<std::size_t>::max() - guardBytes)
        throw std::runtime_error("Cannot obtain the host main stack capacity");
    const auto capacity = host.base - host.deallocation;
    if (capacity % guardBytes != 0) throw std::runtime_error("Unexpected host main stack alignment");
    static std::atomic_flag entered = ATOMIC_FLAG_INIT;
    if (entered.test_and_set()) throw std::runtime_error("Guest main stack already entered");
    void* allocation = nullptr;
    std::uintptr_t low = 0;
    try {
        allocation = GuestArena::GuestArenaAllocate_nid_postfix(capacity + guardBytes, guardBytes);
        low = reinterpret_cast<std::uintptr_t>(allocation) + guardBytes;
        GuestArena::GuestArenaCommit_nid_postfix(reinterpret_cast<void*>(low), capacity, PAGE_READWRITE, 0x1000);
        GuestStackRegister_nid_no_patch(reinterpret_cast<void*>(low), capacity);
    } catch (...) {
        if (allocation != nullptr) {
            GuestArena::GuestArenaUnpinWritable_nid_postfix(reinterpret_cast<void*>(low), capacity);
            GuestArena::GuestArenaReset_nid_postfix(allocation, capacity + guardBytes);
            GuestArena::GuestArenaRelease_nid_postfix(allocation, capacity + guardBytes);
        }
        entered.clear();
        throw;
    }
    CONTEXT context{};
    RtlCaptureContext(&context);
    context.Rsp = low + capacity - 40;
    context.Rbp = 0;
    context.Rip = reinterpret_cast<DWORD64>(entry);
    *reinterpret_cast<std::uintptr_t*>(context.Rsp) = 0;
    ApplyBounds({low + capacity, low, reinterpret_cast<std::uintptr_t>(allocation)});
    RtlRestoreContext(&context, nullptr);
    std::abort();
}
#endif

}
