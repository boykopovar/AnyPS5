#include "SpirvBackend/SpirvImageEmitter.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "PipelineSpecialization.hpp"
#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvBufferFormat.hpp"
#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include <span>
#include <initializer_list>
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

std::uint32_t RuntimeImageDword(SpirvEmitterState& state, std::uint32_t resource, std::uint32_t member) {
    const auto index = state.runtimeImageMetadata != 0u ? state.runtimeImageMetadata : ConstantU32(state, resource);
    const auto dword = Binary(state, spv::OpIAdd, TypeU32(state), ConstantU32(state, state.program.Metadata().bindings.ImageMetadataDword() + member), Binary(state, spv::OpIMul, TypeU32(state), index, ConstantU32(state, sizeof(RuntimeAbi::ResourceMetadata) / sizeof(std::uint32_t))));
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypeStorageBufferElementPointer(state), pointer, state.shaderDataStorageVariable, ConstantU32(state, 0u), dword);
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
    return value;
}

std::uint32_t RuntimeImageSwizzle(SpirvEmitterState& state, std::uint32_t resource, std::uint32_t component) {
    if (state.program.Info().images.at(resource).indirectRoot == ImageResource::NoIndirectImage) return state.module.SpecializationConstant(TypeU32(state), PipelineSpecialization::ImageBase + resource * PipelineSpecialization::ImageWords + 1u + component, component + 4u);
    const auto descriptor = RuntimeImageDword(state, resource, offsetof(RuntimeAbi::ResourceMetadata, descriptor) / sizeof(std::uint32_t) + 3u);
    return Binary(state, spv::OpBitwiseAnd, TypeU32(state), Binary(state, spv::OpShiftRightLogical, TypeU32(state), descriptor, ConstantU32(state, component * 3u)), ConstantU32(state, 7u));
}

std::uint32_t RuntimeSwizzledComponent(SpirvEmitterState& state, std::uint32_t selector, const std::array<std::uint32_t, 4>& components) {
    auto result = Select(state, TypeU32(state), Binary(state, spv::OpIEqual, TypeBool(state), selector, ConstantU32(state, 1u)), ConstantU32(state, 1u), ConstantU32(state, 0u));
    for (std::uint32_t index = 0u; index < components.size(); ++index) result = Select(state, TypeU32(state), Binary(state, spv::OpIEqual, TypeBool(state), selector, ConstantU32(state, index + 4u)), components[index], result);
    return result;
}

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

const RdnaImageDimensionInfo& AddressDimension(const ImageEmitAccess& access) {
    return RdnaImageDimensionInfoFor(access.mem.imageDimension);
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

ImageSampleLayout Layout(const MemoryInfo& mem) {
    ImageSampleLayout layout;
    std::uint32_t cursor = 0;
    const auto& info = RdnaImageDimensionInfoFor(mem.imageDimension);
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

std::uint32_t CoordF32(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t first, std::uint32_t components, std::uint32_t encoded) {
    if (first == NoImageComponent || encoded < components || access.mem.imageAddressComponents < first + encoded) {
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
    const auto encoded = std::min(components, AddressDimension(access).coordinateComponents);
    if (access.mem.imageAddressComponents < encoded) {
        ctx.Fail(access.inst, "has an image address with too few coordinate components");
    }
    const auto component = [&](std::uint32_t index) { return index < encoded ? AddressU32(ctx, access, index) : ConstantU32(ctx.state, 0); };
    const auto x = component(0);
    if (components == 1u) {
        return x;
    }
    const auto y = component(1);
    const auto result = ctx.state.module.AllocateId();
    if (components == 3u) {
        const auto z = component(2);
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
    const auto component = AddressDimension(access).coordinateComponents;
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
    const auto mapped = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeBoolVector(state, 4), mapped, access.table.mapped, access.table.mapped, access.table.mapped, access.table.mapped);
    return Select(state, TypeU32Vector(state, 4), mapped, result, ConstantU32CompositeZero(state, 4));
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
                packed[word] = EmitPackHalf2x16(state, pair);
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
        if (access.image.depthUnorm16) {
            const auto scaled = Binary(state, spv::OpFMul, TypeF32(state), scalar, ConstantF32Value(state, 65535.0f));
            component[index] = Unary(state, spv::OpConvertFToU, TypeU32(state), Binary(state, spv::OpFAdd, TypeF32(state), scaled, ConstantF32Value(state, 0.5f)));
        } else {
            component[index] = SampledComponentBits(ctx, scalar, valueClass);
        }
        if (access.image.depthBits && !gather) {
            const auto selector = RuntimeImageSwizzle(state, mem.resource, index);
            const auto one = Binary(state, spv::OpLogicalOr, TypeBool(state), Binary(state, spv::OpIEqual, TypeBool(state), selector, ConstantU32(state, 1u)), Binary(state, spv::OpIEqual, TypeBool(state), selector, ConstantU32(state, 7u)));
            component[index] = Select(state, TypeU32(state), Binary(state, spv::OpIEqual, TypeBool(state), selector, ConstantU32(state, 4u)), component[index], Select(state, TypeU32(state), one, ConstantU32(state, 1u), ConstantU32(state, 0u)));
        }
    }
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4), result, component[0], component[1], component[2], component[3]);
    return result;
}

constexpr std::array<std::uint32_t, 256> SrgbToLinear{
    0x00000000u, 0x399f0000u, 0x3a1f0000u, 0x3a6f0000u, 0x3a9f0000u, 0x3ac70000u, 0x3aef0000u, 0x3b0b0000u,
    0x3b1f0000u, 0x3b330000u, 0x3b470000u, 0x3b5b0000u, 0x3b710000u, 0x3b840000u, 0x3b900000u, 0x3b9d0000u,
    0x3baa0000u, 0x3bb80000u, 0x3bc60000u, 0x3bd50000u, 0x3be50000u, 0x3bf60000u, 0x3c030000u, 0x3c0c0000u,
    0x3c160000u, 0x3c1f0000u, 0x3c290000u, 0x3c340000u, 0x3c3e0000u, 0x3c490000u, 0x3c550000u, 0x3c600000u,
    0x3c6d0000u, 0x3c790000u, 0x3c830000u, 0x3c8a0000u, 0x3c910000u, 0x3c980000u, 0x3c9f0000u, 0x3ca60000u,
    0x3cae0000u, 0x3cb60000u, 0x3cbe0000u, 0x3cc60000u, 0x3cce0000u, 0x3cd70000u, 0x3ce00000u, 0x3ce90000u,
    0x3cf20000u, 0x3cfc0000u, 0x3d030000u, 0x3d080000u, 0x3d0d0000u, 0x3d120000u, 0x3d170000u, 0x3d1c0000u,
    0x3d220000u, 0x3d280000u, 0x3d2d0000u, 0x3d330000u, 0x3d390000u, 0x3d3f0000u, 0x3d450000u, 0x3d4c0000u,
    0x3d520000u, 0x3d590000u, 0x3d5f0000u, 0x3d660000u, 0x3d6d0000u, 0x3d740000u, 0x3d7b0000u, 0x3d810000u,
    0x3d850000u, 0x3d880000u, 0x3d8c0000u, 0x3d900000u, 0x3d940000u, 0x3d980000u, 0x3d9c0000u, 0x3da00000u,
    0x3da40000u, 0x3da90000u, 0x3dad0000u, 0x3db10000u, 0x3db60000u, 0x3dba0000u, 0x3dbf0000u, 0x3dc30000u,
    0x3dc80000u, 0x3dcd0000u, 0x3dd10000u, 0x3dd60000u, 0x3ddb0000u, 0x3de00000u, 0x3de50000u, 0x3dea0000u,
    0x3df00000u, 0x3df50000u, 0x3dfa0000u, 0x3e000000u, 0x3e020000u, 0x3e050000u, 0x3e080000u, 0x3e0b0000u,
    0x3e0e0000u, 0x3e110000u, 0x3e140000u, 0x3e170000u, 0x3e1a0000u, 0x3e1d0000u, 0x3e200000u, 0x3e230000u,
    0x3e260000u, 0x3e290000u, 0x3e2c0000u, 0x3e300000u, 0x3e330000u, 0x3e360000u, 0x3e3a0000u, 0x3e3d0000u,
    0x3e400000u, 0x3e440000u, 0x3e470000u, 0x3e4b0000u, 0x3e4e0000u, 0x3e520000u, 0x3e560000u, 0x3e590000u,
    0x3e5d0000u, 0x3e610000u, 0x3e650000u, 0x3e680000u, 0x3e6c0000u, 0x3e700000u, 0x3e740000u, 0x3e780000u,
    0x3e7c0000u, 0x3e800000u, 0x3e820000u, 0x3e840000u, 0x3e860000u, 0x3e880000u, 0x3e8a0000u, 0x3e8d0000u,
    0x3e8f0000u, 0x3e910000u, 0x3e930000u, 0x3e950000u, 0x3e980000u, 0x3e9a0000u, 0x3e9c0000u, 0x3e9e0000u,
    0x3ea10000u, 0x3ea30000u, 0x3ea50000u, 0x3ea80000u, 0x3eaa0000u, 0x3ead0000u, 0x3eaf0000u, 0x3eb20000u,
    0x3eb40000u, 0x3eb60000u, 0x3eb90000u, 0x3ebc0000u, 0x3ebe0000u, 0x3ec10000u, 0x3ec30000u, 0x3ec60000u,
    0x3ec80000u, 0x3ecb0000u, 0x3ece0000u, 0x3ed10000u, 0x3ed30000u, 0x3ed60000u, 0x3ed90000u, 0x3edb0000u,
    0x3ede0000u, 0x3ee10000u, 0x3ee40000u, 0x3ee70000u, 0x3eea0000u, 0x3eed0000u, 0x3ef00000u, 0x3ef20000u,
    0x3ef50000u, 0x3ef80000u, 0x3efb0000u, 0x3efe0000u, 0x3f010000u, 0x3f020000u, 0x3f040000u, 0x3f050000u,
    0x3f070000u, 0x3f090000u, 0x3f0a0000u, 0x3f0c0000u, 0x3f0d0000u, 0x3f0f0000u, 0x3f110000u, 0x3f120000u,
    0x3f140000u, 0x3f160000u, 0x3f170000u, 0x3f190000u, 0x3f1b0000u, 0x3f1c0000u, 0x3f1e0000u, 0x3f200000u,
    0x3f210000u, 0x3f230000u, 0x3f250000u, 0x3f270000u, 0x3f290000u, 0x3f2a0000u, 0x3f2c0000u, 0x3f2e0000u,
    0x3f300000u, 0x3f320000u, 0x3f330000u, 0x3f350000u, 0x3f370000u, 0x3f390000u, 0x3f3b0000u, 0x3f3d0000u,
    0x3f3f0000u, 0x3f410000u, 0x3f430000u, 0x3f450000u, 0x3f470000u, 0x3f490000u, 0x3f4b0000u, 0x3f4d0000u,
    0x3f4f0000u, 0x3f510000u, 0x3f530000u, 0x3f550000u, 0x3f570000u, 0x3f590000u, 0x3f5b0000u, 0x3f5d0000u,
    0x3f5f0000u, 0x3f610000u, 0x3f630000u, 0x3f650000u, 0x3f680000u, 0x3f6a0000u, 0x3f6c0000u, 0x3f6e0000u,
    0x3f700000u, 0x3f730000u, 0x3f750000u, 0x3f770000u, 0x3f790000u, 0x3f7b0000u, 0x3f7e0000u, 0x3f800000u,
};

