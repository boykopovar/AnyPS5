#include "Optimization/BindingAllocator.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <utility>

namespace ShaderRecompiler {
namespace {

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

std::vector<std::uint32_t> collectUserData(const IrProgram& program) {
    std::array<bool, NumScalarRegs> registers {};
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            if (inst->Opcode() != IrOpcode::GetUserData || !inst->HasUses()) {
                continue;
            }
            const IrValue* operand = inst->Argument(0);
            if (operand->Type() != IrType::ScalarReg) {
                fail("shader binding layout failed: typed shader contains an invalid user-data register");
            }
            const std::uint32_t index = operand->Register().index;
            if (index >= NumScalarRegs) {
                fail("shader binding layout failed: typed shader contains an invalid user-data register");
            }
            registers[index] = true;
        }
    }
    std::vector<std::uint32_t> result;
    for (std::uint32_t index = 0; index < registers.size(); index++) {
        if (registers[index]) {
            result.push_back(index);
        }
    }
    return result;
}

void addBinding(IrBindingLayout& layout, DescriptorBindingKind kind, std::vector<std::uint32_t> resources = {}) {
    layout.descriptors.push_back(IrDescriptorBinding {kind, std::move(resources)});
}

bool usesGds(const IrProgram& program) {
    const std::vector<MemoryInfo>& memoryInfo = program.Resources().memoryInfo;
    bool result = false;
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            if (SharedAccessOf(inst->Opcode()) == SharedAccess::None) {
                continue;
            }
            const std::uint32_t index = inst->Flags<MemoryFlags>().index;
            if (index >= memoryInfo.size()) {
                fail("shader binding layout failed: typed shader contains invalid shared-memory metadata");
            }
            const ResourceKind kind = memoryInfo[index].kind;
            if (kind != ResourceKind::Lds && kind != ResourceKind::Gds) {
                fail("shader binding layout failed: typed shader contains invalid shared-memory metadata");
            }
            result |= kind == ResourceKind::Gds;
        }
    }
    return result;
}

}

