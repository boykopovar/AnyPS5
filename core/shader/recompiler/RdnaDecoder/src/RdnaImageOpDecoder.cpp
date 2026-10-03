#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include <bit>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

struct ImageOpcodeInfo {
    std::uint32_t encoding;
    RdnaOpcode opcode;
    const char* name;
    std::uint32_t flags;
    bool sample;
    bool gather;
    bool atomic;
};

constexpr ImageOpcodeInfo imageOpcodes[] = {
    {0x20u, RdnaOpcode::ImageSample, "image_sample", 0, true, false, false},
    {0x21u, RdnaOpcode::ImageSampleCl, "image_sample_cl", RdnaImageSampleFlagLodClamp, true, false, false},
    {0x22u, RdnaOpcode::ImageSampleD, "image_sample_d", RdnaImageSampleFlagDerivative, true, false, false},
    {0x23u, RdnaOpcode::ImageSampleDCl, "image_sample_d_cl", RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLodClamp, true, false, false},
    {0x24u, RdnaOpcode::ImageSample, "image_sample_l", RdnaImageSampleFlagLod, true, false, false},
    {0x25u, RdnaOpcode::ImageSample, "image_sample_b", RdnaImageSampleFlagBias, true, false, false},
    {0x26u, RdnaOpcode::ImageSampleBCl, "image_sample_b_cl", RdnaImageSampleFlagBias | RdnaImageSampleFlagLodClamp, true, false, false},
    {0x27u, RdnaOpcode::ImageSample, "image_sample_lz", RdnaImageSampleFlagLevelZero, true, false, false},
    {0x28u, RdnaOpcode::ImageSampleC, "image_sample_c", RdnaImageSampleFlagCompare, true, false, false},
    {0x29u, RdnaOpcode::ImageSampleCCl, "image_sample_c_cl", RdnaImageSampleFlagCompare | RdnaImageSampleFlagLodClamp, true, false, false},
    {0x2au, RdnaOpcode::ImageSampleCD, "image_sample_c_d", RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative, true, false, false},
    {0x2cu, RdnaOpcode::ImageSampleCL, "image_sample_c_l", RdnaImageSampleFlagCompare | RdnaImageSampleFlagLod, true, false, false},
    {0x2du, RdnaOpcode::ImageSampleCB, "image_sample_c_b", RdnaImageSampleFlagCompare | RdnaImageSampleFlagBias, true, false, false},
    {0x2fu, RdnaOpcode::ImageSample, "image_sample_c_lz", RdnaImageSampleFlagCompare | RdnaImageSampleFlagLevelZero, true, false, false},
    {0x30u, RdnaOpcode::ImageSampleO, "image_sample_o", RdnaImageSampleFlagOffset, true, false, false},
    {0x31u, RdnaOpcode::ImageSampleClO, "image_sample_cl_o", RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagOffset, true, false, false},
    {0x32u, RdnaOpcode::ImageSampleDO, "image_sample_d_o", RdnaImageSampleFlagDerivative | RdnaImageSampleFlagOffset, true, false, false},
    {0x34u, RdnaOpcode::ImageSample, "image_sample_l_o", RdnaImageSampleFlagLod | RdnaImageSampleFlagOffset, true, false, false},
    {0x35u, RdnaOpcode::ImageSampleBO, "image_sample_b_o", RdnaImageSampleFlagBias | RdnaImageSampleFlagOffset, true, false, false},
    {0x37u, RdnaOpcode::ImageSampleLzO, "image_sample_lz_o", RdnaImageSampleFlagLevelZero | RdnaImageSampleFlagOffset, true, false, false},
    {0x38u, RdnaOpcode::ImageSampleCO, "image_sample_c_o", RdnaImageSampleFlagCompare | RdnaImageSampleFlagOffset, true, false, false},
    {0x68u, RdnaOpcode::ImageSampleCd, "image_sample_cd", RdnaImageSampleFlagDerivative | RdnaImageSampleFlagCd, true, false, false},
    {0x69u, RdnaOpcode::ImageSampleCdCl, "image_sample_cd_cl", RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagCd, true, false, false},
    {0x6au, RdnaOpcode::ImageSampleCCd, "image_sample_c_cd", RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagCd, true, false, false},
    {0x6bu, RdnaOpcode::ImageSampleCCdCl, "image_sample_c_cd_cl", RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagCd, true, false, false},
    {0x6cu, RdnaOpcode::ImageSampleCdO, "image_sample_cd_o", RdnaImageSampleFlagDerivative | RdnaImageSampleFlagOffset | RdnaImageSampleFlagCd, true, false, false},
    {0x6du, RdnaOpcode::ImageSampleCdClO, "image_sample_cd_cl_o", RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagOffset | RdnaImageSampleFlagCd, true, false, false},
    {0x6eu, RdnaOpcode::ImageSampleCCdO, "image_sample_c_cd_o", RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagOffset | RdnaImageSampleFlagCd, true, false, false},
    {0x6fu, RdnaOpcode::ImageSampleCCdClO, "image_sample_c_cd_cl_o", RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagOffset | RdnaImageSampleFlagCd, true, false, false},
    {0xa2u, RdnaOpcode::ImageSampleDG16, "image_sample_d_g16", RdnaImageSampleFlagDerivative | RdnaImageSampleFlagG16, true, false, false},
    {0xa3u, RdnaOpcode::ImageSampleDClG16, "image_sample_d_cl_g16", RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagG16, true, false, false},
    {0xaau, RdnaOpcode::ImageSampleCDG16, "image_sample_c_d_g16", RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagG16, true, false, false},
    {0xabu, RdnaOpcode::ImageSampleCDClG16, "image_sample_c_d_cl_g16", RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagG16, true, false, false},
    {0xb2u, RdnaOpcode::ImageSampleDOG16, "image_sample_d_o_g16", RdnaImageSampleFlagDerivative | RdnaImageSampleFlagOffset | RdnaImageSampleFlagG16, true, false, false},
    {0xb3u, RdnaOpcode::ImageSampleDClOG16, "image_sample_d_cl_o_g16", RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagOffset | RdnaImageSampleFlagG16, true, false, false},
    {0xbau, RdnaOpcode::ImageSampleCDOG16, "image_sample_c_d_o_g16", RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagOffset | RdnaImageSampleFlagG16, true, false, false},
    {0xbbu, RdnaOpcode::ImageSampleCDClOG16, "image_sample_c_d_cl_o_g16", RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagOffset | RdnaImageSampleFlagG16, true, false, false},
    {0xe8u, RdnaOpcode::ImageSampleCdG16, "image_sample_cd_g16", RdnaImageSampleFlagDerivative | RdnaImageSampleFlagCd | RdnaImageSampleFlagG16, true, false, false},
    {0xe9u, RdnaOpcode::ImageSampleCdClG16, "image_sample_cd_cl_g16", RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagCd | RdnaImageSampleFlagG16, true, false, false},
    {0xeau, RdnaOpcode::ImageSampleCCdG16, "image_sample_c_cd_g16", RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagCd | RdnaImageSampleFlagG16, true, false, false},
    {0xebu, RdnaOpcode::ImageSampleCCdClG16, "image_sample_c_cd_cl_g16", RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagCd | RdnaImageSampleFlagG16, true, false, false},
    {0xecu, RdnaOpcode::ImageSampleCdOG16, "image_sample_cd_o_g16", RdnaImageSampleFlagDerivative | RdnaImageSampleFlagOffset | RdnaImageSampleFlagCd | RdnaImageSampleFlagG16, true, false, false},
    {0xedu, RdnaOpcode::ImageSampleCdClOG16, "image_sample_cd_cl_o_g16", RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagOffset | RdnaImageSampleFlagCd | RdnaImageSampleFlagG16, true, false, false},
    {0xeeu, RdnaOpcode::ImageSampleCCdOG16, "image_sample_c_cd_o_g16", RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagOffset | RdnaImageSampleFlagCd | RdnaImageSampleFlagG16, true, false, false},
    {0xefu, RdnaOpcode::ImageSampleCCdClOG16, "image_sample_c_cd_cl_o_g16", RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagOffset | RdnaImageSampleFlagCd | RdnaImageSampleFlagG16, true, false, false},
    {0xa0u, RdnaOpcode::ImageSample, "image_sample_a", RdnaImageSampleFlagAdjust, true, false, false},
    {0xa1u, RdnaOpcode::ImageSample, "image_sample_cl_a", RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagAdjust, true, false, false},
    {0xa5u, RdnaOpcode::ImageSample, "image_sample_b_a", RdnaImageSampleFlagBias | RdnaImageSampleFlagAdjust, true, false, false},
    {0xa8u, RdnaOpcode::ImageSample, "image_sample_c_a", RdnaImageSampleFlagCompare | RdnaImageSampleFlagAdjust, true, false, false},
    {0xb0u, RdnaOpcode::ImageSample, "image_sample_o_a", RdnaImageSampleFlagOffset | RdnaImageSampleFlagAdjust, true, false, false},
    {0x3cu, RdnaOpcode::ImageSampleCLO, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagLod | RdnaImageSampleFlagOffset, true, false, false},
    {0x3fu, RdnaOpcode::ImageSampleCLzO, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagLevelZero | RdnaImageSampleFlagOffset, true, false, false},
    {0x3au, RdnaOpcode::ImageSampleCDO, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagOffset, true, false, false},
    {0x3bu, RdnaOpcode::ImageSampleCDClO, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagOffset, true, false, false},
    {0x2bu, RdnaOpcode::ImageSampleCDCl, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLodClamp, true, false, false},
    {0x39u, RdnaOpcode::ImageSampleCClO, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagOffset, true, false, false},
    {0x3du, RdnaOpcode::ImageSampleCBO, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagBias | RdnaImageSampleFlagOffset, true, false, false},
    {0x3eu, RdnaOpcode::ImageSampleCBClO, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagBias | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagOffset, true, false, false},
    {0x2eu, RdnaOpcode::ImageSampleCBCl, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagBias | RdnaImageSampleFlagLodClamp, true, false, false},
    {0x36u, RdnaOpcode::ImageSampleBClO, nullptr, RdnaImageSampleFlagBias | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagOffset, true, false, false},
    {0x47u, RdnaOpcode::ImageGather4Lz, nullptr, RdnaImageSampleFlagLevelZero, false, true, false},
    {0x48u, RdnaOpcode::ImageGather4C, nullptr, RdnaImageSampleFlagCompare, false, true, false},
    {0x4fu, RdnaOpcode::ImageGather4CLz, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagLevelZero, false, true, false},
    {0x57u, RdnaOpcode::ImageGather4LzO, nullptr, RdnaImageSampleFlagLevelZero | RdnaImageSampleFlagOffset, false, true, false},
    {0x58u, RdnaOpcode::ImageGather4CO, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagOffset, false, true, false},
    {0x5fu, RdnaOpcode::ImageGather4CLzO, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagLevelZero | RdnaImageSampleFlagOffset, false, true, false},
    {0x61u, RdnaOpcode::ImageGather4h, nullptr, RdnaImageSampleFlagGatherHorizontal, false, true, false},
    {0x50u, RdnaOpcode::ImageGather4O, nullptr, RdnaImageSampleFlagOffset, false, true, false},
    {0x54u, RdnaOpcode::ImageGather4LO, nullptr, RdnaImageSampleFlagLod | RdnaImageSampleFlagOffset, false, true, false},
    {0x44u, RdnaOpcode::ImageGather4L, nullptr, RdnaImageSampleFlagLod, false, true, false},
    {0x5cu, RdnaOpcode::ImageGather4CLO, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagLod | RdnaImageSampleFlagOffset, false, true, false},
    {0x4cu, RdnaOpcode::ImageGather4CL, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagLod, false, true, false},
    {0x59u, RdnaOpcode::ImageGather4CClO, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagOffset, false, true, false},
    {0x49u, RdnaOpcode::ImageGather4CCl, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagLodClamp, false, true, false},
    {0x5du, RdnaOpcode::ImageGather4CBO, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagBias | RdnaImageSampleFlagOffset, false, true, false},
    {0x5eu, RdnaOpcode::ImageGather4CBClO, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagBias | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagOffset, false, true, false},
    {0x4eu, RdnaOpcode::ImageGather4CBCl, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagBias | RdnaImageSampleFlagLodClamp, false, true, false},
    {0x4du, RdnaOpcode::ImageGather4CB, nullptr, RdnaImageSampleFlagCompare | RdnaImageSampleFlagBias, false, true, false},
    {0x51u, RdnaOpcode::ImageGather4ClO, nullptr, RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagOffset, false, true, false},
    {0x41u, RdnaOpcode::ImageGather4Cl, nullptr, RdnaImageSampleFlagLodClamp, false, true, false},
    {0x55u, RdnaOpcode::ImageGather4BO, nullptr, RdnaImageSampleFlagBias | RdnaImageSampleFlagOffset, false, true, false},
    {0x56u, RdnaOpcode::ImageGather4BClO, nullptr, RdnaImageSampleFlagBias | RdnaImageSampleFlagLodClamp | RdnaImageSampleFlagOffset, false, true, false},
    {0x46u, RdnaOpcode::ImageGather4BCl, nullptr, RdnaImageSampleFlagBias | RdnaImageSampleFlagLodClamp, false, true, false},
    {0x45u, RdnaOpcode::ImageGather4B, nullptr, RdnaImageSampleFlagBias, false, true, false},
    {0x40u, RdnaOpcode::ImageGather4, nullptr, 0, false, true, false},
    {0x0fu, RdnaOpcode::ImageAtomicSwap, nullptr, 0, false, false, true},
    {0x11u, RdnaOpcode::ImageAtomicAdd, nullptr, 0, false, false, true},
    {0x15u, RdnaOpcode::ImageAtomicUmin, nullptr, 0, false, false, true},
    {0x17u, RdnaOpcode::ImageAtomicUmax, nullptr, 0, false, false, true},
    {0x18u, RdnaOpcode::ImageAtomicAnd, nullptr, 0, false, false, true},
    {0x19u, RdnaOpcode::ImageAtomicOr, nullptr, 0, false, false, true},
    {0x1au, RdnaOpcode::ImageAtomicXor, nullptr, 0, false, false, true},
    {0x10u, RdnaOpcode::ImageAtomicCmpswap, nullptr, 0, false, false, true},
    {0x12u, RdnaOpcode::ImageAtomicSub, nullptr, 0, false, false, true},
    {0x14u, RdnaOpcode::ImageAtomicSmin, nullptr, 0, false, false, true},
    {0x16u, RdnaOpcode::ImageAtomicSmax, nullptr, 0, false, false, true},
    {0x1bu, RdnaOpcode::ImageAtomicInc, nullptr, 0, false, false, true},
    {0x1cu, RdnaOpcode::ImageAtomicDec, nullptr, 0, false, false, true},
    {0x00u, RdnaOpcode::ImageLoad, nullptr, 0, false, false, false},
    {0x01u, RdnaOpcode::ImageLoadMip, nullptr, 0, false, false, false},
    {0x08u, RdnaOpcode::ImageStore, nullptr, 0, false, false, false},
    {0x09u, RdnaOpcode::ImageStoreMip, nullptr, 0, false, false, false},
    {0x0eu, RdnaOpcode::ImageGetResinfo, nullptr, 0, false, false, false},
    {0x60u, RdnaOpcode::ImageGetLod, nullptr, 0, false, false, false},
    {0xe6u, RdnaOpcode::ImageBvhIntersectRay, "image_bvh_intersect_ray", 0, false, false, false},
};

