#include "BdaTests.hpp"
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
#include <mutex>
#include <new>
#include <string>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
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

void importRunGrowthTests(const Context& context) {
    constexpr std::size_t piece = 65536;
    void* block = ::operator new(2 * piece, std::align_val_t{piece});
    auto* guest = static_cast<std::uint8_t*>(block);
    const auto first = reinterpret_cast<std::uintptr_t>(block);
    std::memset(block, 0x3c, 2 * piece);
    auto importing = context;
    importing.hostImportAlignment = piece;
    const auto change = [&](void* range, std::size_t bytes, bool add) {
        GuestAllocations::Mutation mutation;
        if (add) mutation.Add(range, bytes, true, false, true);
        else mutation.Remove(range);
    };
    const auto build = [&](std::size_t pieces, const std::string& step) {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.AddReadable(first + 64, 16);
        leased.Upload(true);
        const auto ranges = leased.AddressRanges();
        leased.WriteBack();
        Require(std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin <= first && first + pieces * piece <= range.end; }), "the import of a run does not cover its " + std::to_string(pieces) + " registered pieces " + step);
    };
    change(guest, piece, true);
    build(1, "before its neighbour was registered");
    change(guest + piece, piece, true);
    build(2, "after its neighbour was registered");
    change(guest, 0, false);
    change(guest + piece, 0, false);
    ::operator delete(block, std::align_val_t{piece});
}

void importRunReplacedTests(const Context& context) {
    constexpr std::size_t piece = 65536;
    void* block = ::operator new(3 * piece, std::align_val_t{piece});
    auto* guest = static_cast<std::uint8_t*>(block);
    const auto first = reinterpret_cast<std::uintptr_t>(block);
    std::memset(block, 0x3c, 3 * piece);
    auto importing = context;
    importing.hostImportAlignment = piece;
    const auto add = [&](void* range, bool writable) {
        GuestAllocations::Mutation mutation;
        mutation.Add(range, piece, true, writable, true);
    };
    const auto remove = [&](void* range) {
        GuestAllocations::Mutation mutation;
        mutation.Remove(range);
    };
    const auto build = [&](std::size_t pieces, const std::string& step) {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.AddReadable(first + 64, 16);
        leased.Upload(true);
        const auto ranges = leased.AddressRanges();
        leased.WriteBack();
        Require(std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin <= first && first + pieces * piece <= range.end; }), "the import of a run does not cover its " + std::to_string(pieces) + " registered pieces " + step);
    };
    add(guest, true);
    add(guest + piece, false);
    add(guest + 2 * piece, true);
    build(1, "while the read-only middle piece ends it");
    for (int round = 0; round < 4; ++round) {
        remove(guest + piece);
        add(guest + piece, round % 2 == 0);
        build(round % 2 == 0 ? 3 : 1, "after the middle piece was replaced by one that " + std::string(round % 2 == 0 ? "continues" : "ends") + " the run, with as many ranges registered as before");
    }
    remove(guest);
    remove(guest + piece);
    remove(guest + 2 * piece);
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
        Require(owner.use_count() == 2, "a failed import pins its owner beyond the registry and its kept lease snapshot");
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
    const auto [prefixBuffer, prefixCopied] = bind(heapBase, 2 * piece + 16);
    const auto* prefix = HostImportFor(importing, heapBase, 2 * piece + 16);
    Require(prefix != nullptr && prefix->base == heapBase && prefix->bytes == 3 * piece && prefixBuffer == prefix->buffer && prefixCopied == 0, "a buffer bound from the first of three pieces into the last was not imported over the pieces it crosses");
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

class ImportBudgetRanges {
public:
    static constexpr std::size_t piece = 65536;
    static constexpr std::size_t bytes = 3 * piece;

    explicit ImportBudgetRanges(const Context& context) : importing(context) {
        block = static_cast<std::byte*>(::operator new(4 * bytes, std::align_val_t{piece}));
        importing.hostImportAlignment = piece;
        ClearHostImports(importing.device);
        SetHostImportWindow(2 * piece);
        SetHostImportRangeWindow(0);
        SetHostImportBudget(2 * bytes);
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 0; index < 4; ++index) {
            std::memset(block + index * bytes, static_cast<int>(0x31 + index), bytes);
            mutation.Add(block + index * bytes, bytes, true, false, true);
        }
    }

    ~ImportBudgetRanges() {
        ClearHostImports(importing.device);
        {
            GuestAllocations::Mutation mutation;
            for (std::size_t index = 0; index < 4; ++index) mutation.Remove(block + index * bytes);
        }
        SetHostImportBudget(0);
        SetHostImportRangeWindow(std::uint64_t{64} << 20u);
        SetHostImportWindow(0);
        ::operator delete(block, std::align_val_t{piece});
    }

    ImportBudgetRanges(const ImportBudgetRanges&) = delete;
    ImportBudgetRanges& operator=(const ImportBudgetRanges&) = delete;

    std::uintptr_t Address(std::size_t index) const { return reinterpret_cast<std::uintptr_t>(block + index * bytes); }

    void CheckUnchanged() const {
        for (std::size_t index = 0; index < 4; ++index) {
            const auto expected = static_cast<std::byte>(0x31 + index);
            Require(std::all_of(block + index * bytes, block + (index + 1) * bytes, [expected](std::byte value) { return value == expected; }), "import budget probes changed guest bytes");
        }
    }

    Context importing;

private:
    std::byte* block = nullptr;
};

void importCoverageHintBudgetTests(const Context& context, const BdaTestAccess& access) {
    for (const bool acquired : {true, false}) {
        ImportBudgetRanges ranges(context);
        const auto a = ranges.Address(0);
        const auto c = ranges.Address(2);
        const auto d = ranges.Address(3);
        const auto* first = HostImportFor(ranges.importing, a, 16);
        Require(first != nullptr && first->base == a && first->bytes == ranges.bytes, "coverage budget fixture did not import its first range");
        const auto firstBuffer = first->buffer;
        const auto firstSerial = HostImportSerial(ranges.importing, a, 16, false);
        NoteHostImportFrame();
        NoteHostImportFrame();
        for (int use = 0; use < 300; ++use) Require(HostImportFor(ranges.importing, d, 16) != nullptr, "coverage budget fixture did not import its hot range");
        const auto hotSerial = HostImportSerial(ranges.importing, d, 16, false);
        Require(firstSerial != 0 && hotSerial != 0 && HostImportLiveBytes() == 2 * ranges.bytes, "coverage budget fixture has unexpected import ownership or usage");
        {
            GuestBufferMemory memory(ranges.importing);
            memory.AddReadable(c, 32);
            memory.UploadPrepare(false);
            if (acquired) {
                const auto* held = HostImportFor(ranges.importing, a, 16);
                Require(held != nullptr && held->buffer == firstBuffer, "a genuine import acquisition replaced the retained range");
            } else {
                Require(HostImportCovers(ranges.importing, a, 16), "a coverage hint missed the existing range");
            }
            memory.UploadFinish(false);
            std::uint32_t adjustment = 0;
            const auto view = memory.Descriptor(c, 32, adjustment);
            Require(view.range == 32 + adjustment, "a disjoint descriptor has an incorrect extent");
            Require(HostImportSerial(ranges.importing, d, 16, false) == hotSerial && HostImportLiveBytes() == 2 * ranges.bytes, "a coverage probe displaced the hot import or changed the budget");
            if (acquired) {
                Require(view.buffer != firstBuffer, "a disjoint descriptor uses the retained range's import");
                Require(HostImportSerial(ranges.importing, a, 16, false) == firstSerial && HostImportSerial(ranges.importing, c, 16, false) == 0, "a genuine import acquisition did not protect its buffer from eviction");
                Require(memory.CopiedBytes() == 32, "a disjoint descriptor at the budget was not copied while an import was held");
                const auto copied = access.bytes(view.buffer);
                const auto offset = static_cast<std::size_t>(view.offset + adjustment);
                Require(offset + 32 <= copied.size() && std::memcmp(copied.data() + offset, reinterpret_cast<const void*>(c), 32) == 0, "the descriptor copied behind a held import lost its guest bytes");
            } else {
                const auto importedSerial = HostImportSerial(ranges.importing, c, 16, false);
                Require(HostImportSerial(ranges.importing, a, 16, false) == 0 && importedSerial != 0 && importedSerial != firstSerial, "a coverage hint prevented eviction of an unused import");
                const auto* imported = ExistingHostImportFor(ranges.importing, c, 32);
                Require(imported != nullptr && imported->base == c && imported->bytes == ranges.bytes && view.buffer == imported->buffer && view.offset + adjustment == c - imported->base, "a descriptor does not bind the import obtained after a coverage hint");
                Require(memory.CopiedBytes() == 0, "a coverage hint forced the disjoint imported descriptor to copy");
            }
            memory.WriteBack();
        }
        ranges.CheckUnchanged();
    }
}

void failedImportJoinProbeBudgetTests(const Context& context, const BdaTestAccess& access) {
    ImportBudgetRanges ranges(context);
    const auto a = ranges.Address(0);
    const auto crossing = ranges.Address(1) - 16;
    const auto c = ranges.Address(2);
    const auto d = ranges.Address(3);
    const auto* first = HostImportFor(ranges.importing, a, 16);
    Require(first != nullptr && first->base == a && first->bytes == ranges.bytes, "failed join budget fixture did not import its first range");
    const auto firstSerial = HostImportSerial(ranges.importing, a, 16, false);
    NoteHostImportFrame();
    NoteHostImportFrame();
    for (int use = 0; use < 300; ++use) Require(HostImportFor(ranges.importing, d, 16) != nullptr, "failed join budget fixture did not import its hot range");
    const auto hotSerial = HostImportSerial(ranges.importing, d, 16, false);
    Require(firstSerial != 0 && hotSerial != 0 && HostImportLiveBytes() == 2 * ranges.bytes, "failed join budget fixture has unexpected import ownership or usage");
    {
        GuestBufferMemory memory(ranges.importing);
        memory.AddReadable(crossing, 32);
        memory.AddReadable(c, 32);
        memory.Upload(false);
        std::uint32_t adjustment = 0;
        const auto copiedView = memory.Descriptor(crossing, 32, adjustment);
        const auto copied = access.bytes(copiedView.buffer);
        const auto offset = static_cast<std::size_t>(copiedView.offset + adjustment);
        Require(copiedView.range == 32 + adjustment && offset + 32 <= copied.size() && std::memcmp(copied.data() + offset, reinterpret_cast<const void*>(crossing), 32) == 0, "the refused joined descriptor lost either side's guest bytes");
        Require(HostImportSerial(ranges.importing, crossing, 32, false) == 0, "a join over the available budget unexpectedly produced an import");
        const auto importedSerial = HostImportSerial(ranges.importing, c, 16, false);
        Require(HostImportSerial(ranges.importing, a, 16, false) == 0 && importedSerial != 0 && importedSerial != firstSerial, "a failed join endpoint probe prevented eviction of an unused import");
        const auto* imported = ExistingHostImportFor(ranges.importing, c, 32);
        const auto view = memory.Descriptor(c, 32, adjustment);
        Require(imported != nullptr && imported->base == c && imported->bytes == ranges.bytes && view.buffer == imported->buffer && view.offset + adjustment == c - imported->base && view.range == 32 + adjustment, "the descriptor after a failed join does not bind its complete import");
        Require(memory.CopiedBytes() == 32 && HostImportLiveBytes() == 2 * ranges.bytes && HostImportSerial(ranges.importing, d, 16, false) == hotSerial, "a failed join forced a second copy or displaced the hot import");
        memory.WriteBack();
    }
    ranges.CheckUnchanged();
}

void failedJoinWrittenViewTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t piece = 65536;
    constexpr std::size_t large = 3 * piece;
    constexpr std::uint32_t stored = 0x600dcafe;
    auto* block = static_cast<std::uint8_t*>(::operator new(2 * large, std::align_val_t{2 * piece}));
    std::memset(block, 0x5a, 2 * large);
    const auto first = reinterpret_cast<std::uintptr_t>(block);
    const auto second = first + large;
    const auto written = second + 128;
    auto importing = context;
    importing.hostImportAlignment = piece;
    ClearHostImports(importing.device);
    SetHostImportWindow(2 * piece);
    SetHostImportBudget(2 * large);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, large, true, false, true);
        mutation.Add(block + large, large, true, false, true);
    }
    Require(HostImportFor(importing, first, 16) != nullptr && HostImportFor(importing, second, 16) != nullptr && HostImportLiveBytes() == 2 * large, "the failed join fixture did not import its two ranges apart");
    access.limitImports(large);
    {
        GuestBufferMemory memory(importing);
        memory.AcquireRegistered();
        memory.AddReadable(second - 16, 32);
        memory.AddWritable(written, 32);
        memory.Upload(true);
        Require(ExistingHostImportFor(importing, second - 16, 32) == nullptr && ExistingHostImportFor(importing, written, 32) == nullptr, "the join over the import limit did not fall back to copies");
        std::uint32_t adjustment = 0;
        const auto view = memory.Descriptor(written, 32, adjustment);
        std::memcpy(access.bytes(view.buffer).data() + view.offset + adjustment, &stored, sizeof(stored));
        memory.WriteBack();
    }
    std::uint32_t kept = 0;
    std::memcpy(&kept, block + large + 128, sizeof(kept));
    Require(kept == stored, "a write through a view whose import a failed join retired was lost");
    access.limitImports(std::nullopt);
    ClearHostImports(importing.device);
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
        mutation.Remove(block + large);
    }
    SetHostImportBudget(0);
    SetHostImportWindow(0);
    ::operator delete(block, std::align_val_t{2 * piece});
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

struct ObservedHostImport {
    const void* pointer;
    VkDeviceSize bytes;
};

class ImportAllocationProbe {
public:
    explicit ImportAllocationProbe(Context& context) : context(context), original(context.deviceProc) {
        Require(active == nullptr && original != nullptr, "invalid import allocation probe scope");
        active = this;
        context.deviceProc = Resolve;
    }
    ~ImportAllocationProbe() {
        context.deviceProc = original;
        active = nullptr;
    }
    ImportAllocationProbe(const ImportAllocationProbe&) = delete;
    ImportAllocationProbe& operator=(const ImportAllocationProbe&) = delete;

    std::vector<ObservedHostImport> calls;
    VkResult failure = VK_SUCCESS;
    std::function<void()> onImport;

private:
    static PFN_vkVoidFunction VKAPI_CALL Resolve(VkDevice device, const char* name) {
        Require(active != nullptr, "import allocation probe escaped its scope");
        if (std::strcmp(name, "vkAllocateMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Allocate);
        return active->original(device, name);
    }
    static VkResult VKAPI_CALL Allocate(VkDevice device, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks* allocator, VkDeviceMemory* memory) {
        Require(active != nullptr, "import allocation callback escaped its scope");
        for (const auto* next = static_cast<const VkBaseInStructure*>(info->pNext); next != nullptr; next = next->pNext) {
            if (next->sType != VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT) continue;
            const auto* imported = reinterpret_cast<const VkImportMemoryHostPointerInfoEXT*>(next);
            active->calls.push_back({imported->pHostPointer, info->allocationSize});
            if (active->onImport) {
                const auto callback = std::exchange(active->onImport, nullptr);
                callback();
            }
            if (active->failure != VK_SUCCESS) return active->failure;
        }
        const auto allocate = reinterpret_cast<PFN_vkAllocateMemory>(active->original(device, "vkAllocateMemory"));
        Require(allocate != nullptr, "import allocation probe has no original allocator");
        return allocate(device, info, allocator, memory);
    }

    Context& context;
    PFN_vkGetDeviceProcAddr original;
    inline static thread_local ImportAllocationProbe* active = nullptr;
};

class UnrelatedImportProbeRange {
public:
    UnrelatedImportProbeRange() {
        GuestAllocations::Mutation().Add(storage.data(), storage.size(), true, false, false);
    }
    ~UnrelatedImportProbeRange() {
        GuestAllocations::Mutation().Remove(storage.data());
    }
    UnrelatedImportProbeRange(const UnrelatedImportProbeRange&) = delete;
    UnrelatedImportProbeRange& operator=(const UnrelatedImportProbeRange&) = delete;

private:
    std::array<std::byte, 64> storage{};
};

void importMappedFailureGenerationTests(const Context& context) {
    ClearHostImports(context.device);
    WindowedRange range(context, 2 * WindowedRange::piece, 1);
    ImportAllocationProbe probe(range.importing);
    probe.failure = VK_ERROR_INVALID_EXTERNAL_HANDLE;
    GuestAllocations::Mapped mapped;
    const auto collect = [&] {
        mapped.clear();
        const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        for (const auto& owner : lease) if (owner->address == range.First()) mapped.push_back(owner);
        Require(mapped.size() == 1u, "the map failure fixture lost its registered range");
    };
    collect();
    std::unique_ptr<UnrelatedImportProbeRange> unrelated;
    probe.onImport = [&] { unrelated = std::make_unique<UnrelatedImportProbeRange>(); };
    const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Require(ImportMappedRanges(range.importing, mapped, generation, true), "the first failed map import was deferred");
        Require(probe.calls.size() == 1u && unrelated != nullptr, "the map failure fixture did not mutate an unrelated range during allocation");
        Require(GuestAllocations::GuestAllocationsGeneration_nid_postfix() != generation, "the concurrent unrelated mapping did not advance the registry");
        Require(ImportMappedRanges(range.importing, mapped, GuestAllocations::GuestAllocationsGeneration_nid_postfix(), true), "the repeated failed map import was deferred");
        Require(probe.calls.size() == 1u, "an unrelated mapping during a failed import discarded its owner failure memo");
    }
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(reinterpret_cast<void*>(range.First()));
        mutation.Add(reinterpret_cast<void*>(range.First()), range.bytes, true, false, true);
    }
    collect();
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Require(ImportMappedRanges(range.importing, mapped, GuestAllocations::GuestAllocationsGeneration_nid_postfix(), true), "the replacement owner map import was deferred");
        Require(probe.calls.size() == 1u, "a replacement owner was imported again at map instead of on first use");
    }
    HostImportFor(range.importing, range.First(), range.bytes);
    Require(probe.calls.size() == 2u, "a replacement owner did not retry its predecessor's failed import on first use");
    ClearHostImports(context.device);
}

