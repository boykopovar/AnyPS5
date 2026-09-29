#include "ViewAliases.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

constexpr std::size_t BlockBytes = 65536;

std::uint8_t pattern(std::size_t offset) {
    return static_cast<std::uint8_t>(offset * 7u + 3u);
}

struct Probe {
    VkDescriptorBufferInfo binding;
    std::vector<std::uint8_t> store;
};

void recordProbes(const Context& context, Recorder& recorder, std::span<const Probe> probes, Buffer& readback) {
    const auto commands = recorder.Commands();
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
    VkDeviceSize at = 0;
    for (const auto& probe : probes) {
        CopyBuffer(context, commands, probe.binding.buffer, probe.binding.offset, readback.Handle(), at, probe.binding.range);
        at += probe.binding.range;
    }
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    for (const auto& probe : probes) {
        if (probe.store.empty()) continue;
        context.Function<PFN_vkCmdUpdateBuffer>("vkCmdUpdateBuffer")(commands, probe.binding.buffer, probe.binding.offset, probe.store.size(), probe.store.data());
    }
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT);
}

template<typename TAction>
void reject(TAction action, const char* reason) {
    try { action(); }
    catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find(reason) != std::string::npos, std::string("unexpected view alias error: ") + error.what());
        return;
    }
    throw std::runtime_error(std::string("expected view alias rejection: ") + reason);
}

}

