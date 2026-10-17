#include "BdaTests.hpp"
#include "ImageTableAbi.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/mman.h>
#endif
namespace {

using namespace AgcDriver::Graphics;
using Role = ShaderRecompiler::DescriptorRole;

template<typename TAction>
void reject(TAction action, const char* reason) {
    try { action(); }
    catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find(reason) != std::string::npos, std::string("unexpected BDA test error: ") + error.what());
        return;
    }
    throw std::runtime_error(std::string("expected BDA rejection: ") + reason);
}

void addressTableKeyTests() {
    using ShaderRecompiler::BdaAbi::Range;
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> written{{{0x1000, 0x1040}}};
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> otherWritten{{{0x1000, 0x1080}}};
    const std::array<Range, 1> own{{{0x2000, 0x2100, 0x70000000, ShaderRecompiler::BdaAbi::Read, 0}}};
    const std::array<Range, 1> moved{{{0x2000, 0x2100, 0x70010000, ShaderRecompiler::BdaAbi::Read, 0}}};
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 2> ownAsWrites{{{0x2000, 0x2100}, {0x70000000, ShaderRecompiler::BdaAbi::Read}}};
    const auto plain = AddressTableKey({}, own);
    Require(AddressTableKey({}, own) == plain, "one build's BDA table key is not stable");
    Require(AddressTableKey(written, own) != plain && AddressTableKey(written, own) != AddressTableKey(otherWritten, own), "BDA table keys of builds that differ only in their written ranges are equal");
    Require(AddressTableKey({}, moved) != plain, "BDA table keys of builds whose own region moved are equal");
    Require(AddressTableKey(ownAsWrites, {}) != plain, "written ranges and own regions with the same words give one BDA table key");
}

void snapshotTableTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t piece = 65536;
    void* block = ::operator new(piece, std::align_val_t{piece});
    std::memset(block, 0x3c, piece);
    auto importing = context;
    importing.hostImportAlignment = piece;
    const auto change = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, block, piece, true, true, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, block);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    change(true);
    alignas(256) static std::array<std::byte, 512> source{};
    const auto snapshotAddress = reinterpret_cast<std::uint64_t>(source.data());
    std::array<std::byte, 512> first{};
    std::array<std::byte, 512> second{};
    first.fill(std::byte{0x11});
    second.fill(std::byte{0x22});
    struct Build {
        std::unique_ptr<GuestBufferMemory> memory;
        std::unique_ptr<BdaResources> bda;
        std::uint64_t snapshot = 0;
        VkBuffer table = VK_NULL_HANDLE;
        VkBuffer view = VK_NULL_HANDLE;
    };
    const auto build = [&](std::span<const std::byte> bytes, bool written, bool addressable, std::uint64_t address) {
        Build result;
        result.memory = std::make_unique<GuestBufferMemory>(importing);
        auto& memory = *result.memory;
        if (addressable) memory.AcquireRegistered();
        if (written) memory.AddWritable(reinterpret_cast<std::uint64_t>(block) + 256, 64);
        memory.AddSnapshot({address, bytes});
        memory.Upload(addressable);
        std::uint32_t adjustment = 0;
        result.view = memory.Descriptor(address, bytes.size(), adjustment).buffer;
        if (!addressable) return result;
        const auto ranges = memory.AddressRanges();
        const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == address; });
        Require(found != ranges.end() && found->end == address + bytes.size(), "the snapshot has no BDA table entry of its own");
        result.snapshot = found->deviceAddress;
        const auto held = access.addressBytes(result.snapshot);
        Require(held.size() >= bytes.size() && std::memcmp(held.data(), bytes.data(), bytes.size()) == 0, "the snapshot's device copy does not hold its bytes");
        if (const auto cached = memory.CachedAddressTable()) {
            Require(cached->ranges->size() == ranges.size() && std::memcmp(cached->ranges->data(), ranges.data(), ranges.size() * sizeof(ranges.front())) == 0, "a cached BDA table differs from the build's ranges");
        }
        result.bda = std::make_unique<BdaResources>(importing, memory);
        result.table = result.bda->Table().buffer;
        return result;
    };
    const bool spaces = AddressSpaceCounters().enabled;
    {
        auto a = build(first, false, true, snapshotAddress);
        auto plain = build(first, false, false, snapshotAddress);
        auto d = build(first, true, true, snapshotAddress);
        auto b = build(first, false, true, snapshotAddress);
        auto c = build(second, false, true, snapshotAddress);
        Require(a.snapshot == b.snapshot && a.view == b.view, "two builds of one snapshot with the same bytes do not share its device copy");
        Require(c.snapshot != a.snapshot, "a snapshot with other bytes took the device copy of the first");
        Require(plain.view != a.view && d.snapshot == a.snapshot, "a snapshot copy made for BDA serves a build bound by descriptor, or a build with written ranges did not share the snapshot copy");
        if (spaces) {
            Require(a.memory->CachedAddressTable().has_value() && c.memory->CachedAddressTable().has_value() && d.memory->CachedAddressTable().has_value(), "a build with its own regions over the address space has no cached BDA table");
            Require(a.table == b.table, "two builds of the same space and snapshot do not share their BDA table");
            Require(c.table != a.table, "a build with other snapshot bytes took another build's BDA table");
            Require(a.memory->CachedAddressTable()->serial == b.memory->CachedAddressTable()->serial && c.memory->CachedAddressTable()->serial != a.memory->CachedAddressTable()->serial, "cached BDA table serials do not follow the build's ranges");
        }
        ClearSnapshotCopies(importing.device);
        auto e = build(first, false, true, snapshotAddress);
        Require(e.snapshot != a.snapshot, "a snapshot copy outlived the teardown of its device's copies");
        for (auto* item : {&a, &b, &c, &d, &e, &plain}) item->memory->WriteBack();
    }
    if (spaces) {
        std::vector<std::array<std::byte, 512>> variants(66);
        for (std::size_t index = 0; index < variants.size(); ++index) {
            variants[index].fill(std::byte{0x40});
            variants[index][0] = static_cast<std::byte>(index);
            variants[index][1] = static_cast<std::byte>(index >> 8u);
        }
        alignas(256) static std::array<std::byte, 66 * 512> sources{};
        std::vector<Build> builds;
        const auto serial = [&](std::size_t index) {
            builds.push_back(build(variants[index], false, true, reinterpret_cast<std::uint64_t>(sources.data()) + index * 512));
            return builds.back().memory->CachedAddressTable()->serial;
        };
        const auto kept = serial(0);
        const auto evicted = serial(1);
        for (std::size_t index = 2; index < 64; ++index) serial(index);
        Require(serial(0) == kept, "a BDA table was dropped before 64 newer tables of its space");
        serial(64);
        Require(serial(0) == kept, "the most recently used BDA table of a space was evicted");
        Require(serial(1) != evicted, "the least recently used BDA table of a space was kept past 64 tables");
        for (auto& item : builds) item.memory->WriteBack();
    }
    ClearSnapshotCopies(importing.device);
    change(false);
    ::operator delete(block, std::align_val_t{piece});
}

void spaceReuseTests(const Context& context) {
    constexpr std::size_t piece = 65536;
    auto* block = static_cast<std::byte*>(::operator new(2 * piece, std::align_val_t{piece}));
    std::memset(block, 0x5a, 2 * piece);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    auto importing = context;
    importing.hostImportAlignment = piece;
    const auto change = [&](std::byte* range, bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, piece, true, true, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, range);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    change(block, true);
    alignas(256) static std::array<std::byte, 64> captured{};
    captured.fill(std::byte{0x21});
    std::array<std::byte, 16> liveBytes{};
    std::memcpy(liveBytes.data(), block + 128, liveBytes.size());
    const std::array<GuestMemorySnapshot, 2> snapshots{{{reinterpret_cast<std::uint64_t>(captured.data()), captured}, {address + 128, liveBytes}}};
    auto otherCapture = captured;
    otherCapture[3] = std::byte{0x22};
    const std::array<GuestMemorySnapshot, 2> otherCopied{{{reinterpret_cast<std::uint64_t>(captured.data()), otherCapture}, snapshots[1]}};
    const std::array<GuestMemorySnapshot, 1> fewer{{snapshots[0]}};
    {
        GuestBufferMemory memory(importing);
        memory.AcquireRegistered();
        for (const auto& snapshot : snapshots) memory.AddSnapshot(snapshot);
        memory.AddWritable(address + 256, 64);
        memory.Upload(true);
        if (AddressSpaceCounters().enabled && memory.CopiedBytes() != captured.size()) {
            Require(!memory.SpaceRegions().has_value(), "an upload with ranges copied per build offered its regions for reuse");
            std::cout << "registered ranges are copied per build here: address space reuse not tested\n";
            memory.WriteBack();
        } else if (AddressSpaceCounters().enabled) {
            const auto regions = memory.SpaceRegions();
            Require(regions.has_value() && regions->empty(), "an upload over the address space with only a snapshot copy of its own was refused reuse");
            const auto written = memory.DirectWrites();
            Require(written.size() == 1 && written.front() == std::pair{address + 256, address + 320}, "the written range served in place by the address space is not its direct write");
            Require(memory.SameSnapshots(snapshots), "an upload refused the snapshots it was built from");
            Require(!memory.SameSnapshots(otherCopied) && !memory.SameSnapshots(fewer), "an upload took snapshots with other copied bytes, or fewer of them");
            memory.SuspendSpace();
            Require(!memory.HoldsLease(), "a suspended upload still holds its address space");
            Require(memory.ResumeSpace() && memory.HoldsLease() && memory.SpaceImportsCurrent(), "an upload could not take its unchanged address space again");
            memory.SuspendSpace();
            block[128] ^= std::byte{0xff};
            Require(!memory.SameSnapshots(snapshots), "an upload took a snapshot that no longer equals the registered memory it covers");
            block[128] ^= std::byte{0xff};
            change(block + piece, true);
            Require(!memory.ResumeSpace() && !memory.HoldsLease(), "an upload took its address space again after another range was registered");
            change(block + piece, false);
            Require(!memory.ResumeSpace(), "an upload took a replaced address space again");
        } else {
            Require(!memory.SpaceRegions().has_value(), "an upload without the cached address space offered its regions for reuse");
            memory.WriteBack();
        }
    }
    ClearSnapshotCopies(importing.device);
    change(block, false);
    ::operator delete(block, std::align_val_t{piece});
}

void importCrossingTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t half = 65536;
    void* block = ::operator new(2 * half, std::align_val_t{half});
    auto* guest = static_cast<std::uint8_t*>(block);
    std::memset(guest, 0x11, half);
    std::memset(guest + half, 0x22, half);
    const auto first = reinterpret_cast<std::uintptr_t>(block);
    const auto second = first + half;
    auto importing = context;
    importing.hostImportAlignment = half;
    const auto registry = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        for (auto* range : {guest, guest + half}) {
            if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, half, true, false, true);
            else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, range);
        }
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    registry(true);
    {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        Require(HostImportCovers(importing, first, half) && HostImportCovers(importing, second, half), "the registered ranges were not imported");
        const auto secondImport = *HostImportFor(importing, second, half);
        leased.AddReadable(second - 16, 32);
        leased.AddReadable(second + 64, 16);
        leased.Upload(true);
        std::uint32_t adjustment = 0;
        const auto crossing = leased.Descriptor(second - 16, 32, adjustment);
        const auto crossingBytes = access.bytes(crossing.buffer);
        Require(crossing.offset + crossing.range <= crossingBytes.size(), "a view crossing into the next registered range is bound past its host import");
        Require(secondImport.base == first && secondImport.bytes == 2 * half, "adjacent registered ranges with the same access were not imported as one span");
        Require(crossing.buffer == secondImport.buffer && crossing.offset + adjustment == second - 16 - secondImport.base, "a view crossing into the next registered range is not bound in place in the spanning import");
        const auto inside = leased.Descriptor(second + 64, 16, adjustment);
        const auto insideBytes = access.bytes(inside.buffer);
        Require(inside.offset + inside.range <= insideBytes.size() && (inside.buffer == secondImport.buffer ? inside.offset + adjustment == second + 64 - secondImport.base : insideBytes[inside.offset + adjustment] == std::byte{0x22}), "a view in the second registered range is bound past its buffer or misses its bytes");
        for (const auto& range : leased.AddressRanges()) {
            if (range.end <= first || range.begin >= second + half) continue;
            const auto mapped = access.addressBytes(range.deviceAddress);
            Require(mapped.size() >= range.end - range.begin, "a BDA range runs past the end of its host import");
            if (range.begin <= second && second < range.end && (range.deviceAddress < secondImport.address || range.deviceAddress >= secondImport.address + secondImport.bytes)) Require(mapped[second - range.begin] == std::byte{0x22}, "the BDA range over the second registered range misses its bytes");
        }
        leased.WriteBack();
    }
    registry(false);
    Require(HostImportFor(importing, first, half) == nullptr && !HostImportCovers(importing, second, half), "the host imports outlived their ranges");
    ::operator delete(block, std::align_val_t{half});
}

void importSpanTests(const Context& context) {
    constexpr std::size_t piece = 65536;
    void* block = ::operator new(3 * piece, std::align_val_t{piece});
    auto* guest = static_cast<std::uint8_t*>(block);
    const auto first = reinterpret_cast<std::uintptr_t>(block);
    auto importing = context;
    importing.hostImportAlignment = piece;
    const auto change = [&](std::uint8_t* range, bool add, bool writable) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, piece, true, writable, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, range);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    change(guest, true, false);
    change(guest + piece, true, false);
    change(guest + 2 * piece, true, true);
    const auto* span = HostImportFor(importing, first + piece - 16, 32);
    Require(span != nullptr && span->base == first && span->bytes == 2 * piece, "adjacent read-only ranges were not imported as one span");
    const auto* writable = HostImportFor(importing, first + 2 * piece, piece);
    Require(writable != nullptr && writable->base == first + 2 * piece && writable->bytes == piece, "a writable range joined the read-only span");
    Require(HostImportFor(importing, first + 2 * piece - 16, 32) == nullptr, "a view crossing ranges of different access was imported");
    change(guest + piece, false, false);
    const auto* alone = HostImportFor(importing, first, piece);
    Require(alone != nullptr && alone->base == first && alone->bytes == piece, "the span outlived the removal of one of its ranges");
    Require(HostImportFor(importing, first + piece, 16) == nullptr, "the removed range is still imported");
    change(guest, false, false);
    change(guest + 2 * piece, false, false);
    ::operator delete(block, std::align_val_t{piece});
}

