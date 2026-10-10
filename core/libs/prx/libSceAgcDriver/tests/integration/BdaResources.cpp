#include <Testing/Test.hpp>
#include "GraphicsTests.hpp"
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
#include <cstdio>
#include <cstring>
#include <exception>
#include <functional>
#include <initializer_list>
#include <limits>
#include <new>
#include <set>
#include <source_location>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using Role = ShaderRecompiler::DescriptorRole;
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

struct BdaFixture {
    MockVulkanSession session;
    Context context = BdaMockContext();
    BdaTestAccess access = BdaMockAccess();
};

void requireNoLeaks(const BdaFixture& fixture, std::source_location location = std::source_location::current()) {
    RequireEqual(fixture.session.LiveObjects(), std::int64_t{0}, "BDA resources leaked Vulkan objects", location);
}

struct GuestWords {
    alignas(64) std::array<std::uint32_t, 16> words{};

    GuestWords() {
        words[0] = 123;
    }

    std::uintptr_t Address() const {
        return reinterpret_cast<std::uintptr_t>(words.data());
    }

    std::vector<std::uint32_t> Descriptor() const {
        return {static_cast<std::uint32_t>(Address()), static_cast<std::uint32_t>(Address() >> 32) & 0xffffu, sizeof(words), 0x31000000u};
    }
};

ShaderRecompiler::DescriptorBinding binding(Role role, std::uint32_t slot) {
    return {ShaderRecompiler::DescriptorKind::StorageBuffer, role, 0, slot, 1, {}, false};
}

void sweep(const Context& context) {
    GuestBufferMemory leased(context);
    leased.AcquireRegistered();
    leased.Upload(true);
    leased.WriteBack();
}

class Registration {
public:
    Registration(std::vector<std::pair<void*, std::size_t>> ranges, bool writable) : ranges(std::move(ranges)) {
        registered = true;
        GuestAllocations::Mutation mutation;
        for (const auto& [pointer, bytes] : this->ranges) mutation.Add(pointer, bytes, true, writable);
    }

    ~Registration() {
        if (!registered) return;
        try {
            Remove();
        } catch (const std::exception&) {
        }
    }

    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;

    void Remove() {
        registered = false;
        GuestAllocations::Mutation mutation;
        for (const auto& [pointer, bytes] : ranges) mutation.Remove(pointer);
    }

private:
    std::vector<std::pair<void*, std::size_t>> ranges;
    bool registered = false;
};

class ArenaBlock {
public:
    ArenaBlock(std::size_t bytes, std::size_t alignment) : bytes(bytes), block(GuestArena::GuestArenaAllocate_nid_postfix(bytes, alignment)) {
        Require(block != nullptr, "cannot allocate guest arena memory");
#ifdef _WIN32
        GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#endif
    }

    ~ArenaBlock() {
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, bytes);
#endif
        GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
    }

    ArenaBlock(const ArenaBlock&) = delete;
    ArenaBlock& operator=(const ArenaBlock&) = delete;

    void* Pointer() const {
        return block;
    }

    std::uint8_t* Bytes() const {
        return static_cast<std::uint8_t*>(block);
    }

    std::uintptr_t Address() const {
        return reinterpret_cast<std::uintptr_t>(block);
    }

private:
    std::size_t bytes;
    void* block;
};

void skipUnlessWriteWatchedArena(const char* untested) {
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        Testing::Skip(std::string("guest arena unavailable or not write-watched: ") + untested + " not tested");
    }
}

const Case aliasedViews{"GuestBufferMemory_AliasedWritableViews_ShareOneOwnerAndPublishWrites", [] {
    BdaFixture fixture;
    GuestWords guest;
    {
        GuestBufferMemory memory(fixture.context);
        memory.AddWritable(guest.Address(), sizeof(guest.words));
        memory.AddWritable(guest.Address() + 16, 16);
        memory.Upload(true);
        std::uint32_t adjustment = 0;
        const auto first = memory.Descriptor(guest.Address(), sizeof(guest.words), adjustment);
        RequireEqual(adjustment, 0u, "a view at its owner's start is bound off it");
        const auto alias = memory.Descriptor(guest.Address() + 16, 16, adjustment);
        Require(first.buffer == alias.buffer && alias.offset + adjustment == 16 && alias.range == 16 + adjustment, "aliased guest buffers have different owners");
        const auto ranges = memory.AddressRanges();
        Require(ranges.size() == 1 && ranges[0].begin == guest.Address() && ranges[0].end == guest.Address() + sizeof(guest.words), "incorrect BDA range bounds");
        Require(ranges[0].deviceAddress != 0 && ranges[0].permissions == (ShaderRecompiler::BdaAbi::Read | ShaderRecompiler::BdaAbi::Write), "incorrect BDA address or permissions");
        const std::uint32_t changed = 321;
        std::memcpy(fixture.access.bytes(alias.buffer).data() + alias.offset, &changed, sizeof(changed));
        memory.WriteBack();
        RequireEqual(guest.words[4], changed, "aliased GPU write was not published");
        RequireRejection([&] { memory.WriteBack(); }, "cannot be committed twice");
        RequireRejection([&] { memory.AddWritable(guest.Address(), sizeof(guest.words)); }, "frozen");
    }
    requireNoLeaks(fixture);
}};

