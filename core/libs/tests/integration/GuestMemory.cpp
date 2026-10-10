#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>
#if defined(__linux__)
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

extern "C" {
void* APS5_VABI mmap_nid_postfix(void*, std::size_t, int, int, int, std::int64_t) noexcept;
int APS5_VABI munmap_nid_postfix(void*, std::size_t) noexcept;
int APS5_VABI mprotect_nid_postfix(void*, std::size_t, int) noexcept;
int APS5_VABI madvise_nid_postfix(void*, std::size_t, int);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sceKernelMapNamedFlexibleMemory(void**, std::size_t, int, int, const char*);
int APS5_VABI sceKernelMapNamedFlexibleMemoryInternal(void**, std::size_t, int, int, const char*);
int APS5_VABI sceKernelAvailableFlexibleMemorySize(std::size_t*);
int APS5_VABI sceKernelMapFlexibleMemory(void**, std::size_t, int, int);
int APS5_VABI sceKernelMunmap(void*, std::size_t);
int APS5_VABI sceKernelReleaseFlexibleMemory(void*, std::size_t);
int APS5_VABI sceKernelMprotect(const void*, std::size_t, int);
int APS5_VABI sceKernelMtypeprotect(const void*, std::size_t, int, int);
int APS5_VABI sceKernelBatchMap2(KernelBatchMapEntry*, int, int*, int);
int APS5_VABI sceKernelVirtualQuery(const void*, int, VirtualQueryInfo*, std::uint64_t);
int APS5_VABI sceKernelSetVirtualRangeName(const void*, std::uint64_t, const char*);
int APS5_VABI sceKernelClearVirtualRangeName(const void*, std::uint64_t);
int APS5_VABI sceKernelAllocateDirectMemory(std::int64_t, std::int64_t, std::size_t, std::size_t, int, std::int64_t*);
int APS5_VABI sceKernelMapDirectMemory(void**, std::size_t, int, int, std::int64_t, std::size_t);
int APS5_VABI sceKernelReleaseDirectMemory(std::int64_t, std::size_t);
int APS5_VABI sceKernelCheckedReleaseDirectMemory(std::int64_t, std::size_t);
int APS5_VABI sceKernelReserveVirtualRange(void**, std::size_t, int, std::size_t);
int APS5_VABI sceKernelMemoryPoolReserve(void*, std::size_t, std::size_t, int, void**);
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
std::int64_t APS5_VABI sceKernelRead(int, void*, std::size_t);
std::int64_t APS5_VABI sceKernelPread(int, void*, std::size_t, std::int64_t);
int APS5_VABI sceKernelAioInitializeImpl(void*, std::int32_t);
int APS5_VABI sceKernelAioSubmitReadCommands(KernelAioRwRequest*, std::int32_t, std::int32_t, std::int32_t*);
int APS5_VABI sceKernelAioWaitRequest(std::int32_t, std::int32_t*, std::uint32_t*);
int APS5_VABI sceKernelAioDeleteRequest(std::int32_t, std::int32_t*);
int APS5_VABI sceKernelMlock_nid_postfix(void*, std::uint64_t);
int APS5_VABI sceKernelGetDirectMemoryType(std::int64_t, int*, std::int64_t*, std::int64_t*);
}

namespace {

using Testing::Case;
using Testing::Fail;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

template<typename TOperation>
void RequireAnyStdException(const TOperation& operation, const std::string& message) {
    try {
        operation();
    } catch (const Testing::Failure&) {
        throw;
    } catch (const std::exception&) {
        return;
    }
    Fail(message + ": did not throw");
}

constexpr std::size_t page = 0x4000;
constexpr std::int64_t physPage = static_cast<std::int64_t>(page);
constexpr std::int64_t directLimit = 0x7fffffffffll;
constexpr std::size_t flexibleLength = 0x10000;
constexpr int readOnly = 1;
constexpr int readWrite = 3;
constexpr int fixedFlag = 0x10;
constexpr int fixedNoOverwrite = 0x90;
constexpr int guestPrivateAnonymous = 0x1002;
constexpr int guestEinval = 22;
constexpr int guestEopnotsupp = 45;
constexpr std::int32_t batchUnmap = 1;
constexpr std::int32_t batchProtect = 2;
constexpr std::int32_t batchMapFlexible = 3;
constexpr std::int32_t batchTypeProtect = 4;

void* FailedMapping() {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
}

std::uintptr_t Address(const volatile void* pointer) {
    return reinterpret_cast<std::uintptr_t>(pointer);
}

void* At(void* base, std::size_t offset) {
    return static_cast<unsigned char*>(base) + offset;
}

int ByteAt(const void* base, std::size_t offset) {
    return static_cast<const volatile unsigned char*>(base)[offset];
}

void StoreByte(void* base, std::size_t offset, unsigned char value) {
    static_cast<volatile unsigned char*>(base)[offset] = value;
}

class Mappings {
public:
    Mappings() = default;

    ~Mappings() {
        for (auto range = ranges.rbegin(); range != ranges.rend(); ++range) {
            try {
                sceKernelMunmap(range->first, range->second);
            } catch (...) {
            }
        }
    }

    Mappings(const Mappings&) = delete;
    Mappings& operator=(const Mappings&) = delete;

    int Track(int result, void* const* address, std::size_t bytes) {
        if (result == 0) TrackRange(*address, bytes);
        return result;
    }

    void TrackRange(void* address, std::size_t bytes) {
        if (address != nullptr && address != FailedMapping()) ranges.emplace_back(address, bytes);
    }

private:
    std::vector<std::pair<void*, std::size_t>> ranges;
};

class DirectAllocations {
public:
    DirectAllocations() = default;

    ~DirectAllocations() {
        for (auto block = blocks.rbegin(); block != blocks.rend(); ++block) {
            try {
                sceKernelReleaseDirectMemory(block->first, block->second);
            } catch (...) {
            }
        }
    }

    DirectAllocations(const DirectAllocations&) = delete;
    DirectAllocations& operator=(const DirectAllocations&) = delete;

    std::int64_t Allocate(std::size_t bytes, std::string_view label, int type = 0) {
        return AllocateIn(0, directLimit, bytes, label, type);
    }

