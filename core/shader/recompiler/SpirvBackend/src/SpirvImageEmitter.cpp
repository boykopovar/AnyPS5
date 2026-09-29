#include "SpirvBackend/SpirvImageEmitter.hpp"
#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvBufferFormat.hpp"
#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include <algorithm>
#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ShaderRecompiler {
namespace {

// A bindless table root's runtime slot and whether the key is mapped (SPIR-V ids), both 0 for a
// direct image.
struct TableSelection {
    std::uint32_t slot = 0;
    std::uint32_t mapped = 0;
};

struct ImageEmitAccess {
    const IrValue& inst;
    const MemoryInfo& mem;
    const ImageResource& image;
    const IrValue& address;
    TableSelection table;
    std::uint32_t slot = table.slot;
};

struct SampleSetup {
    const RdnaImageDimensionInfo& dimensionInfo;
    ImageSampleLayout layout;
    IrTextureNumericClass numericClass;
    bool dref;
    std::uint32_t coord;
};

std::uint32_t EffectiveDmask(const MemoryInfo& mem) {
    return mem.dmask != 0u ? mem.dmask : 1u;
}

std::uint32_t DmaskComponentIndex(std::uint32_t dmask, std::uint32_t component) {
    std::uint32_t index = 0;
    for (std::uint32_t i = 0; i < component; i++) {
        index += (dmask >> i) & 1u;
    }
    return index;
}

std::uint32_t DmaskComponent(std::uint32_t dmask, std::uint32_t index) {
    for (std::uint32_t component = 0; component < 4u; component++) {
        if (((dmask >> component) & 1u) != 0u && index-- == 0u) {
            return component;
        }
    }
    throw std::runtime_error("image dmask has fewer components than requested");
}

std::uint32_t ImageGatherComponent(std::uint32_t dmask) {
    switch (dmask) {
        case 0x1u:
            return 0;
        case 0x2u:
            return 1;
        case 0x4u:
            return 2;
        case 0x8u:
            return 3;
        default:
            throw std::runtime_error("image gather dmask must select exactly one component");
    }
}

bool HasFlag(const MemoryInfo& mem, std::uint32_t flag) {
    return (mem.imageSampleFlags & flag) != 0u;
}

std::uint32_t ZeroF32(SpirvEmitterState& state) {
    return ConstantF32(state, 0);
}

std::uint32_t F32BitsToU32(SpirvValueEmitContext& ctx, std::uint32_t value) {
    return Unary(ctx.state, spv::OpBitcast, TypeU32(ctx.state), value);
}

std::uint32_t AddressU32(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t component) {
    const auto layout = GetRdnaImageAddressComponentLayout(access.mem.imageSampleFlags, component);
    const auto packed = layout.bitOffset / 32u;
    if (packed >= access.address.ArgumentCount()) {
        ctx.Fail(access.inst, "has an image address component outside of the address operands");
    }
    auto value = ctx.Def(access.address.Argument(packed));
    if (layout.bitWidth == 16u) {
        if ((layout.bitOffset & 31u) != 0u) {
            value = Binary(ctx.state, spv::OpShiftRightLogical, TypeU32(ctx.state), value, ConstantU32(ctx.state, 16));
        }
        value = Binary(ctx.state, spv::OpBitwiseAnd, TypeU32(ctx.state), value, ConstantU32(ctx.state, 0xffffu));
    }
    return value;
}

std::uint32_t AddressF32(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t component) {
    const auto value = AddressU32(ctx, access, component);
    return GetRdnaImageAddressComponentLayout(access.mem.imageSampleFlags, component).bitWidth == 16u ? EmitF16BitsToF32(ctx.state, value) : Unary(ctx.state, spv::OpBitcast, TypeF32(ctx.state), value);
}

ImageSampleLayout Layout(const MemoryInfo& mem, RdnaImageDimension dimension) {
    ImageSampleLayout layout;
    std::uint32_t cursor = 0;
    const auto& info = RdnaImageDimensionInfoFor(dimension);
    if (HasFlag(mem, RdnaImageSampleFlagOffset)) {
        layout.offset = cursor++;
    }
    if (HasFlag(mem, RdnaImageSampleFlagBias)) {
        layout.bias = cursor++;
    }
    if (HasFlag(mem, RdnaImageSampleFlagCompare)) {
        layout.dref = cursor++;
    }
    if (HasFlag(mem, RdnaImageSampleFlagDerivative)) {
        layout.gradX = cursor;
        cursor += info.spatialComponents;
        layout.gradY = cursor;
        cursor += info.spatialComponents;
    }
    layout.coord = cursor;
    cursor += info.coordinateComponents;
    if (HasFlag(mem, RdnaImageSampleFlagLod)) {
        layout.lod = cursor++;
    }
    if (HasFlag(mem, RdnaImageSampleFlagLodClamp)) {
        layout.clamp = cursor++;
    }
    return layout;
}

std::uint32_t CubeAxis(SpirvEmitterState& state, std::uint32_t value) {
    return Binary(state, spv::OpFSub, TypeF32(state), value, ConstantF32(state, 0x3f800000u));
}

std::uint32_t CubeLayer(SpirvEmitterState& state, std::uint32_t value) {
    const auto guest = state.module.AllocateId();
    state.module.AddFunction(spv::OpConvertFToU, TypeU32(state), guest, value);
    const auto padding = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), Binary(state, spv::OpShiftRightLogical, TypeU32(state), guest, ConstantU32(state, 3)), ConstantU32(state, 1));
    const auto host = Binary(state, spv::OpISub, TypeU32(state), guest, padding);
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpConvertUToF, TypeF32(state), result, host);
    return result;
}

std::uint32_t CoordF32(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t first, std::uint32_t components) {
    if (first == NoImageComponent || access.mem.imageAddressComponents < first + components) {
        ctx.Fail(access.inst, "has an image address with too few coordinate components");
    }
    const bool cube = access.image.cube;
    auto x = AddressF32(ctx, access, first);
    if (components == 1u) {
        return x;
    }
    auto y = AddressF32(ctx, access, first + 1u);
    if (cube) {
        x = CubeAxis(ctx.state, x);
        y = CubeAxis(ctx.state, y);
    }
    const auto result = ctx.state.module.AllocateId();
    if (components == 3u) {
        auto z = AddressF32(ctx, access, first + 2u);
        if (cube) {
            z = CubeLayer(ctx.state, z);
        }
        ctx.state.module.AddFunction(spv::OpCompositeConstruct, TypeF32Vector(ctx.state, 3), result, x, y, z);
    } else {
        ctx.state.module.AddFunction(spv::OpCompositeConstruct, TypeF32Vector(ctx.state, 2), result, x, y);
    }
    return result;
}

std::uint32_t CoordU32(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    const auto components = RdnaImageDimensionInfoFor(access.image.dimension).coordinateComponents;
    if (access.mem.imageAddressComponents < components) {
        ctx.Fail(access.inst, "has an image address with too few coordinate components");
    }
    const auto x = AddressU32(ctx, access, 0);
    if (components == 1u) {
        return x;
    }
    const auto y = AddressU32(ctx, access, 1);
    const auto result = ctx.state.module.AllocateId();
    if (components == 3u) {
        const auto z = AddressU32(ctx, access, 2);
        ctx.state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(ctx.state, 3), result, x, y, z);
    } else {
        ctx.state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(ctx.state, 2), result, x, y);
    }
    return result;
}

std::uint32_t LodU32(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    if (!access.mem.imageHasMip) {
        return ConstantU32(ctx.state, 0);
    }
    const auto component = RdnaImageDimensionInfoFor(access.image.dimension).coordinateComponents;
    if (access.mem.imageAddressComponents <= component) {
        ctx.Fail(access.inst, "has an image address without the mip level component");
    }
    return AddressU32(ctx, access, component);
}

