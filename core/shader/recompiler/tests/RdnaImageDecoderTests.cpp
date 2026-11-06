#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace ShaderRecompiler;

constexpr std::uint32_t Dim2D = 1;
constexpr std::uint32_t Dim1D = 0;

void require(const bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void requireFailure(const std::function<void()>& operation, const char* message) {
    try {
        operation();
    } catch (const std::runtime_error&) {
        return;
    }
    throw std::runtime_error(message);
}

std::vector<std::uint32_t> encode(const std::uint32_t opcode, const std::uint32_t dmask, const std::uint32_t dimension) {
    const std::uint32_t word0 = (0x3Cu << 26u) | ((opcode & 0x7Fu) << 18u) | ((opcode >> 7u) & 1u) | (dmask << 8u) | (dimension << 3u);
    const std::uint32_t word1 = 4u | (8u << 8u) | (1u << 16u) | (2u << 21u);
    return {word0, word1};
}

RdnaInstruction decode(const std::uint32_t opcode, const std::uint32_t dmask = 1u, const std::uint32_t dimension = Dim2D) {
    const auto code = encode(opcode, dmask, dimension);
    return DecodeRdnaMimg(0, code, 0);
}

void sampleWithOffset() {
    const struct {
        std::uint32_t opcode;
        const char* name;
        RdnaOpcode decodedOpcode;
        std::uint32_t flags;
        std::uint32_t components;
    } cases[] = {
        {0x3au, "image_sample_c_d_o", RdnaOpcode::ImageSampleCDO, RdnaImageSampleFlagCompare | RdnaImageSampleFlagDerivative | RdnaImageSampleFlagOffset, 8},
        {0x3cu, "image_sample_c_l_o", RdnaOpcode::ImageSampleCLO, RdnaImageSampleFlagCompare | RdnaImageSampleFlagLod | RdnaImageSampleFlagOffset, 5},
        {0x3du, "image_sample_c_b_o", RdnaOpcode::ImageSampleCBO, RdnaImageSampleFlagCompare | RdnaImageSampleFlagBias | RdnaImageSampleFlagOffset, 5},
        {0x3fu, "image_sample_c_lz_o", RdnaOpcode::ImageSampleCLzO, RdnaImageSampleFlagCompare | RdnaImageSampleFlagLevelZero | RdnaImageSampleFlagOffset, 4},
    };
    for (const auto& entry : cases) {
        const auto instruction = decode(entry.opcode);
        require(instruction.op == entry.decodedOpcode, "shadow sample with offset decoded to the wrong opcode");
        const auto gradients = (entry.flags & RdnaImageSampleFlagDerivative) != 0 ? 2u << RdnaImageSampleGradientCountShift : 0u;
        require(instruction.imageSampleFlags == (entry.flags | gradients), "shadow sample with offset decoded the wrong address flags");
        require(instruction.imageAddressComponents == entry.components, "shadow sample with offset decoded the wrong address component count");
        require(std::string(GetRdnaImageSampleOpcodeName(entry.opcode)) == entry.name, "shadow sample with offset reported the wrong name");
        require(instruction.family == RdnaInstructionFamily::MIMG && instruction.wordCount == 2, "shadow sample with offset decoded the wrong instruction shape");
    }
}

void gather() {
    const auto plain = decode(0x40u);
    require(plain.op == RdnaOpcode::ImageGather4, "IMAGE_GATHER4 decoded to the wrong opcode");
    require(plain.imageSampleFlags == 0u, "IMAGE_GATHER4 must not request an address modifier");
    require(plain.imageAddressComponents == 2u, "IMAGE_GATHER4 decoded the wrong address component count");
    require(plain.dataComponents == 4u && plain.dataDwordCount == 4u, "IMAGE_GATHER4 must return four texels");

    const auto offset = decode(0x50u);
    require(offset.op == RdnaOpcode::ImageGather4O, "IMAGE_GATHER4_O decoded to the wrong opcode");
    require(offset.imageSampleFlags == RdnaImageSampleFlagOffset, "IMAGE_GATHER4_O decoded the wrong address flags");
    require(offset.imageAddressComponents == 3u, "IMAGE_GATHER4_O decoded the wrong address component count");
    require(offset.dataComponents == 4u, "IMAGE_GATHER4_O must return four texels");

    require(IsImageOpcode(RdnaOpcode::ImageGather4) && IsImageOpcode(RdnaOpcode::ImageGather4O), "new gather opcodes are not classified as image opcodes");
    requireFailure([] { (void)decode(0x40u, 3u); }, "gather with a multi-bit data mask was accepted");
    requireFailure([] { (void)decode(0x40u, 0u); }, "gather with an empty data mask was accepted");
}

void atomics() {
    const struct {
        std::uint32_t opcode;
        RdnaOpcode expected;
    } cases[] = {
        {0x12u, RdnaOpcode::ImageAtomicSub},
        {0x14u, RdnaOpcode::ImageAtomicSmin},
        {0x16u, RdnaOpcode::ImageAtomicSmax},
    };
    for (const auto& entry : cases) {
        const auto instruction = decode(entry.opcode);
        require(instruction.op == entry.expected, "signed image atomic decoded to the wrong opcode");
        require(instruction.imageSampleFlags == 0u, "image atomic must not request an address modifier");
        require(instruction.dataComponents == 1u, "image atomic operates on a single dword");
        require(IsImageOpcode(entry.expected), "signed image atomic is not classified as an image opcode");
    }
    const auto compareSwap = decode(0x10u, 3u);
    require(compareSwap.op == RdnaOpcode::ImageAtomicCmpswap, "IMAGE_ATOMIC_CMPSWAP decoded to the wrong opcode");
    require(compareSwap.dataComponents == 2u && compareSwap.dataDwordCount == 2u, "IMAGE_ATOMIC_CMPSWAP takes a value and a comparator");
    require(IsImageOpcode(RdnaOpcode::ImageAtomicCmpswap), "IMAGE_ATOMIC_CMPSWAP is not classified as an image opcode");
    requireFailure([] { (void)decode(0x10u, 1u); }, "IMAGE_ATOMIC_CMPSWAP with a single data dword was accepted");
    const auto compareSwap64 = decode(0x10u, 0xfu);
    require(compareSwap64.dataBits == 64u && compareSwap64.dataDwordCount == 4u, "64-bit IMAGE_ATOMIC_CMPSWAP requires two qwords");
    const auto add64 = decode(0x11u, 3u);
    require(add64.dataBits == 64u && add64.dataDwordCount == 2u, "64-bit IMAGE_ATOMIC_ADD requires one qword");
    require(decode(0x11u).op == RdnaOpcode::ImageAtomicAdd, "IMAGE_ATOMIC_ADD regressed");
    require(decode(0x15u).op == RdnaOpcode::ImageAtomicUmin, "IMAGE_ATOMIC_UMIN regressed");
    requireFailure([] { (void)decode(0x13u); }, "reserved atomic opcode 0x13 was accepted");
    require(decode(0x1bu).op == RdnaOpcode::ImageAtomicInc, "IMAGE_ATOMIC_INC decoded to the wrong opcode");
}

void unchanged() {
    require(decode(0x47u).op == RdnaOpcode::ImageGather4Lz, "IMAGE_GATHER4_LZ regressed");
    require(decode(0x57u).op == RdnaOpcode::ImageGather4LzO, "IMAGE_GATHER4_LZ_O regressed");
    const auto sample = decode(0x20u);
    require(sample.op == RdnaOpcode::ImageSample && sample.imageAddressComponents == 2u, "IMAGE_SAMPLE regressed");
    const auto oneDimensional = decode(0x3fu, 1u, Dim1D);
    require(oneDimensional.imageAddressComponents == 3u, "1D shadow sample with offset decoded the wrong address component count");
    require(decode(0x39u).op == RdnaOpcode::ImageSampleCClO, "LOD-clamped shadow sample with offset decoded the wrong opcode");
}

}

int main() {
    try {
        sampleWithOffset();
        gather();
        atomics();
        unchanged();
        std::cout << "RDNA image decoder tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
