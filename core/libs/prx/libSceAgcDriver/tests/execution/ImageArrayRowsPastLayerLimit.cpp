#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "IntermediateRepresentation/IrMetadata/ResourcePlan.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 8;
constexpr std::uint32_t MaxGuestLayers = 8192;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type1DArray = 12;
constexpr std::uint32_t Wrap = 0;
constexpr std::uint32_t ClampEdge = 2;
constexpr std::uint32_t Bilinear = (1u << 20u) | (1u << 22u);

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<float, Threads * 8> Output{};
constexpr std::size_t TexelBytes = 4u << 20u;
constexpr std::size_t TexelAlignment = 65536;
std::array<std::uint8_t, TexelBytes + TexelAlignment - 1> TexelStorage{};
const std::span<std::uint8_t, TexelBytes> Texels{reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(TexelStorage.data()) + TexelAlignment - 1) & ~(TexelAlignment - 1)), TexelBytes};

alignas(256) constexpr std::array<std::uint32_t, 11> SampleLevelZero{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf09c0f20, 0x00820602, 0x34020085, 0xbf8c3f70,
    0xe0781000, 0x80010601, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> SampleLevel{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf0900f20, 0x00820602, 0x34020085, 0xbf8c3f70,
    0xe0781000, 0x80010601, 0xbf810000,
};

std::uint32_t Layers = 0;

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t baseArray) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Width - 1u) & 3u) << 30u),
        (Width - 1u) >> 2u,
        0xfacu | (Type1DArray << 28u),
        (Layers - 1u) | (baseArray << 16u), 0u, 0u, 0u,
    };
}

std::array<std::uint8_t, 4> Texel(std::uint32_t x, std::uint32_t layer) {
    return {static_cast<std::uint8_t>(x * 30u + layer % 5u), static_cast<std::uint8_t>((layer & 1u) != 0u ? 255u : 0u), static_cast<std::uint8_t>(layer), 0xff};
}

void FillTexels() {
    std::ranges::fill(Texels, std::uint8_t{0});
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(0)));
    const auto& mip = surface.mips.at(0);
    Require(surface.layers == Layers && surface.guestBytes <= Texels.size(), "the test array does not fit its storage");
    for (std::uint32_t layer = 0; layer < Layers; ++layer) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto texel = Texel(x, layer);
            std::copy(texel.begin(), texel.end(), Texels.begin() + static_cast<std::ptrdiff_t>(surface.GuestLayerOffset(layer) + mip.tiledOffset + x * 4u));
        }
    }
}

void FillInput(std::uint32_t limit) {
    const auto last = static_cast<float>(Layers - 1u);
    const std::array<float, 16> layers{0.0f, 1.0f, 2.4f, 1.6f, -1.0f, -0.4f, 0.4f, last, last + 0.4f, last + 1.0f, last + 7.0f, 1.0e6f, -1.0e6f, static_cast<float>(limit - 1u), static_cast<float>(limit), static_cast<float>(limit) + 0.6f};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * 4u + 0u] = std::bit_cast<std::uint32_t>(-0.2f + 0.045f * static_cast<float>(tid));
        Input[tid * 4u + 1u] = std::bit_cast<std::uint32_t>(layers[tid % layers.size()]);
        Input[tid * 4u + 2u] = std::bit_cast<std::uint32_t>(static_cast<float>(tid % 3u) * 1.7f);
        Input[tid * 4u + 3u] = 0u;
    }
}

float Unorm(std::uint8_t value) {
    return static_cast<float>(value);
}

class RegisteredTextureRange {
public:
    RegisteredTextureRange(void* address, std::size_t bytes) : address(address) {
        GuestAllocations::Mutation mutation;
        mutation.Add(address, bytes, true, false, true);
    }

    ~RegisteredTextureRange() {
        Unregister();
    }

    void Resize(std::size_t bytes) {
        GuestAllocations::Mutation mutation;
        if (registered) mutation.Remove(address);
        mutation.Add(address, bytes, true, false, true);
        registered = true;
    }

    void Unregister() {
        if (!registered) return;
        GuestAllocations::Mutation mutation;
        mutation.Remove(address);
        registered = false;
    }

    RegisteredTextureRange(const RegisteredTextureRange&) = delete;
    RegisteredTextureRange& operator=(const RegisteredTextureRange&) = delete;

private:
    void* address;
    bool registered = true;
};