void RunViewAliasTests(const Context& context, Recorder& recorder) {
    const auto alignment = context.limits.minStorageBufferOffsetAlignment;
    if (context.hostImportAlignment == 0 || alignment < 8) {
        std::cout << "host imports unavailable or storage buffer offsets 4-byte bindable: view aliases not tested\n";
        return;
    }
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, BlockBytes);
#endif
    Require(block != nullptr, "cannot allocate the view alias test block");
    auto* bytes = static_cast<std::uint8_t*>(block);
    const auto reset = [&] {
        for (std::size_t i = 0; i < BlockBytes; ++i) bytes[i] = pattern(i);
    };
    reset();
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, BlockBytes, true, true);
    }
    const auto* import = HostImportFor(context, address, BlockBytes);
    if (import == nullptr) {
        std::cout << "host import of the view alias block refused: view aliases not tested\n";
        return;
    }
    const auto unit = std::max<std::uint64_t>(alignment, 64);
    const auto half = alignment / 2;
    const auto aligned = [&](const VkDescriptorBufferInfo& info) { return info.offset % alignment == 0; };
    const auto holds = [&](const std::vector<std::byte>& read, std::size_t at, std::uint64_t view, std::size_t length) {
        for (std::size_t i = 0; i < length; ++i) {
            if (static_cast<std::uint8_t>(read[at + i]) != pattern(static_cast<std::size_t>(view - address + i))) return false;
        }
        return true;
    };
    const auto readbackBytes = [](Buffer& buffer) {
        buffer.Invalidate();
        const auto span = buffer.Bytes();
        return std::vector<std::byte>(span.begin(), span.end());
    };
    const std::vector<std::uint8_t> stored{0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8};

    {
        const auto region = address + unit;
        const auto odd = region + 2 * unit + half;
        const auto peek = region + unit / 4 + half;
        const auto even = region + 3 * unit;
        GuestBufferMemory memory(context);
        memory.AddReadable(region, static_cast<std::size_t>(4 * unit));
        memory.AddWritable(odd, 16);
        memory.AddReadable(peek, 8);
        memory.AddWritable(even, 16);
        memory.Upload(false);
        Require(memory.ViewAliases() == 2 && memory.GpuViewAliases() == 2, "misaligned views in an import were not copied by the GPU");
        const auto whole = memory.Descriptor(region, static_cast<std::size_t>(4 * unit));
        const auto oddInfo = memory.Descriptor(odd, 16);
        const auto peekInfo = memory.Descriptor(peek, 8);
        const auto evenInfo = memory.Descriptor(even, 16);
        Require(whole.buffer == import->buffer && evenInfo.buffer == import->buffer && evenInfo.offset == even - import->base, "aligned views do not bind the import in place");
        Require(aligned(oddInfo) && aligned(peekInfo) && oddInfo.buffer != import->buffer && peekInfo.buffer != import->buffer, "misaligned views bind at misaligned offsets");
        reject([&] { memory.Descriptor(odd + 4, 4); }, "offset alignment");
        Require(memory.HasCopiedWrites(), "an aliased written view needs no copy-back");
        Buffer readback(context, 64, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const std::array<Probe, 3> probes{{{oddInfo, stored}, {peekInfo, {}}, {evenInfo, {0xb1, 0xb2, 0xb3, 0xb4}}}};
        recordProbes(context, recorder, probes, readback);
        memory.RecordCopyBacks(recorder);
        Require(!memory.HasCopiedWrites(), "the aliased view's copy-back was not recorded");
        recorder.Sync();
        memory.WriteBack();
        const auto read = readbackBytes(readback);
        Require(holds(read, 0, odd, 16) && holds(read, 16, peek, 8) && holds(read, 24, even, 16), "a view's binding does not hold the guest bytes at its address");
        const auto at = [&](std::uint64_t guest) { return bytes[guest - address]; };
        for (std::size_t i = 0; i < stored.size(); ++i) Require(at(odd + i) == stored[i], "a store through a view alias did not land in guest memory");
        Require(at(even) == 0xb1 && at(even + 3) == 0xb4, "a store in place did not land in guest memory");
        Require(at(odd + 8) == pattern(odd + 8 - address) && at(odd - 1) == pattern(odd - 1 - address) && at(even + 4) == pattern(even + 4 - address), "a copy-back rolled back bytes it did not write");
    }
    reset();
    {
        const auto first = address + 8 * unit + half;
        const auto second = first + half;
        GuestBufferMemory memory(context);
        memory.AddReadable(first, static_cast<std::size_t>(unit));
        memory.AddWritable(second, 32);
        memory.Upload(false);
        Require(memory.ViewAliases() == 1 && memory.GpuViewAliases() == 1, "the overlapping allocation was not given a GPU copy");
        const auto firstInfo = memory.Descriptor(first, static_cast<std::size_t>(unit));
        const auto secondInfo = memory.Descriptor(second, 32);
        Require(aligned(firstInfo) && aligned(secondInfo) && firstInfo.buffer != secondInfo.buffer && firstInfo.buffer != import->buffer, "overlapping allocations bind at misaligned offsets");
        Buffer readback(context, static_cast<std::size_t>(unit) + 32, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const std::array<Probe, 2> probes{{{firstInfo, {}}, {secondInfo, stored}}};
        recordProbes(context, recorder, probes, readback);
        recorder.Sync();
        Require(memory.HasCopiedWrites(), "a use without copy-back has nothing to write back");
        memory.WriteBack();
        const auto read = readbackBytes(readback);
        Require(holds(read, 0, first, static_cast<std::size_t>(unit)) && holds(read, static_cast<std::size_t>(unit), second, 32), "an overlapping allocation's binding does not hold its guest bytes");
        for (std::size_t i = 0; i < stored.size(); ++i) Require(bytes[second - address + i] == stored[i], "the overlapping allocation's store did not land in guest memory");
        Require(bytes[first - address] == pattern(first - address), "the write-back stored outside the written view");
    }
    reset();
    {
        const auto first = address + 12 * unit + half;
        GuestBufferMemory memory(context);
        memory.AddWritable(first, static_cast<std::size_t>(unit));
        memory.AddWritable(first + half, 32);
        reject([&] { memory.Upload(false); }, "incompatible storage buffer offset alignments");
        recorder.Sync();
    }
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    }
    HostImportFor(context, address, BlockBytes);
}