const ImageOpcodeInfo& lookupOpcode(std::uint32_t opcode) {
    for (const auto& entry : imageOpcodes) {
        if (entry.encoding == opcode) {
            return entry;
        }
    }
    throw std::runtime_error("unsupported MIMG opcode");
}

void validateFlags(std::uint32_t flags) {
    constexpr std::uint32_t known = ((1u << 12u) - 1u) | RdnaImageSampleGradientCountMask;
    if ((flags & ~known) != 0u) {
        throw std::runtime_error("unknown image address flags");
    }
    if (((flags & RdnaImageSampleGradientCountMask) != 0u) != ((flags & RdnaImageSampleFlagDerivative) != 0u)) {
        throw std::runtime_error("image derivative flags without a gradient count");
    }
    const auto lodModes = flags & (RdnaImageSampleFlagLod | RdnaImageSampleFlagBias | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLevelZero);
    if (std::popcount(lodModes) > 1) {
        throw std::runtime_error("conflicting image LOD modes");
    }
}

struct ComponentShape {
    std::uint32_t width;
    bool startsDword;
};

ComponentShape componentShape(std::uint32_t flags, std::uint32_t component) {
    const bool a16 = (flags & RdnaImageSampleFlagA16) != 0u;
    std::uint32_t cursor = 0;
    if ((flags & RdnaImageSampleFlagOffset) != 0u) {
        if (component == cursor) {
            return {32, true};
        }
        ++cursor;
    }
    if ((flags & RdnaImageSampleFlagBias) != 0u) {
        if (component == cursor) {
            return {a16 ? 16u : 32u, !a16};
        }
        ++cursor;
    }
    if ((flags & RdnaImageSampleFlagCompare) != 0u) {
        if (component == cursor) {
            return {32, true};
        }
        ++cursor;
    }
    const auto gradients = (flags & RdnaImageSampleGradientCountMask) >> RdnaImageSampleGradientCountShift;
    if (component < cursor + gradients * 2u) {
        if ((flags & RdnaImageSampleFlagG16) == 0u) {
            return {32, true};
        }
        return {16, (component - cursor) % gradients == 0u};
    }
    cursor += gradients * 2u;
    return {a16 ? 16u : 32u, !a16 || (gradients != 0u && component == cursor)};
}