const Case overflowingSnapshot{"GuestBufferMemory_SnapshotPastTheAddressSpace_IsRejected", [] {
    BdaFixture fixture;
    GuestBufferMemory overflow(fixture.context);
    const std::array<std::byte, 8> source{};
    RequireRejection([&] { overflow.AddSnapshot({std::numeric_limits<std::uint64_t>::max() - 3, source}); }, "overflow");
}};

const Case bdaRequirements{"ShaderResources_BdaTableWithoutAbiVersionOrFeature_IsRejected", [] {
    BdaFixture fixture;
    const std::array<std::byte, 8> source{};
    const std::array<GuestMemorySnapshot, 1> snapshots{{{0x7fff12340000ULL, source}}};
    ShaderRecompiler::RecompileResult shader;
    shader.bindings = {binding(Role::BdaPagetable, 4), binding(Role::FaultBuffer, 5)};
    const CompiledShader compiled{ShaderRecompiler::ShaderStage::Compute, &shader, 0};
    RequireRejection([&] { ShaderResources resources(fixture.context, compiled, snapshots); }, "ABI version");
    shader.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    auto disabled = fixture.context;
    disabled.bufferDeviceAddress = false;
    RequireRejection([&] { ShaderResources resources(disabled, compiled, snapshots); }, "not enabled");
    requireNoLeaks(fixture);
}};

const Case rectListFaultBuffer{"ShaderResources_RectListFaultBuffer_NeedsNoBdaAndKeepsGuestWrites", [] {
    BdaFixture fixture;
    GuestWords guest;
    {
        auto disabled = fixture.context;
        disabled.bufferDeviceAddress = false;
        const std::array<std::byte, 8> source{};
        ShaderRecompiler::RecompileResult control;
        control.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
        control.bindings = {binding(Role::FaultBuffer, 5)};
        auto writable = binding(Role::GuestBuffers, 6);
        writable.guestDescriptor = guest.Descriptor();
        control.bindings.push_back(writable);
        const std::array<CompiledShader, 1> stages{{{ShaderRecompiler::ShaderStage::TessellationControl, &control, 0}}};
        const std::array<GuestMemorySnapshot, 1> unusedSnapshots{{{0, source}}};
        ShaderResources resources(disabled, stages, ColorTarget{}, 0, 0, unusedSnapshots);
        const auto fault = fixture.access.bytes(fixture.access.descriptor(5).buffer);
        Require(std::all_of(fault.begin(), fault.end(), [](std::byte value) { return value == std::byte{}; }), "rect-list fault buffer was not initialized");
        const ShaderRecompiler::BdaAbi::Fault report{ShaderRecompiler::BdaAbi::FaultState::Ready, ShaderRecompiler::BdaAbi::FaultReason::InvalidRectangle, 0, 0, 0, 0, 0};
        std::memcpy(fault.data(), &report, sizeof(report));
        RequireRejection([&] { resources.WriteBack(); }, "rect-list requires");
        std::memset(fault.data(), 0, fault.size());
        const std::uint32_t changed = 456;
        std::memcpy(fixture.access.bytes(fixture.access.descriptor(6).buffer).data() + sizeof(std::uint32_t), &changed, sizeof(changed));
        resources.WriteBack();
        RequireEqual(guest.words[1], changed, "rect-list fault-only path lost guest buffer writes");
    }
    requireNoLeaks(fixture);
}};