void spaceWritableWritesTests(const Context& context) {
    if (!AddressSpaceCounters().enabled) return;
    constexpr std::size_t piece = 65536;
    auto* writable = static_cast<std::uint8_t*>(::operator new(piece, std::align_val_t{piece}));
    auto* readOnly = static_cast<std::uint8_t*>(::operator new(piece, std::align_val_t{piece}));
    std::memset(writable, 0x11, piece);
    std::memset(readOnly, 0x22, piece);
    auto importing = context;
    importing.hostImportAlignment = piece;
    const auto change = [&](std::uint8_t* range, bool add, bool canWrite) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, piece, true, canWrite, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, range);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    change(writable, true, true);
    change(readOnly, true, false);
    const auto same = [](const std::vector<ShaderRecompiler::BdaAbi::Range>& left, const std::vector<ShaderRecompiler::BdaAbi::Range>& right) {
        return left.size() == right.size() && (left.empty() || std::memcmp(left.data(), right.data(), left.size() * sizeof(left.front())) == 0);
    };
    struct Built {
        std::uint64_t serial;
        std::vector<ShaderRecompiler::BdaAbi::Range> ranges;
    };
    const auto build = [&](std::initializer_list<std::pair<std::size_t, std::size_t>> written, const char* step) {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        for (const auto& [offset, bytes] : written) leased.AddWritable(reinterpret_cast<std::uintptr_t>(writable) + offset, bytes);
        leased.Upload(true);
        const auto cached = leased.CachedAddressTable();
        Require(cached.has_value() && cached->ranges != nullptr, std::string("a build of the space alone has no cached table: ") + step);
        Built result{cached->serial, leased.AddressRanges()};
        Require(same(*cached->ranges, result.ranges), std::string("the cached table differs from AddressRanges(): ") + step);
        leased.WriteBack();
        return result;
    };
    const auto plain = build({}, "no writes");
    const auto once = build({{256, 512}}, "one write in a writable import");
    const auto twice = build({{4096, 64}, {8192, 128}}, "two writes in a writable import");
    Require(once.serial == plain.serial && twice.serial == plain.serial, "writes in a writable import did not bind the space's table");
    Require(same(once.ranges, plain.ranges) && same(twice.ranges, plain.ranges), "writes in a writable import changed the BDA table");
    const auto writableBegin = reinterpret_cast<std::uint64_t>(writable);
    const auto readOnlyBegin = reinterpret_cast<std::uint64_t>(readOnly);
    for (const auto& range : once.ranges) {
        if (range.begin == writableBegin) Require((range.permissions & ShaderRecompiler::BdaAbi::Write) != 0u, "the writable import lost its write permission");
        if (range.begin == readOnlyBegin) Require((range.permissions & ShaderRecompiler::BdaAbi::Write) == 0u, "the read-only import gained write permission");
    }
    change(writable, false, true);
    change(readOnly, false, false);
    ::operator delete(writable, std::align_val_t{piece});
    ::operator delete(readOnly, std::align_val_t{piece});
}

void failedImportRemapTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t piece = 65536;
    auto* block = static_cast<std::byte*>(::operator new(4 * piece, std::align_val_t{piece}));
    auto importing = context;
    importing.hostImportAlignment = piece;
    const auto address = reinterpret_cast<std::uintptr_t>(block);
    const auto add = [&](std::size_t index) {
        GuestAllocations::Mutation mutation;
        mutation.Add(block + index * piece, piece, true, true, true);
    };
    const auto remove = [&](std::size_t index) {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block + index * piece);
    };
    SetHostImportWindow(0);
    for (std::size_t replaced = 0; replaced < 2; ++replaced) {
        add(0);
        add(1);
        std::weak_ptr<const GuestAllocations::Range> owner;
        {
            const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
            const auto found = std::find_if(lease.begin(), lease.end(), [&](const auto& range) { return range->address == address + replaced * piece; });
            Require(found != lease.end(), "failed import fixture has no registered owner");
            owner = *found;
        }
        access.limitMemory(0);
        const auto attempts = access.allocationAttempts();
        Require(HostImportFor(importing, address, 2 * piece) == nullptr && access.allocationAttempts() == attempts + 1, "host import failure was not exercised");
        Require(owner.use_count() == 1, "a failed import pins its owner");
        Require(HostImportFor(importing, address, 2 * piece) == nullptr && access.allocationAttempts() == attempts + 1, "an unchanged failed import was retried");
        add(3);
        Require(HostImportFor(importing, address, 2 * piece) == nullptr && access.allocationAttempts() == attempts + 1, "an unrelated mapping retried a failed import");
        remove(replaced);
        Require(owner.expired(), "a failed import kept the replaced owner alive");
        add(replaced);
        access.limitMemory(std::nullopt);
        const auto* imported = HostImportFor(importing, address, 2 * piece);
        Require(imported != nullptr && imported->base == address && imported->bytes == 2 * piece && access.allocationAttempts() == attempts + 2, "same-address remap retained a failed import decision");
        remove(0);
        remove(1);
        remove(3);
        ClearHostImports(importing.device);
    }
    add(0);
    add(1);
    access.limitMemory(0);
    Require(HostImportFor(importing, address, 2 * piece) == nullptr, "span failure was not exercised");
    remove(1);
    access.limitMemory(std::nullopt);
    const auto* shortened = HostImportFor(importing, address, piece);
    Require(shortened != nullptr && shortened->bytes == piece, "a failed span suppressed an import of its remaining range");
    remove(0);
    ClearHostImports(importing.device);
    ::operator delete(block, std::align_val_t{piece});
}

void spaceImportRunTests(const Context& context) {
    constexpr std::size_t piece = 65536;
    void* block = ::operator new(2 * piece, std::align_val_t{piece});
    const auto first = reinterpret_cast<std::uintptr_t>(block);
    std::memset(block, 0x3c, 2 * piece);
    auto importing = context;
    importing.hostImportAlignment = piece;
    const auto change = [&](std::uint8_t* range, bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, piece, true, false, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, range);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    auto* guest = static_cast<std::uint8_t*>(block);
    change(guest, true);
    change(guest + piece, true);
    for (int build = 0; build < 2; ++build) {
        const auto before = AddressSpaceCounters();
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.AddReadable(first + piece - 256, 512);
        leased.Upload(true);
        std::uint32_t adjustment = 0;
        const auto view = leased.Descriptor(first + piece - 256, 512, adjustment);
        Require(view.buffer != VK_NULL_HANDLE && view.range == 512 + adjustment, "a V# across one import's ranges has no descriptor");
        const auto ranges = leased.AddressRanges();
        Require(std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin <= first && first + 2 * piece <= range.end; }), "one import's adjacent ranges are not one BDA table entry");
        leased.WriteBack();
        const auto after = AddressSpaceCounters();
        if (after.enabled && build == 1) Require(after.hits == before.hits + 1 && after.dissolvedOverlap == before.dissolvedOverlap, "a V# across one import's ranges dissolved the cached address space");
    }
    change(guest, false);
    change(guest + piece, false);
    ::operator delete(block, std::align_val_t{piece});
}

void spaceReadListTests(const Context& context) {
    if (!AddressSpaceCounters().enabled) return;
    constexpr std::size_t piece = 65536;
    void* block = ::operator new(3 * piece, std::align_val_t{piece});
    std::memset(block, 0x5c, 3 * piece);
    auto importing = context;
    importing.hostImportAlignment = piece;
    auto* guest = static_cast<std::uint8_t*>(block);
    const auto change = [&](std::uint8_t* range, bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, piece, true, true, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, range);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    const auto build = [&](const char* step) {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.Upload(true);
        const auto split = leased.SplitInPlaceReads();
        Require(split.has_value() && split->space != nullptr, std::string("an address-based build that took the space whole has no split reads: ") + step);
        auto joined = *split->space;
        joined.insert(joined.end(), split->own.begin(), split->own.end());
        Require(joined == leased.InPlaceReads(), std::string("the split reads differ from InPlaceReads: ") + step);
        Require(Recorder::ValidReadList(*split->space), std::string("the space's read list is not sorted and disjoint: ") + step);
        leased.WriteBack();
        Require(!leased.SplitInPlaceReads().has_value(), "a committed build still offers split reads");
        return split->space;
    };
    const auto lists = [&](const std::shared_ptr<const Recorder::ReadList>& list, const std::uint8_t* range) {
        const auto begin = reinterpret_cast<std::uintptr_t>(range);
        return std::any_of(list->begin(), list->end(), [&](const auto& read) { return read.first <= begin && begin + piece <= read.second; });
    };
    change(guest, true);
    change(guest + piece, true);
    const auto first = build("first build");
    Require(lists(first, guest) && lists(first, guest + piece) && !lists(first, guest + 2 * piece), "the space's read list does not hold exactly the registered pieces");
    const auto again = build("unchanged registry");
    Require(again == first, "a build in an unchanged registry did not share the space's read list");
    change(guest + 2 * piece, true);
    const auto added = build("range added");
    Require(added != first && lists(added, guest + 2 * piece), "a registered range is missing from the read list after the registry changed");
    change(guest + piece, false);
    const auto removed = build("range removed");
    Require(removed != added && !lists(removed, guest + piece) && lists(removed, guest), "a removed range is still in the read list");
    {
        GuestBufferMemory plain(importing);
        plain.AddReadable(reinterpret_cast<std::uintptr_t>(guest), 256);
        plain.Upload(false);
        Require(!plain.SplitInPlaceReads().has_value(), "a build without the address space offers split reads");
        plain.WriteBack();
    }
    change(guest, false);
    change(guest + 2 * piece, false);
    ::operator delete(block, std::align_val_t{piece});
}

void importWindowTests(const Context& context) {
    constexpr std::size_t piece = 65536;
    constexpr std::size_t window = 2 * piece;
    void* block = ::operator new(4 * piece, std::align_val_t{window});
    auto* guest = static_cast<std::uint8_t*>(block);
    const auto first = reinterpret_cast<std::uintptr_t>(block);
    auto importing = context;
    importing.hostImportAlignment = piece;
    SetHostImportWindow(window);
    const auto change = [&](std::uint8_t* range, bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, piece, true, false, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, range);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    for (std::size_t index = 0; index < 3; ++index) change(guest + index * piece, true);
    const auto* low = HostImportFor(importing, first + piece - 16, 32);
    Require(low != nullptr && low->base == first && low->bytes == window, "adjacent ranges inside one window were not imported as one span");
    const auto lowBuffer = low->buffer;
    const auto* high = HostImportFor(importing, first + window, piece);
    Require(high != nullptr && high->base == first + window && high->bytes == piece, "a span crossed the import window");
    Require(HostImportFor(importing, first + window - 16, 32) == nullptr, "a view crossing the import window was imported");
    change(guest + 3 * piece, true);
    const auto* grown = HostImportFor(importing, first + window, window);
    Require(grown != nullptr && grown->base == first + window && grown->bytes == window, "the next window did not grow into the new range");
    const auto* kept = HostImportFor(importing, first, 16);
    Require(kept != nullptr && kept->buffer == lowBuffer, "growth in one window re-imported another");
    for (std::size_t index = 0; index < 4; ++index) change(guest + index * piece, false);
    SetHostImportWindow(0);
    ::operator delete(block, std::align_val_t{window});
}

void importJoinTests(const Context& context) {
    constexpr std::size_t piece = 65536;
    constexpr std::size_t window = 2 * piece;
    constexpr std::size_t large = 3 * piece;
    void* block = ::operator new(3 * large + 5 * piece, std::align_val_t{window});
    auto* guest = static_cast<std::uint8_t*>(block);
    std::memset(guest, 0x11, 3 * large);
    const auto first = reinterpret_cast<std::uintptr_t>(block);
    const auto second = first + large;
    const auto third = second + large;
    auto importing = context;
    importing.hostImportAlignment = piece;
    SetHostImportWindow(window);
    const auto change = [&](std::uint8_t* range, std::size_t bytes, bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, bytes, true, false, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, range);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    const auto bind = [&](std::uintptr_t address, std::size_t bytes = 32) {
        GuestBufferMemory plain(importing);
        plain.AddReadable(address, bytes);
        plain.Upload(false);
        std::uint32_t adjustment = 0;
        const auto view = plain.Descriptor(address, bytes, adjustment);
        const auto copied = plain.CopiedBytes();
        plain.WriteBack();
        return std::pair{view.buffer, copied};
    };
    change(guest, large, true);
    change(guest + large, large, true);
    const auto* low = HostImportFor(importing, first, 16);
    Require(low != nullptr && low->base == first && low->bytes == large, "a range larger than the import window was not imported alone");
    const auto* high = HostImportFor(importing, second, 16);
    Require(high != nullptr && high->base == second && high->bytes == large, "the next range larger than the import window was not imported alone");
    Require(HostImportFor(importing, second - 16, 32) == nullptr, "a view across two imported ranges was imported before it was bound");
    {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.AddReadable(second - 16, 32);
        leased.Upload(true);
        const auto* joined = HostImportFor(importing, second - 16, 32);
        Require(joined != nullptr && joined->base == first && joined->bytes == 2 * large, "a buffer bound across two adjacent ranges larger than the import window was not imported as one");
        std::uint32_t adjustment = 0;
        const auto crossing = leased.Descriptor(second - 16, 32, adjustment);
        Require(crossing.buffer == joined->buffer && crossing.offset + adjustment == second - 16 - first, "a buffer bound across the joined ranges is not bound in place");
        leased.WriteBack();
    }
    const auto joinedBuffer = HostImportFor(importing, first, 16)->buffer;
    const auto [again, copied] = bind(second - 16);
    Require(again == joinedBuffer && copied == 0, "a later bind across the joined ranges did not use the joined import");
    auto* heap = guest + 3 * large + 2 * piece;
    change(heap, piece, true);
    change(heap + piece, piece, true);
    Require(bind(reinterpret_cast<std::uintptr_t>(heap + piece) - 16).second != 0 && HostImportFor(importing, reinterpret_cast<std::uintptr_t>(heap + piece) - 16, 32) == nullptr, "two pieces no larger than the import window were joined");
    const auto heapBase = reinterpret_cast<std::uintptr_t>(heap);
    const auto [piecesBuffer, piecesCopied] = bind(heapBase, 2 * piece);
    const auto* pieces = HostImportFor(importing, heapBase, 2 * piece);
    Require(pieces != nullptr && pieces->base == heapBase && pieces->bytes == 2 * piece && piecesBuffer == pieces->buffer && piecesCopied == 0, "a buffer bound exactly over two pieces no larger than the import window was not imported as one");
    change(heap + 2 * piece, piece, true);
    static_cast<void>(bind(heapBase + 2 * piece - 16));
    const auto* kept = HostImportFor(importing, first, 16);
    Require(kept != nullptr && kept->buffer == joinedBuffer && kept->bytes == 2 * large, "a registry change elsewhere re-imported the joined ranges");
    change(guest + 2 * large, large, true);
    Require(HostImportFor(importing, first, 16)->buffer == joinedBuffer, "a range registered next to the joined ranges re-imported them");
    const auto [grownBuffer, grownCopied] = bind(third - 16);
    const auto* grown = HostImportFor(importing, third - 16, 32);
    Require(grown != nullptr && grown->base == first && grown->bytes == 3 * large && grownBuffer == grown->buffer && grownCopied == 0, "a bind across the next range did not grow the joined import");
    Require(bind(second - 16).first == grownBuffer && HostImportFor(importing, first, 16)->buffer == grownBuffer, "two joins sharing a range retired each other");
    change(heap, piece, false);
    change(heap + piece, piece, false);
    change(heap + 2 * piece, piece, false);
    change(guest + large, large, false);
    const auto* alone = HostImportFor(importing, first, 16);
    Require(alone != nullptr && alone->base == first && alone->bytes == large, "the joined import outlived one of its ranges");
    change(guest, large, false);
    change(guest + 2 * large, large, false);
    Require(HostImportFor(importing, first, 16) == nullptr && HostImportFor(importing, third, 16) == nullptr, "the host imports outlived their ranges");
    SetHostImportWindow(0);
    ::operator delete(block, std::align_val_t{window});
}

