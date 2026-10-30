#include "ShadowAddressSpace.hpp"
#include "BackingMemory.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdint>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

namespace {

using Range = std::pair<std::uintptr_t, std::uintptr_t>;

std::mutex initMutex;
bool initialized = false;
std::atomic<bool> active{false};
std::uintptr_t pageSize = 0;
std::vector<Range> reservations;
std::vector<Range> holes;

std::vector<Range>::const_iterator FirstEndingAfter(const std::vector<Range>& ranges, std::uintptr_t address) noexcept {
    return std::upper_bound(ranges.begin(), ranges.end(), address, [](std::uintptr_t value, const Range& range) { return value < range.second; });
}

LONG CALLBACK shadowExceptionHandler(PEXCEPTION_POINTERS info) noexcept {
    if (!active.load(std::memory_order_acquire) || info == nullptr || info->ExceptionRecord == nullptr) return EXCEPTION_CONTINUE_SEARCH;
    const auto* record = info->ExceptionRecord;
    if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || (record->ExceptionFlags & EXCEPTION_NONCONTINUABLE) != 0 || record->NumberParameters < 2) return EXCEPTION_CONTINUE_SEARCH;
    const auto operation = record->ExceptionInformation[0];
    if (operation != 0 && operation != 1) return EXCEPTION_CONTINUE_SEARCH;
    const auto address = static_cast<std::uintptr_t>(record->ExceptionInformation[1]);
    const auto reservation = FirstEndingAfter(reservations, address);
    if (reservation == reservations.end() || reservation->first > address) return EXCEPTION_CONTINUE_SEARCH;
    auto* page = reinterpret_cast<void*>(address & ~(pageSize - 1));
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQuery(page, &region, sizeof(region)) != sizeof(region) || region.AllocationBase != reinterpret_cast<void*>(reservation->first)) return EXCEPTION_CONTINUE_SEARCH;
    if (region.State == MEM_RESERVE) {
        if (VirtualAlloc(page, pageSize, MEM_COMMIT, PAGE_READWRITE) != page) return EXCEPTION_CONTINUE_SEARCH;
        if (VirtualQuery(page, &region, sizeof(region)) != sizeof(region)) return EXCEPTION_CONTINUE_SEARCH;
    }
    if (region.State != MEM_COMMIT || (region.Protect & PAGE_GUARD) != 0 || (region.Protect & 0xffu) != PAGE_READWRITE) return EXCEPTION_CONTINUE_SEARCH;
    return EXCEPTION_CONTINUE_EXECUTION;
}

void ReleaseReservations() noexcept {
    for (const auto& reservation : reservations) {
        VirtualFree(reinterpret_cast<void*>(reservation.first), 0, MEM_RELEASE);
    }
    reservations.clear();
    holes.clear();
}

void AddHole(std::uintptr_t begin, std::uintptr_t end) {
    if (!holes.empty() && holes.back().second == begin) {
        holes.back().second = end;
    } else {
        holes.emplace_back(begin, end);
    }
}

bool ReserveFreeRanges(std::uintptr_t granularity) {
    const auto end = AsanRuntime::ShadowBase + AsanRuntime::ShadowSize;
    auto cursor = AsanRuntime::ShadowBase;
    bool retried = false;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<void*>(cursor), &region, sizeof(region)) != sizeof(region)) return false;
        const auto regionEnd = std::min(end, reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize);
        if (regionEnd <= cursor) return false;
        const auto begin = (cursor + granularity - 1) & ~(granularity - 1);
        if (region.State == MEM_FREE && begin < regionEnd) {
            auto* base = VirtualAlloc(reinterpret_cast<void*>(begin), regionEnd - begin, MEM_RESERVE, PAGE_NOACCESS);
            if (base == reinterpret_cast<void*>(begin)) {
                if (cursor < begin) AddHole(cursor, begin);
                reservations.emplace_back(begin, regionEnd);
                cursor = regionEnd;
                retried = false;
                continue;
            }
            if (base != nullptr) VirtualFree(base, 0, MEM_RELEASE);
            if (!retried) {
                retried = true;
                continue;
            }
        }
        AddHole(cursor, regionEnd);
        cursor = regionEnd;
        retried = false;
    }
    return true;
}

}

