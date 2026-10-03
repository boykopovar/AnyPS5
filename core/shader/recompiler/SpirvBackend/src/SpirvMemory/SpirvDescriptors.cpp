#include "SpirvBackend/SpirvMemory/SpirvDescriptors.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler
{
namespace {

[[noreturn]] void FailEmit(const std::string& reason) {
    throw std::runtime_error("SPIR-V module emission failed: " + reason);
}

IrShaderStage StageOf(const SpirvEmitterState& state) {
    return state.program.Resources().stage;
}

constexpr RdnaImageDimensionInfo ImageDimensions[] = {
    {RdnaImageDimension::Dim1D, spv::Dim1D, 1, 1, 0, 0},
    {RdnaImageDimension::Dim2D, spv::Dim2D, 2, 2, 0, 0},
    {RdnaImageDimension::Dim3D, spv::Dim3D, 3, 3, 0, 0},
    //{RdnaImageDimension::DimCube, spv::DimCube, 3, 2, 0, 0},
    {RdnaImageDimension::Dim1DArray, spv::Dim1D, 2, 1, 1, 0},
    {RdnaImageDimension::Dim2DArray, spv::Dim2D, 3, 2, 1, 0},
    {RdnaImageDimension::Dim2DMsaa, spv::Dim2D, 2, 2, 0, 1},
    {RdnaImageDimension::Dim2DMsaaArray, spv::Dim2D, 3, 2, 1, 1},
};

}

[[noreturn]] void ExitDescriptorBindingFailure(const SpirvEmitterState& state, DescriptorBindingKind kind, std::uint32_t resource, const char* reason) {
    throw std::runtime_error("shader binding resolution failed during SPIR-V emit: hash=" + std::to_string(state.program.Resources().shaderHash) + " stage=" + std::to_string(static_cast<std::uint32_t>(StageOf(state))) + " resource=" + std::to_string(resource) + " binding_kind=" + std::to_string(static_cast<std::uint32_t>(kind)) + " reason=" + reason);
}

std::uint32_t ResourceForDescriptor(const SpirvEmitterState& state, DescriptorBindingKind kind, std::uint32_t resource) {
    for (const auto& descriptor : state.program.Metadata().bindings.descriptors) {
        if (descriptor.kind != kind) {
            continue;
        }
        const auto found = std::find(descriptor.resources.begin(), descriptor.resources.end(), resource);
        if (found == descriptor.resources.end()) {
            ExitDescriptorBindingFailure(state, kind, resource, "resource is absent from descriptor group");
        }
        return static_cast<std::uint32_t>(found - descriptor.resources.begin());
    }
    ExitDescriptorBindingFailure(state, kind, resource, "descriptor group was not allocated");
}

std::uint32_t DescriptorElementPointer(SpirvEmitterState& state, std::uint32_t resultPtrType, std::uint32_t variableId, std::uint32_t arrayIndex, DescriptorBindingKind kind, std::uint32_t resource, const char* variableName) {
    if (variableId == 0) {
        ExitDescriptorBindingFailure(state, kind, resource, variableName);
    }
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, resultPtrType, pointer, variableId, ConstantU32(state, arrayIndex));
    return pointer;
}

// The element of a bindless table's runtime slot, `arrayIndex` (the root's element) + `slotId`.
// The slot is wave-uniform; where that is not uniform over the invocation group (see
// SpirvEmitterState::tableIndexNonUniform) the index carries NonUniform, which needs
// VK_EXT_descriptor_indexing on the device.
std::uint32_t TableImageIndex(SpirvEmitterState& state, DescriptorBindingKind kind, std::uint32_t resource, std::uint32_t arrayIndex, std::uint32_t slotId) {
    const bool storage = ImageBindingResourceClass(kind) == ImageResourceClass::Storage;
    const auto supported = [&](std::uint32_t capability) {
        return std::find(state.supportedCapabilities.begin(), state.supportedCapabilities.end(), capability) != state.supportedCapabilities.end();
    };
    const auto dynamic = storage ? spv::CapabilityStorageImageArrayDynamicIndexing : spv::CapabilitySampledImageArrayDynamicIndexing;
    if (!supported(dynamic)) {
        ExitDescriptorBindingFailure(state, kind, resource, "bindless image table needs image array dynamic indexing, which the device lacks");
    }
    state.module.EmitCapability(dynamic);
    const auto index = Binary(state, spv::OpIAdd, TypeU32(state), ConstantU32(state, arrayIndex), slotId);
    if (!state.tableIndexNonUniform) {
        return index;
    }
    const auto nonUniform = storage ? spv::CapabilityStorageImageArrayNonUniformIndexing : spv::CapabilitySampledImageArrayNonUniformIndexing;
    const bool extension = state.spirvVersion < 0x00010500u;
    if (!supported(spv::CapabilityShaderNonUniform) || !supported(nonUniform) || (extension && std::find(state.supportedExtensions.begin(), state.supportedExtensions.end(), "SPV_EXT_descriptor_indexing") == state.supportedExtensions.end())) {
        ResourceMaterializer::CountBindlessRejection(BindlessRejection::NonUniform);
        throw std::runtime_error("bindless image table: the slot is not uniform over the workgroup and the device lacks non-uniform image indexing");
    }
    state.module.EmitCapability(spv::CapabilityShaderNonUniform);
    state.module.EmitCapability(nonUniform);
    if (extension) state.module.EmitExtension("SPV_EXT_descriptor_indexing");
    state.module.AddAnnotation(spv::OpDecorate, index, spv::DecorationNonUniform);
    return index;
}