void importStaleProbeTests(const Context& context, const BdaTestAccess& access) {
    for (const auto scenario : {0u, 1u, 2u}) {
        const bool held = scenario == 1u;
        const bool exactWhole = scenario == 2u;
        WindowedRange range(context, 2 * WindowedRange::piece, 2);
        ImportAllocationProbe probe(range.importing);
        const auto first = range.First();
        auto* guest = reinterpret_cast<std::byte*>(first);
        for (std::size_t offset = 17u; offset < range.bytes; offset += 4096u) guest[offset] = static_cast<std::byte>((offset / 4096u) & 0xffu);
        const std::vector<std::byte> originalGuest(guest, guest + range.bytes);
        const auto* window = HostImportFor(range.importing, first, exactWhole ? range.bytes : 32u);
        Require(window != nullptr && !window->joined && window->base == first && window->bytes == (exactWhole ? range.bytes : range.window), "the stale-probe fixture did not create its ordinary import extent");
        const auto originalBuffer = window->buffer;
        const auto originalUse = window->lastUse;
        const auto originalSerial = HostImportSerial(range.importing, first, 32u, false);
        auto mockContents = access.bytes(window->buffer);
        Require(mockContents.size() == window->bytes, "the mock import buffer has an incorrect extent");
        std::copy_n(originalGuest.begin(), window->bytes, mockContents.begin());
        std::uint32_t scratch = 0x12345678u;
        {
            GuestBufferMemory copied(range.importing);
            copied.AddReadable(reinterpret_cast<std::uintptr_t>(&scratch), sizeof(scratch));
            copied.Upload(false);
            copied.WriteBack();
        }
        Require(window->lastUse == originalUse && HostImportSerial(range.importing, first, 32u, false) == originalSerial, "an unrelated copied build touched or replaced the prefix window");
        GuestBufferMemory reader(range.importing);
        VkDescriptorBufferInfo heldView{};
        std::uint32_t heldAdjustment = 0u;
        if (held) {
            reader.AddReadable(first + 16u, 32u);
            reader.Upload(false);
            heldView = reader.Descriptor(first + 16u, 32u, heldAdjustment);
            Require(heldView.buffer == originalBuffer, "the held control did not bind the prefix window");
        }
        GuestAllocations::Mapped mapped;
        {
            const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
            for (const auto& owner : lease) if (owner->address == first && owner->bytes == range.bytes) mapped.push_back(owner);
        }
        Require(mapped.size() == 1u, "the stale-probe fixture lost its registered range");
        const auto originalOwner = mapped.front().lock();
        const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        UnrelatedImportProbeRange unrelated;
        Require(GuestAllocations::GuestAllocationsGeneration_nid_postfix() != generation && HostImportsIdentity(range.importing).device == VK_NULL_HANDLE,
            "the unrelated registry change did not make imports stale");
        const auto attempts = access.allocationAttempts();
        probe.calls.clear();
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            Require(ImportMappedRanges(range.importing, mapped, GuestAllocations::GuestAllocationsGeneration_nid_postfix(), false), "the current device did not admit the map-time import pass");
        }
        Require(mapped.front().lock() == originalOwner, "an unrelated registry change replaced the import's memory owner");
        if (held) {
            Require(access.allocationAttempts() == attempts && probe.calls.empty(), "a stale probe retired an import held by an actual descriptor");
            const auto* kept = HostImportFor(range.importing, first + 16u, 32u);
            Require(kept != nullptr && kept->buffer == originalBuffer && kept->bytes == range.window && HostImportSerial(range.importing, first, 32u, false) == originalSerial,
                "the actual held import lost its buffer, extent or serial");
            std::uint32_t adjustment = 0u;
            const auto view = reader.Descriptor(first + 16u, 32u, adjustment);
            Require(view.buffer == heldView.buffer && view.offset == heldView.offset && view.range == heldView.range && adjustment == heldAdjustment,
                "a map-time stale probe invalidated a descriptor that still uses its import");
            const auto contents = access.bytes(view.buffer);
            Require(view.offset + adjustment + 32u <= contents.size() && std::memcmp(contents.data() + view.offset + adjustment, reinterpret_cast<const void*>(first + 16u), 32u) == 0,
                "the protected descriptor no longer reads the registered bytes");
            reader.WriteBack();
        } else if (exactWhole) {
            Require(access.allocationAttempts() == attempts && probe.calls.empty() && window->lastUse > originalUse,
                "accepting the stale exact whole import reallocated it or did not protect its returned pointer");
            const auto* kept = HostImportFor(range.importing, first, range.bytes);
            Require(kept != nullptr && kept->buffer == originalBuffer && kept->ranges.size() == 1u && kept->ranges.front() == originalOwner && HostImportSerial(range.importing, first, range.bytes, false) == originalSerial,
                "accepting the exact whole import changed its native or registered identity");
        } else {
            Require(access.allocationAttempts() == attempts + 1u, "a discarded stale probe made an unheld prefix window block the whole import");
            const auto* whole = HostImportFor(range.importing, first, range.bytes);
            Require(whole != nullptr && whole->base == first && whole->bytes == range.bytes && whole->ranges.size() == 1u && whole->ranges.front() == originalOwner,
                "the stale-probe repair did not retain the exact registered owner over the whole import");
            Require(HostImportSerial(range.importing, first, range.bytes, false) != originalSerial, "replacing the unheld window reused its old import identity");
            Require(probe.calls.size() == 1u && probe.calls.front().pointer == reinterpret_cast<const void*>(first) && probe.calls.front().bytes == range.bytes,
                "the replacement native import did not reference the exact whole registered memory");
            Require(access.bytes(whole->buffer).size() == range.bytes, "the replacement import buffer does not cover its registered extent");
        }
        Require(scratch == 0x12345678u && std::equal(originalGuest.begin(), originalGuest.end(), guest), "the import probe changed guest bytes or unrelated copied data");
    }
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

void importPiecesOverAllocationLimitTests(const Context& context) {
    WindowedRange range(context, 2 * WindowedRange::piece, 5);
    const auto first = range.First();
    const auto limit = 3 * range.window;
    SetHostImportMaxBytes(limit);
    {
        GuestBufferMemory leased(range.importing);
        leased.AcquireRegistered();
        leased.Upload(true);
        std::uint64_t covered = first;
        for (const auto& entry : leased.AddressRanges()) {
            if (entry.end <= first || entry.begin >= first + range.bytes) continue;
            Require(entry.begin == covered, "the pieces of a range over the allocation limit leave a gap or overlap in the BDA table");
            Require(entry.end - entry.begin <= limit, "a BDA table entry of a range over the allocation limit is larger than the limit");
            const auto* piece = ExistingHostImportFor(range.importing, entry.begin, static_cast<std::size_t>(entry.end - entry.begin));
            Require(piece != nullptr && piece->bytes <= limit && entry.deviceAddress == piece->address + (entry.begin - piece->base), "a BDA table entry of a range over the allocation limit is not served in place by an import within the limit");
            covered = entry.end;
        }
        Require(covered == first + range.bytes, "the pieces of a range over the allocation limit do not cover it");
        for (std::uint64_t at = first; at < first + range.bytes; at += range.window) {
            const auto* piece = ExistingHostImportFor(range.importing, at, 16);
            Require(piece != nullptr && piece->bytes <= limit, "a range over the allocation limit has an import larger than the limit");
        }
        leased.WriteBack();
    }
    SetHostImportMaxBytes(0);
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
    ClearHostImports(context.device);
    constexpr std::size_t bytes = 2 * 65536;
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
    NoteHostImportFrame();
    NoteHostImportFrame();
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

// Three ranges under a budget for two, each used by a build of its own in every frame, with more lookups between the
// uses than a frame's worth of imports. The two imported first keep their imports and the third is refused at every
// frame, instead of evicting one of the others (which then evicts the next, every frame). A range unused for two frames
// is evicted for the third.
void importEvictionFramesTests(const Context& context) {
    constexpr std::size_t bytes = 65536;
    auto importing = context;
    importing.hostImportAlignment = bytes;
    std::array<void*, 3> blocks{};
    std::array<std::uintptr_t, 3> at{};
    const auto change = [&](void* block, bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, block, bytes, true, false, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, block);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        blocks[index] = ::operator new(bytes, std::align_val_t{bytes});
        at[index] = reinterpret_cast<std::uintptr_t>(blocks[index]);
        change(blocks[index], true);
    }
    SetHostImportBudget(HostImportLiveBytes() + 2 * bytes);
    const auto use = [&](std::size_t index) {
        GuestBufferMemory leased(importing);
        leased.AddReadable(at[index], 32);
        leased.Upload(false);
        leased.WriteBack();
        for (int lookup = 0; lookup < 300; ++lookup) static_cast<void>(HostImportFor(importing, at[index], 16));
        return HostImportSerial(importing, at[index], 16, false);
    };
    NoteHostImportFrame();
    NoteHostImportFrame();
    std::array<std::uint64_t, 2> serials{use(0), use(1)};
    Require(serials[0] != 0 && serials[1] != 0, "two ranges under a budget for two were not imported");
    const auto epoch = HostImportEpoch();
    NoteHostImportFrame();
    Require(HostImportEpoch() == epoch, "a frame end with no import change moved the imports epoch");
    for (int frame = 0; frame < 10; ++frame) {
        Require(use(2) == 0, "a range over the budget evicted an import used in the same frame (frame " + std::to_string(frame) + ")");
        NoteHostImportFrame();
        Require(use(0) == serials[0] && use(1) == serials[1], "an import used in every frame was imported again (frame " + std::to_string(frame) + ")");
    }
    NoteHostImportFrame();
    Require(use(1) == serials[1] && use(2) == 0, "a range over the budget evicted an import used in the previous frame");
    NoteHostImportFrame();
    Require(use(1) == serials[1] && use(2) != 0 && !HostImportCovers(importing, at[0], 16), "an import unused for two frames was not evicted for a range over the budget");
    SetHostImportBudget(0);
    for (auto* block : blocks) change(block, false);
    for (auto* block : blocks) ::operator delete(block, std::align_val_t{bytes});
}

void importRequesterTests(const Context& context, const BdaTestAccess& access) {
    ClearHostImports(context.device);
    constexpr std::size_t piece = 65536;
    constexpr std::size_t pieces = 11;
    auto* block = static_cast<std::uint8_t*>(::operator new(pieces * piece, std::align_val_t{piece}));
    std::memset(block, 0x2d, pieces * piece);
    auto importing = context;
    importing.hostImportAlignment = piece;
    SetHostImportWindow(0);
    const auto address = [&](std::size_t index) { return reinterpret_cast<std::uintptr_t>(block + index * piece); };
    const auto change = [&](std::size_t index, bool add) {
        GuestAllocations::Mutation mutation;
        if (add) mutation.Add(block + index * piece, piece, true, index % 2 == 0, true);
        else mutation.Remove(block + index * piece);
    };
    const auto counted = [](ImportRequester requester) { return HostImportCounters().requesters[static_cast<std::size_t>(requester)]; };
    const auto made = [&](ImportRequester requester) { return counted(requester).made; };
    Require(std::string(ImportRequesterName(ImportRequester::Map)) == "map" && std::string(ImportRequesterName(ImportRequester::Build)) == "build" && std::string(ImportRequesterName(ImportRequester::Access)) == "access" && std::string(ImportRequesterName(ImportRequester::Join)) == "join" && std::string(ImportRequesterName(ImportRequester::Segment)) == "segment" && std::string(ImportRequesterName(ImportRequester::ColorTarget)) == "color target", "an import requester has the wrong name");
    const auto mappedRange = [&](std::size_t index) {
        GuestAllocations::Mapped ranges;
        const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        for (const auto& range : lease) {
            if (range->address == address(index)) ranges.push_back(range);
        }
        Require(ranges.size() == 1, "the requester test's mapped range is not registered");
        return ranges;
    };
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block + 6 * piece, 2 * piece, true, true, true);
    }
    {
        const auto before = made(ImportRequester::Map);
        static_cast<void>(ImportMappedRanges(importing, mappedRange(6), GuestAllocations::GuestAllocationsGeneration_nid_postfix(), true));
        Require(made(ImportRequester::Map) == before + 1 && HostImportCovers(importing, address(6), 2 * piece), "a range imported at map time was not counted for the map requester");
    }
    change(0, true);
    {
        const auto map = made(ImportRequester::Map);
        const auto access = made(ImportRequester::Access);
        static_cast<void>(ImportMappedRanges(importing, mappedRange(0), GuestAllocations::GuestAllocationsGeneration_nid_postfix(), true));
        Require(made(ImportRequester::Map) == map && !HostImportCovers(importing, address(0), 16), "a range of 64 KiB was imported at map time");
        Require(HostImportFor(importing, address(0), 16) != nullptr && HostImportCovers(importing, address(0), piece) && made(ImportRequester::Access) == access + 1 && made(ImportRequester::Map) == map,
            "a range of 64 KiB left to its first GPU use was not imported there, or not counted for that use's requester");
    }
    change(1, true);
    {
        const auto before = made(ImportRequester::Access);
        Require(HostImportFor(importing, address(1), 16) != nullptr && made(ImportRequester::Access) == before + 1, "a lookup's import was not counted for the access requester");
    }
    change(2, true);
    {
        const auto before = made(ImportRequester::Segment);
        const auto access = made(ImportRequester::Access);
        Require(HostImportFor(importing, address(2), 16, ImportRequester::Segment) != nullptr && made(ImportRequester::Segment) == before + 1 && made(ImportRequester::Access) == access, "a write-back segment's import was not counted for the segment requester");
    }
    change(3, true);
    {
        const auto before = counted(ImportRequester::Build);
        const auto access = made(ImportRequester::Access);
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.Upload(true);
        leased.WriteBack();
        Require(made(ImportRequester::Build) > before.made && HostImportCovers(importing, address(3), piece) && made(ImportRequester::Access) == access, "an address-based build's import was not counted for the build requester");
    }
    {
        const auto live = HostImportLiveBytes();
        SetHostImportBudget(live);
        change(4, true);
        const auto before = counted(ImportRequester::Build);
        {
            GuestBufferMemory leased(importing);
            leased.AcquireRegistered();
            leased.Upload(true);
            leased.WriteBack();
        }
        const auto after = counted(ImportRequester::Build);
        SetHostImportBudget(0);
        Require(after.refused > before.refused && after.refusedBytes >= before.refusedBytes + piece && after.made == before.made, "an address-based build's refusal was not counted for the build requester");
    }
    change(5, true);
    {
        const auto before = counted(ImportRequester::Access);
        access.limitMemory(0);
        const auto* failed = HostImportFor(importing, address(5), 16);
        access.limitMemory(std::nullopt);
        Require(failed == nullptr && counted(ImportRequester::Access).failed == before.failed + 1, "a lookup's failed import was not counted for the access requester");
    }
    {
        constexpr std::size_t window = 2 * piece;
        constexpr std::size_t large = 3 * piece;
        auto* joined = static_cast<std::uint8_t*>(::operator new(2 * large, std::align_val_t{window}));
        std::memset(joined, 0x1d, 2 * large);
        SetHostImportWindow(window);
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(joined, large, true, false, true);
            mutation.Add(joined + large, large, true, false, true);
        }
        const auto before = made(ImportRequester::Join);
        const auto second = reinterpret_cast<std::uintptr_t>(joined + large);
        {
            GuestBufferMemory leased(importing);
            leased.AcquireRegistered();
            leased.AddReadable(second - 16, 32);
            leased.Upload(true);
            leased.WriteBack();
        }
        Require(made(ImportRequester::Join) == before + 1, "a join at bind was not counted for the join requester");
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(joined);
            mutation.Remove(joined + large);
        }
        SetHostImportWindow(0);
        ::operator delete(joined, std::align_val_t{window});
    }
    change(10, true);
    change(9, true);
    {
        const auto decided = PrepareImportWatch(importing);
        const auto before = counted(ImportRequester::ColorTarget);
        SetImportWatch(importing, ImportWatch::Unwatch);
        const auto* compared = HostImportFor(importing, address(10), 16, ImportRequester::ColorTarget);
        const auto refused = counted(ImportRequester::ColorTarget);
        SetImportWatch(importing, ImportWatch::Watch);
        const auto* readOnly = HostImportFor(importing, address(9), 16, ImportRequester::ColorTarget);
        const auto* outside = HostImportFor(importing, address(10), 16, ImportRequester::ColorTarget);
        SetImportWatch(importing, decided);
        Require(compared == nullptr && refused.made == before.made && refused.refused == before.refused && refused.failed == before.failed, "a color target's import was made where imports of its memory are compared");
        Require(readOnly == nullptr && !HostImportCovers(importing, address(9), piece), "a color target's import was made of registered memory the guest cannot write");
        Require(outside == nullptr && !HostImportCovers(importing, address(10), piece) && made(ImportRequester::ColorTarget) == before.made, "a color target's import was made of memory outside the write watch");
    }
    const auto counts = HostImportCounters();
    Require(counts.live >= 4 && counts.liveBytes == HostImportLiveBytes(), "the live import counts disagree with the live bytes");
    for (std::size_t index = 0; index < 6; ++index) change(index, false);
    change(10, false);
    change(9, false);
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block + 6 * piece);
    }
    ClearHostImports(importing.device);
    ::operator delete(block, std::align_val_t{piece});
}