RdnaImageDimension decodeDimension(std::uint32_t dimension) {
    switch (dimension) {
        case 0: return RdnaImageDimension::Dim1D;
        case 1: return RdnaImageDimension::Dim2D;
        case 2: return RdnaImageDimension::Dim3D;
        case 3: return RdnaImageDimension::Dim2DArray;
        case 4: return RdnaImageDimension::Dim1DArray;
        case 5: return RdnaImageDimension::Dim2DArray;
        case 6: return RdnaImageDimension::Dim2DMsaa;
        case 7: return RdnaImageDimension::Dim2DMsaaArray;
        default: throw std::runtime_error("invalid image dimension");
    }
}

std::uint32_t coordinateCount(RdnaImageDimension dimension) {
    switch (dimension) {
        case RdnaImageDimension::Dim1D: return 1;
        case RdnaImageDimension::Dim1DArray:
        case RdnaImageDimension::Dim2D: return 2;
        case RdnaImageDimension::Dim3D:
        case RdnaImageDimension::Dim2DArray:
        case RdnaImageDimension::Dim2DMsaa: return 3;
        case RdnaImageDimension::Dim2DMsaaArray: return 4;
        default: throw std::runtime_error("unknown image coordinate layout");
    }
}

std::uint32_t gradientCount(RdnaImageDimension dimension) {
    switch (dimension) {
        case RdnaImageDimension::Dim1D:
        case RdnaImageDimension::Dim1DArray: return 1;
        case RdnaImageDimension::Dim2D:
        case RdnaImageDimension::Dim2DArray: return 2;
        case RdnaImageDimension::Dim3D: return 3;
        default: throw std::runtime_error("unsupported image gradient dimension");
    }
}

