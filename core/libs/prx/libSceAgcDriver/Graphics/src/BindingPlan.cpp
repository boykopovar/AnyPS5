#include "prx/libSceAgcDriver/Graphics/include/BindingPlan.hpp"
#include <algorithm>
#include <limits>
#include <set>

namespace AgcDriver::Graphics {

namespace {

const char* roleName(ShaderRecompiler::DescriptorRole role) {
    switch (role) {
        case ShaderRecompiler::DescriptorRole::GuestBuffers: return "GuestBuffers";
        case ShaderRecompiler::DescriptorRole::GuestImages: return "GuestImages";
        case ShaderRecompiler::DescriptorRole::GuestSamplers: return "GuestSamplers";
        case ShaderRecompiler::DescriptorRole::Gds: return "Gds";
        case ShaderRecompiler::DescriptorRole::BdaPagetable: return "BdaPagetable";
        case ShaderRecompiler::DescriptorRole::FaultBuffer: return "FaultBuffer";
        case ShaderRecompiler::DescriptorRole::FlattenedSrt: return "FlattenedSrt";
        case ShaderRecompiler::DescriptorRole::ShaderData: return "ShaderData";
    }
    throw std::runtime_error("AGC graphics: unknown descriptor role");
}

const char* kindName(ShaderRecompiler::DescriptorKind kind) {
    switch (kind) {
        case ShaderRecompiler::DescriptorKind::UniformBuffer: return "UniformBuffer";
        case ShaderRecompiler::DescriptorKind::StorageBuffer: return "StorageBuffer";
        case ShaderRecompiler::DescriptorKind::UniformTexelBuffer: return "UniformTexelBuffer";
        case ShaderRecompiler::DescriptorKind::StorageTexelBuffer: return "StorageTexelBuffer";
        case ShaderRecompiler::DescriptorKind::SampledImage: return "SampledImage";
        case ShaderRecompiler::DescriptorKind::StorageImage: return "StorageImage";
        case ShaderRecompiler::DescriptorKind::Sampler: return "Sampler";
    }
    throw std::runtime_error("AGC graphics: unknown descriptor kind");
}

}

BindingPlan::BindingPlan(const Context& context, std::span<const CompiledShader> shaders) {
    using Role = ShaderRecompiler::DescriptorRole;
    using Kind = ShaderRecompiler::DescriptorKind;
    Require(!shaders.empty() && context.limits.maxBoundDescriptorSets >= 1, "shader descriptor set exceeds device limits");
    std::set<std::uint32_t> occupied;
    for (std::size_t shaderIndex = 0; shaderIndex < shaders.size(); ++shaderIndex) {
        const auto& shader = shaders[shaderIndex];
        Require(shader.program != nullptr, "missing compiled shader");
        const auto flags = VulkanStage(shader.stage);
        std::uint64_t stageBuffers = 0, stageSampled = 0, stageStorage = 0, stageSamplers = 0;
        const auto firstBuffer = buffers.size();
        const auto firstBinding = bindings.size();
        const auto firstSampler = samplers;
        std::int64_t shaderData = -1;
        for (std::size_t source = 0; source < shader.program->bindings.size(); ++source) {
            const auto& binding = shader.program->bindings[source];
            Require(binding.descriptorSet == 0, "unexpected descriptor set: every shader resource must use descriptor set zero");
            Require(occupied.insert(binding.binding).second, "duplicate shader binding");
            Require(binding.count != 0, "empty descriptor binding");
            Binding item{{binding.binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, binding.count, flags, nullptr}, shaderIndex, source, binding.role, {}, {}};
            if (binding.role == Role::GuestImages || binding.role == Role::GuestSamplers) {
                if (binding.kind == Kind::SampledImage || binding.kind == Kind::StorageImage) Require(stageBuffers + stageSampled + stageStorage + binding.count <= context.limits.maxPerStageResources, "shader descriptors exceed per-stage limits");
                const bool sampled = binding.kind == Kind::SampledImage;
                const bool storage = binding.kind == Kind::StorageImage;
                const bool sampler = binding.kind == Kind::Sampler;
                if (!(sampled || storage || sampler)) Require(false, std::string("unsupported descriptor kind ") + kindName(binding.kind) + " for role " + roleName(binding.role));
                if (storage) Require(binding.role == Role::GuestImages, "storage image binding has a non-image role");
                else Require(((sampled || storage) && binding.role == Role::GuestImages) || (sampler && binding.role == Role::GuestSamplers), "guest image descriptor role disagrees with its kind");
                Require(binding.guestDescriptor.size() % binding.count == 0, "guest image descriptor size is not a multiple of the binding count");
                const auto words = binding.guestDescriptor.size() / binding.count;
                if (sampled) {
                    Require(words == 8, "guest texture descriptor must contain 8 dwords");
                    Require(binding.imageShape.has_value(), "guest image binding is missing an image shape");
                    stageSampled += binding.count;
                    Require(stageSampled <= context.limits.maxPerStageDescriptorSampledImages, "shader sampled-image descriptors exceed per-stage limits");
                    Require(static_cast<std::uint64_t>(sampledImages) + binding.count <= context.limits.maxDescriptorSetSampledImages, "pipeline sampled-image descriptors exceed device limits");
                    item.layout.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    item.imageAllocations = std::ranges::iota_view<std::size_t, std::size_t>{sampledImages, static_cast<std::size_t>(sampledImages) + binding.count};
                    sampledImages += binding.count;
                } else if (storage) {
                    Require(words == 8, "guest storage image descriptors must contain 8 dwords each");
                    stageStorage += binding.count;
                    Require(stageStorage <= context.limits.maxPerStageDescriptorStorageImages, "shader storage-image descriptors exceed per-stage limits");
                    Require(static_cast<std::uint64_t>(storageImages) + binding.count <= context.limits.maxDescriptorSetStorageImages, "pipeline storage-image descriptors exceed device limits");
                    item.layout.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    item.imageAllocations = std::ranges::iota_view<std::size_t, std::size_t>{storageImages, static_cast<std::size_t>(storageImages) + binding.count};
                    storageImages += binding.count;
                } else {
                    Require(words == 4, "guest sampler descriptor must contain 4 dwords");
                    stageSamplers += binding.count;
                    Require(stageSamplers <= context.limits.maxPerStageDescriptorSamplers, "shader sampler descriptors exceed per-stage limits");
                    Require(binding.samplerDepthCompare.size() == binding.count, "guest sampler binding is missing depth comparison metadata");
                    Require(binding.samplerUnnormalized.empty() || binding.samplerUnnormalized.size() == binding.count, "guest sampler binding has unnormalized coordinate metadata of another size");
                    Require(static_cast<std::uint64_t>(samplers) + binding.count <= context.limits.maxDescriptorSetSamplers, "pipeline sampler descriptors exceed device limits");
                    item.layout.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                    item.imageAllocations = std::ranges::iota_view<std::size_t, std::size_t>{samplers, static_cast<std::size_t>(samplers) + binding.count};
                    samplers += binding.count;
                }
            } else {
                const bool address = binding.role == Role::BdaPagetable || binding.role == Role::FaultBuffer;
                if (!(address || binding.role == Role::GuestBuffers || binding.role == Role::ShaderData || binding.role == Role::FlattenedSrt || binding.role == Role::Gds)) Require(false, std::string("unsupported descriptor role ") + roleName(binding.role));
                if (binding.kind != Kind::StorageBuffer) Require(false, std::string("unsupported descriptor kind ") + kindName(binding.kind) + " for role " + roleName(binding.role) + ": only StorageBuffer is supported");
                Require(!binding.readOnly, "read-only descriptors are unsupported because the recompiler emits no NonWritable decoration");
                stageBuffers += binding.count;
                Require(stageBuffers <= context.limits.maxPerStageDescriptorStorageBuffers && stageBuffers <= context.limits.maxPerStageResources, "shader descriptors exceed per-stage limits");
                Require(buffers.size() + static_cast<std::uint64_t>(binding.count) <= context.limits.maxDescriptorSetStorageBuffers, "pipeline descriptors exceed device limits");
                item.allocations = std::ranges::iota_view<std::size_t, std::size_t>{buffers.size(), buffers.size() + binding.count};
                if (binding.role == Role::GuestBuffers) {
                    Require(binding.guestDescriptor.size() == static_cast<std::uint64_t>(binding.count) * 4, "guest buffer descriptor must contain four DWORDs per array element");
                    for (std::uint32_t element = 0; element < binding.count; ++element) {
                        Buffer buffer;
                        buffer.written = element >= binding.bufferWritten.size() || binding.bufferWritten[element];
                        buffer.atomic = element < binding.bufferAtomic.size() && binding.bufferAtomic[element];
                        const auto position = static_cast<std::uint64_t>(shader.program->memoryOffsetDword) * 4 + element;
                        Require(position <= std::numeric_limits<std::uint32_t>::max(), "guest buffer offset exceeds shader data address space");
                        if (!shader.program->pushConstants.empty()) {
                            Require(position < shader.program->pushConstants.size(), "guest buffer offset lies outside the shader's push constants");
                            Require(position + shader.pushConstantOffset <= std::numeric_limits<std::int32_t>::max(), "guest buffer push offset exceeds address space");
                            buffer.pushByte = static_cast<std::int32_t>(position + shader.pushConstantOffset);
                        } else buffer.dataByte = static_cast<std::uint32_t>(position);
                        buffers.push_back(buffer);
                    }
                } else {
                    Require(binding.count == 1, "shader data and flattened SRT descriptors must not be arrays");
                    if (binding.role == Role::Gds) Require(binding.guestDescriptor.empty(), "invalid GDS descriptor contract");
                    else if (!address) Require(!binding.guestDescriptor.empty(), "empty shader data descriptor");
                    if (binding.role == Role::ShaderData) shaderData = static_cast<std::int64_t>(buffers.size());
                    buffers.push_back({});
                }
            }
            Require(stageBuffers + stageSampled + stageStorage <= context.limits.maxPerStageResources, "shader descriptors exceed per-stage limits");
            layout.push_back(item.layout);
            layoutKey.insert(layoutKey.end(), {item.layout.binding, static_cast<std::uint32_t>(item.layout.descriptorType), item.layout.descriptorCount, flags});
            bindings.push_back(item);
        }
        for (auto index = firstBuffer; index < buffers.size(); ++index) buffers[index].dataAllocation = shaderData;
        for (auto index = firstBinding; index < bindings.size(); ++index) {
            bindings[index].firstSampler = firstSampler;
            bindings[index].samplerCount = samplers - firstSampler;
        }
    }
    if (!buffers.empty()) descriptorSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<std::uint32_t>(buffers.size())});
    if (sampledImages != 0) descriptorSizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImages});
    if (storageImages != 0) descriptorSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, storageImages});
    if (samplers != 0) descriptorSizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, samplers});
}