const Case bdaTable{"ShaderResources_BdaTable_HoldsTheSnapshotRangeAndReportsPermissionFaults", [] {
    BdaFixture fixture;
    {
        const std::array<std::byte, 8> source{};
        const std::array<GuestMemorySnapshot, 1> snapshots{{{0x7fff12340000ULL, source}}};
        ShaderRecompiler::RecompileResult shader;
        shader.bindings = {binding(Role::BdaPagetable, 4), binding(Role::FaultBuffer, 5)};
        shader.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
        const CompiledShader compiled{ShaderRecompiler::ShaderStage::Compute, &shader, 0};
        ShaderResources resources(fixture.context, compiled, snapshots);
        const auto table = fixture.access.bytes(fixture.access.descriptor(4).buffer);
        ShaderRecompiler::BdaAbi::Header header{};
        ShaderRecompiler::BdaAbi::Range range{};
        std::memcpy(&header, table.data(), sizeof(header));
        Require(header.version == ShaderRecompiler::BdaAbi::Version && header.count == 1 && header.entryBytes == sizeof(range), "BDA header layout mismatch");
        std::memcpy(&range, table.data() + sizeof(header), sizeof(range));
        Require(range.begin == snapshots[0].address && range.end == range.begin + source.size(), "64-bit guest address was truncated");
        const auto fault = fixture.access.bytes(fixture.access.descriptor(5).buffer);
        Require(std::all_of(fault.begin(), fault.end(), [](std::byte value) { return value == std::byte{}; }), "fault buffer was not initialized");
        const ShaderRecompiler::BdaAbi::Fault denied{ShaderRecompiler::BdaAbi::FaultState::Ready, ShaderRecompiler::BdaAbi::FaultReason::Permission, snapshots[0].address + 4, 4, 0, 0x88, 0};
        std::memcpy(fault.data(), &denied, sizeof(denied));
        RequireRejection([&] { resources.WriteBack(); }, "read-only in the BDA table");
        std::memset(fault.data(), 0, fault.size());
        resources.WriteBack();
    }
    requireNoLeaks(fixture);
}};

const Case faultedCommand{"ShaderResources_FaultedCommand_PublishesNoWrites", [] {
    BdaFixture fixture;
    GuestWords guest;
    {
        ShaderRecompiler::RecompileResult shader;
        shader.bindings = {binding(Role::BdaPagetable, 4), binding(Role::FaultBuffer, 5)};
        shader.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
        auto writable = binding(Role::GuestBuffers, 6);
        writable.guestDescriptor = guest.Descriptor();
        shader.bindings.push_back(writable);
        const CompiledShader compiled{ShaderRecompiler::ShaderStage::Compute, &shader, 0};
        ShaderResources resources(fixture.context, compiled);
        const std::uint32_t changed = 999;
        std::memcpy(fixture.access.bytes(fixture.access.descriptor(6).buffer).data(), &changed, sizeof(changed));
        const ShaderRecompiler::BdaAbi::Fault report{ShaderRecompiler::BdaAbi::FaultState::Ready, ShaderRecompiler::BdaAbi::FaultReason::Unmapped, 0x7fff99880000ULL, 4, 0, 0x44, 0};
        std::memcpy(fixture.access.bytes(fixture.access.descriptor(5).buffer).data(), &report, sizeof(report));
        RequireRejection([&] { resources.WriteBack(); }, "BDA access failed");
        auto invalidRectangle = report;
        invalidRectangle.reason = ShaderRecompiler::BdaAbi::FaultReason::InvalidRectangle;
        std::memcpy(fixture.access.bytes(fixture.access.descriptor(5).buffer).data(), &invalidRectangle, sizeof(invalidRectangle));
        RequireRejection([&] { resources.WriteBack(); }, "rect-list requires");
        RequireEqual(guest.words[0], 123u, "failed GPU command published writes");
    }
    requireNoLeaks(fixture);
}};

const Case unalignedViews{"GuestBufferMemory_UnalignedViews_BindFromTheAlignmentBelowThem", [] {
    BdaFixture fixture;
    GuestWords guest;
    const auto address = guest.Address();
    {
        auto aligned = fixture.context;
        aligned.limits.minStorageBufferOffsetAlignment = 16;
        GuestBufferMemory unaligned(aligned);
        unaligned.AddWritable(address, sizeof(guest.words));
        unaligned.Upload(true);
        std::uint32_t adjustment = 0;
        const auto view = unaligned.Descriptor(address + 4, 4, adjustment);
        Require(adjustment == 4 && view.offset == 0 && view.range == 8, "a view off the offset alignment binds from below it");
        RequireRejection([&] { unaligned.Descriptor(address + sizeof(guest.words), 4, adjustment); }, "exceeds its GPU owner");
        const auto odd = unaligned.Descriptor(address + 2, 6, adjustment);
        Require(adjustment == 2 && odd.offset == 0 && odd.range == 8, "a view off a DWORD boundary does not bind the DWORDs around it");
        const auto late = unaligned.Descriptor(address + 0x13, 8, adjustment);
        Require(adjustment == 3 && late.offset == 16 && late.range == 12, "a view off a DWORD boundary does not bind from the offset alignment below it");
        GuestBufferMemory lone(aligned);
        lone.AddReadable(address + 6, 5);
        lone.Upload(true);
        const auto copied = lone.Descriptor(address + 6, 5, adjustment);
        Require(adjustment == 2 && copied.offset == 0 && copied.range == 8, "a lone view off a DWORD boundary is not copied from the DWORD below it");
        Require(std::memcmp(fixture.access.bytes(copied.buffer).data(), reinterpret_cast<const void*>(address + 4), 8) == 0, "a lone view off a DWORD boundary copied other bytes");
        GuestBufferMemory tail(aligned);
        tail.AddWritable(address, 62);
        tail.Upload(true);
        RequireRejection([&] { tail.Descriptor(address + 58, 4, adjustment); }, "exceeds its GPU owner");
    }
    requireNoLeaks(fixture);
}};

