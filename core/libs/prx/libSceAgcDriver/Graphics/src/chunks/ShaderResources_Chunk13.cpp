#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "ThreadOwned.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PlaceholderImages.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <memory>
#include <mutex>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <stdexcept>
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Optimization/include/Optimization/ShaderStageInputInfo.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "IntermediateRepresentation/IrMetadata/ShaderStage.hpp"
#include "ImageTableAbi.hpp"
#include <cstring>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "prx/libc/include/GuestAllocations.hpp"

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
void forEachImageElement(const ShaderRecompiler::RecompileResult& program, Visit&& visit) {
    for (const auto& binding : program.bindings) {
        if (binding.role != ShaderRecompiler::DescriptorRole::GuestImages || binding.count == 0) continue;
        if (binding.kind != ShaderRecompiler::DescriptorKind::SampledImage && binding.kind != ShaderRecompiler::DescriptorKind::StorageImage) continue;
        const auto elementWords = binding.guestDescriptor.size() / binding.count;
        for (std::uint32_t element = 0; element < binding.count; ++element) visit(binding, element, std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * elementWords, elementWords));
    }
}

}

bool ShaderResources::precollectImages() {
    static const bool disabled = std::getenv("APS5_NO_PRECOLLECT") != nullptr;
    static const bool noRecords = std::getenv("APS5_NO_STAGE_A_IMAGES") != nullptr;
    if (disabled || deferredImages.empty()) return false;
    imageRecords.clear();
    nextImageRecord = 0;
    auto& counters = TextureCounts();
    for (const auto& deferred : deferredImages) {
        const auto& binding = *deferred.binding;
        const auto elementWords = binding.guestDescriptor.size() / binding.count;
        for (std::uint32_t element = 0; element < binding.count; ++element) {
            const bool sampled = binding.kind == ShaderRecompiler::DescriptorKind::SampledImage;
            const auto raw = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * elementWords, elementWords);
            std::array<std::uint32_t, 8> narrowed{};
            const auto words = firstLayerWords(raw, sampled ? binding.imageShape : std::nullopt, narrowed, false);
            if (sampled && !imageRecords.empty()) {
                const auto& previous = imageRecords.back();
                if (previous.sampled && previous.decoded && words.size() == 8 && std::equal(words.begin(), words.end(), previous.words.begin()) && (binding.imageDepthCompare.empty() || !binding.imageDepthCompare.at(element))) {
                    ImageRecord repeated;
                    repeated.sampled = true;
                    repeated.decoded = previous.decoded;
                    repeated.words = previous.words;
                    repeated.resource = previous.resource;
                    repeated.guestBytes = previous.guestBytes;
                    repeated.components = previous.components;
                    imageRecords.push_back(repeated);
                    continue;
                }
            }
            ImageRecord record;
            record.sampled = sampled;
            if (sampled ? !IsNullSampledTextureResource(words) : !IsNullTextureResource(words)) {
                const auto* memo = words.size() == 8 ? &decodedImage(context.device, words) : nullptr;
                record.resource = memo != nullptr ? memo->resource : DecodeTextureResource(words);
                record.guestBytes = memo != nullptr ? memo->guestBytes : SurfaceGuestBytes(record.resource);
                record.generation = GuestMemory::CollectWrites(record.resource.baseAddress, static_cast<std::size_t>(record.guestBytes));
                record.decoded = true;
                if (record.sampled && !noRecords && words.size() == 8 && (binding.imageDepthCompare.empty() || !binding.imageDepthCompare.at(element))) {
                    std::copy(words.begin(), words.end(), record.words.begin());
                    record.components = memo->components;
                    record.keys = TextureClearKeys(record.resource, record.guestBytes);
                    auto& cache = Textures();
                    std::lock_guard lock(cache.mutex);
                    if (const auto it = findSampledEntry(cache, context.device, record.words, record.components); it != cache.entries.end()) {
                        record.texture = it->texture;
                        record.source = it->source;
                        record.entryKeys = it->keys;
                        record.entryGeneration = it->generation;
                        if (TextureCountersReported()) counters.records.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
            imageRecords.push_back(std::move(record));
        }
    }
    return true;
}

std::shared_ptr<Texture> ShaderResources::fastTexture(ImageRecord& record) {
    if (record.texture == nullptr || record.texture->RefreshedPerUse() || record.texture->IsDepthCopy()) return nullptr;
    struct Outcome {
        bool profile;
        std::chrono::steady_clock::time_point start;
        bool hit = false;
        ~Outcome() {
            if (profile) LookupOutcomes::Add(hit ? LookupOutcomes::SampledFast : LookupOutcomes::SampledFastMiss, start);
        }
    } outcome{LookupOutcomes::Profiled(), LookupOutcomes::Profiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{}};
    const auto address = record.resource.baseAddress;
    const auto bytes = static_cast<std::size_t>(record.guestBytes);
    RequireNoMultisampleColorTarget(address, bytes != 0 ? bytes : 1u, "a texture read");
    if (DepthSurfaceAt(address) || !FlushDepthPlanes(address, bytes).empty()) return nullptr;
    if (GuestMemory::CollectWrites(address, bytes) == 0) return nullptr;
    if (PendingStorageOverlaps(address, bytes, record.source.get())) return nullptr;
    auto keys = record.keys;
    if (record.resource.dccAddress != 0) {
        const auto scanStart = outcome.profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        keys = ProvedClearKeys(record.resource, record.guestBytes, SameKeySurface(record.source.get(), record.resource, record.guestBytes) ? record.source->KeyProof() : record.texture->KeyProof());
        if (outcome.profile) LookupOutcomes::Add(LookupOutcomes::DccScan, scanStart);
    }
    if (keys != record.entryKeys) return nullptr;
    if (record.source != nullptr) {
        if (!StorageImageCached(context, record.source.get()) || !GuestMemory::UnchangedSince(address, bytes, record.source->GenerationFor(address, bytes))) return nullptr;
        if (record.resource.dccAddress != 0 && IsDccClear(record.source->FilledKeys())) return nullptr;
        if (keys != DccKeys::Uncompressed && StorageTexture::FindPending(address, record.guestBytes) != record.source) return nullptr;
    } else if ((keys == DccKeys::Uncompressed || keys == DccKeys::ClearSingle) && !GuestMemory::UnchangedSince(address, bytes, record.entryGeneration)) {
        return nullptr;
    }
    auto& cache = Textures();
    std::lock_guard lock(cache.mutex);
    const auto it = findSampledEntry(cache, context.device, record.words, record.components);
    if (it == cache.entries.end() || it->texture != record.texture || it->committed.has_value()) return nullptr;
    if (it->source == nullptr) it->generation = record.generation;
    touchTexture(cache, it);
    logLookup({record.texture.get(), record.resource, record.guestBytes, keys, record.source != nullptr ? 0 : record.generation, record.source.get()});
    outcome.hit = true;
    return std::move(record.texture);
}

void ShaderResources::applySamplerVariants() {
    for (std::size_t index = 0; index < samplerSources.size(); ++index) {
        const auto& source = samplerSources[index];
        const auto variant = SingleLevelSamplerWords(source.words, source.singleLevelImage, source.mipmappedImage);
        if (!variant.has_value() && !source.pinnedLod.has_value()) continue;
        const auto words = source.pinnedLod.has_value() ? PinnedLodSamplerWords(variant.has_value() ? *variant : source.words, *source.pinnedLod) : *variant;
        if (context.samplerCache != nullptr && !SamplerCacheDisabled()) {
            samplers[index] = context.samplerCache->Get(context, words, source.compareEnable, source.unnormalized, true, source.integerBorder);
        } else {
            auto resource = DecodeSamplerResource(words, source.unnormalized, true, source.integerBorder);
            resource.compareEnable = source.compareEnable;
            samplers[index] = std::make_shared<Sampler>(context, resource);
        }
    }
}

void ShaderResources::resolveImageBinding(const ShaderRecompiler::DescriptorBinding& binding, Binding& item, std::span<const std::shared_ptr<Sampler>> shaderSamplers) {
    auto& counters = TextureCounts();
    const auto nextRecord = [&]() -> ImageRecord* { return nextImageRecord < imageRecords.size() ? &imageRecords[nextImageRecord++] : nullptr; };
    if (binding.kind == ShaderRecompiler::DescriptorKind::SampledImage) {
        Require(binding.imageSamplers.size() == binding.count, "guest sampled image binding is missing its image-sampler pairs");
        const auto elementWords = binding.guestDescriptor.size() / binding.count;
        for (std::uint32_t element = 0; element < binding.count; ++element) {
            const auto raw = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * elementWords, elementWords);
            std::array<std::uint32_t, 8> narrowed{};
            const auto words = firstLayerWords(raw, binding.imageShape, narrowed, true);
            auto* record = nextRecord();
            if (IsNullSampledTextureResource(words)) {
                item.imageAllocations.push_back(std::numeric_limits<std::size_t>::max());
                --plannedSampledImages;
                continue;
            }
            const auto resource = record != nullptr && record->decoded ? record->resource : DecodeTextureResource(words);
            const auto minLod = context.shaderImageMinLod && resource.minLod > resource.baseLevel * 256u && resource.lastLevel > resource.baseLevel ? std::min(resource.minLod - resource.baseLevel * 256u, (resource.lastLevel - resource.baseLevel) * 256u) : 0u;
            if (minLod != 0u && !shaderSamplers.empty()) {
                const auto firstSampler = static_cast<std::size_t>(shaderSamplers.data() - samplers.data());
                for (std::uint32_t sampler = 0; sampler < shaderSamplers.size() && sampler < 32u; ++sampler) {
                    auto& source = samplerSources.at(firstSampler + sampler);
                    if (((binding.imageSamplers[element] >> sampler) & 1u) == 0 || !HoldsLodAtZero(source.words)) continue;
                    Require(!source.pinnedLod.has_value() || *source.pinnedLod == minLod, "an S# that holds the LOD at 0 is paired with images of different minimum LODs");
                    source.pinnedLod = minLod;
                }
            }
            const bool forcedSrgb = ForcesDegammaOn(resource.format, binding.imageSamplers[element], shaderSamplers);
            RequireNonSeamlessCube(resource.dimension == TextureDimension::kCube, binding.imageSamplers[element], shaderSamplers);
            RequireDisablesDegammaOn(resource.format, binding.imageSamplers[element], shaderSamplers);
            const bool depthCompare = !binding.imageDepthCompare.empty() && binding.imageDepthCompare.at(element);
            const bool firstLayer = binding.imageShape == ShaderRecompiler::DescriptorImageShape::Image2D && (resource.dimension == TextureDimension::k2DArray || (depthCompare && resource.dimension == TextureDimension::k1DArray));
            const bool depth1D = depthCompare && ((binding.imageShape == ShaderRecompiler::DescriptorImageShape::Image2D && resource.dimension == TextureDimension::k1D) || (binding.imageShape == ShaderRecompiler::DescriptorImageShape::Image2DArray && resource.dimension == TextureDimension::k1DArray));
            const bool firstSample = binding.imageShape == ShaderRecompiler::DescriptorImageShape::Image2D && resource.dimension == TextureDimension::k2DMsaa;
            const bool layerRows = layerRowsBinding(binding.imageShape, resource, depthCompare);
            Require(!layerRows || (resource.baseArray <= resource.depthOrLastArray && resource.depthOrLastArray - resource.baseArray >= context.limits.maxImageArrayLayers), "a 1D array within maxImageArrayLayers was bound as rows of a 2D image");
            if (!firstLayer && !depth1D && !firstSample && !layerRows && !MatchesGuestDimension(*binding.imageShape, resource.dimension)) throw std::runtime_error("AGC graphics: guest texture dimension disagrees with the shader's declared image shape (shape " + std::to_string(static_cast<int>(*binding.imageShape)) + ", dimension " + std::to_string(static_cast<int>(resource.dimension)) + ")");
            const VkComponentMapping components = ViewComponents(resource);
            const auto guestBytes = record != nullptr && record->decoded ? record->guestBytes : SurfaceGuestBytes(resource);
            CompletePendingCpuWrites(context, resource.baseAddress, static_cast<std::size_t>(guestBytes));
            if (resource.dccAddress != 0) CompletePendingCpuWrites(context, resource.dccAddress, DccKeyBytes(guestBytes));
            std::shared_ptr<Texture> texture;
            const VkImageViewType multisampleViewType = binding.imageShape == ShaderRecompiler::DescriptorImageShape::Image2DMultisampleArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
            if (previousSampled.texture != nullptr && previousSampled.depthCompare == depthCompare && previousSampled.multisampleViewType == multisampleViewType && previousSampled.firstSample == firstSample && previousSampled.layerRows == layerRows && words.size() == 8 && std::equal(words.begin(), words.end(), previousSampled.words.begin())) texture = previousSampled.texture;
            if (texture == nullptr && !firstSample && record != nullptr && record->texture != nullptr) {
                texture = fastTexture(*record);
                if (TextureCountersReported()) (texture != nullptr ? counters.fastHits : counters.fastMisses).fetch_add(1, std::memory_order_relaxed);
            }
            if (texture == nullptr && firstSample) {
                Require(!depthCompare, "reading sample 0 of a multisampled texture with a depth comparison through a 2D image is not implemented");
                texture = DepthSurfaceAt(resource.baseAddress) ? DepthSurfaceFirstSample(context, words, resource, components) : MultisampleTargetFirstSample(context, resource, components);
            }
            if (texture == nullptr) texture = cachedTexture(context, words, resource, components, guestBytes, depthCompare, multisampleViewType, layerRows);
            if (resource.dimension == TextureDimension::k2DMsaa) multisampleSources.push_back(resource.baseAddress);
            bool baseLevel = false;
            if (element < binding.imageUnnormalized.size() && binding.imageUnnormalized[element]) {
                const auto range = texture->SampledViewRange(firstLayer);
                const bool exactAtBase = element < binding.imageUnnormalizedBaseLevel.size() && binding.imageUnnormalizedBaseLevel[element];
                const bool singleLevel = range.levels == 1u && resource.baseLevel == 0u;
                const bool supported = range.layers == 1u && EffectiveMinLod(resource) == 0.0f && (singleLevel || exactAtBase);
                if (!supported || (range.type != VK_IMAGE_VIEW_TYPE_1D && range.type != VK_IMAGE_VIEW_TYPE_2D)) throw std::runtime_error("AGC graphics: guest texture sampled with unnormalized coordinates is not a single-layer 1D or 2D view without a minimum LOD clamp, and is neither a single-level view at mip 0 nor read only by image_sample_lz through an S# without minimum LOD or bias, which is not implemented (base level " + std::to_string(resource.baseLevel) + ", levels " + std::to_string(range.levels) + ", layers " + std::to_string(range.layers) + ", view type " + std::to_string(static_cast<int>(range.type)) + ")");
                baseLevel = range.levels > 1u;
            }
            RequireFilterMinmax(context, texture->ViewFormat(), binding.imageSamplers[element], shaderSamplers);
            RequireBorderSwizzle(resource.bcSwizzle, binding.imageSamplers[element], shaderSamplers);
            if (forcedSrgb && !texture->HasSrgbView()) Require(false, "a texture of guest format " + std::to_string(resource.format) + ", which is not sRGB and has no sRGB view, is sampled through a sampler that forces sRGB decoding, which is not implemented");
            if (!shaderSamplers.empty()) {
                const bool singleLevel = DescriptorSingleLevel(words);
                const auto firstSampler = static_cast<std::size_t>(shaderSamplers.data() - samplers.data());
                for (auto mask = binding.imageSamplers[element]; mask != 0u; mask &= mask - 1u) {
                    const auto sampler = static_cast<std::uint32_t>(std::countr_zero(mask));
                    if (sampler >= shaderSamplers.size()) break;
                    auto& source = samplerSources.at(firstSampler + sampler);
                    (singleLevel ? source.singleLevelImage : source.mipmappedImage) = true;
                }
            }
            if (words.size() == 8) {
                std::copy(words.begin(), words.end(), previousSampled.words.begin());
                previousSampled.depthCompare = depthCompare;
                previousSampled.multisampleViewType = multisampleViewType;
                previousSampled.firstSample = firstSample;
                previousSampled.layerRows = layerRows;
                previousSampled.texture = texture;
            }
            textures.push_back(std::move(texture));
            textureFirstLayer.push_back(firstLayer);
            textureBaseLevel.push_back(baseLevel);
            textureSrgb.push_back(forcedSrgb);
            describedRanges.push_back({"texture", resource.baseAddress, guestBytes, resource.width, resource.height, resource.format, static_cast<int>(resource.tileMode), resource.dccAddress});
            item.imageAllocations.push_back(textures.size() - 1);
        }
        reportTextureCounters();
        return;
    }
    previousSampled = {};
    std::uint32_t mipOffset = 0;
    for (std::uint32_t element = 0; element < binding.count; ++element) {
        const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * 8u, 8u);
        const bool sameAsPrevious = SameAsPreviousStorageElement(binding, element);
        if (sameAsPrevious) ++mipOffset;
        else mipOffset = 0;
        const auto* record = nextRecord();
        if (IsNullTextureResource(words)) {
            item.imageAllocations.push_back(std::numeric_limits<std::size_t>::max());
            --plannedStorageImages;
            continue;
        }
        const auto resource = record != nullptr && record->decoded ? record->resource : DecodeTextureResource(words);
        const bool firstLayer = binding.imageShape == ShaderRecompiler::DescriptorImageShape::Image2D && resource.dimension == TextureDimension::k2DArray;
        if (binding.imageShape.has_value() && !firstLayer && !MatchesGuestDimension(*binding.imageShape, resource.dimension)) throw std::runtime_error("AGC graphics: guest storage texture dimension disagrees with the shader's declared image shape (shape " + std::to_string(static_cast<int>(*binding.imageShape)) + ", dimension " + std::to_string(static_cast<int>(resource.dimension)) + ")");
        if (resource.dimension == TextureDimension::k2DMsaa) {
            Require(!(element < binding.imageAtomic.size() && binding.imageAtomic[element]) && !(element < binding.imageAtomic64.size() && binding.imageAtomic64[element]), "storage image atomics on a multisampled texture are not implemented");
            storageTextures.push_back(nullptr);
            multisampleStorageViews.resize(storageTextures.size(), VK_NULL_HANDLE);
            const bool uintStorage = ShaderRecompiler::RuntimeAbi::UintStorageImageBinding(binding.binding);
            multisampleStorageViews.back() = MultisampleTargetStorageView(context, resource, uintStorage);
            multisampleSources.push_back(resource.baseAddress);
            storageMips.push_back(0);
            storageKeys.push_back(0);
            storageFirstLayer.push_back(false);
            storageWritten.push_back(false);
            storageAtomic.push_back(false);
            storageAtomic64.push_back(false);
            storageUint.push_back(false);
            item.imageAllocations.push_back(storageTextures.size() - 1);
            continue;
        }
        const auto mip = std::min(resource.baseLevel + mipOffset, resource.mipCount - 1u);
        Require(resource.minLod <= mip * 256u, "guest storage texture descriptor clamps its minimum LOD above the level it addresses, which is not implemented");
        const auto guestBytes = record != nullptr && record->decoded ? record->guestBytes : SurfaceGuestBytes(resource);
        CompletePendingCpuWrites(context, resource.baseAddress, static_cast<std::size_t>(guestBytes));
        if (resource.dccAddress != 0) CompletePendingCpuWrites(context, resource.dccAddress, DccKeyBytes(guestBytes));
        const bool written = element >= binding.imageWritten.size() || binding.imageWritten[element];
        const bool stored = element < binding.imageWritten.size() && binding.imageWritten[element];
        if (stored) claimStoredMultisampleMemory(context, resource, guestBytes, mip);
        if (sameAsPrevious && StorageDedupeEnabled()) storageTextures.push_back(storageTextures.back());
        else storageTextures.push_back(cachedStorageTexture(context, words, resource, mip, guestBytes, written));
        storageMips.push_back(mip);
        storageKeys.push_back(resource.dccAddress);
        storageFirstLayer.push_back(firstLayer);
        storageWritten.push_back(written);
        storageAtomic.push_back(element < binding.imageAtomic.size() && binding.imageAtomic[element]);
        storageAtomic64.push_back(element < binding.imageAtomic64.size() && binding.imageAtomic64[element]);
        storageUint.push_back(false);
        describedRanges.push_back({"storage", resource.baseAddress, guestBytes, resource.width, resource.height, resource.format, static_cast<int>(resource.tileMode), resource.dccAddress});
        if (stored) {
            describedRanges.back().storedWords.emplace();
            std::copy(words.begin(), words.end(), describedRanges.back().storedWords->begin());
            describedRanges.back().storedMip = mip;
        }
        item.imageAllocations.push_back(storageTextures.size() - 1);
    }
}

