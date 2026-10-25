#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::ColorTarget;
using AgcDriver::Graphics::Require;

constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Side = 64;
constexpr std::byte Marker{0xab};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::byte*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::byte*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "multisampled fill: cannot allocate a guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::byte* Data() { return block; }

    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

    bool Holds(std::byte value, std::size_t offset, std::size_t bytes) const {
        return std::all_of(block + offset, block + offset + bytes, [&](std::byte held) { return held == value; });
    }

private:
    std::byte* block = nullptr;
};

GuestBlock& NewBlock() {
    static auto* blocks = new std::deque<GuestBlock>();
    return blocks->emplace_back();
}

std::uint32_t Low(std::uint64_t value) { return static_cast<std::uint32_t>(value); }

std::uint32_t High(std::uint64_t value) { return static_cast<std::uint32_t>(value >> 32u); }

std::vector<std::uint32_t> Pm4Packet(std::uint32_t opcode, std::initializer_list<std::uint32_t> body) {
    std::vector<std::uint32_t> words{0xc0000000u | (static_cast<std::uint32_t>(body.size() - 1) << 16u) | (opcode << 8u)};
    words.insert(words.end(), body);
    return words;
}

void Submit(std::initializer_list<std::vector<std::uint32_t>> packets) {
    std::vector<std::uint32_t> words;
    for (const auto& packet : packets) words.insert(words.end(), packet.begin(), packet.end());
    Packet dcb{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&dcb) == 0, "multisampled fill: DCB submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

constexpr std::array<std::uint32_t, 9> FillKernel{0xd7460004u, 0x04010c08u, 0x7e000204u, 0x7e020205u, 0x7e040206u, 0x7e060207u, 0xe01c2000u, 0x80000004u, 0xbf810000u};

void DispatchFill(const GuestBlock& code, std::uint64_t base, std::uint32_t bytes, std::uint32_t value) {
    const auto kernel = code.Address();
    Submit({
        Pm4Packet(0x76, {0x20c, Low(kernel >> 8u)}),
        Pm4Packet(0x76, {0x20d, High(kernel >> 8u) & 0xffu}),
        Pm4Packet(0x76, {0x207, 64}),
        Pm4Packet(0x76, {0x208, 1}),
        Pm4Packet(0x76, {0x209, 1}),
        Pm4Packet(0x76, {0x213, (8u << 1u) | 0x80u}),
        Pm4Packet(0x76, {0x240, Low(base)}),
        Pm4Packet(0x76, {0x241, (High(base) & 0xffffu) | (16u << 16u)}),
        Pm4Packet(0x76, {0x242, bytes / 16u}),
        Pm4Packet(0x76, {0x243, 0x4bfacu}),
        Pm4Packet(0x76, {0x244, value}),
        Pm4Packet(0x76, {0x245, value}),
        Pm4Packet(0x76, {0x246, value}),
        Pm4Packet(0x76, {0x247, value}),
        Pm4Packet(0x15, {bytes / 1024u, 1, 1, 0x41}),
    });
}

void Settle(const GuestBlock& code, const GuestBlock& sentinel) {
    static std::uint32_t round = 0;
    ++round;
    DispatchFill(code, sentinel.Address(), 1024, round);
    std::array<std::uint32_t, 4> stored{};
    AgcDriver::GuestMemory::Read(sentinel.Address(), std::as_writable_bytes(std::span(stored)), 4);
    Require(stored == std::array<std::uint32_t, 4>{round, round, round, round}, "the sentinel fill did not land");
}

ColorTarget MultisampledTarget(const GuestBlock& color) {
    ColorTarget target{};
    target.address = color.Address();
    target.surfaceAddress = target.address;
    target.extent = {Side, Side};
    target.surfaceExtent = target.extent;
    target.format = VK_FORMAT_R8G8B8A8_UNORM;
    target.tileMode = AgcDriver::Graphics::ColorTileMode::RenderTarget;
    target.elementBytes = 4;
    target.bytes = BlockBytes / 4;
    target.samples = 4;
    return target;
}

bool FillsTakenAsMultisampledClearsAreNotStored(AgcDriver::VulkanDevice& device) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if ((context.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0) {
        std::cout << "skipped, the device has no 4x color targets\n";
        return false;
    }
    struct ClearTargets {
        AgcDriver::VulkanDevice& device;
        VkDevice handle;
        ~ClearTargets() {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            if (auto* active = AgcDriver::Graphics::Recorder::Active(); active != nullptr && active->Device() != handle) {
                active->Sync();
                active->FlushDeferredReleases();
            }
            device.SubmitRecorded(true);
            device.WaitIdle();
            AgcDriver::Graphics::ClearMultisampleTargets(handle);
            AgcDriver::Graphics::ClearCachedTextures(handle);
        }
    } clearTargets{device, context.device};
    auto& code = NewBlock();
    std::memcpy(code.Data(), FillKernel.data(), sizeof(FillKernel));

    auto& sentinel = NewBlock();
    auto& control = NewBlock();
    std::memset(control.Data(), static_cast<int>(Marker), BlockBytes);
    DispatchFill(code, control.Address(), BlockBytes, 0);
    Settle(code, sentinel);
    if (control.Holds(Marker, 0, BlockBytes)) {
        std::cout << "skipped, the device does not store compute fills into guest memory\n";
        return false;
    }
    Require(control.Holds(std::byte{}, 0, BlockBytes), "a compute fill of 0 over plain guest memory did not store 0");

    auto& color = NewBlock();
    const auto target = MultisampledTarget(color);
    Require(AgcDriver::Graphics::MultisampledColorBytes(target) == BlockBytes, "the 4x 64x64 RGBA8 target does not take one 64 KiB block");
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        auto* active = AgcDriver::Graphics::Recorder::Active();
        Require(active != nullptr && active->Device() != context.device, "the fill test requires a second device's active recorder");
        const auto keptBytes = active->OpenKeptBytes();
        std::shared_ptr<void> owner;
        Require(AgcDriver::Graphics::MultisampleTargetView(context, target, owner) != VK_NULL_HANDLE && owner != nullptr, "the multisampled target has no view");
        Require(active->OpenKeptBytes() == keptBytes, "the multisampled target was charged to another device's recorder");
        device.SubmitRecorded(true);
        device.WaitIdle();
    }
    Require(AgcDriver::Graphics::MultisampledColorFillAbsorbs(color.Address(), BlockBytes, 0), "a fill of 0 over the whole color memory is not taken as the target's clear");
    Require(!AgcDriver::Graphics::MultisampledColorFillAbsorbs(color.Address(), BlockBytes / 2, 0), "a fill over half of the color memory is taken as the target's clear");
    Require(!AgcDriver::Graphics::MultisampledColorFillAbsorbs(color.Address() + 4096, BlockBytes - 4096, 0), "a fill starting inside the color memory is taken as the target's clear");
    Require(!AgcDriver::Graphics::MultisampledColorFillAbsorbs(color.Address(), 2 * BlockBytes, 0), "a fill reaching past the color memory is taken as the target's clear");

    std::memset(color.Data(), static_cast<int>(Marker), BlockBytes);
    DispatchFill(code, color.Address(), BlockBytes, 0);
    Settle(code, sentinel);
    Require(color.Holds(Marker, 0, BlockBytes), "a compute fill taken as the multisampled target's clear was stored into its color memory");
    DispatchFill(code, color.Address(), BlockBytes / 2, 0);
    Settle(code, sentinel);
    Require(color.Holds(std::byte{}, 0, BlockBytes / 2) && color.Holds(Marker, BlockBytes / 2, BlockBytes / 2), "a compute fill over half of the color memory was not stored");

    std::memset(control.Data(), static_cast<int>(Marker), BlockBytes);
    DispatchFill(code, control.Address(), BlockBytes, 0);
    Settle(code, sentinel);
    Require(control.Holds(std::byte{}, 0, BlockBytes), "a compute fill of plain guest memory was not stored once a multisampled target exists");
    auto& srgbColor = NewBlock();
    auto srgb = MultisampledTarget(srgbColor);
    srgb.format = VK_FORMAT_R8G8B8A8_SRGB;
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        auto* active = AgcDriver::Graphics::Recorder::Active();
        Require(active != nullptr && active->Device() != context.device, "the sRGB fill test requires a second device's active recorder");
        const auto keptBytes = active->OpenKeptBytes();
        Require(AgcDriver::Graphics::MultisampleTargetView(context, srgb) != VK_NULL_HANDLE, "the sRGB multisampled target has no view");
        Require(active->OpenKeptBytes() == keptBytes, "the sRGB multisampled target was charged to another device's recorder");
        device.SubmitRecorded(true);
        device.WaitIdle();
    }
    Require(AgcDriver::Graphics::MultisampledColorFillAbsorbs(srgbColor.Address(), BlockBytes, 0), "a fill of 0 over an sRGB target's color memory is not taken as its clear");
    Require(!AgcDriver::Graphics::MultisampledColorFillAbsorbs(srgbColor.Address(), BlockBytes, 0x80808080u), "a fill with no exact sRGB clear value is taken as the target's clear");
    std::memset(srgbColor.Data(), static_cast<int>(Marker), BlockBytes);
    DispatchFill(code, srgbColor.Address(), BlockBytes, 0x80808080u);
    Settle(code, sentinel);
    Require(srgbColor.Holds(std::byte{0x80}, 0, BlockBytes), "a compute fill with no exact clear value was not stored into the color memory");

    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(color.Data());
        mutation.Add(color.Data(), BlockBytes, true, true, true);
    }
    Require(!AgcDriver::Graphics::MultisampledColorFillAbsorbs(color.Address(), BlockBytes, 0), "a fill over a retired target's color memory is taken as its clear");
    std::memset(color.Data(), static_cast<int>(Marker), BlockBytes);
    DispatchFill(code, color.Address(), BlockBytes, 0);
    Settle(code, sentinel);
    Require(color.Holds(std::byte{}, 0, BlockBytes), "a compute fill over a retired target's color memory was not stored");
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        std::shared_ptr<void> owner;
        Require(AgcDriver::Graphics::MultisampleTargetView(context, target, owner) != VK_NULL_HANDLE && owner != nullptr, "the replacement multisampled target has no view");
        const std::weak_ptr<void> targetOwner = owner;
        owner.reset();
        device.SubmitRecorded(true);
        device.WaitIdle();
        AgcDriver::Graphics::ClearMultisampleTargets(context.device);
        Require(targetOwner.expired(), "another device's recorder retained a multisampled target after its device's work completed");
    }
    return true;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool tested = FillsTakenAsMultisampledClearsAreNotStored(*device);
        LibcRunShutdown_nid_postfix();
        if (!tested) return VulkanTestSkipped;
        std::cout << "multisampled fill tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            LibcRunShutdown_nid_postfix();
        } catch (...) {
        }
        return 1;
    }
}