const Case cachedAddressSpace{"GuestBufferMemory_UnchangedRegistry_ReusesTheCachedAddressSpaceUntilAFree", [] {
    BdaFixture fixture;
    {
        void* block = GuestHeap::GuestHeapAllocate_nid_postfix(64);
        struct Free {
            void* block;
            ~Free() {
                if (block != nullptr) GuestHeap::GuestHeapFree_nid_postfix(block);
            }
        } owned{block};
        const auto blockAddress = reinterpret_cast<std::uintptr_t>(block);
        std::memset(block, 0x5a, 64);
        const auto registered = [&] {
            const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
            return std::any_of(lease.begin(), lease.end(), [&](const auto& range) { return range->address == blockAddress; });
        };
        Require(registered(), "guest heap block is not registered");
        const auto before = AddressSpaceCounters();
        for (int build = 0; build < 2; ++build) {
            GuestBufferMemory leased(fixture.context);
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
        owned.block = nullptr;
        GuestHeap::GuestHeapFree_nid_postfix(block);
        Require(!registered(), "freed guest heap block remains registered");
        const auto dropped = AddressSpaceCounters();
        if (dropped.enabled) Require(dropped.waiterDrops == after.waiterDrops + 1 && LeaseCounters().cacheDrops == dropped.waiterDrops, "the free did not drop the cached address space");
    }
    requireNoLeaks(fixture);
}};

const Case importCrossing{"GuestBufferMemory_ViewCrossingHostImports_IsBoundWithBothRangesBytes", [] {
    BdaFixture fixture;
    constexpr std::size_t half = 65536;
    struct AlignedBlock {
        void* block = ::operator new(2 * half, std::align_val_t{half});
        ~AlignedBlock() { ::operator delete(block, std::align_val_t{half}); }
    } storage;
    auto* guest = static_cast<std::uint8_t*>(storage.block);
    std::memset(guest, 0x11, half);
    std::memset(guest + half, 0x22, half);
    const auto first = reinterpret_cast<std::uintptr_t>(storage.block);
    const auto second = first + half;
    auto importing = fixture.context;
    importing.hostImportAlignment = half;
    {
        Registration registration({{guest, half}, {guest + half, half}}, false);
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
            const auto crossingBytes = fixture.access.bytes(crossing.buffer);
            Require(crossing.offset + crossing.range <= crossingBytes.size(), "a view crossing into the next registered range is bound past its host import");
            Require(crossingBytes[crossing.offset + adjustment + 15] == std::byte{0x11} && crossingBytes[crossing.offset + adjustment + 16] == std::byte{0x22}, "a view crossing into the next registered range misses its bytes");
            const auto inside = leased.Descriptor(second + 64, 16, adjustment);
            const auto insideBytes = fixture.access.bytes(inside.buffer);
            Require(inside.offset + inside.range <= insideBytes.size() && (inside.buffer == secondImport.buffer ? inside.offset + adjustment == 64 : insideBytes[inside.offset + adjustment] == std::byte{0x22}), "a view in the second registered range is bound past its buffer or misses its bytes");
            for (const auto& range : leased.AddressRanges()) {
                if (range.end <= first || range.begin >= second + half) continue;
                const auto mapped = fixture.access.addressBytes(range.deviceAddress);
                Require(mapped.size() >= range.end - range.begin, "a BDA range runs past the end of its host import");
                if (range.begin <= second && second < range.end && range.deviceAddress != secondImport.address) Require(mapped[second - range.begin] == std::byte{0x22}, "the BDA range over the second registered range misses its bytes");
            }
            leased.WriteBack();
        }
        registration.Remove();
        Require(HostImportFor(importing, first, half) == nullptr && !HostImportCovers(importing, second, half), "the host imports outlived their ranges");
    }
    requireNoLeaks(fixture);
}};