RdnaOperand vectorRegister(std::uint32_t reg) {
    RdnaOperand operand{};
    operand.kind = RdnaOperandKind::VectorRegister;
    operand.reg = reg;
    return operand;
}

RdnaOperand scalarRegister(std::uint32_t reg) {
    if (reg >= 106u) {
        throw std::runtime_error("invalid MIMG scalar register");
    }
    RdnaOperand operand{};
    operand.kind = RdnaOperandKind::ScalarRegister;
    operand.reg = reg;
    return operand;
}

}

const char* GetRdnaImageSampleOpcodeName(std::uint32_t opcode) {
    const auto& info = lookupOpcode(opcode);
    if (!info.sample || info.name == nullptr) {
        throw std::runtime_error("opcode is not an image sample");
    }
    return info.name;
}

RdnaImageAddressComponent GetRdnaImageAddressComponentLayout(std::uint32_t flags, std::uint32_t component) {
    validateFlags(flags);
    if (component > MaxRdnaImageNsaAddressComponents) {
        throw std::runtime_error("image address component out of range");
    }
    std::uint32_t offset = 0;
    for (std::uint32_t index = 0; index <= component; ++index) {
        const auto shape = componentShape(flags, index);
        if (shape.startsDword) {
            offset = (offset + 31u) & ~31u;
        }
        if (index == component) {
            return {offset, shape.width};
        }
        offset += shape.width;
    }
    throw std::runtime_error("invalid image address component");
}