std::uint32_t DecodeSrgbTexel(SpirvEmitterState& state, std::uint32_t color) {
    const auto pointerType = TypePointer(state, spv::StorageClassPrivate, TypeU32(state));
    std::array<std::uint32_t, SrgbToLinear.size()> words{};
    for (std::size_t index = 0; index < words.size(); ++index) words[index] = ConstantU32(state, SrgbToLinear[index]);
    const auto arrayType = state.module.Type(spv::OpTypeArray, TypeU32(state), ConstantU32(state, static_cast<std::uint32_t>(words.size())));
    if (state.srgbTableVariable == 0) state.srgbTableVariable = state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassPrivate, arrayType), spv::StorageClassPrivate);
    state.module.AddFunction(spv::OpStore, state.srgbTableVariable, state.module.Constant(spv::OpConstantComposite, arrayType, std::span<const std::uint32_t>(words)));
    std::uint32_t components[4] = {};
    for (std::uint32_t index = 0; index < 4u; index++) {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), value, color, index);
        const auto scaled = Binary(state, spv::OpFAdd, TypeF32(state), Binary(state, spv::OpFMul, TypeF32(state), value, ConstantF32Value(state, 255.0f)), ConstantF32Value(state, 0.5f));
        const auto code = state.module.AllocateId();
        state.module.AddFunction(spv::OpExtInst, TypeU32(state), code, GlslStd450(state), GLSLstd450UMin, Unary(state, spv::OpConvertFToU, TypeU32(state), scaled), ConstantU32(state, 255u));
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, pointerType, pointer, state.srgbTableVariable, code);
        const auto bits = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), bits, pointer);
        components[index] = Unary(state, spv::OpBitcast, TypeF32(state), bits);
    }
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeF32Vector(state, 4), result, components[0], components[1], components[2], components[3]);
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

SpirvBufferFormatInfo ImageConversionFormat(const ImageResource& image) {
    const auto format = image.conversionFormat;
    if (format == IrBufferFormat::Invalid) {
        return {};
    }
    const auto info = GetFormatInfo(format);
    if (SampledTextureNumericClass(format) != IrTextureNumericClass::Uint || RemapTextureFormat(format) == format || (info.type != SpirvFormatComponentType::Uint && info.type != SpirvFormatComponentType::Unorm && info.type != SpirvFormatComponentType::Float) || !info.packedBitfield || info.byteSize != sizeof(std::uint32_t) || info.componentCount == 0u || info.componentCount > 4u) {
        throw std::runtime_error("image conversion format is not a packed 32-bit unsigned integer, unorm or float format");
    }
    return info;
}

void RequireConvertedUnormAccess(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, const SpirvBufferFormatInfo& info) {
    if (access.mem.dataBits == 16u) {
        ctx.Fail(access.inst, info.type == SpirvFormatComponentType::Float ? "reads or writes a converted float image with 16-bit data, which is not implemented" : "reads or writes a converted unorm image with 16-bit data, which is not implemented");
    }
    for (std::uint32_t component = 0; component < 4u; component++) {
        const auto selector = (access.image.shaderSwizzle >> (component * 3u)) & 7u;
        if (selector >= 4u && selector - 4u >= info.componentCount) {
            ctx.Fail(access.inst, "selects a channel the converted image format does not have");
        }
    }
}

std::uint32_t UnpackImageTexel(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t texel) {
    auto& state = ctx.state;
    if (access.image.srgbDecode) {
        return DecodeSrgbTexel(state, texel);
    }
    const auto info = ImageConversionFormat(access.image);
    if (info.format == IrBufferFormat::Invalid) {
        return texel;
    }
    const bool unorm = info.type == SpirvFormatComponentType::Unorm || info.type == SpirvFormatComponentType::Float;
    if (unorm) {
        RequireConvertedUnormAccess(ctx, access, info);
    }
    const auto packed = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), packed, texel, 0u);
    std::array<std::uint32_t, 4> components{ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0)};
    for (std::uint32_t component = 0; component < info.componentCount; component++) {
        components[component] = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitFieldUExtract, TypeU32(state), components[component], packed, ConstantU32(state, info.componentBitOffset[component]), ConstantU32(state, info.componentBits[component]));
        if (unorm) {
            components[component] = NormalizeFormatComponent(state, info, component, components[component]);
        }
    }
    for (std::uint32_t component = info.componentCount; component < 4u; component++) {
        components[component] = components[component % info.componentCount];
    }
    std::uint32_t selected[4] = {};
    for (std::uint32_t component = 0; component < 4u; component++) {
        const auto selector = RuntimeImageSwizzle(state, access.mem.resource, component);
        const auto value = RuntimeSwizzledComponent(state, selector, components);
        selected[component] = Select(state, TypeU32(state), Binary(state, spv::OpIEqual, TypeBool(state), selector, ConstantU32(state, 1u)), ConstantU32(state, FormattedConstantBits(info, SpirvFormattedSourceKind::One)), value);
    }
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4), result, selected[0], selected[1], selected[2], selected[3]);
    return result;
}

SpirvBufferFormatInfo PackedTexelFormat(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    const auto format = access.image.packedFormat;
    if (format == IrBufferFormat::Invalid || access.table.mapped != 0 || access.image.depthBits || access.image.conversionFormat != IrBufferFormat::Invalid) {
        ctx.Fail(access.inst, "accesses packed texels of an image without a packed format");
    }
    const auto info = GetFormatInfo(format);
    if (info.packedBitfield) {
        ctx.Fail(access.inst, "accesses packed texels of a bitfield format");
    }
    return info;
}

std::uint32_t PackedImageTexel(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t texel) {
    auto& state = ctx.state;
    const auto info = PackedTexelFormat(ctx, access);
    std::uint32_t texelBits = 0;
    for (std::uint32_t component = 0; component < info.componentCount; component++) {
        const auto bits = info.componentBits[component];
        const bool exact = info.type == SpirvFormatComponentType::Uint || info.type == SpirvFormatComponentType::Sint || (info.type == SpirvFormatComponentType::Unorm && bits <= 16u) || (info.type == SpirvFormatComponentType::Float && bits == 32u);
        if (!exact) {
            ctx.Fail(access.inst, "reads packed texels of a format whose bits are not recoverable from the view");
        }
        texelBits += bits;
    }
    const auto numericClass = access.image.numericClass;
    std::uint32_t words[4] = {ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0)};
    std::uint32_t position = 0;
    for (std::uint32_t component = 0; component < info.componentCount; component++) {
        const auto bits = info.componentBits[component];
        const auto scalar = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, ImageScalarType(state, numericClass), scalar, texel, component);
        std::uint32_t value;
        if (info.type == SpirvFormatComponentType::Unorm) {
            const auto scaled = Binary(state, spv::OpFMul, TypeF32(state), scalar, ConstantF32Value(state, static_cast<float>((1u << bits) - 1u)));
            value = Unary(state, spv::OpConvertFToU, TypeU32(state), Binary(state, spv::OpFAdd, TypeF32(state), scaled, ConstantF32Value(state, 0.5f)));
        } else {
            value = SampledComponentBits(ctx, scalar, numericClass);
        }
        if (bits < 32u) {
            value = Binary(state, spv::OpBitwiseAnd, TypeU32(state), value, ConstantU32(state, (1u << bits) - 1u));
        }
        const auto word = position / 32u;
        if (position % 32u != 0u) {
            value = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), value, ConstantU32(state, position % 32u));
        }
        words[word] = Binary(state, spv::OpBitwiseOr, TypeU32(state), words[word], value);
        position += bits;
    }
    if (access.mem.dataSigned && texelBits < 32u) {
        const auto shift = ConstantU32(state, 32u - texelBits);
        words[0] = Binary(state, spv::OpShiftRightArithmetic, TypeU32(state), Binary(state, spv::OpShiftLeftLogical, TypeU32(state), words[0], shift), shift);
    }
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4), result, words[0], words[1], words[2], words[3]);
    return result;
}