std::uint32_t DrefValueF32(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, const ImageSampleLayout& layout) {
    if (layout.dref == NoImageComponent || access.mem.imageAddressComponents <= layout.dref) {
        ctx.Fail(access.inst, "has no depth reference in the image address");
    }
    return AddressF32(ctx, access, layout.dref);
}

std::uint32_t SampledComponentBits(SpirvValueEmitContext& ctx, std::uint32_t value, IrTextureNumericClass numericClass) {
    if (numericClass == IrTextureNumericClass::Uint) {
        return value;
    }
    return Unary(ctx.state, spv::OpBitcast, TypeU32(ctx.state), value);
}

std::uint32_t SampledComponentZero(SpirvEmitterState& state, IrTextureNumericClass numericClass) {
    switch (numericClass) {
        case IrTextureNumericClass::Float:
            return ZeroF32(state);
        case IrTextureNumericClass::Uint:
            return ConstantU32(state, 0);
        case IrTextureNumericClass::Sint:
            return ConstantI32(state, 0);
        case IrTextureNumericClass::Unsupported:
            break;
    }
    throw std::runtime_error("invalid sampled image numeric class");
}

// An image op's u32x4 result through a bindless table: zeros when the key is not mapped.
std::uint32_t TableResult(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t result) {
    if (access.table.mapped == 0) return result;
    auto& state = ctx.state;
    return Select(state, TypeU32Vector(state, 4), access.table.mapped, result, ConstantU32CompositeZero(state, 4));
}

std::uint32_t ResultVector(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t value, IrTextureNumericClass numericClass, bool dref, bool gather) {
    auto& state = ctx.state;
    const auto& mem = access.mem;
    auto valueClass = numericClass;
    if (dref) {
        valueClass = IrTextureNumericClass::Float;
    }
    const bool integer = valueClass == IrTextureNumericClass::Uint || valueClass == IrTextureNumericClass::Sint;
    if (mem.dataBits == 16u && access.image.depthBits) {
        ctx.Fail(access.inst, "reads the bits of a depth plane as 16-bit results");
    }
    if (mem.dataBits == 16u) {
        if (mem.dataDwords > 4u) {
            ctx.Fail(access.inst, "has more than four packed image result dwords");
        }
        std::uint32_t packed[4] = {ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0)};
        const auto dmask = EffectiveDmask(mem);
        const auto scalar = [&](std::uint32_t index) -> std::uint32_t {
            if (dref) {
                return value;
            }
            const auto component = gather ? index : DmaskComponent(dmask, index);
            const auto result = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeExtract, ImageScalarType(state, valueClass), result, value, component);
            return result;
        };
        for (std::uint32_t word = 0; word < mem.dataDwords; word++) {
            const auto lowIndex = word * 2u;
            const auto highIndex = lowIndex + 1u;
            const auto low = scalar(lowIndex);
            const auto high = highIndex < mem.componentCount ? scalar(highIndex) : SampledComponentZero(state, valueClass);
            if (integer) {
                const auto lowBits = SampledComponentBits(ctx, low, valueClass);
                const auto highBits = SampledComponentBits(ctx, high, valueClass);
                const auto mask = ConstantU32(state, 0xffffu);
                packed[word] = Binary(state, spv::OpBitwiseOr, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), lowBits, mask), Binary(state, spv::OpShiftLeftLogical, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), highBits, mask), ConstantU32(state, 16u)));
            } else {
                const auto pair = state.module.AllocateId();
                state.module.AddFunction(spv::OpCompositeConstruct, TypeF32Vector(state, 2), pair, low, high);
                packed[word] = state.module.AllocateId();
                state.module.AddFunction(spv::OpExtInst, TypeU32(state), packed[word], GlslStd450(state), GLSLstd450PackHalf2x16, pair);
            }
        }
        const auto result = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4), result, packed[0], packed[1], packed[2], packed[3]);
        return result;
    }
    std::uint32_t component[4] = {};
    for (std::uint32_t index = 0; index < 4u; index++) {
        if (dref) {
            component[index] = index == 0u ? F32BitsToU32(ctx, value) : ConstantU32(state, 0);
            continue;
        }
        // Through a depth view read for its bits, only the depth channel (X) is a texel: the view
        // gives 1.0 where the integer view gives 1 (ONE, or W of the single channel).
        const auto selector = (access.image.shaderSwizzle >> (index * 3u)) & 7u;
        if (access.image.depthBits && !gather && selector != 4u) {
            component[index] = ConstantU32(state, selector == 1u || selector == 7u ? 1u : 0u);
            continue;
        }
        const auto scalar = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, ImageScalarType(state, valueClass), scalar, value, index);
        component[index] = SampledComponentBits(ctx, scalar, valueClass);
    }
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4), result, component[0], component[1], component[2], component[3]);
    return result;
}

std::uint32_t QueryDimensions(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    auto& state = ctx.state;
    const auto dimension = access.image.dimension;
    const auto& info = RdnaImageDimensionInfoFor(dimension);
    const auto image = LoadSampledImageDescriptor(state, access.mem.resource, access.slot);
    const auto size = state.module.AllocateId();
    if (info.multisampled != 0u) {
        state.module.AddFunction(spv::OpImageQuerySize, ImageViewSizeType(state, dimension), size, image);
    } else {
        state.module.AddFunction(spv::OpImageQuerySizeLod, ImageViewSizeType(state, dimension), size, image, AddressU32(ctx, access, 0));
    }
    const auto components = info.coordinateComponents;
    std::uint32_t result[4] = {ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0)};
    if (components == 1u) {
        result[0] = size;
    } else {
        for (std::uint32_t index = 0; index < components; index++) {
            result[index] = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), result[index], size, index);
        }
    }
    if (info.multisampled == 0u) {
        result[3] = state.module.AllocateId();
        state.module.AddFunction(spv::OpImageQueryLevels, TypeU32(state), result[3], image);
    }
    const auto vector = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4), vector, result[0], result[1], result[2], result[3]);
    return vector;
}

std::uint32_t PackedOffset(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, const ImageSampleLayout& layout) {
    auto& state = ctx.state;
    if (layout.offset == NoImageComponent || access.mem.imageAddressComponents <= layout.offset) {
        ctx.Fail(access.inst, "has no texel offset in the image address");
    }
    const auto components = RdnaImageDimensionInfoFor(access.image.dimension).spatialComponents;
    const auto packed = Unary(state, spv::OpBitcast, TypeI32(state), AddressU32(ctx, access, layout.offset));
    std::uint32_t values[3] = {};
    for (std::uint32_t index = 0; index < components; index++) {
        values[index] = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitFieldSExtract, TypeI32(state), values[index], packed, ConstantU32(state, index * 8u), ConstantU32(state, 6));
    }
    if (components == 1u) {
        return values[0];
    }
    const auto result = state.module.AllocateId();
    if (components == 3u) {
        state.module.AddFunction(spv::OpCompositeConstruct, TypeI32Vector(state, 3), result, values[0], values[1], values[2]);
    } else {
        state.module.AddFunction(spv::OpCompositeConstruct, TypeI32Vector(state, 2), result, values[0], values[1]);
    }
    return result;
}

std::uint32_t HorizontalOffsets(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    auto& state = ctx.state;
    const auto components = RdnaImageDimensionInfoFor(access.image.dimension).spatialComponents;
    if (components != 1u && components != 2u) {
        ctx.Fail(access.inst, "has horizontal gather offsets for an unsupported image dimension");
    }
    const auto count = ConstantU32(state, 4);
    const auto elementType = components == 1u ? TypeI32(state) : TypeI32Vector(state, 2);
    const auto arrayType = state.module.Type(spv::OpTypeArray, elementType, count);
    std::uint32_t offsets[4] = {};
    for (std::uint32_t index = 0; index < 4u; index++) {
        const auto x = ConstantI32(state, static_cast<std::int32_t>(index) - 1);
        if (components == 1u) {
            offsets[index] = x;
        } else {
            offsets[index] = state.module.Constant(spv::OpConstantComposite, TypeI32Vector(state, 2), x, ConstantI32(state, 0));
        }
    }
    return state.module.Constant(spv::OpConstantComposite, arrayType, offsets[0], offsets[1], offsets[2], offsets[3]);
}