int CaptureStillRowsRegisteredWindow(AgcDriver::VulkanDevice& device, std::uint32_t limit) {
    constexpr std::uint32_t baseArray = 1u;
    if (limit > MaxGuestLayers - 3u) {
        std::puts("still-rows: skipped because the device layer limit leaves no guest descriptor headroom");
        return VulkanTestSkipped;
    }
    if (2u * (limit + 1u) > device.graphicsContext().limits.maxImageDimension2D) {
        std::puts("still-rows: skipped because the registered row geometry exceeds maxImageDimension2D");
        return VulkanTestSkipped;
    }
    Layers = limit + 3u;
    const auto texture = TextureDescriptor(baseArray);
    const auto rawResource = AgcDriver::Graphics::DecodeTextureResource(texture);
    const auto rawGeometry = AgcDriver::Graphics::DescribeSurface(rawResource);
    const auto rawViewLayers = rawGeometry.imageLayers - rawResource.baseArray;
    Require(rawResource.baseArray == baseArray && rawViewLayers == limit + 2u, "still-rows: the raw descriptor does not cover L+2 slices from baseArray 1");
    Require(rawGeometry.layerBytes != 0u, "still-rows: the descriptor has no complete layer size");
    if (rawGeometry.guestBytes > Texels.size()) {
        std::puts("still-rows: skipped because the full descriptor geometry exceeds the 4 MiB test backing");
        return VulkanTestSkipped;
    }
    const auto registeredBytes = static_cast<std::size_t>(rawGeometry.layerBytes * (limit + 2u));
    if (registeredBytes > Texels.size()) {
        std::puts("still-rows: skipped because the exact registered complete-slice range exceeds the 4 MiB test backing");
        return VulkanTestSkipped;
    }
    Require(reinterpret_cast<std::uintptr_t>(Texels.data()) % TexelAlignment == 0u, "still-rows: the descriptor base is not 64 KiB aligned");
    FillTexels();
    FillInput(limit + 1u);
    RegisteredTextureRange registered(Texels.data(), registeredBytes);
    const auto range = GuestAllocations::GuestAllocationsLookup_nid_postfix(reinterpret_cast<std::uintptr_t>(Texels.data()), false);
    Require(range != nullptr && range->address == reinterpret_cast<std::uintptr_t>(Texels.data()) && range->bytes == registeredBytes && range->readable && !range->writable && range->gpu, "still-rows: lookup did not return the exact readable registered extent");
    const auto completeRegisteredSlices = registeredBytes / rawGeometry.layerBytes;
    Require(completeRegisteredSlices == limit + 2u, "still-rows: the registered extent does not contain the expected complete slices");
    const auto expectedLastArray = static_cast<std::uint32_t>(completeRegisteredSlices - 1u);
    const auto boundedRows = expectedLastArray - baseArray + 1u;
    Require(expectedLastArray == limit + 1u && boundedRows == limit + 1u, "still-rows: the independent complete-range oracle did not produce the expected L+1 view");
    std::vector<std::uint32_t> userData(20, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const std::array<std::uint32_t, 4> sampler{ClampEdge | (ClampEdge << 3u) | (ClampEdge << 6u), 0u, 0u, 0u};
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(SampleLevelZero.data()), std::as_bytes(std::span(SampleLevelZero))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(SampleLevelZero.data()), SampleLevelZero, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{reinterpret_cast<std::uintptr_t>(SampleLevelZero.data()), 0, 0, {SampleLevelZero.begin(), SampleLevelZero.end()}, {}};
    request.shader.code = snapshot.code;
    auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request);
    AgcDriver::ShaderMemory shaderMemory(request.context.memory);
    const auto capture = AgcDriver::DriverDetail::CaptureInvocation(shaderMemory, snapshot, 0, request, invocation);
    Require(capture->snapshot.images.size() == 1u && capture->plan->info.images.size() == 1u && capture->plan->info.runtimeImageModes.size() == 1u, "still-rows: capture did not resolve exactly one sampled image");
    const auto& capturedDescriptor = capture->snapshot.images.front();
    const auto capturedResource = AgcDriver::Graphics::DecodeTextureResource(std::span(capturedDescriptor.dwords).first(capturedDescriptor.dwordCount));
    const auto capturedGeometry = AgcDriver::Graphics::DescribeSurface(capturedResource);
    const auto& image = capture->plan->info.images.front();
    const auto& modes = capture->plan->info.runtimeImageModes.front();
    const auto modeIndex = ShaderRecompiler::ResourceMaterializer::RuntimeImageMode(image, capturedDescriptor, modes);
    const auto& selectedMode = modes.at(modeIndex);
    Require(selectedMode.layerRows, "still-rows: the over-limit effective view did not select layerRows mode");
    Require(capture->snapshot.registeredImages.size() == 1u && capture->snapshot.registeredImages.front().provenance.dwords == texture && capture->snapshot.registeredImages.front().effective == capturedDescriptor && capture->snapshot.registeredImages.front().rangeBytes == registeredBytes, "still-rows: capture identity lost original provenance, effective geometry, or the registered extent");
    Require(capturedResource.baseArray == baseArray && capturedResource.depthOrLastArray == expectedLastArray && capturedGeometry.imageLayers - capturedResource.baseArray == boundedRows, "still-rows: captured descriptor geometry differs from the independent complete registered window before dispatch");
    const auto expectedRowHeight = boundedRows * 2u;
    Require(expectedRowHeight <= device.graphicsContext().limits.maxImageDimension2D, "still-rows: the independent bound row height exceeds maxImageDimension2D");
    const auto result = invocation.Materialize(*capture);
    auto prepared = device.PrepareDispatch(*result, {}, Threads, reinterpret_cast<std::uintptr_t>(SampleLevelZero.data()), std::nullopt, &capture->snapshot.registeredImages);
    device.Dispatch(*result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(SampleLevelZero.data()), std::move(prepared), nullptr, std::nullopt, &capture->snapshot.registeredImages);
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto logicalLayer = static_cast<std::uint32_t>(std::clamp(std::nearbyint(std::bit_cast<float>(Input[tid * 4u + 1u])), 0.0f, static_cast<float>(limit)));
        const auto layer = baseArray + logicalLayer;
        const auto x = static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::floor(std::bit_cast<float>(Input[tid * 4u + 0u]) * static_cast<float>(Width))), 0, static_cast<int>(Width) - 1));
        const auto expected = Texel(x, layer);
        for (std::uint32_t component = 0; component < expected.size(); ++component) Require(std::fabs(Output[tid * 8u + component] * 255.0f - Unorm(expected[component])) <= 1.0f, "still-rows: first/last/beyond-limit ClampEdge samples disagree with the existing Texel oracle");
    }
    return 0;
}