struct HeapRangeBuild {
    const Context& context;
    const BdaTestAccess& access;
    std::uintptr_t address;
    std::size_t bytes;

    std::uint64_t operator()(std::uint64_t written, const std::function<void(GuestBufferMemory&)>& gpu) const {
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
    }
};

constexpr std::size_t HeapBytes = 2 * 65536;

const Case readOnlyHeapMirror{"HeapMirror_ReadOnlyRange_IsRefilledOnlyWhereTheCpuWrote", [] {
    skipUnlessWriteWatchedArena("heap mirrors");
    BdaFixture fixture;
    {
        ArenaBlock block(HeapBytes, 65536);
        auto* guest = block.Bytes();
        std::memset(guest, 0x11, HeapBytes);
        const HeapRangeBuild build{fixture.context, fixture.access, block.Address(), HeapBytes};
        sweep(fixture.context);
        Registration registration({{block.Pointer(), HeapBytes}}, false);
        const auto before = MirrorCounters();
        const auto first = build(0, {});
        const auto made = MirrorCounters();
        Require(made.heapMirrors == before.heapMirrors + 1 && made.heapBytes == before.heapBytes + HeapBytes, "a read-only heap range was not mirrored");
        Require(fixture.access.addressBytes(first)[65536 + 3] == std::byte{0x11}, "the heap mirror was not filled");
        Require(build(0, {}) == first && MirrorCounters().blocksCopied == made.blocksCopied && MirrorCounters().heapRefills == made.heapRefills, "an unchanged heap range was read into its mirror again");
        guest[65536 + 3] = 0x22;
        Require(build(0, {}) == first && MirrorCounters().heapRefills == made.heapRefills + 1 && MirrorCounters().blocksCopied == made.blocksCopied + 1, "a written heap block was not read again alone");
        Require(fixture.access.addressBytes(first)[65536 + 3] == std::byte{0x22}, "the heap mirror missed the CPU write");
        const auto one = MirrorCounters();
        guest[5] = 0x33;
        guest[65536 + 7] = 0x44;
        Require(build(0, {}) == first && MirrorCounters().blocksCopied == one.blocksCopied + 2, "two written heap blocks were not read again");
        Require(fixture.access.addressBytes(first)[5] == std::byte{0x33} && fixture.access.addressBytes(first)[65536 + 7] == std::byte{0x44}, "the heap mirror missed the CPU writes");
        const auto quiet = MirrorCounters();
        Require(build(0, {}) == first && MirrorCounters().heapChecks == quiet.heapChecks && MirrorCounters().sweeps == quiet.sweeps && MirrorCounters().blocksCopied == quiet.blocksCopied, "an unchanged heap range was compared block by block or the mirrors were swept again");
        guest[65536 + 9] = 0x66;
        Require(build(0, {}) == first && MirrorCounters().heapChecks > quiet.heapChecks && MirrorCounters().blocksCopied == quiet.blocksCopied + 1 && fixture.access.addressBytes(first)[65536 + 9] == std::byte{0x66}, "a CPU write after an unchanged build was missed");
        registration.Remove();
        sweep(fixture.context);
        const auto swept = MirrorCounters();
        Require(swept.heapMirrors == before.heapMirrors && swept.heapBytes == before.heapBytes, "the heap mirror outlived its range");
    }
    requireNoLeaks(fixture);
}};