std::uint32_t StorageTableCapacity(const Context& context) {
    return std::min<std::uint32_t>({context.imageTableCapacity, 4096u, context.limits.maxPerStageDescriptorStorageImages / 4u});
}

namespace {

std::uint64_t Room(std::uint64_t limit, std::uint64_t reserve) {
    return limit > reserve ? limit - reserve : 0u;
}

std::uint32_t ImageTableCapacityFor(const Context& context, bool updateAfterBind) {
    if (!context.runtimeDescriptorArray || !context.partiallyBound || !context.variableDescriptorCount || !context.descriptorIndexing) return 0u;
    if (updateAfterBind && !context.sampledImageUpdateAfterBind) return 0u;
    constexpr std::uint64_t sampledHeaps = static_cast<std::uint64_t>(ShaderRecompiler::RuntimeAbi::FirstStorageImageBinding - ShaderRecompiler::RuntimeAbi::FirstImageBinding) * ShaderRecompiler::RuntimeAbi::SampledHeapCapacity;
    constexpr std::uint64_t storageHeaps = static_cast<std::uint64_t>(static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::Samplers) - ShaderRecompiler::RuntimeAbi::FirstStorageImageBinding) * ShaderRecompiler::RuntimeAbi::StorageHeapCapacity;
    constexpr std::uint64_t buffers = ShaderRecompiler::RuntimeAbi::BufferCapacity + 8u;
    const auto& indexed = context.descriptorIndexingLimits;
    const std::uint64_t stageImages = updateAfterBind ? indexed.maxPerStageDescriptorUpdateAfterBindSampledImages : context.limits.maxPerStageDescriptorSampledImages;
    const std::uint64_t stageResources = updateAfterBind ? indexed.maxPerStageUpdateAfterBindResources : context.limits.maxPerStageResources;
    const std::uint64_t setImages = updateAfterBind ? indexed.maxDescriptorSetUpdateAfterBindSampledImages : context.limits.maxDescriptorSetSampledImages;
    std::uint64_t capacity = static_cast<std::uint64_t>(ShaderRecompiler::RuntimeAbi::StageCount) * ShaderRecompiler::ImageTableAbi::ImageElementBudget;
    capacity = std::min(capacity, Room(stageImages, sampledHeaps));
    capacity = std::min(capacity, Room(stageResources, sampledHeaps + storageHeaps + buffers));
    capacity = std::min(capacity, Room(setImages, sampledHeaps * ShaderRecompiler::RuntimeAbi::StageCount));
    return static_cast<std::uint32_t>(capacity);
}