    std::int64_t AllocateIn(std::int64_t searchStart, std::int64_t searchEnd, std::size_t bytes, std::string_view label, int type = 0) {
        std::int64_t start = -1;
        const int result = sceKernelAllocateDirectMemory(searchStart, searchEnd, bytes, 0, type, &start);
        if (result == 0) blocks.emplace_back(start, bytes);
        RequireEqual(result, 0, label);
        return start;
    }

private:
    std::vector<std::pair<std::int64_t, std::size_t>> blocks;
};

void* MapFlexible(Mappings& mappings, std::size_t bytes, std::string_view label, int protection = readWrite) {
    void* address = nullptr;
    RequireEqual(mappings.Track(sceKernelMapFlexibleMemory(&address, bytes, protection, 0), &address, bytes), 0, label);
    Require(address != nullptr, label);
    return address;
}

void* MapFlexibleAt(Mappings& mappings, void* at, std::size_t bytes, int flags, std::string_view label) {
    void* address = at;
    RequireEqual(mappings.Track(sceKernelMapFlexibleMemory(&address, bytes, readWrite, flags), &address, bytes), 0, label);
    Require(address == at, std::string(label) + " lands at the requested address");
    return address;
}

void* MapDirect(Mappings& mappings, std::int64_t offset, std::size_t bytes, std::string_view label, int protection = readWrite) {
    void* address = nullptr;
    RequireEqual(mappings.Track(sceKernelMapDirectMemory(&address, bytes, protection, 0, offset, 0), &address, bytes), 0, label);
    Require(address != nullptr, label);
    return address;
}

void* MapDirectAt(Mappings& mappings, void* at, std::int64_t offset, std::size_t bytes, int flags, std::string_view label,
                  int protection = readWrite) {
    void* address = at;
    RequireEqual(mappings.Track(sceKernelMapDirectMemory(&address, bytes, protection, flags, offset, 0), &address, bytes), 0, label);
    Require(address == at, std::string(label) + " lands at the requested address");
    return address;
}

void* Reserve(Mappings& mappings, std::size_t bytes, std::string_view label) {
    void* address = nullptr;
    RequireEqual(mappings.Track(sceKernelReserveVirtualRange(&address, bytes, 0, 0), &address, bytes), 0, label);
    return address;
}

void* UnusedRange(std::size_t bytes) {
    void* probe = nullptr;
    RequireEqual(sceKernelReserveVirtualRange(&probe, bytes, 0, 0), 0, "reserve a probe range");
    RequireEqual(sceKernelMunmap(probe, bytes), 0, "release the probe range");
    return probe;
}

unsigned char* MapAnonymous(Mappings& mappings, std::size_t bytes, std::string_view label) {
    void* mapped = mmap_nid_postfix(nullptr, bytes, readWrite, guestPrivateAnonymous, -1, 0);
    mappings.TrackRange(mapped, (bytes + page - 1) & ~(page - 1));
    Require(mapped != FailedMapping(), label);
    return static_cast<unsigned char*>(mapped);
}

VirtualQueryInfo Query(const void* address, std::string_view label, int flags = 0) {
    VirtualQueryInfo info{};
    RequireEqual(sceKernelVirtualQuery(address, flags, &info, sizeof(info)), 0, label);
    return info;
}

std::string NameAt(const void* address) {
    const VirtualQueryInfo info = Query(address, "query the range name");
    return std::string(info.name, std::find(info.name, info.name + sizeof(info.name), '\0'));
}

std::size_t AvailableFlexible() {
    std::size_t available = 0;
    RequireEqual(sceKernelAvailableFlexibleMemorySize(&available), 0, "query available flexible memory");
    return available;
}

struct DirectType {
    int result;
    int type;
    std::int64_t start;
    std::int64_t end;
};

DirectType TypeAt(std::int64_t offset) {
    DirectType value{0, -1, -1, -1};
    value.result = sceKernelGetDirectMemoryType(offset, &value.type, &value.start, &value.end);
    return value;
}

GuestAllocations::Range FindRange(const void* address) {
    GuestAllocations::Mutation mutation;
    return mutation.Find(address);
}

int Errno() {
    return *__error_nid_postfix();
}

void ClearErrno() {
    *__error_nid_postfix() = 0;
}

const Case splitFlexibleProtection{"VirtualQuery_FlexibleRangeWithProtectedMiddlePage_ReportsEachPage", [] {
    Mappings mappings;
    void* bytes = MapFlexible(mappings, page * 3, "map three pages");
    StoreByte(bytes, 0, 17);
    StoreByte(bytes, page * 2, 29);
    RequireEqual(sceKernelMprotect(At(bytes, page), page, readOnly), 0, "protect the middle page");
    for (std::size_t index = 0; index < 3; ++index) {
        const std::string label = "page " + std::to_string(index);
        const VirtualQueryInfo info = Query(At(bytes, page * index), label + " query");
        RequireEqual(info.start, Address(At(bytes, page * index)), label + " start");
        RequireEqual(info.end, info.start + page, label + " end");
        RequireEqual(info.protection, index == 1 ? 1 : 3, label + " protection");
        Require(info.is_committed && info.is_flexible && !info.is_direct, label + " is committed flexible memory");
    }
}};

const Case unmapMiddleFlexible{"VirtualQuery_AfterUnmappingMiddleFlexiblePage_ReportsRemainingPages", [] {
    Mappings mappings;
    void* bytes = MapFlexible(mappings, page * 3, "map three pages");
    StoreByte(bytes, 0, 17);
    StoreByte(bytes, page * 2, 29);
    RequireEqual(sceKernelMprotect(At(bytes, page), page, readOnly), 0, "protect the middle page");
    RequireEqual(sceKernelMunmap(At(bytes, page), page), 0, "unmap the middle page");
    for (const std::size_t index : {std::size_t{0}, std::size_t{2}}) {
        const std::string label = "page " + std::to_string(index);
        const VirtualQueryInfo info = Query(At(bytes, page * index), label + " query");
        RequireEqual(info.start, Address(At(bytes, page * index)), label + " start");
        RequireEqual(info.end, info.start + page, label + " end");
    }
    const VirtualQueryInfo next = Query(At(bytes, page), "find next from the hole", 1);
    RequireEqual(next.start, Address(At(bytes, page * 2)), "next start");
    RequireEqual(next.end, next.start + page, "next end");
    RequireEqual(next.protection, 3, "next protection");
    RequireEqual(ByteAt(bytes, 0), 17, "first page contents");
    RequireEqual(ByteAt(bytes, page * 2), 29, "last page contents");
    RequireEqual(sceKernelMunmap(bytes, page), 0, "unmap the first page");
    RequireEqual(sceKernelMunmap(At(bytes, page * 2), page), 0, "unmap the last page");
}};

const Case noAccessQuery{"VirtualQuery_NoAccessPosixMapping_ReportsZeroProtection", [] {
    Mappings mappings;
    unsigned char* mapped = MapAnonymous(mappings, page, "map one page");
    RequireEqual(mprotect_nid_postfix(mapped, page, 0), 0, "remove all access");
    const VirtualQueryInfo info = Query(mapped, "query the no-access page");
    RequireEqual(info.start, Address(mapped), "start");
    RequireEqual(info.end, info.start + page, "end");
    RequireEqual(info.protection, 0, "protection");
    RequireEqual(sceKernelMunmap(mapped, page), 0, "unmap");
}};

const Case posixWholeQuery{"VirtualQuery_PosixAnonymousMapping_ReportsWholeRange", [] {
    Mappings mappings;
    unsigned char* memory = MapAnonymous(mappings, page * 3, "map three pages");
    const VirtualQueryInfo before = Query(memory, "query the mapping");
    RequireEqual(before.start, Address(memory), "start");
    RequireEqual(before.end, Address(memory) + page * 3, "end");
}};

const Case posixPartialQuery{"VirtualQuery_AfterPosixMunmapOfMiddlePage_SplitsRange", [] {
    Mappings mappings;
    unsigned char* memory = MapAnonymous(mappings, page * 3, "map three pages");
    RequireEqual(munmap_nid_postfix(memory + page, page), 0, "unmap the middle page");
    const VirtualQueryInfo first = Query(memory, "query the first page");
    RequireEqual(first.start, Address(memory), "first start");
    RequireEqual(first.end, Address(memory) + page, "first end");
    VirtualQueryInfo middle{};
    RequireEqual(sceKernelVirtualQuery(memory + page, 0, &middle, sizeof(middle)), SCE_KERNEL_ERROR_EACCES, "query the hole");
    const VirtualQueryInfo next = Query(memory + page, "find next from the hole", 1);
    RequireEqual(next.start, Address(memory) + page * 2, "next start");
    RequireEqual(next.end, Address(memory) + page * 3, "next end");
    const VirtualQueryInfo tail = Query(memory + page * 2, "query the tail");
    RequireEqual(tail.start, Address(memory) + page * 2, "tail start");
    RequireEqual(tail.end, Address(memory) + page * 3, "tail end");
    RequireEqual(munmap_nid_postfix(memory, page), 0, "unmap the first page");
    RequireEqual(munmap_nid_postfix(memory + page * 2, page), 0, "unmap the tail");
}};

const Case mapFlexibleCharges{"MapFlexibleMemory_Mapping_ReducesAvailableSize", [] {
    const std::size_t before = AvailableFlexible();
    Mappings mappings;
    void* mapped = MapFlexible(mappings, flexibleLength, "map flexible memory");
    StoreByte(mapped, flexibleLength - 1, 1);
    RequireEqual(AvailableFlexible(), before - flexibleLength, "available after mapping");
}};

const Case releaseFlexible{"ReleaseFlexibleMemory_MappedRange_RestoresAvailableSize", [] {
    const std::size_t before = AvailableFlexible();
    Mappings mappings;
    void* mapped = MapFlexible(mappings, flexibleLength, "map flexible memory");
    StoreByte(mapped, flexibleLength - 1, 1);
    RequireEqual(sceKernelReleaseFlexibleMemory(mapped, flexibleLength), 0, "release");
    RequireEqual(AvailableFlexible(), before, "available after release");
}};

const Case remapReleasedFlexible{"MapFlexibleMemory_FixedAtReleasedRange_MapsZeroedPagesThere", [] {
    Mappings mappings;
    void* mapped = MapFlexible(mappings, flexibleLength, "map flexible memory");
    StoreByte(mapped, flexibleLength - 1, 1);
    RequireEqual(sceKernelReleaseFlexibleMemory(mapped, flexibleLength), 0, "release");
    void* again = MapFlexibleAt(mappings, mapped, flexibleLength, fixedNoOverwrite, "remap at the released address");
    RequireEqual(ByteAt(again, flexibleLength - 1), 0, "remapped byte");
    RequireEqual(sceKernelMunmap(again, flexibleLength), 0, "unmap");
}};

const Case namedMapping{"MapNamedFlexibleMemory_Name_IsReportedByVirtualQuery", [] {
    Mappings mappings;
    void* first = nullptr;
    RequireEqual(mappings.Track(sceKernelMapNamedFlexibleMemory(&first, flexibleLength, 3, 0, "first mapping"), &first, flexibleLength), 0,
                 "map named memory");
    RequireEqual(NameAt(first), std::string("first mapping"), "mapping name");
}};

const Case subRangeName{"SetVirtualRangeName_SubRange_NamesOnlyThatRange", [] {
    Mappings mappings;
    void* first = nullptr;
    RequireEqual(mappings.Track(sceKernelMapNamedFlexibleMemory(&first, flexibleLength, 3, 0, "first mapping"), &first, flexibleLength), 0,
                 "map named memory");
    void* middle = At(first, 0x4000);
    RequireEqual(sceKernelSetVirtualRangeName(middle, 0x4000, "middle"), 0, "name the middle");
    RequireEqual(NameAt(first), std::string("first mapping"), "start keeps its name");
    RequireEqual(NameAt(middle), std::string("middle"), "middle name");
}};

const Case clearRangeName{"ClearVirtualRangeName_WholeMapping_RemovesSubRangeName", [] {
    Mappings mappings;
    void* first = nullptr;
    RequireEqual(mappings.Track(sceKernelMapNamedFlexibleMemory(&first, flexibleLength, 3, 0, "first mapping"), &first, flexibleLength), 0,
                 "map named memory");
    void* middle = At(first, 0x4000);
    RequireEqual(sceKernelSetVirtualRangeName(middle, 0x4000, "middle"), 0, "name the middle");
    RequireEqual(sceKernelClearVirtualRangeName(first, flexibleLength), 0, "clear the names");
    RequireEqual(NameAt(middle), std::string(), "middle name after clearing");
}};

const Case nullRangeName{"SetVirtualRangeName_NullAddress_Fails", [] {
    Require(sceKernelSetVirtualRangeName(nullptr, flexibleLength, "x") != 0, "naming a null range fails");
}};

const Case hintedMapping{"MapFlexibleMemory_HintAtLiveMapping_MapsAboveHintOnGuestPageBoundary", [] {
    Mappings mappings;
    void* first = MapFlexible(mappings, flexibleLength, "map the first range");
    void* hinted = first;
    RequireEqual(mappings.Track(sceKernelMapFlexibleMemory(&hinted, flexibleLength, 3, 0), &hinted, flexibleLength), 0, "map with a hint");
    Require(Address(hinted) > Address(first), "hinted mapping lies above the hint");
    RequireEqual(Address(hinted) & 0x3fff, std::uintptr_t{0}, "hinted mapping alignment");
    StoreByte(hinted, flexibleLength - 1, 1);
}};

const Case noOverwriteFlexible{"MapFlexibleMemory_FixedNoOverwriteOnLiveMapping_FailsWithEnomem", [] {
    Mappings mappings;
    void* first = MapFlexible(mappings, flexibleLength, "map the first range");
    void* overwrite = first;
    RequireEqual(mappings.Track(sceKernelMapFlexibleMemory(&overwrite, 0x4000, 3, fixedNoOverwrite), &overwrite, 0x4000),
                 SCE_KERNEL_ERROR_ENOMEM, "fixed no-overwrite mapping");
    Require(overwrite == first, "address left unchanged");
}};

const Case noOverwriteFreed{"MapFlexibleMemory_FixedNoOverwriteOnFreedRange_MapsAtRequestedAddress", [] {
    Mappings mappings;
    void* first = MapFlexible(mappings, flexibleLength, "map the first range");
    void* hinted = first;
    RequireEqual(mappings.Track(sceKernelMapFlexibleMemory(&hinted, flexibleLength, 3, 0), &hinted, flexibleLength), 0, "map with a hint");
    RequireEqual(sceKernelMunmap(hinted, flexibleLength), 0, "unmap the hinted range");
    void* exclusive = MapFlexibleAt(mappings, hinted, flexibleLength, fixedNoOverwrite, "fixed no-overwrite mapping");
    StoreByte(exclusive, 0, 1);
    RequireEqual(sceKernelMunmap(exclusive, flexibleLength), 0, "unmap the exclusive range");
    RequireEqual(sceKernelMunmap(first, flexibleLength), 0, "unmap the first range");
}};

const Case internalNamed{"MapNamedFlexibleMemoryInternal_Mapping_IsNamedAndCharged", [] {
    const std::size_t before = AvailableFlexible();
    Mappings mappings;
    void* mapped = nullptr;
    RequireEqual(mappings.Track(sceKernelMapNamedFlexibleMemoryInternal(&mapped, flexibleLength, 3, 0, "internal mapping"), &mapped,
                                flexibleLength), 0, "map internal named memory");
    Require(mapped != nullptr, "mapping address");
    RequireEqual(NameAt(mapped), std::string("internal mapping"), "mapping name");
    RequireEqual(AvailableFlexible(), before - flexibleLength, "available after mapping");
    RequireEqual(sceKernelMunmap(mapped, flexibleLength), 0, "unmap");
    RequireEqual(AvailableFlexible(), before, "available after unmapping");
}};

const Case internalFlag{"MapNamedFlexibleMemoryInternal_InternalFlag_IsIgnored", [] {
    const std::size_t before = AvailableFlexible();
    Mappings mappings;
    void* flagged = nullptr;
    RequireEqual(mappings.Track(sceKernelMapNamedFlexibleMemoryInternal(&flagged, flexibleLength, 3, 0x8000, "internal flag"), &flagged,
                                flexibleLength), 0, "map with the internal flag");
    Require(flagged != nullptr, "mapping address");
    RequireEqual(NameAt(flagged), std::string("internal flag"), "mapping name");
    StoreByte(flagged, flexibleLength - 1, 1);
    RequireEqual(AvailableFlexible(), before - flexibleLength, "available after mapping");
    RequireEqual(sceKernelMunmap(flagged, flexibleLength), 0, "unmap");
}};

const Case internalUnknownFlag{"MapNamedFlexibleMemoryInternal_UnknownFlag_ThrowsWithoutCharging", [] {
    const std::size_t before = AvailableFlexible();
    Mappings mappings;
    void* unknown = nullptr;
    RequireAnyStdException([&] {
        mappings.Track(sceKernelMapNamedFlexibleMemoryInternal(&unknown, flexibleLength, 3, 0x20000, "internal mapping"), &unknown,
                       flexibleLength);
    }, "flag 0x20000");
    Require(unknown == nullptr, "address left null");
    RequireEqual(AvailableFlexible(), before, "available after the rejected mapping");
}};

KernelBatchMapEntry FlexibleEntry() {
    return KernelBatchMapEntry{nullptr, 0, page, 3, 0, 0, batchMapFlexible};
}

void TrackEntries(Mappings& mappings, const KernelBatchMapEntry* entries, int count) {
    for (int index = 0; index < count; ++index) mappings.TrackRange(entries[index].start, page);
}

const Case batchUnknownOperation{"BatchMap2_UnknownOperation_StopsAtThatEntry", [] {
    for (const std::int32_t operation : {5, 6, -1, std::numeric_limits<std::int32_t>::max()}) {
        const std::string label = "operation " + std::to_string(operation);
        Mappings mappings;
        KernelBatchMapEntry entries[3] = {FlexibleEntry(), FlexibleEntry(), FlexibleEntry()};
        entries[1].operation = operation;
        int processed = -1;
        const int result = sceKernelBatchMap2(entries, 3, &processed, 0);
        TrackEntries(mappings, entries, 3);
        RequireEqual(result, SCE_KERNEL_ERROR_EINVAL, label + " result");
        RequireEqual(processed, 1, label + " processed");
        Require(entries[0].start != nullptr, label + " maps the first entry");
        Require(entries[1].start == nullptr && entries[2].start == nullptr, label + " leaves later entries untouched");
        RequireEqual(sceKernelMunmap(entries[0].start, page), 0, label + " unmap");
    }
}};

const Case batchZeroLengthProtect{"BatchMap2_ZeroLengthProtect_StopsAtThatEntry", [] {
    Mappings mappings;
    KernelBatchMapEntry entries[3] = {FlexibleEntry(), FlexibleEntry(), FlexibleEntry()};
    entries[1].operation = batchProtect;
    entries[1].length = 0;
    int processed = -1;
    const int result = sceKernelBatchMap2(entries, 3, &processed, 0);
    TrackEntries(mappings, entries, 3);
    RequireEqual(result, SCE_KERNEL_ERROR_EINVAL, "result");
    RequireEqual(processed, 1, "processed");
    Require(entries[0].start != nullptr, "first entry mapped");
    Require(entries[1].start == nullptr && entries[2].start == nullptr, "later entries untouched");
}};

const Case batchZeroLengthUnmap{"BatchMap2_ZeroLengthUnmap_StopsAfterPrecedingUnmapAndRangeRemaps", [] {
    Mappings mappings;
    KernelBatchMapEntry entries[3] = {FlexibleEntry(), FlexibleEntry(), FlexibleEntry()};
    entries[1].operation = batchProtect;
    entries[1].length = 0;
    int processed = -1;
    const int setup = sceKernelBatchMap2(entries, 3, &processed, 0);
    TrackEntries(mappings, entries, 3);
    RequireEqual(setup, SCE_KERNEL_ERROR_EINVAL, "setup batch result");
    Require(entries[0].start != nullptr, "setup maps the first entry");
    KernelBatchMapEntry unmaps[2] = {entries[0], entries[0]};
    unmaps[0].operation = batchUnmap;
    unmaps[1].operation = batchUnmap;
    unmaps[1].length = 0;
    processed = -1;
    RequireEqual(sceKernelBatchMap2(unmaps, 2, &processed, 0), SCE_KERNEL_ERROR_EINVAL, "unmap batch result");
    RequireEqual(processed, 1, "unmap batch processed");
    const int remapped = sceKernelBatchMap2(entries, 1, &processed, 0);
    TrackEntries(mappings, entries, 1);
    RequireEqual(remapped, 0, "remap batch result");
    RequireEqual(processed, 1, "remap batch processed");
    RequireEqual(sceKernelMunmap(entries[0].start, page), 0, "unmap");
}};

const Case checkedReleaseUnaligned{"CheckedReleaseDirectMemory_UnalignedArguments_FailWithEinval", [] {
    DirectAllocations direct;
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    RequireEqual(sceKernelCheckedReleaseDirectMemory(phys + 1, page), SCE_KERNEL_ERROR_EINVAL, "unaligned start");
    RequireEqual(sceKernelCheckedReleaseDirectMemory(phys, page + 1), SCE_KERNEL_ERROR_EINVAL, "unaligned length");
}};

const Case checkedReleaseZero{"CheckedReleaseDirectMemory_ZeroLength_Succeeds", [] {
    DirectAllocations direct;
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    RequireEqual(sceKernelCheckedReleaseDirectMemory(phys, 0), 0, "zero length");
}};

const Case checkedReleasePastEnd{"CheckedReleaseDirectMemory_RangePastAllocation_FailsWithEnoent", [] {
    DirectAllocations direct;
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    RequireEqual(sceKernelCheckedReleaseDirectMemory(phys, page * 3), SCE_KERNEL_ERROR_ENOENT, "three pages of a two page block");
}};

const Case checkedReleasePartial{"CheckedReleaseDirectMemory_AlreadyReleasedPages_FailWithEnoent", [] {
    DirectAllocations direct;
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    {
        Mappings mappings;
        void* mapped = MapDirect(mappings, phys, page * 2, "map the block");
        RequireEqual(sceKernelMunmap(mapped, page * 2), 0, "unmap the block");
    }
    RequireEqual(sceKernelCheckedReleaseDirectMemory(phys + physPage, page), 0, "release the second page");
    RequireEqual(sceKernelCheckedReleaseDirectMemory(phys, page * 2), SCE_KERNEL_ERROR_ENOENT, "range including the released page");
    RequireEqual(sceKernelCheckedReleaseDirectMemory(phys, page), 0, "release the first page");
    RequireEqual(sceKernelCheckedReleaseDirectMemory(phys, page), SCE_KERNEL_ERROR_ENOENT, "release the first page again");
}};

const Case audioProtection{"Mprotect_AudioCoprocessorProtections_KeepCpuAccess", [] {
    Mappings mappings;
    void* writable = MapFlexible(mappings, page, "map with protection 0x200", 0x200);
    StoreByte(writable, page - 1, 7);
    RequireEqual(ByteAt(writable, page - 1), 7, "written through protection 0x200");
    RequireEqual(sceKernelMprotect(writable, page, 0x100), 0, "protect 0x100");
    RequireEqual(ByteAt(writable, page - 1), 7, "readable after protection 0x100");
    RequireEqual(sceKernelMprotect(writable, page, 0x3f2), 0, "protect 0x3f2");
    StoreByte(writable, 0, 9);
    RequireEqual(sceKernelMunmap(writable, page), 0, "unmap");
}};

const Case undefinedProtection{"MapFlexibleMemory_UndefinedProtectionBit_ThrowsInvalidArgument", [] {
    Mappings mappings;
    void* undefined = nullptr;
    RequireThrows<std::invalid_argument>([&] {
        mappings.Track(sceKernelMapFlexibleMemory(&undefined, page, 0x400, 0), &undefined, page);
    }, "protection 0x400");
    Require(undefined == nullptr, "address left null");
}};

const Case directAlias{"MapDirectMemory_AliasOfPhysicalPage_SharesContentsBothWays", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    void* first = MapDirect(mappings, phys, page * 2, "map the block");
    StoreByte(first, 0, 11);
    StoreByte(first, page + 5, 22);
    void* alias = MapDirect(mappings, phys + physPage, page, "map an alias of the second page");
    Require(alias != first, "alias has its own address");
    RequireEqual(ByteAt(alias, 5), 22, "alias sees existing contents");
    StoreByte(alias, 5, 37);
    RequireEqual(ByteAt(first, page + 5), 37, "block sees alias write");
    StoreByte(first, page + 6, 48);
    RequireEqual(ByteAt(alias, 6), 48, "alias sees block write");
}};