// One registered range of `cells` import windows of `window` bytes, with the window and the import budget set for the
// test and reset after it.
class WindowedRange {
public:
    WindowedRange(const Context& context, std::size_t window, std::size_t cells, std::uint64_t budget = 0) : window(window), bytes(window * cells), importing(context) {
        block = ::operator new(bytes, std::align_val_t{piece});
        std::memset(block, 0x33, bytes);
        importing.hostImportAlignment = piece;
        SetHostImportRangeWindow(window);
        SetHostImportBudget(budget);
        change(true);
    }
    ~WindowedRange() {
        change(false);
        SetHostImportBudget(0);
        SetHostImportRangeWindow(std::uint64_t{64} << 20u);
        ::operator delete(block, std::align_val_t{piece});
    }
    WindowedRange(const WindowedRange&) = delete;
    WindowedRange& operator=(const WindowedRange&) = delete;

    std::uintptr_t First() const { return reinterpret_cast<std::uintptr_t>(block); }

    static constexpr std::size_t piece = 65536;
    const std::size_t window;
    const std::size_t bytes;
    Context importing;

private:
    void change(bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, block, bytes, true, false, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, block);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    }

    void* block = nullptr;
};

// A registered range larger than the import window, its first window imported by an earlier access: an address-based
// build (AcquireRegistered) must import the whole range, so a view in its last window binds (Demon's Souls threw
// "guest buffer view ... exceeds its GPU owner" with the 64 MiB windows).
void importWholeRangeOverWindowTests(const Context& context) {
    WindowedRange range(context, 2 * WindowedRange::piece, 2);
    const auto first = range.First();
    const auto* windowed = HostImportFor(range.importing, first + 16, 32);
    Require(windowed != nullptr && windowed->base == first && windowed->bytes == range.window, "an access to the first window of a large range was not imported as that window");
    GuestBufferMemory leased(range.importing);
    leased.AcquireRegistered();
    leased.Upload(true);
    std::uint32_t adjustment = 0;
    const auto view = leased.Descriptor(first + range.bytes - WindowedRange::piece + 16, 32, adjustment);
    Require(view.range == 32 + adjustment, "a view in the last window of a large registered range has no descriptor");
    const auto* whole = HostImportFor(range.importing, first, range.bytes);
    Require(whole != nullptr && whole->base == first && whole->bytes == range.bytes && view.buffer == whole->buffer, "an address-based build was served by the range's first window, not an import of the whole range");
    leased.WriteBack();
}

// An address-based build whose whole-range import does not fit the budget leaves the range's window alone: before,
// every such build retired the window and the next access imported it again.
void importRefusedWholeRangeTests(const Context& context) {
    WindowedRange range(context, 2 * WindowedRange::piece, 4, 3 * 2 * WindowedRange::piece);
    const auto first = range.First();
    const auto* windowed = HostImportFor(range.importing, first + 16, 32);
    Require(windowed != nullptr && windowed->bytes == range.window, "the first window of a range over the budget was not imported");
    const auto buffer = windowed->buffer;
    for (int build = 0; build < 3; ++build) {
        GuestBufferMemory leased(range.importing);
        leased.AcquireRegistered();
        leased.Upload(true);
        leased.WriteBack();
        Require(HostImportCovers(range.importing, first + 16, 32), "an address-based build refused its whole-range import and retired the range's window (build " + std::to_string(build) + ")");
    }
    const auto* kept = HostImportFor(range.importing, first + 16, 32);
    Require(kept != nullptr && kept->buffer == buffer, "the range's window was imported again after a refused whole-range import");
}

// Two uses across neighbouring cell edges of one range, each in a build of its own, share one import: the second grows
// the first's window to their union, and later rounds of both keep it. Before, each replaced the other's window at
// every use.
void importNeighbouringEdgesTests(const Context& context) {
    WindowedRange range(context, 2 * WindowedRange::piece, 3);
    const auto first = range.First();
    const auto use = [&](std::uintptr_t address) {
        GuestBufferMemory leased(range.importing);
        leased.AddReadable(address, 32);
        leased.Upload(false);
        leased.WriteBack();
    };
    const auto acrossFirst = first + range.window - 16;
    const auto acrossSecond = first + 2 * range.window - 16;
    use(acrossFirst);
    Require(HostImportCovers(range.importing, acrossFirst, 32), "a use across the first cell edge was not imported");
    use(acrossSecond);
    Require(HostImportCovers(range.importing, first, range.bytes), "a use across the second cell edge did not grow the first use's window to their union");
    const auto serial = HostImportSerial(range.importing, first, range.bytes, false);
    for (int round = 0; round < 10; ++round) {
        use(acrossFirst);
        use(acrossSecond);
        Require(HostImportSerial(range.importing, first, range.bytes, false) == serial, "a use across a cell edge replaced the shared window (round " + std::to_string(round) + ")");
    }
}

// A range mapped while the imports are at the budget is refused at map time without retiring an import: the map
// observer holds no GPU lock, so it must not evict (before, it evicted the least recently used import to fit).
void importAtMapOverBudgetTests(const Context& context) {
    constexpr std::size_t bytes = 65536;
    auto importing = context;
    importing.hostImportAlignment = bytes;
    void* kept = ::operator new(bytes, std::align_val_t{bytes});
    void* used = ::operator new(bytes, std::align_val_t{bytes});
    void* mapped = ::operator new(bytes, std::align_val_t{bytes});
    const auto change = [&](void* block, bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, block, bytes, true, false, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, block);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    change(kept, true);
    change(used, true);
    const auto keptAddress = reinterpret_cast<std::uintptr_t>(kept);
    const auto usedAddress = reinterpret_cast<std::uintptr_t>(used);
    Require(HostImportFor(importing, keptAddress, 16) != nullptr, "the import to keep was not made");
    for (int use = 0; use < 300; ++use) Require(HostImportFor(importing, usedAddress, 16) != nullptr, "the import in use was not made");
    {
        GuestBufferMemory leased(importing);
        leased.AddReadable(usedAddress, 16);
        leased.Upload(false);
        leased.WriteBack();
    }
    const auto live = HostImportLiveBytes();
    SetHostImportBudget(live + bytes - 1);
    change(mapped, true);
    GuestAllocations::Mapped ranges;
    {
        const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        for (const auto& range : lease) {
            if (range->address == reinterpret_cast<std::uintptr_t>(mapped)) ranges.push_back(range);
        }
    }
    Require(ranges.size() == 1, "the mapped range is not registered");
    static_cast<void>(ImportMappedRanges(importing, ranges, GuestAllocations::GuestAllocationsGeneration_nid_postfix(), true));
    SetHostImportBudget(0);
    Require(HostImportLiveBytes() == live, "a range mapped at the budget retired an import at map time (an eviction without the GPU lock)");
    Require(HostImportCovers(importing, keptAddress, 16) && !HostImportCovers(importing, reinterpret_cast<std::uintptr_t>(mapped), 16), "a range mapped at the budget was imported, or the import to keep was retired");
    for (auto* block : {kept, used, mapped}) change(block, false);
    for (auto* block : {kept, used, mapped}) ::operator delete(block, std::align_val_t{bytes});
}

void spaceReadAcrossImportsTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t piece = 65536;
    constexpr std::size_t window = 2 * piece;
    void* block = ::operator new(3 * piece, std::align_val_t{window});
    auto* guest = static_cast<std::uint8_t*>(block);
    const auto first = reinterpret_cast<std::uintptr_t>(block);
    const auto boundary = first + window;
    std::memset(guest, 0x5a, window);
    std::memset(guest + window, 0x6b, piece);
    auto importing = context;
    importing.hostImportAlignment = piece;
    SetHostImportWindow(window);
    const auto change = [&](std::uint8_t* range, bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, piece, true, false, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, range);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    for (std::size_t index = 0; index < 3; ++index) change(guest + index * piece, true);
    for (int build = 0; build < 2; ++build) {
        const auto before = AddressSpaceCounters();
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.AddReadable(boundary - 256, 512);
        leased.Upload(true);
        std::uint32_t adjustment = 0;
        const auto view = leased.Descriptor(boundary - 256, 512, adjustment);
        Require(view.buffer != VK_NULL_HANDLE && view.range == 512 + adjustment, "a read-only V# across two imports has no descriptor");
        const auto bytes = access.bytes(view.buffer);
        Require(view.offset + adjustment + 512 <= bytes.size() && bytes[view.offset + adjustment] == std::byte{0x5a} && bytes[view.offset + adjustment + 256] == std::byte{0x6b}, "a read-only V# across two imports misses the bytes of either range");
        const auto ranges = leased.AddressRanges();
        Require(std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin <= first && boundary <= range.end; }) && std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin <= boundary && boundary + piece <= range.end; }), "an import is missing from the BDA table of a build with a read-only V# across it");
        leased.WriteBack();
        const auto after = AddressSpaceCounters();
        if (after.enabled && build == 1) Require(after.hits == before.hits + 1 && after.dissolvedOverlap == before.dissolvedOverlap, "a read-only V# across two imports dissolved the cached address space");
    }
    for (std::size_t index = 0; index < 3; ++index) change(guest + index * piece, false);
    SetHostImportWindow(0);
    ::operator delete(block, std::align_val_t{window});
}

void spaceReadAcrossImportsWrittenTests(const Context& context) {
    constexpr std::size_t piece = 65536;
    constexpr std::size_t window = 2 * piece;
    void* block = ::operator new(3 * piece, std::align_val_t{window});
    auto* guest = static_cast<std::uint8_t*>(block);
    const auto boundary = reinterpret_cast<std::uintptr_t>(block) + window;
    std::memset(guest, 0x5a, window);
    std::memset(guest + window, 0x6b, piece);
    auto importing = context;
    importing.hostImportAlignment = piece;
    SetHostImportWindow(window);
    const auto change = [&](std::uint8_t* range, bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, piece, true, false, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, range);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    for (const bool writtenFirst : {false, true}) {
        for (std::size_t index = 0; index < 3; ++index) change(guest + index * piece, true);
        for (int build = 0; build < 2; ++build) {
            const auto before = AddressSpaceCounters();
            GuestBufferMemory leased(importing);
            leased.AcquireRegistered();
            if (writtenFirst) leased.AddWritable(boundary - 384, 256);
            leased.AddReadable(boundary - 256, 512);
            if (!writtenFirst) leased.AddWritable(boundary - 384, 256);
            leased.Upload(true);
            std::uint32_t readAdjustment = 0;
            std::uint32_t writeAdjustment = 0;
            const auto read = leased.Descriptor(boundary - 256, 512, readAdjustment);
            const auto written = leased.Descriptor(boundary - 384, 256, writeAdjustment);
            Require(read.buffer != VK_NULL_HANDLE && read.buffer == written.buffer && written.offset + writeAdjustment + 128 == read.offset + readAdjustment, "a written V# inside a read-only V# across two imports maps other bytes than it");
            leased.WriteBack();
            const auto after = AddressSpaceCounters();
            if (after.enabled && build == 0) Require(after.dissolvedOverlap == before.dissolvedOverlap + 1, "a written V# inside a read-only V# across two imports kept the cached address space");
        }
        for (std::size_t index = 0; index < 3; ++index) change(guest + index * piece, false);
    }
    SetHostImportWindow(0);
    ::operator delete(block, std::align_val_t{window});
}

void spaceReadsAcrossImportsMergedTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t piece = 65536;
    constexpr std::size_t window = 2 * piece;
    constexpr std::size_t pieces = 6;
    void* block = ::operator new(pieces * piece, std::align_val_t{window});
    auto* guest = static_cast<std::uint8_t*>(block);
    const auto first = reinterpret_cast<std::uintptr_t>(block);
    for (std::size_t index = 0; index < pieces; ++index) std::memset(guest + index * piece, static_cast<int>(0x10 + index), piece);
    auto importing = context;
    importing.hostImportAlignment = piece;
    SetHostImportWindow(window);
    const auto change = [&](std::uint8_t* range, bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, piece, true, range == guest + 3 * piece, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, range);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    for (std::size_t index = 0; index < pieces; ++index) change(guest + index * piece, true);
    const auto second = first + window;
    const auto third = first + 2 * window;
    const std::uint64_t lower = second - 256;
    const std::uint64_t lowerBytes = 512;
    const std::uint64_t upper = second + 128;
    const std::uint64_t upperBytes = third + 256 - upper;
    for (int build = 0; build < 3; ++build) {
        const auto before = AddressSpaceCounters();
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.AddReadable(lower, lowerBytes);
        leased.AddReadable(upper, upperBytes);
        leased.UploadPrepare(true);
        if (build == 2) ClearHostImports(importing.device);
        leased.UploadFinish(true);
        for (const auto& [address, bytes] : {std::pair{lower, lowerBytes}, std::pair{upper, upperBytes}}) {
            std::uint32_t adjustment = 0;
            const auto view = leased.Descriptor(address, bytes, adjustment);
            Require(view.buffer != VK_NULL_HANDLE && view.range == bytes + adjustment, "a read-only V# over imports, merged with another one, has no descriptor of its size");
            const auto mapped = access.bytes(view.buffer);
            const auto at = static_cast<std::size_t>(view.offset + adjustment);
            const auto expected = [&](std::uint64_t guestAddress) { return std::byte{static_cast<unsigned char>(0x10 + (guestAddress - first) / piece)}; };
            Require(at + bytes <= mapped.size() && mapped[at] == expected(address) && mapped[at + bytes - 1] == expected(address + bytes - 1), "a read-only V# over imports, merged with another one, maps other bytes than its own");
        }
        leased.WriteBack();
        const auto after = AddressSpaceCounters();
        if (after.enabled && build > 0) Require(after.hits == before.hits + 1 && after.dissolvedOverlap == before.dissolvedOverlap && after.dissolvedImports == before.dissolvedImports + (build == 2 ? 1u : 0u), "read-only V#s over imports, merged with each other, left the cached address space other than by the import change");
    }
    for (std::size_t index = 0; index < pieces; ++index) change(guest + index * piece, false);
    SetHostImportWindow(0);
    ::operator delete(block, std::align_val_t{window});
}

void spaceWriteMergedPastImportTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t piece = 65536;
    constexpr std::size_t window = 2 * piece;
    constexpr std::size_t pieces = 5;
    void* block = ::operator new(pieces * piece, std::align_val_t{window});
    auto* guest = static_cast<std::uint8_t*>(block);
    const auto first = reinterpret_cast<std::uintptr_t>(block);
    const auto second = first + window;
    const auto outside = first + 2 * window;
    auto importing = context;
    importing.hostImportAlignment = piece;
    SetHostImportWindow(window);
    const auto change = [&](std::uint8_t* range, bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, piece, true, range == guest + 3 * piece, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, range);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    for (std::size_t index = 0; index + 1 < pieces; ++index) change(guest + index * piece, true);
    const std::uint64_t written = outside + 512;
    for (int build = 0; build < 2; ++build) {
        std::memset(guest, 0x21, pieces * piece);
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.AddReadable(second - 256, window + 512);
        leased.AddReadable(outside + 128, 640);
        leased.AddWritable(written, 256);
        leased.Upload(true);
        std::uint32_t adjustment = 0;
        const auto view = leased.Descriptor(written, 256, adjustment);
        Require(view.buffer != VK_NULL_HANDLE && view.range == 256 + adjustment, "a written V# merged with a read-only V# past an import has no descriptor");
        auto mapped = access.bytes(view.buffer);
        const auto at = static_cast<std::size_t>(view.offset + adjustment);
        Require(at + 256 <= mapped.size() && mapped[at] == std::byte{0x21} && mapped[at + 255] == std::byte{0x21}, "a written V# merged with a read-only V# past an import maps other bytes than its own");
        std::memset(mapped.data() + at, 0x7c, 256);
        Require(leased.HasCopiedWrites(), "a written V# merged with a read-only V# past an import is not stored back");
        leased.WriteBack();
        const auto offset = static_cast<std::size_t>(written - first);
        Require(guest[offset] == 0x7c && guest[offset + 255] == 0x7c && guest[offset - 1] == 0x21 && guest[offset + 256] == 0x21, "a write through a V# merged with a read-only V# past an import did not land on its own bytes");
    }
    for (std::size_t index = 0; index + 1 < pieces; ++index) change(guest + index * piece, false);
    SetHostImportWindow(0);
    ::operator delete(block, std::align_val_t{window});
}

void spaceReadOverWritableImportTableTests(const Context& context) {
    constexpr std::size_t piece = 65536;
    constexpr std::size_t window = 2 * piece;
    void* block = ::operator new(3 * piece, std::align_val_t{window});
    auto* guest = static_cast<std::uint8_t*>(block);
    const auto first = reinterpret_cast<std::uintptr_t>(block);
    const auto boundary = first + window;
    std::memset(guest, 0x5a, 3 * piece);
    auto importing = context;
    importing.hostImportAlignment = piece;
    SetHostImportWindow(window);
    const auto change = [&](std::uint8_t* range, bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, piece, true, range != guest + 2 * piece, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, range);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    for (std::size_t index = 0; index < 3; ++index) change(guest + index * piece, true);
    const auto lookup = [](const std::vector<ShaderRecompiler::BdaAbi::Range>& ranges, std::uint64_t address) {
        const auto found = std::upper_bound(ranges.begin(), ranges.end(), address, [](std::uint64_t value, const auto& range) { return value < range.begin; });
        return found == ranges.begin() ? ranges.end() : std::prev(found);
    };
    for (int build = 0; build < 3; ++build) {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.AddReadable(boundary - 256, 512);
        leased.UploadPrepare(true);
        if (build == 2) ClearHostImports(importing.device);
        leased.UploadFinish(true);
        std::uint32_t adjustment = 0;
        const auto view = leased.Descriptor(boundary - 256, 512, adjustment);
        Require(view.buffer != VK_NULL_HANDLE && view.range == 512 + adjustment, "a read-only V# across a writable and a read-only import has no descriptor");
        const auto ranges = leased.AddressRanges();
        Require(std::adjacent_find(ranges.begin(), ranges.end(), [](const auto& left, const auto& right) { return right.begin < left.end; }) == ranges.end(), "the BDA table of a build with a read-only V# across imports has overlapping ranges");
        for (const auto address : {first + 16, boundary - 256, boundary - 128, boundary - 16}) {
            const auto found = lookup(ranges, address);
            Require(found != ranges.end() && address + 16 <= found->end && (found->permissions & ShaderRecompiler::BdaAbi::Write) != 0u, "a BDA store into a writable import resolves to the read-only copy of a V# across it");
        }
        for (const auto address : {boundary, boundary + 64, boundary + piece - 32}) {
            const auto found = lookup(ranges, address);
            Require(found != ranges.end() && address + 16 <= found->end && (found->permissions & ShaderRecompiler::BdaAbi::Read) != 0u, "a BDA read past a read-only V# across imports finds no range");
        }
        leased.WriteBack();
    }
    for (std::size_t index = 0; index < 3; ++index) change(guest + index * piece, false);
    SetHostImportWindow(0);
    ::operator delete(block, std::align_val_t{window});
}

ShaderRecompiler::DescriptorBinding binding(Role role, std::uint32_t slot) {
    return {ShaderRecompiler::DescriptorKind::StorageBuffer, role, 0, slot, 1, {}, false};
}

void importedHeapMirrorTests(const Context& context, const BdaTestAccess& access) {
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "guest arena unavailable or not write-watched: heap mirrors of imported ranges not tested\n";
        return;
    }
    constexpr std::size_t bytes = 1u << 20u;
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, bytes);
#ifdef _WIN32
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#endif
    std::memset(block, 0x11, bytes);
    const auto address = reinterpret_cast<std::uintptr_t>(block);
    auto importing = context;
    importing.hostImportAlignment = bytes;
    const auto registry = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, block, bytes, true, false, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, block);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    const auto build = [&](const Context& with) {
        GuestAllocations::GuestAllocationsEnd_nid_postfix(GuestAllocations::GuestAllocationsBegin_nid_postfix());
        GuestBufferMemory leased(with);
        leased.AcquireRegistered();
        leased.Upload(true);
        const auto ranges = leased.AddressRanges();
        const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == address && range.end == address + bytes; });
        const auto device = found != ranges.end() ? found->deviceAddress : 0;
        leased.WriteBack();
        return device;
    };
    const auto before = MirrorCounters();
    registry(true);
    Require(build(context) != 0, "the heap range is missing from the BDA table of a build without host imports");
    const auto mirrored = MirrorCounters();
    Require(mirrored.heapMirrors == before.heapMirrors + 1 && mirrored.heapBytes == before.heapBytes + bytes, "a range built without host imports was not heap mirrored");
    Require(build(importing) != 0 && HostImportCovers(importing, address, bytes), "the heap mirrored range was not imported once imports were available");
    const auto imported = MirrorCounters();
    Require(imported.heapMirrors == before.heapMirrors && imported.heapBytes == before.heapBytes, "a heap mirror outlived the host import that serves its range");
    Require(build(importing) != 0 && MirrorCounters().heapMirrors == before.heapMirrors && MirrorCounters().rebuilds == imported.rebuilds, "an imported range was heap mirrored again");
    static_cast<std::uint8_t*>(block)[bytes / 2] = 0x22;
    const auto device = build(context);
    const auto again = MirrorCounters();
    Require(device != 0 && again.heapMirrors == before.heapMirrors + 1 && again.heapBytes == before.heapBytes + bytes && again.rebuilds == imported.rebuilds + 1, "a range no import serves was not heap mirrored again");
    Require(access.addressBytes(device)[bytes / 2] == std::byte{0x22} && access.addressBytes(device)[bytes / 2 + 1] == std::byte{0x11}, "a heap mirror made after its import missed the guest bytes");
    registry(false);
    Require(HostImportFor(importing, address, bytes) == nullptr && !HostImportCovers(importing, address, bytes), "the import outlived its range");
    Require(build(context) == 0, "the unregistered heap range is still in the BDA table");
    Require(MirrorCounters().heapMirrors == before.heapMirrors && MirrorCounters().heapBytes == before.heapBytes, "the heap mirror outlived its range");
#ifdef _WIN32
    GuestArena::GuestArenaReset_nid_postfix(block, bytes);
#endif
    GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
}

void importedFreshTests(const Context& context) {
#ifndef _WIN32
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::cout << "write watch unavailable: imported never-collected pages not tested\n";
        return;
    }
    constexpr std::size_t blockBytes = 65536;
    void* area = mmap(nullptr, 2 * blockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(area != MAP_FAILED, "cannot map the imported block");
    const auto base = (reinterpret_cast<std::uintptr_t>(area) + blockBytes - 1) & ~(blockBytes - 1);
    std::memset(reinterpret_cast<void*>(base), 0x55, blockBytes);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(reinterpret_cast<void*>(base), blockBytes);
    auto importing = context;
    importing.hostImportAlignment = blockBytes;
    const auto registry = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, reinterpret_cast<void*>(base), blockBytes, true, true, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, reinterpret_cast<void*>(base));
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    registry(true);
    if (HostImportFor(importing, base, blockBytes) == nullptr) {
        std::cout << "the never-collected block was not imported: imported never-collected pages not tested\n";
    } else {
        const auto gpuWrite = AgcDriver::GuestMemory::MarkWritten(base, 4);
        Require(gpuWrite != 0, "a GPU write into an imported never-collected block was not stamped");
        reinterpret_cast<volatile std::uint8_t*>(base)[5 * 4096 + 8] = 0x66;
        static_cast<void>(AgcDriver::GuestMemory::CollectWritesUncached(base, blockBytes));
        Require(AgcDriver::GuestMemory::WrittenSince(base + 5 * 4096, 4096, gpuWrite), "a CPU write to an imported page protected after the import was not seen");
    }
    registry(false);
    Require(HostImportFor(importing, base, blockBytes) == nullptr, "the import outlived its range");
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(reinterpret_cast<void*>(base), blockBytes);
    munmap(area, 2 * blockBytes);
#else
    static_cast<void>(context);
#endif
}