std::uint32_t InverseSwizzle(std::uint32_t swizzle, std::uint32_t component) {
    for (std::uint32_t source = 0; source < 4u; source++) {
        if (((swizzle >> (source * 3u)) & 7u) == 4u + component) {
            return source;
        }
    }
    return NoImageComponent;
}

SpirvBufferFormatInfo ImageConversionFormat(const ImageResource& image) {
    const auto format = image.conversionFormat;
    if (format == IrBufferFormat::Invalid) {
        return {};
    }
    const auto info = GetFormatInfo(format);
    if (SampledTextureNumericClass(format) != IrTextureNumericClass::Uint || RemapTextureFormat(format) == format || info.type != SpirvFormatComponentType::Uint || !info.packedBitfield || info.byteSize != sizeof(std::uint32_t) || info.componentCount == 0u || info.componentCount > 4u) {
        throw std::runtime_error("image conversion format is not a packed 32-bit unsigned integer format");
    }
    return info;
}

std::uint32_t UnpackImageTexel(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t texel) {
    auto& state = ctx.state;
    const auto info = ImageConversionFormat(access.image);
    if (info.format == IrBufferFormat::Invalid) {
        return texel;
    }
    const auto packed = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), packed, texel, 0u);
    std::uint32_t components[4] = {ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0)};
    for (std::uint32_t component = 0; component < info.componentCount; component++) {
        components[component] = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitFieldUExtract, TypeU32(state), components[component], packed, ConstantU32(state, info.componentBitOffset[component]), ConstantU32(state, info.componentBits[component]));
    }
    for (std::uint32_t component = info.componentCount; component < 4u; component++) {
        components[component] = components[component % info.componentCount];
    }
    const auto swizzle = access.image.shaderSwizzle;
    std::uint32_t selected[4] = {};
    for (std::uint32_t component = 0; component < 4u; component++) {
        const auto selector = (swizzle >> (component * 3u)) & 7u;
        if (selector == 1u) {
            selected[component] = ConstantU32(state, 1u);
        } else if (selector >= 4u) {
            selected[component] = components[selector - 4u];
        } else {
            selected[component] = ConstantU32(state, 0u);
        }
    }
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4), result, selected[0], selected[1], selected[2], selected[3]);
    return result;
}

std::uint32_t UnpackImageGather(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t gathered) {
    auto& state = ctx.state;
    const auto info = ImageConversionFormat(access.image);
    if (info.format == IrBufferFormat::Invalid) {
        return gathered;
    }
    const auto component = ImageGatherComponent(EffectiveDmask(access.mem));
    const auto selector = (access.image.shaderSwizzle >> (component * 3u)) & 7u;
    if (selector < 4u) {
        const auto value = ConstantU32(state, selector == 1u ? 1u : 0u);
        return state.module.Constant(spv::OpConstantComposite, TypeU32Vector(state, 4), value, value, value, value);
    }
    std::uint32_t values[4] = {};
    for (std::uint32_t lane = 0; lane < 4u; lane++) {
        const auto packed = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), packed, gathered, lane);
        values[lane] = state.module.AllocateId();
        const auto physical = (selector - 4u) % info.componentCount;
        state.module.AddFunction(spv::OpBitFieldUExtract, TypeU32(state), values[lane], packed, ConstantU32(state, info.componentBitOffset[physical]), ConstantU32(state, info.componentBits[physical]));
    }
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4), result, values[0], values[1], values[2], values[3]);
    return result;
}

std::uint32_t EmitOneDimensionalGatherLz(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t coord) {
    auto& state = ctx.state;
    const auto numericClass = access.image.numericClass;
    state.module.EmitCapability(spv::CapabilityImageQuery);
    const auto image = LoadSampledImageDescriptor(state, access.mem.resource, access.slot);
    const auto width = state.module.AllocateId();
    state.module.AddFunction(spv::OpImageQuerySizeLod, TypeU32(state), width, image, ConstantU32(state, 0));
    const auto widthF32 = state.module.AllocateId();
    state.module.AddFunction(spv::OpConvertUToF, TypeF32(state), widthF32, width);
    const auto left = state.module.AllocateId();
    state.module.AddFunction(spv::OpExtInst, TypeF32(state), left, GlslStd450(state), GLSLstd450Floor, Binary(state, spv::OpFSub, TypeF32(state), Binary(state, spv::OpFMul, TypeF32(state), coord, widthF32), ConstantF32(state, 0x3f000000u)));
    const auto sampled = MakeSampledImage(state, access.mem.resource, access.mem.sampler, access.slot);
    const auto vectorType = ImageVectorType(state, numericClass, 4);
    const auto scalarType = ImageScalarType(state, numericClass);
    const auto component = ImageConversionFormat(access.image).format == IrBufferFormat::Invalid ? ImageGatherComponent(EffectiveDmask(access.mem)) : 0u;
    std::uint32_t values[2] = {};
    for (std::uint32_t index = 0; index < 2u; index++) {
        const auto sampleCoord = Binary(state, spv::OpFDiv, TypeF32(state), Binary(state, spv::OpFAdd, TypeF32(state), left, ConstantF32(state, index == 0u ? 0x3f000000u : 0x3fc00000u)), widthF32);
        const auto texel = state.module.AllocateId();
        state.module.AddFunction(spv::OpImageSampleExplicitLod, vectorType, texel, sampled, sampleCoord, spv::ImageOperandsLodMask, ZeroF32(state));
        values[index] = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, scalarType, values[index], texel, component);
    }
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, vectorType, result, values[0], values[1], values[1], values[0]);
    return result;
}

std::uint32_t PackImageTexel(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t texel) {
    auto& state = ctx.state;
    const auto info = ImageConversionFormat(access.image);
    if (info.format == IrBufferFormat::Invalid) {
        return texel;
    }
    auto packed = ConstantU32(state, 0u);
    for (std::uint32_t component = 0; component < info.componentCount; component++) {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), value, texel, component);
        const auto maximum = ConstantU32(state, info.componentBits[component] == 32u ? UINT32_MAX : (1u << info.componentBits[component]) - 1u);
        const auto within = Binary(state, spv::OpULessThan, TypeBool(state), value, maximum);
        const auto clamped = Select(state, TypeU32(state), within, value, maximum);
        const auto shifted = info.componentBitOffset[component] == 0u ? clamped : Binary(state, spv::OpShiftLeftLogical, TypeU32(state), clamped, ConstantU32(state, info.componentBitOffset[component]));
        packed = Binary(state, spv::OpBitwiseOr, TypeU32(state), packed, shifted);
    }
    const auto zero = ConstantU32(state, 0u);
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4), result, packed, zero, zero, zero);
    return result;
}

std::uint32_t StoreTexel(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t data, bool integer) {
    auto& state = ctx.state;
    const auto& mem = access.mem;
    const auto swizzle = access.image.shaderSwizzle;
    std::uint32_t values[4] = {};
    const auto dmask = EffectiveDmask(mem);
    for (std::uint32_t component = 0; component < 4u; component++) {
        const auto source = InverseSwizzle(swizzle, component);
        auto raw = ConstantU32(state, 0);
        if (source < 4u && ((dmask >> source) & 1u) != 0u) {
            const auto packedIndex = DmaskComponentIndex(dmask, source);
            raw = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), raw, data, mem.dataBits == 16u ? packedIndex / 2u : packedIndex);
            if (mem.dataBits == 16u) {
                if ((packedIndex & 1u) != 0u) {
                    raw = Binary(state, spv::OpShiftRightLogical, TypeU32(state), raw, ConstantU32(state, 16u));
                }
                raw = Binary(state, spv::OpBitwiseAnd, TypeU32(state), raw, ConstantU32(state, 0xffffu));
            }
        }
        values[component] = integer ? raw : mem.dataBits == 16u ? EmitF16BitsToF32(state, raw) : Unary(state, spv::OpBitcast, TypeF32(state), raw);
    }
    const auto texel = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, integer ? TypeU32Vector(state, 4) : TypeF32Vector(state, 4), texel, values[0], values[1], values[2], values[3]);
    return PackImageTexel(ctx, access, texel);
}

