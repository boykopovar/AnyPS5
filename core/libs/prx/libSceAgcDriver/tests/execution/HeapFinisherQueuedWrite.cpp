#include "HeapStageSettled.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapStages.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <future>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

using AgcDriver::Graphics::Require;
using namespace ShaderRecompiler;

constexpr std::uint32_t Entries = 1024;
constexpr std::size_t BlockBytes = 65536;
constexpr std::array<std::uint32_t, 23> Code{0xbf068002u, 0xbf850013u, 0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u, 0x7e200500u, 0x93109010u,
                                             0xf4200406u, 0x20000004u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xf09c0f08u, 0x00450000u, 0xe0700000u, 0x80070000u, 0xbf810000u, 0xbf800000u};
alignas(256) std::array<std::array<std::uint32_t, 64>, 3> Textures{};
alignas(256) std::array<std::uint32_t, 64> Output{};
alignas(256) std::array<std::uint32_t, 16> Srt{};

std::array<std::uint32_t, 4> Buffer(const void* pointer, std::uint32_t stride, std::uint32_t records) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) | (stride << 16u), records, stride == 0u ? 0x31016facu : 0x01016facu};
}

std::array<std::uint32_t, 8> Texture(std::uint32_t texture) {
    const auto address = reinterpret_cast<std::uintptr_t>(Textures[texture].data());
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u) | ((texture == 1u ? 22u : 20u) << 20u) | (3u << 30u), 7u, 0x90000facu, 0u, 0u, 0u, 0u};
}

std::uint8_t* WatchedPages(std::size_t bytes) {
#ifdef _WIN32
    auto* data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockBytes));
    Require(data != nullptr, "heap finisher queued write: cannot allocate the table pages");
    GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, bytes);
    return data;
#else
    void* mapped = mmap(nullptr, bytes + BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapped != MAP_FAILED, "heap finisher queued write: cannot map the table pages");
    auto* data = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(mapped) + BlockBytes - 1u) & ~std::uintptr_t{BlockBytes - 1u});
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, bytes);
    return data;
#endif
}

void Run(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::DriverDetail;
    constexpr std::size_t TableBytes = BlockBytes;
    auto* base = WatchedPages(TableBytes * 2u);
    auto* table = reinterpret_cast<std::uint32_t*>(base);
    auto* material = reinterpret_cast<std::uint32_t*>(base + TableBytes);
    for (std::uint32_t entry = 0; entry < Entries; ++entry) {
        const auto descriptor = Texture(entry % 2u);
        std::copy(descriptor.begin(), descriptor.end(), table + entry * 8u);
        material[entry * 4u + 1u] = entry;
    }
    const auto tableBuffer = Buffer(table, 32u, Entries);
    const auto materialBuffer = Buffer(material, 16u, Entries);
    const auto output = Buffer(Output.data(), 0u, 256u);
    std::copy(tableBuffer.begin(), tableBuffer.end(), Srt.begin());
    std::copy(materialBuffer.begin(), materialBuffer.end(), Srt.begin() + 8u);
    std::copy(output.begin(), output.end(), Srt.begin() + 12u);
    const auto srtAddress = reinterpret_cast<std::uintptr_t>(Srt.data());
    const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u), 1u};
    const std::array regions{MemoryRegion{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span(Code))}};
    const ShaderComputeStageInfo compute{{std::min(32u, device.Target().subgroupSize), 1u, 1u}, 0u, {false, false, false}, false, 1u};
    auto target = device.Target();
    target.gpuResolvedImages = true;
    std::vector<MemoryRegion> prepared(regions.begin(), regions.end());
    RecompileRequest request{{ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0u, {}}, {32u, 0u, userData, compute, std::nullopt, std::nullopt, prepared}, target, {0u, 0u, 0u, 128u}};
    const auto handle = PrepareShader(request);
    const auto invocation = PreparedShaderInvocation::TryCreate(request, handle);
    Require(invocation.has_value(), "heap finisher queued write: the prepared table shader refused its own request");
    const auto walk = [&] { return MaterializeHeapStageSettled(*invocation, userData, regions, request.shader.codeAddress); };

    const auto settled = walk();
    Require(settled.result != nullptr && settled.reads.complete, "heap finisher queued write: the heap walk declined the table stage with nothing queued");

    const auto tableBegin = reinterpret_cast<std::uint64_t>(table);
    std::promise<void> release;
    const auto released = release.get_future().share();
    DrawPipeline::Queue0().Enqueue([released] { released.wait(); }, {{tableBegin + 5u * 8u * sizeof(std::uint32_t), tableBegin + 6u * 8u * sizeof(std::uint32_t)}});
    Require(DrawPipeline::Queue0().OverlapsAny(std::array{std::pair{tableBegin, tableBegin + TableBytes}}), "heap finisher queued write: the queued descriptor table write is not listed");

    HeapStageMaterialized onFinisher;
    bool ranOnFinisher = false;
    std::exception_ptr failure;
    DrawFinisher::Get().Enqueue([&] {
        ranOnFinisher = DrawFinisher::OnFinisherThread();
        try {
            onFinisher = walk();
        } catch (...) {
            failure = std::current_exception();
        }
    });
    DrawFinisher::Get().Drain();
    release.set_value();
    DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Packet);
    if (failure != nullptr) std::rethrow_exception(failure);
    Require(ranOnFinisher, "heap finisher queued write: the walk did not run on the finisher thread");
    Require(onFinisher.result == nullptr, "heap finisher queued write: a finisher walk read a descriptor table that a queued write overlaps");

    const auto after = walk();
    Require(after.result != nullptr && after.reads.complete, "heap finisher queued write: the heap walk declined the table stage once the write retired");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!AgcDriver::Graphics::DescriptorHeapEnabled()) return VulkanTestSkipped;
        Run(*device);
        std::cout << "heap finisher queued write tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