void hostImportReportTests() {
    HostImportCounts counts;
    counts.live = 3;
    counts.liveBytes = 3u << 20u;
    counts.requesters[static_cast<std::size_t>(ImportRequester::Map)] = {2, 1u << 20u, 0, 0, 0};
    counts.requesters[static_cast<std::size_t>(ImportRequester::Build)] = {5, 4u << 20u, 7, 7u << 19u, 1, 4};
    counts.requesters[static_cast<std::size_t>(ImportRequester::Segment)] = {1, 1u << 16u, 0, 0, 2};
    counts.requesters[static_cast<std::size_t>(ImportRequester::ColorTarget)] = {1, 1u << 20u, 0, 0, 0};
    const auto text = FormatHostImportCounts(counts);
    Require(text == "3 live (3.0 MiB); made 9 (6.1 MiB), refused 7 (3.5 MiB, 4 remembered), failed 3; by requester (made/refused/failed): map 2/0/0, build 5/7/1, access 0/0/0, join 0/0/0, segment 1/0/2, color target 1/0/0", "the host import report is wrong: " + text);
}

class PieceWindow {
public:
    static constexpr std::size_t Piece = 65536;
    static constexpr std::size_t Window = std::size_t{8} << 20u;

    explicit PieceWindow(const Context& context, bool views = false) : importing(context) {
#ifdef __linux__
        if (views) {
            reserved = mmap(nullptr, 2 * Window, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(reserved != MAP_FAILED, "cannot reserve the block of 16 KiB views");
            block = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(reserved) + Window - 1) / Window * Window);
            file = memfd_create("anyps5-import-block-views", MFD_CLOEXEC);
            Require(file >= 0 && ftruncate(file, static_cast<off_t>(Window)) == 0, "cannot create the file behind the 16 KiB views");
            constexpr std::size_t view = 16384;
            constexpr std::size_t count = Window / view;
            for (std::size_t index = 0; index < count; ++index) {
                const auto offset = static_cast<off_t>((index % 4 * (count / 4) + index / 4) * view);
                Require(mmap(block + index * view, view, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, file, offset) == block + index * view, "cannot map a 16 KiB view");
            }
        }
#endif
        if (block == nullptr) block = static_cast<std::uint8_t*>(::operator new(Window, std::align_val_t{Window}));
        std::memset(block, 0x4b, Window);
        importing.hostImportAlignment = Piece;
        SetHostImportWindow(Window);
        ClearHostImports(importing.device);
    }
    ~PieceWindow() {
        for (const auto offset : registered) {
            GuestAllocations::Mutation mutation;
            mutation.Remove(block + offset);
        }
        ClearHostImports(importing.device);
        SetHostImportBudget(0);
        SetHostImportWindow(0);
#ifndef _WIN32
        if (reserved != nullptr) {
            munmap(reserved, 2 * Window);
            close(file);
            return;
        }
#endif
        ::operator delete(block, std::align_val_t{Window});
    }
    PieceWindow(const PieceWindow&) = delete;
    PieceWindow& operator=(const PieceWindow&) = delete;

    void Add(std::size_t offset, std::size_t bytes, bool writable = true, bool gpu = true) {
        GuestAllocations::Mutation mutation;
        mutation.Add(block + offset, bytes, true, writable, gpu);
        registered.push_back(offset);
    }
    void AddPieces(std::size_t from, std::size_t to) {
        for (auto offset = from; offset < to; offset += Piece) Add(offset, Piece);
    }
    void Remove(std::size_t offset) {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block + offset);
        registered.erase(std::find(registered.begin(), registered.end(), offset));
    }
    std::uintptr_t Address(std::size_t offset) const { return reinterpret_cast<std::uintptr_t>(block + offset); }
    void Build() const {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.Upload(true);
        leased.WriteBack();
    }
    std::uint64_t Serial(std::size_t offset) const { return HostImportSerial(importing, Address(offset), Piece, false); }
    std::size_t Imports(std::size_t from, std::size_t to) const {
        std::vector<std::uint64_t> serials;
        for (auto offset = from; offset < to; offset += Piece) {
            if (const auto serial = Serial(offset); serial != 0) serials.push_back(serial);
        }
        std::sort(serials.begin(), serials.end());
        return static_cast<std::size_t>(std::unique(serials.begin(), serials.end()) - serials.begin());
    }
    bool Covered(std::size_t from, std::size_t to) const {
        for (auto offset = from; offset < to; offset += Piece) {
            if (Serial(offset) == 0) return false;
        }
        return true;
    }
    bool Block(std::size_t offset, std::size_t bytes) const { return HostImportCovers(importing, Address(offset), bytes) && Serial(offset) == Serial(offset + bytes - Piece) && (offset == 0 || Serial(offset - Piece) != Serial(offset)) && (offset + bytes == Window || Serial(offset + bytes) != Serial(offset)); }

    Context importing;

private:
    std::uint8_t* block = nullptr;
    void* reserved = nullptr;
    int file = -1;
    std::vector<std::size_t> registered;
};

std::string blockText(const PieceWindow& window, std::size_t from, std::size_t to) {
    return std::to_string(window.Imports(from, to)) + " imports";
}

void importBlockMapTests(const Context& context, bool views) {
    PieceWindow window(context, views);
    constexpr auto half = PieceWindow::Window / 2;
    for (std::size_t offset = 0; offset < half; offset += PieceWindow::Piece) {
        window.Add(offset, PieceWindow::Piece);
        GuestAllocations::Mapped ranges;
        const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        for (const auto& range : lease) {
            if (range->address == window.Address(offset)) ranges.push_back(range);
        }
        static_cast<void>(ImportMappedRanges(window.importing, ranges, GuestAllocations::GuestAllocationsGeneration_nid_postfix(), true));
    }
    Require(HostImportFor(window.importing, window.Address(0), 16) != nullptr, "the first of 64 pieces mapped one by one was not imported");
    Require(window.Imports(0, half) == 1 && window.Block(0, half), std::string(views ? "64 adjacent 64 KiB pieces of four 16 KiB views each" : "64 adjacent 64 KiB pieces") + " mapped one by one were not imported as one 4 MiB block: " + blockText(window, 0, half));
}

void importBlockBuildTests(const Context& context) {
    PieceWindow window(context);
    constexpr auto half = PieceWindow::Window / 2;
    window.AddPieces(half, PieceWindow::Window);
    window.Build();
    Require(window.Imports(half, PieceWindow::Window) == 1 && window.Block(half, half), "64 adjacent 64 KiB pieces were not imported as one 4 MiB block by an address-based build: " + blockText(window, half, PieceWindow::Window));
    window.Add(0, half, false);
    window.Build();
    Require(window.Imports(0, PieceWindow::Window) == 2 && window.Block(0, half) && window.Block(half, half), "a 4 MiB read-only range and 64 writable 64 KiB pieces in one window were not imported as the range and one 4 MiB block: " + blockText(window, 0, PieceWindow::Window));
}

void importBlockHoleTests(const Context& context) {
    PieceWindow window(context);
    constexpr auto half = PieceWindow::Window / 2;
    window.Add(0, half);
    window.AddPieces(half, PieceWindow::Window - PieceWindow::Piece);
    window.Build();
    constexpr std::size_t KiB = 1024;
    Require(window.Imports(0, PieceWindow::Window) == 7, "a window whose last 64 KiB is unmapped was not imported as 7 blocks: " + blockText(window, 0, PieceWindow::Window));
    Require(window.Block(0, half) && window.Block(half, 2048 * KiB) && window.Block(half + 2048 * KiB, 1024 * KiB) && window.Block(half + 3072 * KiB, 512 * KiB) && window.Block(half + 3584 * KiB, 256 * KiB) && window.Block(half + 3840 * KiB, 128 * KiB) && window.Block(half + 3968 * KiB, 64 * KiB), "a window whose last 64 KiB is unmapped was not split into the largest aligned blocks");
}

void importBlockUnalignedTests(const Context& context) {
    PieceWindow window(context);
    constexpr std::size_t KiB = 1024;
    window.AddPieces(768 * KiB, 3840 * KiB);
    window.Build();
    Require(window.Imports(768 * KiB, 3840 * KiB) == 5, "48 pieces from 768 KiB were not imported as 5 aligned blocks: " + blockText(window, 768 * KiB, 3840 * KiB));
    Require(window.Block(768 * KiB, 256 * KiB) && window.Block(1024 * KiB, 1024 * KiB) && window.Block(2048 * KiB, 1024 * KiB) && window.Block(3072 * KiB, 512 * KiB) && window.Block(3584 * KiB, 256 * KiB), "48 pieces from 768 KiB were not split into 256 KiB, 1 MiB, 1 MiB, 512 KiB and 256 KiB blocks");
}

void importBlockFlagTests(const Context& context) {
    PieceWindow window(context);
    constexpr auto half = PieceWindow::Window / 2;
    constexpr auto split = half / 2;
    window.AddPieces(0, split);
    window.Add(split, PieceWindow::Piece, true, false);
    window.AddPieces(split + PieceWindow::Piece, half);
    window.Build();
    Require(window.Serial(split) == 0 && window.Imports(0, split) == 1 && window.Block(0, split), "pieces before a range that is not GPU mapped were not imported as one block: " + blockText(window, 0, split));
    Require(window.Imports(split + PieceWindow::Piece, half) == 5 && window.Covered(split + PieceWindow::Piece, half), "pieces after a range that is not GPU mapped were not imported as the 5 aligned blocks after it: " + blockText(window, split + PieceWindow::Piece, half));
}

void importBlockRemapTests(const Context& context) {
    PieceWindow window(context);
    constexpr auto half = PieceWindow::Window / 2;
    window.AddPieces(0, half);
    window.Build();
    Require(window.Imports(0, half) == 1 && window.Block(0, half), "64 pieces were not imported as one block before the remap: " + blockText(window, 0, half));
    const auto before = window.Serial(0);
    constexpr auto remapped = 10 * PieceWindow::Piece;
    window.Remove(remapped);
    window.Add(remapped, PieceWindow::Piece);
    window.Build();
    Require(window.Imports(0, half) == 1 && window.Block(0, half) && window.Serial(0) != before, "a piece remapped inside a block did not retire the block and import it again whole: " + blockText(window, 0, half));
}

void importBlockFailureTests(const Context& context, const BdaTestAccess& access) {
    PieceWindow window(context);
    constexpr std::size_t MiB = std::size_t{1} << 20u;
    constexpr auto half = PieceWindow::Window / 2;
    window.AddPieces(0, PieceWindow::Window);
    access.limitImports(half);
    window.Build();
    Require(window.Imports(0, PieceWindow::Window) == 2 && window.Block(0, half) && window.Block(half, half), "a window whose 8 MiB import failed did not step down to two 4 MiB blocks: " + blockText(window, 0, PieceWindow::Window));
    const auto attempts = access.allocationAttempts();
    window.Build();
    Require(access.allocationAttempts() == attempts && window.Imports(0, PieceWindow::Window) == 2, "an unchanged failed block was tried again");
    access.limitImports(std::nullopt);
    constexpr auto remapped = 7 * MiB;
    window.Remove(remapped);
    window.Add(remapped, PieceWindow::Piece);
    window.Build();
    Require(window.Imports(0, PieceWindow::Window) == 1 && window.Block(0, PieceWindow::Window), "a block that failed was not tried again after one of its ranges was mapped again: " + blockText(window, 0, PieceWindow::Window));
}

void importRunFailureShrinkTests(const Context& context, const BdaTestAccess& access) {
    PieceWindow window(context);
    constexpr auto half = PieceWindow::Window / 2;
    window.AddPieces(0, PieceWindow::Window);
    Require(HostImportFor(window.importing, window.Address(0), 16) != nullptr && window.Block(0, PieceWindow::Window), "a window of registered pieces was not imported as one 8 MiB block: " + blockText(window, 0, PieceWindow::Window));
    ClearHostImports(window.importing.device);
    access.limitImports(half);
    Require(HostImportFor(window.importing, window.Address(0), 16) != nullptr && window.Block(0, half), "a lookup on an unchanged registry whose 8 MiB block failed after an earlier lookup did not shrink to the 4 MiB block: " + blockText(window, 0, PieceWindow::Window));
    access.limitImports(std::nullopt);
    Require(HostImportFor(window.importing, window.Address(half), 16) != nullptr && window.Block(0, half) && window.Block(half, half), "a lookup on an unchanged registry after its 8 MiB block failed imported the failed block again: " + blockText(window, 0, PieceWindow::Window));
}

void importBlockBudgetTests(const Context& context) {
    PieceWindow window(context);
    constexpr std::size_t MiB = std::size_t{1} << 20u;
    constexpr auto half = PieceWindow::Window / 2;
    window.AddPieces(0, half);
    SetHostImportBudget(HostImportLiveBytes() + 3 * MiB);
    window.Build();
    Require(window.Imports(0, half) == 2 && window.Block(0, 2 * MiB) && window.Block(2 * MiB, MiB) && !window.Covered(3 * MiB, half), "a 4 MiB block over the budget did not step down to a 2 MiB and a 1 MiB block: " + blockText(window, 0, half));
    const auto low = window.Serial(0);
    const auto high = window.Serial(2 * MiB);
    window.Add(6 * MiB, PieceWindow::Piece);
    window.Build();
    Require(window.Serial(0) == low && window.Serial(2 * MiB) == high && window.Imports(0, PieceWindow::Window) == 2, "blocks stepped down for the budget were imported again after an unrelated range was mapped: " + blockText(window, 0, PieceWindow::Window));
}

void importRefusalMemoryTests(const Context& context) {
    constexpr std::size_t piece = 4096;
    constexpr std::size_t count = 12000;
    constexpr std::size_t fitting = count * 6 / 10;
    constexpr std::size_t refused = count - fitting;
    auto importing = context;
    importing.hostImportAlignment = piece;
    auto* block = static_cast<std::uint8_t*>(::operator new(2 * count * piece, std::align_val_t{piece}));
    auto* other = static_cast<std::uint8_t*>(::operator new(piece, std::align_val_t{piece}));
    const auto at = [&](std::size_t index) { return block + 2 * index * piece; };
    SetHostImportWindow(std::size_t{8} << 20u);
    ClearHostImports(importing.device);
    {
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 0; index < count; ++index) mutation.Add(at(index), piece, true, true, true);
    }
    const auto budget = HostImportLiveBytes() + fitting * piece;
    SetHostImportBudget(budget);
    bool added = false;
    const auto rebuild = [&] {
        {
            GuestAllocations::Mutation mutation;
            if (added) mutation.Remove(other);
            else mutation.Add(other, piece, true, true, false);
        }
        added = !added;
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
    };
    const auto counted = [](ImportRequester requester) { return HostImportCounters().requesters[static_cast<std::size_t>(requester)]; };
    const auto tries = [](const HostImportRequesterCounts& requested) { return requested.refused - requested.remembered; };
    auto before = counted(ImportRequester::Build);
    rebuild();
    auto after = counted(ImportRequester::Build);
    Require(after.made - before.made == fitting && tries(after) - tries(before) == refused, "a registry of 12000 ranges over a budget for 7200 of them was not imported and refused as expected: " + std::to_string(after.made - before.made) + " made, " + std::to_string(tries(after) - tries(before)) + " refused");
    for (int round = 1; round <= 4; ++round) {
        before = after;
        rebuild();
        after = counted(ImportRequester::Build);
        Require(tries(after) == tries(before), "rebuild " + std::to_string(round) + " tried " + std::to_string(tries(after) - tries(before)) + " refused ranges again with the budget unchanged");
        Require(after.refused - before.refused == refused && after.remembered - before.remembered == refused && after.made == before.made, "rebuild " + std::to_string(round) + " did not count its remembered refusals as refusals");
    }
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(at(0));
    }
    before = after;
    rebuild();
    after = counted(ImportRequester::Build);
    Require(after.made - before.made == 1 && HostImportCovers(importing, reinterpret_cast<std::uintptr_t>(at(fitting)), piece), "the budget a retired import freed was not used for a remembered refusal: " + std::to_string(after.made - before.made) + " made");
    SetHostImportBudget(budget + piece);
    before = after;
    rebuild();
    after = counted(ImportRequester::Build);
    Require(after.made - before.made == 1 && HostImportCovers(importing, reinterpret_cast<std::uintptr_t>(at(fitting + 1)), piece), "a larger budget did not end the remembered refusals: " + std::to_string(after.made - before.made) + " made");
    const auto lookups = counted(ImportRequester::Access);
    static_cast<void>(HostImportFor(importing, reinterpret_cast<std::uintptr_t>(at(count - 1)), 16));
    const auto looked = counted(ImportRequester::Access);
    Require(looked.remembered == lookups.remembered && looked.made + looked.refused + looked.failed == lookups.made + lookups.refused + lookups.failed + 1, "a lookup that may evict was answered from the build's remembered refusals");
    {
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 1; index < count; ++index) mutation.Remove(at(index));
        if (added) mutation.Remove(other);
    }
    ClearHostImports(importing.device);
    SetHostImportBudget(0);
    SetHostImportWindow(0);
    ::operator delete(other, std::align_val_t{piece});
    ::operator delete(block, std::align_val_t{piece});
}