std::uint32_t UnpackImageGather(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t gathered) {
    auto& state = ctx.state;
    const auto info = ImageConversionFormat(access.image);
    if (info.format == IrBufferFormat::Invalid) {
        return gathered;
    }
    const auto component = ImageGatherComponent(EffectiveDmask(access.mem));
    const auto selector = RuntimeImageSwizzle(state, access.mem.resource, component);
    std::uint32_t values[4] = {};
    for (std::uint32_t lane = 0; lane < 4u; lane++) {
        const auto packed = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), packed, gathered, lane);
        std::array<std::uint32_t, 4> components{};
        for (std::uint32_t index = 0u; index < components.size(); ++index) {
            const auto physical = index % info.componentCount;
            components[index] = state.module.AllocateId();
            state.module.AddFunction(spv::OpBitFieldUExtract, TypeU32(state), components[index], packed, ConstantU32(state, info.componentBitOffset[physical]), ConstantU32(state, info.componentBits[physical]));
        }
        values[lane] = RuntimeSwizzledComponent(state, selector, components);
    }
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4), result, values[0], values[1], values[2], values[3]);
    return result;
}

std::uint32_t EmitOneDimensionalGatherLz(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, const SampleSetup& setup) {
    auto& state = ctx.state;
    const auto numericClass = access.image.numericClass;
    const bool arrayed = access.image.dimension == RdnaImageDimension::Dim1DArray;
    state.module.EmitCapability(spv::CapabilityImageQuery);
    const auto image = LoadSampledImageDescriptor(state, access.mem.resource, access.slot);
    auto width = state.module.AllocateId();
    state.module.AddFunction(spv::OpImageQuerySizeLod, arrayed ? TypeU32Vector(state, 2) : TypeU32(state), width, image, ConstantU32(state, 0));
    auto coord = setup.coord;
    std::uint32_t layer = 0;
    if (arrayed) {
        const auto size = width;
        width = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), width, size, 0u);
        coord = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), coord, setup.coord, 0u);
        layer = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), layer, setup.coord, 1u);
    }
    const auto widthF32 = state.module.AllocateId();
    state.module.AddFunction(spv::OpConvertUToF, TypeF32(state), widthF32, width);
    auto left = state.module.AllocateId();
    state.module.AddFunction(spv::OpExtInst, TypeF32(state), left, GlslStd450(state), GLSLstd450Floor, Binary(state, spv::OpFSub, TypeF32(state), Binary(state, spv::OpFMul, TypeF32(state), coord, widthF32), ConstantF32(state, 0x3f000000u)));
    if (setup.layout.offset != NoImageComponent) {
        left = Binary(state, spv::OpFAdd, TypeF32(state), left, Unary(state, spv::OpConvertSToF, TypeF32(state), PackedOffset(ctx, access, setup.layout)));
    }
    const auto sampled = MakeSampledImage(state, access.mem.resource, access.mem.sampler, access.slot);
    const auto vectorType = ImageVectorType(state, numericClass, 4);
    const auto scalarType = ImageScalarType(state, numericClass);
    const auto component = ImageConversionFormat(access.image).format == IrBufferFormat::Invalid ? ImageGatherComponent(EffectiveDmask(access.mem)) : 0u;
    std::uint32_t values[2] = {};
    for (std::uint32_t index = 0; index < 2u; index++) {
        auto sampleCoord = Binary(state, spv::OpFDiv, TypeF32(state), Binary(state, spv::OpFAdd, TypeF32(state), left, ConstantF32(state, index == 0u ? 0x3f000000u : 0x3fc00000u)), widthF32);
        if (arrayed) {
            const auto pair = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeConstruct, TypeF32Vector(state, 2), pair, sampleCoord, layer);
            sampleCoord = pair;
        }
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
    const bool unorm = info.type == SpirvFormatComponentType::Unorm || info.type == SpirvFormatComponentType::Float;
    if (unorm) {
        RequireConvertedUnormAccess(ctx, access, info);
    }
    auto packed = ConstantU32(state, 0u);
    for (std::uint32_t component = 0; component < info.componentCount; component++) {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), value, texel, component);
        auto clamped = value;
        if (unorm) {
            clamped = EmitFormatStoreComponent(state, info, component, value);
        } else {
            const auto maximum = ConstantU32(state, info.componentBits[component] == 32u ? UINT32_MAX : (1u << info.componentBits[component]) - 1u);
            const auto within = Binary(state, spv::OpULessThan, TypeBool(state), value, maximum);
            clamped = Select(state, TypeU32(state), within, value, maximum);
        }
        const auto shifted = info.componentBitOffset[component] == 0u ? clamped : Binary(state, spv::OpShiftLeftLogical, TypeU32(state), clamped, ConstantU32(state, info.componentBitOffset[component]));
        packed = Binary(state, spv::OpBitwiseOr, TypeU32(state), packed, shifted);
    }
    const auto zero = ConstantU32(state, 0u);
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4), result, packed, zero, zero, zero);
    return result;
}

SpirvBufferFormatInfo SintStorageFormat(const ImageResource& image) {
    const auto format = image.conversionFormat;
    if (image.resourceClass != ImageResourceClass::Storage || format == IrBufferFormat::Invalid || SampledTextureNumericClass(format) != IrTextureNumericClass::Sint) {
        return {};
    }
    const auto info = GetFormatInfo(format);
    if (image.numericClass != IrTextureNumericClass::Uint || info.type != SpirvFormatComponentType::Sint || info.packedBitfield || info.componentCount == 0u || info.byteSize == 12u) {
        throw std::runtime_error("storage image conversion format is not a SINT format written through a UINT view");
    }
    return info;
}

std::uint32_t StoreTexel(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t data, bool integer) {
    auto& state = ctx.state;
    const auto& mem = access.mem;
    const auto sint = SintStorageFormat(access.image);
    if (sint.format != IrBufferFormat::Invalid && mem.dataBits == 16u) {
        ctx.Fail(access.inst, "stores 16-bit data to an image of a SINT format");
    }
    std::uint32_t values[4] = {};
    const auto dmask = EffectiveDmask(mem);
    for (std::uint32_t component = 0; component < 4u; component++) {
        auto raw = ConstantU32(state, 0);
        for (std::uint32_t reverse = 4u; reverse != 0u; --reverse) {
            const auto source = reverse - 1u;
            const auto packedIndex = DmaskComponentIndex(dmask, source);
            auto candidate = ConstantU32(state, 0u);
            if (((dmask >> source) & 1u) != 0u) {
                candidate = state.module.AllocateId();
                state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), candidate, data, mem.dataBits == 16u ? packedIndex / 2u : packedIndex);
            }
            if (mem.dataBits == 16u && ((dmask >> source) & 1u) != 0u) {
                if ((packedIndex & 1u) != 0u) {
                    candidate = Binary(state, spv::OpShiftRightLogical, TypeU32(state), candidate, ConstantU32(state, 16u));
                }
                candidate = Binary(state, spv::OpBitwiseAnd, TypeU32(state), candidate, ConstantU32(state, 0xffffu));
            }
            raw = Select(state, TypeU32(state), Binary(state, spv::OpIEqual, TypeBool(state), RuntimeImageSwizzle(state, mem.resource, source), ConstantU32(state, component + 4u)), candidate, raw);
        }
        values[component] = integer ? raw : mem.dataBits == 16u ? EmitF16BitsToF32(state, raw) : Unary(state, spv::OpBitcast, TypeF32(state), raw);
        if (component < sint.componentCount) {
            values[component] = EmitFormatStoreComponent(state, sint, component, values[component]);
        }
    }
    const auto texel = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, integer ? TypeU32Vector(state, 4) : TypeF32Vector(state, 4), texel, values[0], values[1], values[2], values[3]);
    return sint.format != IrBufferFormat::Invalid ? texel : PackImageTexel(ctx, access, texel);
}

