#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PlaceholderImages.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#include "ImageTableAbi.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string_view>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using namespace AgcDriver::Graphics;

constexpr std::uint32_t Side = 64;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t AllocationBytes = 2 * BlockBytes;

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        data = GuestArena::GuestArenaAllocate_nid_postfix(AllocationBytes, BlockBytes);
        if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, AllocationBytes, PAGE_READWRITE, AllocationBytes);
#else
        void* mapped = mmap(nullptr, AllocationBytes + BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + AllocationBytes != begin + AllocationBytes + BlockBytes) munmap(reinterpret_cast<void*>(aligned + AllocationBytes), begin + BlockBytes - aligned);
            data = reinterpret_cast<void*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, AllocationBytes);
        }
#endif
        Require(data != nullptr, "cannot allocate tracked image memory");
        std::memset(data, 0, AllocationBytes);
        GuestAllocations::Mutation().Add(data, AllocationBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(data);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(data, AllocationBytes);
        GuestArena::GuestArenaRelease_nid_postfix(data, AllocationBytes);
#else
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, AllocationBytes);
        munmap(data, AllocationBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(data); }

private:
    void* data = nullptr;
};

template<typename TCall>
void RequireColorGuard(TCall&& call, const char* failure) {
    bool guarded = false;
    try {
        call();
    } catch (const std::runtime_error& error) {
        guarded = std::string_view(error.what()).find("multisampled color target") != std::string_view::npos;
        if (!guarded) throw;
    }
    Require(guarded, failure);
}

void CheckCachedImage(AgcDriver::VulkanDevice& device, bool sampled, bool draw) {
    std::printf("checking %s %s image reuse\n", draw ? "draw" : "compute", sampled ? "sampled" : "storage");
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    GuestBlock block;
    struct Cleanup {
        VkDevice device;
        ~Cleanup() {
            ClearMultisampleTargets(device);
            ClearCachedTextures(device);
        }
    } cleanup{context.device};
    const auto address = block.Address();
    ShaderRecompiler::DescriptorBinding binding{};
    binding.kind = sampled ? ShaderRecompiler::DescriptorKind::SampledImage : ShaderRecompiler::DescriptorKind::StorageImage;
    binding.role = ShaderRecompiler::DescriptorRole::GuestImages;
    binding.binding = (sampled ? ShaderRecompiler::RuntimeAbi::FirstImageBinding : ShaderRecompiler::RuntimeAbi::FirstStorageImageBinding) + PlaceholderDimensionsPerClass + 2u;
    binding.count = 1;
    binding.imageShape = ShaderRecompiler::DescriptorImageShape::Image2D;
    binding.imageWritten = {false};
    binding.imageSamplers = {0u};
    binding.guestDescriptor = {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (20u << 20u) | (((Side - 1u) & 3u) << 30u), ((Side - 1u) >> 2u) | ((Side - 1u) << 14u), 0xfacu | (9u << 28u), 0u, 0u, 0u, 0u};
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult program;
    program.bindings.push_back(binding);
    const CompiledShader shader{draw ? ShaderRecompiler::ShaderStage::Fragment : ShaderRecompiler::ShaderStage::Compute, &program, 0};
    const std::array<CompiledShader, 2> stages{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, shader}};
    const auto build = [&] {
        if (draw) return std::make_unique<ShaderResources>(context, stages, ColorTarget{}, 0, 0, 0);
        return std::make_unique<ShaderResources>(context, shader);
    };
    auto resources = build();
    Require(resources->Reusable(), "the ordinary image resources are not reusable");
    const auto revalidate = [&](ShaderResources::ProofReport* report = nullptr) {
        return draw ? resources->Revalidate(stages, report) : resources->Revalidate(shader, report);
    };
    ShaderResources::ProofReport proof;
    Require(revalidate(&proof), "the unchanged image resources failed their initial proof");
    if (std::getenv("APS5_NO_FAST_REVALIDATE") == nullptr) {
        if (AgcDriver::GuestMemory::Watched(address, Side * Side * 4u)) Require(proof.path == ShaderResources::ProofPath::Fast, "the fixture did not exercise the fast image proof");
        else std::puts("image memory is compared, not watched: fast image proof not tested; checking the image guards through the full proof");
    }
    ColorTarget target{};
    target.address = address + BlockBytes;
    target.surfaceAddress = target.address;
    target.extent = {Side, Side};
    target.surfaceExtent = target.extent;
    target.format = VK_FORMAT_R8G8B8A8_UNORM;
    target.tileMode = ColorTileMode::RenderTarget;
    target.elementBytes = 4;
    target.bytes = Side * Side * 4u;
    target.samples = 4;
    Require(MultisampleTargetView(context, target) != VK_NULL_HANDLE, "the disjoint multisample target has no view");
    device.SubmitRecorded(true);
    device.WaitIdle();
    Require(revalidate(), "a disjoint multisample target prevented image reuse");
    target.address = address;
    target.surfaceAddress = address;
    Require(MultisampleTargetView(context, target) != VK_NULL_HANDLE, "the overlapping multisample target has no view");
    device.SubmitRecorded(true);
    device.WaitIdle();
    RequireColorGuard([&] { revalidate(); }, "a cached image access accepted memory owned by a multisampled color target");
    RequireColorGuard([&] { build(); }, "a fresh image access accepted memory owned by a multisampled color target");
    ClearMultisampleTargets(context.device);
    Require(revalidate(), "retiring the multisample targets did not restore image reuse");
}