std::uint32_t ImageAtomicOpcode(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::ImageAtomicSwap32:
            return spv::OpAtomicExchange;
        case IrOpcode::ImageAtomicIAdd32:
            return spv::OpAtomicIAdd;
        case IrOpcode::ImageAtomicUMin32:
            return spv::OpAtomicUMin;
        case IrOpcode::ImageAtomicUMax32:
            return spv::OpAtomicUMax;
        case IrOpcode::ImageAtomicAnd32:
            return spv::OpAtomicAnd;
        case IrOpcode::ImageAtomicOr32:
            return spv::OpAtomicOr;
        case IrOpcode::ImageAtomicXor32:
            return spv::OpAtomicXor;
        default:
            throw std::runtime_error("opcode is not an image atomic");
    }
}

const ImageResource& ImageResourceOf(const SpirvEmitterState& state, const MemoryInfo& mem) {
    return state.program.Resources().info.images.at(mem.resource);
}

void EmitQueryDimensionsOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    ctx.state.module.EmitCapability(spv::CapabilityImageQuery);
    ctx.Define(access.inst, TableResult(ctx, access, QueryDimensions(ctx, access)));
}

void EmitQueryLodOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    auto& state = ctx.state;
    state.module.EmitCapability(spv::CapabilityImageQuery);
    const auto sampled = MakeSampledImage(state, access.mem.resource, access.mem.sampler, access.slot);
    const auto coord = CoordF32(ctx, access, 0, RdnaImageDimensionInfoFor(access.image.dimension).spatialComponents);
    const auto lod = state.module.AllocateId();
    state.module.AddFunction(spv::OpImageQueryLod, TypeF32Vector(state, 2), lod, sampled, coord);
    std::uint32_t values[4] = {ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0)};
    for (std::uint32_t index = 0; index < 2u; index++) {
        const auto component = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), component, lod, index);
        values[index] = F32BitsToU32(ctx, component);
    }
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4), result, values[0], values[1], values[2], values[3]);
    ctx.Define(access.inst, TableResult(ctx, access, result));
}

void EmitReadOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    auto& state = ctx.state;
    const auto& dimensionInfo = RdnaImageDimensionInfoFor(access.image.dimension);
    const auto numericClass = access.image.numericClass;
    const auto condition = ctx.Arg(access.inst, 2);
    ctx.Define(access.inst, EmitValueOrDefaultIfCondition(state, condition, TypeU32Vector(state, 4), ConstantU32CompositeZero(state, 4), [&]() {
        const auto descriptor = LoadSampledImageDescriptor(state, access.mem.resource, access.slot);
        const auto color = state.module.AllocateId();
        const auto coord = CoordU32(ctx, access);
        if (dimensionInfo.multisampled != 0u) {
            if (access.mem.imageAddressComponents <= dimensionInfo.coordinateComponents) {
                ctx.Fail(access.inst, "has no sample index in the image address");
            }
            state.module.AddFunction(spv::OpImageFetch, ImageVectorType(state, numericClass, 4), color, descriptor, coord, spv::ImageOperandsSampleMask, AddressU32(ctx, access, dimensionInfo.coordinateComponents));
        } else {
            state.module.AddFunction(spv::OpImageFetch, ImageVectorType(state, numericClass, 4), color, descriptor, coord, spv::ImageOperandsLodMask, LodU32(ctx, access));
        }
        return TableResult(ctx, access, ResultVector(ctx, access, UnpackImageTexel(ctx, access, color), numericClass, false, false));
    }));
}

void EmitWriteOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    auto& state = ctx.state;
    if (access.slot != 0) {
        ctx.Fail(access.inst, "stores through a bindless image table are unsupported");
    }
    const bool uintImage = access.image.numericClass == IrTextureNumericClass::Uint;
    EmitIfCondition(state, ctx.Arg(access.inst, 3), [&]() {
        const auto mipLod = access.image.mipMode == ImageMipMode::DynamicStorage ? LodU32(ctx, access) : 0u;
        const auto coord = CoordU32(ctx, access);
        const auto texel = StoreTexel(ctx, access, ctx.Arg(access.inst, 2), uintImage);
        EmitStorageImageWrite(state, access.mem.resource, mipLod, coord, texel);
    });
}

void EmitAtomicOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    auto& state = ctx.state;
    if (access.slot != 0) {
        ctx.Fail(access.inst, "atomics through a bindless image table are unsupported");
    }
    const auto atomicOpcode = ImageAtomicOpcode(access.inst.Opcode());
    ctx.Define(access.inst, EmitValueOrZeroIfCondition(state, ctx.Arg(access.inst, 3), [&]() {
        const auto pointer = state.module.AllocateId();
        const auto pointerType = TypePointer(state, spv::StorageClassImage, TypeU32(state));
        state.module.AddFunction(spv::OpImageTexelPointer, pointerType, pointer, StorageImageDescriptorPointer(state, access.mem.resource), CoordU32(ctx, access), ConstantU32(state, 0));
        const auto old = state.module.AllocateId();
        state.module.AddFunction(atomicOpcode, TypeU32(state), old, pointer, ConstantU32(state, spv::ScopeDevice), ConstantU32(state, spv::MemorySemanticsMaskNone), ctx.Arg(access.inst, 2));
        EmitDeviceAtomicMemoryBarrier(state);
        return old;
    }));
}

SampleSetup MakeSampleSetup(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    const auto& dimensionInfo = RdnaImageDimensionInfoFor(access.image.dimension);
    const auto layout = Layout(access.mem, access.image.dimension);
    const bool dref = HasFlag(access.mem, RdnaImageSampleFlagCompare);
    if (dref && access.image.conversionFormat != IrBufferFormat::Invalid) {
        ctx.Fail(access.inst, "uses depth comparison with a packed integer image");
    }
    const auto coord = CoordF32(ctx, access, layout.coord, dimensionInfo.coordinateComponents);
    return {dimensionInfo, layout, access.image.numericClass, dref, coord};
}

void EmitGatherOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, const SampleSetup& setup) {
    auto& state = ctx.state;
    const auto& mem = access.mem;
    const auto dimension = access.image.dimension;
    if (setup.layout.clamp != NoImageComponent) {
        ctx.Fail(access.inst, "is a gather with an LOD clamp, which is not implemented");
    }
    if (dimension == RdnaImageDimension::Dim1D) {
        if (setup.dref || !HasFlag(mem, RdnaImageSampleFlagLevelZero) || HasFlag(mem, RdnaImageSampleFlagOffset) || HasFlag(mem, RdnaImageSampleFlagGatherHorizontal)) {
            ctx.Fail(access.inst, "has an unsupported 1D gather variant");
        }
        const auto sample = EmitOneDimensionalGatherLz(ctx, access, setup.coord);
        ctx.Define(access.inst, TableResult(ctx, access, ResultVector(ctx, access, UnpackImageGather(ctx, access, sample), setup.numericClass, false, true)));
        return;
    }
    if (dimension == RdnaImageDimension::Dim1DArray) {
        ctx.Fail(access.inst, "has an unsupported 1D-array gather");
    }
    const auto sampled = MakeSampledImage(state, mem.resource, mem.sampler, access.slot);
    const auto sample = state.module.AllocateId();
    std::vector<std::uint32_t> words;
    if (setup.dref) {
        const auto drefValue = DrefValueF32(ctx, access, setup.layout);
        words = {spv::OpImageDrefGather, TypeF32Vector(state, 4), sample, sampled, setup.coord, drefValue};
    } else {
        std::uint32_t component = 0;
        if (ImageConversionFormat(access.image).format == IrBufferFormat::Invalid) {
            component = ImageGatherComponent(EffectiveDmask(mem));
        }
        words = {spv::OpImageGather, ImageVectorType(state, setup.numericClass, 4), sample, sampled, setup.coord, ConstantU32(state, component)};
    }
    if (HasFlag(mem, RdnaImageSampleFlagGatherHorizontal)) {
        words.push_back(spv::ImageOperandsConstOffsetsMask);
        words.push_back(HorizontalOffsets(ctx, access));
    } else if (setup.layout.offset != NoImageComponent) {
        words.push_back(spv::ImageOperandsOffsetMask);
        words.push_back(PackedOffset(ctx, access, setup.layout));
    }
    state.module.AddFunction(words);
    auto resultNumericClass = setup.numericClass;
    if (setup.dref) {
        resultNumericClass = IrTextureNumericClass::Float;
    }
    // Debug aid: APS5_GATHER_CONST=<n> makes every non-depth gather return that integer (or its
    // float value) in all four texels, to separate a wrong gather result from wrong math after it.
    static const char* gatherConstText = std::getenv("APS5_GATHER_CONST");
    auto gathered = sample;
    if (gatherConstText != nullptr && !setup.dref) {
        const auto value = static_cast<std::uint32_t>(std::strtoul(gatherConstText, nullptr, 0));
        const bool integer = resultNumericClass == IrTextureNumericClass::Uint || resultNumericClass == IrTextureNumericClass::Sint;
        const auto component = integer ? ConstantU32(state, value) : ConstantF32(state, std::bit_cast<std::uint32_t>(static_cast<float>(value)));
        gathered = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeConstruct, ImageVectorType(state, resultNumericClass, 4), gathered, component, component, component, component);
    }
    ctx.Define(access.inst, TableResult(ctx, access, ResultVector(ctx, access, UnpackImageGather(ctx, access, gathered), resultNumericClass, false, true)));
}

// The bound slot of a bindless table for the runtime key: a binary search over the (key, slot)
// pairs the materializer appended to the flattened SRT. A key the mapping lacks (past the table,
// or a null or unusable entry) selects slot 0 and reports unmapped: its result is zeroed, as a
// null T# samples on hardware.
TableSelection EmitIndirectImageSelector(SpirvValueEmitContext& ctx, const ImageResource& image, std::uint32_t key) {
    auto& state = ctx.state;
    const auto loadMapping = [&](std::uint32_t index) {
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypeStorageBufferElementPointer(state), pointer, state.flattenedSrtVariable, ConstantU32(state, 0), index);
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
        return value;
    };
    const auto mapping = ConstantU32(state, image.indirectMappingOffset);
    auto low = ConstantU32(state, 0u);
    auto high = loadMapping(mapping);
    auto selected = ConstantU32(state, 0u);
    auto mapped = ConstantBool(state, false);
    for (std::uint32_t iteration = 0; iteration < image.indirectSearchIterations; iteration++) {
        const auto active = Binary(state, spv::OpULessThan, TypeBool(state), low, high);
        const auto mid = Binary(state, spv::OpShiftRightLogical, TypeU32(state), Binary(state, spv::OpIAdd, TypeU32(state), low, high), ConstantU32(state, 1u));
        const auto probe = Select(state, TypeU32(state), active, mid, ConstantU32(state, 0u));
        const auto entry = Binary(state, spv::OpIAdd, TypeU32(state), mapping, Binary(state, spv::OpIAdd, TypeU32(state), Binary(state, spv::OpShiftLeftLogical, TypeU32(state), probe, ConstantU32(state, 1u)), ConstantU32(state, 1u)));
        const auto mappedKey = loadMapping(entry);
        const auto candidate = loadMapping(Binary(state, spv::OpIAdd, TypeU32(state), entry, ConstantU32(state, 1u)));
        const auto equal = Binary(state, spv::OpIEqual, TypeBool(state), mappedKey, key);
        const auto match = Binary(state, spv::OpLogicalAnd, TypeBool(state), active, equal);
        selected = Select(state, TypeU32(state), match, candidate, selected);
        mapped = Binary(state, spv::OpLogicalOr, TypeBool(state), mapped, match);
        const auto less = Binary(state, spv::OpULessThan, TypeBool(state), mappedKey, key);
        const auto takeUpper = Binary(state, spv::OpLogicalAnd, TypeBool(state), active, less);
        const auto takeLower = Binary(state, spv::OpLogicalAnd, TypeBool(state), active, Unary(state, spv::OpLogicalNot, TypeBool(state), less));
        low = Select(state, TypeU32(state), takeUpper, Binary(state, spv::OpIAdd, TypeU32(state), mid, ConstantU32(state, 1u)), low);
        high = Select(state, TypeU32(state), takeLower, mid, high);
    }
    return {selected, mapped};
}

void EmitSampleOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, const SampleSetup& setup) {
    auto& state = ctx.state;
    const auto& mem = access.mem;
    const auto& image = access.image;
    const bool explicitLod = HasFlag(mem, RdnaImageSampleFlagDerivative) || HasFlag(mem, RdnaImageSampleFlagLod) || HasFlag(mem, RdnaImageSampleFlagLevelZero) || state.program.Resources().stage != IrShaderStage::Pixel;
    std::uint32_t opcode = spv::OpImageSampleImplicitLod;
    if (explicitLod) {
        opcode = setup.dref ? spv::OpImageSampleDrefExplicitLod : spv::OpImageSampleExplicitLod;
    } else if (setup.dref) {
        opcode = spv::OpImageSampleDrefImplicitLod;
    }
    std::uint32_t resultType = ImageVectorType(state, setup.numericClass, 4);
    std::uint32_t drefValue = 0;
    if (setup.dref) {
        resultType = TypeF32(state);
        drefValue = DrefValueF32(ctx, access, setup.layout);
    }
    std::uint32_t operandMask = 0;
    std::vector<std::uint32_t> operands;
    if (HasFlag(mem, RdnaImageSampleFlagDerivative)) {
        operandMask |= spv::ImageOperandsGradMask;
        operands.push_back(CoordF32(ctx, access, setup.layout.gradX, setup.dimensionInfo.spatialComponents));
        operands.push_back(CoordF32(ctx, access, setup.layout.gradY, setup.dimensionInfo.spatialComponents));
    } else if (explicitLod) {
        operandMask |= spv::ImageOperandsLodMask;
        operands.push_back(HasFlag(mem, RdnaImageSampleFlagLod) ? AddressF32(ctx, access, setup.layout.lod) : ZeroF32(state));
    } else if (setup.layout.bias != NoImageComponent) {
        operandMask |= spv::ImageOperandsBiasMask;
        operands.push_back(AddressF32(ctx, access, setup.layout.bias));
    }
    if (setup.layout.clamp != NoImageComponent) {
        const auto& capabilities = state.requirements.capabilities;
        if (std::find(capabilities.begin(), capabilities.end(), static_cast<std::uint32_t>(spv::CapabilityMinLod)) == capabilities.end()) ctx.Fail(access.inst, "clamps its LOD, which needs the device's shaderResourceMinLod");
        const auto clamp = AddressF32(ctx, access, setup.layout.clamp);
        if ((operandMask & spv::ImageOperandsLodMask) != 0u) {
            // An explicit level takes no MinLod operand: the level itself is clamped.
            const auto clamped = state.module.AllocateId();
            state.module.AddFunction(spv::OpExtInst, TypeF32(state), clamped, GlslStd450(state), GLSLstd450FMax, operands.back(), clamp);
            operands.back() = clamped;
        } else {
            operandMask |= spv::ImageOperandsMinLodMask;
            operands.push_back(clamp);
        }
    }
    const auto sampled = MakeSampledImage(state, mem.resource, mem.sampler, access.slot);
    const auto sample = state.module.AllocateId();
    std::vector<std::uint32_t> words = {opcode, resultType, sample, sampled, setup.coord};
    if (setup.dref) {
        words.push_back(drefValue);
    }
    if (operandMask != 0u) {
        words.push_back(operandMask);
        words.insert(words.end(), operands.begin(), operands.end());
    }
    state.module.AddFunction(words);
    const auto result = setup.dref ? sample : UnpackImageTexel(ctx, access, sample);
    ctx.Define(access.inst, TableResult(ctx, access, ResultVector(ctx, access, result, setup.numericClass, setup.dref, false)));
}