std::uint32_t PackedStoreTexel(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, std::uint32_t data) {
    auto& state = ctx.state;
    const auto info = PackedTexelFormat(ctx, access);
    const auto numericClass = access.image.numericClass;
    if (info.byteSize == 12u) {
        ctx.Fail(access.inst, "stores packed texels of a format the hardware does not write");
    }
    std::uint32_t words[4] = {};
    for (std::uint32_t word = 0; word < 4u; word++) {
        if (((access.mem.dmask >> word) & 1u) == 0u) {
            words[word] = ConstantU32(state, 0u);
            continue;
        }
        words[word] = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), words[word], data, DmaskComponentIndex(access.mem.dmask, word));
    }
    std::uint32_t values[4] = {};
    for (std::uint32_t component = 0; component < 4u; component++) {
        if (component >= info.componentCount) {
            values[component] = numericClass == IrTextureNumericClass::Float ? ZeroF32(state) : ConstantU32(state, 0u);
            continue;
        }
        const auto bits = info.componentBits[component];
        const auto offset = info.componentBitOffset[component];
        const bool exact = (info.type == SpirvFormatComponentType::Uint && numericClass == IrTextureNumericClass::Uint) || (bits == 32u && ((info.type == SpirvFormatComponentType::Sint && numericClass == IrTextureNumericClass::Uint) || (info.type == SpirvFormatComponentType::Float && numericClass == IrTextureNumericClass::Float)));
        if (!exact || offset % 32u + bits > 32u) {
            ctx.Fail(access.inst, "stores packed texels of a format whose bits are not reproducible through the view");
        }
        auto value = words[offset / 32u];
        if (offset % 32u != 0u) {
            value = Binary(state, spv::OpShiftRightLogical, TypeU32(state), value, ConstantU32(state, offset % 32u));
        }
        if (bits < 32u) {
            value = Binary(state, spv::OpBitwiseAnd, TypeU32(state), value, ConstantU32(state, (1u << bits) - 1u));
        }
        values[component] = numericClass == IrTextureNumericClass::Float ? Unary(state, spv::OpBitcast, TypeF32(state), value) : value;
    }
    const auto texel = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, numericClass == IrTextureNumericClass::Float ? TypeF32Vector(state, 4) : TypeU32Vector(state, 4), texel, values[0], values[1], values[2], values[3]);
    return texel;
}

std::uint32_t ImageAtomicOpcode(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::ImageAtomicSwap32:
        case IrOpcode::ImageAtomicSwap64:
            return spv::OpAtomicExchange;
        case IrOpcode::ImageAtomicIAdd32:
        case IrOpcode::ImageAtomicIAdd64:
            return spv::OpAtomicIAdd;
        case IrOpcode::ImageAtomicUMin32:
        case IrOpcode::ImageAtomicUMin64:
            return spv::OpAtomicUMin;
        case IrOpcode::ImageAtomicUMax32:
        case IrOpcode::ImageAtomicUMax64:
            return spv::OpAtomicUMax;
        case IrOpcode::ImageAtomicAnd32:
        case IrOpcode::ImageAtomicAnd64:
            return spv::OpAtomicAnd;
        case IrOpcode::ImageAtomicOr32:
        case IrOpcode::ImageAtomicOr64:
            return spv::OpAtomicOr;
        case IrOpcode::ImageAtomicXor32:
        case IrOpcode::ImageAtomicXor64:
            return spv::OpAtomicXor;
        case IrOpcode::ImageAtomicISub32:
        case IrOpcode::ImageAtomicISub64:
            return spv::OpAtomicISub;
        case IrOpcode::ImageAtomicSMin32:
        case IrOpcode::ImageAtomicSMin64:
            return spv::OpAtomicSMin;
        case IrOpcode::ImageAtomicSMax32:
        case IrOpcode::ImageAtomicSMax64:
            return spv::OpAtomicSMax;
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
    if (ImageConversionFormat(access.image).type == SpirvFormatComponentType::Unorm) {
        ctx.Fail(access.inst, "queries the level of detail of a converted unorm image, which is not implemented");
    }
    if (ImageConversionFormat(access.image).type == SpirvFormatComponentType::Float) {
        ctx.Fail(access.inst, "queries the level of detail of a converted float image, which is not implemented");
    }
    state.module.EmitCapability(spv::CapabilityImageQuery);
    const auto sampled = MakeSampledImage(state, access.mem.resource, access.mem.sampler, access.slot);
    const auto coord = CoordF32(ctx, access, 0, RdnaImageDimensionInfoFor(access.image.dimension).spatialComponents, AddressDimension(access).spatialComponents);
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

void EmitByReadOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    auto& state = ctx.state;
    const auto elements = access.mem.imageByElements;
    const bool packed = access.mem.imagePacked;
    const auto components = packed ? 1u : access.mem.dataDwords / elements;
    if (access.image.dimension != RdnaImageDimension::Dim2D || components == 0u || (packed ? access.mem.dataDwords != 1u : components * elements != access.mem.dataDwords) || access.mem.dataBits != 32u) {
        ctx.Fail(access.inst, "is a MIMG BY2/BY4 load outside the measured 2D, 32-bit data subset");
    }
    state.module.EmitCapability(spv::CapabilityImageQuery);
    const auto condition = ctx.Arg(access.inst, 2);
    ctx.Define(access.inst, EmitValueOrDefaultIfCondition(state, condition, TypeU32Vector(state, 4), ConstantU32CompositeZero(state, 4), [&]() {
        const auto image = LoadSampledImageDescriptor(state, access.mem.resource, access.slot);
        const auto x = AddressU32(ctx, access, 0);
        const auto y = AddressU32(ctx, access, 1);
        const auto lod = LodU32(ctx, access);
        const auto levels = state.module.AllocateId();
        state.module.AddFunction(spv::OpImageQueryLevels, TypeU32(state), levels, image);
        const auto lodInside = Binary(state, spv::OpULessThan, TypeBool(state), lod, levels);
        const auto queryLod = Select(state, TypeU32(state), lodInside, lod, ConstantU32(state, 0));
        const auto size = state.module.AllocateId();
        state.module.AddFunction(spv::OpImageQuerySizeLod, ImageViewSizeType(state, access.image.dimension), size, image, ConstantU32(state, 0));
        const auto levelExtent = [&](std::uint32_t axis) {
            const auto base = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), base, size, axis);
            const auto shifted = Binary(state, spv::OpShiftRightLogical, TypeU32(state), base, queryLod);
            return Select(state, TypeU32(state), Binary(state, spv::OpUGreaterThan, TypeBool(state), shifted, ConstantU32(state, 1)), shifted, ConstantU32(state, 1));
        };
        const auto width = levelExtent(0u);
        const auto height = levelExtent(1u);
        auto inside = Binary(state, spv::OpLogicalAnd, TypeBool(state), lodInside, Binary(state, spv::OpULessThan, TypeBool(state), x, width));
        inside = Binary(state, spv::OpLogicalAnd, TypeBool(state), inside, Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), Binary(state, spv::OpISub, TypeU32(state), width, x), ConstantU32(state, elements)));
        inside = Binary(state, spv::OpLogicalAnd, TypeBool(state), inside, Binary(state, spv::OpULessThan, TypeBool(state), y, height));
        const auto first = Select(state, TypeU32(state), inside, Binary(state, spv::OpBitwiseAnd, TypeU32(state), x, ConstantU32(state, ~(elements - 1u))), ConstantU32(state, 0));
        const auto row = Select(state, TypeU32(state), inside, y, ConstantU32(state, 0));
        MemoryInfo texelMemory = access.mem;
        texelMemory.dmask = (1u << components) - 1u;
        texelMemory.dataDwords = components;
        texelMemory.componentCount = components;
        const ImageEmitAccess texelAccess{access.inst, texelMemory, access.image, access.address, access.table, access.slot};
        std::uint32_t words[4] = {ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0)};
        for (std::uint32_t element = 0; element < elements; ++element) {
            const auto column = Binary(state, spv::OpIAdd, TypeU32(state), first, ConstantU32(state, element));
            const auto coord = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 2), coord, column, row);
            const auto color = state.module.AllocateId();
            state.module.AddFunction(spv::OpImageFetch, ImageVectorType(state, access.image.numericClass, 4), color, image, coord, spv::ImageOperandsLodMask, queryLod);
            if (packed) {
                const auto info = PackedTexelFormat(ctx, access);
                std::uint32_t texelBits = 0;
                for (std::uint32_t component = 0; component < info.componentCount; ++component) texelBits += info.componentBits[component];
                if (texelBits * elements > 32u) ctx.Fail(access.inst, "is a MIMG PCK2/PCK4 load whose elements do not fit one dword");
                const auto raw = state.module.AllocateId();
                state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), raw, PackedImageTexel(ctx, access, color), 0u);
                const auto bits = Binary(state, spv::OpBitwiseAnd, TypeU32(state), raw, ConstantU32(state, texelBits == 32u ? 0xffffffffu : (1u << texelBits) - 1u));
                const auto shifted = element == 0u ? bits : Binary(state, spv::OpShiftLeftLogical, TypeU32(state), bits, ConstantU32(state, element * texelBits));
                words[0] = Binary(state, spv::OpBitwiseOr, TypeU32(state), words[0], Select(state, TypeU32(state), inside, shifted, ConstantU32(state, 0)));
                continue;
            }
            const auto texel = ResultVector(ctx, texelAccess, UnpackImageTexel(ctx, texelAccess, color), access.image.numericClass, false, false);
            for (std::uint32_t component = 0; component < components; ++component) {
                const auto value = state.module.AllocateId();
                state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), value, texel, component);
                words[element * components + component] = Select(state, TypeU32(state), inside, value, ConstantU32(state, 0));
            }
        }
        const auto result = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4), result, words[0], words[1], words[2], words[3]);
        return TableResult(ctx, access, result);
    }));
}

void EmitReadOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    if (access.mem.imageByElements != 0u) {
        EmitByReadOp(ctx, access);
        return;
    }
    auto& state = ctx.state;
    const auto& dimensionInfo = RdnaImageDimensionInfoFor(access.image.dimension);
    const auto numericClass = access.image.numericClass;
    const auto condition = ctx.Arg(access.inst, 2);
    ctx.Define(access.inst, EmitValueOrDefaultIfCondition(state, condition, TypeU32Vector(state, 4), ConstantU32CompositeZero(state, 4), [&]() {
        const auto descriptor = LoadSampledImageDescriptor(state, access.mem.resource, access.slot);
        const auto color = state.module.AllocateId();
        const auto coord = CoordU32(ctx, access);
        if (dimensionInfo.multisampled != 0u) {
            const auto& addressInfo = AddressDimension(access);
            if (addressInfo.multisampled == 0u || access.mem.imageAddressComponents <= addressInfo.coordinateComponents) {
                ctx.Fail(access.inst, "has no sample index in the image address");
            }
            state.module.AddFunction(spv::OpImageFetch, ImageVectorType(state, numericClass, 4), color, descriptor, coord, spv::ImageOperandsSampleMask, AddressU32(ctx, access, addressInfo.coordinateComponents));
        } else {
            state.module.AddFunction(spv::OpImageFetch, ImageVectorType(state, numericClass, 4), color, descriptor, coord, spv::ImageOperandsLodMask, LodU32(ctx, access));
        }
        if (access.mem.imagePacked) {
            return PackedImageTexel(ctx, access, color);
        }
        return TableResult(ctx, access, ResultVector(ctx, access, UnpackImageTexel(ctx, access, color), numericClass, false, false));
    }));
}

std::uint32_t ImageSampleIndex(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    if (RdnaImageDimensionInfoFor(access.image.dimension).multisampled == 0u) return ConstantU32(ctx.state, 0u);
    const auto& addressInfo = AddressDimension(access);
    if (addressInfo.multisampled == 0u || access.mem.imageAddressComponents <= addressInfo.coordinateComponents) ctx.Fail(access.inst, "has no sample index in the image address");
    return AddressU32(ctx, access, addressInfo.coordinateComponents);
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
        const auto texel = access.mem.imagePacked ? PackedStoreTexel(ctx, access, ctx.Arg(access.inst, 2)) : StoreTexel(ctx, access, ctx.Arg(access.inst, 2), uintImage);
        EmitStorageImageWrite(state, access.mem.resource, mipLod, coord, texel, ImageSampleIndex(ctx, access));
    });
}

void EmitAtomicOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    auto& state = ctx.state;
    if (access.slot != 0) {
        ctx.Fail(access.inst, "atomics through a bindless image table are unsupported");
    }
    const auto opcode = access.inst.Opcode();
    const auto condition = ctx.Arg(access.inst, access.inst.ArgumentCount() - 1u);
    if (IsImageAtomic64Opcode(opcode)) {
        ctx.Define(access.inst, EmitValueOrDefaultIfCondition(state, condition, TypeU64(state), ConstantU64(state, 0u), [&]() {
            const auto scalar = TypeScalarU64(state);
            const auto pointer = state.module.AllocateId();
            state.module.AddFunction(spv::OpImageTexelPointer, TypePointer(state, spv::StorageClassImage, scalar), pointer, StorageImageDescriptorPointer(state, access.mem.resource), CoordU32(ctx, access), ConstantU32(state, 0));
            const auto value = Unary(state, spv::OpBitcast, scalar, ctx.Arg(access.inst, 2));
            const auto old = state.module.AllocateId();
            if (opcode == IrOpcode::ImageAtomicCmpSwap64) {
                const auto comparator = Unary(state, spv::OpBitcast, scalar, ctx.Arg(access.inst, 3));
                state.module.AddFunction(spv::OpAtomicCompareExchange, scalar, old, pointer, ConstantU32(state, spv::ScopeDevice), ConstantU32(state, spv::MemorySemanticsMaskNone), ConstantU32(state, spv::MemorySemanticsMaskNone), value, comparator);
            } else {
                state.module.AddFunction(ImageAtomicOpcode(opcode), scalar, old, pointer, ConstantU32(state, spv::ScopeDevice), ConstantU32(state, spv::MemorySemanticsMaskNone), value);
            }
            EmitDeviceAtomicMemoryBarrier(state);
            return Unary(state, spv::OpBitcast, TypeU64(state), old);
        }));
        return;
    }
    const auto value = ctx.Arg(access.inst, 2);
    ctx.Define(access.inst, EmitValueOrZeroIfCondition(state, condition, [&]() {
        const auto pointer = state.module.AllocateId();
        const auto pointerType = TypePointer(state, spv::StorageClassImage, TypeU32(state));
        state.module.AddFunction(spv::OpImageTexelPointer, pointerType, pointer, StorageImageDescriptorPointer(state, access.mem.resource), CoordU32(ctx, access), ImageSampleIndex(ctx, access));
        if (opcode == IrOpcode::ImageAtomicInc32 || opcode == IrOpcode::ImageAtomicDec32) {
            return AtomicUpdate(state, pointer, ResourceKind::Image, [&](std::uint32_t current) {
                return opcode == IrOpcode::ImageAtomicInc32 ? AtomicIncrement(state, current, value) : AtomicDecrement(state, current, value);
            });
        }
        if (opcode == IrOpcode::ImageAtomicFMin32 || opcode == IrOpcode::ImageAtomicFMax32) {
            return AtomicUpdate(state, pointer, ResourceKind::Image, [&](std::uint32_t current) {
                return AtomicFloatMinMax(state, current, value, opcode == IrOpcode::ImageAtomicFMax32);
            });
        }
        if (opcode == IrOpcode::ImageAtomicFCmpSwap32) {
            const auto comparator = ctx.Arg(access.inst, 3);
            return AtomicUpdate(state, pointer, ResourceKind::Image, [&](std::uint32_t current) {
                return AtomicFloatCompareSwap(state, current, value, comparator);
            });
        }
        const auto old = state.module.AllocateId();
        if (opcode == IrOpcode::ImageAtomicCmpSwap32) {
            state.module.AddFunction(spv::OpAtomicCompareExchange, TypeU32(state), old, pointer, ConstantU32(state, spv::ScopeDevice), ConstantU32(state, spv::MemorySemanticsMaskNone), ConstantU32(state, spv::MemorySemanticsMaskNone), value, ctx.Arg(access.inst, 3));
        } else {
            state.module.AddFunction(ImageAtomicOpcode(opcode), TypeU32(state), old, pointer, ConstantU32(state, spv::ScopeDevice), ConstantU32(state, spv::MemorySemanticsMaskNone), value);
        }
        EmitDeviceAtomicMemoryBarrier(state);
        return old;
    }));
}

SampleSetup MakeSampleSetup(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    const auto& dimensionInfo = RdnaImageDimensionInfoFor(access.image.dimension);
    const auto layout = Layout(access.mem);
    const bool dref = HasFlag(access.mem, RdnaImageSampleFlagCompare);
    if (dref && access.image.conversionFormat != IrBufferFormat::Invalid) {
        ctx.Fail(access.inst, "uses depth comparison with a packed integer image");
    }
    if (ImageConversionFormat(access.image).type == SpirvFormatComponentType::Unorm) {
        ctx.Fail(access.inst, "samples or gathers a converted unorm image, which needs filtering in the shader and is not implemented");
    }
    if (ImageConversionFormat(access.image).type == SpirvFormatComponentType::Float) {
        ctx.Fail(access.inst, "samples or gathers a converted float image, which needs filtering in the shader and is not implemented");
    }
    const auto coord = CoordF32(ctx, access, layout.coord, dimensionInfo.coordinateComponents, AddressDimension(access).coordinateComponents);
    return {dimensionInfo, layout, access.image.numericClass, dref, coord};
}

void EmitPackedHorizontalGatherOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access) {
    auto& state = ctx.state;
    const auto& mem = access.mem;
    const auto dimension = access.image.dimension;
    if (dimension != RdnaImageDimension::Dim1D && dimension != RdnaImageDimension::Dim2D) {
        ctx.Fail(access.inst, "is a packed horizontal gather of an image dimension whose row and slice rules are not measured");
    }
    if (Layout(mem).clamp != NoImageComponent || HasFlag(mem, RdnaImageSampleFlagCompare)) {
        ctx.Fail(access.inst, "is a packed horizontal gather with depth comparison or an LOD clamp, which is not implemented");
    }
    if (AddressDimension(access).coordinateComponents < RdnaImageDimensionInfoFor(dimension).coordinateComponents) {
        ctx.Fail(access.inst, "has an image address with too few coordinate components");
    }
    const auto info = PackedTexelFormat(ctx, access);
    std::uint32_t texelBits = 0;
    for (std::uint32_t component = 0; component < info.componentCount; component++) {
        texelBits += info.componentBits[component];
    }
    std::uint32_t streamWords[4] = {ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0)};
    std::uint32_t rowInside = 0;
    const bool twoDimensional = dimension == RdnaImageDimension::Dim2D;
    if (texelBits <= 32u) {
        state.module.EmitCapability(spv::CapabilityImageQuery);
        const auto image = LoadSampledImageDescriptor(state, mem.resource, access.slot);
        const auto size = state.module.AllocateId();
        state.module.AddFunction(spv::OpImageQuerySizeLod, twoDimensional ? TypeI32Vector(state, 2) : TypeI32(state), size, image, ConstantU32(state, 0));
        const auto extent = [&](std::uint32_t axis) {
            if (!twoDimensional) {
                return size;
            }
            const auto value = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeExtract, TypeI32(state), value, size, axis);
            return value;
        };
        const auto texelFloor = [&](std::uint32_t address, std::uint32_t length, bool centre) {
            auto scaled = Binary(state, spv::OpFMul, TypeF32(state), AddressF32(ctx, access, address), Unary(state, spv::OpConvertSToF, TypeF32(state), length));
            state.module.AddAnnotation(spv::OpDecorate, scaled, spv::DecorationNoContraction);
            if (centre) {
                scaled = Binary(state, spv::OpFSub, TypeF32(state), scaled, ConstantF32(state, 0x3f000000u));
            }
            const auto floored = state.module.AllocateId();
            state.module.AddFunction(spv::OpExtInst, TypeF32(state), floored, GlslStd450(state), GLSLstd450Floor, scaled);
            return Unary(state, spv::OpConvertFToS, TypeI32(state), floored);
        };
        const auto clampInto = [&](std::uint32_t value, std::uint32_t length) {
            const auto last = Binary(state, spv::OpISub, TypeI32(state), length, ConstantI32(state, 1));
            const auto low = Select(state, TypeI32(state), Binary(state, spv::OpSLessThan, TypeBool(state), value, ConstantI32(state, 0)), ConstantI32(state, 0), value);
            return Select(state, TypeI32(state), Binary(state, spv::OpSLessThan, TypeBool(state), last, low), last, low);
        };
        const auto width = extent(0u);
        const auto anchor = texelFloor(0u, width, true);
        std::uint32_t row = 0;
        if (twoDimensional) {
            const auto height = extent(1u);
            const auto rawRow = texelFloor(1u, height, false);
            rowInside = Binary(state, spv::OpSLessThan, TypeBool(state), rawRow, height);
            row = clampInto(rawRow, height);
        }
        for (std::uint32_t element = 0; element < 4u; element++) {
            const auto column = clampInto(Binary(state, spv::OpIAdd, TypeI32(state), anchor, ConstantI32(state, static_cast<std::int32_t>(element) - 1)), width);
            std::uint32_t coord = column;
            if (twoDimensional) {
                coord = state.module.AllocateId();
                state.module.AddFunction(spv::OpCompositeConstruct, TypeI32Vector(state, 2), coord, column, row);
            }
            const auto color = state.module.AllocateId();
            state.module.AddFunction(spv::OpImageFetch, ImageVectorType(state, access.image.numericClass, 4), color, image, coord, spv::ImageOperandsLodMask, ConstantI32(state, 0));
            const auto packed = PackedImageTexel(ctx, access, color);
            const auto bits = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), bits, packed, 0u);
            const auto position = element * texelBits;
            const auto word = position / 32u;
            const auto shift = position % 32u;
            streamWords[word] = Binary(state, spv::OpBitwiseOr, TypeU32(state), streamWords[word], shift == 0u ? bits : Binary(state, spv::OpShiftLeftLogical, TypeU32(state), bits, ConstantU32(state, shift)));
            if (shift != 0u && shift + texelBits > 32u) {
                streamWords[word + 1u] = Binary(state, spv::OpBitwiseOr, TypeU32(state), streamWords[word + 1u], Binary(state, spv::OpShiftRightLogical, TypeU32(state), bits, ConstantU32(state, 32u - shift)));
            }
        }
    }
    std::uint32_t selected[4] = {ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0), ConstantU32(state, 0)};
    std::uint32_t next = 0;
    for (std::uint32_t bit = 0; bit < 4u; bit++) {
        if (((mem.dmask >> bit) & 1u) == 0u) {
            continue;
        }
        if (texelBits <= 32u) {
            selected[next] = rowInside != 0u ? Select(state, TypeU32(state), rowInside, streamWords[bit], ConstantU32(state, 0)) : streamWords[bit];
        }
        next++;
    }
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4), result, selected[0], selected[1], selected[2], selected[3]);
    ctx.Define(access.inst, TableResult(ctx, access, result));
}

void EmitGatherOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, const SampleSetup& setup) {
    auto& state = ctx.state;
    const auto& mem = access.mem;
    const auto dimension = access.image.dimension;
    if (setup.layout.clamp != NoImageComponent) {
        ctx.Fail(access.inst, "is a gather with an LOD clamp, which is not implemented");
    }
    if (setup.dref && (access.image.emulatedCompare & EmulatedCompare::Enabled) != 0u) {
        ctx.Fail(access.inst, "is a comparison gather of a color texture, which is not implemented");
    }
    if (dimension == RdnaImageDimension::Dim1D || dimension == RdnaImageDimension::Dim1DArray) {
        if (setup.dref || !HasFlag(mem, RdnaImageSampleFlagLevelZero) || HasFlag(mem, RdnaImageSampleFlagGatherHorizontal)) {
            ctx.Fail(access.inst, "has an unsupported 1D gather variant");
        }
        const auto sample = EmitOneDimensionalGatherLz(ctx, access, setup);
        ctx.Define(access.inst, TableResult(ctx, access, ResultVector(ctx, access, UnpackImageGather(ctx, access, sample), setup.numericClass, false, true)));
        return;
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
    auto selected = ConstantU32(state, image.indirectRoot);
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

void EmitEmulatedCompareSample(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, const SampleSetup& setup) {
    auto& state = ctx.state;
    const auto& mem = access.mem;
    const auto& image = access.image;
    const auto parameter = [&](std::uint32_t word) { return state.module.SpecializationConstant(TypeU32(state), PipelineSpecialization::CompareBase + mem.resource * PipelineSpecialization::CompareWords + word, 0u); };
    const auto equal = [&](std::uint32_t value, std::uint32_t literal) { return Binary(state, spv::OpIEqual, TypeBool(state), value, ConstantU32(state, literal)); };
    if (access.slot != 0) ctx.Fail(access.inst, "compares against a color texture through a bindless image table, which is not implemented");
    if (HasFlag(mem, RdnaImageSampleFlagDerivative) || HasFlag(mem, RdnaImageSampleFlagLod)) ctx.Fail(access.inst, "compares against a color texture with gradients or an explicit LOD, which is not implemented");
    const bool arrayed = image.dimension == RdnaImageDimension::Dim2DArray;
    if (image.dimension != RdnaImageDimension::Dim2D && !arrayed) ctx.Fail(access.inst, "compares against a color texture that is not a 2D or 2D array view, which is not implemented");
    const auto f32 = TypeF32(state);
    const auto i32 = TypeI32(state);
    const auto u32 = TypeU32(state);
    const auto ext = [&](std::uint32_t type, std::uint32_t op, std::initializer_list<std::uint32_t> args) {
        const auto value = state.module.AllocateId();
        std::vector<std::uint32_t> words{spv::OpExtInst, type, value, GlslStd450(state), op};
        words.insert(words.end(), args.begin(), args.end());
        state.module.AddFunction(std::span<const std::uint32_t>(words));
        return value;
    };
    const auto extract = [&](std::uint32_t type, std::uint32_t composite, std::uint32_t index) {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, type, value, composite, index);
        return value;
    };
    const auto f32Constant = [&](float value) { return ConstantF32(state, std::bit_cast<std::uint32_t>(value)); };
    const auto descriptor = LoadSampledImageDescriptor(state, mem.resource, access.slot);
    state.module.EmitCapability(spv::CapabilityImageQuery);
    const auto size = state.module.AllocateId();
    state.module.AddFunction(spv::OpImageQuerySizeLod, ImageViewSizeType(state, image.dimension), size, descriptor, ConstantU32(state, 0u));
    const auto width = Unary(state, spv::OpBitcast, i32, extract(u32, size, 0u));
    const auto height = Unary(state, spv::OpBitcast, i32, extract(u32, size, 1u));
    auto layer = ConstantU32(state, 0u);
    if (arrayed) {
        const auto layers = Unary(state, spv::OpBitcast, i32, extract(u32, size, 2u));
        const auto rounded = Unary(state, spv::OpConvertFToS, i32, ext(f32, GLSLstd450Floor, {Binary(state, spv::OpFAdd, f32, extract(f32, setup.coord, 2u), f32Constant(0.5f))}));
        layer = Unary(state, spv::OpBitcast, u32, ext(i32, GLSLstd450SClamp, {rounded, ConstantI32(state, 0), Binary(state, spv::OpISub, i32, layers, ConstantI32(state, 1))}));
    }
    auto reference = DrefValueF32(ctx, access, setup.layout);
    reference = Select(state, f32, equal(parameter(4u), EmulatedCompare::ReferenceUnorm), ext(f32, GLSLstd450FClamp, {reference, f32Constant(0.0f), f32Constant(1.0f)}), reference);
    reference = Select(state, f32, equal(parameter(4u), EmulatedCompare::ReferenceSnorm), ext(f32, GLSLstd450FClamp, {reference, f32Constant(-1.0f), f32Constant(1.0f)}), reference);
    const auto address = [&](std::uint32_t index, std::uint32_t extent, std::uint32_t mode) {
        return Select(state, i32, equal(mode, EmulatedCompare::AddressWrap), Binary(state, spv::OpSMod, i32, index, extent), ext(i32, GLSLstd450SClamp, {index, ConstantI32(state, 0), Binary(state, spv::OpISub, i32, extent, ConstantI32(state, 1))}));
    };
    const auto inside = [&](std::uint32_t index, std::uint32_t extent) {
        const auto notBelow = Binary(state, spv::OpSGreaterThanEqual, TypeBool(state), index, ConstantI32(state, 0));
        const auto below = Binary(state, spv::OpSLessThan, TypeBool(state), index, extent);
        return Binary(state, spv::OpLogicalAnd, TypeBool(state), notBelow, below);
    };
    const auto one = f32Constant(1.0f);
    const auto zero = f32Constant(0.0f);
    const auto borderRed = Select(state, f32, equal(parameter(5u), 1u), one, zero);
    const auto compareTexel = [&](std::uint32_t x, std::uint32_t y) {
        const auto modeX = parameter(2u);
        const auto modeY = parameter(3u);
        const auto ux = Unary(state, spv::OpBitcast, u32, address(x, width, modeX));
        const auto uy = Unary(state, spv::OpBitcast, u32, address(y, height, modeY));
        const auto coord = state.module.AllocateId();
        if (arrayed) state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 3), coord, ux, uy, layer);
        else state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 2), coord, ux, uy);
        const auto texel = state.module.AllocateId();
        state.module.AddFunction(spv::OpImageFetch, ImageVectorType(state, IrTextureNumericClass::Float, 4), texel, descriptor, coord, spv::ImageOperandsLodMask, ConstantU32(state, 0u));
        auto red = extract(f32, texel, 0u);
        red = Select(state, f32, equal(modeX, EmulatedCompare::AddressBorder), Select(state, f32, inside(x, width), red, borderRed), red);
        red = Select(state, f32, equal(modeY, EmulatedCompare::AddressBorder), Select(state, f32, inside(y, height), red, borderRed), red);
        constexpr std::array<spv::Op, 6> operations{spv::OpFOrdLessThan, spv::OpFOrdEqual, spv::OpFOrdLessThanEqual, spv::OpFOrdGreaterThan, spv::OpFOrdNotEqual, spv::OpFOrdGreaterThanEqual};
        auto result = Select(state, f32, equal(parameter(0u), 7u), one, zero);
        for (std::uint32_t index = 0; index < operations.size(); ++index) {
            const auto compared = Select(state, f32, Binary(state, operations[index], TypeBool(state), reference, red), one, zero);
            result = Select(state, f32, equal(parameter(0u), index + 1u), compared, result);
        }
        return result;
    };
    const auto scaledU = Binary(state, spv::OpFMul, f32, extract(f32, setup.coord, 0u), Unary(state, spv::OpConvertSToF, f32, width));
    const auto scaledV = Binary(state, spv::OpFMul, f32, extract(f32, setup.coord, 1u), Unary(state, spv::OpConvertSToF, f32, height));
    auto offsetX = ConstantI32(state, 0);
    auto offsetY = ConstantI32(state, 0);
    if (setup.layout.offset != NoImageComponent) {
        const auto offset = PackedOffset(ctx, access, setup.layout);
        offsetX = extract(i32, offset, 0u);
        offsetY = extract(i32, offset, 1u);
    }
    const auto texelIndex = [&](std::uint32_t coordinate, std::uint32_t offset) {
        return Binary(state, spv::OpIAdd, i32, Unary(state, spv::OpConvertFToS, i32, ext(f32, GLSLstd450Floor, {coordinate})), offset);
    };
    const auto result = EmitValueIfElse(state, equal(parameter(1u), 0u), f32, [&] {
        return compareTexel(texelIndex(scaledU, offsetX), texelIndex(scaledV, offsetY));
    }, [&] {
        const auto half = f32Constant(0.5f);
        const auto centreU = Binary(state, spv::OpFSub, f32, scaledU, half);
        const auto centreV = Binary(state, spv::OpFSub, f32, scaledV, half);
        const auto floorU = ext(f32, GLSLstd450Floor, {centreU});
        const auto floorV = ext(f32, GLSLstd450Floor, {centreV});
        const auto weightU = Binary(state, spv::OpFSub, f32, centreU, floorU);
        const auto weightV = Binary(state, spv::OpFSub, f32, centreV, floorV);
        const auto x0 = Binary(state, spv::OpIAdd, i32, Unary(state, spv::OpConvertFToS, i32, floorU), offsetX);
        const auto y0 = Binary(state, spv::OpIAdd, i32, Unary(state, spv::OpConvertFToS, i32, floorV), offsetY);
        const auto x1 = Binary(state, spv::OpIAdd, i32, x0, ConstantI32(state, 1));
        const auto y1 = Binary(state, spv::OpIAdd, i32, y0, ConstantI32(state, 1));
        const auto top = ext(f32, GLSLstd450FMix, {compareTexel(x0, y0), compareTexel(x1, y0), weightU});
        const auto bottom = ext(f32, GLSLstd450FMix, {compareTexel(x0, y1), compareTexel(x1, y1), weightU});
        return ext(f32, GLSLstd450FMix, {top, bottom, weightV});
    });
    ctx.Define(access.inst, TableResult(ctx, access, ResultVector(ctx, access, result, setup.numericClass, true, false)));
}