const Case readOnlyAlias{"MapDirectMemory_ReadOnlyAlias_SeesWritesAndRegainsWriteAccess", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    void* first = MapDirect(mappings, phys, page * 2, "map the block");
    void* alias = MapDirect(mappings, phys + physPage, page, "map an alias of the second page");
    RequireEqual(sceKernelMprotect(alias, page, 1), 0, "make the alias read-only");
    StoreByte(first, page + 5, 59);
    RequireEqual(ByteAt(alias, 5), 59, "read-only alias sees block write");
    RequireEqual(sceKernelMprotect(alias, page, 3), 0, "make the alias writable");
    StoreByte(alias, 5, 22);
    RequireEqual(ByteAt(first, page + 5), 22, "block sees alias write");
    RequireEqual(sceKernelMunmap(alias, page), 0, "unmap the alias");
    StoreByte(first, page + 5, 22);
    RequireEqual(sceKernelMunmap(first, page * 2), 0, "unmap the block");
}};

const Case unmappedPhysicalPages{"MapDirectMemory_AfterAllViewsUnmapped_KeepsPhysicalContents", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    void* first = MapDirect(mappings, phys, page * 2, "map the block");
    StoreByte(first, 0, 11);
    StoreByte(first, page + 5, 22);
    RequireEqual(sceKernelMunmap(first, page * 2), 0, "unmap the block");
    MapFlexible(mappings, page * 2, "map a filler");
    void* second = MapDirect(mappings, phys + physPage, page, "map the second page again");
    Require(second != first, "new view has a new address");
    RequireEqual(ByteAt(second, 5), 22, "second page contents survive");
    void* reserved = Reserve(mappings, page, "reserve a page");
    void* fixed = MapDirectAt(mappings, reserved, phys, page, fixedFlag, "map the first page read-only into the reservation", 1);
    RequireEqual(ByteAt(fixed, 0), 11, "first page contents survive");
    RequireEqual(sceKernelMunmap(fixed, page), 0, "unmap the fixed view");
    RequireEqual(sceKernelMunmap(second, page), 0, "unmap the second view");
}};

const Case reallocateZeroed{"ReleaseDirectMemory_ThenReallocate_ReturnsZeroedPages", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    void* first = MapDirect(mappings, phys, page * 2, "map the block");
    StoreByte(first, 0, 11);
    StoreByte(first, page + 5, 22);
    RequireEqual(sceKernelMunmap(first, page * 2), 0, "unmap the block");
    void* filler = MapFlexible(mappings, page * 2, "map a filler");
    RequireEqual(sceKernelReleaseDirectMemory(phys, page * 2), 0, "release");
    const std::int64_t again = direct.Allocate(page * 2, "allocate again");
    RequireEqual(again, phys, "reallocated offset");
    void* fresh = MapDirect(mappings, again, page * 2, "map the reallocated block");
    RequireEqual(ByteAt(fresh, 0), 0, "first page is zeroed");
    RequireEqual(ByteAt(fresh, page + 5), 0, "second page is zeroed");
    RequireEqual(sceKernelMunmap(fresh, page * 2), 0, "unmap the reallocated block");
    RequireEqual(sceKernelMunmap(filler, page * 2), 0, "unmap the filler");
    RequireEqual(sceKernelReleaseDirectMemory(again, page * 2), 0, "release again");
}};

const Case releaseSecondHalf{"ReleaseDirectMemory_SecondHalf_KeepsFirstHalfMapped", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    void* mapped = MapDirect(mappings, phys, page * 2, "map the block");
    const VirtualQueryInfo before = Query(mapped, "query before release");
    Require(before.is_direct, "direct before release");
    RequireEqual(before.offset, static_cast<std::uint64_t>(phys), "offset before release");
    RequireEqual(sceKernelReleaseDirectMemory(phys + physPage, page), 0, "release the second page");
    const VirtualQueryInfo split = Query(mapped, "query after release");
    Require(split.is_direct, "first page still direct");
    RequireEqual(split.offset, static_cast<std::uint64_t>(phys), "first page offset");
    VirtualQueryInfo dropped{};
    const int droppedResult = sceKernelVirtualQuery(At(mapped, page), 0, &dropped, sizeof(dropped));
    Require(droppedResult != 0 || !dropped.is_direct, "released page is no longer direct memory");
}};

const Case releaseRemovesViews{"ReleaseDirectMemory_Range_RemovesEveryViewOfIt", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    void* mapped = MapDirect(mappings, phys, page * 2, "map the block");
    RequireEqual(sceKernelReleaseDirectMemory(phys + physPage, page), 0, "release the second page");
    void* alias = MapDirect(mappings, phys, page, "map an alias of the first page");
    RequireEqual(sceKernelReleaseDirectMemory(phys, page), 0, "release the first page");
    for (void* view : {mapped, alias}) {
        const std::string label = view == mapped ? "block view" : "alias view";
        VirtualQueryInfo cleared{};
        Require(sceKernelVirtualQuery(view, 0, &cleared, sizeof(cleared)) != 0, label + " is unmapped");
        VirtualQueryInfo next{};
        if (sceKernelVirtualQuery(view, 1, &next, sizeof(next)) == 0) {
            Require(next.start != Address(view), label + " is not found as the next range");
        }
    }
}};

const Case fixedPartialOverlap{"MapDirectMemory_FixedOverPartialOverlap_ReplacesExistingMapping", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 3, "allocate");
    void* reserved = MapDirect(mappings, phys, page * 3, "map to find an address");
    RequireEqual(sceKernelMunmap(reserved, page * 3), 0, "unmap the probe");
    MapDirectAt(mappings, reserved, phys, page, fixedFlag, "map the head page");
    void* fixed = MapDirectAt(mappings, reserved, phys, page * 3, fixedFlag, "map over the head page");
    StoreByte(fixed, page * 2 + 7, 0x5c);
    const VirtualQueryInfo info = Query(fixed, "query the fixed mapping");
    Require(info.is_direct, "direct");
    RequireEqual(info.offset, static_cast<std::uint64_t>(phys), "offset");
    RequireEqual(ByteAt(fixed, page * 2 + 7), 0x5c, "contents");
    RequireEqual(sceKernelMunmap(fixed, page * 3), 0, "unmap");
    RequireEqual(sceKernelReleaseDirectMemory(phys, page * 3), 0, "release");
}};

class TypedBlocks {
public:
    TypedBlocks() {
        first = direct.Allocate(page * 2, "allocate the type 3 block", 3);
        second = direct.AllocateIn(first + physPage * 2, first + physPage * 3, page, "allocate the type 1 block", 1);
        RequireEqual(second, first + physPage * 2, "type 1 block follows the type 3 block");
    }

    DirectAllocations direct;
    std::int64_t first = 0;
    std::int64_t second = 0;
};

void RequireType(const DirectType& actual, int type, std::int64_t start, std::int64_t end, const std::string& label) {
    RequireEqual(actual.result, 0, label + " result");
    RequireEqual(actual.type, type, label + " type");
    RequireEqual(actual.start, start, label + " start");
    RequireEqual(actual.end, end, label + " end");
}

const Case directTypeBounds{"GetDirectMemoryType_OffsetsInsideBlocks_ReportTypeAndBounds", [] {
    const TypedBlocks blocks;
    RequireType(TypeAt(blocks.first), 3, blocks.first, blocks.first + physPage * 2, "block start");
    RequireType(TypeAt(blocks.first + physPage * 2 - 1), 3, blocks.first, blocks.first + physPage * 2, "block last byte");
    RequireType(TypeAt(blocks.first + physPage * 2), 1, blocks.second, blocks.second + physPage, "next block");
}};

const Case directTypeNullOutput{"GetDirectMemoryType_NullOutput_FailsWithEinval", [] {
    const TypedBlocks blocks;
    int type = -1;
    std::int64_t start = -1;
    std::int64_t end = -1;
    RequireEqual(sceKernelGetDirectMemoryType(blocks.first, nullptr, &start, &end), SCE_KERNEL_ERROR_EINVAL, "null type");
    RequireEqual(sceKernelGetDirectMemoryType(blocks.first, &type, nullptr, &end), SCE_KERNEL_ERROR_EINVAL, "null start");
    RequireEqual(sceKernelGetDirectMemoryType(blocks.first, &type, &start, nullptr), SCE_KERNEL_ERROR_EINVAL, "null end");
}};

const Case directTypeReleased{"GetDirectMemoryType_ReleasedOrNegativeOffset_FailsWithEnoentAndKeepsOutputs", [] {
    const TypedBlocks blocks;
    RequireEqual(sceKernelReleaseDirectMemory(blocks.first, page), 0, "release the first page");
    const DirectType released = TypeAt(blocks.first);
    RequireEqual(released.result, SCE_KERNEL_ERROR_ENOENT, "released offset");
    RequireEqual(released.type, -1, "type untouched");
    RequireEqual(released.start, std::int64_t{-1}, "start untouched");
    RequireEqual(released.end, std::int64_t{-1}, "end untouched");
    RequireEqual(TypeAt(-1).result, SCE_KERNEL_ERROR_ENOENT, "negative offset");
}};

const Case directTypeRemainder{"GetDirectMemoryType_PartiallyReleasedBlock_ReportsRemainder", [] {
    const TypedBlocks blocks;
    RequireEqual(sceKernelReleaseDirectMemory(blocks.first, page), 0, "release the first page");
    RequireType(TypeAt(blocks.first + physPage), 3, blocks.first + physPage, blocks.first + physPage * 2, "remaining page");
}};

const Case directTypeAllReleased{"GetDirectMemoryType_ReleasedBlock_FailsWithEnoent", [] {
    const TypedBlocks blocks;
    RequireEqual(sceKernelReleaseDirectMemory(blocks.first, page), 0, "release the first page");
    RequireEqual(sceKernelReleaseDirectMemory(blocks.first + physPage, page * 2), 0, "release the rest");
    RequireEqual(TypeAt(blocks.second).result, SCE_KERNEL_ERROR_ENOENT, "released type 1 block");
}};

class TypedMapping {
public:
    TypedMapping() {
        phys = direct.Allocate(page * 3, "allocate");
        bytes = MapDirect(mappings, phys, page * 3, "map the block");
        RequireEqual(sceKernelMtypeprotect(At(bytes, page + 1), 1, 3, 1), 0, "retype one byte of the middle page");
    }

    DirectAllocations direct;
    Mappings mappings;
    std::int64_t phys = 0;
    void* bytes = nullptr;
};

void RequirePage(const TypedMapping& mapping, std::size_t index, int expectedType, int expectedProtection) {
    const std::string label = "page " + std::to_string(index);
    void* address = At(mapping.bytes, page * index);
    const VirtualQueryInfo info = Query(address, label + " query");
    Require(info.is_direct, label + " is direct");
    RequireEqual(info.memory_type, expectedType, label + " memory type");
    RequireEqual(info.protection, expectedProtection, label + " protection");
    RequireEqual(info.start, Address(address), label + " start");
    RequireEqual(info.end, info.start + page, label + " end");
    RequireEqual(info.offset, static_cast<std::uint64_t>(mapping.phys) + page * index, label + " offset");
    const std::int64_t offset = mapping.phys + physPage * static_cast<std::int64_t>(index);
    RequireType(TypeAt(offset), expectedType, offset, offset + physPage, label + " direct memory type");
}

const Case mtypeprotectPage{"Mtypeprotect_PartialPage_RetypesOnlyThatPage", [] {
    const TypedMapping mapping;
    RequirePage(mapping, 0, 0, 3);
    RequirePage(mapping, 1, 3, 1);
    RequirePage(mapping, 2, 0, 3);
}};

const Case batchTypeProtectPage{"BatchMap2_TypeProtectOperation_RetypesEntryPage", [] {
    const TypedMapping mapping;
    KernelBatchMapEntry entry{};
    entry.start = At(mapping.bytes, page * 2);
    entry.length = page;
    entry.protection = 3;
    entry.type = 5;
    entry.operation = batchTypeProtect;
    int processed = -1;
    RequireEqual(sceKernelBatchMap2(&entry, 1, &processed, 0), 0, "type protect batch");
    RequireEqual(processed, 1, "processed");
    RequirePage(mapping, 0, 0, 3);
    RequirePage(mapping, 1, 3, 1);
    RequirePage(mapping, 2, 5, 3);
}};

const Case mtypeprotectWhole{"Mtypeprotect_WholeRange_AppliesOneTypeAndProtection", [] {
    const TypedMapping mapping;
    RequireEqual(sceKernelMtypeprotect(mapping.bytes, page * 3, 2, 3), 0, "retype the whole range");
    const VirtualQueryInfo whole = Query(mapping.bytes, "query the range");
    RequireEqual(whole.memory_type, 2, "memory type");
    RequireEqual(whole.protection, 3, "protection");
    const DirectType type = TypeAt(mapping.phys + physPage * 2);
    RequireEqual(type.result, 0, "direct memory type result");
    RequireEqual(type.type, 2, "direct memory type");
    RequireEqual(sceKernelMunmap(mapping.bytes, page * 3), 0, "unmap");
    RequireEqual(sceKernelReleaseDirectMemory(mapping.phys, page * 3), 0, "release");
}};

const Case gpuProtection{"MapDirectMemory_GpuProtectionBits_AreCpuWritable", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page, "allocate");
    void* mapping = MapDirect(mappings, phys, page, "map with protection 0x3f2", 0x3f2);
    StoreByte(mapping, 0, 11);
    RequireEqual(ByteAt(mapping, 0), 11, "contents");
    RequireEqual(sceKernelMunmap(mapping, page), 0, "unmap");
    RequireEqual(sceKernelReleaseDirectMemory(phys, page), 0, "release");
}};

