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
bool SplitsSets(const Context& context, std::span<const CompiledShader> shaders) {
    const auto fragmentSet = [](const CompiledShader& shader) { return shader.program != nullptr && std::any_of(shader.program->bindings.begin(), shader.program->bindings.end(), [](const auto& binding) { return binding.descriptorSet == ShaderRecompiler::RuntimeAbi::FragmentDescriptorSet; }); };
    const auto fragmentStage = [](const CompiledShader& shader) { return shader.stage == ShaderRecompiler::ShaderStage::Fragment; };
    return std::any_of(shaders.begin(), shaders.end(), fragmentSet) || (context.graphicsPipelineLibrary && std::any_of(shaders.begin(), shaders.end(), fragmentStage));
}

DescribedSetLayouts DescribeSetLayouts(const Context& context, std::span<const CompiledShader> shaders) {
    Require(context.descriptorCache != nullptr, "described set layouts need the descriptor cache");
    DescribedSetLayouts result;
    std::array<std::vector<VkDescriptorSetLayoutBinding>, 2> descriptions;
    const bool split = SplitsSets(context, shaders);
    std::array<std::vector<VkDescriptorBindingFlags>, 2> bindingFlags;
    std::array<VkShaderStageFlags, 2> tableFlags{};
    std::array<VkShaderStageFlags, 2> storageTableFlags{};
    bool updateAfterBind = false;
    bool tablesUpdateAfterBind = false;
    std::uint64_t storageBuffers = 0;
    for (const auto& shader : shaders) {
        Require(shader.program != nullptr, "missing compiled shader");
        const VkShaderStageFlags flags = VulkanStage(shader.stage);
        std::uint64_t stageStorageBuffers = 0;
        std::uint64_t stageResources = 0;
        for (const auto& binding : shader.program->bindings) {
            const auto set = binding.descriptorSet == ShaderRecompiler::RuntimeAbi::FragmentDescriptorSet ? 1 : 0;
            if (binding.role == ShaderRecompiler::DescriptorRole::ImageTable) {
                tableFlags[split && shader.stage == ShaderRecompiler::ShaderStage::Fragment ? 1 : 0] |= flags;
                continue;
            }
            if (binding.role == ShaderRecompiler::DescriptorRole::ImageStorageTable) {
                storageTableFlags[split && shader.stage == ShaderRecompiler::ShaderStage::Fragment ? 1 : 0] |= flags;
                continue;
            }
            if (binding.role == ShaderRecompiler::DescriptorRole::SamplerTable) {
                descriptions[set].push_back({binding.binding, VK_DESCRIPTOR_TYPE_SAMPLER, context.samplerTableCapacity, flags, nullptr});
                bindingFlags[set].push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | (context.samplerTableUpdateAfterBind ? VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT : 0));
                tablesUpdateAfterBind |= context.samplerTableUpdateAfterBind;
                continue;
            }
            const bool image = binding.role == ShaderRecompiler::DescriptorRole::GuestImages || binding.role == ShaderRecompiler::DescriptorRole::GuestSamplers;
            const auto type = !image ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : binding.kind == ShaderRecompiler::DescriptorKind::StorageImage ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : binding.kind == ShaderRecompiler::DescriptorKind::SampledImage ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
            if (type != VK_DESCRIPTOR_TYPE_SAMPLER) stageResources += binding.count;
            if (type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
                stageStorageBuffers += binding.count;
                storageBuffers += binding.count;
            }
            if (stageStorageBuffers > context.limits.maxPerStageDescriptorStorageBuffers || stageResources > context.limits.maxPerStageResources) updateAfterBind = true;
            descriptions[set].push_back({binding.binding, type, binding.count, flags, nullptr});
            bindingFlags[set].push_back(0);
        }
    }
    for (std::size_t set = 0; set < storageTableFlags.size(); ++set) {
        if (storageTableFlags[set] == 0) continue;
        descriptions[set].push_back({ShaderRecompiler::RuntimeAbi::ImageStorageTableBinding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, StorageTableCapacity(context), storageTableFlags[set], nullptr});
        bindingFlags[set].push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
    }
    for (std::size_t set = 0; set < tableFlags.size(); ++set) {
        if (tableFlags[set] == 0) continue;
        descriptions[set].push_back({ShaderRecompiler::RuntimeAbi::ImageTableBinding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, context.imageTableCapacity, tableFlags[set], nullptr});
        bindingFlags[set].push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | (context.imageTableUpdateAfterBind ? VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT : 0));
        tablesUpdateAfterBind |= context.imageTableUpdateAfterBind;
    }
    if (storageBuffers > context.limits.maxDescriptorSetStorageBuffers) updateAfterBind = true;
    for (std::size_t set = 0; set < descriptions.size(); ++set) {
        for (std::size_t index = 0; index < descriptions[set].size(); ++index) {
            if (updateAfterBind && descriptions[set][index].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) bindingFlags[set][index] |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
        }
    }
    updateAfterBind = updateAfterBind || tablesUpdateAfterBind;
    for (std::size_t set = 0; set < descriptions.size(); ++set) {
        auto& key = set == 0 ? result.mainKey : result.fragmentKey;
        for (const auto& binding : descriptions[set]) key.insert(key.end(), {binding.binding, static_cast<std::uint32_t>(binding.descriptorType), binding.descriptorCount, binding.stageFlags});
        if (updateAfterBind && (set == 0 || split)) key.push_back(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    }
    result.layouts.split = split;
    result.layouts.main = context.descriptorCache->Layout(result.mainKey, descriptions[0], updateAfterBind, bindingFlags[0]);
    if (split) result.layouts.fragment = context.descriptorCache->Layout(result.fragmentKey, descriptions[1], updateAfterBind, bindingFlags[1]);
    return result;
}