void CaptureRegisteredWindow(AgcDriver::VulkanDevice& device, std::uint32_t limit) {
    constexpr std::uint32_t baseArray = 1u;
    Layers = limit + 2u;
    const auto texture = TextureDescriptor(baseArray);
    const auto rawResource = AgcDriver::Graphics::DecodeTextureResource(texture);
    const auto rawGeometry = AgcDriver::Graphics::DescribeSurface(rawResource);
    const auto rawViewLayers = rawGeometry.imageLayers - rawResource.baseArray;
    Require(rawResource.baseAddress == reinterpret_cast<std::uintptr_t>(Texels.data()) && rawResource.baseArray == baseArray && rawResource.format == Format8888UNorm, "registered window: the decoded descriptor lost its aligned sampled-image base, baseArray, or format");
    Require(rawViewLayers == limit + 1u, "registered window: the raw descriptor is not maxImageArrayLayers + 1 from baseArray 1");
    Require(rawGeometry.layerBytes != 0u && rawGeometry.guestBytes <= Texels.size() && rawGeometry.layerBytes * (baseArray + limit) <= Texels.size(), "registered window: the complete raw geometry or bounded complete-slice range does not fit the real backing");
    Require(reinterpret_cast<std::uintptr_t>(Texels.data()) % TexelAlignment == 0u, "registered window: the descriptor base is not 64 KiB aligned");

    const auto registeredBytes = static_cast<std::size_t>(rawGeometry.layerBytes * (baseArray + limit));
    const auto generationBefore = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    RegisteredTextureRange registered(Texels.data(), registeredBytes);
    const auto generationAfter = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    Require(generationAfter > generationBefore, "registered window: adding the slice range did not advance the allocation generation");
    {
        const auto range = GuestAllocations::GuestAllocationsLookup_nid_postfix(reinterpret_cast<std::uintptr_t>(Texels.data()), false);
        Require(range != nullptr && range->address == reinterpret_cast<std::uintptr_t>(Texels.data()) && range->bytes == registeredBytes && range->readable && !range->writable && range->gpu, "registered window: lookup did not return the one readable sampled-image extent");
        Require(range->allocationAddress == range->address && range->allocationBytes == registeredBytes, "registered window: lookup allocation extent differs from the registered slice range");
        const auto registeredViewLayers = range->bytes / rawGeometry.layerBytes - rawResource.baseArray;
        Require(registeredViewLayers == limit, "registered window: the actual registered extent does not bound the descriptor view to the device limit");
    }

    std::vector<std::uint32_t> userData(20, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const std::array<std::uint32_t, 4> sampler{ClampEdge | (ClampEdge << 3u) | (ClampEdge << 6u), 0u, 0u, 0u};
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(SampleLevelZero.data()), std::as_bytes(std::span(SampleLevelZero))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(SampleLevelZero.data()), SampleLevelZero, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{reinterpret_cast<std::uintptr_t>(SampleLevelZero.data()), 0, 0, {SampleLevelZero.begin(), SampleLevelZero.end()}, {}};
    request.shader.code = snapshot.code;
    auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request);
    Require(!invocation.Request().context.layerRowSamples, "registered window: the fresh invocation already has layer-row sampling enabled");
    const auto beforeCapture = snapshot.prepared->entries.size();
    Require(beforeCapture == 1u, "registered window: the fresh snapshot did not prepare exactly one initial native invocation");
    AgcDriver::ShaderMemory shaderMemory(request.context.memory);
    const auto capture = AgcDriver::DriverDetail::CaptureInvocation(shaderMemory, snapshot, 0, request, invocation);
    Require(!invocation.Request().context.layerRowSamples, "registered window: capture promoted the sampled array to layer rows before dispatch");
    Require(snapshot.prepared->entries.size() == beforeCapture, "registered window: capture added a prepared mode while selecting the effective native view");
    Require(capture->snapshot.images.size() == 1u && capture->plan->info.images.size() == 1u && capture->plan->info.runtimeImageModes.size() == 1u, "registered window: capture did not resolve exactly one sampled image");

    const auto& capturedDescriptor = capture->snapshot.images.front();
    Require(capturedDescriptor.dwordCount == 8u, "registered window: capture did not retain a complete image descriptor");
    const auto capturedResource = AgcDriver::Graphics::DecodeTextureResource(std::span(capturedDescriptor.dwords).first(capturedDescriptor.dwordCount));
    const auto capturedGeometry = AgcDriver::Graphics::DescribeSurface(capturedResource);
    Require(capturedResource.baseArray == baseArray && capturedGeometry.imageLayers - capturedResource.baseArray == limit, "registered window: captured descriptor window was not normalized to the complete registered slice extent");
    const auto& image = capture->plan->info.images.front();
    const auto& modes = capture->plan->info.runtimeImageModes.front();
    const auto modeIndex = ShaderRecompiler::ResourceMaterializer::RuntimeImageMode(image, capturedDescriptor, modes);
    const auto& selectedMode = modes.at(modeIndex);
    Require(selectedMode.dimension == ShaderRecompiler::RdnaImageDimension::Dim1DArray && !selectedMode.layerRows, "registered window: the normalized capture did not select the native 1D-array mode");
    Require(capture->snapshot.registeredImages.size() == 1u && capture->snapshot.registeredImages.front().provenance.dwords == texture, "registered window: capture did not retain the original descriptor provenance");
    Require(capture->snapshot.registeredImages.front().effective == capturedDescriptor && capture->snapshot.registeredImages.front().generation == generationAfter && capture->snapshot.registeredImages.front().rangeBytes == registeredBytes, "registered window: capture identity does not match the effective view's registered range");
    Require(capture->snapshot.imageAllocationGeneration == generationAfter, "registered window: capture did not bind the stable allocation generation");
    FillTexels();
    FillInput(limit);
    auto cachedRequest = request;
    cachedRequest.useCache = true;
    auto cachedInvocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, cachedRequest);
    AgcDriver::ShaderMemory cachedMemory(cachedRequest.context.memory);
    const auto cachedCapture = AgcDriver::DriverDetail::CaptureInvocation(cachedMemory, snapshot, 0, cachedRequest, cachedInvocation);
    const auto cachedResult = cachedInvocation.Materialize(*cachedCapture);
    Require(cachedResult == cachedInvocation.Materialize(*cachedCapture), "registered window: identical registered captures did not reuse the cached materialization");
    const auto dispatchCaptured = [&](const auto& result, const auto& currentCapture) {
        auto prepared = device.PrepareDispatch(*result, {}, Threads, reinterpret_cast<std::uintptr_t>(SampleLevelZero.data()), std::nullopt, &currentCapture->snapshot.registeredImages);
        device.Dispatch(*result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(SampleLevelZero.data()), std::move(prepared), nullptr, std::nullopt, &currentCapture->snapshot.registeredImages);
        device.WaitIdle();
    };
    dispatchCaptured(cachedResult, cachedCapture);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto layer = baseArray + static_cast<std::uint32_t>(std::clamp(std::nearbyint(std::bit_cast<float>(Input[tid * 4u + 1u])), 0.0f, static_cast<float>(limit - 1u)));
        const auto x = static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::floor(std::bit_cast<float>(Input[tid * 4u + 0u]) * static_cast<float>(Width))), 0, static_cast<int>(Width) - 1));
        const auto expected = Texel(x, layer);
        for (std::uint32_t component = 0; component < expected.size(); ++component) Require(std::fabs(Output[tid * 8u + component] * 255.0f - Unorm(expected[component])) <= 1.0f, "registered window: bound native image geometry sampled outside its registered extent");
    }
    const auto stableView = cachedCapture->snapshot.registeredImages.front();
    std::array<std::uint32_t, 4> unrelatedStorage{};
    const auto stableGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    std::shared_ptr<const ShaderRecompiler::RecompileResult> unchangedResult;
    std::shared_ptr<const ShaderRecompiler::ResourceCapture> unchangedCapture;
    {
        RegisteredTextureRange unrelated(unrelatedStorage.data(), sizeof(unrelatedStorage));
        const auto unrelatedGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        Require(unrelatedGeneration > stableGeneration, "registered window: the unrelated mapping did not advance the allocation generation");
        auto unchangedInvocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, cachedRequest);
        AgcDriver::ShaderMemory unchangedMemory(cachedRequest.context.memory);
        unchangedCapture = AgcDriver::DriverDetail::CaptureInvocation(unchangedMemory, snapshot, 0, cachedRequest, unchangedInvocation);
        const auto& unchangedView = unchangedCapture->snapshot.registeredImages.front();
        Require(unchangedView.provenance == stableView.provenance && unchangedView.effective == stableView.effective && unchangedView.address == stableView.address && unchangedView.rangeAddress == stableView.rangeAddress && unchangedView.rangeBytes == stableView.rangeBytes && unchangedView.allocationAddress == stableView.allocationAddress && unchangedView.allocationBytes == stableView.allocationBytes && unchangedView.backingSerial == stableView.backingSerial && unchangedView.hasRange == stableView.hasRange && unchangedView.readable == stableView.readable && unchangedView.writable == stableView.writable && unchangedView.gpu == stableView.gpu, "registered window: an unrelated mapping changed the unchanged sampled-image range facts");
        Require(!unchangedInvocation.Request().context.layerRowSamples && snapshot.prepared->entries.size() == beforeCapture, "registered window: an unrelated generation bump promoted or prepared a row-mode artifact for the unchanged range");
        const auto unchangedMode = ShaderRecompiler::ResourceMaterializer::RuntimeImageMode(unchangedCapture->plan->info.images.front(), unchangedCapture->snapshot.images.front(), unchangedCapture->plan->info.runtimeImageModes.front());
        Require(unchangedCapture->plan->info.runtimeImageModes.front().at(unchangedMode).dimension == ShaderRecompiler::RdnaImageDimension::Dim1DArray && !unchangedCapture->plan->info.runtimeImageModes.front().at(unchangedMode).layerRows, "registered window: an unrelated generation bump changed the unchanged native image mode");
        unchangedResult = unchangedInvocation.Materialize(*unchangedCapture);
        Require(unchangedResult == cachedResult, "registered window: an unrelated generation bump missed the warm cached materialization for unchanged range identity");
    }
    const auto removedUnrelatedGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    Require(removedUnrelatedGeneration > stableGeneration, "registered window: removing the unrelated mapping did not advance the allocation generation");
    auto removedUnrelatedInvocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, cachedRequest);
    AgcDriver::ShaderMemory removedUnrelatedMemory(cachedRequest.context.memory);
    const auto removedUnrelatedCapture = AgcDriver::DriverDetail::CaptureInvocation(removedUnrelatedMemory, snapshot, 0, cachedRequest, removedUnrelatedInvocation);
    const auto& removedUnrelatedView = removedUnrelatedCapture->snapshot.registeredImages.front();
    Require(removedUnrelatedView.effective == stableView.effective && removedUnrelatedView.rangeAddress == stableView.rangeAddress && removedUnrelatedView.rangeBytes == stableView.rangeBytes && removedUnrelatedView.backingSerial == stableView.backingSerial, "registered window: removing an unrelated mapping changed the sampled-image range identity");
    Require(!removedUnrelatedInvocation.Request().context.layerRowSamples && snapshot.prepared->entries.size() == beforeCapture, "registered window: removing an unrelated mapping promoted or prepared a row-mode artifact for the unchanged range");
    const auto removedUnrelatedResult = removedUnrelatedInvocation.Materialize(*removedUnrelatedCapture);
    Require(removedUnrelatedResult == cachedResult, "registered window: removing an unrelated mapping missed the warm cached materialization for unchanged range identity");
    dispatchCaptured(unchangedResult, unchangedCapture);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto layer = baseArray + static_cast<std::uint32_t>(std::clamp(std::nearbyint(std::bit_cast<float>(Input[tid * 4u + 1u])), 0.0f, static_cast<float>(limit - 1u)));
        const auto x = static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::floor(std::bit_cast<float>(Input[tid * 4u + 0u]) * static_cast<float>(Width))), 0, static_cast<int>(Width) - 1));
        const auto expected = Texel(x, layer);
        for (std::uint32_t component = 0; component < expected.size(); ++component) Require(std::fabs(Output[tid * 8u + component] * 255.0f - Unorm(expected[component])) <= 1.0f, "registered window: an unrelated registration altered the bound native image result");
    }
    dispatchCaptured(removedUnrelatedResult, removedUnrelatedCapture);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto layer = baseArray + static_cast<std::uint32_t>(std::clamp(std::nearbyint(std::bit_cast<float>(Input[tid * 4u + 1u])), 0.0f, static_cast<float>(limit - 1u)));
        const auto x = static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::floor(std::bit_cast<float>(Input[tid * 4u + 0u]) * static_cast<float>(Width))), 0, static_cast<int>(Width) - 1));
        const auto expected = Texel(x, layer);
        for (std::uint32_t component = 0; component < expected.size(); ++component) Require(std::fabs(Output[tid * 8u + component] * 255.0f - Unorm(expected[component])) <= 1.0f, "registered window: an unrelated generation change altered the bound native image result");
    }
    if (limit > 1u) {
        std::uint64_t previousSerial = 0;
        {
            const auto range = GuestAllocations::GuestAllocationsLookup_nid_postfix(reinterpret_cast<std::uintptr_t>(Texels.data()), false);
            previousSerial = range->backingSerial;
        }
        registered.Resize(registeredBytes);
        const auto sameExtentGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        {
            const auto sameExtentRange = GuestAllocations::GuestAllocationsLookup_nid_postfix(reinterpret_cast<std::uintptr_t>(Texels.data()), false);
            Require(sameExtentGeneration != generationAfter && sameExtentRange != nullptr && sameExtentRange->backingSerial != previousSerial, "registered window: same-extent registration replacement did not change allocation identity");
        }
        auto sameExtentInvocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, cachedRequest);
        AgcDriver::ShaderMemory sameExtentMemory(cachedRequest.context.memory);
        const auto sameExtentCapture = AgcDriver::DriverDetail::CaptureInvocation(sameExtentMemory, snapshot, 0, cachedRequest, sameExtentInvocation);
        const auto sameExtentResult = sameExtentInvocation.Materialize(*sameExtentCapture);
        Require(sameExtentCapture->snapshot.registeredImages.front().effective == cachedCapture->snapshot.registeredImages.front().effective && sameExtentCapture->snapshot.registeredImages.front().backingSerial != cachedCapture->snapshot.registeredImages.front().backingSerial, "registered window: same-extent replacement lost descriptor equality or allocation identity");
        Require(sameExtentResult != cachedResult, "registered window: allocation-only change reused a stale cached materialization");
        dispatchCaptured(sameExtentResult, sameExtentCapture);
        registered.Resize(static_cast<std::size_t>(rawGeometry.layerBytes * (baseArray + limit - 1u)));
        const auto resizedGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        std::size_t resizedBytes = 0;
        {
            const auto resizedRange = GuestAllocations::GuestAllocationsLookup_nid_postfix(reinterpret_cast<std::uintptr_t>(Texels.data()), false);
            Require(resizedGeneration != generationAfter && resizedRange != nullptr && resizedRange->bytes < registeredBytes, "registered window: resizing the registration did not change its identity");
            resizedBytes = resizedRange->bytes;
        }
        auto resizedInvocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, cachedRequest);
        AgcDriver::ShaderMemory resizedMemory(cachedRequest.context.memory);
        const auto resizedCapture = AgcDriver::DriverDetail::CaptureInvocation(resizedMemory, snapshot, 0, cachedRequest, resizedInvocation);
        Require(!resizedInvocation.Request().context.layerRowSamples && snapshot.prepared->entries.size() == beforeCapture, "registered window: allocation-only change promoted or added a row-mode artifact");
        Require(resizedCapture->snapshot.imageAllocationGeneration == resizedGeneration, "registered window: recapture reused a stale allocation generation");
        Require(resizedCapture->snapshot.registeredImages.size() == 1u && resizedCapture->snapshot.registeredImages.front().generation == resizedGeneration && resizedCapture->snapshot.registeredImages.front().rangeBytes == resizedBytes, "registered window: recapture reused stale registered-view identity");
        Require(resizedCapture->snapshot.registeredImages.front().provenance == capture->snapshot.registeredImages.front().provenance, "registered window: allocation-only change altered original descriptor provenance");
        Require(resizedCapture->snapshot.registeredImages.front().effective != capture->snapshot.registeredImages.front().effective, "registered window: allocation extent change did not change effective descriptor identity");
        const auto resizedResource = AgcDriver::Graphics::DecodeTextureResource(resizedCapture->snapshot.images.front().dwords);
        const auto resizedGeometry = AgcDriver::Graphics::DescribeSurface(resizedResource);
        Require(resizedResource.baseArray == baseArray && resizedGeometry.imageLayers - resizedResource.baseArray == limit - 1u, "registered window: recaptured physical geometry disagrees with the effective registered extent");
        const auto resizedMode = ShaderRecompiler::ResourceMaterializer::RuntimeImageMode(resizedCapture->plan->info.images.front(), resizedCapture->snapshot.images.front(), resizedCapture->plan->info.runtimeImageModes.front());
        Require(resizedCapture->plan->info.runtimeImageModes.front().at(resizedMode).dimension == ShaderRecompiler::RdnaImageDimension::Dim1DArray && !resizedCapture->plan->info.runtimeImageModes.front().at(resizedMode).layerRows, "registered window: resized native view changed to a layer-row mode");
        const auto resizedResult = resizedInvocation.Materialize(*resizedCapture);
        Require(resizedResult != sameExtentResult, "registered window: changed complete-slice geometry reused a stale cached materialization");
        dispatchCaptured(resizedResult, resizedCapture);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto layer = baseArray + static_cast<std::uint32_t>(std::clamp(std::nearbyint(std::bit_cast<float>(Input[tid * 4u + 1u])), 0.0f, static_cast<float>(limit - 2u)));
            const auto x = static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::floor(std::bit_cast<float>(Input[tid * 4u + 0u]) * static_cast<float>(Width))), 0, static_cast<int>(Width) - 1));
            const auto expected = Texel(x, layer);
            for (std::uint32_t component = 0; component < expected.size(); ++component) Require(std::fabs(Output[tid * 8u + component] * 255.0f - Unorm(expected[component])) <= 1.0f, "registered window: resized bound geometry disagrees with the current registered slice extent");
        }
        registered.Resize(rawGeometry.guestBytes);
        const auto fullRangeGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        auto fullRowInvocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, cachedRequest);
        AgcDriver::ShaderMemory fullRowMemory(cachedRequest.context.memory);
        const auto fullRowCapture = AgcDriver::DriverDetail::CaptureInvocation(fullRowMemory, snapshot, 0, cachedRequest, fullRowInvocation);
        Require(fullRowInvocation.Request().context.layerRowSamples, "registered window: complete over-limit registration did not produce the full-row mode");
        Require(fullRowCapture->snapshot.imageAllocationGeneration == fullRangeGeneration && fullRowCapture->snapshot.registeredImages.size() == 1u && fullRowCapture->snapshot.registeredImages.front().provenance.dwords == texture, "registered window: full-row capture lost the unchanged descriptor or current allocation generation");
        const auto fullRowResource = AgcDriver::Graphics::DecodeTextureResource(fullRowCapture->snapshot.images.front().dwords);
        const auto fullRowGeometry = AgcDriver::Graphics::DescribeSurface(fullRowResource);
        Require(fullRowResource.baseArray == baseArray && fullRowGeometry.imageLayers - fullRowResource.baseArray == limit + 1u, "registered window: full-row capture did not retain the complete over-limit guest view");
        const auto fullRowMode = ShaderRecompiler::ResourceMaterializer::RuntimeImageMode(fullRowCapture->plan->info.images.front(), fullRowCapture->snapshot.images.front(), fullRowCapture->plan->info.runtimeImageModes.front());
        Require(fullRowCapture->plan->info.runtimeImageModes.front().at(fullRowMode).layerRows, "registered window: complete over-limit capture did not select layer-row mode");
        const auto fullRowResult = fullRowInvocation.Materialize(*fullRowCapture);
        Require(fullRowResult == fullRowInvocation.Materialize(*fullRowCapture), "registered window: identical full-row captures did not reuse the cached materialization");
        Require(fullRowResult != resizedResult, "registered window: full-row transition reused the bounded-native cached materialization");
        const auto fullRowPreparedEntries = snapshot.prepared->entries.size();
        registered.Resize(registeredBytes);
        const auto reboundGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        auto reboundInvocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, cachedRequest);
        AgcDriver::ShaderMemory reboundMemory(cachedRequest.context.memory);
        const auto reboundCapture = AgcDriver::DriverDetail::CaptureInvocation(reboundMemory, snapshot, 0, cachedRequest, reboundInvocation);
        Require(reboundCapture->snapshot.imageAllocationGeneration == reboundGeneration && reboundCapture->snapshot.registeredImages.size() == 1u, "registered window: native recapture lost the current bounded allocation generation");
        const auto& reboundView = reboundCapture->snapshot.registeredImages.front();
        Require(reboundView.provenance.dwords == fullRowCapture->snapshot.registeredImages.front().provenance.dwords && reboundView.provenance.dwords == texture && reboundView.effective != fullRowCapture->snapshot.registeredImages.front().effective && reboundView.backingSerial != fullRowCapture->snapshot.registeredImages.front().backingSerial && reboundView.rangeBytes == registeredBytes, "registered window: bounded native recapture reused full-row range identity or changed raw descriptor provenance");
        const auto reboundResource = AgcDriver::Graphics::DecodeTextureResource(reboundCapture->snapshot.images.front().dwords);
        const auto reboundGeometry = AgcDriver::Graphics::DescribeSurface(reboundResource);
        Require(reboundResource.baseArray == baseArray && reboundGeometry.imageLayers - reboundResource.baseArray == limit, "registered window: bounded native recapture did not restore the device-limit view geometry");
        const auto reboundMode = ShaderRecompiler::ResourceMaterializer::RuntimeImageMode(reboundCapture->plan->info.images.front(), reboundCapture->snapshot.images.front(), reboundCapture->plan->info.runtimeImageModes.front());
        Require(reboundCapture->plan->info.runtimeImageModes.front().at(reboundMode).dimension == ShaderRecompiler::RdnaImageDimension::Dim1DArray && !reboundCapture->plan->info.runtimeImageModes.front().at(reboundMode).layerRows, "registered window: bounded native recapture did not select native 1D-array mode");
        Require(snapshot.prepared->entries.size() == fullRowPreparedEntries, "registered window: bounded native recovery added an unnecessary prepared mode");
        const auto reboundResult = reboundInvocation.Materialize(*reboundCapture);
        Require(reboundResult != fullRowResult, "registered window: bounded native recapture reused the full-row materialization");
        dispatchCaptured(reboundResult, reboundCapture);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto layer = baseArray + static_cast<std::uint32_t>(std::clamp(std::nearbyint(std::bit_cast<float>(Input[tid * 4u + 1u])), 0.0f, static_cast<float>(limit - 1u)));
            const auto x = static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::floor(std::bit_cast<float>(Input[tid * 4u + 0u]) * static_cast<float>(Width))), 0, static_cast<int>(Width) - 1));
            const auto expected = Texel(x, layer);
            for (std::uint32_t component = 0; component < expected.size(); ++component) Require(std::fabs(Output[tid * 8u + component] * 255.0f - Unorm(expected[component])) <= 1.0f, "registered window: bounded native recovery after full-row capture produced incorrect sampled output");
        }
        registered.Unregister();
        const auto missingGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        auto missingInvocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, cachedRequest);
        AgcDriver::ShaderMemory missingMemory(cachedRequest.context.memory);
        const auto missingCapture = AgcDriver::DriverDetail::CaptureInvocation(missingMemory, snapshot, 0, cachedRequest, missingInvocation);
        Require(missingInvocation.Request().context.layerRowSamples && missingCapture->snapshot.imageAllocationGeneration == missingGeneration && missingCapture->snapshot.registeredImages.size() == 1u && !missingCapture->snapshot.registeredImages.front().hasRange && missingCapture->snapshot.registeredImages.front().provenance.dwords == texture, "registered window: missing-range capture did not retain the raw descriptor and current absent-range identity");
        const auto missingResult = missingInvocation.Materialize(*missingCapture);
        Require(missingResult != reboundResult, "registered window: missing-range materialization reused the bounded-native cached result");
        registered.Resize(registeredBytes);
        const auto restoredGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        auto restoredInvocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, cachedRequest);
        AgcDriver::ShaderMemory restoredMemory(cachedRequest.context.memory);
        const auto restoredCapture = AgcDriver::DriverDetail::CaptureInvocation(restoredMemory, snapshot, 0, cachedRequest, restoredInvocation);
        Require(restoredCapture->snapshot.imageAllocationGeneration == restoredGeneration && restoredCapture->snapshot.registeredImages.size() == 1u, "registered window: complete registration did not restore native mode after missing-range capture");
        const auto& restoredView = restoredCapture->snapshot.registeredImages.front();
        Require(restoredView.provenance.dwords == texture && restoredView.effective != missingCapture->snapshot.registeredImages.front().effective && restoredView.backingSerial != missingCapture->snapshot.registeredImages.front().backingSerial && restoredView.rangeBytes == registeredBytes && restoredView.hasRange, "registered window: restored native capture reused missing-range identity");
        const auto restoredResource = AgcDriver::Graphics::DecodeTextureResource(restoredCapture->snapshot.images.front().dwords);
        const auto restoredGeometry = AgcDriver::Graphics::DescribeSurface(restoredResource);
        Require(restoredResource.baseArray == baseArray && restoredGeometry.imageLayers - restoredResource.baseArray == limit, "registered window: restored native capture did not recover bounded view geometry");
        const auto restoredMode = ShaderRecompiler::ResourceMaterializer::RuntimeImageMode(restoredCapture->plan->info.images.front(), restoredCapture->snapshot.images.front(), restoredCapture->plan->info.runtimeImageModes.front());
        Require(restoredCapture->plan->info.runtimeImageModes.front().at(restoredMode).dimension == ShaderRecompiler::RdnaImageDimension::Dim1DArray && !restoredCapture->plan->info.runtimeImageModes.front().at(restoredMode).layerRows, "registered window: restored native capture did not select native 1D-array mode");
        Require(snapshot.prepared->entries.size() == fullRowPreparedEntries, "registered window: missing-range recovery added an unnecessary prepared mode");
        const auto restoredResult = restoredInvocation.Materialize(*restoredCapture);
        Require(restoredResult != missingResult && restoredResult != reboundResult, "registered window: restored registration reused stale missing-range or prior-backing materialization");
        dispatchCaptured(restoredResult, restoredCapture);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto layer = baseArray + static_cast<std::uint32_t>(std::clamp(std::nearbyint(std::bit_cast<float>(Input[tid * 4u + 1u])), 0.0f, static_cast<float>(limit - 1u)));
            const auto x = static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::floor(std::bit_cast<float>(Input[tid * 4u + 0u]) * static_cast<float>(Width))), 0, static_cast<int>(Width) - 1));
            const auto expected = Texel(x, layer);
            for (std::uint32_t component = 0; component < expected.size(); ++component) Require(std::fabs(Output[tid * 8u + component] * 255.0f - Unorm(expected[component])) <= 1.0f, "registered window: native recovery after missing-range capture produced incorrect sampled output");
        }
    }
}

