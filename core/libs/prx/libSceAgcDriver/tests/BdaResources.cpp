#include "BdaTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/GuestArena.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <string>

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

ShaderRecompiler::DescriptorBinding binding(Role role, std::uint32_t slot) {
    return {ShaderRecompiler::DescriptorKind::StorageBuffer, role, 0, slot, 1, {}, false};
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
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, block, bytes, true, writable);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, block);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    const auto build = [&](std::uint64_t written, const std::function<void(GuestBufferMemory&)>& gpu) {
        GuestBufferMemory leased(context);
        leased.AcquireRegistered();
        if (written != 0) leased.AddWritable(written, 32);
        leased.Upload(true);
        const auto ranges = leased.AddressRanges();
        const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == address && range.end == address + bytes; });
        Require(found != ranges.end() && found->permissions == ShaderRecompiler::BdaAbi::Read, "the heap range is missing from the BDA table");
        const auto device = found->deviceAddress;
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
                if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, half, true, false);
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
            if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, raw, size, true, true);
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
#ifdef _WIN32
    GuestArena::GuestArenaReset_nid_postfix(block, bytes);
#endif
    GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
}

}

void RunBdaResourceTests(const Context& context, const BdaTestAccess& access) {
    {
        std::array<std::byte, 160> bytes{};
        for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<std::byte>(i);
        constexpr std::uint64_t base = 0x7fff12340000ULL;
        const std::array<std::pair<std::size_t, std::size_t>, 6> pieces{{{96, 112}, {0, 32}, {16, 48}, {40, 64}, {128, 144}, {104, 120}}};
        const std::array<std::pair<std::size_t, std::size_t>, 3> expected{{{0, 64}, {96, 120}, {128, 144}}};
        for (std::size_t order = 0; order < 2 * pieces.size(); ++order) {
            GuestBufferMemory merged(context);
            for (std::size_t i = 0; i < pieces.size(); ++i) {
                const auto index = (order + (order < pieces.size() ? i : pieces.size() - i)) % pieces.size();
                const auto [begin, end] = pieces[index];
                merged.AddSnapshot({base + begin, std::span<const std::byte>(bytes).subspan(begin, end - begin)});
            }
            merged.Upload(true);
            const auto ranges = merged.AddressRanges();
            Require(ranges.size() == expected.size(), "overlapping snapshots lost a disjoint range");
            for (std::size_t i = 0; i < expected.size(); ++i) {
                const auto [begin, end] = expected[i];
                Require(ranges[i].begin == base + begin && ranges[i].end == base + end, "overlapping snapshots have incorrect bounds");
                Require(std::memcmp(access.addressBytes(ranges[i].deviceAddress).data(), bytes.data() + begin, end - begin) == 0, "overlapping snapshots lost their contents");
            }
            for (const auto [begin, end] : pieces) {
                std::uint32_t adjustment = 0;
                const auto view = merged.Descriptor(base + begin, end - begin, adjustment);
                Require(std::memcmp(access.bytes(view.buffer).data() + view.offset + adjustment, bytes.data() + begin, end - begin) == 0, "merged snapshot subview has incorrect contents");
            }
            merged.WriteBack();
        }
        GuestBufferMemory inconsistent(context);
        inconsistent.AddSnapshot({base, std::span<const std::byte>(bytes).first(32)});
        bytes[16] = std::byte{255};
        inconsistent.AddSnapshot({base + 16, std::span<const std::byte>(bytes).subspan(16, 32)});
        reject([&] { inconsistent.Upload(true); }, "inconsistent overlapping guest snapshots");
    }
    alignas(64) std::array<std::uint32_t, 16> guest{};
    guest[0] = 123;
    const auto address = reinterpret_cast<std::uintptr_t>(guest.data());
    GuestBufferMemory memory(context);
    memory.AddWritable(address, sizeof(guest));
    memory.AddWritable(address + 16, 16);
    memory.Upload(true);
    std::uint32_t adjustment = 0;
    const auto first = memory.Descriptor(address, sizeof(guest), adjustment);
    Require(adjustment == 0, "a view at its owner's start is bound off it");
    const auto alias = memory.Descriptor(address + 16, 16, adjustment);
    Require(first.buffer == alias.buffer && alias.offset + adjustment == 16 && alias.range == 16 + adjustment, "aliased guest buffers have different owners");
    const auto ranges = memory.AddressRanges();
    Require(ranges.size() == 1 && ranges[0].begin == address && ranges[0].end == address + sizeof(guest), "incorrect BDA range bounds");
    Require(ranges[0].deviceAddress != 0 && ranges[0].permissions == ShaderRecompiler::BdaAbi::Read, "incorrect BDA address or permissions");
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
        ShaderResources resources(disabled, stages, ColorTarget{}, 0, 0, unusedSnapshots);
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
        ShaderResources resources(context, compiled, snapshots);
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
    }
    {
        // The cached address space: a second build in an unchanged registry takes the first one's
        // space, and a guest free of a range pinned only by the cache goes through the pin waiter's
        // drop (no GPU work to wait for) and empties the registry of it.
        void* block = GuestHeap::GuestHeapAllocate_nid_postfix(64);
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
    heapMirrorTests(context, access);
    Require(AddressCopyOverflow({{0x1000, 0x3000, 0x2000, "uncommitted pages"}}, 0x2000).empty(), "copies within the limit were refused");
    const auto copies = AddressCopyOverflow({{0x1000, 0x2000, 0x1000, "not mirrored"}, {0x10000, 0x30000, 0x18000, "uncommitted pages"}}, 0x2000);
    Require(!copies.empty() && copies.find("0x10000+0x20000 (0.1 MiB committed, uncommitted pages)") < copies.find("0x1000+0x1000"), "the copy limit does not name the largest copy first");
}