void evictingRefusalMemoryTests(const Context& context) {
    constexpr std::size_t piece = 65536;
    constexpr std::size_t filled = 32;
    constexpr std::size_t count = filled + 3;
    auto importing = context;
    importing.hostImportAlignment = piece;
    auto* block = static_cast<std::uint8_t*>(::operator new(2 * count * piece, std::align_val_t{piece}));
    const auto at = [&](std::size_t index) { return block + 2 * index * piece; };
    const auto address = [&](std::size_t index) { return reinterpret_cast<std::uintptr_t>(at(index)); };
    ClearHostImports(importing.device);
    {
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 0; index < count; ++index) mutation.Add(at(index), piece, true, true, true);
    }
    const auto budget = HostImportLiveBytes() + filled * piece;
    SetHostImportBudget(budget);
    for (std::size_t index = 0; index < filled; ++index) Require(HostImportFor(importing, address(index), 16) != nullptr, "a piece inside the import budget was not imported");
    Require(HostImportLiveBytes() == budget, "the pieces did not fill the import budget");
    const auto counted = [] { return HostImportCounters().requesters[static_cast<std::size_t>(ImportRequester::Access)]; };
    auto before = counted();
    for (int request = 0; request < 8; ++request) Require(HostImportFor(importing, address(filled), 16) == nullptr, "a piece over a full budget with nothing evictable was imported");
    auto after = counted();
    Require(after.refused - before.refused == 8 && after.remembered - before.remembered == 7, "8 evicting requests for one refused piece at a full budget with nothing evictable scanned the imports " + std::to_string(8 - (after.remembered - before.remembered)) + " times, not once");
    SetHostImportBudget(budget - piece);
    before = counted();
    Require(HostImportFor(importing, address(filled), 16) == nullptr && counted().remembered == before.remembered + 1, "a remembered evicting refusal ended when the budget was lowered");
    SetHostImportBudget(budget);
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(at(0));
    }
    before = counted();
    Require(HostImportFor(importing, address(filled), 16) != nullptr && counted().remembered == before.remembered, "an evicting request was answered from its remembered refusal after an import retired");
    static_cast<void>(HostImportFor(importing, address(filled + 1), 16));
    before = counted();
    Require(HostImportFor(importing, address(filled + 1), 16) == nullptr && counted().remembered == before.remembered + 1, "a second evicting request for a refused piece was not answered from memory");
    SetHostImportBudget(budget + piece);
    before = counted();
    Require(HostImportFor(importing, address(filled + 1), 16) != nullptr && counted().remembered == before.remembered, "an evicting request was answered from its remembered refusal after the budget was raised");
    static_cast<void>(HostImportFor(importing, address(filled + 2), 16));
    {
        GuestBufferMemory upload(importing);
        upload.Upload(false);
        upload.WriteBack();
    }
    before = counted();
    Require(HostImportFor(importing, address(filled + 2), 16) == nullptr && counted().remembered == before.remembered + 1, "a remembered evicting refusal ended when a build raised the floor while no import could age out of its protection yet");
    NoteHostImportFrame();
    NoteHostImportFrame();
    for (int use = 0; use < 200; ++use) Require(HostImportFor(importing, address(filled + 1), 16) != nullptr, "a live import was not found");
    before = counted();
    Require(HostImportFor(importing, address(filled + 2), 16) != nullptr && counted().remembered == before.remembered, "an evicting request was answered from its remembered refusal after the imports aged out of their protection");
    {
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 1; index < count; ++index) mutation.Remove(at(index));
    }
    ClearHostImports(importing.device);
    SetHostImportBudget(0);
    ::operator delete(block, std::align_val_t{piece});
}

void evictingRefusalAgingTests(const Context& context) {
    constexpr std::size_t piece = 65536;
    constexpr std::size_t filled = 16;
    auto importing = context;
    importing.hostImportAlignment = piece;
    auto* block = static_cast<std::uint8_t*>(::operator new(2 * (filled + 1) * piece, std::align_val_t{piece}));
    const auto at = [&](std::size_t index) { return block + 2 * index * piece; };
    const auto address = [&](std::size_t index) { return reinterpret_cast<std::uintptr_t>(at(index)); };
    ClearHostImports(importing.device);
    {
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 0; index <= filled; ++index) mutation.Add(at(index), piece, true, true, true);
    }
    SetHostImportBudget(HostImportLiveBytes() + filled * piece);
    for (std::size_t index = 0; index < filled; ++index) Require(HostImportFor(importing, address(index), 16) != nullptr, "a piece inside the import budget was not imported");
    {
        GuestBufferMemory upload(importing);
        upload.Upload(false);
        upload.WriteBack();
    }
    const auto counted = [] { return HostImportCounters().requesters[static_cast<std::size_t>(ImportRequester::Access)]; };
    static_cast<void>(HostImportFor(importing, address(filled), 16));
    auto before = counted();
    Require(HostImportFor(importing, address(filled), 16) == nullptr && counted().remembered == before.remembered + 1, "a refusal while the imports were recent but no longer held by a build was not remembered");
    NoteHostImportFrame();
    NoteHostImportFrame();
    for (int use = 0; use < 200; ++use) Require(HostImportFor(importing, address(filled - 1), 16) != nullptr, "a live import was not found");
    before = counted();
    Require(HostImportFor(importing, address(filled), 16) != nullptr && counted().remembered == before.remembered, "an evicting request was answered from its remembered refusal after imports no build held aged out of their protection");
    {
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 0; index <= filled; ++index) mutation.Remove(at(index));
    }
    ClearHostImports(importing.device);
    SetHostImportBudget(0);
    ::operator delete(block, std::align_val_t{piece});
}

void evictingRefusalRangeTests(const Context& context) {
    PieceWindow window(context);
    constexpr auto piece = PieceWindow::Piece;
    constexpr auto fillers = PieceWindow::Window / 2;
    window.Add(0, 2 * piece);
    window.Add(2 * piece, piece);
    window.Add(3 * piece, piece);
    for (std::size_t filler = 0; filler < 4; ++filler) {
        window.Add(fillers + 2 * filler * piece, piece);
        Require(HostImportFor(window.importing, window.Address(fillers + 2 * filler * piece), 16) != nullptr, "a filler piece inside the import budget was not imported");
    }
    SetHostImportBudget(HostImportLiveBytes() + piece);
    Require(HostImportFor(window.importing, window.Address(0), 16) == nullptr, "a 128 KiB range in a 256 KiB block over the budget was imported");
    Require(HostImportFor(window.importing, window.Address(2 * piece), 16) != nullptr && window.Block(2 * piece, piece), "a 64 KiB range of the same 256 KiB block was answered from the 128 KiB range's refusal instead of stepping down to its own import");
}

void evictingRefusalBlockRangesTests(const Context& context) {
    PieceWindow window(context);
    constexpr auto piece = PieceWindow::Piece;
    constexpr auto fillers = PieceWindow::Window / 2;
    window.AddPieces(0, 4 * piece);
    for (std::size_t filler = 0; filler < 4; ++filler) {
        window.Add(fillers + 2 * filler * piece, piece);
        Require(HostImportFor(window.importing, window.Address(fillers + 2 * filler * piece), 16) != nullptr, "a filler piece inside the import budget was not imported");
    }
    SetHostImportBudget(HostImportLiveBytes());
    const auto counted = [] { return HostImportCounters().requesters[static_cast<std::size_t>(ImportRequester::Access)]; };
    const auto before = counted();
    for (int request = 0; request < 8; ++request) Require(HostImportFor(window.importing, window.Address(static_cast<std::size_t>(request % 2) * piece), 16) == nullptr, "a 64 KiB range of a block over a full budget with nothing evictable was imported");
    const auto after = counted();
    Require(after.refused - before.refused == 8 && after.remembered - before.remembered == 7, "8 evicting requests alternating between two 64 KiB ranges of one refused 256 KiB block scanned the imports " + std::to_string(8 - (after.remembered - before.remembered)) + " times, not once");
    SetHostImportBudget(HostImportLiveBytes() + piece);
    Require(HostImportFor(window.importing, window.Address(piece), 16) != nullptr && window.Block(piece, piece), "a 64 KiB range of the block was answered from the full budget's refusal instead of stepping down to its own import once the budget had room for it");
}

void evictingRefusalJoinTests(const Context& context) {
    PieceWindow window(context);
    constexpr auto piece = PieceWindow::Piece;
    constexpr auto fillers = PieceWindow::Window / 2;
    window.AddPieces(0, 3 * piece);
    for (std::size_t filler = 0; filler < 4; ++filler) {
        window.Add(fillers + 2 * filler * piece, piece);
        Require(HostImportFor(window.importing, window.Address(fillers + 2 * filler * piece), 16) != nullptr, "a filler piece inside the import budget was not imported");
    }
    SetHostImportBudget(HostImportLiveBytes());
    const auto upload = [&] {
        GuestBufferMemory plain(window.importing);
        plain.AddReadable(window.Address(0), 3 * piece);
        plain.Upload(false);
        plain.WriteBack();
    };
    const auto counted = [] { return HostImportCounters().requesters[static_cast<std::size_t>(ImportRequester::Join)]; };
    const auto before = counted();
    for (int request = 0; request < 8; ++request) upload();
    const auto after = counted();
    Require(!HostImportCovers(window.importing, window.Address(0), 3 * piece), "a join of three 64 KiB ranges over a full budget with nothing evictable was imported");
    Require(after.refused - before.refused == 8 && after.remembered - before.remembered == 7, "8 uploads bound across three refused ranges scanned the imports for their join " + std::to_string(8 - (after.remembered - before.remembered)) + " times, not once");
    SetHostImportBudget(HostImportLiveBytes() + 3 * piece);
    upload();
    Require(HostImportCovers(window.importing, window.Address(0), 3 * piece) && counted().made == after.made + 1, "the join was answered from the full budget's refusal after the budget grew");
}

void evictingRefusalStepDownTests(const Context& context) {
    PieceWindow window(context);
    constexpr auto piece = PieceWindow::Piece;
    constexpr auto fillers = PieceWindow::Window / 2;
    window.AddPieces(0, 4 * piece);
    for (std::size_t filler = 0; filler < 4; ++filler) {
        window.Add(fillers + 2 * filler * piece, piece);
        Require(HostImportFor(window.importing, window.Address(fillers + 2 * filler * piece), 16) != nullptr, "a filler piece inside the import budget was not imported");
    }
    SetHostImportBudget(HostImportLiveBytes() + 2 * piece);
    {
        GuestBufferMemory leased(window.importing);
        leased.AcquireRegistered();
        leased.Upload(true);
        leased.WriteBack();
    }
    Require(window.Block(0, 2 * piece) && window.Serial(2 * piece) == 0, "the address-based build did not step down from the refused 256 KiB block to the 128 KiB block that fits");
    {
        GuestBufferMemory upload(window.importing);
        upload.Upload(false);
        upload.WriteBack();
    }
    const auto counted = [] { return HostImportCounters().requesters[static_cast<std::size_t>(ImportRequester::Access)]; };
    const auto before = counted();
    for (int request = 0; request < 8; ++request) Require(HostImportFor(window.importing, window.Address(2 * piece), 16) == nullptr, "the rest of a block over a full budget with nothing evictable was imported");
    const auto after = counted();
    const auto scans = (after.refused - before.refused) - (after.remembered - before.remembered);
    Require(after.refused - before.refused == 16 && scans == 1, "8 evicting requests that step down from a refused block scanned the imports " + std::to_string(scans) + " times, not once");
    SetHostImportBudget(HostImportLiveBytes() + piece);
    Require(HostImportFor(window.importing, window.Address(2 * piece), 16) != nullptr && window.Block(2 * piece, piece), "a 64 KiB range was answered from a block's refusal remembered before the budget grew instead of stepping down to its own import");
}

void evictingRefusalWindowTests(const Context& context) {
    PieceWindow window(context);
    constexpr auto piece = PieceWindow::Piece;
    constexpr auto fillers = PieceWindow::Window / 2;
    SetHostImportWindow(0);
    window.Add(0, piece);
    window.Add(piece, piece);
    for (std::size_t filler = 0; filler < 4; ++filler) {
        window.Add(fillers + 2 * filler * piece, piece);
        Require(HostImportFor(window.importing, window.Address(fillers + 2 * filler * piece), 16) != nullptr, "a filler piece inside the import budget was not imported");
    }
    SetHostImportBudget(HostImportLiveBytes() + piece);
    Require(HostImportFor(window.importing, window.Address(0), 16) == nullptr, "two adjacent 64 KiB ranges over the budget were imported without an import window");
    SetHostImportWindow(PieceWindow::Window);
    Require(HostImportFor(window.importing, window.Address(0), 16) != nullptr && window.Block(0, piece), "a 64 KiB range was answered from a refusal made without the import window instead of stepping down once the window was set");
}

void importDeviceBudgetTests() {
    constexpr std::uint64_t MiB = std::uint64_t{1} << 20u;
    constexpr std::uint64_t GiB = std::uint64_t{1} << 30u;
    VkPhysicalDeviceMemoryProperties memory{};
    memory.memoryHeapCount = 2;
    memory.memoryHeaps[0] = {8 * GiB, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    memory.memoryHeaps[1] = {16 * GiB, 0};
    VkPhysicalDeviceMemoryBudgetPropertiesEXT reported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
    const auto text = [](std::uint64_t bytes) { return std::to_string(bytes >> 20u) + " MiB"; };
    Require(HostImportBudgetFor(memory, nullptr, 6 * GiB, 0) == 6 * GiB, "an import budget without a memory budget report is not the configured one");
    Require(HostImportBudgetFor(memory, &reported, 6 * GiB, 0) == 6 * GiB, "a zero reported budget of the host heap did not leave the configured import budget");
    reported.heapBudget[1] = 12 * GiB;
    reported.heapUsage[1] = 5 * GiB;
    const auto granted = HostImportBudgetFor(memory, &reported, 10 * GiB, 2 * GiB);
    Require(granted == 7 * GiB + 512 * MiB, "a 12 GiB host heap budget using 3 GiB besides 2 GiB of imports did not give 7.5 GiB to imports: " + text(granted));
    Require(HostImportBudgetFor(memory, &reported, 6 * GiB, 2 * GiB) == 6 * GiB, "a reported host heap budget raised the configured import budget");
    reported.heapUsage[1] = 5 * GiB + 10 * MiB;
    const auto rounded = HostImportBudgetFor(memory, &reported, 10 * GiB, 2 * GiB);
    Require(rounded == 7 * GiB + 448 * MiB, "the import budget from a reported heap was not rounded down to 64 MiB: " + text(rounded));
    reported.heapUsage[1] = 5 * GiB;
    const auto uncounted = HostImportBudgetFor(memory, &reported, 64 * GiB, 8 * GiB);
    Require(uncounted == 10 * GiB + 512 * MiB, "imports the heap usage does not count were taken from other usage: " + text(uncounted));
    reported.heapBudget[1] = 4 * GiB;
    reported.heapUsage[1] = 6 * GiB;
    Require(HostImportBudgetFor(memory, &reported, 6 * GiB, GiB) == 0, "a host heap used past its reported budget left room for imports");
    reported.heapBudget[0] = 1 * GiB;
    reported.heapBudget[1] = 0;
    Require(HostImportBudgetFor(memory, &reported, 6 * GiB, 0) == 6 * GiB, "the device-local heap's budget was taken for the host heap's");
    constexpr auto unreported = std::numeric_limits<std::uint64_t>::max();
    Require(HostImportBudgetWith(6400 * MiB, true, 4480 * MiB, false, 0) == 6400 * MiB, "a host heap report lowered a set import budget");
    Require(HostImportBudgetWith(6400 * MiB, true, 4480 * MiB, true, 0) == 6400 * MiB, "a host heap report lowered a set import budget after a vkAllocateMemory failure");
    Require(HostImportBudgetWith(6400 * MiB, true, 4480 * MiB, true, 3752 * MiB) == 3752 * MiB, "the ceiling after a vkAllocateMemory failure did not lower a set import budget");
    Require(HostImportBudgetWith(5888 * MiB, false, 4480 * MiB, false, 0) == 5888 * MiB, "a host heap report lowered the default import budget before any vkAllocateMemory failure");
    Require(HostImportBudgetWith(5888 * MiB, false, 4480 * MiB, true, 0) == 4480 * MiB, "the default import budget did not take a lower host heap report after a vkAllocateMemory failure");
    Require(HostImportBudgetWith(5888 * MiB, false, 8 * GiB, true, 0) == 5888 * MiB, "a higher host heap report raised the default import budget");
    Require(HostImportBudgetWith(5888 * MiB, false, unreported, true, 0) == 5888 * MiB, "a device without a memory budget report changed the default import budget");
    Require(HostImportBudgetWith(5888 * MiB, false, 4480 * MiB, true, 3000 * MiB) == 3000 * MiB, "the ceiling below a host heap report did not hold the default import budget");
    Require(HostImportBudgetWith(5888 * MiB, false, 0, true, 0) != 0, "a host heap report that leaves imports no room made the default import budget 0, which the refusal memory reads as no budget");
}

void importAllocationErrorTests(const Context& context, VkResult failure, bool nearBudget) {
    constexpr std::size_t piece = 65536;
    constexpr std::size_t count = 8;
    auto* block = static_cast<std::uint8_t*>(::operator new(2 * count * piece, std::align_val_t{piece}));
    auto importing = context;
    importing.hostImportAlignment = piece;
    ImportAllocationProbe probe(importing);
    SetHostImportWindow(0);
    ClearHostImports(importing.device);
    {
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 0; index < count; ++index) mutation.Add(block + 2 * index * piece, piece, true, true, true);
    }
    SetHostImportBudget((nearBudget ? 2 : 64) * piece);
    std::size_t next = 0;
    const auto lookup = [&] { return HostImportFor(importing, reinterpret_cast<std::uintptr_t>(block + 2 * next++ * piece), 16) != nullptr; };
    const auto counted = [] { return HostImportCounters().requesters[static_cast<std::size_t>(ImportRequester::Access)]; };
    Require(lookup(), "the allocation error test's first import was not made");
    probe.failure = failure;
    const auto before = counted();
    for (int attempt = 0; attempt < 3; ++attempt) Require(!lookup(), "an import with an injected allocation failure was made");
    const auto after = counted();
    probe.failure = VK_SUCCESS;
    const bool exhausted = failure == VK_ERROR_OUT_OF_DEVICE_MEMORY || failure == VK_ERROR_OUT_OF_HOST_MEMORY;
    const auto failed = exhausted && nearBudget ? 1u : 3u;
    Require(after.failed - before.failed == failed && after.refused - before.refused == 3 - failed, "allocation error " + std::to_string(failure) + " changed the import failure/refusal counts");
    Require(lookup() != exhausted, "allocation error " + std::to_string(failure) + " incorrectly changed the host import budget");
    {
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 0; index < count; ++index) mutation.Remove(block + 2 * index * piece);
    }
    ClearHostImports(importing.device);
    SetHostImportBudget(0);
    ::operator delete(block, std::align_val_t{piece});
}