void importWatchTests() {
#ifndef _WIN32
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::cout << "write watch unavailable: import watch not tested\n";
        return;
    }
    constexpr std::size_t bytes = 4 * 65536;
    void* block = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(block != MAP_FAILED, "cannot map the import watch block");
    auto* guest = static_cast<std::uint8_t*>(block);
    const auto address = reinterpret_cast<std::uintptr_t>(block);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
    std::memset(block, 0x22, bytes);
    const auto written = [&] {
        std::size_t reported = 0;
        Require(GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(address, bytes, [](void* context, std::uintptr_t begin, std::uintptr_t end) { *static_cast<std::size_t*>(context) += end - begin; }, &reported), "the write watch did not collect the block");
        return reported;
    };
    Require(AgcDriver::GuestMemory::ImportWatched(address, bytes, [] { return true; }), "importing a block nothing had collected failed");
    Require(written() == bytes, "an import write-protected pages nothing had collected");
    guest[3 * 4096] = 0x33;
    Require(AgcDriver::GuestMemory::ImportWatched(address, bytes, [] { return true; }), "importing a collected block failed");
    Require(written() == 0, "an import did not take the writes to pages a collect had protected");
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, bytes);
    munmap(block, bytes);
    constexpr std::size_t blockBytes = 65536;
    void* area = mmap(nullptr, bytes + 2 * blockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(area != MAP_FAILED, "cannot map the GPU write block");
    const auto aligned = (reinterpret_cast<std::uintptr_t>(area) + blockBytes - 1) & ~(blockBytes - 1);
    const auto imported = aligned + 4096;
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(reinterpret_cast<void*>(imported), bytes);
    Require(AgcDriver::GuestMemory::ImportWatched(imported, bytes, [] { return true; }), "importing a fresh range failed");
    const auto gpuWrite = AgcDriver::GuestMemory::MarkWritten(imported + 4096, 4096);
    Require(gpuWrite != 0, "a GPU write into the imported range was not stamped");
    static_cast<void>(AgcDriver::GuestMemory::CollectWritesUncached(imported, bytes));
    Require(!AgcDriver::GuestMemory::WrittenSince(imported + 4096, 4096, gpuWrite), "a collect after a GPU write into an uncollected import read as a CPU write over the GPU's results");
    reinterpret_cast<volatile std::uint8_t*>(imported)[4096 + 100] = 0x44;
    static_cast<void>(AgcDriver::GuestMemory::CollectWritesUncached(imported, bytes));
    Require(AgcDriver::GuestMemory::WrittenSince(imported + 4096, 4096, gpuWrite), "a CPU write after a GPU write into an import was not seen");
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(reinterpret_cast<void*>(imported), bytes);
    munmap(area, bytes + 2 * blockBytes);

    void* labels = mmap(nullptr, 2 * blockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(labels != MAP_FAILED, "cannot map the label block");
    const auto label = (reinterpret_cast<std::uintptr_t>(labels) + blockBytes - 1) & ~(blockBytes - 1);
    auto* words = reinterpret_cast<volatile std::uint32_t*>(label);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(reinterpret_cast<void*>(label), blockBytes);
    words[0] = 1;
    static_cast<void>(AgcDriver::GuestMemory::CollectWritesUncached(label, 4));
    const auto before = AgcDriver::GuestMemory::TrackerGeneration();
    Require(AgcDriver::GuestMemory::MarkWritten(label, 4) != 0, "a GPU write next to never-collected pages was not stamped");
    Require(AgcDriver::GuestMemory::UnchangedSinceCollected(label, 4, before), "a GPU write next to never-collected pages read as a CPU write of its block");
    Require(AgcDriver::GuestMemory::StoredOver(label + 8 * 4096, 4096, before), "a never-collected page a GPU write collected is not reported as possibly stored");
    const std::array<std::uint64_t, 1> since{before};
    std::array<std::uint8_t, 1> changed{};
    std::array<std::uint8_t, 1> cpu{};
    Require(AgcDriver::GuestMemory::ChangedBlocks(label, blockBytes, since, changed, cpu) && changed[0] == AgcDriver::GuestMemory::BlockWritten && cpu[0] == 0, "collecting never-collected pages at a GPU write stamped their block as written by the CPU");
    const auto now = AgcDriver::GuestMemory::TrackerGeneration();
    Require(AgcDriver::GuestMemory::MarkWritten(label, 4) == now + 1, "a GPU write into a block with nothing left to collect advanced the generation more than once");
    words[1] = 2;
    Require(!AgcDriver::GuestMemory::UnchangedSinceCollected(label, 4, before), "a CPU write next to a GPU write was not seen");
    void* awaitedArea = mmap(nullptr, 2 * blockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(awaitedArea != MAP_FAILED, "cannot map the awaited block");
    const auto awaitedBlock = (reinterpret_cast<std::uintptr_t>(awaitedArea) + blockBytes - 1) & ~(blockBytes - 1);
    const auto awaited = awaitedBlock + 5 * 4096;
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(reinterpret_cast<void*>(awaitedBlock), blockBytes);
    static_cast<void>(AgcDriver::GuestMemory::CollectWritesUncached(awaitedBlock, 4));
    const auto closed = AgcDriver::GuestMemory::TrackerGeneration();
    *reinterpret_cast<volatile std::uint32_t*>(awaited) = 7;
    Require(AgcDriver::GuestMemory::MarkWritten(awaitedBlock, 4) != 0, "a GPU write into the awaited block was not stamped");
    Require(!AgcDriver::GuestMemory::UnchangedSinceCollected(awaited, 4, closed), "a CPU store to a never-collected page that a GPU write elsewhere in its block collected was not seen");
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(reinterpret_cast<void*>(awaitedBlock), blockBytes);
    munmap(awaitedArea, 2 * blockBytes);
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(reinterpret_cast<void*>(label), blockBytes);
    munmap(labels, 2 * blockBytes);
#endif
}

void heapMirrorTests(const Context& context, const BdaTestAccess& access) {
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "guest arena unavailable or not write-watched: heap mirrors not tested\n";
        return;
    }
    constexpr std::size_t bytes = 2 * 65536;
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536);
#ifdef _WIN32
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#endif
    auto* guest = static_cast<std::uint8_t*>(block);
    std::memset(block, 0x11, bytes);
    const auto address = reinterpret_cast<std::uintptr_t>(block);
    const auto registry = [&](bool add, bool writable) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, block, bytes, true, writable, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, block);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    const auto build = [&](std::uint64_t written, const std::function<void(GuestBufferMemory&)>& gpu) {
        GuestBufferMemory leased(context);
        leased.AcquireRegistered();
        if (written != 0) leased.AddWritable(written, 32);
        leased.Upload(true);
        const auto ranges = leased.AddressRanges();
        const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == address; });
        Require(found != ranges.end() && found->deviceAddress != 0u, "the heap range is missing from the BDA table");
        const auto device = found->deviceAddress;
        auto cursor = address;
        std::uint64_t writableBytes = 0;
        for (auto part = found; cursor < address + bytes && part != ranges.end(); ++part) {
            Require(part->begin == cursor && part->end > cursor && part->end <= address + bytes, "heap BDA ranges have gaps or overlap");
            Require(part->deviceAddress == device + cursor - address, "heap BDA ranges are not contiguous on the device");
            const bool writable = written != 0u && part->begin >= written && part->end <= written + 32u;
            const auto permissions = ShaderRecompiler::BdaAbi::Read | (writable ? ShaderRecompiler::BdaAbi::Write : 0u);
            Require(part->permissions == permissions, "heap BDA range has incorrect permissions");
            if (writable) writableBytes += part->end - part->begin;
            cursor = part->end;
        }
        Require(cursor == address + bytes && writableBytes == (written != 0u ? 32u : 0u), "heap BDA coverage or writable extent is incorrect");
        if (gpu) gpu(leased);
        leased.WriteBack();
        return device;
    };
    const auto sweep = [&] {
        GuestBufferMemory leased(context);
        leased.AcquireRegistered();
        leased.Upload(true);
        leased.WriteBack();
    };
    sweep();
    registry(true, false);
    const auto before = MirrorCounters();
    const auto first = build(0, {});
    const auto made = MirrorCounters();
    Require(made.heapMirrors == before.heapMirrors + 1 && made.heapBytes == before.heapBytes + bytes, "a read-only heap range was not mirrored");
    Require(access.addressBytes(first)[65536 + 3] == std::byte{0x11}, "the heap mirror was not filled");
    Require(build(0, {}) == first && MirrorCounters().blocksCopied == made.blocksCopied && MirrorCounters().heapRefills == made.heapRefills, "an unchanged heap range was read into its mirror again");
    guest[65536 + 3] = 0x22;
    Require(build(0, {}) == first && MirrorCounters().heapRefills == made.heapRefills + 1 && MirrorCounters().blocksCopied == made.blocksCopied + 1, "a written heap block was not read again alone");
    Require(access.addressBytes(first)[65536 + 3] == std::byte{0x22}, "the heap mirror missed the CPU write");
    const auto one = MirrorCounters();
    guest[5] = 0x33;
    guest[65536 + 7] = 0x44;
    Require(build(0, {}) == first && MirrorCounters().blocksCopied == one.blocksCopied + 2, "two written heap blocks were not read again");
    Require(access.addressBytes(first)[5] == std::byte{0x33} && access.addressBytes(first)[65536 + 7] == std::byte{0x44}, "the heap mirror missed the CPU writes");
    const auto quiet = MirrorCounters();
    Require(build(0, {}) == first && MirrorCounters().heapChecks == quiet.heapChecks && MirrorCounters().sweeps == quiet.sweeps && MirrorCounters().blocksCopied == quiet.blocksCopied, "an unchanged heap range was compared block by block or the mirrors were swept again");
    guest[65536 + 9] = 0x66;
    Require(build(0, {}) == first && MirrorCounters().heapChecks > quiet.heapChecks && MirrorCounters().blocksCopied == quiet.blocksCopied + 1 && access.addressBytes(first)[65536 + 9] == std::byte{0x66}, "a CPU write after an unchanged build was missed");
    registry(false, false);
    sweep();
    const auto swept = MirrorCounters();
    Require(swept.heapMirrors == before.heapMirrors && swept.heapBytes == before.heapBytes, "the heap mirror outlived its range");

    registry(true, true);
    const auto device = build(address + 16, [&](GuestBufferMemory& leased) {
        std::uint32_t adjustment = 0;
        const auto view = leased.Descriptor(address + 16, 32, adjustment);
        access.bytes(view.buffer)[view.offset + adjustment] = std::byte{0x77};
        guest[16 + 8] = 0x55;
    });
    Require(guest[16] == 0x77 && guest[16 + 8] == 0x55, "a writable heap mirror's write-back lost the GPU's store or rolled back the CPU's");
    Require(build(0, {}) == device && access.addressBytes(device)[16] == std::byte{0x77} && access.addressBytes(device)[16 + 8] == std::byte{0x55}, "the writable heap mirror missed the stores");
    Require(build(address + 40, [&](GuestBufferMemory& leased) {
        std::uint32_t adjustment = 0;
        const auto view = leased.Descriptor(address + 40, 8, adjustment);
        access.bytes(view.buffer)[view.offset + adjustment] = std::byte{0x78};
    }) == device && guest[40] == 0x78, "a writable heap mirror's second write-back lost the GPU's store");
    guest[65536 + 1] = 0x79;
    Require(build(0, {}) == device && access.addressBytes(device)[40] == std::byte{0x78} && access.addressBytes(device)[65536 + 1] == std::byte{0x79}, "the writable heap mirror missed a store after a write-back");
    registry(false, true);
    sweep();
    Require(MirrorCounters().heapMirrors == before.heapMirrors, "the writable heap mirror outlived its range");

#ifdef _WIN32
    {
        constexpr std::size_t half = 2 * 65536;
        void* pair = GuestArena::GuestArenaAllocate_nid_postfix(2 * half, 65536);
        GuestArena::GuestArenaCommit_nid_postfix(pair, 2 * half, PAGE_READWRITE, 2 * half);
        std::memset(pair, 0x11, 2 * half);
        auto* first = static_cast<std::uint8_t*>(pair);
        auto* second = first + half;
        const auto registerPair = [&](bool add) {
            auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
            for (auto* range : {first, second}) {
                if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, half, true, false, true);
                else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, range);
            }
            GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
        };
        const auto firstDevice = [&] {
            GuestBufferMemory leased(context);
            leased.AcquireRegistered();
            leased.Upload(true);
            const auto ranges = leased.AddressRanges();
            const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == reinterpret_cast<std::uintptr_t>(first); });
            Require(found != ranges.end(), "the first heap range is missing from the BDA table");
            const auto device = found->deviceAddress;
            leased.WriteBack();
            return device;
        };
        registerPair(true);
        const auto device = firstDevice();
        first[5] = 0x66;
        DWORD previous = 0;
        Require(VirtualProtect(second, 65536, PAGE_NOACCESS, &previous) != 0, "cannot protect the second heap range");
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(second), 65536);
        bool threw = false;
        try {
            GuestBufferMemory leased(context);
            leased.AcquireRegistered();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(VirtualProtect(second, 65536, PAGE_READWRITE, &previous) != 0, "cannot unprotect the second heap range");
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(second), 65536);
        Require(threw, "a build over an inaccessible heap mirror range did not fail");
        Require(firstDevice() == device && access.addressBytes(device)[5] == std::byte{0x66}, "an interrupted build left a changed heap block marked current");
        registerPair(false);
        sweep();
        GuestArena::GuestArenaReset_nid_postfix(pair, 2 * half);
        GuestArena::GuestArenaRelease_nid_postfix(pair, 2 * half);
    }
#endif
#ifdef _WIN32
    {
        constexpr std::size_t size = 2 * 65536;
        void* raw = GuestArena::GuestArenaAllocate_nid_postfix(size, 65536);
        GuestArena::GuestArenaCommit_nid_postfix(raw, size, PAGE_READWRITE, size);
        auto* bytes8 = static_cast<std::uint8_t*>(raw);
        std::memset(raw, 0x11, size);
        const auto base = reinterpret_cast<std::uintptr_t>(raw);
        const auto page = base + 65536;
        DWORD previous = 0;
        Require(VirtualProtect(reinterpret_cast<void*>(page), 4096, PAGE_READONLY, &previous) != 0, "cannot make the aliased page read-only");
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(page, 4096);
        const auto registerRange = [&](bool add) {
            auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
            if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, raw, size, true, true, true);
            else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, raw);
            GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
        };
        registerRange(true);
        const auto storeAt = [&](std::uint64_t at) {
            GuestBufferMemory leased(context);
            leased.AcquireRegistered();
            leased.AddWritable(page - 16, 32);
            leased.Upload(true);
            const auto ranges = leased.AddressRanges();
            const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == base; });
            Require(found != ranges.end() && access.addressBytes(found->deviceAddress)[65536 + 8] == std::byte{0x11}, "a writable range with a read-only page was not mirrored with its bytes");
            std::uint32_t adjustment = 0;
            const auto view = leased.Descriptor(page - 16, 32, adjustment);
            access.bytes(view.buffer)[view.offset + adjustment + static_cast<std::size_t>(at - (page - 16))] = std::byte{0x77};
            leased.WriteBack();
        };
        const auto made = MirrorCounters().heapMirrors;
        storeAt(page - 8);
        Require(MirrorCounters().heapMirrors == made + 1 && bytes8[65536 - 8] == 0x77, "a store next to a read-only page was not written back");
        bool refused = false;
        try {
            storeAt(page + 4);
        } catch (const std::runtime_error& error) {
            refused = std::string(error.what()).find("aliased writes are not implemented") != std::string::npos;
        }
        Require(refused && bytes8[65536 + 4] == 0x11, "a GPU change of a read-only page was not refused");
        {
            GuestBufferMemory plain(context);
            plain.AddReadable(page, 16);
            plain.Upload(false);
            std::uint32_t adjustment = 0;
            const auto view = plain.Descriptor(page, 16, adjustment);
            Require(access.bytes(view.buffer)[view.offset + adjustment] == std::byte{0x11}, "a descriptor over a read-only page of a writable range read zeros");
            plain.WriteBack();
        }
        registerRange(false);
        sweep();
        Require(VirtualProtect(reinterpret_cast<void*>(page), 4096, PAGE_READWRITE, &previous) != 0, "cannot restore the aliased page");
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(page, 4096);
        GuestArena::GuestArenaReset_nid_postfix(raw, size);
        GuestArena::GuestArenaRelease_nid_postfix(raw, size);
    }