void CheckMultisampleTable(AgcDriver::VulkanDevice& device) {
    namespace Abi = ShaderRecompiler::ImageTableAbi;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (context.imageTableCapacity == 0) {
        std::puts("skipped, native multisample table check needs image table support");
        return;
    }
    GuestBlock block;
    struct Cleanup {
        VkDevice device;
        ~Cleanup() { ClearMultisampleTargets(device); }
    } cleanup{context.device};
    ColorTarget target{};
    target.address = block.Address();
    target.surfaceAddress = target.address;
    target.extent = {Side, Side};
    target.surfaceExtent = target.extent;
    target.format = VK_FORMAT_R8G8B8A8_UNORM;
    target.tileMode = ColorTileMode::RenderTarget;
    target.elementBytes = 4;
    target.bytes = Side * Side * 4u;
    target.samples = 4;
    Require(MultisampleTargetView(context, target) != VK_NULL_HANDLE, "the native multisample table target has no view");
    device.SubmitRecorded(true);
    device.WaitIdle();
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding map{};
    map.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    map.role = ShaderRecompiler::DescriptorRole::ImageTableMap;
    map.binding = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::ImageTableMap);
    map.count = 1;
    map.guestDescriptor.resize(Abi::HeaderWords + 1u);
    map.guestDescriptor[Abi::HeaderVersion] = Abi::Version;
    map.guestDescriptor[Abi::HeaderElementCount] = 1u;
    map.guestDescriptor[Abi::HeaderElementFaults] = Abi::HeaderWords;
    map.guestDescriptor[Abi::HeaderSamplerFaults] = Abi::HeaderWords + 1u;
    program.bindings.push_back(map);
    ShaderRecompiler::DescriptorBinding image{};
    image.kind = ShaderRecompiler::DescriptorKind::SampledImage;
    image.role = ShaderRecompiler::DescriptorRole::ImageTable;
    image.binding = ShaderRecompiler::RuntimeAbi::ImageTableBinding;
    image.count = 1;
    image.imageShapes = {ShaderRecompiler::DescriptorImageShape::Image2DMultisample};
    image.imageDepthCompare = {false};
    image.imageWritten = {false};
    image.imageSamplers = {0u};
    const auto address = block.Address();
    image.guestDescriptor = {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (56u << 20u) | (((Side - 1u) & 3u) << 30u), ((Side - 1u) >> 2u) | ((Side - 1u) << 14u), 0xfacu | (2u << 16u) | (0x1bu << 20u) | (14u << 28u), 0u, 2u << 4u, 0u, 0u};
    program.bindings.push_back(image);
    const CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    ShaderResources resources(context, shader);
    Require(resources.SampledTextures().size() == 1u, "the native multisample table entry was rejected during construction");
    Require(resources.Reusable(), "the native multisample table did not reach resource validation");
    static_cast<void>(resources.Revalidate(shader));
    device.SubmitRecorded(true);
    device.WaitIdle();
    std::puts("native multisample table validation did not reject its color target");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        if (!AgcDriver::GuestMemory::WriteWatched() || (context.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0) {
            Require(std::getenv("ANYPS5_REQUIRE_VULKAN") == nullptr, "cached image guard tests require write tracking and 4x color targets");
            std::puts("skipped, cached image guard tests require write tracking and 4x color targets");
            return VulkanTestSkipped;
        }
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        for (const bool draw : {false, true}) {
            for (const bool sampled : {false, true}) CheckCachedImage(*device, sampled, draw);
        }
        CheckMultisampleTable(*device);
        std::puts("cached image multisample guard tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