void importCeilingTests(const Context& context, const BdaTestAccess& access, bool nearBudget) {
    constexpr std::size_t piece = 65536;
    constexpr std::size_t count = 24;
    auto* block = static_cast<std::uint8_t*>(::operator new(2 * count * piece, std::align_val_t{piece}));
    auto importing = context;
    importing.hostImportAlignment = piece;
    SetHostImportWindow(0);
    ClearHostImports(importing.device);
    {
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 0; index < count; ++index) mutation.Add(block + 2 * index * piece, piece, true, true, true);
    }
    std::size_t next = 0;
    const auto lookup = [&] { return HostImportFor(importing, reinterpret_cast<std::uintptr_t>(block + 2 * next++ * piece), 16) != nullptr; };
    const auto counted = [] { return HostImportCounters().requesters[static_cast<std::size_t>(ImportRequester::Access)]; };
    const auto outcome = [](const HostImportRequesterCounts& before, const HostImportRequesterCounts& after) { return std::to_string(after.failed - before.failed) + " failed, " + std::to_string(after.refused - before.refused) + " refused"; };
    if (nearBudget) {
        SetHostImportBudget(HostImportLiveBytes() + 2 * piece);
        Require(lookup(), "the ceiling test's first import was not made");
        access.limitImports(0);
        const auto before = counted();
        for (int attempt = 0; attempt < 3; ++attempt) static_cast<void>(lookup());
        const auto after = counted();
        access.limitImports(std::nullopt);
        Require(after.failed - before.failed == 1 && after.refused - before.refused == 2, "imports after a vkAllocateMemory failure within an eighth of the budget were not one failure and then refusals: " + outcome(before, after));
    } else {
        SetHostImportBudget(64 * piece);
        Require(lookup(), "the ceiling test's first import was not made");
        access.limitImports(0);
        auto before = counted();
        Require(!lookup(), "an import past the mock's import limit was made");
        access.limitImports(std::nullopt);
        Require(lookup() && counted().failed == before.failed + 1, "one vkAllocateMemory failure far below the import budget lowered it");
        access.limitImports(0);
        before = counted();
        for (int attempt = 0; attempt < 5; ++attempt) static_cast<void>(lookup());
        const auto after = counted();
        access.limitImports(std::nullopt);
        Require(after.failed - before.failed == 3 && after.refused - before.refused == 2, "five imports after vkAllocateMemory failures far below the budget were not three failures and then refusals: " + outcome(before, after));
    }
    Require(!lookup(), "an import was made over the lowered ceiling");
    SetHostImportBudget(0);
    Require(lookup(), "a new import budget did not lift the ceiling");
    {
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 0; index < count; ++index) mutation.Remove(block + 2 * index * piece);
    }
    ClearHostImports(importing.device);
    ::operator delete(block, std::align_val_t{piece});
}

std::uint64_t reportedHostHeapBudget = 0;
std::uint64_t reportedHostHeapUsage = 0;

void VKAPI_PTR reportHostHeap(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2* properties) {
    for (auto* next = static_cast<VkBaseOutStructure*>(properties->pNext); next != nullptr; next = next->pNext) {
        if (next->sType != VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT) continue;
        auto* reported = reinterpret_cast<VkPhysicalDeviceMemoryBudgetPropertiesEXT*>(next);
        reported->heapBudget[1] = reportedHostHeapBudget;
        reported->heapUsage[1] = reportedHostHeapUsage;
    }
}

void importReportFloorTests(const Context& context, bool setBudget, VkResult failure) {
    constexpr std::size_t piece = 65536;
    constexpr std::size_t count = 24;
    constexpr std::uint64_t GiB = std::uint64_t{1} << 30u;
    static std::uintptr_t physical = 0x1000;
    auto* block = static_cast<std::uint8_t*>(::operator new(2 * count * piece, std::align_val_t{piece}));
    auto importing = context;
    importing.hostImportAlignment = piece;
    ImportAllocationProbe probe(importing);
    importing.memory.memoryHeapCount = 2;
    importing.memory.memoryHeaps[0] = {8 * GiB, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    importing.memory.memoryHeaps[1] = {16 * GiB, 0};
    importing.memoryProperties2 = reportHostHeap;
    importing.physical = reinterpret_cast<VkPhysicalDevice>(physical += 0x10);
    reportedHostHeapBudget = 8 * (std::uint64_t{1} << 20u);
    reportedHostHeapUsage = reportedHostHeapBudget;
    SetHostImportWindow(0);
    SetHostImportBudget(setBudget ? 64 * piece : 0);
    ClearHostImports(importing.device);
    {
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 0; index < count; ++index) mutation.Add(block + 2 * index * piece, piece, true, true, true);
    }
    std::size_t next = 0;
    const auto lookup = [&] { return HostImportFor(importing, reinterpret_cast<std::uintptr_t>(block + 2 * next++ * piece), 16) != nullptr; };
    const std::string budget = setBudget ? "a set import budget" : "the default import budget";
    for (int made = 0; made < 3; ++made) Require(lookup(), budget + " was lowered below " + std::to_string(made + 1) + " imports by a host heap report that leaves imports no room, before any vkAllocateMemory failure");
    probe.failure = failure;
    Require(!lookup(), "an import with an injected allocation failure was made");
    probe.failure = VK_SUCCESS;
    Require(lookup() == setBudget, budget + " incorrectly applied the host heap report after allocation error " + std::to_string(failure));
    SetHostImportBudget(0);
    {
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 0; index < count; ++index) mutation.Remove(block + 2 * index * piece);
    }
    ClearHostImports(importing.device);
    ::operator delete(block, std::align_val_t{piece});
}

void importReportRefusalMemoryTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t piece = 65536;
    constexpr std::size_t count = 24;
    constexpr std::uint64_t GiB = std::uint64_t{1} << 30u;
    static std::uintptr_t physical = 0x800000;
    auto* block = static_cast<std::uint8_t*>(::operator new(2 * count * piece, std::align_val_t{piece}));
    auto* other = static_cast<std::uint8_t*>(::operator new(piece, std::align_val_t{piece}));
    auto importing = context;
    importing.hostImportAlignment = piece;
    importing.memory.memoryHeapCount = 2;
    importing.memory.memoryHeaps[0] = {8 * GiB, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    importing.memory.memoryHeaps[1] = {16 * GiB, 0};
    importing.memoryProperties2 = reportHostHeap;
    importing.physical = reinterpret_cast<VkPhysicalDevice>(physical += 0x10);
    reportedHostHeapBudget = 8 * (std::uint64_t{1} << 20u);
    reportedHostHeapUsage = reportedHostHeapBudget;
    SetHostImportWindow(0);
    SetHostImportBudget(0);
    ClearHostImports(importing.device);
    {
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 0; index < count; ++index) mutation.Add(block + 2 * index * piece, piece, true, true, true);
    }
    access.limitImports(0);
    Require(HostImportFor(importing, reinterpret_cast<std::uintptr_t>(block), 16) == nullptr, "an import past the mock's import limit was made");
    access.limitImports(std::nullopt);
    bool added = false;
    const auto rebuild = [&] {
        {
            GuestAllocations::Mutation mutation;
            if (added) mutation.Remove(other);
            else mutation.Add(other, piece, true, true, false);
        }
        added = !added;
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
    };
    const auto counted = [] { return HostImportCounters().requesters[static_cast<std::size_t>(ImportRequester::Build)]; };
    const auto tries = [](const HostImportRequesterCounts& requested) { return requested.refused - requested.remembered; };
    auto before = counted();
    rebuild();
    auto after = counted();
    Require(after.made == before.made && tries(after) > tries(before), "an address-based build under a host heap report that leaves imports no room imported or refused nothing: " + std::to_string(after.made - before.made) + " made, " + std::to_string(tries(after) - tries(before)) + " refused");
    before = after;
    rebuild();
    after = counted();
    Require(tries(after) == tries(before) && after.remembered > before.remembered, "a second address-based build under a host heap report that leaves imports no room tried " + std::to_string(tries(after) - tries(before)) + " refused ranges again instead of answering them from the refusal memory");
    {
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 0; index < count; ++index) mutation.Remove(block + 2 * index * piece);
        if (added) mutation.Remove(other);
    }
    ClearHostImports(importing.device);
    ::operator delete(other, std::align_val_t{piece});
    ::operator delete(block, std::align_val_t{piece});
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

void partialHeapImportBudgetCase(const Context& context, const BdaTestAccess& access, std::size_t bytes, std::size_t budgetBytes, std::size_t prefixBytes) {
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) return;
    constexpr std::size_t window = 2 * 65536;
    const auto allocated = bytes + 65536;
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(allocated, 65536);
#ifdef _WIN32
    GuestArena::GuestArenaCommit_nid_postfix(block, allocated, PAGE_READWRITE, allocated);
#endif
    auto* guest = static_cast<std::uint8_t*>(block);
    std::memset(block, 0x39, bytes);
    std::memset(guest + bytes, 0xa7, allocated - bytes);
    const auto first = reinterpret_cast<std::uintptr_t>(block);
    auto importing = context;
    importing.hostImportAlignment = 65536;
    const auto live = HostImportLiveBytes();
    const auto before = MirrorCounters();
    SetHostImportRangeWindow(window);
    SetHostImportBudget(live + budgetBytes);
    const auto registry = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, block, bytes, true, true, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, block);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    registry(true);
    {
        auto withoutImports = context;
        withoutImports.hostImportAlignment = 0;
        GuestBufferMemory leased(withoutImports);
        leased.AcquireRegistered();
        leased.AddWritable(first + bytes - 16, 4);
        leased.Upload(true);
        std::uint32_t adjustment = 0;
        const auto view = leased.Descriptor(first + bytes - 16, 4, adjustment);
        access.bytes(view.buffer)[view.offset + adjustment] = std::byte{0x43};
        leased.WriteBack();
    }
    Require(MirrorCounters().heapBytes == before.heapBytes + bytes && guest[bytes - 16] == 0x43, "the partial-import fixture did not start with a valid full heap mirror");
    GuestAllocations::GuestAllocationsEnd_nid_postfix(GuestAllocations::GuestAllocationsBegin_nid_postfix());
    const auto build = [&](bool write) {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        if (write) {
            leased.AddWritable(first + 16, 32);
            leased.AddWritable(first + bytes - 48, 32);
        }
        leased.Upload(true);
        const auto* prefix = ExistingHostImportFor(importing, first, window);
        Require(prefix != nullptr, "a heap exceeding the import budget did not import its fitting prefix");
        Require(HostImportLiveBytes() <= live + budgetBytes, "partial heap imports exceeded the configured budget");
        Require(MirrorCounters().heapBytes == before.heapBytes + bytes - prefixBytes, "a partially imported heap mirrored bytes already imported in place");
        auto covered = first;
        for (const auto& entry : leased.AddressRanges()) {
            if (entry.end <= first || entry.begin >= first + bytes) continue;
            Require(entry.begin == covered && entry.end <= first + bytes, "partial heap BDA ranges have a gap or overlap");
            if (entry.end <= first + prefixBytes) {
                const auto* imported = ExistingHostImportFor(importing, entry.begin, static_cast<std::size_t>(entry.end - entry.begin));
                Require(imported != nullptr && imported->bytes <= window && entry.deviceAddress == imported->address + entry.begin - imported->base, "the partial heap prefix is not served by bounded imports");
            }
            if (entry.begin >= first + prefixBytes) {
                std::uint32_t adjustment = 0;
                const auto view = leased.Descriptor(entry.begin, static_cast<std::size_t>(entry.end - entry.begin), adjustment);
                const auto data = access.bytes(view.buffer).subspan(view.offset + adjustment);
                Require(data[0] == static_cast<std::byte>(guest[entry.begin - first]) && data[entry.end - entry.begin - 1] == static_cast<std::byte>(guest[entry.end - first - 1]), "a partial heap mirror lost its first or last bytes");
            }
            covered = entry.end;
        }
        Require(covered == first + bytes, "a partially imported heap does not cover its registered range");
        if (write) {
            for (const auto at : {first + 16, first + bytes - 48}) {
                if (at < first + prefixBytes) guest[at - first] = 0x71;
                else {
                    std::uint32_t adjustment = 0;
                    const auto view = leased.Descriptor(at, 32, adjustment);
                    access.bytes(view.buffer)[view.offset + adjustment] = std::byte{0x71};
                }
                guest[at - first + 8] = 0x52;
            }
        }
        leased.WriteBack();
    };
    build(true);
    Require(guest[16] == 0x71 && guest[24] == 0x52 && guest[bytes - 48] == 0x71 && guest[bytes - 40] == 0x52, "partial heap write-back lost GPU or genuine CPU stores");
    guest[bytes - 1] = 0x67;
    build(false);
    const auto quiet = MirrorCounters();
    guest[bytes + 9] = 0x8a;
    build(false);
    Require(MirrorCounters().blocksCopied == quiet.blocksCopied, "a neighboring CPU write refreshed an unrelated partial heap mirror");
    Require(guest[bytes + 9] == 0x8a && guest[bytes] == 0xa7 && guest[allocated - 1] == 0xa7, "partial heap uses changed unregistered neighboring memory");
    registry(false);
    {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.Upload(true);
        leased.WriteBack();
    }
    Require(MirrorCounters().heapBytes == before.heapBytes, "a partial heap mirror outlived its registered range");
    SetHostImportBudget(0);
    SetHostImportRangeWindow(std::uint64_t{64} << 20u);
    GuestArena::GuestArenaReset_nid_postfix(block, allocated);
    GuestArena::GuestArenaRelease_nid_postfix(block, allocated);
}

void partialHeapImportBudgetTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t window = 2 * 65536;
    partialHeapImportBudgetCase(context, access, 3 * window, 2 * window, 2 * window);
    partialHeapImportBudgetCase(context, access, 3 * window + 65536, 3 * window, 2 * window);
    SetHostImportMaxBytes(window);
    partialHeapImportBudgetCase(context, access, 3 * window, 2 * window, 2 * window);
    SetHostImportMaxBytes(0);
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

void retiredImportWatchTests(const Context& context) {
#ifdef _WIN32
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "guest arena unavailable or not write-watched: the write watch of retired imports not tested\n";
        return;
    }
    constexpr std::size_t piece = 65536;
    constexpr std::size_t bytes = 2 * piece;
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, bytes);
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
    std::memset(block, 0x5a, bytes);
    auto* guest = static_cast<std::uint8_t*>(block);
    const auto address = reinterpret_cast<std::uintptr_t>(block);
    auto importing = context;
    importing.hostImportAlignment = piece;
    SetHostImportWindow(bytes);
    SetHostImportDeferredSettles(false);
    ClearHostImports(importing.device);
    const auto decided = PrepareImportWatch(importing);
    SetImportWatch(importing, ImportWatch::Unwatch);
    const auto registry = [&](std::size_t offset, bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, guest + offset, piece, true, true, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, guest + offset);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    const auto build = [&] {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.Upload(true);
        leased.WriteBack();
    };
    registry(0, true);
    build();
    Require(HostImportCovers(importing, address, piece), "the first 64 KiB piece was not imported on its own");
    Require(!AgcDriver::GuestMemory::Watched(address, piece), "an unwatched 64 KiB import kept its piece in the write watch");
    registry(piece, true);
    SetHostImportBudget(4096);
    const auto before = MirrorCounters();
    build();
    Require(!HostImportCovers(importing, address, piece), "the piece's import outlived the run that grew past it, or a 128 KiB block fit a 4 KiB budget");
    Require(AgcDriver::GuestMemory::Watched(address, piece), "a 64 KiB piece whose import retired when its run grew into a block stayed outside the write watch");
    Require(MirrorCounters().heapMirrors == before.heapMirrors + 2, "a 64 KiB piece whose import retired when its run grew into a block was copied, not heap mirrored, once no import fit");
    registry(piece, false);
    registry(0, false);
    build();
    SetImportWatch(importing, decided);
    ClearHostImports(importing.device);
    SetHostImportBudget(0);
    SetHostImportWindow(0);
    SetHostImportDeferredSettles(true);
    GuestArena::GuestArenaReset_nid_postfix(block, bytes);
    GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