// A bindless table root's selection for this access (see EmitIndirectImageSelector), ids 0 for
// a direct image.
TableSelection TableSlot(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, const ImageResource& image) {
    auto& state = ctx.state;
    if (image.indirectRoot != mem.resource) {
        return {};
    }
    const auto* handle = inst.Argument(0);
    const auto& sources = state.program.Resources().descriptorSources;
    if (handle == nullptr || image.source >= sources.size() || !sources[image.source].indirectImage.has_value() || sources[image.source].indirectImage->keyArg >= handle->ArgumentCount()) {
        ctx.Fail(inst, "has invalid bindless image table key provenance");
    }
    if (state.flattenedSrtVariable == 0 || image.indirectSearchIterations == 0u) {
        ctx.Fail(inst, "has no bindless image table runtime mapping");
    }
    const auto key = ctx.Def(handle->Argument(sources[image.source].indirectImage->keyArg));
    return EmitIndirectImageSelector(ctx, image, key);
}

void EmitSamplingOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    const auto setup = MakeSampleSetup(ctx, access);
    if (access.inst.Opcode() == IrOpcode::ImageGatherRaw) {
        EmitGatherOp(ctx, access, setup);
    } else {
        EmitSampleOp(ctx, access, setup);
    }
}

}

void EmitImageOperation(SpirvModule& module, const IrValue& value, std::uint32_t resultId) {
    throw std::logic_error("EmitImageOperation(SpirvModule&) is not used: image instructions are emitted per opcode through EmitImage");
}

void EmitImageOperation(SpirvValueEmitContext& context, const IrValue& value, std::uint32_t resultId) {
    throw std::logic_error("EmitImageOperation(SpirvValueEmitContext&) is not used: image instructions are emitted per opcode through EmitImage");
}

void EmitImage(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto irOpcode = inst.Opcode();
    const auto imageInfo = ImageOpcodeInfoOf(irOpcode);
    const auto& mem = ctx.Memory(inst);
    ctx.ResourceIndex(inst.Argument(0), IrOpcode::GetImageResource);
    const auto& image = ImageResourceOf(ctx.state, mem);
    const auto* address = ctx.ImageAddress(inst.Argument(imageInfo.needsSampler ? 2 : 1));
    if (address == nullptr) {
        ctx.Fail(inst, "has no image address");
    }
    const ImageEmitAccess access{inst, mem, image, *address, TableSlot(ctx, inst, mem, image)};
    switch (irOpcode) {
        case IrOpcode::ImageQueryDimensions:
            EmitQueryDimensionsOp(ctx, access);
            return;
        case IrOpcode::ImageQueryLod:
            EmitQueryLodOp(ctx, access);
            return;
        case IrOpcode::ImageRead:
            EmitReadOp(ctx, access);
            return;
        case IrOpcode::ImageWrite:
            EmitWriteOp(ctx, access);
            return;
        case IrOpcode::ImageSampleRaw:
        case IrOpcode::ImageGatherRaw:
            EmitSamplingOp(ctx, access);
            return;
        case IrOpcode::ImageAtomicSwap32:
        case IrOpcode::ImageAtomicIAdd32:
        case IrOpcode::ImageAtomicUMin32:
        case IrOpcode::ImageAtomicUMax32:
        case IrOpcode::ImageAtomicAnd32:
        case IrOpcode::ImageAtomicOr32:
        case IrOpcode::ImageAtomicXor32:
            EmitAtomicOp(ctx, access);
            return;
        default:
            ctx.Fail(inst, "has no image SPIR-V emitter");
    }
}

void EmitGetImageResource(SpirvValueEmitContext& context) {
    EmitVoid(context);
}

void EmitGetSamplerResource(SpirvValueEmitContext& context) {
    EmitVoid(context);
}

void EmitMakeImageAddress(SpirvValueEmitContext& context) {
    EmitVoid(context);
}