void* ReserveFixed(Mappings& mappings, void* requested, std::size_t bytes, int flags, std::string_view label) {
    void* reserved = requested;
    RequireEqual(mappings.Track(sceKernelReserveVirtualRange(&reserved, bytes, flags, 0), &reserved, bytes), 0, label);
    Require(reserved == requested, std::string(label) + " lands at the requested address");
    return reserved;
}

const Case fixedReservation{"ReserveVirtualRange_FixedFreeAddress_ReservesThereRepeatedly", [] {
    void* const requested = At(UnusedRange(page * 4), page);
    Mappings mappings;
    ReserveFixed(mappings, requested, page * 2, 0x400010, "first fixed reservation");
    ReserveFixed(mappings, requested, page * 2, 0x400010, "second fixed reservation");
}};

const Case reservationOverMapping{"ReserveVirtualRange_FixedOverDirectMapping_ReplacesMappingWithReservation", [] {
    void* const requested = At(UnusedRange(page * 4), page);
    DirectAllocations direct;
    Mappings mappings;
    ReserveFixed(mappings, requested, page * 2, 0x400010, "fixed reservation");
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    void* mapped = MapDirectAt(mappings, requested, phys, page * 2, fixedFlag, "map into the reservation");
    StoreByte(mapped, 0, 11);
    Require(Query(mapped, "query the mapping").is_direct, "mapping is direct");
    void* reserved = ReserveFixed(mappings, requested, page * 2, fixedFlag, "reserve over the mapping");
    const VirtualQueryInfo after = Query(reserved, "query the reservation");
    Require(!after.is_committed && !after.is_direct, "reservation is neither committed nor direct");
    RequireEqual(after.protection, 0, "reservation protection");
}};

const Case remapReplacedReservation{"MapDirectMemory_FixedIntoReplacedReservation_MapsAgain", [] {
    void* const requested = At(UnusedRange(page * 4), page);
    DirectAllocations direct;
    Mappings mappings;
    ReserveFixed(mappings, requested, page * 2, 0x400010, "fixed reservation");
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    MapDirectAt(mappings, requested, phys, page * 2, fixedFlag, "map into the reservation");
    ReserveFixed(mappings, requested, page * 2, fixedFlag, "reserve over the mapping");
    void* remapped = MapDirectAt(mappings, requested, phys, page * 2, fixedFlag, "map into the replaced reservation");
    Require(Query(remapped, "query the remapped range").is_direct, "remapped range is direct");
}};

const Case reservationNoOverwrite{"ReserveVirtualRange_FixedNoOverwriteOnLiveMapping_Throws", [] {
    void* const requested = At(UnusedRange(page * 4), page);
    DirectAllocations direct;
    Mappings mappings;
    ReserveFixed(mappings, requested, page * 2, 0x400010, "fixed reservation");
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    MapDirectAt(mappings, requested, phys, page * 2, fixedFlag, "map into the reservation");
    void* reserved = ReserveFixed(mappings, requested, page * 2, fixedFlag, "reserve over the mapping");
    MapDirectAt(mappings, requested, phys, page * 2, fixedFlag, "map into the replaced reservation");
    RequireAnyStdException([&] {
        mappings.Track(sceKernelReserveVirtualRange(&reserved, page * 2, fixedNoOverwrite, 0), &reserved, page * 2);
    }, "fixed no-overwrite reservation over a live mapping");
    RequireEqual(sceKernelMunmap(reserved, page * 2), 0, "unmap");
    RequireEqual(sceKernelReleaseDirectMemory(phys, page * 2), 0, "release");
}};

const Case poolReserve{"MemoryPoolReserve_FixedFreeAddress_ReservesThere", [] {
    void* const requested = At(UnusedRange(page * 4), page);
    Mappings mappings;
    void* pooled = nullptr;
    RequireEqual(mappings.Track(sceKernelMemoryPoolReserve(requested, page * 2, 0, fixedFlag, &pooled), &pooled, page * 2), 0,
                 "pool reservation");
    Require(pooled == requested, "pool reservation lands at the requested address");
    RequireEqual(sceKernelMunmap(pooled, page * 2), 0, "unmap");
}};

const Case reservedNotCommitted{"ReserveVirtualRange_NewRange_IsNotCommitted", [] {
    Mappings mappings;
    void* reserved = Reserve(mappings, page * 2, "reserve two pages");
    const auto start = Address(reserved);
    const VirtualQueryInfo info = Query(reserved, "query the reservation");
    Require(!info.is_committed && !info.is_direct && !info.is_flexible, "reservation is uncommitted");
    RequireEqual(info.protection, 0, "protection");
    RequireEqual(info.start, start, "start");
    RequireEqual(info.end, start + page * 2, "end");
}};

const Case halfCommitted{"MapDirectMemory_FixedIntoFirstHalfOfReservation_CommitsOnlyThatHalf", [] {
    DirectAllocations direct;
    Mappings mappings;
    void* reserved = Reserve(mappings, page * 2, "reserve two pages");
    const auto start = Address(reserved);
    const std::int64_t phys = direct.Allocate(page, "allocate");
    void* fixed = MapDirectAt(mappings, reserved, phys, page, fixedFlag, "map into the first half");
    StoreByte(fixed, 0, 7);
    const VirtualQueryInfo head = Query(reserved, "query the first half");
    Require(head.is_committed && head.is_direct, "first half is committed direct memory");
    RequireEqual(head.protection, 3, "first half protection");
    RequireEqual(head.start, start, "first half start");
    RequireEqual(head.end, start + page, "first half end");
    const VirtualQueryInfo tail = Query(At(reserved, page), "query the second half");
    Require(!tail.is_committed, "second half is uncommitted");
    RequireEqual(tail.protection, 0, "second half protection");
    RequireEqual(tail.start, start + page, "second half start");
    RequireEqual(tail.end, start + page * 2, "second half end");
    RequireEqual(sceKernelMunmap(reserved, page * 2), 0, "unmap");
    RequireEqual(sceKernelReleaseDirectMemory(phys, page), 0, "release");
}};

const Case directNoOverwrite{"MapDirectMemory_FixedNoOverwriteOnLiveMapping_FailsWithEnomem", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    void* mapped = MapDirect(mappings, phys, page, "map the first page");
    StoreByte(mapped, 0, 13);
    void* again = mapped;
    RequireEqual(mappings.Track(sceKernelMapDirectMemory(&again, page, 3, fixedNoOverwrite, phys + physPage, 0), &again, page),
                 SCE_KERNEL_ERROR_ENOMEM, "fixed no-overwrite direct mapping");
    Require(again == mapped, "address left unchanged");
}};

const Case flexibleNoOverwriteDirect{"MapFlexibleMemory_FixedNoOverwriteOnLiveDirectMapping_FailsAndKeepsMapping", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    void* mapped = MapDirect(mappings, phys, page, "map the first page");
    StoreByte(mapped, 0, 13);
    void* flexible = mapped;
    RequireEqual(mappings.Track(sceKernelMapFlexibleMemory(&flexible, page, 3, fixedNoOverwrite), &flexible, page), SCE_KERNEL_ERROR_ENOMEM,
                 "fixed no-overwrite flexible mapping");
    RequireEqual(ByteAt(mapped, 0), 13, "contents kept");
    const VirtualQueryInfo info = Query(mapped, "query the direct mapping");
    Require(info.is_direct, "still direct");
    RequireEqual(info.offset, static_cast<std::uint64_t>(phys), "offset");
    RequireEqual(sceKernelMunmap(mapped, page), 0, "unmap");
    RequireEqual(sceKernelReleaseDirectMemory(phys, page * 2), 0, "release");
}};

#ifdef _WIN32
class ArenaCommit {
public:
    ArenaCommit(void* pointer, std::size_t bytes) : pointer(pointer), bytes(bytes) {
        GuestArena::GuestArenaCommit_nid_postfix(pointer, bytes, PAGE_READWRITE, bytes);
    }

    ~ArenaCommit() {
        try {
            GuestArena::GuestArenaReset_nid_postfix(pointer, bytes);
        } catch (...) {
        }
    }

    ArenaCommit(const ArenaCommit&) = delete;
    ArenaCommit& operator=(const ArenaCommit&) = delete;

private:
    void* pointer;
    std::size_t bytes;
};

const Case hostOccupied{"MapDirectMemory_FixedNoOverwriteOnHostCommittedPages_IsRejected", [] {
    void* target = At(UnusedRange(page * 4), page);
    const ArenaCommit commit(target, page);
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page, "allocate");
    void* fixed = target;
    bool rejected = false;
    try {
        rejected = mappings.Track(sceKernelMapDirectMemory(&fixed, page, 3, fixedNoOverwrite, phys, 0), &fixed, page) != 0;
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected, "mapping over host committed pages is rejected");
}};

constexpr std::uintptr_t applicationAreaEnd = 0xFC00000000ull;

const Case arenaRange{"GuestArena_Range_EndsAtApplicationAreaEnd", [] {
    std::uintptr_t base = 0;
    std::size_t size = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &size);
    RequireEqual(base, std::uintptr_t{0x200000000ull}, "arena base");
    RequireEqual(base + size, applicationAreaEnd, "arena end");
}};

const Case applicationAreaEndMapping{"MapDirectMemory_FixedAtApplicationAreaEnd_MapsLastPages", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    void* mapped = MapDirectAt(mappings, reinterpret_cast<void*>(applicationAreaEnd - page * 2), phys, page * 2, fixedNoOverwrite,
                               "map the last two pages");
    StoreByte(mapped, page * 2 - 1, 7);
    void* alias = MapDirect(mappings, phys + physPage, page, "map an alias of the last page");
    RequireEqual(ByteAt(alias, page - 1), 7, "alias sees the write");
    const VirtualQueryInfo info = Query(mapped, "query the mapping");
    Require(info.is_direct, "direct");
    RequireEqual(info.end, applicationAreaEnd, "mapping end");
    RequireEqual(sceKernelMunmap(alias, page), 0, "unmap the alias");
    RequireEqual(sceKernelMunmap(mapped, page * 2), 0, "unmap the mapping");
    RequireEqual(sceKernelReleaseDirectMemory(phys, page * 2), 0, "release");
}};

const Case beyondApplicationArea{"MapDirectMemory_FixedBeyondApplicationAreaEnd_IsRefused", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    MapDirectAt(mappings, reinterpret_cast<void*>(applicationAreaEnd - page * 2), phys, page * 2, fixedNoOverwrite, "map the last two pages");
    void* beyond = reinterpret_cast<void*>(applicationAreaEnd);
    bool refused = false;
    try {
        refused = mappings.Track(sceKernelMapDirectMemory(&beyond, page, 3, fixedFlag, phys, 0), &beyond, page) != 0;
    } catch (const std::exception&) {
        refused = true;
    }
    Require(refused, "mapping beyond the application area is refused");
    RequireEqual(Address(beyond), applicationAreaEnd, "address left unchanged");
}};
#endif

#ifdef _WIN32
constexpr std::size_t mlockLength = 0x400000;
#else
constexpr std::size_t mlockLength = 0x10000;
#endif

#if defined(__linux__)
std::size_t LockedKilobytes() {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmLck:", 0) == 0) return std::strtoull(line.c_str() + 6, nullptr, 10);
    }
    return 0;
}
#endif

const Case mlockZero{"Mlock_ZeroLength_Succeeds", [] {
    Mappings mappings;
    void* mapped = MapFlexible(mappings, mlockLength, "map");
    RequireEqual(sceKernelMlock_nid_postfix(mapped, 0), 0, "zero length");
}};

const Case mlockUnaligned{"Mlock_UnalignedRange_LocksEnclosingGuestPages", [] {
    Mappings mappings;
    void* mapped = MapFlexible(mappings, mlockLength, "map");
#if defined(__linux__)
    const auto lockedBefore = LockedKilobytes();
#endif
    RequireEqual(sceKernelMlock_nid_postfix(At(mapped, 1), mlockLength - page), 0, "lock an unaligned range");
#ifdef _WIN32
    SIZE_T minimum = 0;
    SIZE_T maximum = 0;
    DWORD limits = 0;
    Require(GetProcessWorkingSetSizeEx(GetCurrentProcess(), &minimum, &maximum, &limits), "query the working set");
    Require(minimum >= mlockLength, "working set minimum covers the locked range");
    Require(maximum > minimum, "working set maximum above minimum");
    Require(VirtualUnlock(mapped, mlockLength), "whole range was locked");
    const BOOL unlockedAgain = VirtualUnlock(mapped, mlockLength);
    const DWORD error = GetLastError();
    Require(!unlockedAgain, "second unlock fails");
    RequireEqual(error, static_cast<DWORD>(ERROR_NOT_LOCKED), "second unlock error");
#elif defined(__linux__)
    RequireEqual(LockedKilobytes() - lockedBefore, mlockLength / 1024, "locked kilobytes");
#endif
}};

const Case mlockTwice{"Mlock_LockedRange_CanBeLockedAgain", [] {
    Mappings mappings;
    void* mapped = MapFlexible(mappings, mlockLength, "map");
    RequireEqual(sceKernelMlock_nid_postfix(mapped, mlockLength), 0, "first lock");
    RequireEqual(sceKernelMlock_nid_postfix(mapped, mlockLength), 0, "second lock");
    StoreByte(mapped, mlockLength - 1, 7);
}};

const Case mlockOverflow{"Mlock_RangeWrappingAddressSpace_FailsWithEinval", [] {
    void* top = reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - page + 1);
    RequireEqual(sceKernelMlock_nid_postfix(top, page * 2), SCE_KERNEL_ERROR_EINVAL, "wrapping range");
}};

const Case mlockReserved{"Mlock_ReservedRange_FailsWithEnomem", [] {
    Mappings mappings;
    void* reserved = Reserve(mappings, page, "reserve");
    RequireEqual(sceKernelMlock_nid_postfix(reserved, page), SCE_KERNEL_ERROR_ENOMEM, "reserved range");
    RequireEqual(sceKernelMunmap(reserved, page), 0, "unmap the reservation");
}};

const Case mlockUnmapped{"Mlock_UnmappedRange_FailsWithEnomem", [] {
    Mappings mappings;
    void* mapped = MapFlexible(mappings, mlockLength, "map");
    RequireEqual(sceKernelMunmap(mapped, mlockLength), 0, "unmap");
    RequireEqual(sceKernelMlock_nid_postfix(mapped, page), SCE_KERNEL_ERROR_ENOMEM, "unmapped range");
}};

class SharedViews {
public:
    explicit SharedViews(std::size_t pages) : bytes(page * pages) {
        phys = direct.Allocate(bytes, "allocate");
        first = MapDirect(mappings, phys, bytes, "map the first view");
        second = MapDirect(mappings, phys, bytes, "map the second view");
    }