#else
    static_cast<void>(context);
#endif
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

void sparseImportedHeapTests(const Context& context, const BdaTestAccess& access) {
#ifndef _WIN32
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::cout << "write watch unavailable: sparse imported heaps not tested\n";
        return;
    }
    constexpr std::size_t bytes = 65536;
    void* raw = mmap(nullptr, 4 * bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(raw != MAP_FAILED, "cannot reserve the sparse imported heap fixture");
    const auto base = (reinterpret_cast<std::uintptr_t>(raw) + bytes - 1) & ~(bytes - 1);
    auto* guest = reinterpret_cast<std::uint8_t*>(base);
    Require(mprotect(guest, bytes, PROT_READ | PROT_WRITE) == 0, "cannot commit the sparse imported heap prefix");
    std::memset(guest, 0x57, bytes);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(guest, bytes);
    auto importing = context;
    importing.hostImportAlignment = bytes;
    auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
    GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, guest, bytes, true, true, true);
    GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    const auto* imported = HostImportFor(importing, base, bytes);
    Require(imported != nullptr, "the sparse heap prefix was not imported");
    const auto before = MirrorCounters();
    for (unsigned index = 0; index < 3; ++index) {
        {
            GuestBufferMemory described(importing);
            described.AddReadable(base, 2 * bytes);
            described.Upload(false);
            std::uint32_t adjustment = 0;
            const auto view = described.Descriptor(base, 2 * bytes, adjustment);
            Require(view.buffer == imported->buffer && view.range == bytes && adjustment == 0, "the sparse descriptor did not bind its imported prefix");
            Require(described.CopiedBytes() == 0 && MirrorCounters().rebuilds == before.rebuilds, "the sparse imported descriptor rebuilt or copied its heap mirror");
            reject([&] { static_cast<void>(described.Descriptor(base + bytes + 16, 4, adjustment)); }, "starts past its copy");
            described.WriteBack();
        }
        {
            GuestBufferMemory addressed(importing);
            addressed.AcquireRegistered();
            addressed.Upload(true);
            Require(MirrorCounters().rebuilds == before.rebuilds && MirrorCounters().heapMirrors == before.heapMirrors, "alternating descriptor and address builds rebuilt the imported heap mirror");
            addressed.WriteBack();
        }
    }
    {
        auto misaligned = importing;
        misaligned.limits.minStorageBufferOffsetAlignment = 256;
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Recorder recorder(misaligned);
        recorder.Activate();
        GuestBufferMemory memory(misaligned);
        memory.AddReadable(base + 16, 2 * bytes - 16);
        memory.Upload(false);
        std::uint32_t adjustment = 0;
        const auto view = memory.Descriptor(base + 16, 2 * bytes - 16, adjustment);
        Require(view.buffer != imported->buffer, "a misaligned sparse descriptor bound its import in place");
        Require(view.range != 0 && view.range <= bytes && memory.CopiedBytes() <= bytes && access.bytes(view.buffer)[view.offset + adjustment] == std::byte{0x57}, "a misaligned sparse descriptor does not serve its committed prefix alone");
        memory.WriteBack();
    }
    {
        GuestBufferMemory written(importing);
        written.AddWritable(base, 2 * bytes);
        written.Upload(false);
        std::uint32_t adjustment = 0;
        const auto view = written.Descriptor(base, 2 * bytes, adjustment);
        Require(view.buffer != imported->buffer && view.range == bytes && MirrorCounters().rebuilds == before.rebuilds + 1, "a sparse writable descriptor bound a shortened import");
        access.bytes(view.buffer)[view.offset] = std::byte{0x68};
        guest[8] = 0x79;
        written.WriteBack();
        Require(guest[0] == 0x68 && guest[8] == 0x79, "the sparse writable fallback lost a GPU store or rolled back a CPU store");
    }
    mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
    GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, guest);
    GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    Require(HostImportFor(importing, base, bytes) == nullptr, "the sparse heap import outlived its range");
    {
        GuestBufferMemory sweep(importing);
        sweep.AcquireRegistered();
        sweep.Upload(true);
        sweep.WriteBack();
    }
    Require(MirrorCounters().heapMirrors == before.heapMirrors, "the sparse imported heap fixture leaked its mirror");
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(guest, bytes);
    munmap(raw, 4 * bytes);
    std::cout << "Sparse imported heap tests passed\n";
#else
    static_cast<void>(context);
    static_cast<void>(access);
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

void spaceCommittedPagesTests(const Context& context, const BdaTestAccess& access) {
    if (!AddressSpaceCounters().enabled) return;
    constexpr std::size_t unit = 65536;
    constexpr std::size_t reserved = 3 * unit;
    auto* block = reserveSparse(reserved, unit);
    std::memset(block, 0x31, unit);
    const auto base = reinterpret_cast<std::uintptr_t>(block);
    auto copying = context;
    copying.hostImportAlignment = 0;
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, reserved, true, false, true);
    }
    const auto build = [&](std::byte expected, bool hit, const char* step) {
        const auto before = AddressSpaceCounters();
        GuestBufferMemory leased(copying);
        leased.AcquireRegistered();
        leased.AddReadable(base, 16);
        leased.AddReadable(base + 2 * unit, 16);
        leased.Upload(true);
        std::uint32_t adjustment = 0;
        const auto head = leased.Descriptor(base, 16, adjustment);
        const auto headBytes = access.bytes(head.buffer);
        Require(head.offset + adjustment < headBytes.size() && headBytes[head.offset + adjustment] == std::byte{0x31}, std::string("the committed head of a registered sparse range was not read: ") + step);
        const auto tail = leased.Descriptor(base + 2 * unit, 16, adjustment);
        const auto tailBytes = access.bytes(tail.buffer);
        Require(tail.offset + adjustment < tailBytes.size() && tailBytes[tail.offset + adjustment] == expected, std::string("a cached address space served another commit state of its pages: ") + step);
        leased.WriteBack();
        const auto after = AddressSpaceCounters();
        if (hit) Require(after.hits == before.hits + 1, std::string("the unchanged registry did not hit the cached address space: ") + step);
    };
    build(std::byte{}, false, "first build");
    build(std::byte{}, true, "hit before the commit");
    {
        GuestAllocations::Mutation mutation;
#ifdef _WIN32
        Require(VirtualAlloc(block + 2 * unit, unit, MEM_COMMIT, PAGE_READWRITE) != nullptr, "cannot commit the sparse range tail");
#else
        Require(mprotect(block + 2 * unit, unit, PROT_READ | PROT_WRITE) == 0, "cannot commit the sparse range tail");
#endif
        std::memset(block + 2 * unit, 0x72, unit);
    }
    build(std::byte{0x72}, false, "first build after the commit");
    build(std::byte{0x72}, true, "hit after the commit");
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    }
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
                    Buffer seed(context, copied, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
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

void RunHeapMirrorUnwatchTests(const Context& context, const BdaTestAccess& access, bool throughImport) {
    constexpr std::size_t block = 65536;
    constexpr std::size_t heapBytes = 32 * block;
    constexpr std::size_t idleBytes = 16 * block;
    constexpr std::size_t busyBytes = block;
#ifdef _WIN32
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "guest arena unavailable or not write-watched: heap mirrors leaving the write watch not tested\n";
        return;
    }
#else
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::cout << "write watch unavailable: heap mirrors leaving the write watch not tested\n";
        return;
    }
#endif
    struct Piece {
        void* pointer = nullptr;
        void* mapping = nullptr;
        std::size_t allocated = 0;
        std::size_t bytes = 0;
    };
    std::array<Piece, 3> pieces{};
    const std::array<std::pair<std::size_t, std::size_t>, 3> sizes{{{heapBytes + block, heapBytes}, {heapBytes + block, idleBytes}, {busyBytes + block, busyBytes}}};
    for (std::size_t index = 0; index < pieces.size(); ++index) {
        auto& piece = pieces[index];
        piece.allocated = sizes[index].first;
        piece.bytes = sizes[index].second;
#ifdef _WIN32
        piece.pointer = GuestArena::GuestArenaAllocate_nid_postfix(piece.allocated, block);
        Require(piece.pointer != nullptr, "cannot allocate the heap mirror ranges in the guest arena");
        GuestArena::GuestArenaCommit_nid_postfix(piece.pointer, piece.allocated, PAGE_READWRITE, piece.allocated);
        piece.mapping = piece.pointer;
#else
        piece.mapping = mmap(nullptr, piece.allocated + block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(piece.mapping != MAP_FAILED, "cannot map the heap mirror ranges");
        piece.pointer = reinterpret_cast<void*>((reinterpret_cast<std::uintptr_t>(piece.mapping) + block - 1) & ~std::uintptr_t{block - 1});
#endif
        std::memset(piece.pointer, 0x5a, piece.allocated);
    }
    if (pieces[1].pointer < pieces[0].pointer) {
        std::swap(pieces[0].pointer, pieces[1].pointer);
        std::swap(pieces[0].mapping, pieces[1].mapping);
    }
    const auto heap = reinterpret_cast<std::uintptr_t>(pieces[0].pointer);
    const auto idle = reinterpret_cast<std::uintptr_t>(pieces[1].pointer);
    const auto busy = reinterpret_cast<std::uintptr_t>(pieces[2].pointer);
#ifndef _WIN32
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(pieces[0].pointer, heapBytes);
#endif
    const auto registry = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        for (const auto& piece : pieces) {
            if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, piece.pointer, piece.bytes, true, false, true);
            else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, piece.pointer);
        }
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    registry(true);
#ifdef _WIN32
    AgcDriver::GuestMemory::Unwatch(idle, idleBytes);
    AgcDriver::GuestMemory::Unwatch(busy, busyBytes);
#endif
    auto importing = context;
    importing.hostImportAlignment = block;
    SetHostImportBudget(heapBytes + idleBytes + busyBytes - block);
    Require(HostImportFor(importing, idle, 64) != nullptr && HostImportFor(importing, busy, 64) != nullptr, "the idle and busy ranges were not imported");
    Require(HostImportLiveBytes() == idleBytes + busyBytes && !HostImportCovers(importing, heap, heapBytes), "the imports before the first build are not the idle and busy ranges");
    Require(AgcDriver::GuestMemory::Watched(heap, heapBytes), "the heap is not write-watched");
    const auto before = MirrorCounters();
    {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.Upload(true);
        Require(MirrorCounters().heapMirrors == before.heapMirrors + 1 && !HostImportCovers(importing, heap, heapBytes), "the first build did not mirror the heap");
        leased.WriteBack();
    }
    if (throughImport) {
        const auto decided = PrepareImportWatch(importing);
        SetImportWatch(importing, ImportWatch::Unwatch);
        SetHostImportBudget(2 * (heapBytes + idleBytes + busyBytes));
        Require(HostImportFor(importing, heap, heapBytes) != nullptr && HostImportCovers(importing, heap, heapBytes), "the heap was not imported once the budget held it");
        SetImportWatch(importing, decided);
    } else {
        AgcDriver::GuestMemory::Unwatch(heap + heapBytes / 2, block);
    }
    Require(!AgcDriver::GuestMemory::Watched(heap, heapBytes), "the heap is still write-watched");
    std::memset(reinterpret_cast<void*>(heap + heapBytes / 2), 0x6b, block);
    std::memset(reinterpret_cast<void*>(heap + block), 0x3c, block);
    SetAddressCopyLimit(heapBytes / 2);
    const auto refreshed = MirrorCounters();
    {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.Upload(true);
        const bool imported = HostImportCovers(importing, heap, heapBytes);
        Require(imported == throughImport, throughImport ? "the second build did not take the heap's import" : "the second build imported the heap past the budget");
        Require(MirrorCounters().heapMirrors == before.heapMirrors + (imported ? 0 : 1), imported ? "the heap mirror whose range the import took off the write watch was kept" : "the refused heap's mirror was dropped once its range left the write watch");
        Require(imported || MirrorCounters().blocksCopied >= refreshed.blocksCopied + heapBytes / block, "the refused heap's mirror was not refreshed in full");
        const auto ranges = leased.AddressRanges();
        const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == heap; });
        Require(found != ranges.end() && found->deviceAddress != 0u, "the heap is missing from the BDA table");
        Require(imported || (access.addressBytes(found->deviceAddress)[heapBytes / 2 + 3] == std::byte{0x6b} && access.addressBytes(found->deviceAddress)[block + 3] == std::byte{0x3c}), "the second build does not serve the heap's new bytes");
        leased.WriteBack();
    }
    if (!throughImport) {
        SetHostImportBudget(2 * (heapBytes + idleBytes + busyBytes));
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.Upload(true);
        Require(HostImportCovers(importing, heap, heapBytes) && MirrorCounters().heapMirrors == before.heapMirrors, "the heap was not imported again once the budget held it");
        leased.WriteBack();
    }
    SetAddressCopyLimit(0);
    SetHostImportBudget(0);
    registry(false);
    ClearImageMirrors(importing.device);
    ClearHostImports(importing.device);
    ClearSnapshotCopies(importing.device);
#ifndef _WIN32
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(pieces[0].pointer, heapBytes);
#endif
    for (const auto& piece : pieces) {
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(piece.pointer, piece.allocated);
        GuestArena::GuestArenaRelease_nid_postfix(piece.pointer, piece.allocated);
#else
        munmap(piece.mapping, piece.allocated + block);
#endif
    }
    std::cout << (throughImport ? "an address-based build plans a heap again once a host import took its mirror's range off the write watch\n" : "an address-based build keeps and refreshes a refused heap's mirror once its range has left the write watch, and imports the heap once the budget holds it\n");
}

void RunSparseHeapMirrorUnwatchTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t block = 65536;
    constexpr std::size_t reserved = 64 * block;
    constexpr std::size_t committed = 32 * block;
#ifdef _WIN32
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "guest arena unavailable or not write-watched: sparse heap mirrors leaving the write watch not tested\n";
        return;
    }
    auto* guest = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(reserved, block));
    Require(guest != nullptr, "cannot allocate the sparse heap in the guest arena");
    GuestArena::GuestArenaCommit_nid_postfix(guest, committed, PAGE_READWRITE, block);
#else
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::cout << "write watch unavailable: sparse heap mirrors leaving the write watch not tested\n";
        return;
    }
    auto* guest = reserveSparse(reserved, committed);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(guest, reserved);
#endif
    std::memset(guest, 0x5a, committed);
    const auto base = reinterpret_cast<std::uintptr_t>(guest);
    const auto registry = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, guest, reserved, true, false, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, guest);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    registry(true);
    Require(AgcDriver::GuestMemory::Watched(base, committed), "the sparse heap is not write-watched");
    const auto before = MirrorCounters();
    {
        GuestBufferMemory leased(context);
        leased.AcquireRegistered();
        leased.Upload(true);
        Require(MirrorCounters().heapMirrors == before.heapMirrors + 1, "the first build did not mirror the sparse heap");
        leased.WriteBack();
    }
    AgcDriver::GuestMemory::Unwatch(base + block, block);
    Require(!AgcDriver::GuestMemory::Watched(base, committed), "the sparse heap is still write-watched");
    std::memset(guest + block, 0x6b, block);
    std::memset(guest + 2 * block, 0x3c, block);
    SetAddressCopyLimit(committed / 2);
    const auto refreshed = MirrorCounters();
    {
        GuestBufferMemory leased(context);
        leased.AcquireRegistered();
        leased.Upload(true);
        Require(MirrorCounters().heapMirrors == before.heapMirrors + 1, "the sparse heap's mirror was dropped once its range left the write watch");
        Require(MirrorCounters().blocksCopied >= refreshed.blocksCopied + committed / block, "the sparse heap's mirror was not refreshed in full");
        const auto ranges = leased.AddressRanges();
        const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == base; });
        Require(found != ranges.end() && found->deviceAddress != 0u, "the sparse heap is missing from the BDA table");
        Require(access.addressBytes(found->deviceAddress)[block + 3] == std::byte{0x6b} && access.addressBytes(found->deviceAddress)[2 * block + 3] == std::byte{0x3c}, "the second build does not serve the sparse heap's new bytes");
        leased.WriteBack();
    }
    SetAddressCopyLimit(0);
    registry(false);
    ClearImageMirrors(context.device);
    ClearSnapshotCopies(context.device);
#ifdef _WIN32
    GuestArena::GuestArenaReset_nid_postfix(guest, reserved);
    GuestArena::GuestArenaRelease_nid_postfix(guest, reserved);
#else
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(guest, reserved);
    releaseSparse(guest, reserved);
#endif
    std::cout << "an address-based build keeps and refreshes a sparse heap's mirror once its range has left the write watch\n";
}

void RunSparseHeapMirrorBudgetTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t block = 65536;
    constexpr std::size_t reserved = 128 * block;
    constexpr std::size_t committed = 80 * block;
#ifdef _WIN32
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "guest arena unavailable or not write-watched: sparse heaps past the heap mirror budget not tested\n";
        return;
    }
    auto* guest = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(reserved, block));
    Require(guest != nullptr, "cannot allocate the sparse heap in the guest arena");
    GuestArena::GuestArenaCommit_nid_postfix(guest, committed, PAGE_READWRITE, block);
#else
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::cout << "write watch unavailable: sparse heaps past the heap mirror budget not tested\n";
        return;
    }
    auto* guest = reserveSparse(reserved, committed);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(guest, reserved);
