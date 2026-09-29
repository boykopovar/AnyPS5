#include "BdaTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

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

}

void RunBdaResourceTests(const Context& context, const BdaTestAccess& access) {
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
}