void DecorateTableNonUniform(SpirvEmitterState& state, std::uint32_t slotId, std::uint32_t id) {
    if (slotId != 0 && state.tableIndexNonUniform) state.module.AddAnnotation(spv::OpDecorate, id, spv::DecorationNonUniform);
}

std::uint32_t ImageScalarType(SpirvEmitterState& state, IrTextureNumericClass numericClass) {
    switch (numericClass) {
    case IrTextureNumericClass::Float: return TypeF32(state);
    case IrTextureNumericClass::Uint: return TypeU32(state);
    case IrTextureNumericClass::Sint: return TypeI32(state);
    case IrTextureNumericClass::Unsupported: break;
    }
    FailEmit("invalid image numeric class");
}

std::uint32_t ImageVectorType(SpirvEmitterState& state, IrTextureNumericClass numericClass, std::uint32_t components) {
    return state.module.Type(spv::OpTypeVector, ImageScalarType(state, numericClass), components);
}

std::uint32_t ImageType(SpirvEmitterState& state, const ImageResource& image) {
    std::uint32_t sampled = 0;
    std::uint32_t format = spv::ImageFormatUnknown;
    if (image.resourceClass == ImageResourceClass::Sampled) {
        if (image.atomic) {
            FailEmit("sampled image cannot be atomic");
        }
        sampled = 1;
    } else if (image.resourceClass == ImageResourceClass::Storage) {
        if (image.numericClass == IrTextureNumericClass::Sint || image.numericClass == IrTextureNumericClass::Unsupported) {
            FailEmit("storage image numeric class is not supported");
        }
        sampled = 2;
        if (image.atomic) {
            if (image.numericClass != IrTextureNumericClass::Uint) {
                FailEmit("atomic storage image must be uint");
            }
            format = spv::ImageFormatR32ui;
        }
    } else {
        FailEmit("invalid image resource class");
    }
    const auto& info = RdnaImageDimensionInfoFor(image.dimension);
    return state.module.Type(spv::OpTypeImage, ImageScalarType(state, image.numericClass), info.spirvDimension, image.depthCompare ? 1u : 0u, info.arrayed, info.multisampled, sampled, format);
}

std::uint32_t ImageViewSizeType(SpirvEmitterState& state, RdnaImageDimension dimension) {
    switch (RdnaImageDimensionInfoFor(dimension).coordinateComponents) {
    case 1u: return TypeU32(state);
    case 2u: return TypeU32Vector(state, 2);
    case 3u: return TypeU32Vector(state, 3);
    default: FailEmit("image view size component count is invalid");
    }
}

std::uint32_t LoadSampledImageDescriptor(SpirvEmitterState& state, std::uint32_t resource, std::uint32_t slotId) {
    const auto& imageResource = state.program.Info().images.at(resource);
    if (imageResource.resourceClass != ImageResourceClass::Sampled) {
        FailEmit("sampled image descriptor requested for a non-sampled image");
    }
    if (imageResource.empty) {
        FailEmit("reading an image view that starts past its surface's last mip level is not implemented");
    }
    const auto kind = DescriptorBindingForImage(imageResource);
    const auto arrayIndex = ResourceForDescriptor(state, kind, resource);
    const auto variable = state.imageVariables.at(ImageBindingIndex(kind));
    const auto imageType = ImageType(state, imageResource);
    const auto pointerType = state.module.Type(spv::OpTypePointer, spv::StorageClassUniformConstant, imageType);
    if (variable == 0) {
        ExitDescriptorBindingFailure(state, kind, resource, "sampled image descriptor array was not emitted");
    }
    std::uint32_t pointer = 0;
    if (slotId == 0) {
        pointer = DescriptorElementPointer(state, pointerType, variable, arrayIndex, kind, resource, "sampled image descriptor array was not emitted");
    } else {
        pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, pointerType, pointer, variable, TableImageIndex(state, kind, resource, arrayIndex, slotId));
        DecorateTableNonUniform(state, slotId, pointer);
    }
    const auto image = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, imageType, image, pointer);
    DecorateTableNonUniform(state, slotId, image);
    return image;
}