ShaderRecompiler::PipelineIdentity DescribedPipelineIdentity(const ShaderRecompiler::RecompileResult& shader) {
    const auto hash = shader.spirv.ContentHash();
    if (hash == ShaderRecompiler::SpirvContentHash{}) return {};
    ShaderRecompiler::PipelineIdentity identity{hash.low, hash.high};
    const auto mix = [&](std::uint64_t value) {
        identity.low = (identity.low ^ value) * 0x9e3779b97f4a7c15ull;
        identity.low ^= identity.low >> 29u;
        identity.high = (identity.high ^ (value + 0x7f4a7c159e3779b9ull)) * 0xc2b2ae3d27d4eb4full;
        identity.high ^= identity.high >> 31u;
    };
    mix(shader.specialization.size());
    for (const auto& constant : shader.specialization) mix(constant.id | (static_cast<std::uint64_t>(constant.value) << 32u));
    mix(shader.hostSubgroupSize | (shader.fullSubgroups ? 0x10000u : 0u) | (static_cast<std::uint64_t>(shader.workgroupMemoryDwords) << 32u));
    mix(shader.bindings.size());
    for (const auto& binding : shader.bindings) {
        const bool fixedCapacity = binding.role == ShaderRecompiler::DescriptorRole::ImageTable || binding.role == ShaderRecompiler::DescriptorRole::ImageStorageTable || binding.role == ShaderRecompiler::DescriptorRole::SamplerTable;
        const std::uint64_t count = fixedCapacity ? 0u : binding.count;
        mix(binding.descriptorSet | (static_cast<std::uint64_t>(binding.binding) << 16u) | (static_cast<std::uint64_t>(binding.kind) << 40u) | (count << 48u));
    }
    if (identity.Empty()) identity.low = 1;
    return identity;
}