const Case writableHeapMirror{"HeapMirror_WritableRange_WritesBackGpuStoresWithoutLosingCpuStores", [] {
    skipUnlessWriteWatchedArena("heap mirrors");
    BdaFixture fixture;
    {
        ArenaBlock block(HeapBytes, 65536);
        auto* guest = block.Bytes();
        const auto address = block.Address();
        std::memset(guest, 0x11, HeapBytes);
        const HeapRangeBuild build{fixture.context, fixture.access, address, HeapBytes};
        sweep(fixture.context);
        const auto before = MirrorCounters();
        Registration registration({{block.Pointer(), HeapBytes}}, true);
        const auto device = build(address + 16, [&](GuestBufferMemory& leased) {
            std::uint32_t adjustment = 0;
            const auto view = leased.Descriptor(address + 16, 32, adjustment);
            fixture.access.bytes(view.buffer)[view.offset + adjustment] = std::byte{0x77};
            guest[16 + 8] = 0x55;
        });
        Require(guest[16] == 0x77 && guest[16 + 8] == 0x55, "a writable heap mirror's write-back lost the GPU's store or rolled back the CPU's");
        Require(build(0, {}) == device && fixture.access.addressBytes(device)[16] == std::byte{0x77} && fixture.access.addressBytes(device)[16 + 8] == std::byte{0x55}, "the writable heap mirror missed the stores");
        Require(build(address + 40, [&](GuestBufferMemory& leased) {
            std::uint32_t adjustment = 0;
            const auto view = leased.Descriptor(address + 40, 8, adjustment);
            fixture.access.bytes(view.buffer)[view.offset + adjustment] = std::byte{0x78};
        }) == device && guest[40] == 0x78, "a writable heap mirror's second write-back lost the GPU's store");
        guest[65536 + 1] = 0x79;
        Require(build(0, {}) == device && fixture.access.addressBytes(device)[40] == std::byte{0x78} && fixture.access.addressBytes(device)[65536 + 1] == std::byte{0x79}, "the writable heap mirror missed a store after a write-back");
        registration.Remove();
        sweep(fixture.context);
        RequireEqual(MirrorCounters().heapMirrors, before.heapMirrors, "the writable heap mirror outlived its range");
    }
    requireNoLeaks(fixture);
}};

#ifdef _WIN32
const Case interruptedBuild{"HeapMirror_BuildInterruptedByAnInaccessibleRange_KeepsChangedBlocksStale", [] {
    skipUnlessWriteWatchedArena("heap mirrors");
    BdaFixture fixture;
    {
        constexpr std::size_t half = 2 * 65536;
        ArenaBlock pair(2 * half, 65536);
        std::memset(pair.Pointer(), 0x11, 2 * half);
        auto* first = pair.Bytes();
        auto* second = first + half;
        Registration registration({{first, half}, {second, half}}, false);
        const auto firstDevice = [&] {
            GuestBufferMemory leased(fixture.context);
            leased.AcquireRegistered();
            leased.Upload(true);
            const auto ranges = leased.AddressRanges();
            const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == reinterpret_cast<std::uintptr_t>(first); });
            Require(found != ranges.end(), "the first heap range is missing from the BDA table");
            const auto device = found->deviceAddress;
            leased.WriteBack();
            return device;
        };
        const auto device = firstDevice();
        first[5] = 0x66;
        DWORD previous = 0;
        Require(VirtualProtect(second, 65536, PAGE_NOACCESS, &previous) != 0, "cannot protect the second heap range");
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(second), 65536);
        bool threw = false;
        try {
            GuestBufferMemory leased(fixture.context);
            leased.AcquireRegistered();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(VirtualProtect(second, 65536, PAGE_READWRITE, &previous) != 0, "cannot unprotect the second heap range");
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(second), 65536);
        Require(threw, "a build over an inaccessible heap mirror range did not fail");
        Require(firstDevice() == device && fixture.access.addressBytes(device)[5] == std::byte{0x66}, "an interrupted build left a changed heap block marked current");
        registration.Remove();
        sweep(fixture.context);
    }
    requireNoLeaks(fixture);
}};