#endif
    {
        constexpr std::size_t large = 36 * 65536;
        constexpr std::size_t small = 17 * 65536;
        constexpr std::size_t total = 2 * large + small;
        void* raw = GuestArena::GuestArenaAllocate_nid_postfix(total, 65536);
#ifdef _WIN32
        GuestArena::GuestArenaCommit_nid_postfix(raw, total, PAGE_READWRITE, total);
#endif
        auto* const heap = static_cast<std::uint8_t*>(raw);
        const std::array<std::uint8_t*, 3> heaps{heap, heap + large, heap + 2 * large};
        const std::array<std::size_t, 3> sizes{large, large, small};
        for (std::size_t index = 0; index < heaps.size(); ++index) std::memset(heaps[index], 0x21 + static_cast<int>(index), sizes[index]);
        const auto change = [&](std::initializer_list<std::size_t> added, std::initializer_list<std::size_t> removed) {
            auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
            for (const auto index : removed) GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, heaps[index]);
            for (const auto index : added) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, heaps[index], sizes[index], true, false, true);
            GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
        };
        const auto mirrored = [&](std::size_t index) {
            GuestBufferMemory leased(context);
            leased.AcquireRegistered();
            leased.Upload(true);
            const auto ranges = leased.AddressRanges();
            const auto begin = reinterpret_cast<std::uintptr_t>(heaps[index]);
            const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& entry) { return entry.begin == begin && entry.end == begin + sizes[index]; });
            Require(found != ranges.end(), "a mirrored heap range is missing from the BDA table");
            const auto last = access.addressBytes(found->deviceAddress)[sizes[index] - 1];
            leased.WriteBack();
            return last;
        };
        sweep();
        const auto before = MirrorCounters();
        change({0}, {});
        Require(mirrored(0) == std::byte{0x21}, "the first heap range was not mirrored");
        const auto one = MirrorCounters();
        access.limitMemory(large - 1);
        change({1}, {0});
        Require(mirrored(1) == std::byte{0x22}, "a heap mirror past APS5_HEAP_MIRROR_MIB was not made in the memory of the expired mirror");
        const auto swept = MirrorCounters();
        Require(swept.heapMirrors == one.heapMirrors && swept.heapBytes == one.heapBytes, "the expired heap mirror was not swept before the allocation");
        access.limitMemory(small - 1);
        change({2}, {});
        char expected[64];
        std::snprintf(expected, sizeof(expected), "heap mirror of 0x%llx+0x%llx: ", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(heaps[2])), static_cast<unsigned long long>(small));
        const auto attempts = access.allocationAttempts();
        bool refused = false;
        try {
            mirrored(2);
        } catch (const std::runtime_error& error) {
            const std::string what = error.what();
            refused = what.find(expected) != std::string::npos && what.find("Vulkan result -2") != std::string::npos;
        }
        Require(refused, "a heap mirror the memory cannot hold did not fail its build with the Vulkan result");
        Require(access.allocationAttempts() == attempts + 1, "a refused heap mirror was allocated again");
        Require(MirrorCounters().heapMirrors == swept.heapMirrors && MirrorCounters().heapBytes == swept.heapBytes, "a refused heap mirror was registered");
        access.limitMemory(std::nullopt);
        Require(mirrored(2) == std::byte{0x23} && MirrorCounters().heapMirrors == swept.heapMirrors + 1, "a heap mirror was not made once the memory was free again");
        change({}, {1, 2});
        sweep();
        Require(MirrorCounters().heapMirrors == before.heapMirrors && MirrorCounters().heapBytes == before.heapBytes, "the heap mirrors outlived their ranges");
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(raw, total);
#endif
        GuestArena::GuestArenaRelease_nid_postfix(raw, total);
    }
#ifdef _WIN32
    GuestArena::GuestArenaReset_nid_postfix(block, bytes);
#endif
    GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
}

}

void gpuMappingTests(const Context& context) {
    constexpr std::size_t bytes = 65536;
    auto* cpu = static_cast<std::byte*>(::operator new(bytes, std::align_val_t{bytes}));
    auto* gpu = static_cast<std::byte*>(::operator new(bytes, std::align_val_t{bytes}));
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(cpu, bytes, true, true, false);
        mutation.Add(gpu, bytes, true, true, true);
    }
    {
        GuestBufferMemory leased(context);
        leased.AcquireRegistered();
        leased.Upload(true);
        const auto ranges = leased.AddressRanges();
        const auto has = [&](const std::byte* block) { return std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == reinterpret_cast<std::uintptr_t>(block); }); };
        Require(has(gpu), "a GPU-mapped range is missing from the BDA table");
        Require(!has(cpu), "a range mapped without GPU access is in the BDA table");
        leased.WriteBack();
    }
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(cpu);
        mutation.Remove(gpu);
    }
    ::operator delete(cpu, std::align_val_t{bytes});
    ::operator delete(gpu, std::align_val_t{bytes});
}

void stridedOverhangTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t allocation = 0x8000;
    constexpr std::uint32_t stride = 20;
    constexpr std::uint32_t records = (allocation + stride - 1) / stride;
    auto* block = static_cast<std::byte*>(::operator new(allocation * 2, std::align_val_t{allocation}));
    std::memset(block, 0x5a, allocation * 2);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, allocation, true, false, true);
    }
    const auto address = reinterpret_cast<std::uintptr_t>(block);
    ShaderRecompiler::RecompileResult shader;
    auto buffer = binding(Role::GuestBuffers, 6);
    buffer.guestDescriptor = {static_cast<std::uint32_t>(address), (static_cast<std::uint32_t>(address >> 32) & 0xffffu) | (stride << 16u), records, 0x31000000u};
    buffer.bufferWritten = {false};
    shader.bindings = {buffer};
    CompiledShader compiled{ShaderRecompiler::ShaderStage::Compute, &shader, 0};
    {
        ShaderResources resources(context, compiled);
        Require(access.descriptor(6).range == allocation, "a strided buffer's rounded-up last record was bound past its allocation");
        resources.WriteBack();
    }
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block + allocation, allocation, true, false, true);
    }
    {
        ShaderResources resources(context, compiled);
        Require(access.descriptor(6).range == allocation, "a strided buffer's rounded-up last record was bound into the next allocation");
        resources.WriteBack();
    }
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block + allocation);
    }
    buffer.guestDescriptor[2] = records - 1;
    shader.bindings = {buffer};
    {
        ShaderResources resources(context, compiled);
        Require(access.descriptor(6).range == static_cast<VkDeviceSize>(records - 1) * stride, "a strided buffer inside its allocation was shortened");
        resources.WriteBack();
    }
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    }
    ::operator delete(block, std::align_val_t{allocation});
}

std::byte* reserveSparse(std::size_t reserved, std::size_t committed) {
#ifdef _WIN32
    auto* block = static_cast<std::byte*>(VirtualAlloc(nullptr, reserved, MEM_RESERVE, PAGE_NOACCESS));
    Require(block != nullptr && VirtualAlloc(block, committed, MEM_COMMIT, PAGE_READWRITE) != nullptr, "cannot reserve the sparse heap");
#else
    void* mapped = mmap(nullptr, reserved, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapped != MAP_FAILED && mprotect(mapped, committed, PROT_READ | PROT_WRITE) == 0, "cannot reserve the sparse heap");
    auto* block = static_cast<std::byte*>(mapped);
#endif
    return block;
}

void releaseSparse(std::byte* block, std::size_t reserved) {
#ifdef _WIN32
    static_cast<void>(reserved);
    VirtualFree(block, 0, MEM_RELEASE);
#else
    munmap(block, reserved);
#endif
}

void sparseHeapTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t reserved = 64u << 20u;
    constexpr std::size_t committed = 65536;
    auto* block = reserveSparse(reserved, committed);
    std::memset(block, 0x5a, committed);
    const auto base = reinterpret_cast<std::uintptr_t>(block);
    access.limitMemory(4u << 20u);
    {
        GuestBufferMemory memory(context);
        memory.AddWritable(base, reserved);
        memory.Upload(false);
        std::uint32_t adjustment = 0;
        const auto view = memory.Descriptor(base, reserved, adjustment);
        Require(adjustment == 0 && view.range == committed, "a sparse heap view is not cut at its last committed byte");
        Require(memory.CopiedBytes() == committed, "a cut sparse heap is charged at its reserved size");
        const auto bytes = access.bytes(view.buffer);
        Require(bytes.size() < reserved && bytes[view.offset + committed - 1] == std::byte{0x5a}, "a sparse heap was not copied up to its last committed byte");
        bytes[view.offset + 8] = std::byte{0x77};
        memory.WriteBack();
        Require(block[8] == std::byte{0x77}, "a store to a cut sparse heap was not written back");
    }
    {
        constexpr std::size_t late = 1u << 20u;
        GuestBufferMemory memory(context);
        memory.AddWritable(base, reserved);
        memory.AddReadable(base + late, 16);
        memory.Upload(true);
        std::uint32_t adjustment = 0;
        const auto view = memory.Descriptor(base + late, 16, adjustment);
        Require(view.range == 16 + adjustment, "a view in the uncommitted part of a sparse heap is not bound");
        const auto bytes = access.bytes(view.buffer);
        const auto whole = memory.Descriptor(base, reserved, adjustment);
        Require(whole.buffer == view.buffer && bytes.size() >= whole.offset + late + 16, "a view in the uncommitted part of a sparse heap has another owner");
        Require(bytes[whole.offset + committed - 1] == std::byte{0x5a} && std::all_of(bytes.begin() + static_cast<std::ptrdiff_t>(whole.offset + committed), bytes.begin() + static_cast<std::ptrdiff_t>(whole.offset + late + 16), [](std::byte byte) { return byte == std::byte{}; }), "the uncommitted part of a cut sparse heap does not read as zeros");
        Require(whole.range == late + 256, "a sparse heap copy does not end past its last view's start");
        Require(memory.CopiedBytes() == late + 256, "a cut sparse heap charge does not include its last view");
        const auto ranges = memory.AddressRanges();
        Require(ranges.size() == 1 && ranges[0].begin == base && ranges[0].end == base + late + 256, "the BDA range of a cut sparse heap is not clamped to its copy");
        memory.WriteBack();
    }
    access.limitMemory(std::nullopt);
    releaseSparse(block, reserved);
}

bool RunSparseBufferHoleTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t unit = 65536;
    constexpr std::size_t copied = 3 * unit;
    bool passed = true;
    for (const bool writable : {false, true}) {
        for (const std::string shape : {"interior", "shortened", "committed"}) {
            const auto name = std::string(writable ? "writable " : "readable ") + shape;
            try {
                const bool whole = shape == "committed";
                const auto reserved = shape == "shortened" ? 4 * unit : copied;
                struct Reservation {
                    std::size_t bytes;
                    std::byte* data;
                    ~Reservation() { releaseSparse(data, bytes); }
                } reservation{reserved, reserveSparse(reserved, whole ? reserved : unit)};
                if (!whole) {
#ifdef _WIN32
                    Require(VirtualAlloc(reservation.data + 2 * unit, unit, MEM_COMMIT, PAGE_READWRITE) != nullptr, "cannot commit the sparse heap tail");
#else
                    Require(mprotect(reservation.data + 2 * unit, unit, PROT_READ | PROT_WRITE) == 0, "cannot commit the sparse heap tail");
#endif
                }
                std::memset(reservation.data, 0x31, unit);
                if (whole) std::memset(reservation.data + unit, 0x3c, unit);
                std::memset(reservation.data + 2 * unit, 0x72, unit);
                VkBuffer seeded = VK_NULL_HANDLE;
                {
                    Buffer seed(context, copied, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                    std::fill(seed.Bytes().begin(), seed.Bytes().end(), std::byte{0xa5});
                    seeded = seed.Handle();
                }
                const auto base = reinterpret_cast<std::uintptr_t>(reservation.data);
                GuestBufferMemory memory(context);
                if (writable) memory.AddWritable(base, reserved);
                else memory.AddReadable(base, reserved);
                memory.Upload(false);
                std::uint32_t adjustment = 0;
                const auto view = memory.Descriptor(base, reserved, adjustment);
                Require(view.buffer == seeded, "the sparse upload did not reuse the poisoned buffer");
                Require(adjustment == 0 && view.offset == 0 && view.range == copied && memory.CopiedBytes() == copied, "the sparse upload has the wrong allocated span");
                auto bytes = access.bytes(view.buffer).first(copied);
                const auto equals = [](std::span<const std::byte> part, std::byte value) {
                    return std::all_of(part.begin(), part.end(), [value](std::byte byte) { return byte == value; });
                };
                Require(equals(bytes.first(unit), std::byte{0x31}) && equals(bytes.subspan(2 * unit, unit), std::byte{0x72}), "committed GPU bytes changed during upload");
                const bool holeZero = equals(bytes.subspan(unit, unit), whole ? std::byte{0x3c} : std::byte{});
                bytes[8] = std::byte{0x19};
                bytes[unit + 8] = std::byte{0xd4};
                bytes[2 * unit + 8] = std::byte{0x27};
                memory.WriteBack();
                Require(reservation.data[8] == (writable ? std::byte{0x19} : std::byte{0x31}) && reservation.data[2 * unit + 8] == (writable ? std::byte{0x27} : std::byte{0x72}), "sparse write-back did not preserve its writable ranges");
                if (whole) Require(reservation.data[unit + 8] == (writable ? std::byte{0xd4} : std::byte{0x3c}), "committed middle write-back changed permissions");
                Require(holeZero, "uncommitted GPU bytes retain prior buffer contents");
                std::cout << "sparse buffer holes: " << name << " passed\n";
            } catch (const std::exception& error) {
                std::cerr << "sparse buffer holes: " << name << ": " << error.what() << '\n';
                passed = false;
            }
        }
    }
    return passed;
}

void hostWriteWatchExclusionTests() {
#ifdef _WIN32
    std::vector<std::uint8_t> host(64, 0x31);
    const auto address = reinterpret_cast<std::uintptr_t>(host.data());
    Require(!AgcDriver::GuestMemory::Watched(address, host.size()), "ordinary host heap memory was reported as write-watched");
    Require(AgcDriver::GuestMemory::CollectWritesUncached(address, host.size()) == 0, "ordinary host heap memory returned a guest write generation");
    Require(!AgcDriver::GuestMemory::UnchangedSince(address, host.size(), 1), "ordinary host heap memory was proven unchanged");
    const std::array queries{AgcDriver::GuestMemory::UnchangedQuery{address, host.size(), 1}};
    Require(!AgcDriver::GuestMemory::UnchangedSinceAll(queries), "a host heap range passed a bulk unchanged proof");
    Require(std::all_of(host.begin(), host.end(), [](std::uint8_t value) { return value == 0x31; }), "write-watch eligibility changed host bytes");
#endif
}