std::uint32_t SamplerTableCapacityFor(const Context& context, bool updateAfterBind) {
    if (!context.runtimeDescriptorArray || !context.partiallyBound) return 0u;
    if (updateAfterBind && !context.sampledImageUpdateAfterBind) return 0u;
    const auto& indexed = context.descriptorIndexingLimits;
    const std::uint64_t stageSamplers = updateAfterBind ? indexed.maxPerStageDescriptorUpdateAfterBindSamplers : context.limits.maxPerStageDescriptorSamplers;
    const std::uint64_t setSamplers = updateAfterBind ? indexed.maxDescriptorSetUpdateAfterBindSamplers : context.limits.maxDescriptorSetSamplers;
    std::uint64_t capacity = ShaderRecompiler::ImageTableAbi::SamplerElementBudget;
    capacity = std::min(capacity, Room(stageSamplers, ShaderRecompiler::RuntimeAbi::SamplerHeapCapacity));
    capacity = std::min(capacity, Room(setSamplers / ShaderRecompiler::RuntimeAbi::StageCount, ShaderRecompiler::RuntimeAbi::SamplerHeapCapacity));
    return static_cast<std::uint32_t>(capacity - capacity % ShaderRecompiler::ImageTableAbi::SamplerEntryElements);
}

}

std::uint64_t& DeviceProcLookups() {
    thread_local std::uint64_t count = 0;
    return count;
}

std::uint32_t ImageTableCapacity(const Context& context) {
    const auto capacity = ImageTableCapacityFor(context, false);
    return capacity != 0u ? capacity : ImageTableCapacityFor(context, true);
}

std::uint32_t SamplerTableCapacity(const Context& context) {
    const auto capacity = SamplerTableCapacityFor(context, false);
    return capacity != 0u ? capacity : SamplerTableCapacityFor(context, true);
}

}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