void EmitSampleOp(SpirvValueEmitContext& ctx, const ImageEmitAccess& access, const SampleSetup& setup) {
    auto& state = ctx.state;
    const auto& mem = access.mem;
    const auto& image = access.image;
    if (setup.dref && (image.emulatedCompare & EmulatedCompare::Enabled) != 0u) {
        EmitEmulatedCompareSample(ctx, access, setup);
        return;
    }
    const bool explicitLod = ImageSampleExplicitLod(mem.imageSampleFlags, state.program.Resources().stage);
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
        operands.push_back(CoordF32(ctx, access, setup.layout.gradX, setup.dimensionInfo.spatialComponents, AddressDimension(access).spatialComponents));
        operands.push_back(CoordF32(ctx, access, setup.layout.gradY, setup.dimensionInfo.spatialComponents, AddressDimension(access).spatialComponents));
    } else if (explicitLod) {
        operandMask |= spv::ImageOperandsLodMask;
        operands.push_back(HasFlag(mem, RdnaImageSampleFlagLod) ? AddressF32(ctx, access, setup.layout.lod) : ZeroF32(state));
    } else if (setup.layout.bias != NoImageComponent) {
        operandMask |= spv::ImageOperandsBiasMask;
        operands.push_back(AddressF32(ctx, access, setup.layout.bias));
    }
    if (setup.layout.clamp != NoImageComponent) {
        const auto& capabilities = state.supportedCapabilities;
        if (std::find(capabilities.begin(), capabilities.end(), static_cast<std::uint32_t>(spv::CapabilityMinLod)) == capabilities.end()) ctx.Fail(access.inst, "clamps its LOD, which needs the device's shaderResourceMinLod");
        const auto clamp = AddressF32(ctx, access, setup.layout.clamp);
        if ((operandMask & spv::ImageOperandsLodMask) != 0u) {
            const auto clamped = state.module.AllocateId();
            state.module.AddFunction(spv::OpExtInst, TypeF32(state), clamped, GlslStd450(state), GLSLstd450FMax, operands.back(), clamp);
            operands.back() = clamped;
        } else {
            state.module.EmitCapability(spv::CapabilityMinLod);
            operandMask |= spv::ImageOperandsMinLodMask;
            operands.push_back(clamp);
        }
    }
    const auto constantOffset = [&]() -> const IrValue* {
        const auto component = GetRdnaImageAddressComponentLayout(mem.imageSampleFlags, setup.layout.offset);
        const auto argument = component.bitOffset / 32u;
        if (component.bitWidth != 32u || argument >= access.address.ArgumentCount()) return nullptr;
        const auto* value = access.address.Argument(argument)->Resolve();
        return value->HasImmediate() ? value : nullptr;
    };
    if (setup.layout.offset != NoImageComponent && constantOffset() == nullptr) {
        const bool gatherExtended = std::find(state.supportedCapabilities.begin(), state.supportedCapabilities.end(), static_cast<std::uint32_t>(spv::CapabilityImageGatherExtended)) != state.supportedCapabilities.end();
        if (!state.nonConstantImageOffsets || !gatherExtended) {
            ctx.Fail(access.inst, "has a texel offset that is not a constant, which image sampling takes only with VK_KHR_maintenance8 and shaderImageGatherExtended");
        }
        state.module.EmitCapability(spv::CapabilityImageGatherExtended);
        operandMask |= spv::ImageOperandsOffsetMask;
        operands.insert(operands.end() - ((operandMask & spv::ImageOperandsMinLodMask) != 0u ? 1 : 0), PackedOffset(ctx, access, setup.layout));
    } else if (setup.layout.offset != NoImageComponent) {
        const auto bits = constantOffset()->ImmediateU32();
        std::array<std::uint32_t, 3> values{};
        for (std::uint32_t index = 0; index < setup.dimensionInfo.spatialComponents; index++) {
            const auto field = (bits >> (index * 8u)) & 0x3fu;
            values[index] = ConstantI32(state, static_cast<std::int32_t>(field ^ 0x20u) - 0x20);
        }
        const auto count = setup.dimensionInfo.spatialComponents;
        const auto offset = count == 1u ? values[0] : count == 2u
            ? state.module.Constant(spv::OpConstantComposite, TypeI32Vector(state, 2), values[0], values[1])
            : state.module.Constant(spv::OpConstantComposite, TypeI32Vector(state, 3), values[0], values[1], values[2]);
        operandMask |= spv::ImageOperandsConstOffsetMask;
        operands.insert(operands.end() - ((operandMask & spv::ImageOperandsMinLodMask) != 0u ? 1 : 0), offset);

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
    if (access.image.srgbDecode) {
        ctx.Fail(access.inst, "samples or gathers an sRGB image the device cannot sample, which is not implemented");
    }
    if (access.mem.imagePacked) {
        EmitPackedHorizontalGatherOp(ctx, access);
        return;
    }
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

void EmitImageMode(SpirvValueEmitContext& ctx, const IrValue& inst, const ImageResource& image) {
    const auto irOpcode = inst.Opcode();
    const auto imageInfo = ImageOpcodeInfoOf(irOpcode);
    const auto& mem = ctx.Memory(inst);
    ctx.ResourceIndex(inst.Argument(0), IrOpcode::GetImageResource);
    const auto* address = ctx.ImageAddress(inst.Argument(imageInfo.needsSampler ? 2 : 1));
    if (address == nullptr) {
        ctx.Fail(inst, "has no image address");
    }
    const ImageEmitAccess access{inst, mem, image, *address, TableSlot(ctx, inst, mem, image)};
    if (imageInfo.access == ImageAccess::Atomic) {
        EmitAtomicOp(ctx, access);
        return;
    }
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
        default:
            ctx.Fail(inst, "has no image SPIR-V emitter");
    }
}