void EmitImageQueryDimensions(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageQueryLod(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageRead(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageWrite(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageSampleRaw(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageGatherRaw(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicSwap32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicIAdd32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicUMin32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicUMax32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicAnd32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicOr32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicXor32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

// image_bvh_intersect_ray tests one ray against one BVH node; the guest shader owns the traversal
// and its stack. This is the GFX10 (RT IP 1.1) node test as AMD's GPURT software fallback defines it
// (IntersectCommon.hlsl), with the node layouts of GPURT's gfx10 TriangleNode1_0 and BoxNode1_0, as
// SharpEmu ports it. The node is read through the BDA page table; an inactive lane, a null BVH, an
// out-of-range node or another node type returns four invalid dwords.
void EmitImageBvhIntersectRay(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto u32 = TypeU32(state);
    const auto u64 = TypeScalarU64(state);
    const auto f32 = TypeF32(state);
    const auto boolean = TypeBool(state);
    const auto result4 = TypeU32Composite(state, 4u);
    const auto invalid = ConstantU32(state, 0xffffffffu);
    const auto uint = [&](std::uint32_t value) { return ConstantU32(state, value); };
    const auto real = [&](std::uint32_t bits) { return ConstantF32(state, bits); };
    const auto op = [&](spv::Op opcode, std::uint32_t type, std::uint32_t lhs, std::uint32_t rhs) { return Binary(state, opcode, type, lhs, rhs); };
    const auto compose = [&](const std::array<std::uint32_t, 4>& values) {
        const auto id = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeConstruct, result4, id, values[0], values[1], values[2], values[3]);
        return id;
    };
    const auto asFloat = [&](std::uint32_t value) { return Unary(state, spv::OpBitcast, f32, value); };
    const auto asUint = [&](std::uint32_t value) { return Unary(state, spv::OpBitcast, u32, value); };
    const auto selectF = [&](std::uint32_t condition, std::uint32_t whenTrue, std::uint32_t whenFalse) { return Select(state, f32, condition, whenTrue, whenFalse); };
    const auto selectU = [&](std::uint32_t condition, std::uint32_t whenTrue, std::uint32_t whenFalse) { return Select(state, u32, condition, whenTrue, whenFalse); };
    const auto both = [&](std::uint32_t lhs, std::uint32_t rhs) { return op(spv::OpLogicalAnd, boolean, lhs, rhs); };
    const auto either = [&](std::uint32_t lhs, std::uint32_t rhs) { return op(spv::OpLogicalOr, boolean, lhs, rhs); };
    const auto isNan = [&](std::uint32_t value) { return Unary(state, spv::OpIsNan, boolean, value); };
    const auto widen = [&](std::uint32_t value) { return Unary(state, spv::OpUConvert, u64, value); };
    const auto pair = [&](std::uint32_t low, std::uint32_t high) { return op(spv::OpBitwiseOr, u64, widen(low), op(spv::OpShiftLeftLogical, u64, widen(high), BdaConstant(state, 32u))); };

    const auto query = [&] {
        const auto descriptor = ctx.Arg(inst, 0);
        std::array<std::uint32_t, 4> word{};
        for (std::uint32_t i = 0; i < 4u; ++i) {
            word[i] = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeExtract, u32, word[i], descriptor, i);
        }
        const IrValue* address = ctx.ImageAddress(inst.Argument(1));
        const auto component = [&](std::uint32_t index) { return ctx.Arg(*address, index); };
        const bool a16 = (ctx.Memory(inst).imageSampleFlags & RdnaImageSampleFlagA16) != 0u;

        // The node pointer: its byte offset divided by eight above bits 2:0, which hold the type
        // (0-1 triangle, 4 float16 box, 5 float32 box). The T#: base_address[39:0] in 256-byte
        // units, box_grow_value[62:55], box_sort_en[63], size[105:64] (the last valid 64-byte
        // node), triangle_return_mode[120].
        const auto node = component(0);
        const auto type = op(spv::OpBitwiseAnd, u32, node, uint(7u));
        const auto nodeIndex = op(spv::OpShiftRightLogical, u64, widen(node), BdaConstant(state, 3u));
        const auto baseUnits = pair(word[0], op(spv::OpBitwiseAnd, u32, word[1], uint(0xffu)));
        const auto lastNode = pair(word[2], op(spv::OpBitwiseAnd, u32, word[3], uint(0x3ffu)));
        const auto boxGrow = op(spv::OpBitwiseAnd, u32, op(spv::OpShiftRightLogical, u32, word[1], uint(23u)), uint(0xffu));
        const auto boxSort = op(spv::OpINotEqual, boolean, op(spv::OpBitwiseAnd, u32, word[1], uint(0x80000000u)), uint(0u));
        const auto barycentrics = op(spv::OpINotEqual, boolean, op(spv::OpBitwiseAnd, u32, word[3], uint(1u << 24u)), uint(0u));
        const auto nodeAddress = op(spv::OpIAdd, u64, op(spv::OpShiftLeftLogical, u64, baseUnits, BdaConstant(state, 8u)), op(spv::OpShiftLeftLogical, u64, widen(op(spv::OpBitwiseAnd, u32, node, uint(~7u))), BdaConstant(state, 3u)));
        const auto present = op(spv::OpINotEqual, boolean, baseUnits, BdaConstant(state, 0u));
        const auto valid = both(present, op(spv::OpULessThanEqual, boolean, nodeIndex, lastNode));
        const auto isTriangle = both(valid, op(spv::OpULessThanEqual, boolean, type, uint(1u)));
        const auto isBox16 = both(valid, op(spv::OpIEqual, boolean, type, uint(4u)));
        // A float32 box node spans two 64-byte units.
        const auto isBox32 = both(both(present, op(spv::OpULessThan, boolean, nodeIndex, lastNode)), op(spv::OpIEqual, boolean, type, uint(5u)));

        const auto extent = asFloat(component(1));
        std::array<std::uint32_t, 3> origin{};
        std::array<std::uint32_t, 3> direction{};
        std::array<std::uint32_t, 3> inverse{};
        for (std::uint32_t axis = 0; axis < 3u; ++axis) {
            origin[axis] = asFloat(component(2u + axis));
            if (!a16) {
                direction[axis] = asFloat(component(5u + axis));
                inverse[axis] = asFloat(component(8u + axis));
                continue;
            }
            // Six halves in three dwords: the direction, then the inverse direction.
            const auto half = [&](std::uint32_t index) {
                const auto packed = component(5u + index / 2u);
                return EmitF16BitsToF32(state, index % 2u == 0u ? op(spv::OpBitwiseAnd, u32, packed, uint(0xffffu)) : op(spv::OpShiftRightLogical, u32, packed, uint(16u)));
            };
            direction[axis] = half(axis);
            inverse[axis] = half(3u + axis);
        }

        // The node's first `count` dwords (a multiple of four). A node lies in one range, so one
        // probe of its span and direct loads read it; a node the table does not map as one aligned
        // range records an Unmapped fault and reads as zeros. APS5_BDA_BYTE_READS=1 (no probe)
        // reads it with the per-byte lookups.
        const auto nodeDwords = [&](std::uint32_t count) {
            std::vector<std::uint32_t> dwords;
            if (state.bdaProbeFunction == 0) {
                for (std::uint32_t first = 0; first < count; first += 4u) {
                    const auto read = EmitBdaDwordReads(ctx, inst, nodeAddress, first * 4u, 4u, true);
                    dwords.insert(dwords.end(), read.begin(), read.end());
                }
                return dwords;
            }
            const auto pc = uint(inst.Flags<MemoryFlags>().pc);
            const auto bytes = uint(count * 4u);
            const auto physical = state.module.AllocateId();
            state.module.AddFunction(spv::OpFunctionCall, u64, physical, state.bdaProbeFunction, nodeAddress, bytes, pc);
            const auto mapped = both(op(spv::OpINotEqual, boolean, physical, BdaConstant(state, 0u)), op(spv::OpIEqual, boolean, op(spv::OpBitwiseAnd, u64, physical, BdaConstant(state, 3u)), BdaConstant(state, 0u)));
            EmitIfCondition(state, Unary(state, spv::OpLogicalNot, boolean, mapped), [&] { RecordBdaFault(state, nodeAddress, bytes, pc, BdaAbi::FaultReason::Unmapped); });
            for (std::uint32_t first = 0; first < count; first += 4u) {
                const auto quad = EmitValueOrDefaultIfCondition(state, mapped, result4, ConstantU32CompositeZero(state, 4u), [&] {
                    std::array<std::uint32_t, 4> values{};
                    for (std::uint32_t i = 0; i < 4u; ++i) {
                        const auto pointer = state.module.AllocateId();
                        state.module.AddFunction(spv::OpConvertUToPtr, TypePhysicalU32Pointer(state), pointer, op(spv::OpIAdd, u64, physical, BdaConstant(state, (first + i) * 4u)));
                        values[i] = state.module.AllocateId();
                        state.module.AddFunction(spv::OpLoad, u32, values[i], pointer, spv::MemoryAccessAlignedMask, 4u);
                    }
                    return compose(values);
                });
                for (std::uint32_t i = 0; i < 4u; ++i) {
                    const auto value = state.module.AllocateId();
                    state.module.AddFunction(spv::OpCompositeExtract, u32, value, quad, i);
                    dwords.push_back(value);
                }
            }
            return dwords;
        };
        const auto sub = [&](const std::array<std::uint32_t, 3>& a, const std::array<std::uint32_t, 3>& b) {
            return std::array<std::uint32_t, 3>{op(spv::OpFSub, f32, a[0], b[0]), op(spv::OpFSub, f32, a[1], b[1]), op(spv::OpFSub, f32, a[2], b[2])};
        };
        const auto cross = [&](const std::array<std::uint32_t, 3>& a, const std::array<std::uint32_t, 3>& b) {
            const auto term = [&](std::uint32_t i, std::uint32_t j) { return op(spv::OpFSub, f32, op(spv::OpFMul, f32, a[i], b[j]), op(spv::OpFMul, f32, a[j], b[i])); };
            return std::array<std::uint32_t, 3>{term(1, 2), term(2, 0), term(0, 1)};
        };
        const auto dot = [&](const std::array<std::uint32_t, 3>& a, const std::array<std::uint32_t, 3>& b) {
            return op(spv::OpFAdd, f32, op(spv::OpFAdd, f32, op(spv::OpFMul, f32, a[0], b[0]), op(spv::OpFMul, f32, a[1], b[1])), op(spv::OpFMul, f32, a[2], b[2]));
        };

        // fast_intersect_triangle and SwizzleBarycentrics: type 0 tests (v0, v1, v2), type 1 tests
        // (v1, v3, v2). A miss returns t_num = +inf over t_denom = 1. The barycentric return mode
        // yields the i/j numerators the builder's rotation maps back; the other mode yields the
        // triangle id and a hit flag.
        const auto triangle = [&] {
            const auto d = nodeDwords(16u);
            const auto second = op(spv::OpIEqual, boolean, type, uint(1u));
            std::array<std::uint32_t, 3> v1{};
            std::array<std::uint32_t, 3> v2{};
            std::array<std::uint32_t, 3> v3{};
            for (std::uint32_t axis = 0; axis < 3u; ++axis) {
                v1[axis] = selectF(second, asFloat(d[3u + axis]), asFloat(d[axis]));
                v2[axis] = selectF(second, asFloat(d[9u + axis]), asFloat(d[3u + axis]));
                v3[axis] = asFloat(d[6u + axis]);
            }
            const auto e1 = sub(v2, v1);
            const auto e2 = sub(v3, v1);
            const auto e3 = sub(origin, v1);
            const auto s1 = cross(direction, e2);
            const auto s2 = cross(e3, e1);
            auto tNum = dot(e2, s2);
            auto tDenom = dot(s1, e1);
            const auto iNum = dot(e3, s1);
            const auto jNum = dot(direction, s2);
            const auto t = op(spv::OpFDiv, f32, tNum, tDenom);
            const auto u = op(spv::OpFDiv, f32, iNum, tDenom);
            const auto v = op(spv::OpFDiv, f32, jNum, tDenom);
            const auto zero = real(0u);
            const auto one = real(0x3f800000u);
            const auto missed = either(either(either(op(spv::OpFOrdLessThan, boolean, u, zero), op(spv::OpFOrdGreaterThan, boolean, u, one)), either(op(spv::OpFOrdLessThan, boolean, v, zero), op(spv::OpFOrdGreaterThan, boolean, op(spv::OpFAdd, f32, u, v), one))), op(spv::OpFOrdLessThan, boolean, t, zero));
            tNum = selectF(missed, real(0x7f800000u), tNum);
            tDenom = selectF(missed, one, tDenom);
            const auto triangleId = d[15];
            const auto shift = op(spv::OpShiftLeftLogical, u32, type, uint(3u));
            const auto barycentric = [&](std::uint32_t sourceShift) {
                const auto source = op(spv::OpBitwiseAnd, u32, op(spv::OpShiftRightLogical, u32, triangleId, op(spv::OpIAdd, u32, shift, uint(sourceShift))), uint(3u));
                const auto rest = op(spv::OpFSub, f32, op(spv::OpFSub, f32, tDenom, iNum), jNum);
                return selectF(op(spv::OpIEqual, boolean, source, uint(1u)), iNum, selectF(op(spv::OpIEqual, boolean, source, uint(2u)), jNum, rest));
            };
            const auto hit = Unary(state, spv::OpLogicalNot, boolean, missed);
            return compose({asUint(tNum), asUint(tDenom), selectU(barycentrics, asUint(barycentric(0u)), triangleId), selectU(barycentrics, asUint(barycentric(2u)), selectU(hit, uint(1u), uint(0u)))});
        };

        // IntersectNodeBvh4 with fast_intersect_bbox: a slab test clipped to [0, extent], a NaN
        // interval misses, box_grow_value widens the exit time by that many 2^-24 steps, and the
        // optional sort orders the hit children by entry time.
        const auto boxes = [&](bool fp16) {
            const auto d = nodeDwords(fp16 ? 16u : 28u);
            // Bound 0-2 is the minimum, 3-5 the maximum of one child box, from dword 4.
            const auto bound = [&](std::uint32_t child, std::uint32_t index) {
                const auto position = child * 6u + index;
                if (!fp16) {
                    return asFloat(d[4u + position]);
                }
                const auto packed = d[4u + position / 2u];
                return EmitF16BitsToF32(state, position % 2u == 0u ? op(spv::OpBitwiseAnd, u32, packed, uint(0xffffu)) : op(spv::OpShiftRightLogical, u32, packed, uint(16u)));
            };
            // HLSL max3/min3: a NaN operand propagates to the NaN check.
            const auto nanMax = [&](std::uint32_t a, std::uint32_t b) { return selectF(either(isNan(a), op(spv::OpFOrdGreaterThan, boolean, a, b)), a, b); };
            const auto nanMin = [&](std::uint32_t a, std::uint32_t b) { return selectF(either(isNan(a), op(spv::OpFOrdLessThan, boolean, a, b)), a, b); };
            const auto zero = real(0u);
            const auto growFactor = op(spv::OpFAdd, f32, real(0x3f800000u), op(spv::OpFMul, f32, Unary(state, spv::OpConvertUToF, f32, boxGrow), real(0x33800000u)));
            std::array<std::uint32_t, 4> children{};
            std::array<std::uint32_t, 4> keys{};
            for (std::uint32_t child = 0; child < 4u; ++child) {
                std::uint32_t enter = 0;
                std::uint32_t leave = 0;
                for (std::uint32_t axis = 0; axis < 3u; ++axis) {
                    const auto planeMin = op(spv::OpFMul, f32, op(spv::OpFSub, f32, bound(child, axis), origin[axis]), inverse[axis]);
                    const auto planeMax = op(spv::OpFMul, f32, op(spv::OpFSub, f32, bound(child, axis + 3u), origin[axis]), inverse[axis]);
                    const auto positive = op(spv::OpFOrdGreaterThanEqual, boolean, inverse[axis], zero);
                    const auto near = selectF(positive, planeMin, planeMax);
                    const auto far = selectF(positive, planeMax, planeMin);
                    enter = axis == 0u ? near : nanMax(enter, near);
                    leave = axis == 0u ? far : nanMin(leave, far);
                }
                const auto nan = either(isNan(enter), isNan(leave));
                const auto minT = selectF(nan, real(0x7f800000u), selectF(op(spv::OpFOrdGreaterThan, boolean, enter, zero), enter, zero));
                const auto maxT = selectF(nan, real(0xff800000u), selectF(op(spv::OpFOrdLessThan, boolean, leave, extent), leave, extent));
                const auto hit = op(spv::OpFOrdLessThanEqual, boolean, minT, op(spv::OpFMul, f32, maxT, growFactor));
                children[child] = selectU(hit, d[child], invalid);
                keys[child] = minT;
            }
            // The hardware sorting network; an invalid child always sinks.
            auto sorted = children;
            for (const auto [a, b] : std::array<std::pair<std::uint32_t, std::uint32_t>, 5>{{{0, 2}, {1, 3}, {0, 1}, {2, 3}, {1, 2}}}) {
                const auto bValid = op(spv::OpINotEqual, boolean, sorted[b], invalid);
                const auto aInvalid = op(spv::OpIEqual, boolean, sorted[a], invalid);
                const auto swap = either(both(bValid, op(spv::OpFOrdLessThan, boolean, keys[b], keys[a])), aInvalid);
                const auto sortedA = selectU(swap, sorted[b], sorted[a]);
                const auto sortedB = selectU(swap, sorted[a], sorted[b]);
                const auto keyA = selectF(swap, keys[b], keys[a]);
                const auto keyB = selectF(swap, keys[a], keys[b]);
                sorted[a] = sortedA;
                sorted[b] = sortedB;
                keys[a] = keyA;
                keys[b] = keyB;
            }
            for (std::uint32_t child = 0; child < 4u; ++child) {
                children[child] = selectU(boxSort, sorted[child], children[child]);
            }
            return compose(children);
        };

        const auto none = compose({invalid, invalid, invalid, invalid});
        const auto tested = EmitValueOrDefaultIfCondition(state, isTriangle, result4, none, triangle);
        const auto box16 = EmitValueOrDefaultIfCondition(state, isBox16, result4, tested, [&] { return boxes(true); });
        return EmitValueOrDefaultIfCondition(state, isBox32, result4, box16, [&] { return boxes(false); });
    };
    const auto inactive = compose({invalid, invalid, invalid, invalid});
    ctx.Define(inst, EmitValueOrDefaultIfCondition(state, ctx.Arg(inst, 2), result4, inactive, query));
}

}