const Case readOnlyPage{"HeapMirror_ReadOnlyPageInAWritableRange_RefusesGpuChangesToThePage", [] {
    skipUnlessWriteWatchedArena("heap mirrors");
    BdaFixture fixture;
    {
        constexpr std::size_t size = 2 * 65536;
        ArenaBlock raw(size, 65536);
        auto* bytes8 = raw.Bytes();
        std::memset(raw.Pointer(), 0x11, size);
        const auto base = raw.Address();
        const auto page = base + 65536;
        DWORD previous = 0;
        Require(VirtualProtect(reinterpret_cast<void*>(page), 4096, PAGE_READONLY, &previous) != 0, "cannot make the aliased page read-only");
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(page, 4096);
        struct RestorePage {
            std::uintptr_t page;
            ~RestorePage() {
                DWORD previous = 0;
                VirtualProtect(reinterpret_cast<void*>(page), 4096, PAGE_READWRITE, &previous);
                GuestAllocations::GuestAllocationsInvalidate_nid_postfix(page, 4096);
            }
        } restore{page};
        Registration registration({{raw.Pointer(), size}}, true);
        const auto storeAt = [&](std::uint64_t at) {
            GuestBufferMemory leased(fixture.context);
            leased.AcquireRegistered();
            leased.AddWritable(page - 16, 32);
            leased.Upload(true);
            const auto ranges = leased.AddressRanges();
            const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == base; });
            Require(found != ranges.end() && fixture.access.addressBytes(found->deviceAddress)[65536 + 8] == std::byte{0x11}, "a writable range with a read-only page was not mirrored with its bytes");
            std::uint32_t adjustment = 0;
            const auto view = leased.Descriptor(page - 16, 32, adjustment);
            fixture.access.bytes(view.buffer)[view.offset + adjustment + static_cast<std::size_t>(at - (page - 16))] = std::byte{0x77};
            leased.WriteBack();
        };
        const auto made = MirrorCounters().heapMirrors;
        storeAt(page - 8);
        Require(MirrorCounters().heapMirrors == made + 1 && bytes8[65536 - 8] == 0x77, "a store next to a read-only page was not written back");
        RequireRejection([&] { storeAt(page + 4); }, "aliased writes are not implemented");
        RequireEqual(bytes8[65536 + 4], std::uint8_t{0x11}, "a GPU change of a read-only page was not refused");
        {
            GuestBufferMemory plain(fixture.context);
            plain.AddReadable(page, 16);
            plain.Upload(false);
            std::uint32_t adjustment = 0;
            const auto view = plain.Descriptor(page, 16, adjustment);
            Require(fixture.access.bytes(view.buffer)[view.offset + adjustment] == std::byte{0x11}, "a descriptor over a read-only page of a writable range read zeros");
            plain.WriteBack();
        }
        registration.Remove();
        sweep(fixture.context);
    }
    requireNoLeaks(fixture);
}};
#endif

const Case heapMirrorBudget{"HeapMirror_PastTheMirrorBudget_ReusesTheExpiredMirrorsMemory", [] {
    skipUnlessWriteWatchedArena("heap mirrors");
    BdaFixture fixture;
    {
        constexpr std::size_t large = 36 * 65536;
        constexpr std::size_t small = 17 * 65536;
        constexpr std::size_t total = 2 * large + small;
        ArenaBlock raw(total, 65536);
        auto* const heap = raw.Bytes();
        const std::array<std::uint8_t*, 3> heaps{heap, heap + large, heap + 2 * large};
        const std::array<std::size_t, 3> sizes{large, large, small};
        for (std::size_t index = 0; index < heaps.size(); ++index) std::memset(heaps[index], 0x21 + static_cast<int>(index), sizes[index]);
        std::set<std::size_t> registered;
        const auto change = [&](std::initializer_list<std::size_t> added, std::initializer_list<std::size_t> removed) {
            GuestAllocations::Mutation mutation;
            for (const auto index : removed) {
                registered.erase(index);
                mutation.Remove(heaps[index]);
            }
            for (const auto index : added) {
                registered.insert(index);
                mutation.Add(heaps[index], sizes[index], true, false);
            }
        };
        struct Unregister {
            std::set<std::size_t>& registered;
            const std::array<std::uint8_t*, 3>& heaps;
            ~Unregister() {
                if (registered.empty()) return;
                try {
                    GuestAllocations::Mutation mutation;
                    for (const auto index : registered) mutation.Remove(heaps[index]);
                } catch (const std::exception&) {
                }
            }
        } unregister{registered, heaps};
        const auto mirrored = [&](std::size_t index) {
            GuestBufferMemory leased(fixture.context);
            leased.AcquireRegistered();
            leased.Upload(true);
            const auto ranges = leased.AddressRanges();
            const auto begin = reinterpret_cast<std::uintptr_t>(heaps[index]);
            const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& entry) { return entry.begin == begin && entry.end == begin + sizes[index]; });
            Require(found != ranges.end(), "a mirrored heap range is missing from the BDA table");
            const auto last = fixture.access.addressBytes(found->deviceAddress)[sizes[index] - 1];
            leased.WriteBack();
            return last;
        };
        sweep(fixture.context);
        const auto before = MirrorCounters();
        change({0}, {});
        Require(mirrored(0) == std::byte{0x21}, "the first heap range was not mirrored");
        const auto one = MirrorCounters();
        fixture.access.limitMemory(large - 1);
        change({1}, {0});
        Require(mirrored(1) == std::byte{0x22}, "a heap mirror past APS5_HEAP_MIRROR_MIB was not made in the memory of the expired mirror");
        const auto swept = MirrorCounters();
        Require(swept.heapMirrors == one.heapMirrors && swept.heapBytes == one.heapBytes, "the expired heap mirror was not swept before the allocation");
        fixture.access.limitMemory(small - 1);
        change({2}, {});
        char expected[64];
        std::snprintf(expected, sizeof(expected), "heap mirror of 0x%llx+0x%llx: ", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(heaps[2])), static_cast<unsigned long long>(small));
        const auto attempts = fixture.access.allocationAttempts();
        const auto error = Testing::RequireThrows<std::runtime_error>([&] { mirrored(2); }, "a heap mirror the memory cannot hold did not fail its build");
        const std::string what = error.what();
        Require(what.find(expected) != std::string::npos && what.find("Vulkan result -2") != std::string::npos, "a heap mirror the memory cannot hold did not fail its build with the Vulkan result: " + what);
        RequireEqual(fixture.access.allocationAttempts(), attempts + 1, "a refused heap mirror was allocated again");
        Require(MirrorCounters().heapMirrors == swept.heapMirrors && MirrorCounters().heapBytes == swept.heapBytes, "a refused heap mirror was registered");
        fixture.access.limitMemory(std::nullopt);
        Require(mirrored(2) == std::byte{0x23} && MirrorCounters().heapMirrors == swept.heapMirrors + 1, "a heap mirror was not made once the memory was free again");
        change({}, {1, 2});
        sweep(fixture.context);
        Require(MirrorCounters().heapMirrors == before.heapMirrors && MirrorCounters().heapBytes == before.heapBytes, "the heap mirrors outlived their ranges");
    }
    requireNoLeaks(fixture);
}};