void EmitImage(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto& memory = ctx.Memory(inst);
    const auto& base = ImageResourceOf(state, memory);
    const auto modes = ResourceMaterializer::RuntimeImageModes(base);
    state.runtimeImageMetadata = ConstantU32(state, memory.resource);
    const auto table = TableSlot(ctx, inst, memory, base);
    if (table.slot != 0u) {
        state.runtimeImageMetadata = table.slot;
    }
    const auto emitMode = [&](const ImageResource& mode) {
        state.runtimeImage = &mode;
        if (mode.packedFormat == IrBufferFormat::Fmask8_S2_F1) {
            if (inst.Opcode() == IrOpcode::ImageRead && memory.dataBits == 32u) {
                const auto value = state.module.AllocateId();
                state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4u), value, ConstantU32(state, 0x76543210u), ConstantU32(state, 0xfedcba98u), ConstantU32(state, 0u), ConstantU32(state, 0u));
                ctx.Define(inst, Select(state, TypeU32Vector(state, 4u), ctx.Arg(inst, 2u), value, ConstantU32CompositeZero(state, 4u)));
            } else {
                ctx.Fail(inst, "FMASK requires a 32-bit image read");
            }
        } else if (mode.depthBits && memory.dataBits == 16u) {
            ctx.Fail(inst, "depth bits require a 32-bit image result");
        } else {
            EmitImageMode(ctx, inst, mode);
        }
    };
    if (modes.size() == 1u) {
        emitMode(modes.front());
        state.runtimeImage = nullptr;
        state.runtimeImageMetadata = 0u;
        return;
    }
    const auto selector = base.indirectRoot == ImageResource::NoIndirectImage ? state.module.SpecializationConstant(TypeU32(state), PipelineSpecialization::ImageBase + memory.resource * PipelineSpecialization::ImageWords, 0u) : Binary(state, spv::OpShiftRightLogical, TypeU32(state), RuntimeImageDword(state, memory.resource, offsetof(RuntimeAbi::ResourceMetadata, flags) / sizeof(std::uint32_t)), ConstantU32(state, 1u));
    const bool returnsValue = inst.Opcode() != IrOpcode::ImageWrite;
    const auto resultId = returnsValue ? ctx.Result(inst) : 0u;
    ctx.definitions.erase(&inst);
    const auto merge = state.module.AllocateId();
    const auto invalid = state.module.AllocateId();
    std::vector<std::uint32_t> labels(modes.size());
    std::vector<std::uint32_t> words{spv::OpSwitch, selector, invalid};
    for (std::uint32_t index = 0u; index < modes.size(); ++index) {
        labels[index] = state.module.AllocateId();
        words.insert(words.end(), {index, labels[index]});
    }
    state.module.AddFunction(spv::OpSelectionMerge, merge, spv::SelectionControlMaskNone);
    state.module.AddFunction(words);
    std::vector<std::uint32_t> incoming;
    for (std::uint32_t index = 0u; index < modes.size(); ++index) {
        EmitLabel(state, labels[index]);
        std::uint32_t modeMerge = 0u;
        if (base.indirectRoot != ImageResource::NoIndirectImage) {
            const auto enabled = state.module.SpecializationConstant(TypeU32(state), PipelineSpecialization::ImageModeBase + memory.resource * PipelineSpecialization::ImageModeStride + index, 1u);
            const auto active = Binary(state, spv::OpINotEqual, TypeBool(state), enabled, ConstantU32(state, 0u));
            const auto activeLabel = state.module.AllocateId();
            const auto inactiveLabel = state.module.AllocateId();
            modeMerge = state.module.AllocateId();
            state.module.AddFunction(spv::OpSelectionMerge, modeMerge, spv::SelectionControlMaskNone);
            state.module.AddFunction(spv::OpBranchConditional, active, activeLabel, inactiveLabel);
            EmitLabel(state, inactiveLabel);
            state.module.AddFunction(spv::OpUnreachable);
            EmitLabel(state, activeLabel);
        }
        emitMode(modes[index]);
        if (modeMerge != 0u) {
            state.module.AddFunction(spv::OpBranch, modeMerge);
            EmitLabel(state, modeMerge);
        }
        if (returnsValue) incoming.insert(incoming.end(), {ctx.Def(&inst), state.currentLabel});
        ctx.definitions.erase(&inst);
        state.module.AddFunction(spv::OpBranch, merge);
    }
    state.runtimeImage = nullptr;
    state.runtimeImageMetadata = 0u;
    EmitLabel(state, invalid);
    state.module.AddFunction(spv::OpUnreachable);
    EmitLabel(state, merge);
    if (returnsValue) {
        std::vector<std::uint32_t> phi{spv::OpPhi, TypeId(state, inst.Type()), resultId};
        phi.insert(phi.end(), incoming.begin(), incoming.end());
        state.module.AddFunction(phi);
        ctx.definitions.emplace(&inst, resultId);
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

void EmitImageAtomicCmpSwap32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicISub32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicSMin32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicSMax32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicInc32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicDec32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicFCmpSwap32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicFMin32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicFMax32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicSwap64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicIAdd64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicISub64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicUMin64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicUMax64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicSMin64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicSMax64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicAnd64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicOr64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicXor64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

void EmitImageAtomicCmpSwap64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitImage(ctx, inst);
}

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
        const bool node64 = ctx.Memory(inst).addressIsFull;
        const std::uint32_t ray = node64 ? 2u : 1u;

        const auto nodeLow = component(0);
        const auto node = node64 ? pair(nodeLow, component(1)) : widen(nodeLow);
        const auto type = op(spv::OpBitwiseAnd, u32, nodeLow, uint(7u));
        const auto nodeIndex = op(spv::OpShiftRightLogical, u64, node, BdaConstant(state, 3u));
        const auto baseUnits = pair(word[0], op(spv::OpBitwiseAnd, u32, word[1], uint(0xffu)));
        const auto lastNode = pair(word[2], op(spv::OpBitwiseAnd, u32, word[3], uint(0x3ffu)));
        const auto boxGrow = op(spv::OpBitwiseAnd, u32, op(spv::OpShiftRightLogical, u32, word[1], uint(23u)), uint(0xffu));
        const auto boxSort = op(spv::OpINotEqual, boolean, op(spv::OpBitwiseAnd, u32, word[1], uint(0x80000000u)), uint(0u));
        const auto barycentrics = op(spv::OpINotEqual, boolean, op(spv::OpBitwiseAnd, u32, word[3], uint(1u << 24u)), uint(0u));
        const auto nodeAddress = op(spv::OpIAdd, u64, op(spv::OpShiftLeftLogical, u64, baseUnits, BdaConstant(state, 8u)), op(spv::OpShiftLeftLogical, u64, op(spv::OpBitwiseAnd, u64, node, BdaConstant(state, ~std::uint64_t{7u})), BdaConstant(state, 3u)));
        const auto within = [&](spv::Op compare) {
            const auto inRange = op(compare, boolean, nodeIndex, lastNode);
            return node64 ? inRange : both(op(spv::OpINotEqual, boolean, baseUnits, BdaConstant(state, 0u)), inRange);
        };
        const auto valid = within(spv::OpULessThanEqual);
        const auto isTriangle = both(valid, op(spv::OpULessThanEqual, boolean, type, uint(1u)));
        const auto isBox16 = both(valid, op(spv::OpIEqual, boolean, type, uint(4u)));
        const auto isBox32 = both(within(spv::OpULessThan), op(spv::OpIEqual, boolean, type, uint(5u)));

        const auto extent = asFloat(component(ray));
        std::array<std::uint32_t, 3> origin{};
        std::array<std::uint32_t, 3> direction{};
        std::array<std::uint32_t, 3> inverse{};
        for (std::uint32_t axis = 0; axis < 3u; ++axis) {
            origin[axis] = asFloat(component(ray + 1u + axis));
            if (!a16) {
                direction[axis] = asFloat(component(ray + 4u + axis));
                inverse[axis] = asFloat(component(ray + 7u + axis));
                continue;
            }
            const auto half = [&](std::uint32_t index) {
                const auto packed = component(ray + 4u + index / 2u);
                return EmitF16BitsToF32(state, index % 2u == 0u ? op(spv::OpBitwiseAnd, u32, packed, uint(0xffffu)) : op(spv::OpShiftRightLogical, u32, packed, uint(16u)));
            };
            direction[axis] = half(axis);
            inverse[axis] = half(3u + axis);
        }

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

        const auto boxes = [&](bool fp16) {
            const auto d = nodeDwords(fp16 ? 16u : 28u);
            const auto bound = [&](std::uint32_t child, std::uint32_t index) {
                const auto position = child * 6u + index;
                if (!fp16) {
                    return asFloat(d[4u + position]);
                }
                const auto packed = d[4u + position / 2u];
                return EmitF16BitsToF32(state, position % 2u == 0u ? op(spv::OpBitwiseAnd, u32, packed, uint(0xffffu)) : op(spv::OpShiftRightLogical, u32, packed, uint(16u)));
            };
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