    DirectAllocations direct;
    Mappings mappings;
    std::size_t bytes;
    std::int64_t phys = 0;
    void* first = nullptr;
    void* second = nullptr;
};

const Case twoViews{"MapDirectMemory_TwoViewsOfSameBlock_ShareContents", [] {
    const SharedViews views(3);
    const VirtualQueryInfo info = Query(views.second, "query the second view");
    Require(info.is_direct && !info.is_flexible, "second view is direct memory");
    RequireEqual(info.offset, static_cast<std::uint64_t>(views.phys), "second view offset");
    StoreByte(views.first, 0, 31);
    StoreByte(views.second, page, 47);
    StoreByte(views.first, page * 2, 63);
    RequireEqual(ByteAt(views.second, 0), 31, "second view sees page 0");
    RequireEqual(ByteAt(views.first, page), 47, "first view sees page 1");
    RequireEqual(ByteAt(views.second, page * 2), 63, "second view sees page 2");
}};

const Case noAccessView{"MapDirectMemory_NoAccessView_ReadsSharedContentsAfterMprotect", [] {
    SharedViews views(3);
    void* inaccessible = MapDirect(views.mappings, views.phys + physPage, page, "map a no-access view", 0);
    StoreByte(views.first, page, 48);
    RequireEqual(sceKernelMprotect(inaccessible, page, 1), 0, "make the view readable");
    RequireEqual(ByteAt(inaccessible, 0), 48, "view sees the write");
    RequireEqual(sceKernelMunmap(inaccessible, page), 0, "unmap the view");
}};

const Case unmapViewPage{"Munmap_PageOfOneView_KeepsOtherViewAndFixedRemapRestoresIt", [] {
    SharedViews views(3);
    StoreByte(views.first, 0, 31);
    StoreByte(views.first, page * 2, 63);
    RequireEqual(sceKernelMunmap(At(views.first, page), page), 0, "unmap the middle of the first view");
    StoreByte(views.second, page, 79);
    RequireEqual(ByteAt(views.first, 0), 31, "first view page 0 kept");
    RequireEqual(ByteAt(views.first, page * 2), 63, "first view page 2 kept");
    void* middle = MapDirectAt(views.mappings, At(views.first, page), views.phys + physPage, page, fixedFlag, "remap the middle page");
    RequireEqual(ByteAt(views.first, page), 79, "remapped page shows shared contents");
    const VirtualQueryInfo info = Query(middle, "query the remapped page");
    RequireEqual(info.offset, static_cast<std::uint64_t>(views.phys) + page, "remapped offset");
    RequireEqual(info.start, Address(middle), "remapped start");
    RequireEqual(sceKernelMunmap(views.first, page), 0, "unmap first view page 0");
    RequireEqual(sceKernelMunmap(At(views.first, page * 2), page), 0, "unmap first view page 2");
    RequireEqual(sceKernelMunmap(middle, page), 0, "unmap the remapped page");
}};

const Case noAccessOtherView{"Mprotect_NoAccessOnOneView_KeepsWritesVisibleThroughIt", [] {
    const SharedViews views(3);
    RequireEqual(sceKernelMprotect(views.second, page * 3, 0), 0, "remove access from the second view");
    StoreByte(views.first, page, 95);
    RequireEqual(sceKernelMprotect(views.second, page * 3, 1), 0, "make the second view readable");
    RequireEqual(ByteAt(views.second, page), 95, "second view sees the write");
    RequireEqual(sceKernelMprotect(views.second, page * 3, 3), 0, "make the second view writable");
}};

const Case fixedSharedView{"MapDirectMemory_FixedIntoReservation_SharesPhysicalPage", [] {
    SharedViews views(3);
    StoreByte(views.second, page * 2, 111);
    void* reserved = Reserve(views.mappings, page * 3, "reserve three pages");
    void* fixed = MapDirectAt(views.mappings, At(reserved, page), views.phys + physPage * 2, page, fixedFlag, "map into the reservation");
    RequireEqual(ByteAt(fixed, 0), 111, "fixed view sees contents");
    StoreByte(fixed, 0, 127);
    RequireEqual(ByteAt(views.second, page * 2), 127, "second view sees the fixed view write");
}};

const Case flexibleOverView{"MapFlexibleMemory_FixedOverDirectView_DetachesFromPhysicalPage", [] {
    SharedViews views(3);
    StoreByte(views.second, page * 2, 111);
    void* reserved = Reserve(views.mappings, page * 3, "reserve three pages");
    void* fixed = MapDirectAt(views.mappings, At(reserved, page), views.phys + physPage * 2, page, fixedFlag, "map into the reservation");
    StoreByte(fixed, 0, 127);
    MapFlexibleAt(views.mappings, fixed, page, fixedFlag, "map flexible memory over the view");
    RequireEqual(ByteAt(fixed, 0), 0, "flexible page is zeroed");
    const VirtualQueryInfo info = Query(fixed, "query the flexible page");
    Require(!info.is_direct && info.is_flexible, "page is flexible memory");
    RequireEqual(info.offset, std::uint64_t{0}, "flexible offset");
    StoreByte(fixed, 0, 143);
    RequireEqual(ByteAt(views.second, page * 2), 127, "physical page unaffected");
    RequireEqual(sceKernelMunmap(reserved, page * 3), 0, "unmap the reservation");
    RequireEqual(sceKernelMunmap(views.second, page * 3), 0, "unmap the second view");
}};

const Case releaseMiddlePage{"ReleaseDirectMemory_MiddlePageThenReallocate_MapsZeroedMiddlePage", [] {
    SharedViews views(3);
    StoreByte(views.first, 0, 31);
    StoreByte(views.first, page, 79);
    StoreByte(views.first, page * 2, 127);
    RequireEqual(sceKernelMunmap(views.first, page * 3), 0, "unmap the first view");
    RequireEqual(sceKernelMunmap(views.second, page * 3), 0, "unmap the second view");
    RequireEqual(sceKernelReleaseDirectMemory(views.phys + physPage, page), 0, "release the middle page");
    const std::int64_t replacement = views.direct.AllocateIn(views.phys + physPage, views.phys + physPage * 2, page, "reallocate the middle page");
    RequireEqual(replacement, views.phys + physPage, "replacement offset");
    void* mixed = MapDirect(views.mappings, views.phys, page * 3, "map the whole block");
    RequireEqual(ByteAt(mixed, 0), 31, "page 0 kept");
    RequireEqual(ByteAt(mixed, page), 0, "page 1 zeroed");
    RequireEqual(ByteAt(mixed, page * 2), 127, "page 2 kept");
    RequireEqual(sceKernelMunmap(mixed, page * 3), 0, "unmap");
    RequireEqual(sceKernelReleaseDirectMemory(views.phys, page * 3), 0, "release");
}};

class HeapBlocks {
public:
    HeapBlocks() = default;

    ~HeapBlocks() {
        for (const auto& block : blocks) GuestHeap::GuestHeapFree_nid_postfix(block.first);
    }

    HeapBlocks(const HeapBlocks&) = delete;
    HeapBlocks& operator=(const HeapBlocks&) = delete;

    unsigned char* Add(void* pointer, std::size_t bytes) {
        if (pointer != nullptr) blocks.emplace_back(static_cast<unsigned char*>(pointer), bytes);
        Require(pointer != nullptr, "heap allocation of " + std::to_string(bytes) + " bytes");
        return static_cast<unsigned char*>(pointer);
    }

    void Free(unsigned char* pointer) {
        const auto found = std::find_if(blocks.begin(), blocks.end(), [pointer](const auto& block) { return block.first == pointer; });
        if (found != blocks.end()) blocks.erase(found);
        GuestHeap::GuestHeapFree_nid_postfix(pointer);
    }

    const std::vector<std::pair<unsigned char*, std::size_t>>& Blocks() const noexcept { return blocks; }

private:
    std::vector<std::pair<unsigned char*, std::size_t>> blocks;
};

void RequireFilled(const unsigned char* pointer, std::size_t bytes, unsigned char value, const std::string& label) {
    const auto mismatch = std::find_if(pointer, pointer + bytes, [value](unsigned char byte) { return byte != value; });
    if (mismatch == pointer + bytes) return;
    Fail(label + ": byte " + std::to_string(mismatch - pointer) + " is " + std::to_string(*mismatch) + ", expected " + std::to_string(value));
}

const Case heapAlignment{"GuestHeap_Allocations_Are32ByteAlignedAndKeepContents", [] {
    HeapBlocks heap;
    for (std::size_t bytes = 1; bytes <= 600; ++bytes) heap.Add(GuestHeap::GuestHeapAllocate_nid_postfix(bytes), bytes);
    for (const std::size_t bytes : {std::size_t{4000}, std::size_t{70000}, std::size_t{0x30000}}) {
        heap.Add(GuestHeap::GuestHeapAllocate_nid_postfix(bytes), bytes);
    }
    void* initial = GuestHeap::GuestHeapAllocate_nid_postfix(8);
    Require(initial != nullptr, "allocate 8 bytes");
    void* grown = GuestHeap::GuestHeapReallocate_nid_postfix(initial, 333);
    heap.Add(grown != nullptr ? grown : initial, 333);
    const auto& blocks = heap.Blocks();
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        const auto [pointer, bytes] = blocks[index];
        RequireEqual(Address(pointer) & 31u, std::uintptr_t{0}, "alignment of block " + std::to_string(index) + " (" + std::to_string(bytes) + " bytes)");
        std::memset(pointer, static_cast<int>(index & 0xffu), bytes);
    }
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        const auto [pointer, bytes] = blocks[index];
        RequireFilled(pointer, bytes, static_cast<unsigned char>(index & 0xffu),
                      "contents of block " + std::to_string(index) + " (" + std::to_string(bytes) + " bytes)");
    }
}};

const Case heapReuse{"GuestHeap_AllocationAfterFreeingLargeBlock_IsUsable", [] {
    constexpr std::size_t bytes = 0x30000;
    HeapBlocks heap;
    unsigned char* pointer = heap.Add(GuestHeap::GuestHeapAllocate_nid_postfix(bytes), bytes);
    std::memset(pointer, 0x5a, bytes);
    RequireEqual(static_cast<int>(pointer[0]), 0x5a, "first allocation first byte");
    RequireEqual(static_cast<int>(pointer[bytes - 1]), 0x5a, "first allocation last byte");
    heap.Free(pointer);
    pointer = heap.Add(GuestHeap::GuestHeapAllocate_nid_postfix(bytes), bytes);
    std::memset(pointer, 0xa5, bytes);
    RequireEqual(static_cast<int>(pointer[0]), 0xa5, "second allocation first byte");
    RequireEqual(static_cast<int>(pointer[bytes - 1]), 0xa5, "second allocation last byte");
    heap.Free(pointer);
}};

#ifdef _WIN32
std::size_t CollectWrites(void* address, std::size_t bytes, bool clear = true) {
    std::array<void*, 32> pages{};
    std::size_t count = pages.size();
    Require(GuestArena::GuestArenaCollectWrites_nid_postfix(Address(address), bytes, pages.data(), &count, clear), "collect writes");
    return count;
}

bool TryCollectWrites(void* address, std::size_t bytes, std::size_t& count) {
    std::array<void*, 32> pages{};
    count = pages.size();
    return GuestArena::GuestArenaCollectWrites_nid_postfix(Address(address), bytes, pages.data(), &count, true);
}

DWORD HostProtection(void* address) {
    MEMORY_BASIC_INFORMATION info{};
    RequireEqual(VirtualQuery(address, &info, sizeof(info)), sizeof(info), "query host protection");
    return info.Protect;
}

const Case newViewsWrites{"CollectWrites_NewDirectViews_ReportEveryPageOnce", [] {
    const SharedViews views(3);
    RequireEqual(CollectWrites(views.first, page * 3), std::size_t{12}, "first view initial pages");
    RequireEqual(CollectWrites(views.second, page * 3), std::size_t{12}, "second view initial pages");
    RequireEqual(CollectWrites(views.first, page * 3), std::size_t{0}, "first view after clearing");
    RequireEqual(CollectWrites(views.second, page * 3), std::size_t{0}, "second view after clearing");
}};

const Case writeThroughView{"CollectWrites_WriteThroughOneView_IsReportedForEveryView", [] {
    const SharedViews views(3);
    CollectWrites(views.first, page * 3);
    CollectWrites(views.second, page * 3);
    StoreByte(views.first, page + 5, 21);
    RequireEqual(ByteAt(views.second, page + 5), 21, "second view sees the write");
    RequireEqual(CollectWrites(views.first, page * 3, false), std::size_t{4}, "peek first view");
    RequireEqual(CollectWrites(views.first, page * 3, false), std::size_t{4}, "peek first view again");
    RequireEqual(CollectWrites(views.first, page * 3), std::size_t{4}, "collect first view");
    RequireEqual(CollectWrites(views.second, page * 3), std::size_t{4}, "collect second view");
    RequireEqual(CollectWrites(views.first, page * 3), std::size_t{0}, "first view after clearing");
    StoreByte(views.second, page * 2, 42);
    RequireEqual(CollectWrites(views.first, page * 3), std::size_t{4}, "first view after second view write");
    RequireEqual(CollectWrites(views.second, page * 3), std::size_t{4}, "second view after its write");
    RequireEqual(CollectWrites(views.second, page * 3), std::size_t{0}, "second view after clearing");
}};

const Case readOnlyViewWrites{"CollectWrites_ReadOnlyView_StillTracksWritesThroughOtherView", [] {
    const SharedViews views(3);
    CollectWrites(views.first, page * 3);
    CollectWrites(views.second, page * 3);
    RequireEqual(sceKernelMprotect(views.first, page * 3, 1), 0, "make the first view read-only");
    CollectWrites(views.first, page * 3);
    CollectWrites(views.second, page * 3);
    StoreByte(views.second, 0, 63);
    RequireEqual(ByteAt(views.first, 0), 63, "read-only view sees the write");
    RequireEqual(CollectWrites(views.first, page * 3), std::size_t{4}, "read-only view reports the write");
    RequireEqual(sceKernelMprotect(views.first, page * 3, 3), 0, "make the first view writable");
    CollectWrites(views.first, page * 3);
    StoreByte(views.first, 0, 84);
    RequireEqual(ByteAt(views.second, 0), 84, "second view sees the write");
    Require(CollectWrites(views.second, page * 3) != 0, "second view reports the write");
}};

const Case thirdAliasWrites{"CollectWrites_WriteThroughThirdAlias_IsReportedForEveryView", [] {
    SharedViews views(3);
    void* third = MapDirect(views.mappings, views.phys + physPage, page, "map a third view");
    CollectWrites(views.first, page * 3);
    CollectWrites(views.second, page * 3);
    StoreByte(third, 0, 105);
    RequireEqual(CollectWrites(views.first, page * 3), std::size_t{4}, "first view");
    RequireEqual(CollectWrites(views.second, page * 3), std::size_t{4}, "second view");
    RequireEqual(sceKernelMunmap(third, page), 0, "unmap the third view");
    RequireEqual(sceKernelMunmap(views.first, page * 3), 0, "unmap the first view");
    RequireEqual(sceKernelMunmap(views.second, page * 3), 0, "unmap the second view");
    RequireEqual(sceKernelReleaseDirectMemory(views.phys, page * 3), 0, "release");
}};