void sparseHeapMirrorTests(const Context& context, const BdaTestAccess& access) {
#ifdef _WIN32
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "guest arena unavailable or not write-watched: sparse heap mirrors not tested\n";
        return;
    }
    constexpr std::size_t reserved = 64u << 20u;
    constexpr std::size_t committed = 2 * 65536;
    constexpr std::size_t extent = 1u << 20u;
    constexpr std::size_t late = 512u << 10u;
    constexpr std::size_t grown = 2u << 20u;
    constexpr std::size_t past = 3584u << 10u;
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(reserved, 65536);
    auto* guest = static_cast<std::uint8_t*>(block);
    GuestArena::GuestArenaCommit_nid_postfix(block, committed, PAGE_READWRITE, 65536);
    std::memset(block, 0x5a, committed);
    const auto base = reinterpret_cast<std::uintptr_t>(block);
    const auto registry = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, block, reserved, true, true, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, block);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    const auto bind = [&](bool written, const std::function<void(std::span<std::byte>, const VkDescriptorBufferInfo&)>& gpu) {
        GuestBufferMemory memory(context);
        if (written) memory.AddWritable(base, reserved);
        else memory.AddReadable(base, reserved);
        memory.Upload(false);
        std::uint32_t adjustment = 0;
        const auto view = memory.Descriptor(base, reserved, adjustment);
        Require(adjustment == 0, "a sparse heap view at its base is adjusted");
        gpu(access.bytes(view.buffer), view);
        memory.WriteBack();
        return view;
    };
    const auto address = [&] {
        GuestBufferMemory memory(context);
        memory.AcquireRegistered();
        memory.Upload(true);
        const auto ranges = memory.AddressRanges();
        const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == base; });
        const auto result = found != ranges.end() ? std::pair{found->end, found->deviceAddress} : std::pair<std::uint64_t, std::uint64_t>{0, 0};
        memory.WriteBack();
        return result;
    };
    const auto sweep = [&] {
        GuestBufferMemory memory(context);
        memory.AcquireRegistered();
        memory.Upload(true);
        memory.WriteBack();
    };
    sweep();
    registry(true);
    const auto before = MirrorCounters();
    const auto first = bind(true, [&](std::span<std::byte> bytes, const VkDescriptorBufferInfo& view) {
        Require(view.range == extent && bytes[view.offset + committed - 1] == std::byte{0x5a} && bytes[view.offset + committed] == std::byte{}, "a sparse heap is not bound to a mirror of its committed extent");
        bytes[view.offset + 8] = std::byte{0x77};
        guest[24] = 0x55;
    });
    const auto made = MirrorCounters();
    Require(made.heapMirrors == before.heapMirrors + 1 && made.heapBytes == before.heapBytes + extent && made.rebuilds == before.rebuilds + 1 && made.sparseBinds == before.sparseBinds + 1, "a sparse heap was not mirrored once");
    Require(guest[8] == 0x77 && guest[24] == 0x55, "a sparse heap mirror's write-back lost the GPU's store or rolled back the CPU's");
    const auto second = bind(false, [&](std::span<std::byte> bytes, const VkDescriptorBufferInfo& view) {
        Require(bytes[view.offset + 8] == std::byte{0x77} && bytes[view.offset + 24] == std::byte{0x55}, "the sparse heap mirror missed the stores");
    });
    const auto again = MirrorCounters();
    Require(second.buffer == first.buffer && again.rebuilds == made.rebuilds && again.blocksCopied == made.blocksCopied + 1, "a sparse heap bound again was read again beyond its written block");
    bind(false, [](std::span<std::byte>, const VkDescriptorBufferInfo&) {});
    Require(MirrorCounters().blocksCopied == again.blocksCopied && MirrorCounters().rebuilds == again.rebuilds, "an unchanged sparse heap was read again");
    GuestArena::GuestArenaCommit_nid_postfix(guest + late, 65536, PAGE_READWRITE, 65536);
    guest[late + 4] = 0x66;
    bind(false, [&](std::span<std::byte> bytes, const VkDescriptorBufferInfo& view) {
        Require(view.buffer == first.buffer && bytes[view.offset + late + 4] == std::byte{0x66} && bytes[view.offset + late + 65536] == std::byte{}, "a page committed after the mirror was made was not read into it");
    });
    const auto [end, device] = address();
    Require(end == base + extent && device != 0 && access.addressBytes(device)[late + 4] == std::byte{0x66} && MirrorCounters().rebuilds == again.rebuilds, "an address-based build does not map the sparse heap mirror up to its extent");
    GuestArena::GuestArenaCommit_nid_postfix(guest + grown, 65536, PAGE_READWRITE, 65536);
    guest[grown + 2] = 0x44;
    const auto [grownEnd, grownDevice] = address();
    Require(grownEnd == base + 3 * extent && grownDevice != device && access.addressBytes(grownDevice)[grown + 2] == std::byte{0x44} && access.addressBytes(grownDevice)[8] == std::byte{0x77}, "a sparse heap committed past its mirror was not mirrored again");
    {
        GuestBufferMemory memory(context);
        memory.AddReadable(base, reserved);
        memory.AddReadable(base + past, 16);
        memory.Upload(false);
        std::uint32_t adjustment = 0;
        const auto view = memory.Descriptor(base + past, 16, adjustment);
        const auto whole = memory.Descriptor(base, reserved, adjustment);
        Require(view.buffer == whole.buffer && access.bytes(whole.buffer)[whole.offset + grown + 2] == std::byte{0x44}, "a view past the sparse heap mirror is not copied with the heap");
        memory.WriteBack();
    }
    registry(false);
    sweep();
    Require(MirrorCounters().heapMirrors == before.heapMirrors && MirrorCounters().heapBytes == before.heapBytes, "the sparse heap mirror outlived its range");
    {
        constexpr std::size_t whole = 1u << 20u;
        GuestArena::GuestArenaReset_nid_postfix(block, reserved);
        GuestArena::GuestArenaCommit_nid_postfix(guest, whole, PAGE_READWRITE, 65536);
        std::memset(guest, 0x31, whole);
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, block, whole, true, true, true);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
        const auto start = MirrorCounters();
        const auto view = bind(true, [&](std::span<std::byte> bytes, const VkDescriptorBufferInfo& info) {
            Require(info.range == whole, "a heap bound whole over a smaller registered range is not clamped to that range");
            Require(bytes[info.offset + whole - 1] == std::byte{0x31}, "a heap bound whole over a smaller registered range misses its last committed byte");
            bytes[info.offset + 16] = std::byte{0x12};
        });
        Require(view.range == whole && MirrorCounters().sparseBinds == start.sparseBinds + 1 && guest[16] == 0x12, "a heap bound whole over a smaller registered range was copied");
        mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, block);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
        sweep();
    }
    GuestArena::GuestArenaReset_nid_postfix(block, reserved);
    GuestArena::GuestArenaRelease_nid_postfix(block, reserved);
#else
    static_cast<void>(context);
    static_cast<void>(access);
#endif
}