std::uint32_t LoadSamplerDescriptor(SpirvEmitterState& state, std::uint32_t sampler) {
    const auto arrayIndex = ResourceForDescriptor(state, DescriptorBindingKind::Samplers, sampler);
    const auto samplerType = state.module.Type(spv::OpTypeSampler);
    const auto pointerType = state.module.Type(spv::OpTypePointer, spv::StorageClassUniformConstant, samplerType);
    const auto pointer = DescriptorElementPointer(state, pointerType, state.samplerVariable, arrayIndex, DescriptorBindingKind::Samplers, sampler, "sampler descriptor array was not emitted");
    const auto samplerId = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, samplerType, samplerId, pointer);
    return samplerId;
}

std::uint32_t MakeSampledImage(SpirvEmitterState& state, std::uint32_t resource, std::uint32_t sampler, std::uint32_t slotId) {
    const auto& imageResource = state.program.Info().images.at(resource);
    const auto image = LoadSampledImageDescriptor(state, resource, slotId);
    const auto samplerId = LoadSamplerDescriptor(state, sampler);
    const auto sampledImage = state.module.AllocateId();
    const auto sampledType = state.module.Type(spv::OpTypeSampledImage, ImageType(state, imageResource));
    state.module.AddFunction(spv::OpSampledImage, sampledType, sampledImage, image, samplerId);
    DecorateTableNonUniform(state, slotId, sampledImage);
    return sampledImage;
}

std::uint32_t StorageImageDescriptorPointer(SpirvEmitterState& state, std::uint32_t resource) {
    const auto& image = state.program.Info().images.at(resource);
    if (image.resourceClass != ImageResourceClass::Storage) {
        FailEmit("storage image descriptor requested for a non-storage image");
    }
    if (image.empty) {
        FailEmit("reading an image view that starts past its surface's last mip level is not implemented");
    }
    const auto kind = DescriptorBindingForImage(image);
    const auto arrayIndex = ResourceForDescriptor(state, kind, resource);
    const auto pointerType = state.module.Type(spv::OpTypePointer, spv::StorageClassUniformConstant, ImageType(state, image));
    const auto variable = state.imageVariables.at(ImageBindingIndex(kind));
    return DescriptorElementPointer(state, pointerType, variable, arrayIndex, kind, resource, "storage image descriptor array was not emitted");
}

void EmitStorageImageWrite(SpirvEmitterState& state, std::uint32_t resource, std::uint32_t mipLod, std::uint32_t coord, std::uint32_t texel) {
    const auto& image = state.program.Info().images.at(resource);
    if (image.resourceClass != ImageResourceClass::Storage) {
        FailEmit("storage image write requested for a non-storage image");
    }
    if (image.empty) {
        if (image.atomic) FailEmit("atomics on an image view that starts past its surface's last mip level are not implemented");
        return;
    }
    if (!image.atomic) {
        state.module.EmitCapability(spv::CapabilityStorageImageWriteWithoutFormat);
    }
    const auto kind = DescriptorBindingForImage(image);
    const auto arrayIndex = ResourceForDescriptor(state, kind, resource);
    const auto imageType = ImageType(state, image);
    const auto pointerType = state.module.Type(spv::OpTypePointer, spv::StorageClassUniformConstant, imageType);
    const auto variable = state.imageVariables.at(ImageBindingIndex(kind));
    const auto loadAt = [&](std::uint32_t index) {
        const auto pointer = DescriptorElementPointer(state, pointerType, variable, index, kind, resource, "storage image descriptor array was not emitted");
        const auto descriptor = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, imageType, descriptor, pointer);
        return descriptor;
    };
    if (image.mipMode != ImageMipMode::DynamicStorage) {
        state.module.AddFunction(spv::OpImageWrite, loadAt(arrayIndex), coord, texel);
        return;
    }
    if (image.mipCount == 0u) {
        ExitDescriptorBindingFailure(state, kind, resource, "dynamic storage image has no mip descriptors");
    }
    const auto mergeLabel = state.module.AllocateId();
    std::vector<std::uint32_t> labels(image.mipCount);
    std::vector<std::uint32_t> words{spv::OpSwitch, mipLod, mergeLabel};
    for (std::uint32_t mip = 0; mip < image.mipCount; mip++) {
        labels[mip] = state.module.AllocateId();
        words.push_back(mip);
        words.push_back(labels[mip]);
    }
    state.module.AddFunction(spv::OpSelectionMerge, mergeLabel, spv::SelectionControlMaskNone);
    state.module.AddFunction(std::span<const std::uint32_t>(words));
    for (std::uint32_t mip = 0; mip < image.mipCount; mip++) {
        EmitLabel(state, labels[mip]);
        state.module.AddFunction(spv::OpImageWrite, loadAt(arrayIndex + mip), coord, texel);
        state.module.AddFunction(spv::OpBranch, mergeLabel);
    }
    EmitLabel(state, mergeLabel);
}

const RdnaImageDimensionInfo& RdnaImageDimensionInfoFor(RdnaImageDimension dimension) {
    for (const auto& info : ImageDimensions) {
        if (info.dimension == dimension) {
            return info;
        }
    }
    FailEmit("image dimension " + std::to_string(static_cast<std::uint32_t>(dimension)) + " is invalid");
}

}