void ShaderResources::buildComplete() {
    PerformanceTimer frameTiming("Resources.Complete");
    BuildLookups lookups;
    Require(!completed, "cannot update a published descriptor heap");
    Require(registeredImageViewsCurrent(registeredImageViews), "AGC graphics: registered image range changed after resource preparation");
    const auto stageStart = std::chrono::steady_clock::now();
    phaseStart = stageStart;
    try {
        previousSampled = {};
        for (auto& source : samplerSources) {
            source.singleLevelImage = source.mipmappedImage = false;
            source.pinnedLod.reset();
        }
        for (const auto& deferred : deferredImages) resolveImageBinding(*deferred.binding, bindings[deferred.index], std::span<const std::shared_ptr<Sampler>>(samplers).subspan(deferred.firstSampler, deferred.samplerCount));
        previousSampled = {};
        deferredImages.clear();
        applySamplerVariants();
        imageRecords.clear();
        imageRecords.shrink_to_fit();
        nextImageRecord = 0;
        Require(textures.size() == plannedSampledImages && storageTextures.size() == plannedStorageImages, "image lookups disagree with the descriptor plan");
        resolveImageTables();
        Require(registeredImageViewsCurrent(registeredImageViews), "AGC graphics: registered image range changed during resource materialization");
        timing.bindingsMs += phase(BuildPhase::Images);
        requireGuestBuffers();
        guestMemory.UploadFinish(usesBda, htileClear);
        timing.uploadMs += phase(BuildPhase::Upload);
        if (usesBda) bda = std::make_unique<BdaResources>(context, guestMemory, faultReportOnly);
        else if (usesFaultBuffer) bda = std::make_unique<BdaResources>(context, faultReportOnly);
        if (bda) bda->SetSrtGuards(std::move(srtGuardFaults));
        finishImageTables();
        phase(BuildPhase::Bda);
        if (_set != VK_NULL_HANDLE || fragment.set != VK_NULL_HANDLE) {
            std::size_t bufferCount = 0;
            std::size_t imageCount = 0;
            for (const auto& binding : bindings) {
                bufferCount += binding.allocations.size();
                if (binding.layout.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) imageCount += binding.flags != 0 ? binding.imageAllocations.size() : binding.layout.descriptorCount;
            }
            auto& buffers = descriptorBuffers;
            auto& images = descriptorImages;
            buffers.reserve(bufferCount);
            images.reserve(imageCount);
            auto& writes = descriptorWrites;
            writes.reserve(bindings.size());
            std::vector<std::pair<std::size_t, std::size_t>> keptWrites;
            const auto absentImage = [this](std::uint32_t binding) {
                constexpr auto bindingCount = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::Count);
                const auto view = context.nullDescriptors ? VK_NULL_HANDLE : context.placeholderImages->View(binding % bindingCount);
                return VkDescriptorImageInfo{VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_GENERAL};
            };
            for (const auto& binding : bindings) {
                if (binding.flags != 0) {
                    const auto set = setOf(binding);
                    const bool sampled = binding.layout.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    const auto& allocation = set == 0 ? cacheAllocation : fragment.cacheAllocation;
                    const bool recorded = sampled && set < tableWrites.size() && allocation.layout != VK_NULL_HANDLE;
                    std::vector<std::pair<std::uint32_t, std::uint32_t>> runs;
                    const auto firstWrite = writes.size();
                    const auto firstImage = images.size();
                    for (std::size_t first = 0; first < binding.imageAllocations.size();) {
                        if (binding.imageAllocations[first] == std::numeric_limits<std::size_t>::max()) {
                            ++first;
                            continue;
                        }
                        auto last = first;
                        while (last < binding.imageAllocations.size() && binding.imageAllocations[last] != std::numeric_limits<std::size_t>::max()) ++last;
                        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                        write.dstSet = set == 0 ? _set : fragment.set;
                        write.dstBinding = binding.layout.binding;
                        write.dstArrayElement = static_cast<std::uint32_t>(first);
                        write.descriptorCount = static_cast<std::uint32_t>(last - first);
                        write.descriptorType = binding.layout.descriptorType;
                        write.pImageInfo = images.data() + images.size();
                        for (auto element = first; element < last; ++element) {
                            const auto index = binding.imageAllocations[element];
                            if (binding.layout.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) images.push_back({samplers[index]->Handle(), VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED});
                            else if (binding.layout.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) images.push_back({VK_NULL_HANDLE, storageAtomic64[index] ? storageTextures[index]->Atomic64View(storageMips[index], storageFirstLayer[index]) : storageAtomic[index] ? storageTextures[index]->AtomicView(storageMips[index], storageFirstLayer[index]) : storageTextures[index]->StorageView(storageMips[index], storageFirstLayer[index], ShaderRecompiler::RuntimeAbi::UintStorageImageBinding(binding.layout.binding) || storageUint[index]), VK_IMAGE_LAYOUT_GENERAL});
                            else images.push_back({VK_NULL_HANDLE, textureFirstLayer[index] ? textures[index]->FirstLayerView() : arrayViewTextures.contains(index) ? textures[index]->ArrayView() : textures[index]->View(), textures[index]->Layout()});
                        }
                        if (recorded) runs.emplace_back(write.dstArrayElement, write.descriptorCount);
                        writes.push_back(write);
                        first = last;
                    }
                    if (!recorded) continue;
                    const auto& held = allocation.table;
                    auto& counters = TextureCounts();
                    const bool heldAlive = held != nullptr && held->textures != nullptr && std::none_of(held->textures->begin(), held->textures->end(), [](const std::weak_ptr<Texture>& texture) { return texture.expired(); });
                    if (heldAlive && held->binding == binding.layout.binding && held->runs == runs && std::equal(held->images.begin(), held->images.end(), images.begin() + static_cast<std::ptrdiff_t>(firstImage), images.end(), [](const VkDescriptorImageInfo& a, const VkDescriptorImageInfo& b) { return a.sampler == b.sampler && a.imageView == b.imageView && a.imageLayout == b.imageLayout; })) {
                        keptWrites.emplace_back(firstWrite, writes.size());
                        counters.tableWritesKept.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        counters.tableWritesMade.fetch_add(1, std::memory_order_relaxed);
                    }
                    tableWrites[set] = {true, false, binding.layout.binding, firstImage, images.size() - firstImage, std::move(runs)};
                    continue;
                }
                VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                write.dstSet = setOf(binding) == 0 ? _set : fragment.set;
                write.dstBinding = binding.layout.binding;
                write.descriptorCount = binding.layout.descriptorCount;
                write.descriptorType = binding.layout.descriptorType;
                switch (binding.layout.descriptorType) {
                    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                        write.pBufferInfo = buffers.data() + buffers.size();
                        for (std::size_t element = 0; element < binding.allocations.size(); ++element) {
                            const auto index = binding.allocations[element];
                            if (allocations[index].role == ShaderRecompiler::DescriptorRole::FaultBuffer) faultDescriptors.push_back({writes.size(), buffers.size(), static_cast<std::uint32_t>(element)});
                            buffers.push_back(descriptor(allocations[index]));
                        }
                        break;
                    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                        write.pImageInfo = images.data() + images.size();
                        for (const auto index : binding.imageAllocations) {
                            if (index == std::numeric_limits<std::size_t>::max()) images.push_back(absentImage(binding.layout.binding));
                            else images.push_back({VK_NULL_HANDLE, textureSrgb[index] ? textures[index]->SrgbView(textureFirstLayer[index], textureBaseLevel[index]) : textureBaseLevel[index] ? textures[index]->BaseLevelView() : textureFirstLayer[index] ? textures[index]->FirstLayerView() : textures[index]->View(), textures[index]->Layout()});
                        }
                        Require(binding.imageAllocations.size() == binding.layout.descriptorCount, "descriptor allocations disagree with compact binding");
                        break;
                    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                        write.pImageInfo = images.data() + images.size();
                        for (const auto index : binding.imageAllocations) {
                            if (index == std::numeric_limits<std::size_t>::max()) images.push_back(absentImage(binding.layout.binding));
                            else if (storageTextures[index] == nullptr) images.push_back({VK_NULL_HANDLE, multisampleStorageViews[index], VK_IMAGE_LAYOUT_GENERAL});
                            else images.push_back({VK_NULL_HANDLE, storageAtomic64[index] ? storageTextures[index]->Atomic64View(storageMips[index], storageFirstLayer[index]) : storageAtomic[index] ? storageTextures[index]->AtomicView(storageMips[index], storageFirstLayer[index]) : storageTextures[index]->StorageView(storageMips[index], storageFirstLayer[index], ShaderRecompiler::RuntimeAbi::UintStorageImageBinding(binding.layout.binding) || storageUint[index]), VK_IMAGE_LAYOUT_GENERAL});
                        }
                        Require(binding.imageAllocations.size() == binding.layout.descriptorCount, "descriptor allocations disagree with compact binding");
                        break;
                    case VK_DESCRIPTOR_TYPE_SAMPLER:
                        write.pImageInfo = images.data() + images.size();
                        for (const auto index : binding.imageAllocations) images.push_back({samplers[index]->Handle(), VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED});
                        Require(binding.imageAllocations.size() == binding.layout.descriptorCount, "descriptor allocations disagree with compact binding");
                        break;
                    default: throw std::runtime_error("AGC graphics: ShaderResources encountered an unknown descriptor type while writing the descriptor set");
                }
                writes.push_back(write);
            }
            if (keptWrites.empty()) {
                context.Resolved(&DeviceFunctions::updateDescriptorSets, "vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
            } else {
                std::vector<VkWriteDescriptorSet> made;
                made.reserve(writes.size());
                std::size_t next = 0;
                for (std::size_t index = 0; index < writes.size(); ++index) {
                    if (next < keptWrites.size() && index == keptWrites[next].first) {
                        index = keptWrites[next++].second - 1u;
                        continue;
                    }
                    made.push_back(writes[index]);
                }
                if (!made.empty()) context.Resolved(&DeviceFunctions::updateDescriptorSets, "vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(made.size()), made.data(), 0, nullptr);
            }
            for (auto& table : tableWrites) table.written = table.recorded;
        }
        for (const auto& allocation : allocations) {
            if (allocation.adjustment == 0) continue;
            if (allocation.pushByte >= 0) {
                pushPatches.emplace_back(static_cast<std::uint32_t>(allocation.pushByte), allocation.adjustment);
                continue;
            }
            Require(allocation.dataAllocation >= 0, "a guest buffer off the storage buffer offset alignment in a shader without shader data is not implemented");
            auto& data = allocations[static_cast<std::size_t>(allocation.dataAllocation)];
            Require(data.buffer != nullptr && allocation.dataByte < data.size, "guest buffer offset lies outside the shader's data buffer");
            data.buffer->Bytes()[allocation.dataByte] = static_cast<std::byte>(allocation.adjustment);
            dataPatches.push_back({static_cast<std::size_t>(allocation.dataAllocation), allocation.dataByte, allocation.adjustment});
        }
        timing.descriptorsMs += phase(BuildPhase::Descriptors);
        noteReusable();
        completed = true;
        if (BuildProfiled()) {
            timing.completeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - stageStart).count();
            addBuildPhase(BuildPhase::StageB, timing.completeMs);
            reportDescriptorCaches();
        }
    } catch (...) {
        release();
        throw;
    }
}