void addressMovedBufferTests(const Context& context) {
    alignas(64) static std::array<std::uint32_t, 16> first{};
    alignas(64) static std::array<std::uint32_t, 16> second{};
    const auto vsharp = [](const void* data) {
        const auto address = reinterpret_cast<std::uint64_t>(data);
        return std::vector<std::uint32_t>{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32) & 0xffffu, static_cast<std::uint32_t>(sizeof(first)), 0x31000000u};
    };
    ShaderRecompiler::RecompileResult vertex;
    auto buffer = binding(Role::GuestBuffers, 6);
    buffer.guestDescriptor = vsharp(first.data());
    buffer.bufferWritten = {false};
    auto data = binding(Role::FlattenedSrt, 7);
    data.guestDescriptor = {7u, 8u, 9u, 10u};
    vertex.bindings = {buffer, data};
    ShaderRecompiler::RecompileResult fragment;
    fragment.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    fragment.bindings = {binding(Role::BdaPagetable, 4), binding(Role::FaultBuffer, 5)};
    auto moved = vertex;
    moved.bindings[0].guestDescriptor = vsharp(second.data());
    auto rewritten = vertex;
    rewritten.bindings[1].guestDescriptor = {7u, 8u, 9u, 11u};
    const std::array<CompiledShader, 2> built{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
    const std::array<CompiledShader, 2> movedStages{{{ShaderRecompiler::ShaderStage::Vertex, &moved, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
    const std::array<CompiledShader, 2> rewrittenStages{{{ShaderRecompiler::ShaderStage::Vertex, &rewritten, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Recorder recorder(context);
    {
        ShaderResources resources(context, built, ColorTarget{}, 0, 0, 0, {});
        Require(resources.UsesAddressTable(), "a draw build with a BDA table stage does not use the BDA table");
        const auto unchanged = resources.MovedReadOnlyBuffers(built, recorder);
        Require(unchanged.has_value() && unchanged->empty(), "an address-based draw build found a buffer moved in the stages it was built from");
        Require(!resources.MovedReadOnlyBuffers(movedStages, recorder).has_value(), "an address-based draw build would serve a draw whose read-only buffer moved, which it cannot bind per draw");
        Require(!resources.MovedReadOnlyBuffers(rewrittenStages, recorder).has_value(), "an address-based draw build would serve a draw with other data words, which it cannot bind per draw");
        resources.WriteBack();
    }
    recorder.Sync();
}

void addressRejectedTableElementTests(const Context& context) {
    namespace Abi = ShaderRecompiler::ImageTableAbi;
    constexpr std::size_t surfaceBytes = 65536;
    constexpr std::uint32_t width = 64;
    constexpr std::uint32_t height = 4;
    constexpr std::uint32_t format32Float = 22;
    constexpr std::uint32_t type2D = 9;
#ifdef _WIN32
    auto* surface = static_cast<std::byte*>(VirtualAlloc(nullptr, surfaceBytes, MEM_RESERVE, PAGE_NOACCESS));
#else
    void* mapped = mmap(nullptr, surfaceBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    auto* surface = mapped == MAP_FAILED ? nullptr : static_cast<std::byte*>(mapped);
#endif
    Require(surface != nullptr, "cannot reserve an image table element's surface");
    const auto address = reinterpret_cast<std::uint64_t>(surface);
    auto tableContext = context;
    tableContext.hostImportAlignment = 65536;
    tableContext.imageTableCapacity = 2;
    tableContext.limits.maxDescriptorSetSampledImages = 2;
    tableContext.limits.maxPerStageDescriptorSampledImages = 2;
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult fragment;
    fragment.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    auto map = binding(Role::ImageTableMap, static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::ImageTableMap));
    map.guestDescriptor.assign(Abi::HeaderWords + 1u, 0u);
    map.guestDescriptor[Abi::HeaderVersion] = Abi::Version;
    map.guestDescriptor[Abi::HeaderElementCount] = 1u;
    map.guestDescriptor[Abi::HeaderElementFaults] = Abi::HeaderWords;
    map.guestDescriptor[Abi::HeaderSamplerFaults] = Abi::HeaderWords + 1u;
    auto images = binding(Role::ImageTable, ShaderRecompiler::RuntimeAbi::ImageTableBinding);
    images.kind = ShaderRecompiler::DescriptorKind::SampledImage;
    images.guestDescriptor = {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format32Float << 20u) | (((width - 1u) & 3u) << 30u), ((width - 1u) >> 2u) | ((height - 1u) << 14u) | (1u << 31u), 0xfacu | (type2D << 28u), 0u, 0u, 0u, 0u};
    images.imageShapes = {ShaderRecompiler::DescriptorImageShape::Image2D};
    images.imageDepthCompare = {false};
    images.imageSamplers = {0u};
    fragment.bindings = {binding(Role::BdaPagetable, 4), binding(Role::FaultBuffer, 5), map, images};
    const std::array<CompiledShader, 2> stages{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Recorder recorder(tableContext);
    {
        ShaderResources resources(tableContext, stages, ColorTarget{}, 0, 0, 0, {});
        Require(resources.UsesAddressTable(), "a draw build with a BDA table stage and an image table does not use the BDA table");
#ifdef _WIN32
        Require(VirtualAlloc(surface, surfaceBytes, MEM_COMMIT, PAGE_READWRITE) != nullptr, "cannot commit the image table element's surface");
#else
        Require(mprotect(surface, surfaceBytes, PROT_READ | PROT_WRITE) == 0, "cannot commit the image table element's surface");
#endif
        resources.EndUse();
        Require(!resources.Revalidate(stages, nullptr, {}), "an address-based draw build whose image table element was rejected as unmapped was reused after the element's surface was committed");
        resources.WriteBack();
    }
    recorder.Sync();
#ifdef _WIN32
    VirtualFree(surface, 0, MEM_RELEASE);
#else
    munmap(surface, surfaceBytes);
#endif
}

void addressFailedSubmissionTests(const Context& context, const BdaTestAccess& access) {
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult fragment;
    fragment.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    fragment.bindings = {binding(Role::BdaPagetable, 4), binding(Role::FaultBuffer, 5)};
    const std::array<CompiledShader, 2> stages{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}}};
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Recorder recorder(context);
    for (const bool reuse : {false, true}) {
        auto resources = std::make_shared<ShaderResources>(context, stages, ColorTarget{}, 0, 0, 0, std::span<const GuestMemorySnapshot>{});
        Require(resources->UsesAddressTable() && resources->Reusable(), "an address-based draw build is not reusable");
        if (reuse) {
            resources->EndUse();
            Require(resources->Revalidate(stages, nullptr, {}), "an unchanged address-based draw build was not proved reusable");
        }
        const auto holds = AddressReuseCounters().reusedHolds;
        recorder.OnComplete([resources, hold = resources->BeginUse()]() mutable {
            const auto held = std::move(hold);
            resources->CompleteUse();
        });
        resources->EndUse();
        Require(resources->InUse(), "an address-based draw build taken by a recorded batch is not in use");
        Require(AddressReuseCounters().reusedHolds == holds + (reuse ? 1u : 0u), reuse ? "a reused address-based draw did not count its hold of the space" : "a built address-based draw counted a reused hold of the space");
        access.submitResult(VK_ERROR_OUT_OF_HOST_MEMORY);
        bool failed = false;
        try {
            recorder.Submit();
        } catch (const std::exception&) {
            failed = true;
        }
        access.submitResult(VK_SUCCESS);
        Require(failed, "a batch whose submission failed did not report it");
        Require(!resources->InUse(), "an address-based draw build stayed in use after the batch that took it failed to submit");
        Require(AddressReuseCounters().reusedHolds == holds, "a reused address-based draw's hold of its space stayed counted after the batch that took it failed to submit");
        resources->WriteBack();
    }
    recorder.Sync();
}

void RunBdaResourceTests(const Context& context, const BdaTestAccess& access) {
    alignas(64) std::array<std::uint32_t, 16> guest{};
    guest[0] = 123;
    const auto address = reinterpret_cast<std::uintptr_t>(guest.data());
    GuestBufferMemory memory(context);
    memory.AddWritable(address, sizeof(guest));
    memory.AddWritable(address + 16, 16);
    Require(memory.CopiedBytes() == 0, "a region counted as copied before its upload");
    memory.Upload(true);
    Require(memory.CopiedBytes() == sizeof(guest), "copied bytes " + std::to_string(memory.CopiedBytes()) + " do not match the one region copied");
    std::uint32_t adjustment = 0;
    const auto first = memory.Descriptor(address, sizeof(guest), adjustment);
    Require(adjustment == 0, "a view at its owner's start is bound off it");
    const auto alias = memory.Descriptor(address + 16, 16, adjustment);
    Require(first.buffer == alias.buffer && alias.offset + adjustment == 16 && alias.range == 16 + adjustment, "aliased guest buffers have different owners");
    const auto ranges = memory.AddressRanges();
    Require(ranges.size() == 1 && ranges[0].begin == address && ranges[0].end == address + sizeof(guest), "incorrect BDA range bounds");
    Require(ranges[0].deviceAddress != 0 && ranges[0].permissions == (ShaderRecompiler::BdaAbi::Read | ShaderRecompiler::BdaAbi::Write), "incorrect BDA address or permissions");
    std::uint32_t changed = 321;
    std::memcpy(access.bytes(alias.buffer).data() + alias.offset, &changed, sizeof(changed));
    memory.WriteBack();
    Require(guest[4] == changed, "aliased GPU write was not published");
    reject([&] { memory.WriteBack(); }, "cannot be committed twice");
    reject([&] { memory.AddWritable(address, sizeof(guest)); }, "frozen");
    GuestBufferMemory overflow(context);
    const std::array<std::byte, 8> source{};
    reject([&] { overflow.AddSnapshot({std::numeric_limits<std::uint64_t>::max() - 3, source}); }, "overflow");

    const std::array<GuestMemorySnapshot, 1> snapshots{{{0x7fff12340000ULL, source}}};
    ShaderRecompiler::RecompileResult shader;
    shader.bindings = {binding(Role::BdaPagetable, 4), binding(Role::FaultBuffer, 5)};
    CompiledShader compiled{ShaderRecompiler::ShaderStage::Compute, &shader, 0};
    reject([&] { ShaderResources resources(context, compiled, snapshots); }, "ABI version");
    shader.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    auto disabled = context;
    disabled.bufferDeviceAddress = false;
    reject([&] { ShaderResources resources(disabled, compiled, snapshots); }, "not enabled");
    {
        // A rect-list fault buffer must not require BDA or consume guest snapshots.
        ShaderRecompiler::RecompileResult control;
        control.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
        control.bindings = {binding(Role::FaultBuffer, 5)};
        auto writable = binding(Role::GuestBuffers, 6);
        writable.guestDescriptor = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32) & 0xffffu, sizeof(guest), 0x31000000u};
        control.bindings.push_back(writable);
        const std::array<CompiledShader, 1> stages{{{ShaderRecompiler::ShaderStage::TessellationControl, &control, 0}}};
        const std::array<GuestMemorySnapshot, 1> unusedSnapshots{{{0, source}}};
        ShaderResources resources(disabled, stages, ColorTarget{}, 0, 0, 0, unusedSnapshots);
        Require(!ShaderResources::NeverReusable(stages), "a fault-buffer build is refused the resource cache");
        Require(!resources.Reusable(), "a fault-buffer build with copied writes is reusable");
        Require(resources.NeedsCompletion() && !resources.UsesAddressTable(), "a fault-buffer build reaches guest memory through the BDA table");
        const auto fault = access.bytes(access.descriptor(5).buffer);
        for (const auto byte : fault) Require(byte == std::byte{}, "rect-list fault buffer was not initialized");
        const ShaderRecompiler::BdaAbi::Fault report{ShaderRecompiler::BdaAbi::FaultState::Ready, ShaderRecompiler::BdaAbi::FaultReason::InvalidRectangle, 0, 0, 0, 0, 0};
        std::memcpy(fault.data(), &report, sizeof(report));
        reject([&] { resources.WriteBack(); }, "rect-list requires");
        std::memset(fault.data(), 0, fault.size());
        changed = 456;
        std::memcpy(access.bytes(access.descriptor(6).buffer).data() + sizeof(std::uint32_t), &changed, sizeof(changed));
        resources.WriteBack();
        Require(guest[1] == changed, "rect-list fault-only path lost guest buffer writes");
    }
    {
        ShaderRecompiler::RecompileResult control;
        control.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
        control.bindings = {binding(Role::FaultBuffer, 5)};
        const std::array<CompiledShader, 1> stages{{{ShaderRecompiler::ShaderStage::TessellationControl, &control, 0}}};
        ShaderResources resources(disabled, stages, ColorTarget{}, 0, 0, 0, {});
        Require(!ShaderResources::NeverReusable(stages) && resources.Reusable() && resources.ChecksFaults(), "a fault-only build is not reusable");
        resources.CheckFaults();
        const auto fault = access.bytes(access.descriptor(5).buffer);
        const ShaderRecompiler::BdaAbi::Fault report{ShaderRecompiler::BdaAbi::FaultState::Ready, ShaderRecompiler::BdaAbi::FaultReason::InvalidRectangle, 0, 0, 0, 0, 0};
        std::memcpy(fault.data(), &report, sizeof(report));
        reject([&] { resources.CheckFaults(); }, "rect-list requires");
    }
    {
        ShaderResources resources(context, compiled, snapshots);
        Require(!ShaderResources::NeverReusable(std::span<const CompiledShader>(&compiled, 1)), "address-based stages are refused the resource cache");
        Require(!resources.Reusable() && !resources.InUse(), "an address-based dispatch build is reusable");
        Require(resources.UsesAddressTable(), "an address-based build does not use the BDA table");
        ShaderRecompiler::RecompileResult plain;
        plain.bindings = {binding(Role::GuestBuffers, 6)};
        const CompiledShader plainStage{ShaderRecompiler::ShaderStage::Compute, &plain, 0};
        Require(!ShaderResources::NeverReusable(std::span<const CompiledShader>(&plainStage, 1)), "a build without address tables or a fault buffer is refused the resource cache");
        const auto table = access.bytes(access.descriptor(4).buffer);
        ShaderRecompiler::BdaAbi::Header header{};
        ShaderRecompiler::BdaAbi::Range range{};
        std::memcpy(&header, table.data(), sizeof(header));
        Require(header.version == ShaderRecompiler::BdaAbi::Version && header.count == 1 && header.entryBytes == sizeof(range), "BDA header layout mismatch");
        std::memcpy(&range, table.data() + sizeof(header), sizeof(range));
        Require(range.begin == snapshots[0].address && range.end == range.begin + source.size(), "64-bit guest address was truncated");
        const auto fault = access.bytes(access.descriptor(5).buffer);
        for (const auto byte : fault) Require(byte == std::byte{}, "fault buffer was not initialized");
        const ShaderRecompiler::BdaAbi::Fault denied{ShaderRecompiler::BdaAbi::FaultState::Ready, ShaderRecompiler::BdaAbi::FaultReason::Permission, snapshots[0].address + 4, 4, 0, 0x88, 0};
        std::memcpy(fault.data(), &denied, sizeof(denied));
        reject([&] { resources.WriteBack(); }, "read-only in the BDA table");
        std::memset(fault.data(), 0, fault.size());
        resources.WriteBack();
    }
    {
        auto writable = binding(Role::GuestBuffers, 6);
        writable.guestDescriptor = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32) & 0xffffu, sizeof(guest), 0x31000000u};
        shader.bindings.push_back(writable);
        ShaderResources resources(context, compiled);
        changed = 999;
        std::memcpy(access.bytes(access.descriptor(6).buffer).data(), &changed, sizeof(changed));
        const ShaderRecompiler::BdaAbi::Fault report{ShaderRecompiler::BdaAbi::FaultState::Ready, ShaderRecompiler::BdaAbi::FaultReason::Unmapped, 0x7fff99880000ULL, 4, 0, 0x44, 0};
        std::memcpy(access.bytes(access.descriptor(5).buffer).data(), &report, sizeof(report));
        reject([&] { resources.WriteBack(); }, "BDA access failed");
        auto invalidRectangle = report;
        invalidRectangle.reason = ShaderRecompiler::BdaAbi::FaultReason::InvalidRectangle;
        std::memcpy(access.bytes(access.descriptor(5).buffer).data(), &invalidRectangle, sizeof(invalidRectangle));
        reject([&] { resources.WriteBack(); }, "rect-list requires");
        Require(guest[0] == 123, "failed GPU command published writes");
    }
    {
        auto aligned = context;
        aligned.limits.minStorageBufferOffsetAlignment = 16;
        GuestBufferMemory unaligned(aligned);
        unaligned.AddWritable(address, sizeof(guest));
        unaligned.Upload(true);
        std::uint32_t adjustment = 0;
        const auto view = unaligned.Descriptor(address + 4, 4, adjustment);
        Require(adjustment == 4 && view.offset == 0 && view.range == 8, "a view off the offset alignment binds from below it");
        reject([&] { unaligned.Descriptor(address + sizeof(guest), 4, adjustment); }, "exceeds its GPU owner");
        const auto odd = unaligned.Descriptor(address + 2, 6, adjustment);
        Require(adjustment == 2 && odd.offset == 0 && odd.range == 8, "a view off a DWORD boundary does not bind the DWORDs around it");
        const auto late = unaligned.Descriptor(address + 0x13, 8, adjustment);
        Require(adjustment == 3 && late.offset == 16 && late.range == 12, "a view off a DWORD boundary does not bind from the offset alignment below it");
        GuestBufferMemory lone(aligned);
        lone.AddReadable(address + 6, 5);
        lone.Upload(true);
        const auto copied = lone.Descriptor(address + 6, 5, adjustment);
        Require(adjustment == 2 && copied.offset == 0 && copied.range == 8, "a lone view off a DWORD boundary is not copied from the DWORD below it");
        Require(std::memcmp(access.bytes(copied.buffer).data(), reinterpret_cast<const void*>(address + 4), 8) == 0, "a lone view off a DWORD boundary copied other bytes");
        GuestBufferMemory tail(aligned);
        tail.AddWritable(address, 62);
        tail.Upload(true);
        reject([&] { tail.Descriptor(address + 58, 4, adjustment); }, "exceeds its GPU owner");
    }
    {
        void* plain = GuestHeap::GuestHeapAllocate_nid_postfix(64);
        void* gpu = GuestHeap::GuestHeapAlignForGpu_nid_postfix(16, 64);
        const auto lookup = [](const void* pointer) {
            GuestAllocations::Mutation mutation;
            return mutation.Find(pointer);
        };
        Require(!lookup(plain).gpu && lookup(gpu).gpu, "a guest heap block's GPU access does not follow how it was allocated");
        gpu = GuestHeap::GuestHeapReallocate_nid_postfix(gpu, 128);
        plain = GuestHeap::GuestHeapRealign_nid_postfix(plain, 128, 64);
        Require(!lookup(plain).gpu && lookup(gpu).gpu, "a reallocated guest heap block changed its GPU access");
        {
            GuestBufferMemory leased(context);
            leased.AcquireRegistered();
            leased.Upload(true);
            const auto ranges = leased.AddressRanges();
            const auto listed = [&](const void* pointer) {
                const auto address = reinterpret_cast<std::uintptr_t>(pointer);
                return std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin <= address && address < range.end; });
            };
            Require(!listed(plain), "a guest heap block without GPU access is in an address-based build");
            Require(listed(gpu), "a guest heap block with GPU access is missing from an address-based build");
            leased.WriteBack();
        }
        GuestHeap::GuestHeapFree_nid_postfix(plain);
        GuestHeap::GuestHeapFree_nid_postfix(gpu);
    }
    {
        // The cached address space: a second build in an unchanged registry takes the first one's
        // space, and a guest free of a range pinned only by the cache goes through the pin waiter's
        // drop (no GPU work to wait for) and empties the registry of it.
        void* block = GuestHeap::GuestHeapAlignForGpu_nid_postfix(16, 64);
        const auto blockAddress = reinterpret_cast<std::uintptr_t>(block);
        std::memset(block, 0x5a, 64);
        const auto registered = [&] {
            const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
            return std::any_of(lease.begin(), lease.end(), [&](const auto& range) { return range->address == blockAddress; });
        };
        Require(registered(), "guest heap block is not registered");
        const auto before = AddressSpaceCounters();
        for (int build = 0; build < 2; ++build) {
            GuestBufferMemory leased(context);
            leased.AcquireRegistered();
            Require(leased.HoldsLease(), "address-based build holds no lease");
            leased.Upload(true);
            std::uint32_t adjustment = 0;
            const auto view = leased.Descriptor(blockAddress, 64, adjustment);
            Require(view.range == 64 + adjustment, "leased block has no descriptor");
            const auto ranges = leased.AddressRanges();
            Require(std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == blockAddress && range.end == blockAddress + 64; }), "leased block is missing from the BDA table");
            leased.WriteBack();
            Require(!leased.HoldsLease(), "write-back kept the lease");
        }
        const auto after = AddressSpaceCounters();
        if (after.enabled) Require(after.hits == before.hits + 1 && after.rebuiltFirst + after.rebuiltGeneration + after.rebuiltWaiterDrop + after.rebuiltEpoch + after.rebuiltDevice == before.rebuiltFirst + before.rebuiltGeneration + before.rebuiltWaiterDrop + before.rebuiltEpoch + before.rebuiltDevice + 1, "second build did not take the cached address space");
        GuestHeap::GuestHeapFree_nid_postfix(block);
        Require(!registered(), "freed guest heap block remains registered");
        const auto dropped = AddressSpaceCounters();
        if (dropped.enabled) Require(dropped.waiterDrops == after.waiterDrops + 1 && LeaseCounters().cacheDrops == dropped.waiterDrops, "the free did not drop the cached address space");
    }
    importCrossingTests(context, access);
    addressTableKeyTests();
    snapshotTableTests(context, access);
    spaceReuseTests(context);
    addressMovedBufferTests(context);
    addressRejectedTableElementTests(context);
    addressFailedSubmissionTests(context, access);
    importSpanTests(context);
    failedImportRemapTests(context, access);
    spaceImportRunTests(context);
    spaceWritableWritesTests(context);
    spaceReadListTests(context);
    importWindowTests(context);
    spaceReadAcrossImportsTests(context, access);
    spaceReadAcrossImportsWrittenTests(context);
    spaceReadsAcrossImportsMergedTests(context, access);
    spaceWriteMergedPastImportTests(context, access);
    spaceReadOverWritableImportTableTests(context);
    importJoinTests(context);
    importWholeRangeOverWindowTests(context);
    importRefusedWholeRangeTests(context);
    importNeighbouringEdgesTests(context);
    importAtMapOverBudgetTests(context);
    heapMirrorTests(context, access);
    importedHeapMirrorTests(context, access);
    importWatchTests();
    importedFreshTests(context);
    gpuMappingTests(context);
    stridedOverhangTests(context, access);
    sparseHeapTests(context, access);
    hostWriteWatchExclusionTests();
    sparseHeapMirrorTests(context, access);
    Require(AddressCopyOverflow({{0x1000, 0x3000, 0x2000, "uncommitted pages"}}, 0x2000).empty(), "copies within the limit were refused");
    const auto copies = AddressCopyOverflow({{0x1000, 0x2000, 0x1000, "not mirrored"}, {0x10000, 0x30000, 0x18000, "uncommitted pages"}}, 0x2000);
    Require(!copies.empty() && copies.find("0x10000+0x20000 (0.1 MiB committed, uncommitted pages)") < copies.find("0x1000+0x1000"), "the copy limit does not name the largest copy first");
    ClearSnapshotCopies(context.device);
}