#endif
    std::memset(guest, 0x5a, committed);
    const auto base = reinterpret_cast<std::uintptr_t>(guest);
    const auto registry = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, guest, reserved, true, true, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, guest);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    registry(true);
    const auto before = MirrorCounters();
    {
        GuestBufferMemory memory(context);
        memory.AddWritable(base, reserved);
        memory.Upload(false);
        std::uint32_t adjustment = 0;
        const auto view = memory.Descriptor(base, reserved, adjustment);
        const auto bytes = access.bytes(view.buffer);
        Require(adjustment == 0 && view.range >= committed && bytes[view.offset + committed - 1] == std::byte{0x5a}, "a sparse heap past the heap mirror budget was not copied up to its last committed byte");
        bytes[view.offset + 8] = std::byte{0x77};
        memory.WriteBack();
    }
    Require(guest[8] == std::byte{0x77}, "a store to a sparse heap copied past the heap mirror budget was not written back");
    const auto after = MirrorCounters();
    Require(after.sparseBinds == before.sparseBinds && after.heapMirrors == before.heapMirrors && after.heapBytes == before.heapBytes, "a sparse heap past the heap mirror budget was mirrored or left heap mirror bytes held");
    registry(false);
    ClearImageMirrors(context.device);
    ClearSnapshotCopies(context.device);
#ifdef _WIN32
    GuestArena::GuestArenaReset_nid_postfix(guest, reserved);
    GuestArena::GuestArenaRelease_nid_postfix(guest, reserved);
#else
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(guest, reserved);
    releaseSparse(guest, reserved);
#endif
    std::cout << "a sparse heap whose mirror would pass the heap mirror budget is copied\n";
}

void RunHeapMirrorRunUnwatchTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t block = 65536;
    constexpr std::size_t heapBytes = 32 * block;
    constexpr std::size_t allocated = 2 * heapBytes + block;
#ifdef _WIN32
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "guest arena unavailable or not write-watched: a heap run leaving the write watch not tested\n";
        return;
    }
    auto* guest = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(allocated, block));
    Require(guest != nullptr, "cannot allocate the heap run in the guest arena");
    GuestArena::GuestArenaCommit_nid_postfix(guest, allocated, PAGE_READWRITE, allocated);
#else
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::cout << "write watch unavailable: a heap run leaving the write watch not tested\n";
        return;
    }
    void* mapping = mmap(nullptr, allocated + block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED, "cannot map the heap run");
    auto* guest = reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(mapping) + block - 1) & ~std::uintptr_t{block - 1});
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(guest, 2 * heapBytes);
#endif
    std::memset(guest, 0x5a, allocated);
    const auto first = reinterpret_cast<std::uintptr_t>(guest);
    const auto second = first + heapBytes;
    const auto registry = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        for (auto* piece : {guest, guest + heapBytes}) {
            if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, piece, heapBytes, true, false, true);
            else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, piece);
        }
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    registry(true);
    Require(AgcDriver::GuestMemory::Watched(first, 2 * heapBytes), "the heap run is not write-watched");
    const auto before = MirrorCounters();
    {
        GuestBufferMemory leased(context);
        leased.AcquireRegistered();
        leased.Upload(true);
        Require(MirrorCounters().heapMirrors == before.heapMirrors + 2, "the first build did not mirror both heaps of the run");
        leased.WriteBack();
    }
    AgcDriver::GuestMemory::Unwatch(first + block, block);
    Require(!AgcDriver::GuestMemory::Watched(first, heapBytes) && AgcDriver::GuestMemory::Watched(second, heapBytes), "only the first heap of the run should have left the write watch");
    std::memset(guest + block, 0x6b, block);
    std::memset(guest + heapBytes + 2 * block, 0x3c, block);
    {
        GuestBufferMemory leased(context);
        leased.AcquireRegistered();
        leased.Upload(true);
        Require(MirrorCounters().heapMirrors == before.heapMirrors + 2, "a heap mirror of the run was dropped once part of the run left the write watch");
        const auto ranges = leased.AddressRanges();
        const auto served = [&](std::uintptr_t base) {
            const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == base; });
            Require(found != ranges.end() && found->deviceAddress != 0u, "a heap of the run is missing from the BDA table");
            return access.addressBytes(found->deviceAddress);
        };
        Require(served(first)[block + 3] == std::byte{0x6b}, "the unwatched heap of the run does not serve its new bytes");
        Require(served(second)[2 * block + 3] == std::byte{0x3c}, "the watched heap beside an unwatched one missed the CPU's write");
        leased.WriteBack();
    }
    registry(false);
    ClearImageMirrors(context.device);
    ClearSnapshotCopies(context.device);
#ifdef _WIN32
    GuestArena::GuestArenaReset_nid_postfix(guest, allocated);
    GuestArena::GuestArenaRelease_nid_postfix(guest, allocated);
#else
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(guest, 2 * heapBytes);
    munmap(mapping, allocated + block);
#endif
    std::cout << "an address-based build refreshes the unwatched heap of a run in full and its watched neighbour by its writes\n";
}

void RunAddressCopyCapTests(const Context& context, const BdaTestAccess& access, bool remembered, int frames) {
    constexpr std::size_t block = 65536;
    constexpr std::size_t heapBytes = 32 * block;
    constexpr std::size_t idleBytes = 16 * block;
    constexpr std::size_t busyBytes = block;
#ifdef _WIN32
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "guest arena unavailable or not write-watched: the address copy cap not tested\n";
        return;
    }
#else
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::cout << "write watch unavailable: the address copy cap not tested\n";
        return;
    }
#endif
    struct Piece {
        void* pointer = nullptr;
        void* mapping = nullptr;
        std::size_t allocated = 0;
        std::size_t bytes = 0;
    };
    std::array<Piece, 4> pieces{};
    const std::array<std::pair<std::size_t, std::size_t>, 4> sizes{{{heapBytes + block, heapBytes}, {heapBytes + block, idleBytes}, {busyBytes + block, busyBytes}, {busyBytes + block, busyBytes}}};
    for (std::size_t index = 0; index < pieces.size(); ++index) {
        auto& piece = pieces[index];
        piece.allocated = sizes[index].first;
        piece.bytes = sizes[index].second;
#ifdef _WIN32
        piece.pointer = GuestArena::GuestArenaAllocate_nid_postfix(piece.allocated, block);
        Require(piece.pointer != nullptr, "cannot allocate the address copy cap ranges in the guest arena");
        GuestArena::GuestArenaCommit_nid_postfix(piece.pointer, piece.allocated, PAGE_READWRITE, piece.allocated);
        piece.mapping = piece.pointer;
#else
        piece.mapping = mmap(nullptr, piece.allocated + block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(piece.mapping != MAP_FAILED, "cannot map the address copy cap ranges");
        piece.pointer = reinterpret_cast<void*>((reinterpret_cast<std::uintptr_t>(piece.mapping) + block - 1) & ~std::uintptr_t{block - 1});
#endif
        std::memset(piece.pointer, 0x5a, piece.allocated);
    }
    if (pieces[1].pointer < pieces[0].pointer) {
        std::swap(pieces[0].pointer, pieces[1].pointer);
        std::swap(pieces[0].mapping, pieces[1].mapping);
    }
    const auto heap = reinterpret_cast<std::uintptr_t>(pieces[0].pointer);
    const auto idle = reinterpret_cast<std::uintptr_t>(pieces[1].pointer);
    const auto busy = reinterpret_cast<std::uintptr_t>(pieces[2].pointer);
    Require(heap < idle, "the idle range does not follow the heap in the registry");
#ifndef _WIN32
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(pieces[0].pointer, heapBytes);
#endif
    const auto registry = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        for (std::size_t index = 0; index < 3; ++index) {
            if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, pieces[index].pointer, pieces[index].bytes, true, false, true);
            else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, pieces[index].pointer);
        }
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    const auto unrelated = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, pieces[3].pointer, pieces[3].bytes, true, false, false);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, pieces[3].pointer);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    registry(true);
#ifdef _WIN32
    AgcDriver::GuestMemory::Unwatch(idle, idleBytes);
    AgcDriver::GuestMemory::Unwatch(busy, busyBytes);
#endif
    auto importing = context;
    importing.hostImportAlignment = block;
    SetHostImportBudget(heapBytes + idleBytes + busyBytes - block);
    Require(HostImportFor(importing, idle, 64) != nullptr && HostImportFor(importing, busy, 64) != nullptr, "the idle and busy ranges were not imported");
    Require(HostImportLiveBytes() == idleBytes + busyBytes && !HostImportCovers(importing, heap, heapBytes), "the imports before the build are not the idle and busy ranges");
    Require(AgcDriver::GuestMemory::Watched(heap, heapBytes), "the heap is not write-watched");
    const auto before = MirrorCounters();
    const auto builds = [] { return HostImportCounters().requesters[static_cast<std::size_t>(ImportRequester::Build)]; };
    const auto unwatch = [&] {
        AgcDriver::GuestMemory::Unwatch(heap + heapBytes / 2, block);
        Require(AgcDriver::GuestMemory::Watched(heap, heapBytes / 2) && !AgcDriver::GuestMemory::Watched(heap, heapBytes), "the heap is not partly outside the write watch");
    };
    if (remembered) {
        unwatch();
        SetAddressCopyLimit(2 * heapBytes);
        {
            GuestBufferMemory leased(importing);
            leased.AcquireRegistered();
            leased.Upload(true);
            Require(MirrorCounters().heapMirrors == before.heapMirrors && !HostImportCovers(importing, heap, heapBytes), "the first build did not copy the heap whose import the budget refused");
            leased.WriteBack();
        }
        SetAddressCopyLimit(0);
        unrelated(true);
    }
    for (int frame = 0; frame < frames; ++frame) NoteHostImportFrame();
    for (int use = 0; use < 300; ++use) Require(HostImportFor(importing, busy, 64) != nullptr, "the busy range lost its import");
    if (!remembered) unwatch();
    const auto refused = builds();
    {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.Upload(true);
        const bool imported = HostImportCovers(importing, heap, heapBytes);
        Require(imported || (!remembered && MirrorCounters().heapMirrors == before.heapMirrors + 1), "the heap is neither imported nor mirrored");
        const auto ranges = leased.AddressRanges();
        const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == heap; });
        Require(found != ranges.end() && found->deviceAddress != 0u, "the heap is missing from the BDA table");
        Require(imported || access.addressBytes(found->deviceAddress)[heapBytes / 2 + 3] == std::byte{0x5a}, "the heap mirror does not hold the guest bytes");
        leased.WriteBack();
    }
    Require(!remembered || builds().remembered == refused.remembered + 1, "the build under the copy cap was not answered from the refusal the first build remembered");
    SetHostImportBudget(0);
    if (remembered) unrelated(false);
    registry(false);
    ClearImageMirrors(importing.device);
    ClearHostImports(importing.device);
    ClearSnapshotCopies(importing.device);
#ifndef _WIN32
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(pieces[0].pointer, heapBytes);
#endif
    for (const auto& piece : pieces) {
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(piece.pointer, piece.allocated);
        GuestArena::GuestArenaRelease_nid_postfix(piece.pointer, piece.allocated);
#else
        munmap(piece.mapping, piece.allocated + block);
#endif
    }
    std::cout << (remembered ? "an address-based build answered from a remembered budget refusal evicts instead of copying a partly unwatched heap past the cap\n" : "an address-based build serves a partly unwatched heap whose import is over the budget\n");
}

void RunAddressCopyCapMirrorTests(const Context& context, const BdaTestAccess& access, int frames) {
    constexpr std::size_t block = 65536;
    constexpr std::size_t unwatchedBytes = 32 * block;
    constexpr std::size_t mirroredBytes = 16 * block;
#ifdef _WIN32
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "guest arena unavailable or not write-watched: imports yielding to the address copy cap not tested\n";
        return;
    }
#else
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::cout << "write watch unavailable: imports yielding to the address copy cap not tested\n";
        return;
    }
#endif
    struct Piece {
        void* pointer = nullptr;
        void* mapping = nullptr;
        std::size_t allocated = 0;
        std::size_t bytes = 0;
    };
    std::array<Piece, 2> pieces{};
    for (auto& piece : pieces) {
        piece.allocated = unwatchedBytes + block;
#ifdef _WIN32
        piece.pointer = GuestArena::GuestArenaAllocate_nid_postfix(piece.allocated, block);
        Require(piece.pointer != nullptr, "cannot allocate the yielding import ranges in the guest arena");
        GuestArena::GuestArenaCommit_nid_postfix(piece.pointer, piece.allocated, PAGE_READWRITE, piece.allocated);
        piece.mapping = piece.pointer;
#else
        piece.mapping = mmap(nullptr, piece.allocated + block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(piece.mapping != MAP_FAILED, "cannot map the yielding import ranges");
        piece.pointer = reinterpret_cast<void*>((reinterpret_cast<std::uintptr_t>(piece.mapping) + block - 1) & ~std::uintptr_t{block - 1});
#endif
        std::memset(piece.pointer, 0x5a, piece.allocated);
    }
    if (pieces[1].pointer < pieces[0].pointer) {
        std::swap(pieces[0].pointer, pieces[1].pointer);
        std::swap(pieces[0].mapping, pieces[1].mapping);
        std::swap(pieces[0].allocated, pieces[1].allocated);
    }
    pieces[0].bytes = unwatchedBytes;
    pieces[1].bytes = mirroredBytes;
    const auto unwatched = reinterpret_cast<std::uintptr_t>(pieces[0].pointer);
    const auto mirrored = reinterpret_cast<std::uintptr_t>(pieces[1].pointer);
#ifndef _WIN32
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(pieces[1].pointer, mirroredBytes);
#endif
    {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        for (const auto& piece : pieces) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, piece.pointer, piece.bytes, true, false, true);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    }
#ifdef _WIN32
    AgcDriver::GuestMemory::Unwatch(unwatched, unwatchedBytes);
#endif
    Require(!AgcDriver::GuestMemory::Watched(unwatched, unwatchedBytes) && AgcDriver::GuestMemory::Watched(mirrored, mirroredBytes), "the first range is not outside the write watch or the second is not inside it");
    auto importing = context;
    importing.hostImportAlignment = block;
    SetHostImportBudget(unwatchedBytes + block);
    for (int frame = 0; frame < frames; ++frame) NoteHostImportFrame();
    for (int use = 0; use < 300; ++use) Require(HostImportFor(importing, mirrored, 64) != nullptr, "the write-watched range was not imported");
    Require(HostImportLiveBytes() == mirroredBytes, "the import before the build is not the write-watched range");
    const auto before = MirrorCounters();
    {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.Upload(true);
        Require(HostImportCovers(importing, unwatched, unwatchedBytes), "an address-based build did not import a range that only an import serves in place of a recently used import a mirror can stand in for");
        Require(!HostImportCovers(importing, mirrored, mirroredBytes) && MirrorCounters().heapMirrors == before.heapMirrors + 1, "the write-watched range whose import yielded is not mirrored");
        const auto ranges = leased.AddressRanges();
        const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == mirrored; });
        Require(found != ranges.end() && found->deviceAddress != 0u, "the write-watched range is missing from the BDA table");
        Require(access.addressBytes(found->deviceAddress)[mirroredBytes / 2 + 3] == std::byte{0x5a}, "the mirror of the write-watched range does not hold the guest bytes");
        leased.WriteBack();
    }
    SetHostImportBudget(0);
    {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        for (const auto& piece : pieces) GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, piece.pointer);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    }
    ClearImageMirrors(importing.device);
    ClearHostImports(importing.device);
    ClearSnapshotCopies(importing.device);
#ifndef _WIN32
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(pieces[1].pointer, mirroredBytes);
#endif
    for (const auto& piece : pieces) {
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(piece.pointer, piece.allocated);
        GuestArena::GuestArenaRelease_nid_postfix(piece.pointer, piece.allocated);
#else
        munmap(piece.mapping, piece.allocated + block);
#endif
    }
    std::cout << "an address-based build past the copy cap takes a range only an import serves from a recently used import a mirror stands in for\n";
}

void RunUnwatchedMirrorImportTests(const Context& context, const BdaTestAccess& access, int frames) {
    constexpr std::size_t block = 65536;
    constexpr std::size_t rangeBytes = 16 * block;
#ifdef _WIN32
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "guest arena unavailable or not write-watched: imports of heaps whose mirror left the write watch not tested\n";
        return;
    }
#else
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::cout << "write watch unavailable: imports of heaps whose mirror left the write watch not tested\n";
        return;
    }