BindingAllocationResult BindingAllocator::Allocate(IrProgram& program, const BindingLayout& layout) const {
    IrProgramMetadata& metadata = program.Metadata();
    if (!metadata.shaderInfoComplete || metadata.bindingLayoutComplete) {
        fail(metadata.shaderInfoComplete ? "shader binding layout failed: binding layout already allocated"
                                          : "shader binding layout failed: shader info is not ready");
    }
    if (layout.descriptorSet != RuntimeAbi::DescriptorSet) {
        fail("shader binding layout failed: descriptor set must be 0");
    }
    if (layout.firstBinding != 0u) {
        fail("shader binding layout failed: first binding must be 0");
    }
    if (layout.pushConstantOffsetBytes % 4u != 0u) {
        fail("shader binding layout failed: push constant offset is not dword-aligned");
    }
    if (layout.pushConstantSizeBytes % 4u != 0u) {
        fail("shader binding layout failed: push constant size is not dword-aligned");
    }
    if (layout.pushConstantOffsetBytes + layout.pushConstantSizeBytes > NativePushConstantSize) {
        fail("shader binding layout failed: push constant range exceeds the native push constant size");
    }

    const ShaderInfo& info = program.Resources().info;

    IrBindingLayout next;
    next.userDataRegisters = collectUserData(program);
    next.memoryOffsetDword = static_cast<std::uint32_t>(next.userDataRegisters.size());
    next.memoryOffsetCount = static_cast<std::uint32_t>(info.buffers.size());
    next.dispatchThreadLimit = info.dispatchThreadLimit;
    if (info.buffers.size() > RuntimeAbi::BufferCapacity || info.images.size() > RuntimeAbi::ImageCapacity || info.samplers.size() > RuntimeAbi::SamplerHeapCapacity) fail("shader binding layout exceeds runtime metadata capacity");
    if (std::ranges::any_of(next.userDataRegisters, [](auto reg) { return reg >= RuntimeAbi::UserDataCapacity; })) fail("shader user data exceeds runtime ABI capacity");
    if (std::ranges::any_of(info.images, [](const auto& image) { return image.indirectRoot != ImageResource::NoIndirectImage; })) next.runtimeImageCount = static_cast<std::uint32_t>(info.images.size());
    const auto pushSize = layout.pushConstantSizeBytes / 4u;
    next.pushDataStartDword = next.runtimeImageCount == 0u && next.ShaderDataDwords() != 0u && next.ShaderDataDwords() <= pushSize ? layout.pushConstantOffsetBytes / 4u : PushData::NoStart;

    if (!info.buffers.empty()) {
        std::vector<std::uint32_t> resources(info.buffers.size());
        for (std::uint32_t i = 0; i < resources.size(); i++) {
            resources[i] = i;
        }
        addBinding(next, DescriptorBindingKind::Buffers, std::move(resources));
    }

    std::array<std::vector<std::uint32_t>, ImageBindingCount> imageGroups;
    const auto place = [&](std::uint32_t i) {
        const bool dynamic = info.images[i].mipMode == ImageMipMode::DynamicStorage;
        const std::uint32_t count = dynamic ? RuntimeAbi::StorageHeapCapacity : 1u;
        std::array<bool, ImageBindingCount> placed{};
        for (const auto& mode : ResourceMaterializer::RuntimeImageModes(info.images[i])) {
            const auto group = ImageBindingIndex(DescriptorBindingForImage(mode));
            if (placed.at(group)) continue;
            placed[group] = true;
            imageGroups[group].insert(imageGroups[group].end(), count, i);
        }
    };
    // A bindless table's slots follow their root as consecutive elements: the SPIR-V indexes the
    // binding with element(root) + slot.
    for (std::uint32_t i = 0; i < info.images.size(); i++) {
        const auto root = info.images[i].indirectRoot;
        if (root != ImageResource::NoIndirectImage && root != i) {
            continue;
        }
        place(i);
        if (root != i) {
            continue;
        }
        for (const auto slot : info.images[i].indirectResources) {
            if (slot >= info.images.size() || info.images[slot].indirectRoot != i) {
                fail("shader binding layout failed: image " + std::to_string(i) + " has an inconsistent table slot");
            }
            if (slot != i) place(slot);
        }
    }
    for (std::uint32_t i = 0; i < imageGroups.size(); i++) {
        if (!imageGroups[i].empty()) {
            if (imageGroups[i].size() > RuntimeAbi::HeapCapacity(static_cast<DescriptorBindingKind>(FirstImageBinding + i))) fail("shader image heap capacity exceeded");
            addBinding(next, static_cast<DescriptorBindingKind>(FirstImageBinding + i), std::move(imageGroups[i]));
        }
    }

    if (!info.samplers.empty()) {
        if (info.samplers.size() > RuntimeAbi::SamplerHeapCapacity / 2u) fail("shader sampler pairs exceed runtime heap capacity");
        std::vector<std::uint32_t> resources(info.samplers.size() * 2u);
        for (std::uint32_t i = 0; i < resources.size(); i++) {
            resources[i] = i / 2u;
        }
        addBinding(next, DescriptorBindingKind::Samplers, std::move(resources));
    }
    if (usesGds(program)) {
        addBinding(next, DescriptorBindingKind::Gds);
    }
    if (info.usesDma) {
        addBinding(next, DescriptorBindingKind::BdaPagetable);
        addBinding(next, DescriptorBindingKind::FaultBuffer);
    }

    const bool usesFlattenedRuntime = !program.Resources().srtReads.empty() ||
        std::ranges::any_of(info.images, [](const ImageResource& image) {
            return image.indirectSearchIterations != 0u;
        });
    if (usesFlattenedRuntime) {
        addBinding(next, DescriptorBindingKind::FlattenedSrt);
    }

    if (next.ShaderDataDwords() != 0u && !next.UsesPushData()) {
        addBinding(next, DescriptorBindingKind::ShaderData);
    }

    metadata.bindings = std::move(next);
    metadata.bindingLayoutComplete = true;

    BindingAllocationResult result;
    result.layout = metadata.bindings;
    if (result.layout.UsesPushData()) {
        result.pushConstantOffsetBytes = result.layout.pushDataStartDword * static_cast<std::uint32_t>(sizeof(std::uint32_t));
        result.pushConstantSizeBytes = result.layout.ShaderDataDwords() * static_cast<std::uint32_t>(sizeof(std::uint32_t));
    }
    return result;
}

const IrDescriptorBinding& BindingAllocator::FindBinding(const IrBindingLayout& layout, DescriptorBindingKind kind) const {
    for (const IrDescriptorBinding& binding : layout.descriptors) {
        if (binding.kind == kind) {
            return binding;
        }
    }
    fail("shader binding layout failed: requested descriptor binding kind is not present in the layout");
}

}
