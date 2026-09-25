#include "SpirvBackend/SpirvImageEmitter.hpp"
#include "SpirvBackend/SpirvBufferFormat.hpp"
#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace ShaderRecompiler {
namespace {

struct ImageEmitAccess {
    const IrValue& inst;
    const MemoryInfo& mem;
    const ImageResource& image;
    const IrValue& address;
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

std::uint32_t ResultVector(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t value, IrTextureNumericClass numericClass, bool dref, bool gather) {
    auto& state = ctx.state;
    const auto& mem = access.mem;
    auto valueClass = numericClass;
    if (dref) {
        valueClass = IrTextureNumericClass::Float;
    }
    const bool integer = valueClass == IrTextureNumericClass::Uint || valueClass == IrTextureNumericClass::Sint;
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
    const auto image = LoadSampledImageDescriptor(state, access.mem.resource);
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
    const auto image = LoadSampledImageDescriptor(state, access.mem.resource);
    const auto width = state.module.AllocateId();
    state.module.AddFunction(spv::OpImageQuerySizeLod, TypeU32(state), width, image, ConstantU32(state, 0));
    const auto widthF32 = state.module.AllocateId();
    state.module.AddFunction(spv::OpConvertUToF, TypeF32(state), widthF32, width);
    const auto left = state.module.AllocateId();
    state.module.AddFunction(spv::OpExtInst, TypeF32(state), left, GlslStd450(state), GLSLstd450Floor, Binary(state, spv::OpFSub, TypeF32(state), Binary(state, spv::OpFMul, TypeF32(state), coord, widthF32), ConstantF32(state, 0x3f000000u)));
    const auto sampled = MakeSampledImage(state, access.mem.resource, access.mem.sampler);
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
    ctx.Define(access.inst, QueryDimensions(ctx, access));
}

void EmitQueryLodOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    auto& state = ctx.state;
    state.module.EmitCapability(spv::CapabilityImageQuery);
    const auto sampled = MakeSampledImage(state, access.mem.resource, access.mem.sampler);
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
    ctx.Define(access.inst, result);
}

void EmitReadOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    auto& state = ctx.state;
    const auto& dimensionInfo = RdnaImageDimensionInfoFor(access.image.dimension);
    const auto numericClass = access.image.numericClass;
    const auto condition = ctx.Arg(access.inst, 2);
    ctx.Define(access.inst, EmitValueOrDefaultIfCondition(state, condition, TypeU32Vector(state, 4), ConstantU32CompositeZero(state, 4), [&]() {
        const auto descriptor = LoadSampledImageDescriptor(state, access.mem.resource);
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
        return ResultVector(ctx, access, UnpackImageTexel(ctx, access, color), numericClass, false, false);
    }));
}

void EmitWriteOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    auto& state = ctx.state;
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
    if (dimension == RdnaImageDimension::Dim1D) {
        if (setup.dref || !HasFlag(mem, RdnaImageSampleFlagLevelZero) || HasFlag(mem, RdnaImageSampleFlagOffset) || HasFlag(mem, RdnaImageSampleFlagGatherHorizontal)) {
            ctx.Fail(access.inst, "has an unsupported 1D gather variant");
        }
        const auto sample = EmitOneDimensionalGatherLz(ctx, access, setup.coord);
        ctx.Define(access.inst, ResultVector(ctx, access, UnpackImageGather(ctx, access, sample), setup.numericClass, false, true));
        return;
    }
    if (dimension == RdnaImageDimension::Dim1DArray) {
        ctx.Fail(access.inst, "has an unsupported 1D-array gather");
    }
    const auto sampled = MakeSampledImage(state, mem.resource, mem.sampler);
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
    ctx.Define(access.inst, ResultVector(ctx, access, UnpackImageGather(ctx, access, gathered), resultNumericClass, false, true));
}

std::uint32_t EmitIndirectImageSelector(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t key) {
    auto& state = ctx.state;
    const auto& image = access.image;
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
        const auto less = Binary(state, spv::OpULessThan, TypeBool(state), mappedKey, key);
        const auto takeUpper = Binary(state, spv::OpLogicalAnd, TypeBool(state), active, less);
        const auto takeLower = Binary(state, spv::OpLogicalAnd, TypeBool(state), active, Unary(state, spv::OpLogicalNot, TypeBool(state), less));
        low = Select(state, TypeU32(state), takeUpper, Binary(state, spv::OpIAdd, TypeU32(state), mid, ConstantU32(state, 1u)), low);
        high = Select(state, TypeU32(state), takeLower, mid, high);
    }
    return selected;
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
    const auto emitSample = [&](std::uint32_t resource) {
        const auto sampled = MakeSampledImage(state, resource, mem.sampler);
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
        return sample;
    };
    if (image.indirectRoot != mem.resource) {
        const auto sample = emitSample(mem.resource);
        const auto result = setup.dref ? sample : UnpackImageTexel(ctx, access, sample);
        ctx.Define(access.inst, ResultVector(ctx, access, result, setup.numericClass, setup.dref, false));
        return;
    }
    const auto* handle = access.inst.Argument(0);
    const auto& sources = state.program.Resources().descriptorSources;
    if (handle == nullptr || image.source >= sources.size() || !sources[image.source].indirectImage.has_value() || sources[image.source].indirectImage->keyArg >= handle->ArgumentCount()) {
        ctx.Fail(access.inst, "has invalid indirect image key provenance");
    }
    if (state.flattenedSrtVariable == 0 || image.indirectSearchIterations == 0u || image.indirectResources.size() < 2u) {
        ctx.Fail(access.inst, "has no indirect image runtime mapping");
    }
    const auto key = ctx.Def(handle->Argument(sources[image.source].indirectImage->keyArg));
    const auto selected = EmitIndirectImageSelector(ctx, access, key);
    const auto defaultLabel = state.module.AllocateId();
    const auto mergeLabel = state.module.AllocateId();
    std::vector<std::uint32_t> labels(image.indirectResources.size() - 1u);
    std::vector<std::uint32_t> switchWords = {spv::OpSwitch, selected, defaultLabel};
    for (std::uint32_t candidate = 1; candidate < image.indirectResources.size(); candidate++) {
        labels[candidate - 1u] = state.module.AllocateId();
        switchWords.push_back(candidate);
        switchWords.push_back(labels[candidate - 1u]);
    }
    state.module.AddFunction(spv::OpSelectionMerge, mergeLabel, spv::SelectionControlMaskNone);
    state.module.AddFunction(switchWords);
    std::vector<std::uint32_t> phiWords = {spv::OpPhi, resultType, state.module.AllocateId()};
    EmitLabel(state, defaultLabel);
    phiWords.push_back(emitSample(image.indirectResources[0]));
    phiWords.push_back(defaultLabel);
    state.module.AddFunction(spv::OpBranch, mergeLabel);
    for (std::uint32_t candidate = 1; candidate < image.indirectResources.size(); candidate++) {
        EmitLabel(state, labels[candidate - 1u]);
        phiWords.push_back(emitSample(image.indirectResources[candidate]));
        phiWords.push_back(labels[candidate - 1u]);
        state.module.AddFunction(spv::OpBranch, mergeLabel);
    }
    EmitLabel(state, mergeLabel);
    state.module.AddFunction(phiWords);
    auto result = phiWords[2];
    if (!setup.dref) {
        result = UnpackImageTexel(ctx, access, result);
    }
    ctx.Define(access.inst, ResultVector(ctx, access, result, setup.numericClass, setup.dref, false));
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
    const ImageEmitAccess access{inst, mem, image, *address};
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

}