class ArenaBlock {
public:
    ArenaBlock(std::size_t bytes, std::size_t alignment)
        : pointer(static_cast<unsigned char*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, alignment))), bytes(bytes) {
        Require(pointer != nullptr, "allocate an arena block");
    }

    ~ArenaBlock() {
        try {
            GuestArena::GuestArenaRelease_nid_postfix(pointer, bytes);
        } catch (...) {
        }
    }

    ArenaBlock(const ArenaBlock&) = delete;
    ArenaBlock& operator=(const ArenaBlock&) = delete;

    unsigned char* Pointer() const noexcept { return pointer; }

private:
    unsigned char* pointer;
    std::size_t bytes;
};

const Case collectGap{"CollectWrites_RangeWithUncommittedPage_FailsAndKeepsWrites", [] {
    const ArenaBlock block(page * 3, page);
    GuestArena::GuestArenaCommit_nid_postfix(block.Pointer(), page, PAGE_READWRITE, page);
    GuestArena::GuestArenaCommit_nid_postfix(block.Pointer() + page * 2, page, PAGE_READWRITE, page);
    std::size_t count = 0;
    Require(TryCollectWrites(block.Pointer(), page, count), "collect the first page");
    Require(TryCollectWrites(block.Pointer() + page * 2, page, count), "collect the last page");
    StoreByte(block.Pointer(), 8, 1);
    Require(!TryCollectWrites(block.Pointer(), page * 3, count), "collect across the uncommitted page fails");
    Require(TryCollectWrites(block.Pointer(), page, count), "collect the first page again");
    RequireEqual(count, std::size_t{1}, "write kept after the failed collect");
}};

const Case collectNoAccess{"CollectWrites_RangeWithNoAccessPage_FailsAndKeepsWrites", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 2, "allocate");
    void* mapped = MapDirect(mappings, phys, page * 2, "map");
    std::size_t count = 0;
    Require(TryCollectWrites(mapped, page * 2, count), "initial collect");
    StoreByte(mapped, 8, 2);
    RequireEqual(sceKernelMprotect(At(mapped, page), page, 0), 0, "remove access from the second page");
    Require(!TryCollectWrites(mapped, page * 2, count), "collect across the no-access page fails");
    Require(TryCollectWrites(mapped, page, count), "collect the first page");
    RequireEqual(count, std::size_t{4}, "write kept after the failed collect");
    RequireEqual(sceKernelMunmap(mapped, page * 2), 0, "unmap");
    RequireEqual(sceKernelReleaseDirectMemory(phys, page * 2), 0, "release");
}};

class Pin {
public:
    Pin(void* pointer, std::size_t bytes) : pointer(pointer), bytes(bytes) {
        GuestArena::GuestArenaPinWritable_nid_postfix(pointer, bytes);
    }

    ~Pin() {
        if (pointer == nullptr) return;
        try {
            GuestArena::GuestArenaUnpinWritable_nid_postfix(pointer, bytes);
        } catch (...) {
        }
    }

    Pin(const Pin&) = delete;
    Pin& operator=(const Pin&) = delete;

    void Unpin() {
        GuestArena::GuestArenaUnpinWritable_nid_postfix(std::exchange(pointer, nullptr), bytes);
    }

private:
    void* pointer;
    std::size_t bytes;
};

const Case clearedArmed{"CollectWrites_ClearedSharedMapping_IsReadOnlyUntilWritten", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 3, "allocate");
    void* mapped = MapDirect(mappings, phys, page * 3, "map");
    CollectWrites(mapped, page * 3);
    RequireEqual(CollectWrites(mapped, page * 3), std::size_t{0}, "nothing written");
    RequireEqual(HostProtection(mapped), static_cast<DWORD>(PAGE_READONLY), "page 0 armed");
    RequireEqual(HostProtection(At(mapped, page)), static_cast<DWORD>(PAGE_READONLY), "page 1 armed");
}};

const Case pinnedPage{"PinWritable_SharedPage_StaysWritableAndIsAlwaysReported", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 3, "allocate");
    void* mapped = MapDirect(mappings, phys, page * 3, "map");
    CollectWrites(mapped, page * 3);
    CollectWrites(mapped, page * 3);
    const Pin pin(At(mapped, page), page);
    RequireEqual(HostProtection(At(mapped, page)), static_cast<DWORD>(PAGE_READWRITE), "pinned page writable");
    RequireEqual(HostProtection(mapped), static_cast<DWORD>(PAGE_READONLY), "page 0 armed");
    RequireEqual(HostProtection(At(mapped, page * 2)), static_cast<DWORD>(PAGE_READONLY), "page 2 armed");
    RequireEqual(CollectWrites(mapped, page * 3), std::size_t{4}, "pinned page reported");
    RequireEqual(CollectWrites(mapped, page * 3), std::size_t{4}, "pinned page reported again");
    RequireEqual(HostProtection(At(mapped, page)), static_cast<DWORD>(PAGE_READWRITE), "pinned page still writable");
    StoreByte(mapped, page + 8, 7);
    StoreByte(mapped, 0, 9);
    RequireEqual(CollectWrites(mapped, page * 3), std::size_t{8}, "pinned page and written page");
    RequireEqual(CollectWrites(mapped, page * 3), std::size_t{4}, "only the pinned page");
    RequireEqual(HostProtection(mapped), static_cast<DWORD>(PAGE_READONLY), "page 0 rearmed");
    RequireEqual(HostProtection(At(mapped, page)), static_cast<DWORD>(PAGE_READWRITE), "pinned page writable after collect");
}};

const Case unpinnedPage{"UnpinWritable_SharedPage_RearmsWriteTracking", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 3, "allocate");
    void* mapped = MapDirect(mappings, phys, page * 3, "map");
    CollectWrites(mapped, page * 3);
    Pin pin(At(mapped, page), page);
    StoreByte(mapped, page + 8, 7);
    CollectWrites(mapped, page * 3);
    pin.Unpin();
    CollectWrites(mapped, page * 3);
    RequireEqual(CollectWrites(mapped, page * 3), std::size_t{0}, "nothing reported after unpinning");
    RequireEqual(HostProtection(At(mapped, page)), static_cast<DWORD>(PAGE_READONLY), "unpinned page armed");
    StoreByte(mapped, page + 8, 11);
    RequireEqual(CollectWrites(mapped, page * 3), std::size_t{4}, "write to the unpinned page");
    RequireEqual(sceKernelMunmap(mapped, page * 3), 0, "unmap");
    RequireEqual(sceKernelReleaseDirectMemory(phys, page * 3), 0, "release");
}};

class SeedFile {
public:
    SeedFile()
        : path("anyps5-tracked-read-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin"),
          contents(page * 2) {
        for (std::size_t i = 0; i < contents.size(); ++i) contents[i] = static_cast<char>(i * 7 + 1);
        std::ofstream stream(path, std::ios::binary);
        stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        Require(static_cast<bool>(stream), "write the seed file");
    }

    ~SeedFile() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }

    SeedFile(const SeedFile&) = delete;
    SeedFile& operator=(const SeedFile&) = delete;

    const std::filesystem::path path;
    std::vector<char> contents;
};

class KernelFile {
public:
    explicit KernelFile(const std::filesystem::path& path) : descriptor(sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDONLY, 0)) {
        Require(descriptor >= 0, "open " + path.string());
    }

    ~KernelFile() {
        if (descriptor < 0) return;
        try {
            sceKernelClose(descriptor);
        } catch (...) {
        }
    }

    KernelFile(const KernelFile&) = delete;
    KernelFile& operator=(const KernelFile&) = delete;

    int Descriptor() const noexcept { return descriptor; }

    int Close() {
        return sceKernelClose(std::exchange(descriptor, -1));
    }

private:
    int descriptor;
};

class AioRequest {
public:
    explicit AioRequest(std::int32_t id) : id(id) {}

    ~AioRequest() {
        if (!active) return;
        std::int32_t deleted = -1;
        try {
            sceKernelAioDeleteRequest(id, &deleted);
        } catch (...) {
        }
    }

    AioRequest(const AioRequest&) = delete;
    AioRequest& operator=(const AioRequest&) = delete;

    int Delete() {
        active = false;
        std::int32_t deleted = -1;
        return sceKernelAioDeleteRequest(id, &deleted);
    }

private:
    std::int32_t id;
    bool active = true;
};

class TrackedRead {
public:
    TrackedRead() {
        phys = direct.Allocate(page * 2, "allocate");
        mapped = MapDirect(mappings, phys, page * 2, "map");
    }

    std::size_t Collect() { return CollectWrites(mapped, page * 2); }

    SeedFile seed;
    DirectAllocations direct;
    Mappings mappings;
    std::int64_t phys = 0;
    void* mapped = nullptr;
};

const Case preadTracked{"Pread_IntoTrackedDirectMemory_ReportsEveryWrittenPage", [] {
    TrackedRead fixture;
    KernelFile file(fixture.seed.path);
    fixture.Collect();
    RequireEqual(fixture.Collect(), std::size_t{0}, "nothing written before the read");
    RequireEqual(sceKernelPread(file.Descriptor(), fixture.mapped, page * 2, 0), static_cast<std::int64_t>(page * 2), "pread size");
    Require(std::memcmp(fixture.mapped, fixture.seed.contents.data(), page * 2) == 0, "pread contents");
    RequireEqual(fixture.Collect(), std::size_t{8}, "pages written by pread");
    RequireEqual(file.Close(), 0, "close");
    RequireEqual(sceKernelMunmap(fixture.mapped, page * 2), 0, "unmap");
    RequireEqual(sceKernelReleaseDirectMemory(fixture.phys, page * 2), 0, "release");
}};

const Case readTracked{"Read_IntoTrackedDirectMemory_ReportsWrittenPage", [] {
    TrackedRead fixture;
    fixture.Collect();
    KernelFile sequential(fixture.seed.path);
    RequireEqual(sceKernelRead(sequential.Descriptor(), At(fixture.mapped, page), page), static_cast<std::int64_t>(page), "read size");
    RequireEqual(sequential.Close(), 0, "close");
    Require(std::memcmp(At(fixture.mapped, page), fixture.seed.contents.data(), page) == 0, "read contents");
    RequireEqual(fixture.Collect(), std::size_t{4}, "pages written by read");
}};

const Case aioTracked{"AioRead_IntoTrackedDirectMemory_ReportsWrittenPage", [] {
    TrackedRead fixture;
    KernelFile file(fixture.seed.path);
    RequireEqual(sceKernelAioInitializeImpl(nullptr, 0), 0, "initialize aio");
    fixture.Collect();
    KernelAioResult result{-1, 0};
    KernelAioRwRequest request{static_cast<std::int64_t>(page), page, fixture.mapped, &result, file.Descriptor()};
    std::int32_t id = 0;
    std::int32_t state = 0;
    RequireEqual(sceKernelAioSubmitReadCommands(&request, 1, 0, &id), 0, "submit");
    AioRequest submitted(id);
    RequireEqual(sceKernelAioWaitRequest(id, &state, nullptr), 0, "wait");
    RequireEqual(result.state, std::uint32_t{3}, "request state");
    RequireEqual(result.return_value, static_cast<std::int64_t>(page), "bytes read");
    Require(std::memcmp(fixture.mapped, fixture.seed.contents.data() + page, page) == 0, "aio contents");
    RequireEqual(fixture.Collect(), std::size_t{4}, "pages written by aio");
    RequireEqual(submitted.Delete(), 0, "delete the request");
}};

const Case preadReadOnly{"Pread_IntoReadOnlyTrackedMemory_Throws", [] {
    TrackedRead fixture;
    KernelFile file(fixture.seed.path);
    RequireEqual(sceKernelMprotect(fixture.mapped, page * 2, 1), 0, "make the mapping read-only");
    RequireAnyStdException([&] { sceKernelPread(file.Descriptor(), fixture.mapped, page, 0); }, "pread into read-only memory");
}};

const Case aioReadOnly{"AioRead_IntoReadOnlyTrackedMemory_ReportsEfault", [] {
    TrackedRead fixture;
    KernelFile file(fixture.seed.path);
    RequireEqual(sceKernelAioInitializeImpl(nullptr, 0), 0, "initialize aio");
    RequireEqual(sceKernelMprotect(fixture.mapped, page * 2, 1), 0, "make the mapping read-only");
    KernelAioResult refusedResult{-1, 0};
    KernelAioRwRequest refusedRequest{0, page, fixture.mapped, &refusedResult, file.Descriptor()};
    std::int32_t id = 0;
    std::int32_t state = 0;
    RequireEqual(sceKernelAioSubmitReadCommands(&refusedRequest, 1, 0, &id), 0, "submit");
    AioRequest submitted(id);
    RequireEqual(sceKernelAioWaitRequest(id, &state, nullptr), 0, "wait");
    RequireEqual(refusedResult.return_value, static_cast<std::int64_t>(SCE_KERNEL_ERROR_EFAULT), "aio result");
    RequireEqual(submitted.Delete(), 0, "delete the request");
}};
#endif

#if defined(__linux__)
using PageRuns = std::vector<std::pair<std::uintptr_t, std::uintptr_t>>;
constexpr std::size_t small = 4096;

struct RunCollector {
    std::uintptr_t origin;
    PageRuns* into;
};

void AppendRun(void* context, std::uintptr_t begin, std::uintptr_t end) {
    auto& collector = *static_cast<RunCollector*>(context);
    auto& runs = *collector.into;
    const auto first = (begin - collector.origin) / small;
    const auto last = (end - collector.origin) / small;
    if (!runs.empty() && runs.back().second == first) runs.back().second = last;
    else runs.emplace_back(first, last);
}

bool CollectRuns(const volatile void* base, std::size_t offset, std::size_t bytes, PageRuns& runs) {
    runs.clear();
    const auto address = Address(base);
    RunCollector collector{address, &runs};
    return GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(address + offset, bytes, &AppendRun, &collector);
}

std::string DescribeRuns(const PageRuns& runs) {
    std::string text = "{";
    for (const auto& [first, last] : runs) text += " [" + std::to_string(first) + ", " + std::to_string(last) + ")";
    return text + " }";
}

void RequireWritten(const volatile void* base, std::size_t bytes, const PageRuns& expected, const std::string& label) {
    PageRuns runs;
    const bool complete = CollectRuns(base, 0, bytes, runs);
    if (complete && runs == expected) return;
    Fail(label + ": collect " + (complete ? "complete" : "incomplete") + ", written pages " + DescribeRuns(runs) + ", expected " +
         DescribeRuns(expected));
}