void Expect(std::uint32_t tid, const std::string& name, std::uint32_t component, float scaled, float expected, float tolerance) {
    Require(std::fabs(scaled - expected) <= tolerance, name + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(scaled) + "/255, expected " + std::to_string(expected) + "/255");
}

struct Compiled {
    std::size_t bindings = 0;
    std::uint32_t elements = 0;
    ShaderRecompiler::SpirvContentHash spirv;
    std::uint64_t registeredVariant = 0;
    std::uint32_t registeredElements = 0;
    std::uint32_t preparedModes = 0;
    std::size_t preparedEntries = 0;
    bool promotedAtCapture = false;
};

Compiled Run(AgcDriver::VulkanDevice& device, std::uint32_t baseArray, std::uint32_t addressX, std::uint32_t filter, const char* name, const std::array<std::uint32_t, 11>& code = SampleLevelZero) {
    Output.fill(-1.0f);
    std::vector<std::uint32_t> userData(20, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor(baseArray);
    const std::array<std::uint32_t, 4> sampler{addressX | (ClampEdge << 3u) | (ClampEdge << 6u), 0u, filter, 0u};
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span(code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    static AgcDriver::DriverDetail::ShaderSnapshot zeroSnapshot{reinterpret_cast<std::uintptr_t>(SampleLevelZero.data()), 0, 0, {SampleLevelZero.begin(), SampleLevelZero.end()}, {}};
    static AgcDriver::DriverDetail::ShaderSnapshot levelSnapshot{reinterpret_cast<std::uintptr_t>(SampleLevel.data()), 0, 0, {SampleLevel.begin(), SampleLevel.end()}, {}};
    auto& snapshot = &code == &SampleLevel ? levelSnapshot : zeroSnapshot;
    request.shader.code = snapshot.code;
    auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request);
    const auto beforeCapture = snapshot.prepared->entries.size();
    AgcDriver::ShaderMemory shaderMemory(request.context.memory);
    const auto capture = AgcDriver::DriverDetail::CaptureInvocation(shaderMemory, snapshot, 0, request, invocation);
    const auto registered = invocation.Materialize(*capture);
    auto prepared = device.PrepareDispatch(*registered, {}, Threads, reinterpret_cast<std::uintptr_t>(code.data()), std::nullopt, &capture->snapshot.registeredImages);
    device.Dispatch(*registered, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()), std::move(prepared), nullptr, std::nullopt, &capture->snapshot.registeredImages);
    device.WaitIdle();
    const auto viewed = Layers - baseArray;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const float u = std::bit_cast<float>(Input[tid * 4u + 0u]);
        const auto layer = baseArray + static_cast<std::uint32_t>(std::clamp(std::nearbyint(std::bit_cast<float>(Input[tid * 4u + 1u])), 0.0f, static_cast<float>(viewed - 1u)));
        const float* actual = &Output[tid * 8u];
        std::array<float, 4> expected{};
        std::array<float, 4> tolerance{0.01f, 0.01f, 0.01f, 0.01f};
        if (filter == 0u) {
            const auto scaledU = static_cast<int>(std::floor(u * static_cast<float>(Width)));
            const auto x = addressX == Wrap ? static_cast<std::uint32_t>(((scaledU % static_cast<int>(Width)) + static_cast<int>(Width)) % static_cast<int>(Width)) : static_cast<std::uint32_t>(std::clamp(scaledU, 0, static_cast<int>(Width) - 1));
            const auto texel = Texel(x, layer);
            for (std::uint32_t component = 0; component < 4u; ++component) expected[component] = Unorm(texel[component]);
        } else {
            const float position = u * static_cast<float>(Width) - 0.5f;
            const auto left = static_cast<int>(std::floor(position));
            const float weight = position - std::floor(position);
            const auto clampX = [](int x) { return static_cast<std::uint32_t>(std::clamp(x, 0, static_cast<int>(Width) - 1)); };
            const auto a = Texel(clampX(left), layer);
            const auto b = Texel(clampX(left + 1), layer);
            for (std::uint32_t component = 0; component < 4u; ++component) expected[component] = Unorm(a[component]) * (1.0f - weight) + Unorm(b[component]) * weight;
            tolerance[0] = 1.0f;
        }
        for (std::uint32_t component = 0; component < 4u; ++component) Expect(tid, name, component, actual[component] * 255.0f, expected[component], tolerance[component]);
    }
    Compiled compiled{result.bindings.size(), 0u, result.spirv.ContentHash()};
    for (const auto& binding : result.bindings) compiled.elements += binding.count;
    compiled.registeredVariant = registered->variantId;
    for (const auto& binding : registered->bindings) compiled.registeredElements += binding.count;
    for (const auto& image : capture->plan->info.images) compiled.preparedModes += static_cast<std::uint32_t>(ShaderRecompiler::ResourceMaterializer::RuntimeImageModes(image).size());
    compiled.preparedEntries = snapshot.prepared->entries.size();
    compiled.promotedAtCapture = compiled.preparedEntries > beforeCapture;
    std::printf("%s: %zu bindings, %u descriptor elements, SPIR-V %016llx%016llx\n", name, compiled.bindings, compiled.elements, static_cast<unsigned long long>(compiled.spirv.high), static_cast<unsigned long long>(compiled.spirv.low));
    std::printf("registered: %u descriptor elements, prepared: %u image modes, variant %llu, capture promotion %u\n", compiled.registeredElements, compiled.preparedModes, static_cast<unsigned long long>(compiled.registeredVariant), compiled.promotedAtCapture ? 1u : 0u);
    return compiled;
}

}