namespace {

bool TemplateDataRefresh() {
    static const bool enabled = std::getenv("APS5_NO_TEMPLATE_DATA_REFRESH") == nullptr;
    return enabled;
}

bool DataRole(ShaderRecompiler::DescriptorRole role) {
    return role == ShaderRecompiler::DescriptorRole::ShaderData || role == ShaderRecompiler::DescriptorRole::FlattenedSrt;
}

constexpr std::size_t MaxRefreshBytes = 65536;

constexpr std::uint64_t FnvOffset = 14695981039346656037ull;
constexpr std::uint64_t FnvPrime = 1099511628211ull;
void mixDataWords(std::uint64_t& hash, std::span<const std::uint32_t> words) {
    hash = (hash ^ static_cast<std::uint64_t>(words.size())) * FnvPrime;
    for (const auto word : words) hash = (hash ^ word) * FnvPrime;
}

std::atomic<std::uint64_t> resourceCacheFinds{0};
std::atomic<std::uint64_t> resourceCacheTouches{0};
std::atomic<std::uint64_t> depthGuardRefusals{0};
thread_local bool indexProof = false;

}

namespace {

bool LoopGuardEnabled() {
    static const bool enabled = std::getenv("APS5_LOOP_GUARD") != nullptr;
    return enabled;
}

}