namespace AsanRuntime {

extern "C" bool InitializeShadow_nid_no_patch() noexcept {
    if (active.load(std::memory_order_acquire)) return true;
    try {
        std::lock_guard lock(initMutex);
        if (initialized) return true;
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        if (info.dwPageSize == 0 || !std::has_single_bit(info.dwPageSize) || !std::has_single_bit(info.dwAllocationGranularity) || !active.is_lock_free()) return false;
        if (reinterpret_cast<std::uintptr_t>(info.lpMaximumApplicationAddress) < ShadowBase + ShadowSize - 1) return false;
        pageSize = info.dwPageSize;
        if (!ReserveFreeRanges(info.dwAllocationGranularity)) {
            ReleaseReservations();
            return false;
        }
        auto* handler = AddVectoredExceptionHandler(1, shadowExceptionHandler);
        if (handler == nullptr) {
            ReleaseReservations();
            return false;
        }
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&InitializeShadow_nid_no_patch), &module)) {
            RemoveVectoredExceptionHandler(handler);
            ReleaseReservations();
            return false;
        }
        active.store(true, std::memory_order_release);
        initialized = true;
        return true;
    } catch (...) {
        ReleaseReservations();
        return false;
    }
}

extern "C" bool ShadowRangeAvailable_nid_no_patch(std::uintptr_t begin, std::uintptr_t end) noexcept {
    if (begin < ShadowBase || end > ShadowBase + ShadowSize || begin > end || !InitializeShadow_nid_no_patch()) return false;
    if (begin == end || holes.empty()) return true;
    const auto hole = FirstEndingAfter(holes, begin);
    return hole == holes.end() || hole->first >= end;
}

extern "C" void* AllocateBacking_nid_no_patch(std::size_t size, std::size_t alignment) noexcept {
    if (size == 0 || !std::has_single_bit(alignment) || !InitializeShadow_nid_no_patch()) return nullptr;
    using AllocateFunction = PVOID (WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
    static const auto allocate = [] {
        const auto module = GetModuleHandleW(L"KernelBase.dll");
        return module ? reinterpret_cast<AllocateFunction>(GetProcAddress(module, "VirtualAlloc2")) : nullptr;
    }();
    if (!allocate || size > std::numeric_limits<std::size_t>::max() - (pageSize - 1)) return nullptr;
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::uintptr_t granularity = info.dwAllocationGranularity;
    const auto bytes = (size + pageSize - 1) & ~(pageSize - 1);
    const auto limit = std::min<std::uintptr_t>((1ull << 47), reinterpret_cast<std::uintptr_t>(info.lpMaximumApplicationAddress) + 1) & ~(granularity - 1);
    const auto requiredAlignment = std::max<std::size_t>(alignment, granularity);
    if (requiredAlignment >= limit) return nullptr;
    const Range applicationRanges[] = {{ShadowBase + ShadowSize, limit}, {granularity, ShadowBase}};
    for (const auto& application : applicationRanges) {
        for (const auto& reservation : reservations) {
            const auto begin = (std::max(application.first, (reservation.first - ShadowBase) << 3) + requiredAlignment - 1) & ~(requiredAlignment - 1);
            const auto end = std::min(application.second, (reservation.second - ShadowBase) << 3) & ~(granularity - 1);
            if (begin >= end || bytes > end - begin) continue;
            MEM_ADDRESS_REQUIREMENTS requirements{};
            requirements.LowestStartingAddress = reinterpret_cast<void*>(begin);
            requirements.HighestEndingAddress = reinterpret_cast<void*>(end - 1);
            requirements.Alignment = requiredAlignment;
            MEM_EXTENDED_PARAMETER parameter{};
            parameter.Type = MemExtendedParameterAddressRequirements;
            parameter.Pointer = &requirements;
            if (void* block = allocate(nullptr, nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, &parameter, 1)) return block;
        }
    }
    return nullptr;
}

extern "C" void FreeBacking_nid_no_patch(void* pointer, std::size_t) noexcept {
    if (pointer) VirtualFree(pointer, 0, MEM_RELEASE);
}

}
#endif