int main() {
    try {
        Require(reinterpret_cast<std::uintptr_t>(Texels.data()) % TexelAlignment == 0, "the test array is not 64 KiB aligned");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto limit = device->graphicsContext().limits.maxImageArrayLayers;
        Require(limit != 0u, "the device reports no maxImageArrayLayers");
        if (limit >= MaxGuestLayers) {
            std::puts("skipped: the device allows every array a T# can describe");
            return VulkanTestSkipped;
        }
        if (limit > MaxGuestLayers - 2u) {
            std::puts("skipped: maxImageArrayLayers + 1 from baseArray 1 exceeds the guest descriptor's 13-bit last-array field");
            return VulkanTestSkipped;
        }
        const auto stillRowsResult = CaptureStillRowsRegisteredWindow(*device, limit);
        if (stillRowsResult == VulkanTestSkipped) return stillRowsResult;
        CaptureRegisteredWindow(*device, limit);
        Layers = std::min(limit + 3u, MaxGuestLayers);
        const auto baseWithin = Layers - limit;
        const auto basePast = baseWithin - 1u;
        FillTexels();
        FillInput(limit);
        const auto atLimit = Run(*device, baseWithin, ClampEdge, 0, "point image_sample_lz on a view of exactly maxImageArrayLayers slices");
        const auto atLimitLevel = Run(*device, baseWithin, ClampEdge, Bilinear, "bilinear image_sample_l at levels 0 to 3.4 on a view of exactly maxImageArrayLayers slices", SampleLevel);
        const auto pastLimit = Run(*device, basePast, ClampEdge, 0, "point image_sample_lz on a view of maxImageArrayLayers + 1 slices");
        const auto pastLimitLevel = Run(*device, basePast, ClampEdge, Bilinear, "bilinear image_sample_l at levels 0 to 3.4 on a view of maxImageArrayLayers + 1 slices", SampleLevel);
        Require(atLimit.preparedModes < pastLimit.preparedModes && atLimitLevel.preparedModes < pastLimitLevel.preparedModes, "a shader that never saw a view past maxImageArrayLayers was given the layer-row mode");
        Require(!(atLimit.spirv == pastLimit.spirv) && !(atLimitLevel.spirv == pastLimitLevel.spirv), "a view past maxImageArrayLayers was sampled through the shader prepared for views within the limit");
        Require(pastLimit.promotedAtCapture && pastLimitLevel.promotedAtCapture, "capturing the first oversized view did not promote its registered shader");
        Require(atLimit.registeredVariant != pastLimit.registeredVariant && atLimitLevel.registeredVariant != pastLimitLevel.registeredVariant, "registered shaders used the same artifact before and after layer-row promotion");
        const auto repeated = Run(*device, basePast, ClampEdge, 0, "repeated oversized view through its promoted registered shader");
        const auto returned = Run(*device, baseWithin, ClampEdge, 0, "within-limit view through its promoted registered shader");
        const auto repeatedLevel = Run(*device, basePast, ClampEdge, Bilinear, "repeated explicit-LOD oversized view through its promoted registered shader", SampleLevel);
        const auto returnedLevel = Run(*device, baseWithin, ClampEdge, Bilinear, "within-limit explicit-LOD view through its promoted registered shader", SampleLevel);
        for (const auto* reused : {&repeated, &returned}) {
            Require(reused->registeredVariant == pastLimit.registeredVariant && reused->preparedEntries == pastLimit.preparedEntries && !reused->promotedAtCapture, "registered layer-row shader was prepared again instead of reused");
        }
        for (const auto* reused : {&repeatedLevel, &returnedLevel}) {
            Require(reused->registeredVariant == pastLimitLevel.registeredVariant && reused->preparedEntries == pastLimitLevel.preparedEntries && !reused->promotedAtCapture, "registered explicit-LOD layer-row shader was prepared again instead of reused");
        }
        Run(*device, 0, ClampEdge, 0, "point image_sample_lz on a 1D array past maxImageArrayLayers");
        Run(*device, 0, Wrap, 0, "wrapped point image_sample_lz on a 1D array past maxImageArrayLayers");
        Run(*device, 0, ClampEdge, Bilinear, "bilinear image_sample_lz on a 1D array past maxImageArrayLayers");
        Run(*device, 0, ClampEdge, Bilinear, "bilinear image_sample_l at levels 0 to 3.4 on a single-level 1D array past maxImageArrayLayers", SampleLevel);
        std::puts("a 1D array past maxImageArrayLayers was sampled as rows of a 2D image");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