void ShaderResources::noteReusable() {
    const bool spaceReusable = !htileClear.has_value() && drawBuild && usesBda && bdaReadOnly && !SpaceReuseDisabled() && guestMemory.SpaceOnly() && !guestMemory.HasCopiedWrites() && multisampleSources.empty() && !(bda != nullptr && LoopGuardEnabled());
    captureValidation(spaceReusable);
    reusable = false;
    importsReconcilable = false;
    spaceReuse = false;
    directRegions.clear();
    if (!spaceReusable && (htileClear.has_value() || guestMemory.HasCopiedWrites() || HoldsLease() || usesBda || !multisampleSources.empty() || (bda != nullptr && LoopGuardEnabled()))) return;
    if (std::any_of(textures.begin(), textures.end(), [](const std::shared_ptr<Texture>& texture) { return texture != nullptr && texture->RefreshedPerUse(); })) return;
    if (TemplateDataRefresh() && std::any_of(allocations.begin(), allocations.end(), [](const Allocation& allocation) { return allocation.buffer != nullptr && !allocation.guest && allocation.role != ShaderRecompiler::DescriptorRole::ImageTableMap && allocation.size > MaxRefreshBytes; })) return;
    if (spaceReusable) {
        spaceReuse = true;
        reusable = true;
        return;
    }
    const auto regions = guestMemory.DirectRegions();
    if (!regions.has_value()) return;
    bool mirrored = false;
    for (const auto& [begin, end] : *regions) {
        auto serial = HostImportSerial(context, begin, static_cast<std::size_t>(end - begin), false);
        const bool imported = serial != 0;
        if (serial == 0) serial = ImageMirrorSerial(context, begin, static_cast<std::size_t>(end - begin));
        if (serial == 0) return;
        directRegions.push_back({begin, end, serial});
        mirrored = mirrored || !imported;
    }
    bool bounded = false;
    for (const auto& cut : guestMemory.CutRegions()) {
        for (auto& region : directRegions) {
            if (region.begin != cut.begin) continue;
            region.regionEnd = cut.end;
            region.committedEnd = cut.committedEnd;
            bounded = true;
        }
    }
    importsReconcilable = !mirrored && !bounded;
    reusable = true;
}

std::uint64_t ShaderResources::DepthGuardRefusals() {
    return depthGuardRefusals.load(std::memory_order_relaxed);
}

bool ShaderResources::NeverReusable(std::span<const CompiledShader> shaders) {
    const bool loopGuard = LoopGuardEnabled();
    return std::any_of(shaders.begin(), shaders.end(), [loopGuard](const CompiledShader& shader) {
        return shader.program != nullptr && std::any_of(shader.program->bindings.begin(), shader.program->bindings.end(), [&](const ShaderRecompiler::DescriptorBinding& binding) { return (binding.role == ShaderRecompiler::DescriptorRole::BdaPagetable && (SpaceReuseDisabled() || ShaderStoresThroughAddresses(shader.program->spirv))) || (loopGuard && binding.role == ShaderRecompiler::DescriptorRole::FaultBuffer); });
    });
}

}
}
}
}
}
}
}
}