const Case importedHeapMirror{"HeapMirror_RangeServedByAHostImport_IsNotMirroredUntilTheImportIsGone", [] {
    skipUnlessWriteWatchedArena("heap mirrors of imported ranges");
    BdaFixture fixture;
    {
        constexpr std::size_t bytes = 1u << 20u;
        ArenaBlock block(bytes, bytes);
        std::memset(block.Pointer(), 0x11, bytes);
        const auto address = block.Address();
        auto importing = fixture.context;
        importing.hostImportAlignment = bytes;
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
        Registration registration({{block.Pointer(), bytes}}, false);
        Require(build(fixture.context) != 0, "the heap range is missing from the BDA table of a build without host imports");
        const auto mirrored = MirrorCounters();
        Require(mirrored.heapMirrors == before.heapMirrors + 1 && mirrored.heapBytes == before.heapBytes + bytes, "a range built without host imports was not heap mirrored");
        Require(build(importing) != 0 && HostImportCovers(importing, address, bytes), "the heap mirrored range was not imported once imports were available");
        const auto imported = MirrorCounters();
        Require(imported.heapMirrors == before.heapMirrors && imported.heapBytes == before.heapBytes, "a heap mirror outlived the host import that serves its range");
        Require(build(importing) != 0 && MirrorCounters().heapMirrors == before.heapMirrors && MirrorCounters().rebuilds == imported.rebuilds, "an imported range was heap mirrored again");
        block.Bytes()[bytes / 2] = 0x22;
        const auto device = build(fixture.context);
        const auto again = MirrorCounters();
        Require(device != 0 && again.heapMirrors == before.heapMirrors + 1 && again.heapBytes == before.heapBytes + bytes && again.rebuilds == imported.rebuilds + 1, "a range no import serves was not heap mirrored again");
        Require(fixture.access.addressBytes(device)[bytes / 2] == std::byte{0x22} && fixture.access.addressBytes(device)[bytes / 2 + 1] == std::byte{0x11}, "a heap mirror made after its import missed the guest bytes");
        registration.Remove();
        Require(HostImportFor(importing, address, bytes) == nullptr && !HostImportCovers(importing, address, bytes), "the import outlived its range");
        RequireEqual(build(fixture.context), std::uint64_t{0}, "the unregistered heap range is still in the BDA table");
        Require(MirrorCounters().heapMirrors == before.heapMirrors && MirrorCounters().heapBytes == before.heapBytes, "the heap mirror outlived its range");
    }
    requireNoLeaks(fixture);
}};

const Case copyOverflow{"AddressCopyOverflow_CopiesPastTheLimit_NameTheLargestCopyFirst", [] {
    Require(AddressCopyOverflow({{0x1000, 0x3000, 0x2000, "uncommitted pages"}}, 0x2000).empty(), "copies within the limit were refused");
    const auto copies = AddressCopyOverflow({{0x1000, 0x2000, 0x1000, "not mirrored"}, {0x10000, 0x30000, 0x18000, "uncommitted pages"}}, 0x2000);
    Require(!copies.empty() && copies.find("0x10000+0x20000 (0.1 MiB committed, uncommitted pages)") < copies.find("0x1000+0x1000"), "the copy limit does not name the largest copy first");
}};

} // namespace