void ClearWritten(const volatile void* base, std::size_t bytes) {
    PageRuns ignored;
    CollectRuns(base, 0, bytes, ignored);
}

void RequireWriteWatch() {
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) Testing::Skip("write watch unavailable");
}

bool Covers(const volatile void* address, std::size_t bytes) {
    return GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(Address(address), bytes);
}

constexpr std::size_t watchedLength = 0x100000;

const Case watchCovers{"WriteWatch_FlexibleMapping_CoversExactlyItsRange", [] {
    RequireWriteWatch();
    Mappings mappings;
    void* mapping = MapFlexible(mappings, watchedLength, "map");
    const int local = 0;
    Require(Covers(mapping, watchedLength), "covers the mapping");
    Require(Covers(At(mapping, small), small), "covers a page inside");
    Require(!Covers(mapping, watchedLength + small), "does not cover past the end");
    Require(!Covers(&local, sizeof(local)), "does not cover the stack");
}};

const Case watchFresh{"WriteWatch_NewMapping_ReportsEveryPageOnce", [] {
    RequireWriteWatch();
    Mappings mappings;
    void* mapping = MapFlexible(mappings, watchedLength, "map");
    RequireWritten(mapping, watchedLength, {{0, watchedLength / small}}, "initial collect");
    RequireWritten(mapping, watchedLength, {}, "second collect");
}};

const Case watchSingleWrite{"WriteWatch_SingleWrite_ReportsOnlyThatPageAndReadsNothing", [] {
    RequireWriteWatch();
    Mappings mappings;
    void* mapping = MapFlexible(mappings, watchedLength, "map");
    ClearWritten(mapping, watchedLength);
    StoreByte(mapping, 5 * small + 17, 1);
    RequireWritten(mapping, watchedLength, {{5, 6}}, "after writing page 5");
    RequireWritten(mapping, watchedLength, {}, "after clearing");
    static_cast<void>(ByteAt(mapping, 10 * small));
    RequireWritten(mapping, watchedLength, {}, "after reading page 10");
}};

const Case watchPartialCollect{"WriteWatch_PartialCollect_ClearsOnlyCollectedPages", [] {
    RequireWriteWatch();
    Mappings mappings;
    void* mapping = MapFlexible(mappings, watchedLength, "map");
    ClearWritten(mapping, watchedLength);
    StoreByte(mapping, 7 * small, 1);
    StoreByte(mapping, 9 * small, 1);
    PageRuns runs;
    Require(CollectRuns(mapping, 8 * small, small, runs), "collect page 8");
    Require(runs.empty(), "page 8 clean, got " + DescribeRuns(runs));
    Require(CollectRuns(mapping, 7 * small + 100, 1, runs), "collect one byte of page 7");
    Require(runs == PageRuns{{7, 8}}, "page 7 written, got " + DescribeRuns(runs));
    RequireWritten(mapping, watchedLength, {{9, 10}}, "remaining write");
}};

class Pipe {
public:
    Pipe() { Require(::pipe(ends) == 0, "create a pipe"); }
    ~Pipe() {
        ::close(ends[0]);
        ::close(ends[1]);
    }
    Pipe(const Pipe&) = delete;
    Pipe& operator=(const Pipe&) = delete;

    int Read() const noexcept { return ends[0]; }
    int Write() const noexcept { return ends[1]; }

private:
    int ends[2] = {-1, -1};
};

const Case watchKernelWrite{"WriteWatch_KernelWriteFromPipe_IsReported", [] {
    RequireWriteWatch();
    Mappings mappings;
    void* mapping = MapFlexible(mappings, watchedLength, "map");
    ClearWritten(mapping, watchedLength);
    const Pipe pipe;
    RequireEqual(::write(pipe.Write(), "kernel", 6), ssize_t{6}, "write to the pipe");
    RequireEqual(::read(pipe.Read(), At(mapping, 20 * small + 8), 6), ssize_t{6}, "read from the pipe into the mapping");
    RequireEqual(ByteAt(mapping, 20 * small + 8), static_cast<int>('k'), "read contents");
    RequireWritten(mapping, watchedLength, {{20, 21}}, "kernel write");
}};

const Case watchThreadWrite{"WriteWatch_WriteFromAnotherThread_IsReported", [] {
    RequireWriteWatch();
    Mappings mappings;
    void* mapping = MapFlexible(mappings, watchedLength, "map");
    ClearWritten(mapping, watchedLength);
    std::thread([mapping] { StoreByte(mapping, 30 * small + 5, 3); }).join();
    RequireWritten(mapping, watchedLength, {{30, 31}}, "thread write");
}};

const Case watchMemcpy{"WriteWatch_CopyAcrossPages_ReportsEveryTouchedPage", [] {
    RequireWriteWatch();
    Mappings mappings;
    void* mapping = MapFlexible(mappings, watchedLength, "map");
    ClearWritten(mapping, watchedLength);
    std::vector<unsigned char> source(2 * small, 0xab);
    std::memcpy(At(mapping, 40 * small + 2048), source.data(), source.size());
    RequireWritten(mapping, watchedLength, {{40, 43}}, "memcpy");
}};

const Case watchMprotect{"WriteWatch_MprotectRoundTrip_ReportsNothingThenTracksWrites", [] {
    RequireWriteWatch();
    Mappings mappings;
    void* mapping = MapFlexible(mappings, watchedLength, "map");
    ClearWritten(mapping, watchedLength);
    RequireEqual(sceKernelMprotect(mapping, watchedLength, 1), 0, "make read-only");
    RequireEqual(sceKernelMprotect(mapping, watchedLength, 3), 0, "make writable");
    RequireWritten(mapping, watchedLength, {}, "after mprotect");
    StoreByte(mapping, 50 * small, 1);
    RequireWritten(mapping, watchedLength, {{50, 51}}, "after writing page 50");
}};

const Case watchHole{"WriteWatch_UnmappedHole_StopsCoveringUntilRemapped", [] {
    RequireWriteWatch();
    Mappings mappings;
    void* mapping = MapFlexible(mappings, watchedLength, "map");
    ClearWritten(mapping, watchedLength);
    void* middle = At(mapping, 4 * page);
    RequireEqual(sceKernelMunmap(middle, page), 0, "unmap a guest page");
    Require(!Covers(mapping, watchedLength), "range with a hole is not covered");
    PageRuns runs;
    Require(!CollectRuns(mapping, 0, watchedLength, runs), "collect across the hole fails");
    Require(runs.empty(), "no runs across the hole, got " + DescribeRuns(runs));
    MapFlexibleAt(mappings, middle, page, fixedFlag, "remap the hole");
    Require(Covers(mapping, watchedLength), "remapped range is covered");
    RequireWritten(mapping, watchedLength, {{16, 20}}, "remapped page reported");
    RequireWritten(mapping, watchedLength, {}, "after clearing");
    StoreByte(middle, 1, 1);
    RequireWritten(mapping, watchedLength, {{16, 17}}, "write to the remapped page");
}};

const Case watchUnmapped{"WriteWatch_AfterMunmap_StopsCovering", [] {
    RequireWriteWatch();
    Mappings mappings;
    void* mapping = MapFlexible(mappings, watchedLength, "map");
    RequireEqual(sceKernelMunmap(mapping, watchedLength), 0, "unmap");
    Require(!Covers(mapping, small), "unmapped range is not covered");
}};

class HostMapping {
public:
    explicit HostMapping(std::size_t bytes) : address(mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)), bytes(bytes) {
        Require(address != MAP_FAILED, "map host memory");
    }

    ~HostMapping() {
        if (address != MAP_FAILED) munmap(address, bytes);
    }

    HostMapping(const HostMapping&) = delete;
    HostMapping& operator=(const HostMapping&) = delete;

    void* Address() const noexcept { return address; }

    int Unmap() { return munmap(std::exchange(address, MAP_FAILED), bytes); }

private:
    void* address;
    std::size_t bytes;
};

class WatchRegistration {
public:
    WatchRegistration(void* region, std::size_t bytes) : region(region), bytes(bytes) {
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(region, bytes);
    }

    ~WatchRegistration() {
        if (region != nullptr) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(region, bytes);
    }

    WatchRegistration(const WatchRegistration&) = delete;
    WatchRegistration& operator=(const WatchRegistration&) = delete;

    void Unregister() { GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(std::exchange(region, nullptr), bytes); }

private:
    void* region;
    std::size_t bytes;
};

const Case watchSpanningTables{"WriteWatch_RegisteredRangeSpanningTables_ReportsWritesInEachTable", [] {
    RequireWriteWatch();
    constexpr std::size_t tableSpan = 0x200000;
    constexpr std::size_t spanned = 2 * tableSpan;
    HostMapping raw(spanned + tableSpan);
    const auto rawAddress = reinterpret_cast<std::uintptr_t>(raw.Address());
    void* region = reinterpret_cast<void*>((rawAddress + tableSpan - 1) & ~(tableSpan - 1));
    WatchRegistration registration(region, spanned);
    Require(Covers(region, spanned), "registered region is covered");
    RequireWritten(region, spanned, {{0, spanned / small}}, "initial collect");
    RequireWritten(region, spanned, {}, "second collect");
    StoreByte(region, tableSpan + 3 * small, 1);
    StoreByte(region, 7 * small, 1);
    RequireWritten(region, spanned, {{7, 8}, {tableSpan / small + 3, tableSpan / small + 4}}, "writes in both tables");
    RequireWritten(region, spanned, {}, "after clearing");
    registration.Unregister();
    RequireEqual(raw.Unmap(), 0, "unmap host memory");
}};

std::string BackingOf(const void* address) {
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        if (std::strtoull(line.c_str(), nullptr, 16) != reinterpret_cast<std::uintptr_t>(address)) continue;
        const auto path = line.find('/');
        return path == std::string::npos ? std::string() : line.substr(path);
    }
    return {};
}

const Case watchDirect{"WriteWatch_DirectMapping_ReportsWrites", [] {
    RequireWriteWatch();
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 3, "allocate");
    void* first = MapDirect(mappings, phys, page * 3, "map");
    Require(Covers(first, page * 3), "direct mapping is covered");
    RequireWritten(first, page * 3, {{0, page * 3 / small}}, "initial collect");
    RequireWritten(first, page * 3, {}, "second collect");
    StoreByte(first, page + 5, 1);
    RequireWritten(first, page * 3, {{page / small, page / small + 1}}, "after writing the second guest page");
}};

const Case watchAlias{"WriteWatch_AliasedDirectPage_IsNotCovered", [] {
    RequireWriteWatch();
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 3, "allocate");
    void* first = MapDirect(mappings, phys, page * 3, "map");
    ClearWritten(first, page * 3);
    void* alias = MapDirect(mappings, phys + physPage, page, "map an alias of the second page");
    Require(!Covers(alias, page), "alias is not covered");
    Require(!Covers(At(first, page), page), "aliased page is not covered");
    Require(Covers(first, page), "first page is covered");
    Require(Covers(At(first, page * 2), page), "third page is covered");
    PageRuns runs;
    Require(!CollectRuns(first, 0, page * 3, runs), "collect across the aliased page fails");
    StoreByte(first, 0, 2);
    StoreByte(alias, 7, 3);
    RequireEqual(ByteAt(first, page + 7), 3, "block sees the alias write");
    Require(CollectRuns(first, 0, page, runs), "collect the first page");
    Require(runs == PageRuns{{0, 1}}, "first page written, got " + DescribeRuns(runs));
    Require(CollectRuns(first, page * 2, page, runs), "collect the third page");
    Require(runs.empty(), "third page clean, got " + DescribeRuns(runs));
    RequireEqual(sceKernelMunmap(alias, page), 0, "unmap the alias");
}};

const Case watchDirectUnmapped{"WriteWatch_AfterDirectMunmap_StopsCovering", [] {
    RequireWriteWatch();
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 3, "allocate");
    void* first = MapDirect(mappings, phys, page * 3, "map");
    RequireEqual(sceKernelMunmap(first, page * 3), 0, "unmap");
    Require(!Covers(first, page), "unmapped range is not covered");
}};

const Case watchFixedDirect{"WriteWatch_FixedDirectPageInFlexibleMapping_ReportsThatPage", [] {
    RequireWriteWatch();
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page * 3, "allocate");
    void* flexible = MapFlexible(mappings, page * 3, "map flexible memory");
    RequireWritten(flexible, page * 3, {{0, page * 3 / small}}, "initial collect");
    RequireWritten(flexible, page * 3, {}, "second collect");
    void* fixed = MapDirectAt(mappings, At(flexible, page), phys + physPage * 2, page, fixedFlag, "map direct memory into the middle");
    Require(Covers(flexible, page * 3), "mixed range is covered");
    RequireWritten(flexible, page * 3, {{page / small, page * 2 / small}}, "replaced page reported");
    RequireWritten(flexible, page * 3, {}, "after clearing");
    StoreByte(fixed, 9, 4);
    RequireWritten(flexible, page * 3, {{page / small, page / small + 1}}, "write to the direct page");
    RequireEqual(sceKernelMunmap(flexible, page * 3), 0, "unmap");
    RequireEqual(sceKernelReleaseDirectMemory(phys, page * 3), 0, "release");
}};

const Case memfdBacking{"MapDirectMemory_HostBacking_IsMemfd", [] {
    DirectAllocations direct;
    Mappings mappings;
    const std::int64_t phys = direct.Allocate(page, "allocate");
    void* mapped = MapDirect(mappings, phys, page, "map");
    const auto backing = BackingOf(mapped);
    Require(backing.rfind("/memfd:", 0) == 0, "direct memory backing is a memfd, got '" + backing + "'");
    RequireEqual(sceKernelMunmap(mapped, page), 0, "unmap");
    RequireEqual(sceKernelReleaseDirectMemory(phys, page), 0, "release");
}};

class BackingFile {
public:
    BackingFile() = default;
    ~BackingFile() {
        if (descriptor >= 0) close(descriptor);
    }
    BackingFile(const BackingFile&) = delete;
    BackingFile& operator=(const BackingFile&) = delete;

    bool Query(std::uintptr_t address, std::size_t bytes) {
        if (descriptor >= 0) close(descriptor);
        descriptor = -1;
        offset = 0;
        int file = -1;
        const bool found = GuestArena::GuestArenaSharedBacking_nid_postfix(address, bytes, &file, &offset);
        if (found) descriptor = file;
        return found;
    }

    int Descriptor() const noexcept { return descriptor; }
    std::uint64_t Offset() const noexcept { return offset; }

private:
    int descriptor = -1;
    std::uint64_t offset = 0;
};

class BackedMapping {
public:
    BackedMapping() {
        phys = direct.Allocate(page * 3, "allocate");
        first = MapDirect(mappings, phys + physPage, page * 2, "map the upper two pages");
        address = Address(first);
        StoreByte(first, page + 5, 0x5a);
    }