BindingPlanCache::BindingPlanCache(const Context& context, std::size_t capacity) : context(context), capacity(capacity) {
    Require(capacity != 0, "binding plan cache capacity is zero");
}

std::shared_ptr<const BindingPlan> BindingPlanCache::Get(std::span<const CompiledShader> shaders) {
    if (shaders.size() > Key{}.size() || std::any_of(shaders.begin(), shaders.end(), [](const auto& shader) { return shader.program == nullptr || shader.program->variantId == 0; })) return std::make_shared<const BindingPlan>(context, shaders);
    Key key{};
    for (std::size_t i = 0; i < shaders.size(); ++i) key[i] = {shaders[i].program->variantId, shaders[i].stage, shaders[i].pushConstantOffset};
    std::lock_guard lock(mutex);
    if (const auto found = entries.find(key); found != entries.end()) {
        order.splice(order.begin(), order, found->second.order);
        return found->second.plan;
    }
    auto plan = std::make_shared<const BindingPlan>(context, shaders);
    order.push_front(key);
    entries.emplace(key, Entry{plan, order.begin()});
    if (entries.size() > capacity) {
        entries.erase(order.back());
        order.pop_back();
    }
    return plan;
}

std::size_t BindingPlanCache::Size() const {
    std::lock_guard lock(mutex);
    return entries.size();
}

}