#endif
    struct Piece {
        void* pointer = nullptr;
        void* mapping = nullptr;
        std::size_t allocated = rangeBytes + block;
    };
    std::array<Piece, 2> pieces{};
    for (auto& piece : pieces) {
#ifdef _WIN32
        piece.pointer = GuestArena::GuestArenaAllocate_nid_postfix(piece.allocated, block);
        Require(piece.pointer != nullptr, "cannot allocate the mirrored heap ranges in the guest arena");
        GuestArena::GuestArenaCommit_nid_postfix(piece.pointer, piece.allocated, PAGE_READWRITE, piece.allocated);
        piece.mapping = piece.pointer;
#else
        piece.mapping = mmap(nullptr, piece.allocated + block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(piece.mapping != MAP_FAILED, "cannot map the mirrored heap ranges");
        piece.pointer = reinterpret_cast<void*>((reinterpret_cast<std::uintptr_t>(piece.mapping) + block - 1) & ~std::uintptr_t{block - 1});
#endif
        std::memset(piece.pointer, 0x5a, piece.allocated);
#ifndef _WIN32
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(piece.pointer, rangeBytes);
#endif
    }
    const auto leaving = reinterpret_cast<std::uintptr_t>(pieces[0].pointer);
    const auto staying = reinterpret_cast<std::uintptr_t>(pieces[1].pointer);
    {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        for (const auto& piece : pieces) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, piece.pointer, rangeBytes, true, false, true);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    }
    Require(AgcDriver::GuestMemory::Watched(leaving, rangeBytes) && AgcDriver::GuestMemory::Watched(staying, rangeBytes), "the heap ranges are not write-watched");
    auto importing = context;
    importing.hostImportAlignment = block;
    SetHostImportBudget(rangeBytes + block);
    for (int frame = 0; frame < frames; ++frame) NoteHostImportFrame();
    for (int use = 0; use < 300; ++use) Require(HostImportFor(importing, staying, 64) != nullptr, "the staying range was not imported");
    const auto build = [&](const char* what) {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        leased.Upload(true);
        const auto ranges = leased.AddressRanges();
        for (const auto address : {leaving, staying}) {
            const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == address; });
            Require(found != ranges.end() && found->deviceAddress != 0u, std::string("a heap range is missing from the BDA table ") + what);
            Require(HostImportCovers(importing, address, rangeBytes) || access.addressBytes(found->deviceAddress)[rangeBytes / 2 + 3] == std::byte{0x5a}, std::string("the mirror of a heap range does not hold its guest bytes ") + what);
        }
        leased.WriteBack();
    };
    build("before the write watch left");
    Require(!HostImportCovers(importing, leaving, rangeBytes) && HostImportCovers(importing, staying, rangeBytes), "the first build did not mirror the heap the budget refused beside the imported one");
    AgcDriver::GuestMemory::Unwatch(leaving, rangeBytes);
    Require(!AgcDriver::GuestMemory::Watched(leaving, rangeBytes), "the mirrored heap did not leave the write watch");
    build("after the write watch left");
    Require(HostImportCovers(importing, leaving, rangeBytes), "a heap whose mirror left the write watch was not imported in place of an import a mirror can replace");
    Require(!HostImportCovers(importing, staying, rangeBytes), "the write-watched heap kept its import beside the heap that left the write watch");
    SetHostImportBudget(0);
    {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        for (const auto& piece : pieces) GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, piece.pointer);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    }
    ClearImageMirrors(importing.device);
    ClearHostImports(importing.device);
    ClearSnapshotCopies(importing.device);
#ifndef _WIN32
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(pieces[1].pointer, rangeBytes);
#endif
    for (const auto& piece : pieces) {
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(piece.pointer, piece.allocated);
        GuestArena::GuestArenaRelease_nid_postfix(piece.pointer, piece.allocated);
#else
        munmap(piece.mapping, piece.allocated + block);
#endif
    }
    std::cout << "a heap whose mirror left the write watch is imported in place of an import a mirror can replace\n";
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

void sparseJoinedHeapMirrorTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t half = 1u << 20u;
    constexpr std::size_t reserved = 2 * half;
    constexpr std::size_t page = 65536;
#ifdef _WIN32
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "guest arena unavailable or not write-watched: sparse heap mirrors over adjacent ranges not tested\n";
        return;
    }
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(reserved, page);
    auto* guest = static_cast<std::uint8_t*>(block);
    GuestArena::GuestArenaCommit_nid_postfix(guest, page, PAGE_READWRITE, page);
    GuestArena::GuestArenaCommit_nid_postfix(guest + half, page, PAGE_READWRITE, page);
#else
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::cout << "write watch unavailable: sparse heap mirrors over adjacent ranges not tested\n";
        return;
    }
    void* mapping = mmap(nullptr, reserved + page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED, "cannot reserve the sparse heap over adjacent ranges");
    auto* guest = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(mapping) + page - 1) & ~std::uintptr_t{page - 1});
    Require(mprotect(guest, page, PROT_READ | PROT_WRITE) == 0 && mprotect(guest + half, page, PROT_READ | PROT_WRITE) == 0, "cannot commit the sparse heap pages");
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(guest, reserved);
    void* block = guest;
#endif
    auto* second = guest + half;
    std::memset(guest, 0x5a, page);
    std::memset(second, 0x6b, page);
    const auto base = reinterpret_cast<std::uintptr_t>(block);
    const auto registry = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) {
            GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, guest, half, true, true, true);
            GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, second, half, true, true, true);
        } else {
            GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, guest);
            GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, second);
        }
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    const auto sweep = [&] {
        GuestBufferMemory memory(context);
        memory.AcquireRegistered();
        memory.Upload(true);
        memory.WriteBack();
    };
    sweep();
    registry(true);
    const auto bind = [&] {
        GuestBufferMemory memory(context);
        memory.AddWritable(base, reserved);
        memory.Upload(false);
        std::uint32_t adjustment = 0;
        const auto view = memory.Descriptor(base, reserved, adjustment);
        Require(adjustment == 0, "a sparse heap view over adjacent ranges is adjusted");
        const auto bytes = access.bytes(view.buffer);
        Require(view.range >= half + page && bytes[view.offset + page - 1] == std::byte{0x5a} && bytes[view.offset + page] == std::byte{} && bytes[view.offset + half] == std::byte{0x6b}, "a sparse heap over adjacent ranges is not bound to its committed extent");
        bytes[view.offset + half + 8] = std::byte{0x77};
        memory.WriteBack();
        return view;
    };
    const auto before = MirrorCounters();
    const auto first = bind();
    const auto made = MirrorCounters();
    Require(made.sparseBinds == before.sparseBinds + 1 && made.heapMirrors == before.heapMirrors + 1, "a sparse heap whose committed span crosses an adjacent range was not mirrored");
    Require(second[8] == 0x77, "a store to the adjacent range of a mirrored sparse heap was not written back");
    second[8] = 0x6b;
    const auto again = bind();
    Require(again.buffer == first.buffer && MirrorCounters().rebuilds == made.rebuilds && MirrorCounters().sparseBinds == made.sparseBinds + 1, "a sparse heap over unchanged adjacent ranges did not reuse its mirror");
    registry(false);
    sweep();
    Require(MirrorCounters().heapMirrors == before.heapMirrors && MirrorCounters().heapBytes == before.heapBytes, "the sparse heap mirror over adjacent ranges outlived its ranges");
#ifdef _WIN32
    GuestArena::GuestArenaReset_nid_postfix(block, reserved);
    GuestArena::GuestArenaRelease_nid_postfix(block, reserved);
#else
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(guest, reserved);
    munmap(mapping, reserved + page);
#endif
}

void joinedHeapMirrorPartialImportTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t half = 1u << 20u;
    constexpr std::size_t committed = 2 * half;
    constexpr std::size_t reserved = 4 * half;
    constexpr std::size_t page = 65536;
#ifdef _WIN32
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "guest arena unavailable or not write-watched: joined heap mirrors under a partial import not tested\n";
        return;
    }
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(reserved, half);
    auto* guest = static_cast<std::uint8_t*>(block);
    GuestArena::GuestArenaCommit_nid_postfix(guest, committed, PAGE_READWRITE, page);
#else
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::cout << "write watch unavailable: joined heap mirrors under a partial import not tested\n";
        return;
    }
    void* mapping = mmap(nullptr, reserved + half, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED, "cannot reserve the joined heap");
    auto* guest = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(mapping) + half - 1) & ~std::uintptr_t{half - 1});
    Require(mprotect(guest, committed, PROT_READ | PROT_WRITE) == 0, "cannot commit the joined heap ranges");
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(guest, reserved);
    void* block = guest;
#endif
    std::memset(guest, 0x3c, committed);
    const auto base = reinterpret_cast<std::uintptr_t>(block);
    const auto registry = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) {
            GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, guest, half, true, true, true);
            GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, guest + half, half, true, true, true);
        } else {
            GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, guest);
            GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, guest + half);
        }
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    const auto sweep = [&](const Context& with) {
        GuestBufferMemory memory(with);
        memory.AcquireRegistered();
        memory.Upload(true);
        memory.WriteBack();
    };
    const auto bind = [&] {
        GuestBufferMemory memory(context);
        memory.AddWritable(base, reserved);
        memory.Upload(false);
        std::uint32_t adjustment = 0;
        const auto view = memory.Descriptor(base, reserved, adjustment);
        Require(adjustment == 0 && access.bytes(view.buffer)[view.offset + half + 32] == std::byte{0x3c}, "a heap bound across two committed ranges did not read its second range");
        memory.WriteBack();
        return view;
    };
    sweep(context);
    registry(true);
    const auto before = MirrorCounters();
    const auto first = bind();
    const auto made = MirrorCounters();
    Require(made.heapMirrors == before.heapMirrors + 1 && made.sparseBinds == before.sparseBinds + 1, "a heap bound across two committed ranges was not mirrored over both");
    auto importing = context;
    importing.hostImportAlignment = page;
    SetHostImportWindow(half);
    SetHostImportRangeWindow(half);
    SetHostImportBudget(HostImportLiveBytes() + half);
    {
        GuestBufferMemory memory(importing);
        memory.AcquireRegistered();
        Require(ExistingHostImportFor(importing, base, half) != nullptr && ExistingHostImportFor(importing, base + half, half) == nullptr, "the joined heap partial-import fixture did not import exactly its first range");
        Require(HeapMirrorsWithin(base, base + half).mirrors == 1, "an address build that imported one range of a joined heap mirror released the mirror");
        memory.Upload(true);
        memory.WriteBack();
    }
    const auto partial = MirrorCounters();
    const auto again = bind();
    Require(again.buffer == first.buffer && MirrorCounters().rebuilds == partial.rebuilds, "a joined heap mirror was rebuilt after an import of one of its ranges");
    {
        GuestBufferMemory memory(importing);
        memory.AcquireRegistered();
        memory.AddWritable(base + half, 32);
        memory.Upload(true);
        if (ExistingHostImportFor(importing, base + half, 32) != nullptr) guest[half] = 0x71;
        else {
            std::uint32_t adjustment = 0;
            const auto view = memory.Descriptor(base + half, 32, adjustment);
            access.bytes(view.buffer)[view.offset + adjustment] = std::byte{0x71};
        }
        guest[half + 8] = 0x52;
        memory.WriteBack();
    }
    Require(guest[half] == 0x71 && guest[half + 8] == 0x52, "a joined heap address build lost GPU or genuine CPU stores");
    const auto refreshed = bind();
    Require(refreshed.buffer == first.buffer && access.bytes(refreshed.buffer)[refreshed.offset + half] == std::byte{0x71} && access.bytes(refreshed.buffer)[refreshed.offset + half + 8] == std::byte{0x52}, "the retained joined mirror did not refresh stores from an address build");
    SetHostImportBudget(HostImportLiveBytes() + committed);
    GuestAllocations::GuestAllocationsEnd_nid_postfix(GuestAllocations::GuestAllocationsBegin_nid_postfix());
    sweep(importing);
    Require(ExistingHostImportFor(importing, base, half) != nullptr && ExistingHostImportFor(importing, base + half, half) != nullptr, "the joined heap full-import fixture did not import both ranges");
    Require(HeapMirrorsWithin(base, base + committed).mirrors == 0, "imports covering every joined heap range left a heap mirror allocated");
    SetHostImportBudget(0);
    SetHostImportWindow(0);
    SetHostImportRangeWindow(std::uint64_t{64} << 20u);
    ClearHostImports(context.device);
    registry(false);
    sweep(context);
#ifdef _WIN32
    GuestArena::GuestArenaReset_nid_postfix(block, reserved);
    GuestArena::GuestArenaRelease_nid_postfix(block, reserved);
#else
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(guest, reserved);
    munmap(mapping, reserved + half);
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
        Require(ShaderResources::NeverReusable(std::span<const CompiledShader>(&compiled, 1)) && !resources.Reusable(), "an address-based build is reusable");
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
        const auto fail = [&](ShaderRecompiler::BdaAbi::FaultReason reason, std::uint32_t word, std::uint32_t value, std::uint32_t otherFaults = 0) {
            ShaderResources resources(context, compiled);
            std::memcpy(access.bytes(access.descriptor(6).buffer).data() + word * sizeof(std::uint32_t), &value, sizeof(value));
            const ShaderRecompiler::BdaAbi::Fault report{ShaderRecompiler::BdaAbi::FaultState::Ready, reason, 0x7fff99880002ULL, 4, 0, 0x5c8, otherFaults};
            std::memcpy(access.bytes(access.descriptor(5).buffer).data(), &report, sizeof(report));
            try {
                resources.WriteBack();
            } catch (const BdaUnalignedAccess& error) {
                Require(std::string(error.what()).find("BDA access failed") != std::string::npos, "an unaligned BDA access did not report the failed access");
                return true;
            } catch (const std::runtime_error& error) {
                Require(std::string(error.what()).find("BDA access failed") != std::string::npos, "a BDA fault did not report the failed access");
                return false;
            }
            throw std::runtime_error("a recorded BDA fault was not reported");
        };
        Require(fail(ShaderRecompiler::BdaAbi::FaultReason::Unaligned, 2, 777) && guest[2] == 777, "an unaligned BDA access lost the stores of the dispatch's other invocations");
        Require(!fail(ShaderRecompiler::BdaAbi::FaultReason::Unmapped, 3, 888) && guest[3] == 0, "a failed GPU command published writes");
        Require(!fail(ShaderRecompiler::BdaAbi::FaultReason::Unaligned, 5, 999, 1) && guest[5] == 0, "an unaligned BDA access that hid a fault of another kind published writes");
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
    importSpanTests(context);
    importRunGrowthTests(context);
    importRunReplacedTests(context);
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
    importCoverageHintBudgetTests(context, access);
    failedImportJoinProbeBudgetTests(context, access);
    failedJoinWrittenViewTests(context, access);
    importWholeRangeOverWindowTests(context);
    importMappedFailureGenerationTests(context);
    importStaleProbeTests(context, access);
    importRefusedWholeRangeTests(context);
    importPiecesOverAllocationLimitTests(context);
    importNeighbouringEdgesTests(context);
    importAtMapOverBudgetTests(context);
    importRequesterTests(context, access);
    hostImportReportTests();
    importBlockMapTests(context, false);
#ifdef __linux__
    importBlockMapTests(context, true);
#endif
    importBlockBuildTests(context);
    importBlockHoleTests(context);
    importBlockUnalignedTests(context);
    importBlockFlagTests(context);
    importBlockRemapTests(context);
    importBlockFailureTests(context, access);
    importRunFailureShrinkTests(context, access);
    importBlockBudgetTests(context);
    importRefusalMemoryTests(context);
    evictingRefusalMemoryTests(context);
    evictingRefusalAgingTests(context);
    evictingRefusalRangeTests(context);
    evictingRefusalWindowTests(context);
    evictingRefusalBlockRangesTests(context);
    evictingRefusalJoinTests(context);
    evictingRefusalStepDownTests(context);
    importDeviceBudgetTests();
    importCeilingTests(context, access, false);
    importCeilingTests(context, access, true);
    for (const auto failure : {VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY, VK_ERROR_INVALID_EXTERNAL_HANDLE, VK_ERROR_UNKNOWN}) {
        importAllocationErrorTests(context, failure, false);
        importAllocationErrorTests(context, failure, true);
    }
    for (const auto failure : {VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY, VK_ERROR_INVALID_EXTERNAL_HANDLE, VK_ERROR_UNKNOWN}) {
        importReportFloorTests(context, true, failure);
        importReportFloorTests(context, false, failure);
    }
    importReportRefusalMemoryTests(context, access);
    sparseImportedHeapTests(context, access);
    importEvictionFramesTests(context);
    heapMirrorTests(context, access);
    partialHeapImportBudgetTests(context, access);
    importedHeapMirrorTests(context, access);
    retiredImportWatchTests(context);
    importWatchTests();
    importedFreshTests(context);
    gpuMappingTests(context);
    stridedOverhangTests(context, access);
    sparseHeapTests(context, access);
    spaceCommittedPagesTests(context, access);
    hostWriteWatchExclusionTests();
    sparseHeapMirrorTests(context, access);
    sparseJoinedHeapMirrorTests(context, access);
    joinedHeapMirrorPartialImportTests(context, access);
    Require(AddressCopyOverflow({{0x1000, 0x3000, 0x2000, "uncommitted pages"}}, 0x2000).empty(), "copies within the limit were refused");
    const auto copies = AddressCopyOverflow({{0x1000, 0x2000, 0x1000, "not mirrored"}, {0x10000, 0x30000, 0x18000, "uncommitted pages"}}, 0x2000);
    Require(!copies.empty() && copies.find("0x10000+0x20000 (0.1 MiB committed, uncommitted pages)") < copies.find("0x1000+0x1000"), "the copy limit does not name the largest copy first");
    AgcDriver::GuestMemory::Unwatch(0x10000, 0x20000);
    AgcDriver::GuestMemory::Unwatch(0x40000, 0x10000);
    Require(!AgcDriver::GuestMemory::Watched(0x10000, 0x20000), "the unregistered heap copy range is write-watched");
    const auto heapCopies = AddressCopyOverflow({{0x10000, 0x30000, 0x18000, nullptr, true, true}, {0x40000, 0x50000, 0x10000, nullptr, true, false}}, 0x2000);
    Require(heapCopies.find("0x10000+0x20000 (0.1 MiB committed, import over the host import budget, heap not fully write-watched)") != std::string::npos && heapCopies.find("0x40000+0x10000 (0.1 MiB committed, heap not fully write-watched)") != std::string::npos, "the copy limit does not name a copied heap's reason");
    ClearSnapshotCopies(context.device);
}