    DirectAllocations direct;
    Mappings mappings;
    std::int64_t phys = 0;
    void* first = nullptr;
    std::uintptr_t address = 0;
};

const Case sharedBackingFile{"GuestArenaSharedBacking_DirectMapping_ReturnsSealedCloexecFileAtPhysicalOffset", [] {
    const BackedMapping mapping;
    BackingFile backing;
    Require(backing.Query(mapping.address + page, page), "backing found");
    RequireEqual(backing.Offset(), std::uint64_t{page * 2}, "backing offset");
    unsigned char byte = 0;
    RequireEqual(pread(backing.Descriptor(), &byte, 1, static_cast<off_t>(backing.Offset() + 5)), ssize_t{1}, "read the backing file");
    RequireEqual(static_cast<int>(byte), 0x5a, "backing contents");
    const int seals = fcntl(backing.Descriptor(), F_GET_SEALS);
    Require(seals >= 0, "query seals");
    Require((seals & F_SEAL_SHRINK) != 0 && (seals & F_SEAL_GROW) != 0, "backing size is sealed");
    Require((fcntl(backing.Descriptor(), F_GETFD) & FD_CLOEXEC) != 0, "backing is close-on-exec");
}};

const Case sharedBackingOffsets{"GuestArenaSharedBacking_SubRanges_ReportPhysicalOffsets", [] {
    const BackedMapping mapping;
    BackingFile backing;
    Require(backing.Query(mapping.address, page * 2), "whole mapping backing found");
    RequireEqual(backing.Offset(), std::uint64_t{page}, "whole mapping offset");
    Require(backing.Query(mapping.address + page + 0x1000, 0x1000), "sub-page backing found");
    RequireEqual(backing.Offset(), std::uint64_t{page * 2 + 0x1000}, "sub-page offset");
}};

const Case sharedBackingOutside{"GuestArenaSharedBacking_RangePastMappingOrEmpty_Fails", [] {
    const BackedMapping mapping;
    BackingFile backing;
    Require(!backing.Query(mapping.address, page * 3), "range past the mapping");
    Require(!backing.Query(mapping.address, 0), "empty range");
}};

const Case sharedBackingFlexible{"GuestArenaSharedBacking_FlexibleMapping_Fails", [] {
    Mappings mappings;
    void* flexible = MapFlexible(mappings, page * 2, "map flexible memory");
    BackingFile backing;
    Require(!backing.Query(Address(flexible), page), "flexible memory has no shared backing");
}};

const Case sharedBackingAdjacent{"GuestArenaSharedBacking_FixedViewsOfAdjacentPages_ReportOneBacking", [] {
    BackedMapping mapping;
    void* flexible = MapFlexible(mapping.mappings, page * 2, "map flexible memory");
    MapDirectAt(mapping.mappings, flexible, mapping.phys, page, fixedFlag, "map the low page");
    MapDirectAt(mapping.mappings, At(flexible, page), mapping.phys + physPage, page, fixedFlag, "map the high page");
    BackingFile backing;
    Require(backing.Query(Address(flexible), page * 2), "contiguous views have one backing");
    RequireEqual(backing.Offset(), std::uint64_t{0}, "backing offset");
}};

const Case sharedBackingGap{"GuestArenaSharedBacking_FixedViewsOfNonAdjacentPages_FailsAcrossBoth", [] {
    BackedMapping mapping;
    void* flexible = MapFlexible(mapping.mappings, page * 2, "map flexible memory");
    MapDirectAt(mapping.mappings, flexible, mapping.phys, page, fixedFlag, "map the low page");
    MapDirectAt(mapping.mappings, At(flexible, page), mapping.phys + physPage, page, fixedFlag, "map the high page");
    void* high = MapDirectAt(mapping.mappings, At(flexible, page), mapping.phys + physPage * 2, page, fixedFlag, "remap the high page");
    BackingFile backing;
    Require(!backing.Query(Address(flexible), page * 2), "non-contiguous views have no single backing");
    Require(backing.Query(Address(high), page), "high page backing found");
    RequireEqual(backing.Offset(), std::uint64_t{page * 2}, "high page offset");
}};

const Case sharedBackingUnmapped{"GuestArenaSharedBacking_AfterMunmap_Fails", [] {
    BackedMapping mapping;
    void* flexible = MapFlexible(mapping.mappings, page * 2, "map flexible memory");
    MapDirectAt(mapping.mappings, flexible, mapping.phys, page, fixedFlag, "map the low page");
    void* high = MapDirectAt(mapping.mappings, At(flexible, page), mapping.phys + physPage * 2, page, fixedFlag, "map the high page");
    BackingFile backing;
    RequireEqual(sceKernelMunmap(high, page), 0, "unmap the high page");
    Require(!backing.Query(Address(high), page), "unmapped high page has no backing");
    RequireEqual(sceKernelMunmap(flexible, page), 0, "unmap the low page");
    RequireEqual(sceKernelMunmap(mapping.first, page * 2), 0, "unmap the upper mapping");
    Require(!backing.Query(mapping.address, page), "unmapped mapping has no backing");
    RequireEqual(sceKernelReleaseDirectMemory(mapping.phys, page * 3), 0, "release");
}};
#endif

struct RejectedMapping {
    std::size_t length;
    int protection;
    int flags;
    int descriptor;
    std::int64_t offset;
    int error;
    const char* label;
};

void RequireRejected(const RejectedMapping& input) {
    Mappings mappings;
    ClearErrno();
    void* result = mmap_nid_postfix(nullptr, input.length, input.protection, input.flags, input.descriptor, input.offset);
    mappings.TrackRange(result, page);
    const int error = Errno();
    Require(result == FailedMapping(), std::string(input.label) + " is rejected");
    RequireEqual(error, input.error, std::string(input.label) + " errno");
}

const Case mmapInvalid{"Mmap_InvalidArguments_FailWithEinval", [] {
    const RejectedMapping inputs[] = {
        {0, 3, guestPrivateAnonymous, -1, 0, guestEinval, "zero length"},
        {std::numeric_limits<std::size_t>::max(), 3, guestPrivateAnonymous, -1, 0, guestEinval, "maximum length"},
        {page, 8, guestPrivateAnonymous, -1, 0, guestEinval, "protection 8"},
        {page, 3, guestPrivateAnonymous, 0, 0, guestEinval, "anonymous with descriptor 0"},
        {page, 3, guestPrivateAnonymous, -1, 1, guestEinval, "anonymous with offset 1"},
    };
    for (const auto& input : inputs) RequireRejected(input);
}};

const Case mmapUnsupported{"Mmap_UnsupportedMappingKinds_FailWithEopnotsupp", [] {
    const RejectedMapping inputs[] = {
        {page, 3, 0x1001, -1, 0, guestEopnotsupp, "shared anonymous"},
        {page, 3, 0x1012, -1, 0, guestEopnotsupp, "fixed anonymous"},
        {page, 3, 0x2, 0, 0, guestEopnotsupp, "file-backed"},
        {page, 3, 0x22, -1, 0, guestEopnotsupp, "host MAP_ANON value"},
    };
    for (const auto& input : inputs) RequireRejected(input);
}};

const Case mmapAnonymous{"Mmap_AnonymousPrivate_ReturnsZeroedPageAlignedGuestPages", [] {
    Mappings mappings;
    unsigned char* memory = MapAnonymous(mappings, page * 3 - 1, "map three pages less one byte");
    RequireEqual(Address(memory) & (page - 1), std::uintptr_t{0}, "page alignment");
    const auto nonZero = std::find_if(memory, memory + page * 3, [](unsigned char byte) { return byte != 0; });
    RequireEqual(static_cast<std::size_t>(nonZero - memory), page * 3, "offset of the first non-zero byte");
    const auto range = FindRange(memory);
    RequireEqual(range.bytes, page * 3, "registered size");
    Require(range.readable && range.writable, "registered as readable and writable");
}};

unsigned char* MapPosixFixture(Mappings& mappings) {
    unsigned char* memory = MapAnonymous(mappings, page * 3 - 1, "map three pages less one byte");
    memory[0] = 42;
    memory[page * 2] = 73;
    return memory;
}

unsigned char* FreedPosixRange(Mappings& mappings) {
    unsigned char* memory = MapPosixFixture(mappings);
    RequireEqual(munmap_nid_postfix(memory, page * 3), 0, "unmap the fixture");
    return memory;
}

const Case mprotectZero{"Mprotect_ZeroLength_Succeeds", [] {
    Mappings mappings;
    unsigned char* memory = MapPosixFixture(mappings);
    RequireEqual(mprotect_nid_postfix(memory, 0, 1), 0, "zero length");
}};

const Case mprotectNull{"Mprotect_NullAddress_FailsWithEinval", [] {
    ClearErrno();
    RequireEqual(mprotect_nid_postfix(nullptr, page, 1), -1, "null address");
    RequireEqual(Errno(), guestEinval, "errno");
}};

const Case mprotectSubPage{"Mprotect_SubPageRange_AppliesToWholeGuestPage", [] {
    Mappings mappings;
    unsigned char* memory = MapPosixFixture(mappings);
    RequireEqual(mprotect_nid_postfix(memory + 1, 1, 1), 0, "protect one byte");
    const auto readOnlyRange = FindRange(memory);
    Require(readOnlyRange.readable && !readOnlyRange.writable, "page is read-only");
    RequireEqual(static_cast<int>(memory[0]), 42, "contents kept");
    RequireEqual(mprotect_nid_postfix(memory, page, 3), 0, "restore write access");
    Require(FindRange(memory).writable, "page is writable again");
}};

const Case madviseStandard{"Madvise_StandardAdvice_SucceedsAndKeepsContents", [] {
    Mappings mappings;
    unsigned char* memory = MapPosixFixture(mappings);
    for (int advice = 0; advice <= 9; ++advice) {
        RequireEqual(madvise_nid_postfix(memory, page * 3, advice), 0, "advice " + std::to_string(advice));
    }
    RequireEqual(static_cast<int>(memory[0]), 42, "first page contents");
    RequireEqual(static_cast<int>(memory[page * 2]), 73, "last page contents");
    RequireEqual(madvise_nid_postfix(memory + 1, 0, 4), 0, "zero length at an unaligned address");
}};

const Case madviseInvalid{"Madvise_InvalidArguments_FailWithEinval", [] {
    Mappings mappings;
    unsigned char* memory = MapPosixFixture(mappings);
    struct Input {
        void* address;
        std::size_t length;
        int advice;
        const char* label;
    };
    const Input inputs[] = {
        {memory, page, 11, "advice 11"},
        {memory, page, -1, "advice -1"},
        {memory, 0, 11, "zero length with advice 11"},
        {memory, std::numeric_limits<std::size_t>::max(), 0, "maximum length"},
        {reinterpret_cast<void*>(0x800000000000), 1, 0, "address past user space"},
    };
    for (const auto& input : inputs) {
        ClearErrno();
        RequireEqual(madvise_nid_postfix(input.address, input.length, input.advice), -1, input.label);
        RequireEqual(Errno(), guestEinval, std::string(input.label) + " errno");
    }
}};

const Case madviseTop{"Madvise_LastUserPage_Succeeds", [] {
    RequireEqual(madvise_nid_postfix(reinterpret_cast<void*>(0x7fffffffc000), 0x4000, 4), 0, "last user page");
}};

const Case munmapInvalid{"Munmap_UnalignedOrEmptyRange_FailsWithEinvalAndKeepsContents", [] {
    Mappings mappings;
    unsigned char* memory = MapPosixFixture(mappings);
    ClearErrno();
    RequireEqual(munmap_nid_postfix(memory + 1, page), -1, "unaligned address");
    RequireEqual(Errno(), guestEinval, "unaligned address errno");
    ClearErrno();
    RequireEqual(munmap_nid_postfix(memory, 0), -1, "zero length");
    RequireEqual(Errno(), guestEinval, "zero length errno");
    RequireEqual(static_cast<int>(memory[0]), 42, "contents kept");
}};

const Case munmapPieces{"Munmap_PieceByPiece_KeepsRemainingPages", [] {
    Mappings mappings;
    unsigned char* memory = MapPosixFixture(mappings);
    RequireEqual(munmap_nid_postfix(memory + page, 1), 0, "unmap one byte of the middle page");
    RequireEqual(static_cast<int>(memory[0]), 42, "first page kept");
    RequireEqual(static_cast<int>(memory[page * 2]), 73, "last page kept");
    RequireEqual(munmap_nid_postfix(memory, page), 0, "unmap the first page");
    RequireEqual(static_cast<int>(memory[page * 2]), 73, "last page still kept");
    RequireEqual(munmap_nid_postfix(memory + page * 2, page), 0, "unmap the last page");
}};

const Case munmapUnmapped{"Munmap_UnmappedRange_Fails", [] {
    Mappings mappings;
    unsigned char* memory = FreedPosixRange(mappings);
    RequireEqual(munmap_nid_postfix(memory, page), -1, "unmapped range");
}};

const Case madviseUnmapped{"Madvise_UnmappedRange_Succeeds", [] {
    Mappings mappings;
    unsigned char* memory = FreedPosixRange(mappings);
    RequireEqual(madvise_nid_postfix(memory, page, 4), 0, "unmapped range");
}};

const Case mprotectUnmapped{"Mprotect_UnmappedRange_FailsWithEinval", [] {
    Mappings mappings;
    unsigned char* memory = FreedPosixRange(mappings);
    ClearErrno();
    RequireEqual(mprotect_nid_postfix(memory, page, 1), -1, "unmapped range");
    RequireEqual(Errno(), guestEinval, "errno");
}};

const Case mprotectOverflow{"Mprotect_LengthPastAddressSpace_FailsWithEinval", [] {
    Mappings mappings;
    unsigned char* memory = FreedPosixRange(mappings);
    ClearErrno();
    RequireEqual(mprotect_nid_postfix(memory, std::numeric_limits<std::size_t>::max(), 1), -1, "maximum length");
    RequireEqual(Errno(), guestEinval, "errno");
}};

const Case mmapProtections{"Mmap_HintedProtection_RegistersMatchingAccess", [] {
    Mappings fixtureMappings;
    unsigned char* memory = FreedPosixRange(fixtureMappings);
    for (const int protection : {0, 1, 3, 5}) {
        const std::string label = "protection " + std::to_string(protection);
        Mappings mappings;
        void* mapped = mmap_nid_postfix(memory, 1, protection, guestPrivateAnonymous, -1, 0);
        mappings.TrackRange(mapped, page);
        Require(mapped != FailedMapping(), label + " maps");
        const auto range = FindRange(mapped);
        RequireEqual(range.readable, (protection & 3) != 0, label + " readable");
        RequireEqual(range.writable, (protection & 2) != 0, label + " writable");
        RequireEqual(munmap_nid_postfix(mapped, 1), 0, label + " unmap");
    }
}};

} // namespace