std::uint32_t GetRdnaImageAddressDwordCount(std::uint32_t flags, std::uint32_t components) {
    validateFlags(flags);
    if (components == 0u || components > MaxRdnaImageNsaAddressComponents + 1u) {
        throw std::runtime_error("invalid image address component count");
    }
    const auto last = GetRdnaImageAddressComponentLayout(flags, components - 1u);
    return (last.bitOffset + last.bitWidth + 31u) / 32u;
}

RdnaInstruction DecodeRdnaImageOp(std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    if (wordIndex > std::numeric_limits<std::uint32_t>::max() / 4u) {
        throw std::runtime_error("MIMG program counter overflow");
    }
    return DecodeRdnaMimg(wordIndex * 4u, code, wordIndex);
}

RdnaInstruction DecodeRdnaMimg(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    const std::size_t index = wordIndex;
    if (index >= code.size() || code.size() - index < 2u) {
        throw std::runtime_error("truncated MIMG instruction");
    }
    const auto word0 = code[index];
    const auto word1 = code[index + 1u];
    if ((word0 >> 26u) != 0x3Cu) {
        throw std::runtime_error("instruction is not MIMG");
    }
    const auto opcode = ((word0 >> 18u) & 0x7Fu) | ((word0 & 1u) << 7u);
    const auto& info = lookupOpcode(opcode);
    const auto reservedWord0 = info.sample || info.gather ? 0x000350C0u : 0x000340C0u;
    if ((word0 & reservedWord0) != 0u || (word1 & 0x3C000000u) != 0u) {
        char message[96];
        std::snprintf(message, sizeof(message), "unsupported or reserved MIMG control bits (words %08x %08x)", word0, word1);
        throw std::runtime_error(message);
    }
    const auto nsa = (word0 >> 1u) & 3u;
    const auto wordCount = 2u + nsa;
    if (code.size() - index < wordCount) {
        throw std::runtime_error("truncated MIMG NSA payload");
    }
    if (programCounter % 4u != 0u || programCounter > std::numeric_limits<std::uint32_t>::max() - (wordCount * 4u - 1u)) {
        throw std::runtime_error("invalid MIMG program counter");
    }
    const bool a16 = (word1 & 0x40000000u) != 0u;
    const bool d16 = (word1 & 0x80000000u) != 0u;
    const auto dimension = decodeDimension((word0 >> 3u) & 7u);
    const bool multisampled = dimension == RdnaImageDimension::Dim2DMsaa || dimension == RdnaImageDimension::Dim2DMsaaArray;
    if (multisampled && (info.sample || info.gather || opcode == 0x60u || opcode == 1u || opcode == 9u)) {
        throw std::runtime_error("unsupported multisampled MIMG operation");
    }
    auto flags = info.flags | (a16 ? RdnaImageSampleFlagA16 : 0u);
    if ((info.flags & RdnaImageSampleFlagDerivative) != 0u) {
        flags |= gradientCount(dimension) << RdnaImageSampleGradientCountShift;
    }
    validateFlags(flags);
    const auto dmask = (word0 >> 8u) & 15u;
    const bool compareSwap = info.opcode == RdnaOpcode::ImageAtomicCmpswap;
    if (dmask == 0u || (compareSwap ? dmask != 3u : (info.gather || info.atomic) && !std::has_single_bit(dmask))) {
        throw std::runtime_error("invalid MIMG data mask");
    }
    if (d16 && !(info.sample || info.gather || opcode == 0u || opcode == 1u || opcode == 8u || opcode == 9u)) {
        throw std::runtime_error("MIMG opcode does not support D16");
    }
    const bool rayQuery = info.opcode == RdnaOpcode::ImageBvhIntersectRay;
    std::uint32_t components = rayQuery ? (a16 ? 8u : 11u) : opcode == 0x0Eu ? 1u : coordinateCount(dimension);
    if (opcode == 1u || opcode == 9u) {
        ++components;
    }
    if (info.sample || info.gather) {
        components += std::popcount(info.flags & (RdnaImageSampleFlagOffset | RdnaImageSampleFlagCompare | RdnaImageSampleFlagBias | RdnaImageSampleFlagLod | RdnaImageSampleFlagLodClamp));
        if ((flags & RdnaImageSampleFlagDerivative) != 0u) {
            components += gradientCount(dimension) * 2u;
        }
    }
    const auto addressFlags = rayQuery ? info.flags : flags;
    const auto addressDwords = GetRdnaImageAddressDwordCount(addressFlags, components);
    const auto vaddr = word1 & 255u;
    const auto vdata = (word1 >> 8u) & 255u;
    if (nsa != 0u && addressDwords > 1u + nsa * 4u) {
        throw std::runtime_error("insufficient MIMG NSA registers");
    }
    if (nsa == 0u && addressDwords > 256u - vaddr) {
        throw std::runtime_error("MIMG address register range overflow");
    }
    const auto dataComponents = info.gather ? 4u : static_cast<std::uint32_t>(std::popcount(dmask));
    const auto dataDwords = d16 ? (dataComponents + 1u) / 2u : dataComponents;
    if (dataDwords > 256u - vdata) {
        throw std::runtime_error("MIMG data register range overflow");
    }
    const auto resource = ((word1 >> 16u) & 31u) * 4u;
    const auto sampler = ((word1 >> 21u) & 31u) * 4u;
    const bool r128 = (word0 & 0x8000u) != 0u;
    if (resource + (r128 ? 4u : 8u) > 106u || ((info.sample || info.gather || opcode == 0x60u) && sampler + 4u > 106u)) {
        throw std::runtime_error("MIMG descriptor register range overflow");
    }
    RdnaInstruction instruction{};
    instruction.op = info.opcode;
    instruction.family = RdnaInstructionFamily::MIMG;
    instruction.programCounter = programCounter;
    instruction.wordCount = wordCount;
    instruction.opcodeId = opcode;
    instruction.imageOpcodeId = opcode;
    instruction.imageDmask = dmask;
    instruction.dataComponents = dataComponents;
    instruction.dataBits = d16 ? 16u : 32u;
    instruction.dataDwordCount = dataDwords;
    instruction.glc = (word0 & 0x2000u) != 0u;
    instruction.slc = (word0 & 0x02000000u) != 0u;
    instruction.imageGlc = instruction.glc;
    instruction.imageSlc = instruction.slc;
    instruction.imageA16 = a16;
    instruction.imageD16 = d16;
    instruction.imageR128 = r128;
    instruction.imageDimension = dimension;
    instruction.imageSampleFlags = addressFlags;
    instruction.imageAddressComponents = components;
    instruction.imageNsaDwordCount = nsa;
    instruction.destination = vectorRegister(vdata);
    instruction.source0 = vectorRegister(vaddr);
    instruction.source1 = scalarRegister(resource);
    instruction.source2 = scalarRegister(sampler);
    instruction.sourceCount = 3;
    for (std::uint32_t i = 0; i < wordCount; ++i) {
        instruction.rawWords[i] = code[index + i];
    }
    for (std::uint32_t i = 0; i < nsa * 4u; ++i) {
        instruction.imageNsaVectorRegisters[i] = (code[index + 2u + i / 4u] >> ((i % 4u) * 8u)) & 255u;
    }
    return instruction;
}

}
