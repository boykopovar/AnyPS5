#include "RdnaDecoder/RdnaVectorOpDecoder.hpp"
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <RdnaDecoder/RdnaInstructionDecoder.hpp>

namespace ShaderRecompiler {

namespace {

std::uint32_t toProgramCounter(std::uint32_t wordIndex) {
    if (wordIndex > std::numeric_limits<std::uint32_t>::max() / 4u) {
        throw std::invalid_argument("vector instruction program counter overflow");
    }
    return wordIndex * 4u;
}

struct VectorOpcodeInfo {
    std::uint32_t encoding;
    RdnaOpcode opcode;
};

enum class Vop2SdwaProfile {
    None,
    Float32,
    Float16,
    PackedFloat16,
    IntegerFullDestination,
    IntegerPartialDestination,
    ReverseRightShift,
    ReverseLogicalLeft,
    Bitwise,
};

struct Vop2OpcodeInfo {
    std::uint32_t encoding;
    RdnaOpcode opcode;
    Vop2SdwaProfile sdwaProfile = Vop2SdwaProfile::None;
};

struct VopcOpcodeInfo {
    std::uint32_t encoding;
    RdnaOpcode opcode;
    bool supportsDpp = true;
};

constexpr Vop2OpcodeInfo vop2Opcodes[] = {
    {0x01u, RdnaOpcode::VCndmaskB32, Vop2SdwaProfile::IntegerPartialDestination},
    {0x02u, RdnaOpcode::VDot2cF32F16},
    {0x03u, RdnaOpcode::VAddF32, Vop2SdwaProfile::Float32},
    {0x04u, RdnaOpcode::VSubF32, Vop2SdwaProfile::Float32},
    {0x05u, RdnaOpcode::VSubrevF32},
    {0x08u, RdnaOpcode::VMulF32, Vop2SdwaProfile::Float32},
    {0x09u, RdnaOpcode::VMulI32I24, Vop2SdwaProfile::IntegerFullDestination},
    {0x0au, RdnaOpcode::VMulHiI32I24, Vop2SdwaProfile::IntegerFullDestination},
    {0x0bu, RdnaOpcode::VMulU32U24, Vop2SdwaProfile::IntegerFullDestination},
    {0x0cu, RdnaOpcode::VMulHiU32U24, Vop2SdwaProfile::IntegerFullDestination},
    {0x0fu, RdnaOpcode::VMinF32},
    {0x10u, RdnaOpcode::VMaxF32},
    {0x11u, RdnaOpcode::VMinI32},
    {0x12u, RdnaOpcode::VMaxI32},
    {0x13u, RdnaOpcode::VMinU32, Vop2SdwaProfile::IntegerPartialDestination},
    {0x14u, RdnaOpcode::VMaxU32, Vop2SdwaProfile::IntegerFullDestination},
    {0x15u, RdnaOpcode::VLshrB32},
    {0x16u, RdnaOpcode::VLshrrevB32, Vop2SdwaProfile::ReverseRightShift},
    {0x17u, RdnaOpcode::VAshrI32},
    {0x18u, RdnaOpcode::VAshrrevI32, Vop2SdwaProfile::ReverseRightShift},
    {0x19u, RdnaOpcode::VLshlB32},
    {0x1au, RdnaOpcode::VLshlrevB32, Vop2SdwaProfile::ReverseLogicalLeft},
    {0x1bu, RdnaOpcode::VAndB32, Vop2SdwaProfile::Bitwise},
    {0x1cu, RdnaOpcode::VOrB32, Vop2SdwaProfile::Bitwise},
    {0x1du, RdnaOpcode::VXorB32, Vop2SdwaProfile::Bitwise},
    {0x1eu, RdnaOpcode::VXnorB32, Vop2SdwaProfile::Bitwise},
    {0x1fu, RdnaOpcode::VMacF32},
    {0x20u, RdnaOpcode::VMadmkF32},
    {0x21u, RdnaOpcode::VMadakF32},
    {0x22u, RdnaOpcode::VBcntU32B32},
    {0x23u, RdnaOpcode::VMbcntLoU32B32},
    {0x24u, RdnaOpcode::VMbcntHiU32B32},
    {0x25u, RdnaOpcode::VAddNcU32, Vop2SdwaProfile::IntegerPartialDestination},
    {0x28u, RdnaOpcode::VAddcU32, Vop2SdwaProfile::IntegerFullDestination},
    {0x29u, RdnaOpcode::VSubCoCiU32},
    {0x2au, RdnaOpcode::VSubrevCoCiU32},
    {0x26u, RdnaOpcode::VSubNcU32, Vop2SdwaProfile::IntegerPartialDestination},
    {0x27u, RdnaOpcode::VSubrevNcU32, Vop2SdwaProfile::IntegerFullDestination},
    {0x2bu, RdnaOpcode::VMacF32},
    {0x2cu, RdnaOpcode::VMadmkF32},
    {0x2du, RdnaOpcode::VMadakF32},
    {0x2fu, RdnaOpcode::VCvtPkrtzF16F32, Vop2SdwaProfile::PackedFloat16},
    {0x32u, RdnaOpcode::VAddF16, Vop2SdwaProfile::Float16},
    {0x33u, RdnaOpcode::VSubF16, Vop2SdwaProfile::Float16},
    {0x34u, RdnaOpcode::VSubrevF16, Vop2SdwaProfile::Float16},
    {0x35u, RdnaOpcode::VMulF16, Vop2SdwaProfile::Float16},
    {0x36u, RdnaOpcode::VFmacF16},
    {0x37u, RdnaOpcode::VFmamkF16},
    {0x38u, RdnaOpcode::VFmaakF16},
    {0x39u, RdnaOpcode::VMaxF16, Vop2SdwaProfile::Float16},
    {0x3au, RdnaOpcode::VMinF16, Vop2SdwaProfile::Float16},
    {0x3cu, RdnaOpcode::VPkFmacF16},
    {0x06u, RdnaOpcode::VMacLegacyF32},
    {0x07u, RdnaOpcode::VMulLegacyF32},
    {0x3bu, RdnaOpcode::VLdexpF16},
};

constexpr VectorOpcodeInfo vop1Opcodes[] = {
    {0x00u, RdnaOpcode::VNop},
    {0x01u, RdnaOpcode::VMovB32},
    {0x02u, RdnaOpcode::VReadfirstlaneB32},
    {0x05u, RdnaOpcode::VCvtF32I32},
    {0x06u, RdnaOpcode::VCvtF32U32},
    {0x07u, RdnaOpcode::VCvtU32F32},
    {0x08u, RdnaOpcode::VCvtI32F32},
    {0x0au, RdnaOpcode::VCvtF16F32},
    {0x0bu, RdnaOpcode::VCvtF32F16},
    {0x0cu, RdnaOpcode::VCvtRpiI32F32},
    {0x0du, RdnaOpcode::VCvtFlrI32F32},
    {0x0eu, RdnaOpcode::VCvtOffF32I4},
    {0x11u, RdnaOpcode::VCvtF32Ubyte0},
    {0x12u, RdnaOpcode::VCvtF32Ubyte1},
    {0x13u, RdnaOpcode::VCvtF32Ubyte2},
    {0x14u, RdnaOpcode::VCvtF32Ubyte3},
    {0x1bu, RdnaOpcode::VPipeflush},
    {0x2au, RdnaOpcode::VRcpF32},
    {0x20u, RdnaOpcode::VFractF32},
    {0x21u, RdnaOpcode::VTruncF32},
    {0x22u, RdnaOpcode::VCeilF32},
    {0x23u, RdnaOpcode::VRndneF32},
    {0x24u, RdnaOpcode::VFloorF32},
    {0x03u, RdnaOpcode::VCvtI32F64},
    {0x04u, RdnaOpcode::VCvtF64I32},
    {0x0fu, RdnaOpcode::VCvtF32F64},
    {0x10u, RdnaOpcode::VCvtF64F32},
    {0x15u, RdnaOpcode::VCvtU32F64},
    {0x16u, RdnaOpcode::VCvtF64U32},
    {0x17u, RdnaOpcode::VTruncF64},
    {0x18u, RdnaOpcode::VCeilF64},
    {0x19u, RdnaOpcode::VRndneF64},
    {0x1au, RdnaOpcode::VFloorF64},
    {0x3cu, RdnaOpcode::VFrexpExpI32F64},
    {0x3du, RdnaOpcode::VFrexpMantF64},
    {0x3eu, RdnaOpcode::VFractF64},
    {0x2fu, RdnaOpcode::VRcpF64},
    {0x31u, RdnaOpcode::VRsqF64},
    {0x34u, RdnaOpcode::VSqrtF64},
    {0x25u, RdnaOpcode::VExpF32},
    {0x27u, RdnaOpcode::VLogF32},
    {0x2bu, RdnaOpcode::VRcpIflagF32},
    {0x2eu, RdnaOpcode::VRsqF32},
    {0x33u, RdnaOpcode::VSqrtF32},
    {0x35u, RdnaOpcode::VSinF32},
    {0x36u, RdnaOpcode::VCosF32},
    {0x37u, RdnaOpcode::VNotB32},
    {0x38u, RdnaOpcode::VBfrevB32},
    {0x39u, RdnaOpcode::VFfbhU32},
    {0x3au, RdnaOpcode::VFfblB32},
    {0x3bu, RdnaOpcode::VFfbhI32},
    {0x3fu, RdnaOpcode::VFrexpExpI32F32},
    {0x40u, RdnaOpcode::VFrexpMantF32},
    {0x41u, RdnaOpcode::VClrexcp},
    {0x42u, RdnaOpcode::VMovreldB32},
    {0x43u, RdnaOpcode::VMovrelsB32},
    {0x50u, RdnaOpcode::VCvtF16U16},
    {0x51u, RdnaOpcode::VCvtF16I16},
    {0x52u, RdnaOpcode::VCvtU16F16},
    {0x53u, RdnaOpcode::VCvtI16F16},
    {0x54u, RdnaOpcode::VRcpF16},
    {0x55u, RdnaOpcode::VSqrtF16},
    {0x56u, RdnaOpcode::VRsqF16},
    {0x57u, RdnaOpcode::VLogF16},
    {0x58u, RdnaOpcode::VExpF16},
    {0x5bu, RdnaOpcode::VFloorF16},
    {0x5cu, RdnaOpcode::VCeilF16},
    {0x5du, RdnaOpcode::VTruncF16},
    {0x5eu, RdnaOpcode::VRndneF16},
    {0x5fu, RdnaOpcode::VFractF16},
    {0x60u, RdnaOpcode::VSinF16},
    {0x61u, RdnaOpcode::VCosF16},
    {0x59u, RdnaOpcode::VFrexpMantF16},
    {0x5au, RdnaOpcode::VFrexpExpI16F16},
    {0x62u, RdnaOpcode::VSatPkU8I16},
    {0x63u, RdnaOpcode::VCvtNormI16F16},
    {0x64u, RdnaOpcode::VCvtNormU16F16},
};

constexpr VectorOpcodeInfo vop3EncodedVop1Opcodes[] = {
    {0x00u, RdnaOpcode::VNop},
    {0x01u, RdnaOpcode::VMovB32},
    {0x02u, RdnaOpcode::VReadfirstlaneB32},
    {0x05u, RdnaOpcode::VCvtF32I32},
    {0x06u, RdnaOpcode::VCvtF32U32},
    {0x07u, RdnaOpcode::VCvtU32F32},
    {0x08u, RdnaOpcode::VCvtI32F32},
    {0x0au, RdnaOpcode::VCvtF16F32},
    {0x0bu, RdnaOpcode::VCvtF32F16},
    {0x0cu, RdnaOpcode::VCvtRpiI32F32},
    {0x0du, RdnaOpcode::VCvtFlrI32F32},
    {0x0eu, RdnaOpcode::VCvtOffF32I4},
    {0x11u, RdnaOpcode::VCvtF32Ubyte0},
    {0x12u, RdnaOpcode::VCvtF32Ubyte1},
    {0x13u, RdnaOpcode::VCvtF32Ubyte2},
    {0x14u, RdnaOpcode::VCvtF32Ubyte3},
    {0x1bu, RdnaOpcode::VPipeflush},
    {0x2au, RdnaOpcode::VRcpF32},
    {0x20u, RdnaOpcode::VFractF32},
    {0x21u, RdnaOpcode::VTruncF32},
    {0x22u, RdnaOpcode::VCeilF32},
    {0x23u, RdnaOpcode::VRndneF32},
    {0x24u, RdnaOpcode::VFloorF32},
    {0x03u, RdnaOpcode::VCvtI32F64},
    {0x04u, RdnaOpcode::VCvtF64I32},
    {0x0fu, RdnaOpcode::VCvtF32F64},
    {0x10u, RdnaOpcode::VCvtF64F32},
    {0x15u, RdnaOpcode::VCvtU32F64},
    {0x16u, RdnaOpcode::VCvtF64U32},
    {0x17u, RdnaOpcode::VTruncF64},
    {0x18u, RdnaOpcode::VCeilF64},
    {0x19u, RdnaOpcode::VRndneF64},
    {0x1au, RdnaOpcode::VFloorF64},
    {0x3cu, RdnaOpcode::VFrexpExpI32F64},
    {0x3du, RdnaOpcode::VFrexpMantF64},
    {0x3eu, RdnaOpcode::VFractF64},
    {0x2fu, RdnaOpcode::VRcpF64},
    {0x31u, RdnaOpcode::VRsqF64},
    {0x34u, RdnaOpcode::VSqrtF64},
    {0x25u, RdnaOpcode::VExpF32},
    {0x27u, RdnaOpcode::VLogF32},
    {0x2bu, RdnaOpcode::VRcpIflagF32},
    {0x2eu, RdnaOpcode::VRsqF32},
    {0x33u, RdnaOpcode::VSqrtF32},
    {0x35u, RdnaOpcode::VSinF32},
    {0x36u, RdnaOpcode::VCosF32},
    {0x37u, RdnaOpcode::VNotB32},
    {0x38u, RdnaOpcode::VBfrevB32},
    {0x39u, RdnaOpcode::VFfbhU32},
    {0x3au, RdnaOpcode::VFfblB32},
    {0x3bu, RdnaOpcode::VFfbhI32},
    {0x3fu, RdnaOpcode::VFrexpExpI32F32},
    {0x40u, RdnaOpcode::VFrexpMantF32},
    {0x41u, RdnaOpcode::VClrexcp},
    {0x42u, RdnaOpcode::VMovreldB32},
    {0x43u, RdnaOpcode::VMovrelsB32},
    {0x50u, RdnaOpcode::VCvtF16U16},
    {0x51u, RdnaOpcode::VCvtF16I16},
    {0x52u, RdnaOpcode::VCvtU16F16},
    {0x53u, RdnaOpcode::VCvtI16F16},
    {0x54u, RdnaOpcode::VRcpF16},
    {0x55u, RdnaOpcode::VSqrtF16},
    {0x56u, RdnaOpcode::VRsqF16},
    {0x57u, RdnaOpcode::VLogF16},
    {0x58u, RdnaOpcode::VExpF16},
    {0x5bu, RdnaOpcode::VFloorF16},
    {0x5cu, RdnaOpcode::VCeilF16},
    {0x5du, RdnaOpcode::VTruncF16},
    {0x5eu, RdnaOpcode::VRndneF16},
    {0x5fu, RdnaOpcode::VFractF16},
    {0x60u, RdnaOpcode::VSinF16},
    {0x61u, RdnaOpcode::VCosF16},
    {0x59u, RdnaOpcode::VFrexpMantF16},
    {0x5au, RdnaOpcode::VFrexpExpI16F16},
    {0x62u, RdnaOpcode::VSatPkU8I16},
    {0x63u, RdnaOpcode::VCvtNormI16F16},
    {0x64u, RdnaOpcode::VCvtNormU16F16},
};

constexpr VopcOpcodeInfo vopcOpcodes[] = {
    {0x00u, RdnaOpcode::VCmpFF32},
    {0x01u, RdnaOpcode::VCmpLtF32},
    {0x02u, RdnaOpcode::VCmpEqF32},
    {0x03u, RdnaOpcode::VCmpLeF32},
    {0x04u, RdnaOpcode::VCmpGtF32},
    {0x05u, RdnaOpcode::VCmpLgF32},
    {0x06u, RdnaOpcode::VCmpGeF32},
    {0x07u, RdnaOpcode::VCmpOF32},
    {0x08u, RdnaOpcode::VCmpUF32},
    {0x09u, RdnaOpcode::VCmpNgeF32},
    {0x0au, RdnaOpcode::VCmpNlgF32},
    {0x0bu, RdnaOpcode::VCmpNgtF32},
    {0x0cu, RdnaOpcode::VCmpNleF32},
    {0x0du, RdnaOpcode::VCmpNeqF32},
    {0x0eu, RdnaOpcode::VCmpNltF32},
    {0x0fu, RdnaOpcode::VCmpTruF32},
    {0x11u, RdnaOpcode::VCmpxLtF32},
    {0x12u, RdnaOpcode::VCmpxEqF32},
    {0x13u, RdnaOpcode::VCmpxLeF32},
    {0x14u, RdnaOpcode::VCmpxGtF32},
    {0x15u, RdnaOpcode::VCmpxLgF32},
    {0x16u, RdnaOpcode::VCmpxGeF32},
    {0x19u, RdnaOpcode::VCmpxNgeF32},
    {0x1au, RdnaOpcode::VCmpxNlgF32},
    {0x1bu, RdnaOpcode::VCmpxNgtF32},
    {0x1cu, RdnaOpcode::VCmpxNleF32},
    {0x1du, RdnaOpcode::VCmpxNeqF32},
    {0x1eu, RdnaOpcode::VCmpxNltF32},
    {0x80u, RdnaOpcode::VCmpFI32},
    {0x81u, RdnaOpcode::VCmpLtI32},
    {0x82u, RdnaOpcode::VCmpEqI32},
    {0x83u, RdnaOpcode::VCmpLeI32},
    {0x84u, RdnaOpcode::VCmpGtI32},
    {0x85u, RdnaOpcode::VCmpNeI32},
    {0x86u, RdnaOpcode::VCmpGeI32},
    {0x87u, RdnaOpcode::VCmpTI32},
    {0x88u, RdnaOpcode::VCmpClassF32},
    {0x8fu, RdnaOpcode::VCmpClassF16},
    {0x89u, RdnaOpcode::VCmpLtI16},
    {0x8au, RdnaOpcode::VCmpEqI16},
    {0x8bu, RdnaOpcode::VCmpLeI16},
    {0x8cu, RdnaOpcode::VCmpGtI16},
    {0x8du, RdnaOpcode::VCmpNeI16},
    {0x8eu, RdnaOpcode::VCmpGeI16},
    {0x91u, RdnaOpcode::VCmpxLtI32},
    {0x92u, RdnaOpcode::VCmpxEqI32},
    {0x93u, RdnaOpcode::VCmpxLeI32},
    {0x94u, RdnaOpcode::VCmpxGtI32},
    {0x95u, RdnaOpcode::VCmpxNeI32},
    {0x96u, RdnaOpcode::VCmpxGeI32},
    {0x98u, RdnaOpcode::VCmpxClassF32},
    {0x9fu, RdnaOpcode::VCmpxClassF16},
    {0x99u, RdnaOpcode::VCmpxLtI16},
    {0x9au, RdnaOpcode::VCmpxEqI16},
    {0x9bu, RdnaOpcode::VCmpxLeI16},
    {0x9cu, RdnaOpcode::VCmpxGtI16},
    {0x9du, RdnaOpcode::VCmpxNeI16},
    {0x9eu, RdnaOpcode::VCmpxGeI16},
    {0xa9u, RdnaOpcode::VCmpLtU16},
    {0xaau, RdnaOpcode::VCmpEqU16},
    {0xabu, RdnaOpcode::VCmpLeU16},
    {0xacu, RdnaOpcode::VCmpGtU16},
    {0xadu, RdnaOpcode::VCmpNeU16},
    {0xaeu, RdnaOpcode::VCmpGeU16},
    {0xb9u, RdnaOpcode::VCmpxLtU16, false},
    {0xbau, RdnaOpcode::VCmpxEqU16},
    {0xbbu, RdnaOpcode::VCmpxLeU16},
    {0xbcu, RdnaOpcode::VCmpxGtU16},
    {0xbdu, RdnaOpcode::VCmpxNeU16},
    {0xbeu, RdnaOpcode::VCmpxGeU16},
    {0xc0u, RdnaOpcode::VCmpFU32},
    {0xc1u, RdnaOpcode::VCmpLtU32},
    {0xc2u, RdnaOpcode::VCmpEqU32},
    {0xc3u, RdnaOpcode::VCmpLeU32},
    {0xc4u, RdnaOpcode::VCmpGtU32},
    {0xc5u, RdnaOpcode::VCmpNeU32},
    {0xc6u, RdnaOpcode::VCmpGeU32},
    {0xc7u, RdnaOpcode::VCmpTU32},
    {0xa2u, RdnaOpcode::VCmpEqI64, false},
    {0xb5u, RdnaOpcode::VCmpxNeI64, false},
    {0xd1u, RdnaOpcode::VCmpxLtU32},
    {0xd2u, RdnaOpcode::VCmpxEqU32},
    {0xd3u, RdnaOpcode::VCmpxLeU32},
    {0xd4u, RdnaOpcode::VCmpxGtU32},
    {0xd5u, RdnaOpcode::VCmpxNeU32},
    {0xd6u, RdnaOpcode::VCmpxGeU32},
    {0xe1u, RdnaOpcode::VCmpLtU64, false},
    {0xe2u, RdnaOpcode::VCmpEqU64, false},
    {0xe4u, RdnaOpcode::VCmpGtU64, false},
    {0xe5u, RdnaOpcode::VCmpNeU64, false},
    {0xf5u, RdnaOpcode::VCmpxNeU64, false},
    {0xc9u, RdnaOpcode::VCmpLtF16},
    {0xcau, RdnaOpcode::VCmpEqF16},
    {0xcbu, RdnaOpcode::VCmpLeF16},
    {0xccu, RdnaOpcode::VCmpGtF16},
    {0xcdu, RdnaOpcode::VCmpLgF16},
    {0xceu, RdnaOpcode::VCmpGeF16},
    {0xedu, RdnaOpcode::VCmpNeqF16},
    {0xd9u, RdnaOpcode::VCmpxLtF16},
    {0xdau, RdnaOpcode::VCmpxEqF16},
    {0xdbu, RdnaOpcode::VCmpxLeF16},
    {0xdcu, RdnaOpcode::VCmpxGtF16},
    {0xdeu, RdnaOpcode::VCmpxGeF16},
    {0xfbu, RdnaOpcode::VCmpxNgtF16},
    {0xfdu, RdnaOpcode::VCmpxNeqF16},
    {0xfeu, RdnaOpcode::VCmpxNltF16},
    {0x10u, RdnaOpcode::VCmpxFF32},
    {0x17u, RdnaOpcode::VCmpxOF32},
    {0x18u, RdnaOpcode::VCmpxUF32},
    {0x1fu, RdnaOpcode::VCmpxTruF32},
    {0x90u, RdnaOpcode::VCmpxFI32},
    {0x97u, RdnaOpcode::VCmpxTI32},
    {0xd0u, RdnaOpcode::VCmpxFU32},
    {0xd7u, RdnaOpcode::VCmpxTU32},
    {0xa0u, RdnaOpcode::VCmpFI64, false},
    {0xa1u, RdnaOpcode::VCmpLtI64, false},
    {0xa3u, RdnaOpcode::VCmpLeI64, false},
    {0xa4u, RdnaOpcode::VCmpGtI64, false},
    {0xa5u, RdnaOpcode::VCmpNeI64, false},
    {0xa6u, RdnaOpcode::VCmpGeI64, false},
    {0xa7u, RdnaOpcode::VCmpTI64, false},
    {0xb0u, RdnaOpcode::VCmpxFI64, false},
    {0xb1u, RdnaOpcode::VCmpxLtI64, false},
    {0xb2u, RdnaOpcode::VCmpxEqI64, false},
    {0xb3u, RdnaOpcode::VCmpxLeI64, false},
    {0xb4u, RdnaOpcode::VCmpxGtI64, false},
    {0xb6u, RdnaOpcode::VCmpxGeI64, false},
    {0xb7u, RdnaOpcode::VCmpxTI64, false},
    {0xe0u, RdnaOpcode::VCmpFU64, false},
    {0xe3u, RdnaOpcode::VCmpLeU64, false},
    {0xe6u, RdnaOpcode::VCmpGeU64, false},
    {0xe7u, RdnaOpcode::VCmpTU64, false},
    {0xf0u, RdnaOpcode::VCmpxFU64, false},
    {0xf1u, RdnaOpcode::VCmpxLtU64, false},
    {0xf2u, RdnaOpcode::VCmpxEqU64, false},
    {0xf3u, RdnaOpcode::VCmpxLeU64, false},
    {0xf4u, RdnaOpcode::VCmpxGtU64, false},
    {0xf6u, RdnaOpcode::VCmpxGeU64, false},
    {0xf7u, RdnaOpcode::VCmpxTU64, false},
    {0xc8u, RdnaOpcode::VCmpFF16},
    {0xcfu, RdnaOpcode::VCmpOF16},
    {0xe8u, RdnaOpcode::VCmpUF16},
    {0xe9u, RdnaOpcode::VCmpNgeF16},
    {0xeau, RdnaOpcode::VCmpNlgF16},
    {0xebu, RdnaOpcode::VCmpNgtF16},
    {0xecu, RdnaOpcode::VCmpNleF16},
    {0xeeu, RdnaOpcode::VCmpNltF16},
    {0xefu, RdnaOpcode::VCmpTruF16},
    {0xd8u, RdnaOpcode::VCmpxFF16},
    {0xddu, RdnaOpcode::VCmpxLgF16},
    {0xdfu, RdnaOpcode::VCmpxOF16},
    {0xf8u, RdnaOpcode::VCmpxUF16},
    {0xf9u, RdnaOpcode::VCmpxNgeF16},
    {0xfau, RdnaOpcode::VCmpxNlgF16},
    {0xfcu, RdnaOpcode::VCmpxNleF16},
    {0xffu, RdnaOpcode::VCmpxTruF16},
    {0x20u, RdnaOpcode::VCmpFF64, false},
    {0x21u, RdnaOpcode::VCmpLtF64, false},
    {0x22u, RdnaOpcode::VCmpEqF64, false},
    {0x23u, RdnaOpcode::VCmpLeF64, false},
    {0x24u, RdnaOpcode::VCmpGtF64, false},
    {0x25u, RdnaOpcode::VCmpLgF64, false},
    {0x26u, RdnaOpcode::VCmpGeF64, false},
    {0x27u, RdnaOpcode::VCmpOF64, false},
    {0x28u, RdnaOpcode::VCmpUF64, false},
    {0x29u, RdnaOpcode::VCmpNgeF64, false},
    {0x2au, RdnaOpcode::VCmpNlgF64, false},
    {0x2bu, RdnaOpcode::VCmpNgtF64, false},
    {0x2cu, RdnaOpcode::VCmpNleF64, false},
    {0x2du, RdnaOpcode::VCmpNeqF64, false},
    {0x2eu, RdnaOpcode::VCmpNltF64, false},
    {0x2fu, RdnaOpcode::VCmpTruF64, false},
    {0x30u, RdnaOpcode::VCmpxFF64, false},
    {0x31u, RdnaOpcode::VCmpxLtF64, false},
    {0x32u, RdnaOpcode::VCmpxEqF64, false},
    {0x33u, RdnaOpcode::VCmpxLeF64, false},
    {0x34u, RdnaOpcode::VCmpxGtF64, false},
    {0x35u, RdnaOpcode::VCmpxLgF64, false},
    {0x36u, RdnaOpcode::VCmpxGeF64, false},
    {0x37u, RdnaOpcode::VCmpxOF64, false},
    {0x38u, RdnaOpcode::VCmpxUF64, false},
    {0x39u, RdnaOpcode::VCmpxNgeF64, false},
    {0x3au, RdnaOpcode::VCmpxNlgF64, false},
    {0x3bu, RdnaOpcode::VCmpxNgtF64, false},
    {0x3cu, RdnaOpcode::VCmpxNleF64, false},
    {0x3du, RdnaOpcode::VCmpxNeqF64, false},
    {0x3eu, RdnaOpcode::VCmpxNltF64, false},
    {0x3fu, RdnaOpcode::VCmpxTruF64, false},
    {0xa8u, RdnaOpcode::VCmpClassF64, false},
    {0xb8u, RdnaOpcode::VCmpxClassF64, false},
};

constexpr VectorOpcodeInfo vop3Opcodes[] = {
    {0x140u, RdnaOpcode::VMadLegacyF32},
    {0x141u, RdnaOpcode::VMadF32},
    {0x142u, RdnaOpcode::VMadI32I24},
    {0x143u, RdnaOpcode::VMadU32U24},
    {0x176u, RdnaOpcode::VMadU64U32},
    {0x144u, RdnaOpcode::VCubeidF32},
    {0x145u, RdnaOpcode::VCubescF32},
    {0x146u, RdnaOpcode::VCubetcF32},
    {0x147u, RdnaOpcode::VCubemaF32},
    {0x14bu, RdnaOpcode::VFmaF32},
    {0x148u, RdnaOpcode::VBfeU32},
    {0x149u, RdnaOpcode::VBfeI32},
    {0x14au, RdnaOpcode::VBfiB32},
    {0x14eu, RdnaOpcode::VAlignbitB32},
    {0x150u, RdnaOpcode::VMullitF32},
    {0x151u, RdnaOpcode::VMin3F32},
    {0x152u, RdnaOpcode::VMin3I32},
    {0x153u, RdnaOpcode::VMin3U32},
    {0x351u, RdnaOpcode::VMin3F16},
    {0x352u, RdnaOpcode::VMin3I16},
    {0x353u, RdnaOpcode::VMin3U16},
    {0x154u, RdnaOpcode::VMax3F32},
    {0x155u, RdnaOpcode::VMax3I32},
    {0x156u, RdnaOpcode::VMax3U32},
    {0x354u, RdnaOpcode::VMax3F16},
    {0x355u, RdnaOpcode::VMax3I16},
    {0x356u, RdnaOpcode::VMax3U16},
    {0x157u, RdnaOpcode::VMed3F32},
    {0x158u, RdnaOpcode::VMed3I32},
    {0x159u, RdnaOpcode::VMed3U32},
    {0x357u, RdnaOpcode::VMed3F16},
    {0x358u, RdnaOpcode::VMed3I16},
    {0x359u, RdnaOpcode::VMed3U16},
    {0x15au, RdnaOpcode::VSadU8},
    {0x15bu, RdnaOpcode::VSadHiU8},
    {0x15cu, RdnaOpcode::VSadU16},
    {0x15du, RdnaOpcode::VSadU32},
    {0x171u, RdnaOpcode::VMsadU8},
    {0x15eu, RdnaOpcode::VCvtPkU8F32},
    {0x178u, RdnaOpcode::VXor3B32},
    {0x12fu, RdnaOpcode::VCvtPkrtzF16F32},
    {0x169u, RdnaOpcode::VMulLoU32},
    {0x16au, RdnaOpcode::VMulHiU32},
    {0x16bu, RdnaOpcode::VMulLoI32},
    {0x16cu, RdnaOpcode::VMulHiI32},
    {0x2ffu, RdnaOpcode::VLshlrevB64},
    {0x300u, RdnaOpcode::VLshrrevB64},
    {0x301u, RdnaOpcode::VAshrrevI64},
    {0x305u, RdnaOpcode::VMulLoU16},
    {0x340u, RdnaOpcode::VMadU16},
    {0x35eu, RdnaOpcode::VMadI16},
    {0x373u, RdnaOpcode::VMadU32U16},
    {0x375u, RdnaOpcode::VMadI32I16},
    {0x376u, RdnaOpcode::VSubNcI32},
    {0x37fu, RdnaOpcode::VAddNcI32},
    {0x303u, RdnaOpcode::VAddNcU16},
    {0x304u, RdnaOpcode::VSubNcU16},
    {0x307u, RdnaOpcode::VLshrrevB16},
    {0x308u, RdnaOpcode::VAshrrevI16},
    {0x309u, RdnaOpcode::VMaxU16},
    {0x30au, RdnaOpcode::VMaxI16},
    {0x30bu, RdnaOpcode::VMinU16},
    {0x30cu, RdnaOpcode::VMinI16},
    {0x30du, RdnaOpcode::VAddNcI16},
    {0x30eu, RdnaOpcode::VSubNcI16},
    {0x30fu, RdnaOpcode::VAddI32},
    {0x310u, RdnaOpcode::VSubI32},
    {0x311u, RdnaOpcode::VPackB32F16},
    {0x314u, RdnaOpcode::VLshlrevB16},
    {0x319u, RdnaOpcode::VSubrevI32},
    {0x344u, RdnaOpcode::VPermB32},
    {0x14du, RdnaOpcode::VLerpU8},
    {0x345u, RdnaOpcode::VXadU32},
    {0x346u, RdnaOpcode::VLshlAddU32},
    {0x347u, RdnaOpcode::VAddLshlU32},
    {0x360u, RdnaOpcode::VReadlaneB32},
    {0x361u, RdnaOpcode::VWritelaneB32},
    {0x362u, RdnaOpcode::VLdexpF32},
    {0x312u, RdnaOpcode::VCvtPknormI16F16},
    {0x313u, RdnaOpcode::VCvtPknormU16F16},
    {0x14cu, RdnaOpcode::VFmaF64},
    {0x164u, RdnaOpcode::VAddF64},
    {0x165u, RdnaOpcode::VMulF64},
    {0x166u, RdnaOpcode::VMinF64},
    {0x167u, RdnaOpcode::VMaxF64},
    {0x168u, RdnaOpcode::VLdexpF64},
    {0x174u, RdnaOpcode::VTrigPreopF64},
    {0x363u, RdnaOpcode::VBfmB32},
    {0x364u, RdnaOpcode::VBcntU32B32},
    {0x365u, RdnaOpcode::VMbcntLoU32B32},
    {0x366u, RdnaOpcode::VMbcntHiU32B32},
    {0x368u, RdnaOpcode::VCvtPknormI16F32},
    {0x369u, RdnaOpcode::VCvtPknormU16F32},
    {0x36au, RdnaOpcode::VCvtPkU16U32},
    {0x36bu, RdnaOpcode::VCvtPkI16I32},
    {0x36fu, RdnaOpcode::VLshlOrB32},
    {0x371u, RdnaOpcode::VAndOrB32},
    {0x372u, RdnaOpcode::VOr3B32},
    {0x377u, RdnaOpcode::VPermlane16B32},
    {0x378u, RdnaOpcode::VPermlanex16B32},
    {0x34bu, RdnaOpcode::VFmaF16},
    {0x36du, RdnaOpcode::VAdd3U32},
    {0x14fu, RdnaOpcode::VAlignbyteB32},
};

constexpr VectorOpcodeInfo vop3pOpcodes[] = {
    {0x00u, RdnaOpcode::VPkMadI16},
    {0x01u, RdnaOpcode::VPkMulLoU16},
    {0x02u, RdnaOpcode::VPkAddI16},
    {0x03u, RdnaOpcode::VPkSubI16},
    {0x04u, RdnaOpcode::VPkLshlrevB16},
    {0x05u, RdnaOpcode::VPkLshrrevB16},
    {0x06u, RdnaOpcode::VPkAshrrevI16},
    {0x07u, RdnaOpcode::VPkMaxI16},
    {0x08u, RdnaOpcode::VPkMinI16},
    {0x09u, RdnaOpcode::VPkMadU16},
    {0x0au, RdnaOpcode::VPkAddU16},
    {0x0bu, RdnaOpcode::VPkSubU16},
    {0x0cu, RdnaOpcode::VPkMaxU16},
    {0x0du, RdnaOpcode::VPkMinU16},
    {0x0eu, RdnaOpcode::VPkFmaF16},
    {0x0fu, RdnaOpcode::VPkAddF16},
    {0x10u, RdnaOpcode::VPkMulF16},
    {0x11u, RdnaOpcode::VPkMinF16},
    {0x12u, RdnaOpcode::VPkMaxF16},
    {0x20u, RdnaOpcode::VFmaF32},
    {0x21u, RdnaOpcode::VMadMixloF16},
    {0x22u, RdnaOpcode::VMadMixhiF16},
};

template <typename TEntry, std::size_t Size>
const TEntry* findVectorOpcodeEntry(const TEntry (&table)[Size], std::uint32_t encoding) {
    for (const auto& entry : table) {
        if (entry.encoding == encoding) {
            return &entry;
        }
    }
    return nullptr;
}

template <typename TEntry, std::size_t Size>
RdnaOpcode lookupVectorOpcode(const TEntry (&table)[Size], std::uint32_t encoding, const char* notSupportedReason) {
    const auto* entry = findVectorOpcodeEntry(table, encoding);
    if (entry == nullptr) {
        throw std::invalid_argument(notSupportedReason);
    }
    return entry->opcode;
}

bool isVop2LiteralMadOpcode(std::uint32_t opcode) {
    return opcode == 0x20u || opcode == 0x21u || opcode == 0x2cu || opcode == 0x2du;
}

bool isUnsupportedVop3EncodedVop2Alias(std::uint32_t opcode) {
    return isVop2LiteralMadOpcode(opcode) || opcode == 0x02u;
}

bool isVop3EncodedVopc(std::uint32_t opcode) {
    return opcode <= 0xffu;
}

bool isVop3EncodedVop2(std::uint32_t opcode) {
    return opcode >= 0x100u && opcode <= 0x13fu;
}

bool isVop3EncodedVop1(std::uint32_t opcode) {
    return opcode >= 0x180u && opcode <= 0x1ffu;
}

RdnaOpcode lookupVop3Opcode(std::uint32_t opcode) {
    const auto* direct = findVectorOpcodeEntry(vop3Opcodes, opcode);
    if (direct != nullptr) {
        return direct->opcode;
    }
    if (isVop3EncodedVopc(opcode)) {
        return lookupVectorOpcode(vopcOpcodes, opcode, "VOP3 opcode is not implemented");
    }
    if (isVop3EncodedVop2(opcode)) {
        const auto vop2Encoding = opcode - 0x100u;
        if (isUnsupportedVop3EncodedVop2Alias(vop2Encoding)) {
            throw std::invalid_argument("VOP3 opcode is not implemented");
        }
        return lookupVectorOpcode(vop2Opcodes, vop2Encoding, "VOP3 opcode is not implemented");
    }
    if (isVop3EncodedVop1(opcode)) {
        return lookupVectorOpcode(vop3EncodedVop1Opcodes, opcode - 0x180u, "VOP3 opcode is not implemented");
    }
    throw std::invalid_argument("VOP3 opcode is not implemented");
}

RdnaOpcode lookupVintrpOpcode(std::uint32_t opcode) {
    switch (opcode) {
        case 0x00u: return RdnaOpcode::VInterpP1F32;
        case 0x01u: return RdnaOpcode::VInterpP2F32;
        case 0x02u: return RdnaOpcode::VInterpMovF32;
        default: throw std::invalid_argument("VINTRP opcode is not implemented");
    }
}

bool isVop1WithoutOperands(RdnaOpcode opcode) {
    return opcode == RdnaOpcode::VNop || opcode == RdnaOpcode::VPipeflush || opcode == RdnaOpcode::VClrexcp;
}

bool usesScalarDestination(RdnaOpcode opcode) {
    return opcode == RdnaOpcode::VReadfirstlaneB32 || opcode == RdnaOpcode::VReadlaneB32;
}

bool isVop3BCarryOutOpcode(RdnaOpcode opcode) {
    return opcode == RdnaOpcode::VAddI32 || opcode == RdnaOpcode::VSubI32 || opcode == RdnaOpcode::VSubrevI32;
}

bool isVop3BMadU64Opcode(RdnaOpcode opcode) {
    return opcode == RdnaOpcode::VMadU64U32;
}

bool isPermlaneOpcode(RdnaOpcode opcode) {
    return opcode == RdnaOpcode::VPermlane16B32 || opcode == RdnaOpcode::VPermlanex16B32;
}

bool isNativeVop3F16TernaryOpcode(RdnaOpcode opcode) {
    return opcode == RdnaOpcode::VMin3F16 || opcode == RdnaOpcode::VMax3F16 || opcode == RdnaOpcode::VMed3F16 ||
        opcode == RdnaOpcode::VFmaF16;
}

bool isNativeVop3I16TernaryOpcode(RdnaOpcode opcode) {
    return opcode == RdnaOpcode::VMed3I16 || opcode == RdnaOpcode::VMadU16 || opcode == RdnaOpcode::VMadI16 ||
        opcode == RdnaOpcode::VMadU32U16 || opcode == RdnaOpcode::VMadI32I16;
}

bool usesSignedSaturateClamp(RdnaOpcode opcode) {
    return opcode == RdnaOpcode::VAddNcI32 || opcode == RdnaOpcode::VSubNcI32;
}

bool isNativeVop3B16BinaryOpcode(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::VAddNcU16:
        case RdnaOpcode::VMulLoU16:
        case RdnaOpcode::VSubNcU16:
        case RdnaOpcode::VMaxU16:
        case RdnaOpcode::VMaxI16:
        case RdnaOpcode::VMinU16:
        case RdnaOpcode::VMinI16:
        case RdnaOpcode::VAddNcI16:
        case RdnaOpcode::VSubNcI16:
        case RdnaOpcode::VLshlrevB16:
        case RdnaOpcode::VLshrrevB16:
        case RdnaOpcode::VAshrrevI16: return true;
        default: return false;
    }
}

bool isVop1FloatSourceOpcode(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::VFrexpMantF16:
        case RdnaOpcode::VFrexpExpI16F16:
        case RdnaOpcode::VCvtNormI16F16:
        case RdnaOpcode::VCvtNormU16F16:
        case RdnaOpcode::VMovB32:
        case RdnaOpcode::VCvtF32F16:
        case RdnaOpcode::VCvtU32F32:
        case RdnaOpcode::VCvtI32F32:
        case RdnaOpcode::VCvtRpiI32F32:
        case RdnaOpcode::VCvtFlrI32F32:
        case RdnaOpcode::VFrexpExpI32F32:
        case RdnaOpcode::VFrexpMantF32:
        case RdnaOpcode::VCvtF16F32:
        case RdnaOpcode::VCvtU16F16:
        case RdnaOpcode::VCvtI16F16:
        case RdnaOpcode::VRcpF32:
        case RdnaOpcode::VRcpIflagF32:
        case RdnaOpcode::VFractF32:
        case RdnaOpcode::VTruncF32:
        case RdnaOpcode::VCeilF32:
        case RdnaOpcode::VRndneF32:
        case RdnaOpcode::VFloorF32:
        case RdnaOpcode::VCvtI32F64:
        case RdnaOpcode::VCvtF32F64:
        case RdnaOpcode::VCvtU32F64:
        case RdnaOpcode::VCvtF64F32:
        case RdnaOpcode::VTruncF64:
        case RdnaOpcode::VCeilF64:
        case RdnaOpcode::VRndneF64:
        case RdnaOpcode::VFloorF64:
        case RdnaOpcode::VFrexpExpI32F64:
        case RdnaOpcode::VFrexpMantF64:
        case RdnaOpcode::VFractF64:
        case RdnaOpcode::VRcpF64:
        case RdnaOpcode::VRsqF64:
        case RdnaOpcode::VSqrtF64:
        case RdnaOpcode::VExpF32:
        case RdnaOpcode::VLogF32:
        case RdnaOpcode::VRsqF32:
        case RdnaOpcode::VSqrtF32:
        case RdnaOpcode::VRcpF16:
        case RdnaOpcode::VSqrtF16:
        case RdnaOpcode::VRsqF16:
        case RdnaOpcode::VLogF16:
        case RdnaOpcode::VExpF16:
        case RdnaOpcode::VFloorF16:
        case RdnaOpcode::VCeilF16:
        case RdnaOpcode::VTruncF16:
        case RdnaOpcode::VRndneF16:
        case RdnaOpcode::VFractF16:
        case RdnaOpcode::VSinF16:
        case RdnaOpcode::VCosF16:
        case RdnaOpcode::VSinF32:
        case RdnaOpcode::VCosF32: return true;
        default: return false;
    }
}

bool isVop1FloatResultOpcode(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::VCvtF32I32:
        case RdnaOpcode::VCvtF32U32:
        case RdnaOpcode::VCvtF32F16:
        case RdnaOpcode::VCvtF16F32:
        case RdnaOpcode::VCvtF16U16:
        case RdnaOpcode::VCvtF16I16:
        case RdnaOpcode::VCvtOffF32I4:
        case RdnaOpcode::VCvtF32Ubyte0:
        case RdnaOpcode::VCvtF32Ubyte1:
        case RdnaOpcode::VCvtF32Ubyte2:
        case RdnaOpcode::VCvtF32Ubyte3:
        case RdnaOpcode::VRcpF32:
        case RdnaOpcode::VRcpIflagF32:
        case RdnaOpcode::VFractF32:
        case RdnaOpcode::VTruncF32:
        case RdnaOpcode::VCeilF32:
        case RdnaOpcode::VRndneF32:
        case RdnaOpcode::VFloorF32:
        case RdnaOpcode::VCvtF64I32:
        case RdnaOpcode::VCvtF64U32:
        case RdnaOpcode::VCvtF32F64:
        case RdnaOpcode::VCvtF64F32:
        case RdnaOpcode::VTruncF64:
        case RdnaOpcode::VCeilF64:
        case RdnaOpcode::VRndneF64:
        case RdnaOpcode::VFloorF64:
        case RdnaOpcode::VFrexpMantF64:
        case RdnaOpcode::VFractF64:
        case RdnaOpcode::VRcpF64:
        case RdnaOpcode::VRsqF64:
        case RdnaOpcode::VSqrtF64:
        case RdnaOpcode::VFrexpMantF32:
        case RdnaOpcode::VExpF32:
        case RdnaOpcode::VLogF32:
        case RdnaOpcode::VRsqF32:
        case RdnaOpcode::VSqrtF32:
        case RdnaOpcode::VRcpF16:
        case RdnaOpcode::VSqrtF16:
        case RdnaOpcode::VRsqF16:
        case RdnaOpcode::VLogF16:
        case RdnaOpcode::VExpF16:
        case RdnaOpcode::VFloorF16:
        case RdnaOpcode::VCeilF16:
        case RdnaOpcode::VTruncF16:
        case RdnaOpcode::VRndneF16:
        case RdnaOpcode::VFractF16:
        case RdnaOpcode::VSinF16:
        case RdnaOpcode::VCosF16:
        case RdnaOpcode::VSinF32:
        case RdnaOpcode::VCosF32: return true;
        default: return false;
    }
}

bool isVopcFloat64CompareOpcode(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::VCmpFF64:
        case RdnaOpcode::VCmpLtF64:
        case RdnaOpcode::VCmpEqF64:
        case RdnaOpcode::VCmpLeF64:
        case RdnaOpcode::VCmpGtF64:
        case RdnaOpcode::VCmpLgF64:
        case RdnaOpcode::VCmpGeF64:
        case RdnaOpcode::VCmpOF64:
        case RdnaOpcode::VCmpUF64:
        case RdnaOpcode::VCmpNgeF64:
        case RdnaOpcode::VCmpNlgF64:
        case RdnaOpcode::VCmpNgtF64:
        case RdnaOpcode::VCmpNleF64:
        case RdnaOpcode::VCmpNeqF64:
        case RdnaOpcode::VCmpNltF64:
        case RdnaOpcode::VCmpTruF64:
        case RdnaOpcode::VCmpClassF64:
        case RdnaOpcode::VCmpxFF64:
        case RdnaOpcode::VCmpxLtF64:
        case RdnaOpcode::VCmpxEqF64:
        case RdnaOpcode::VCmpxLeF64:
        case RdnaOpcode::VCmpxGtF64:
        case RdnaOpcode::VCmpxLgF64:
        case RdnaOpcode::VCmpxGeF64:
        case RdnaOpcode::VCmpxOF64:
        case RdnaOpcode::VCmpxUF64:
        case RdnaOpcode::VCmpxNgeF64:
        case RdnaOpcode::VCmpxNlgF64:
        case RdnaOpcode::VCmpxNgtF64:
        case RdnaOpcode::VCmpxNleF64:
        case RdnaOpcode::VCmpxNeqF64:
        case RdnaOpcode::VCmpxNltF64:
        case RdnaOpcode::VCmpxTruF64:
        case RdnaOpcode::VCmpxClassF64: return true;
        default: return false;
    }
}

bool isVopcFloatCompareOpcode(RdnaOpcode opcode) {
    if (isVopcFloat64CompareOpcode(opcode)) {
        return true;
    }
    switch (opcode) {
        case RdnaOpcode::VCmpFF32:
        case RdnaOpcode::VCmpLtF32:
        case RdnaOpcode::VCmpEqF32:
        case RdnaOpcode::VCmpLeF32:
        case RdnaOpcode::VCmpGtF32:
        case RdnaOpcode::VCmpLgF32:
        case RdnaOpcode::VCmpGeF32:
        case RdnaOpcode::VCmpOF32:
        case RdnaOpcode::VCmpUF32:
        case RdnaOpcode::VCmpNgeF32:
        case RdnaOpcode::VCmpNlgF32:
        case RdnaOpcode::VCmpNgtF32:
        case RdnaOpcode::VCmpNleF32:
        case RdnaOpcode::VCmpNeqF32:
        case RdnaOpcode::VCmpNltF32:
        case RdnaOpcode::VCmpTruF32:
        case RdnaOpcode::VCmpxLtF32:
        case RdnaOpcode::VCmpxEqF32:
        case RdnaOpcode::VCmpxLeF32:
        case RdnaOpcode::VCmpxGtF32:
        case RdnaOpcode::VCmpxLgF32:
        case RdnaOpcode::VCmpxGeF32:
        case RdnaOpcode::VCmpxNgeF32:
        case RdnaOpcode::VCmpxNlgF32:
        case RdnaOpcode::VCmpxNgtF32:
        case RdnaOpcode::VCmpxNleF32:
        case RdnaOpcode::VCmpxNeqF32:
        case RdnaOpcode::VCmpxNltF32:
        case RdnaOpcode::VCmpLtF16:
        case RdnaOpcode::VCmpEqF16:
        case RdnaOpcode::VCmpLeF16:
        case RdnaOpcode::VCmpGtF16:
        case RdnaOpcode::VCmpLgF16:
        case RdnaOpcode::VCmpGeF16:
        case RdnaOpcode::VCmpNeqF16:
        case RdnaOpcode::VCmpxLtF16:
        case RdnaOpcode::VCmpxEqF16:
        case RdnaOpcode::VCmpxLeF16:
        case RdnaOpcode::VCmpxGtF16:
        case RdnaOpcode::VCmpxGeF16:
        case RdnaOpcode::VCmpxNgtF16:
        case RdnaOpcode::VCmpxNeqF16:
        case RdnaOpcode::VCmpxNltF16:
        case RdnaOpcode::VCmpxFF32:
        case RdnaOpcode::VCmpxOF32:
        case RdnaOpcode::VCmpxUF32:
        case RdnaOpcode::VCmpxTruF32:
        case RdnaOpcode::VCmpFF16:
        case RdnaOpcode::VCmpOF16:
        case RdnaOpcode::VCmpUF16:
        case RdnaOpcode::VCmpNgeF16:
        case RdnaOpcode::VCmpNlgF16:
        case RdnaOpcode::VCmpNgtF16:
        case RdnaOpcode::VCmpNleF16:
        case RdnaOpcode::VCmpNltF16:
        case RdnaOpcode::VCmpTruF16:
        case RdnaOpcode::VCmpxFF16:
        case RdnaOpcode::VCmpxLgF16:
        case RdnaOpcode::VCmpxOF16:
        case RdnaOpcode::VCmpxUF16:
        case RdnaOpcode::VCmpxNgeF16:
        case RdnaOpcode::VCmpxNlgF16:
        case RdnaOpcode::VCmpxNleF16:
        case RdnaOpcode::VCmpxTruF16:
        case RdnaOpcode::VCmpClassF32:
        case RdnaOpcode::VCmpxClassF32:
        case RdnaOpcode::VCmpClassF16:
        case RdnaOpcode::VCmpxClassF16: return true;
        default: return false;
    }
}

bool isVop2FloatOpcode(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::VAddF32:
        case RdnaOpcode::VSubF32:
        case RdnaOpcode::VSubrevF32:
        case RdnaOpcode::VMulF32:
        case RdnaOpcode::VMinF32:
        case RdnaOpcode::VMaxF32:
        case RdnaOpcode::VMacF32:
        case RdnaOpcode::VMadmkF32:
        case RdnaOpcode::VMadakF32:
        case RdnaOpcode::VCvtPkrtzF16F32:
        case RdnaOpcode::VAddF16:
        case RdnaOpcode::VSubF16:
        case RdnaOpcode::VSubrevF16:
        case RdnaOpcode::VMulF16:
        case RdnaOpcode::VFmacF16:
        case RdnaOpcode::VFmamkF16:
        case RdnaOpcode::VFmaakF16:
        case RdnaOpcode::VMaxF16:
        case RdnaOpcode::VMinF16: return true;
        default: return false;
    }
}

bool usesInexactClampControl(RdnaOpcode opcode) {
    return opcode == RdnaOpcode::VCvtU32F32 || opcode == RdnaOpcode::VCvtI32F32 ||
        opcode == RdnaOpcode::VCvtU16F16 || opcode == RdnaOpcode::VCvtI16F16;
}

bool supportsVop1Clamp(RdnaOpcode opcode) {
    return isVop1FloatResultOpcode(opcode) || usesInexactClampControl(opcode);
}

constexpr std::uint32_t sdwaSel(std::uint32_t sel) {
    return 1u << sel;
}

constexpr std::uint32_t sdwaSelBytes() {
    return sdwaSel(0) | sdwaSel(1) | sdwaSel(2) | sdwaSel(3);
}

constexpr std::uint32_t sdwaSelWords() {
    return sdwaSel(4) | sdwaSel(5);
}

constexpr std::uint32_t sdwaSelFull() {
    return sdwaSel(6);
}

constexpr std::uint32_t sdwaSelAll() {
    return sdwaSelBytes() | sdwaSelWords() | sdwaSelFull();
}

bool hasSdwaSelector(std::uint32_t mask, std::uint32_t selector) {
    return selector <= 6u && (mask & sdwaSel(selector)) != 0u;
}

bool isValidFullSdwaDestinationUnused(std::uint32_t dstUnused) {
    return dstUnused != 3u;
}

struct Vop1SdwaRule {
    RdnaOpcode opcode = RdnaOpcode::Unknown;
    std::uint32_t sourceSelectors = sdwaSelFull();
    std::uint32_t partialDstSelectors = 0;
    std::uint32_t partialDstSourceSelectors = 0;
    bool sourceModifiers = false;
};

constexpr Vop1SdwaRule vop1SdwaRules[] = {
    {RdnaOpcode::VMovB32, sdwaSelAll(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), false},
    {RdnaOpcode::VCvtF32U32, sdwaSelAll(), 0, 0, false},
    {RdnaOpcode::VCvtF32I32, sdwaSelAll(), 0, 0, false},
    {RdnaOpcode::VCvtF32Ubyte0, sdwaSelAll(), 0, 0, false},
    {RdnaOpcode::VNotB32, sdwaSelAll(), 0, 0, false},
    {RdnaOpcode::VFfblB32, sdwaSelAll(), 0, 0, false},
    {RdnaOpcode::VFfbhU32, sdwaSelAll(), 0, 0, false},
    {RdnaOpcode::VBfrevB32, sdwaSelAll(), 0, 0, false},
    {RdnaOpcode::VCvtF32F16, sdwaSelWords() | sdwaSelFull(), 0, 0, true},
    {RdnaOpcode::VCvtF16F32, sdwaSelAll(), sdwaSelWords(), sdwaSelAll(), true},
    {RdnaOpcode::VCvtF16U16, sdwaSelWords() | sdwaSelFull(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), false},
    {RdnaOpcode::VCvtU16F16, sdwaSelWords() | sdwaSelFull(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), true},
    {RdnaOpcode::VCvtF16I16, sdwaSelWords() | sdwaSelFull(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), false},
    {RdnaOpcode::VCvtI16F16, sdwaSelWords() | sdwaSelFull(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), true},
    {RdnaOpcode::VRcpF16, sdwaSelWords() | sdwaSelFull(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), true},
    {RdnaOpcode::VSqrtF16, sdwaSelWords() | sdwaSelFull(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), true},
    {RdnaOpcode::VRsqF16, sdwaSelWords() | sdwaSelFull(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), true},
    {RdnaOpcode::VLogF16, sdwaSelWords() | sdwaSelFull(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), true},
    {RdnaOpcode::VExpF16, sdwaSelWords() | sdwaSelFull(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), true},
    {RdnaOpcode::VFloorF16, sdwaSelWords() | sdwaSelFull(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), true},
    {RdnaOpcode::VCeilF16, sdwaSelWords() | sdwaSelFull(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), true},
    {RdnaOpcode::VTruncF16, sdwaSelWords() | sdwaSelFull(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), true},
    {RdnaOpcode::VRndneF16, sdwaSelWords() | sdwaSelFull(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), true},
    {RdnaOpcode::VFractF16, sdwaSelWords() | sdwaSelFull(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), true},
    {RdnaOpcode::VSinF16, sdwaSelWords() | sdwaSelFull(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), true},
    {RdnaOpcode::VCosF16, sdwaSelWords() | sdwaSelFull(), sdwaSelWords(), sdwaSelWords() | sdwaSelFull(), true},
    {RdnaOpcode::VCvtU32F32, sdwaSelFull(), sdwaSelBytes() | sdwaSelWords(), sdwaSelFull(), false},
    {RdnaOpcode::VCvtI32F32, sdwaSelFull(), sdwaSelBytes() | sdwaSelWords(), sdwaSelFull(), false},
    {RdnaOpcode::VCvtRpiI32F32, sdwaSelFull(), sdwaSelBytes() | sdwaSelWords(), sdwaSelFull(), false},
    {RdnaOpcode::VCvtFlrI32F32, sdwaSelFull(), sdwaSelBytes() | sdwaSelWords(), sdwaSelFull(), false},
};

const Vop1SdwaRule* findVop1SdwaRule(RdnaOpcode opcode) {
    for (const auto& rule : vop1SdwaRules) {
        if (rule.opcode == opcode) {
            return &rule;
        }
    }
    return nullptr;
}

bool isVop1SdwaSourceSupported(RdnaOpcode opcode, std::uint32_t sourceSelector, bool sourceNegate, bool sourceAbsolute) {
    if (sourceSelector == 6u) {
        return isVop1FloatSourceOpcode(opcode) || (!sourceNegate && !sourceAbsolute);
    }
    const auto* rule = findVop1SdwaRule(opcode);
    if (rule == nullptr || !hasSdwaSelector(rule->sourceSelectors, sourceSelector)) {
        return false;
    }
    return rule->sourceModifiers || (!sourceNegate && !sourceAbsolute);
}

bool isVop1SdwaDestinationSupported(RdnaOpcode opcode, std::uint32_t destinationSelector, std::uint32_t destinationUnused, std::uint32_t sourceSelector) {
    if (destinationSelector == 6u) {
        return isValidFullSdwaDestinationUnused(destinationUnused);
    }
    const auto* rule = findVop1SdwaRule(opcode);
    if (rule == nullptr || destinationUnused == 3u) {
        return false;
    }
    return hasSdwaSelector(rule->partialDstSelectors, destinationSelector) &&
        hasSdwaSelector(rule->partialDstSourceSelectors, sourceSelector);
}

void validateVop1Sdwa(const RdnaInstruction& instruction, std::uint32_t destinationSelector, std::uint32_t destinationUnused, std::uint32_t clamp, std::uint32_t omod, std::uint32_t sourceSelector, bool sourceNegate, bool sourceAbsolute) {
    if (sourceSelector > 6u || destinationSelector > 6u) {
        throw std::invalid_argument("VOP1 SDWA selector is invalid");
    }
    if ((clamp != 0u && !supportsVop1Clamp(instruction.op)) || (omod != 0u && !isVop1FloatResultOpcode(instruction.op))) {
        throw std::invalid_argument("VOP1 SDWA output modifiers are not supported");
    }
    if (!isVop1SdwaDestinationSupported(instruction.op, destinationSelector, destinationUnused, sourceSelector)) {
        throw std::invalid_argument("VOP1 SDWA destination selector is not supported");
    }
    if (!isVop1SdwaSourceSupported(instruction.op, sourceSelector, sourceNegate, sourceAbsolute)) {
        char text[96];
        std::snprintf(text, sizeof(text), "VOP1 SDWA source selector %u of opcode 0x%x at pc 0x%x is not supported", sourceSelector, instruction.opcodeId, instruction.programCounter);
        throw std::invalid_argument(text);
    }
}

void decodeVop1Sdwa(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex, std::uint32_t vdst, RdnaInstruction& instruction) {
    if (static_cast<std::size_t>(wordIndex) + 1u >= code.size()) {
        throw std::out_of_range("truncated VOP1 SDWA instruction");
    }
    const auto modifier = code[wordIndex + 1u];
    const auto src0 = modifier & 0xffu;
    const auto dstSel = (modifier >> 8u) & 0x7u;
    const auto dstUnused = (modifier >> 11u) & 0x3u;
    const auto clamp = (modifier >> 13u) & 0x1u;
    const auto omod = (modifier >> 14u) & 0x3u;
    const auto src0Sel = (modifier >> 16u) & 0x7u;
    const auto src0Sext = (modifier >> 19u) & 0x1u;
    const auto src0Neg = (modifier >> 20u) & 0x1u;
    const auto src0Abs = (modifier >> 21u) & 0x1u;
    const auto s0 = (modifier >> 23u) & 0x1u;

    SetRdnaRawWords(instruction, code, wordIndex, 2);
    validateVop1Sdwa(instruction, dstSel, dstUnused, clamp, omod, src0Sel, src0Neg != 0u, src0Abs != 0u);

    instruction.destination = usesScalarDestination(instruction.op)
        ? DecodeRdnaScalarDestination(vdst, programCounter)
        : DecodeRdnaVectorGpr(vdst);
    instruction.source0 = DecodeRdnaScalarSource(src0 + (s0 == 0u ? 256u : 0u), programCounter);
    instruction.destination.sdwaSel = dstSel;
    instruction.destination.sdwaDstUnused = dstUnused;
    instruction.destination.explicitSdwaDst = true;
    instruction.destination.clamp = clamp != 0u;
    instruction.destination.omod = omod;
    instruction.source0.sdwaSel = src0Sel;
    instruction.source0.sdwaSext = src0Sext != 0u;
    instruction.source0.negate = src0Neg != 0u;
    instruction.source0.absolute = src0Abs != 0u;
    instruction.sourceCount = 1;
    ReadRdnaLiteralOperands(code, wordIndex, instruction);
}

void applyDppModifier(RdnaOperand& operand, std::uint32_t modifier) {
    operand.negate = ((modifier >> 20u) & 0x1u) != 0u;
    operand.absolute = ((modifier >> 21u) & 0x1u) != 0u;
    operand.dpp = true;
    operand.dppCtrl = (modifier >> 8u) & 0x1ffu;
    operand.dppFetchInactive = ((modifier >> 18u) & 0x1u) != 0u;
    operand.dppBoundCtrl = ((modifier >> 19u) & 0x1u) != 0u;
    operand.dppBankMask = (modifier >> 24u) & 0xfu;
    operand.dppRowMask = (modifier >> 28u) & 0xfu;
}

void decodeVop1Dpp(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex, std::uint32_t vdst, RdnaInstruction& instruction) {
    if (static_cast<std::size_t>(wordIndex) + 1u >= code.size()) {
        throw std::out_of_range("truncated VOP1 DPP instruction");
    }
    const auto modifier = code[wordIndex + 1u];
    const auto src0 = modifier & 0xffu;
    SetRdnaRawWords(instruction, code, wordIndex, 2);

    instruction.destination = usesScalarDestination(instruction.op)
        ? DecodeRdnaScalarDestination(vdst, programCounter)
        : DecodeRdnaVectorGpr(vdst);
    instruction.source0 = DecodeRdnaScalarSource(src0 + 256u, programCounter);
    applyDppModifier(instruction.source0, modifier);
    instruction.sourceCount = 1;

    if (!isVop1FloatSourceOpcode(instruction.op) && (instruction.source0.negate || instruction.source0.absolute)) {
        throw std::invalid_argument("VOP1 DPP integer source modifiers are not supported");
    }
    ReadRdnaLiteralOperands(code, wordIndex, instruction);
}

struct Vop2SdwaFields {
    std::uint32_t src0 = 0;
    std::uint32_t dstSel = 6;
    std::uint32_t dstUnused = 0;
    std::uint32_t clamp = 0;
    std::uint32_t omod = 0;
    std::uint32_t src0Sel = 6;
    std::uint32_t src0Sext = 0;
    std::uint32_t src0Neg = 0;
    std::uint32_t src0Abs = 0;
    std::uint32_t s0 = 0;
    std::uint32_t src1Sel = 6;
    std::uint32_t src1Sext = 0;
    std::uint32_t src1Neg = 0;
    std::uint32_t src1Abs = 0;
    std::uint32_t s1 = 0;
};

Vop2SdwaFields decodeVop2SdwaFields(std::uint32_t modifier) {
    Vop2SdwaFields fields;
    fields.src0 = modifier & 0xffu;
    fields.dstSel = (modifier >> 8u) & 0x7u;
    fields.dstUnused = (modifier >> 11u) & 0x3u;
    fields.clamp = (modifier >> 13u) & 0x1u;
    fields.omod = (modifier >> 14u) & 0x3u;
    fields.src0Sel = (modifier >> 16u) & 0x7u;
    fields.src0Sext = (modifier >> 19u) & 0x1u;
    fields.src0Neg = (modifier >> 20u) & 0x1u;
    fields.src0Abs = (modifier >> 21u) & 0x1u;
    fields.s0 = (modifier >> 23u) & 0x1u;
    fields.src1Sel = (modifier >> 24u) & 0x7u;
    fields.src1Sext = (modifier >> 27u) & 0x1u;
    fields.src1Neg = (modifier >> 28u) & 0x1u;
    fields.src1Abs = (modifier >> 29u) & 0x1u;
    fields.s1 = (modifier >> 31u) & 0x1u;
    return fields;
}

struct Vop2SdwaRule {
    std::uint32_t dstSelectors = sdwaSelFull();
    std::uint32_t src0Selectors = sdwaSelFull();
    std::uint32_t src1Selectors = sdwaSelFull();
    bool partialDst = false;
    bool sourceModifiers = false;
};

constexpr Vop2SdwaRule vop2SdwaRules[] = {
    {},
    {sdwaSelFull(), sdwaSelFull(), sdwaSelFull(), false, true},
    {sdwaSelWords() | sdwaSelFull(), sdwaSelWords() | sdwaSelFull(), sdwaSelWords() | sdwaSelFull(), true, true},
    {sdwaSelAll(), sdwaSelAll(), sdwaSelAll(), true, true},
    {sdwaSelFull(), sdwaSelAll(), sdwaSelAll(), false, false},
    {sdwaSelAll(), sdwaSelAll(), sdwaSelAll(), true, false},
    {sdwaSelFull(), sdwaSelAll(), sdwaSelAll(), false, false},
    {sdwaSelFull(), sdwaSelAll(), sdwaSelAll(), false, false},
    {sdwaSelWords() | sdwaSelFull(), sdwaSelAll(), sdwaSelAll(), true, false},
};

const Vop2SdwaRule* findVop2SdwaRule(std::uint32_t encoding) {
    const auto* info = findVectorOpcodeEntry(vop2Opcodes, encoding);
    if (info == nullptr || info->sdwaProfile == Vop2SdwaProfile::None) {
        return nullptr;
    }
    return &vop2SdwaRules[static_cast<std::size_t>(info->sdwaProfile)];
}

bool isVop2SdwaDestinationSupported(const Vop2SdwaRule& rule, const Vop2SdwaFields& fields) {
    if (!hasSdwaSelector(rule.dstSelectors, fields.dstSel)) {
        return false;
    }
    if (fields.dstSel == 6u) {
        return isValidFullSdwaDestinationUnused(fields.dstUnused);
    }
    return fields.dstUnused != 3u && rule.partialDst;
}

bool isFullWidthVop2Sdwa(const Vop2SdwaFields& fields) {
    return fields.dstSel == 6u && fields.dstUnused == 0u && fields.src0Sel == 6u && fields.src1Sel == 6u;
}

void validateVop2Sdwa(const RdnaInstruction& instruction, std::uint32_t opcode, const Vop2SdwaFields& fields) {
    if (fields.src0Sel > 6u || fields.src1Sel > 6u || fields.dstSel > 6u) {
        throw std::invalid_argument("VOP2 SDWA selector is invalid");
    }
    if ((fields.clamp != 0u || fields.omod != 0u) && !isVop2FloatOpcode(instruction.op)) {
        throw std::invalid_argument("VOP2 SDWA output modifiers are not supported");
    }
    if (isFullWidthVop2Sdwa(fields)) {
        if (!isVop2FloatOpcode(instruction.op) && instruction.op != RdnaOpcode::VCndmaskB32 &&
            (fields.src0Neg != 0u || fields.src0Abs != 0u || fields.src1Neg != 0u || fields.src1Abs != 0u)) {
            throw std::invalid_argument("VOP2 SDWA source modifiers are not supported");
        }
        return;
    }

    const auto* rule = findVop2SdwaRule(opcode);
    if (rule == nullptr) {
        throw std::invalid_argument("VOP2 SDWA modifier is not supported for opcode");
    }
    if (!isVop2SdwaDestinationSupported(*rule, fields)) {
        throw std::invalid_argument("VOP2 SDWA destination selector is not supported");
    }
    if (!hasSdwaSelector(rule->src0Selectors, fields.src0Sel) || !hasSdwaSelector(rule->src1Selectors, fields.src1Sel)) {
        throw std::invalid_argument("VOP2 SDWA source selector is not supported");
    }
    const bool hasSourceModifiers = fields.src0Neg != 0u || fields.src0Abs != 0u || fields.src1Neg != 0u || fields.src1Abs != 0u;
    const bool cndmaskFloatModifiers = instruction.op == RdnaOpcode::VCndmaskB32 && fields.src0Sel == 6u && fields.src1Sel == 6u;
    if (!rule->sourceModifiers && hasSourceModifiers && !cndmaskFloatModifiers) {
        throw std::invalid_argument("VOP2 SDWA source modifiers are not supported");
    }
}

void finalizeVop2Instruction(std::span<const std::uint32_t> code, std::uint32_t wordIndex, RdnaInstruction& instruction) {
    switch (instruction.op) {
        case RdnaOpcode::VMadmkF32:
        case RdnaOpcode::VFmamkF16:
            instruction.source2 = instruction.source1;
            instruction.source1 = RdnaOperand{};
            instruction.source1.kind = RdnaOperandKind::LiteralConstant;
            instruction.sourceCount = 3;
            break;
        case RdnaOpcode::VMadakF32:
        case RdnaOpcode::VFmaakF16:
            instruction.source2 = RdnaOperand{};
            instruction.source2.kind = RdnaOperandKind::LiteralConstant;
            instruction.sourceCount = 3;
            break;
        case RdnaOpcode::VAddcU32:
        case RdnaOpcode::VSubCoCiU32:
        case RdnaOpcode::VSubrevCoCiU32:
            instruction.destination2.kind = RdnaOperandKind::VccLo;
            instruction.source2.kind = RdnaOperandKind::VccLo;
            instruction.sourceCount = 3;
            break;
        case RdnaOpcode::VPkFmacF16:
            instruction.source0.opSelHi = true;
            instruction.source1.opSelHi = true;
            instruction.destination.opSelHi = true;
            instruction.sourceCount = 2;
            break;
        default: instruction.sourceCount = 2; break;
    }
    ReadRdnaLiteralOperands(code, wordIndex, instruction);
}

void decodeVop2Sdwa(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex, std::uint32_t opcode, std::uint32_t vdst, std::uint32_t vsrc1, RdnaInstruction& instruction) {
    if (static_cast<std::size_t>(wordIndex) + 1u >= code.size()) {
        throw std::out_of_range("truncated VOP2 SDWA instruction");
    }
    const auto modifier = code[wordIndex + 1u];
    const auto fields = decodeVop2SdwaFields(modifier);
    SetRdnaRawWords(instruction, code, wordIndex, 2);
    validateVop2Sdwa(instruction, opcode, fields);

    instruction.destination = DecodeRdnaVectorGpr(vdst);
    instruction.source0 = DecodeRdnaScalarSource(fields.src0 + (fields.s0 == 0u ? 256u : 0u), programCounter);
    instruction.source1 = DecodeRdnaScalarSource(vsrc1 + (fields.s1 == 0u ? 256u : 0u), programCounter);
    instruction.destination.sdwaSel = fields.dstSel;
    instruction.destination.sdwaDstUnused = fields.dstUnused;
    instruction.destination.explicitSdwaDst = true;
    instruction.destination.clamp = fields.clamp != 0u;
    instruction.destination.omod = fields.omod;
    instruction.source0.sdwaSel = fields.src0Sel;
    instruction.source0.sdwaSext = fields.src0Sext != 0u;
    instruction.source0.negate = fields.src0Neg != 0u;
    instruction.source0.absolute = fields.src0Abs != 0u;
    instruction.source1.sdwaSel = fields.src1Sel;
    instruction.source1.sdwaSext = fields.src1Sext != 0u;
    instruction.source1.negate = fields.src1Neg != 0u;
    instruction.source1.absolute = fields.src1Abs != 0u;
    finalizeVop2Instruction(code, wordIndex, instruction);
}

void decodeVop2Dpp(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex, std::uint32_t opcode, std::uint32_t vdst, std::uint32_t vsrc1, RdnaInstruction& instruction) {
    if (static_cast<std::size_t>(wordIndex) + 1u >= code.size()) {
        throw std::out_of_range("truncated VOP2 DPP instruction");
    }
    const auto modifier = code[wordIndex + 1u];
    const auto src0 = modifier & 0xffu;
    SetRdnaRawWords(instruction, code, wordIndex, 2);

    instruction.destination = DecodeRdnaVectorGpr(vdst);
    instruction.source1 = DecodeRdnaVectorGpr(vsrc1);
    instruction.source0 = DecodeRdnaScalarSource(src0 + 256u, programCounter);
    applyDppModifier(instruction.source0, modifier);
    instruction.source1.negate = ((modifier >> 22u) & 0x1u) != 0u;
    instruction.source1.absolute = ((modifier >> 23u) & 0x1u) != 0u;
    const bool packedFmac = instruction.op == RdnaOpcode::VPkFmacF16;
    if (packedFmac) {
        instruction.source0.negateHi = instruction.source0.negate;
        instruction.source1.negateHi = instruction.source1.negate;
    }

    if (!isVop2FloatOpcode(instruction.op) && !packedFmac &&
        (instruction.source0.negate || instruction.source0.absolute || instruction.source1.negate || instruction.source1.absolute)) {
        throw std::invalid_argument("VOP2 DPP integer source modifiers are not supported");
    }
    finalizeVop2Instruction(code, wordIndex, instruction);
}

struct VopcSdwaFields {
    std::uint32_t src0 = 0;
    std::uint32_t sdst = 0;
    std::uint32_t sd = 0;
    std::uint32_t src0Sel = 6;
    std::uint32_t src0Sext = 0;
    std::uint32_t src0Neg = 0;
    std::uint32_t src0Abs = 0;
    std::uint32_t s0 = 0;
    std::uint32_t src1Sel = 6;
    std::uint32_t src1Sext = 0;
    std::uint32_t src1Neg = 0;
    std::uint32_t src1Abs = 0;
    std::uint32_t s1 = 0;
};

VopcSdwaFields decodeVopcSdwaFields(std::uint32_t modifier) {
    VopcSdwaFields fields;
    fields.src0 = modifier & 0xffu;
    fields.sdst = (modifier >> 8u) & 0x7fu;
    fields.sd = (modifier >> 15u) & 0x1u;
    fields.src0Sel = (modifier >> 16u) & 0x7u;
    fields.src0Sext = (modifier >> 19u) & 0x1u;
    fields.src0Neg = (modifier >> 20u) & 0x1u;
    fields.src0Abs = (modifier >> 21u) & 0x1u;
    fields.s0 = (modifier >> 23u) & 0x1u;
    fields.src1Sel = (modifier >> 24u) & 0x7u;
    fields.src1Sext = (modifier >> 27u) & 0x1u;
    fields.src1Neg = (modifier >> 28u) & 0x1u;
    fields.src1Abs = (modifier >> 29u) & 0x1u;
    fields.s1 = (modifier >> 31u) & 0x1u;
    return fields;
}

bool isVopcCompareExec(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::VCmpxLtF32:
        case RdnaOpcode::VCmpxEqF32:
        case RdnaOpcode::VCmpxLeF32:
        case RdnaOpcode::VCmpxGtF32:
        case RdnaOpcode::VCmpxLgF32:
        case RdnaOpcode::VCmpxGeF32:
        case RdnaOpcode::VCmpxNgeF32:
        case RdnaOpcode::VCmpxNlgF32:
        case RdnaOpcode::VCmpxNgtF32:
        case RdnaOpcode::VCmpxNleF32:
        case RdnaOpcode::VCmpxNeqF32:
        case RdnaOpcode::VCmpxNltF32:
        case RdnaOpcode::VCmpxLtI32:
        case RdnaOpcode::VCmpxEqI32:
        case RdnaOpcode::VCmpxLeI32:
        case RdnaOpcode::VCmpxGtI32:
        case RdnaOpcode::VCmpxNeI32:
        case RdnaOpcode::VCmpxGeI32:
        case RdnaOpcode::VCmpxClassF32:
        case RdnaOpcode::VCmpxClassF16:
        case RdnaOpcode::VCmpxLtU32:
        case RdnaOpcode::VCmpxEqU32:
        case RdnaOpcode::VCmpxLeU32:
        case RdnaOpcode::VCmpxGtU32:
        case RdnaOpcode::VCmpxNeU32:
        case RdnaOpcode::VCmpxGeU32:
        case RdnaOpcode::VCmpxNeI64:
        case RdnaOpcode::VCmpxNeU64:
        case RdnaOpcode::VCmpxFF32:
        case RdnaOpcode::VCmpxOF32:
        case RdnaOpcode::VCmpxUF32:
        case RdnaOpcode::VCmpxTruF32:
        case RdnaOpcode::VCmpxFI32:
        case RdnaOpcode::VCmpxTI32:
        case RdnaOpcode::VCmpxFU32:
        case RdnaOpcode::VCmpxTU32:
        case RdnaOpcode::VCmpxFI64:
        case RdnaOpcode::VCmpxLtI64:
        case RdnaOpcode::VCmpxEqI64:
        case RdnaOpcode::VCmpxLeI64:
        case RdnaOpcode::VCmpxGtI64:
        case RdnaOpcode::VCmpxGeI64:
        case RdnaOpcode::VCmpxTI64:
        case RdnaOpcode::VCmpxFU64:
        case RdnaOpcode::VCmpxLtU64:
        case RdnaOpcode::VCmpxEqU64:
        case RdnaOpcode::VCmpxLeU64:
        case RdnaOpcode::VCmpxGtU64:
        case RdnaOpcode::VCmpxGeU64:
        case RdnaOpcode::VCmpxTU64:
        case RdnaOpcode::VCmpxFF16:
        case RdnaOpcode::VCmpxLgF16:
        case RdnaOpcode::VCmpxOF16:
        case RdnaOpcode::VCmpxUF16:
        case RdnaOpcode::VCmpxNgeF16:
        case RdnaOpcode::VCmpxNlgF16:
        case RdnaOpcode::VCmpxNleF16:
        case RdnaOpcode::VCmpxTruF16:
        case RdnaOpcode::VCmpxLtI16:
        case RdnaOpcode::VCmpxEqI16:
        case RdnaOpcode::VCmpxLeI16:
        case RdnaOpcode::VCmpxGtI16:
        case RdnaOpcode::VCmpxNeI16:
        case RdnaOpcode::VCmpxGeI16:
        case RdnaOpcode::VCmpxLtU16:
        case RdnaOpcode::VCmpxEqU16:
        case RdnaOpcode::VCmpxLeU16:
        case RdnaOpcode::VCmpxGtU16:
        case RdnaOpcode::VCmpxNeU16:
        case RdnaOpcode::VCmpxGeU16:
        case RdnaOpcode::VCmpxLtF16:
        case RdnaOpcode::VCmpxEqF16:
        case RdnaOpcode::VCmpxLeF16:
        case RdnaOpcode::VCmpxGtF16:
        case RdnaOpcode::VCmpxGeF16:
        case RdnaOpcode::VCmpxNgtF16:
        case RdnaOpcode::VCmpxNeqF16:
        case RdnaOpcode::VCmpxNltF16:
        case RdnaOpcode::VCmpxFF64:
        case RdnaOpcode::VCmpxLtF64:
        case RdnaOpcode::VCmpxEqF64:
        case RdnaOpcode::VCmpxLeF64:
        case RdnaOpcode::VCmpxGtF64:
        case RdnaOpcode::VCmpxLgF64:
        case RdnaOpcode::VCmpxGeF64:
        case RdnaOpcode::VCmpxOF64:
        case RdnaOpcode::VCmpxUF64:
        case RdnaOpcode::VCmpxNgeF64:
        case RdnaOpcode::VCmpxNlgF64:
        case RdnaOpcode::VCmpxNgtF64:
        case RdnaOpcode::VCmpxNleF64:
        case RdnaOpcode::VCmpxNeqF64:
        case RdnaOpcode::VCmpxNltF64:
        case RdnaOpcode::VCmpxTruF64:
        case RdnaOpcode::VCmpxClassF64: return true;
        default: return false;
    }
}

void decodeVopcSdwa(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex, std::uint32_t vsrc1, RdnaInstruction& instruction) {
    if (static_cast<std::size_t>(wordIndex) + 1u >= code.size()) {
        throw std::out_of_range("truncated VOPC SDWA instruction");
    }
    const auto modifier = code[wordIndex + 1u];
    const auto fields = decodeVopcSdwaFields(modifier);
    SetRdnaRawWords(instruction, code, wordIndex, 2);
    if (fields.src0Sel > 6u || fields.src1Sel > 6u) {
        throw std::invalid_argument("VOPC SDWA selector is invalid");
    }

    instruction.source0 = DecodeRdnaScalarSource(fields.src0 + (fields.s0 == 0u ? 256u : 0u), programCounter);
    instruction.source1 = DecodeRdnaScalarSource(vsrc1 + (fields.s1 == 0u ? 256u : 0u), programCounter);
    if (isVopcCompareExec(instruction.op)) {
        instruction.destination.kind = RdnaOperandKind::ExecLo;
    } else if (fields.sd == 0u) {
        instruction.destination.kind = RdnaOperandKind::VccLo;
    } else {
        instruction.destination = DecodeRdnaScalarDestination(fields.sdst, programCounter);
    }
    instruction.source0.sdwaSel = fields.src0Sel;
    instruction.source0.sdwaSext = fields.src0Sext != 0u;
    instruction.source0.negate = fields.src0Neg != 0u;
    instruction.source0.absolute = fields.src0Abs != 0u;
    instruction.source1.sdwaSel = fields.src1Sel;
    instruction.source1.sdwaSext = fields.src1Sext != 0u;
    instruction.source1.negate = fields.src1Neg != 0u;
    instruction.source1.absolute = fields.src1Abs != 0u;
    instruction.sourceCount = 2;
    ReadRdnaLiteralOperands(code, wordIndex, instruction);
}

void decodeVopcDpp(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex, std::uint32_t opcode, std::uint32_t vsrc1, RdnaInstruction& instruction) {
    if (static_cast<std::size_t>(wordIndex) + 1u >= code.size()) {
        throw std::out_of_range("truncated VOPC DPP instruction");
    }
    const auto modifier = code[wordIndex + 1u];
    const auto src0 = modifier & 0xffu;
    SetRdnaRawWords(instruction, code, wordIndex, 2);
    const auto* info = findVectorOpcodeEntry(vopcOpcodes, opcode);
    if (info == nullptr || !info->supportsDpp) {
        throw std::invalid_argument("VOPC DPP modifier is not supported for opcode");
    }
    instruction.source1 = DecodeRdnaVectorGpr(vsrc1);
    instruction.source0 = DecodeRdnaScalarSource(src0 + 256u, programCounter);
    instruction.destination.kind = isVopcCompareExec(instruction.op) ? RdnaOperandKind::ExecLo : RdnaOperandKind::VccLo;
    applyDppModifier(instruction.source0, modifier);
    instruction.source1.negate = ((modifier >> 22u) & 0x1u) != 0u;
    instruction.source1.absolute = ((modifier >> 23u) & 0x1u) != 0u;
    instruction.sourceCount = 2;
    ReadRdnaLiteralOperands(code, wordIndex, instruction);
}

std::uint32_t nativeVop3SourceCount(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::VMulLoU32:
        case RdnaOpcode::VMulHiU32:
        case RdnaOpcode::VMulLoI32:
        case RdnaOpcode::VMulHiI32:
        case RdnaOpcode::VMulI32I24:
        case RdnaOpcode::VAddNcU16:
        case RdnaOpcode::VSubNcU16:
        case RdnaOpcode::VMaxU16:
        case RdnaOpcode::VMaxI16:
        case RdnaOpcode::VMinU16:
        case RdnaOpcode::VMinI16:
        case RdnaOpcode::VAddNcI16:
        case RdnaOpcode::VSubNcI16:
        case RdnaOpcode::VLshlrevB64:
        case RdnaOpcode::VLshrrevB64:
        case RdnaOpcode::VAshrrevI64:
        case RdnaOpcode::VAddNcI32:
        case RdnaOpcode::VSubNcI32:
        case RdnaOpcode::VMulLoU16:
        case RdnaOpcode::VLshlrevB16:
        case RdnaOpcode::VLshrrevB16:
        case RdnaOpcode::VAshrrevI16:
        case RdnaOpcode::VAddI32:
        case RdnaOpcode::VSubI32:
        case RdnaOpcode::VSubrevI32:
        case RdnaOpcode::VPackB32F16:
        case RdnaOpcode::VReadlaneB32:
        case RdnaOpcode::VWritelaneB32:
        case RdnaOpcode::VCvtPkrtzF16F32:
        case RdnaOpcode::VLdexpF32:
        case RdnaOpcode::VAddF64:
        case RdnaOpcode::VMulF64:
        case RdnaOpcode::VMinF64:
        case RdnaOpcode::VMaxF64:
        case RdnaOpcode::VLdexpF64:
        case RdnaOpcode::VTrigPreopF64:
        case RdnaOpcode::VBfmB32:
        case RdnaOpcode::VBcntU32B32:
        case RdnaOpcode::VCvtPknormI16F32:
        case RdnaOpcode::VCvtPknormU16F32:
        case RdnaOpcode::VCvtPknormI16F16:
        case RdnaOpcode::VCvtPknormU16F16:
        case RdnaOpcode::VCvtPkU16U32:
        case RdnaOpcode::VCvtPkI16I32: return 2;
        default: return 3;
    }
}

std::uint32_t vop3pSourceCount(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::VPkMulLoU16:
        case RdnaOpcode::VPkAddI16:
        case RdnaOpcode::VPkSubI16:
        case RdnaOpcode::VPkLshlrevB16:
        case RdnaOpcode::VPkLshrrevB16:
        case RdnaOpcode::VPkAshrrevI16:
        case RdnaOpcode::VPkMaxI16:
        case RdnaOpcode::VPkMinI16:
        case RdnaOpcode::VPkAddU16:
        case RdnaOpcode::VPkSubU16:
        case RdnaOpcode::VPkMaxU16:
        case RdnaOpcode::VPkMinU16:
        case RdnaOpcode::VPkAddF16:
        case RdnaOpcode::VPkMulF16:
        case RdnaOpcode::VPkMinF16:
        case RdnaOpcode::VPkMaxF16: return 2;
        default: return 3;
    }
}

bool isPackedVop3p(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::VPkAddF16:
        case RdnaOpcode::VPkMulF16:
        case RdnaOpcode::VPkMinF16:
        case RdnaOpcode::VPkMaxF16:
        case RdnaOpcode::VPkFmaF16: return true;
        default: return false;
    }
}

bool isMadMixF16(RdnaOpcode opcode) {
    return opcode == RdnaOpcode::VMadMixloF16 || opcode == RdnaOpcode::VMadMixhiF16;
}

void applyVop3pSourceModifiers(RdnaInstruction& instruction, std::uint32_t opSel, std::uint32_t opSelHi, std::uint32_t neg, std::uint32_t negHi) {
    RdnaOperand* sources[] = {&instruction.source0, &instruction.source1, &instruction.source2};
    for (std::uint32_t i = 0; i < 3u; ++i) {
        sources[i]->opSel = ((opSel >> i) & 1u) != 0u;
        sources[i]->opSelHi = ((opSelHi >> i) & 1u) != 0u;
        sources[i]->negate = ((neg >> i) & 1u) != 0u;
        sources[i]->negateHi = ((negHi >> i) & 1u) != 0u;
    }
}

void applyVop3pMixAbsModifiers(RdnaInstruction& instruction) {
    RdnaOperand* sources[] = {&instruction.source0, &instruction.source1, &instruction.source2};
    for (auto* source : sources) {
        source->absolute = source->negateHi;
        source->negateHi = false;
    }
}

void applyNativeVop3TernaryModifiers(RdnaInstruction& instruction, std::uint32_t opSel, std::uint32_t abs, std::uint32_t neg) {
    RdnaOperand* sources[] = {&instruction.source0, &instruction.source1, &instruction.source2};
    for (std::uint32_t i = 0; i < 3u; ++i) {
        sources[i]->opSel = ((opSel >> i) & 1u) != 0u;
        sources[i]->opSelHi = true;
        sources[i]->negate = ((neg >> i) & 1u) != 0u;
        sources[i]->absolute = ((abs >> i) & 1u) != 0u;
    }
    instruction.destination.sdwaSel = (opSel & 0x8u) != 0u ? 5u : 4u;
}

void applyNativeVop3I16TernarySelectors(RdnaInstruction& instruction, std::uint32_t opSel) {
    RdnaOperand* sources[] = {&instruction.source0, &instruction.source1, &instruction.source2};
    const bool wideResult = instruction.op == RdnaOpcode::VMadU32U16 || instruction.op == RdnaOpcode::VMadI32I16;
    for (std::uint32_t i = 0; i < (wideResult ? 2u : 3u); ++i) {
        sources[i]->opSel = ((opSel >> i) & 1u) != 0u;
    }
    if (!wideResult) {
        instruction.destination.sdwaSel = (opSel & 0x8u) != 0u ? 5u : 4u;
    }
}

void applyNativeVop3B16BinaryModifiers(RdnaInstruction& instruction, std::uint32_t opSel) {
    instruction.source0.opSel = (opSel & 0x1u) != 0u;
    instruction.source1.opSel = (opSel & 0x2u) != 0u;
    instruction.destination.sdwaSel = (opSel & 0x8u) != 0u ? 5u : 4u;
}

void applyNativeVop3PackB32F16Modifiers(RdnaInstruction& instruction, std::uint32_t opSel, std::uint32_t abs, std::uint32_t neg) {
    instruction.source0.opSel = (opSel & 0x1u) != 0u;
    instruction.source1.opSel = (opSel & 0x2u) != 0u;
    instruction.source0.negate = (neg & 0x1u) != 0u;
    instruction.source1.negate = (neg & 0x2u) != 0u;
    instruction.source0.absolute = (abs & 0x1u) != 0u;
    instruction.source1.absolute = (abs & 0x2u) != 0u;
}

bool supportsNativeVop3SourceModifiers(RdnaOpcode opcode) {
    if (isVop1FloatSourceOpcode(opcode) || isVopcFloatCompareOpcode(opcode) || isNativeVop3F16TernaryOpcode(opcode)) {
        return true;
    }
    switch (opcode) {
        case RdnaOpcode::VMulLegacyF32:
        case RdnaOpcode::VMacLegacyF32:
        case RdnaOpcode::VCndmaskB32:
        case RdnaOpcode::VAddF32:
        case RdnaOpcode::VSubF32:
        case RdnaOpcode::VSubrevF32:
        case RdnaOpcode::VMulF32:
        case RdnaOpcode::VMinF32:
        case RdnaOpcode::VMaxF32:
        case RdnaOpcode::VAddF16:
        case RdnaOpcode::VSubF16:
        case RdnaOpcode::VSubrevF16:
        case RdnaOpcode::VMulF16:
        case RdnaOpcode::VMinF16:
        case RdnaOpcode::VMaxF16:
        case RdnaOpcode::VMacF32:
        case RdnaOpcode::VMadF32:
        case RdnaOpcode::VFmaF32:
        case RdnaOpcode::VPackB32F16:
        case RdnaOpcode::VCubeidF32:
        case RdnaOpcode::VCubescF32:
        case RdnaOpcode::VCubetcF32:
        case RdnaOpcode::VCubemaF32:
        case RdnaOpcode::VCvtPkrtzF16F32:
        case RdnaOpcode::VCvtPknormI16F32:
        case RdnaOpcode::VCvtPknormU16F32:
        case RdnaOpcode::VMin3F32:
        case RdnaOpcode::VMax3F32:
        case RdnaOpcode::VMed3F32:
        case RdnaOpcode::VLdexpF32:
        case RdnaOpcode::VFmaF64:
        case RdnaOpcode::VAddF64:
        case RdnaOpcode::VMulF64:
        case RdnaOpcode::VMinF64:
        case RdnaOpcode::VMaxF64:
        case RdnaOpcode::VLdexpF64:
        case RdnaOpcode::VTrigPreopF64: return true;
        default: return false;
    }
}

bool supportsNativeVop3ResultModifiers(RdnaOpcode opcode) {
    if (isVop1FloatResultOpcode(opcode)) {
        return true;
    }
    switch (opcode) {
        case RdnaOpcode::VAddF32:
        case RdnaOpcode::VSubF32:
        case RdnaOpcode::VSubrevF32:
        case RdnaOpcode::VMulF32:
        case RdnaOpcode::VMinF32:
        case RdnaOpcode::VMaxF32:
        case RdnaOpcode::VMacF32:
        case RdnaOpcode::VMadF32:
        case RdnaOpcode::VFmaF32:
        case RdnaOpcode::VFmaF16:
        case RdnaOpcode::VCvtPkrtzF16F32:
        case RdnaOpcode::VLdexpF32:
        case RdnaOpcode::VFmaF64:
        case RdnaOpcode::VAddF64:
        case RdnaOpcode::VMulF64:
        case RdnaOpcode::VMinF64:
        case RdnaOpcode::VMaxF64:
        case RdnaOpcode::VLdexpF64:
        case RdnaOpcode::VTrigPreopF64:
        case RdnaOpcode::VMin3F32:
        case RdnaOpcode::VMax3F32:
        case RdnaOpcode::VMed3F32: return true;
        default: return false;
    }
}

bool supportsNativeVop3Clamp(RdnaOpcode opcode) {
    return supportsNativeVop3ResultModifiers(opcode) || usesInexactClampControl(opcode) || usesSignedSaturateClamp(opcode);
}

void checkNativeVop3Modifiers(RdnaOpcode opcode, bool permlane, bool carryInOut, bool scalarDst, std::uint32_t abs, std::uint32_t opSel, std::uint32_t clamp, std::uint32_t omod, std::uint32_t neg) {
    if (permlane) {
        if (abs != 0u || (opSel & ~0x3u) != 0u || clamp != 0u || omod != 0u || neg != 0u) {
            throw std::invalid_argument("VOP3 perm-lane op_sel bits are not implemented");
        }
        return;
    }
    if (isNativeVop3F16TernaryOpcode(opcode)) {
        if (opcode != RdnaOpcode::VFmaF16 && (clamp != 0u || omod != 0u)) {
            throw std::invalid_argument("VOP3 source modifiers are not implemented");
        }
        return;
    }
    if (isNativeVop3I16TernaryOpcode(opcode)) {
        if (abs != 0u || clamp != 0u || omod != 0u || neg != 0u) {
            throw std::invalid_argument("VOP3 source modifiers are not implemented");
        }
        return;
    }
    if (isNativeVop3B16BinaryOpcode(opcode)) {
        if (abs != 0u || clamp != 0u || omod != 0u || neg != 0u) {
            throw std::invalid_argument("VOP3 source modifiers are not implemented");
        }
        return;
    }
    if (carryInOut || scalarDst) {
        const bool floatCompare = scalarDst && isVopcFloatCompareOpcode(opcode);
        if (clamp != 0u || omod != 0u || (neg != 0u && !floatCompare)) {
            throw std::invalid_argument("VOP3 source modifiers are not implemented");
        }
        return;
    }
    if (usesSignedSaturateClamp(opcode)) {
        if (abs != 0u || opSel != 0u || omod != 0u || neg != 0u) {
            throw std::invalid_argument("VOP3 source modifiers are not implemented");
        }
        return;
    }
    if (opcode == RdnaOpcode::VLdexpF32 || opcode == RdnaOpcode::VLdexpF64 || opcode == RdnaOpcode::VTrigPreopF64) {
        if ((abs & ~1u) != 0u || opSel != 0u || (neg & ~1u) != 0u) {
            throw std::invalid_argument("VOP3 source modifiers are not implemented");
        }
        return;
    }
    if (opcode == RdnaOpcode::VCndmaskB32) {
        if ((abs & ~0x3u) != 0u || opSel != 0u || clamp != 0u || omod != 0u || (neg & ~0x3u) != 0u) {
            throw std::invalid_argument("VOP3 source modifiers are not implemented");
        }
        return;
    }
    if (opcode == RdnaOpcode::VPackB32F16) {
        if ((abs & ~0x3u) != 0u || (opSel & ~0x3u) != 0u || clamp != 0u || omod != 0u || (neg & ~0x3u) != 0u) {
            throw std::invalid_argument("VOP3 source modifiers are not implemented");
        }
        return;
    }
    const bool sourceModifiers = supportsNativeVop3SourceModifiers(opcode);
    const bool resultModifiers = supportsNativeVop3ResultModifiers(opcode);
    const bool clampModifier = supportsNativeVop3Clamp(opcode);
    if (sourceModifiers) {
        if (opSel != 0u || (omod != 0u && !resultModifiers) || (clamp != 0u && !clampModifier)) {
            throw std::invalid_argument("VOP3 source modifiers are not implemented");
        }
        return;
    }
    if (resultModifiers) {
        if (abs != 0u || opSel != 0u || neg != 0u) {
            throw std::invalid_argument("VOP3 source modifiers are not implemented");
        }
        return;
    }
    if (abs != 0u || opSel != 0u || clamp != 0u || omod != 0u || neg != 0u) {
        throw std::invalid_argument("VOP3 source modifiers are not implemented");
    }
}

void applyNativeVop3SourceModifiers(RdnaInstruction& instruction, std::uint32_t abs, std::uint32_t neg) {
    RdnaOperand* sources[] = {&instruction.source0, &instruction.source1, &instruction.source2};
    for (std::uint32_t i = 0; i < instruction.sourceCount && i < 3u; ++i) {
        sources[i]->absolute = ((abs >> i) & 1u) != 0u;
        sources[i]->negate = ((neg >> i) & 1u) != 0u;
    }
}

}

RdnaInstruction DecodeRdnaVectorOp(std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    const auto programCounter = toProgramCounter(wordIndex);
    if (static_cast<std::size_t>(wordIndex) >= code.size()) {
        throw std::out_of_range("truncated vector instruction");
    }
    const auto word = code[wordIndex];
    if ((word & 0x80000000u) == 0u) {
        switch ((word >> 25u) & 0x3fu) {
            case 0x3eu: return DecodeRdnaVopc(programCounter, code, wordIndex);
            case 0x3fu: return DecodeRdnaVop1(programCounter, code, wordIndex);
            default: return DecodeRdnaVop2(programCounter, code, wordIndex);
        }
    }
    if ((word & 0xc0000000u) == 0xc0000000u) {
        switch (word >> 26u) {
            case 0x32u: return DecodeRdnaVintrp(programCounter, code, wordIndex);
            case 0x33u: return DecodeRdnaVop3p(programCounter, code, wordIndex);
            case 0x35u: return DecodeRdnaVop3(programCounter, code, wordIndex);
            default: throw std::invalid_argument("instruction is not a vector operation");
        }
    }
    throw std::invalid_argument("instruction is not a vector operation");
}

RdnaInstruction DecodeRdnaVop1(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    const auto word = code[wordIndex];
    const auto opcode = (word >> 9u) & 0xffu;
    const auto src0 = word & 0x1ffu;
    const auto vdst = (word >> 17u) & 0xffu;

    RdnaInstruction instruction;
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::VOP1;
    instruction.opcodeId = opcode;
    instruction.op = lookupVectorOpcode(vop1Opcodes, opcode, "VOP1 opcode is not implemented");
    SetRdnaRawWords(instruction, code, wordIndex, 1);

    if (isVop1WithoutOperands(instruction.op)) {
        instruction.destination.kind = RdnaOperandKind::Null;
        instruction.sourceCount = 0;
        return instruction;
    }
    if (src0 == 249u) {
        decodeVop1Sdwa(programCounter, code, wordIndex, vdst, instruction);
        return instruction;
    }
    if (src0 == 250u) {
        decodeVop1Dpp(programCounter, code, wordIndex, vdst, instruction);
        return instruction;
    }

    instruction.destination = usesScalarDestination(instruction.op)
        ? DecodeRdnaScalarDestination(vdst, programCounter)
        : DecodeRdnaVectorGpr(vdst);
    instruction.source0 = DecodeRdnaScalarSource(src0, programCounter);
    instruction.sourceCount = 1;
    ReadRdnaLiteralOperands(code, wordIndex, instruction);
    return instruction;
}

RdnaInstruction DecodeRdnaVop2(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    const auto word = code[wordIndex];
    const auto opcode = (word >> 25u) & 0x3fu;
    const auto vdst = (word >> 17u) & 0xffu;
    const auto src0 = word & 0x1ffu;
    const auto vsrc1 = (word >> 9u) & 0xffu;

    RdnaInstruction instruction;
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::VOP2;
    instruction.opcodeId = opcode;
    instruction.op = lookupVectorOpcode(vop2Opcodes, opcode, "VOP2 opcode is not implemented");
    SetRdnaRawWords(instruction, code, wordIndex, 1);

    if (src0 == 249u) {
        decodeVop2Sdwa(programCounter, code, wordIndex, opcode, vdst, vsrc1, instruction);
        return instruction;
    }
    if (src0 == 250u) {
        decodeVop2Dpp(programCounter, code, wordIndex, opcode, vdst, vsrc1, instruction);
        return instruction;
    }

    instruction.destination = DecodeRdnaVectorGpr(vdst);
    instruction.source1 = DecodeRdnaVectorGpr(vsrc1);
    instruction.source0 = DecodeRdnaScalarSource(src0, programCounter);
    finalizeVop2Instruction(code, wordIndex, instruction);
    return instruction;
}

RdnaInstruction DecodeRdnaVopc(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    const auto word = code[wordIndex];
    const auto opcode = (word >> 17u) & 0xffu;
    const auto src0 = word & 0x1ffu;
    const auto vsrc1 = (word >> 9u) & 0xffu;

    RdnaInstruction instruction;
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::VOPC;
    instruction.opcodeId = opcode;
    SetRdnaRawWords(instruction, code, wordIndex, 1);

    if (src0 == 249u) {
        instruction.op = lookupVectorOpcode(vopcOpcodes, opcode, "VOPC SDWA modifier is not supported for opcode");
        if (isVopcFloat64CompareOpcode(instruction.op)) {
            throw std::invalid_argument("VOPC SDWA modifier is not supported for opcode");
        }
        instruction.destination.kind = isVopcCompareExec(instruction.op) ? RdnaOperandKind::ExecLo : RdnaOperandKind::VccLo;
        decodeVopcSdwa(programCounter, code, wordIndex, vsrc1, instruction);
        return instruction;
    }
    instruction.op = lookupVectorOpcode(vopcOpcodes, opcode, "VOPC opcode is not implemented");
    instruction.destination.kind = isVopcCompareExec(instruction.op) ? RdnaOperandKind::ExecLo : RdnaOperandKind::VccLo;
    if (src0 == 250u) {
        decodeVopcDpp(programCounter, code, wordIndex, opcode, vsrc1, instruction);
        return instruction;
    }

    instruction.source1 = DecodeRdnaVectorGpr(vsrc1);
    instruction.source0 = DecodeRdnaScalarSource(src0, programCounter);
    instruction.sourceCount = 2;
    ReadRdnaLiteralOperands(code, wordIndex, instruction);
    return instruction;
}

RdnaInstruction DecodeRdnaVop3(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    if (static_cast<std::size_t>(wordIndex) + 1u >= code.size()) {
        throw std::out_of_range("truncated VOP3 instruction");
    }
    const auto word0 = code[wordIndex];
    const auto word1 = code[wordIndex + 1u];
    const auto opcode = (word0 >> 16u) & 0x3ffu;
    const auto vdst = word0 & 0xffu;
    const auto sdst = (word0 >> 8u) & 0x7fu;
    const auto src0 = word1 & 0x1ffu;
    const auto src1 = (word1 >> 9u) & 0x1ffu;
    const auto src2 = (word1 >> 18u) & 0x1ffu;
    const auto abs = (word0 >> 8u) & 0x7u;
    const auto opSel = (word0 >> 11u) & 0xfu;
    const auto clamp = (word0 >> 15u) & 0x1u;
    const auto omod = (word1 >> 27u) & 0x3u;
    const auto neg = (word1 >> 29u) & 0x7u;

    RdnaInstruction instruction;
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::VOP3;
    instruction.opcodeId = opcode;
    instruction.op = lookupVop3Opcode(opcode);
    SetRdnaRawWords(instruction, code, wordIndex, 2);

    const bool carryInOut = (instruction.op == RdnaOpcode::VAddcU32 && opcode == 0x128u) ||
        (instruction.op == RdnaOpcode::VSubCoCiU32 && opcode == 0x129u) ||
        (instruction.op == RdnaOpcode::VSubrevCoCiU32 && opcode == 0x12au);
    const bool vop3bCarryOut = isVop3BCarryOutOpcode(instruction.op);
    const bool vop3bMadU64 = isVop3BMadU64Opcode(instruction.op);
    const bool vop3bUsesSdst = carryInOut || vop3bCarryOut || vop3bMadU64;
    const bool permlane = isPermlaneOpcode(instruction.op);
    const bool vop3Vopc = isVop3EncodedVopc(opcode);
    const bool compareExec = vop3Vopc && isVopcCompareExec(instruction.op);
    const bool scalarDst = vop3Vopc || usesScalarDestination(instruction.op);
    const bool nativeSourceModifiers = supportsNativeVop3SourceModifiers(instruction.op);
    const bool nativeResultModifiers = supportsNativeVop3ResultModifiers(instruction.op);

    checkNativeVop3Modifiers(instruction.op, permlane, vop3bUsesSdst, scalarDst, abs, opSel, clamp, omod, neg);

    if (isVop1WithoutOperands(instruction.op)) {
        instruction.destination.kind = RdnaOperandKind::Null;
        instruction.sourceCount = 0;
        return instruction;
    }
    if (compareExec) {
        instruction.destination.kind = RdnaOperandKind::ExecLo;
    } else if (scalarDst) {
        instruction.destination = DecodeRdnaScalarDestination(vdst, programCounter);
    } else {
        instruction.destination = DecodeRdnaVectorGpr(vdst);
    }
    instruction.source0 = DecodeRdnaScalarSource(src0, programCounter);
    instruction.destination.clamp = supportsNativeVop3Clamp(instruction.op) && clamp != 0u;
    instruction.destination.omod = nativeResultModifiers ? omod : 0u;
    if (permlane) {
        instruction.destination.opSel = (opSel & 0x1u) != 0u;
        instruction.destination.opSelHi = (opSel & 0x2u) != 0u;
    }
    if (vop3Vopc) {
        instruction.source1 = DecodeRdnaScalarSource(src1, programCounter);
        instruction.sourceCount = 2;
        if (nativeSourceModifiers) {
            applyNativeVop3SourceModifiers(instruction, abs, neg);
        }
        ReadRdnaLiteralOperands(code, wordIndex, instruction);
        return instruction;
    }
    if (isVop3EncodedVop1(opcode)) {
        instruction.sourceCount = 1;
        if (nativeSourceModifiers) {
            applyNativeVop3SourceModifiers(instruction, abs, neg);
        }
        ReadRdnaLiteralOperands(code, wordIndex, instruction);
        return instruction;
    }
    if (carryInOut) {
        instruction.destination2 = DecodeRdnaScalarDestination(sdst, programCounter);
        instruction.source1 = DecodeRdnaScalarSource(src1, programCounter);
        instruction.source2 = DecodeRdnaScalarSource(src2, programCounter);
        instruction.sourceCount = 3;
        ReadRdnaLiteralOperands(code, wordIndex, instruction);
        return instruction;
    }
    if (vop3bCarryOut) {
        instruction.destination2 = DecodeRdnaScalarDestination(sdst, programCounter);
        instruction.source1 = DecodeRdnaScalarSource(src1, programCounter);
        instruction.sourceCount = 2;
        ReadRdnaLiteralOperands(code, wordIndex, instruction);
        return instruction;
    }
    if (vop3bMadU64) {
        instruction.destination2 = DecodeRdnaScalarDestination(sdst, programCounter);
        instruction.source1 = DecodeRdnaScalarSource(src1, programCounter);
        instruction.source2 = DecodeRdnaScalarSource(src2, programCounter);
        instruction.sourceCount = 3;
        ReadRdnaLiteralOperands(code, wordIndex, instruction);
        return instruction;
    }
    if (isVop3EncodedVop2(opcode)) {
        instruction.source1 = DecodeRdnaScalarSource(src1, programCounter);
        if (instruction.op == RdnaOpcode::VCndmaskB32) {
            instruction.source2 = DecodeRdnaScalarSource(src2, programCounter);
            instruction.sourceCount = 3;
        } else {
            instruction.sourceCount = 2;
        }
        if (nativeSourceModifiers) {
            applyNativeVop3SourceModifiers(instruction, abs, neg);
        }
        ReadRdnaLiteralOperands(code, wordIndex, instruction);
        return instruction;
    }
    instruction.source1 = DecodeRdnaScalarSource(src1, programCounter);
    instruction.sourceCount = nativeVop3SourceCount(instruction.op);
    if (instruction.sourceCount > 2u) {
        instruction.source2 = DecodeRdnaScalarSource(src2, programCounter);
    }
    if (isNativeVop3F16TernaryOpcode(instruction.op)) {
        applyNativeVop3TernaryModifiers(instruction, opSel, abs, neg);
    } else if (isNativeVop3I16TernaryOpcode(instruction.op)) {
        applyNativeVop3I16TernarySelectors(instruction, opSel);
    } else if (isNativeVop3B16BinaryOpcode(instruction.op)) {
        applyNativeVop3B16BinaryModifiers(instruction, opSel);
    } else if (instruction.op == RdnaOpcode::VPackB32F16) {
        applyNativeVop3PackB32F16Modifiers(instruction, opSel, abs, neg);
    } else if (nativeSourceModifiers) {
        applyNativeVop3SourceModifiers(instruction, abs, neg);
    }
    ReadRdnaLiteralOperands(code, wordIndex, instruction);
    return instruction;
}

RdnaInstruction DecodeRdnaVop3p(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    if (static_cast<std::size_t>(wordIndex) + 1u >= code.size()) {
        throw std::out_of_range("truncated VOP3P instruction");
    }
    const auto word0 = code[wordIndex];
    const auto word1 = code[wordIndex + 1u];
    const auto opcode = (word0 >> 16u) & 0x7fu;
    const auto vdst = word0 & 0xffu;
    const auto negHi = (word0 >> 8u) & 0x7u;
    const auto opSel = (word0 >> 11u) & 0x7u;
    const auto opSelHi2 = (word0 >> 14u) & 0x1u;
    const auto clamp = (word0 >> 15u) & 0x1u;
    const auto src0 = word1 & 0x1ffu;
    const auto src1 = (word1 >> 9u) & 0x1ffu;
    const auto src2 = (word1 >> 18u) & 0x1ffu;
    const auto opSelHi = ((word1 >> 27u) & 0x3u) | (opSelHi2 << 2u);
    const auto neg = (word1 >> 29u) & 0x7u;

    RdnaInstruction instruction;
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::VOP3P;
    instruction.opcodeId = opcode;
    instruction.op = lookupVectorOpcode(vop3pOpcodes, opcode, "VOP3P opcode is not implemented");
    SetRdnaRawWords(instruction, code, wordIndex, 2);

    instruction.sourceCount = vop3pSourceCount(instruction.op);
    instruction.destination = DecodeRdnaVectorGpr(vdst);
    instruction.source0 = DecodeRdnaScalarSource(src0, programCounter);
    instruction.source1 = DecodeRdnaScalarSource(src1, programCounter);
    if (instruction.sourceCount > 2u) {
        instruction.source2 = DecodeRdnaScalarSource(src2, programCounter);
    }
    if (instruction.op == RdnaOpcode::VFmaF32 || isMadMixF16(instruction.op) || isPackedVop3p(instruction.op)) {
        instruction.destination.clamp = clamp != 0u;
    } else if (clamp != 0u) {
        throw std::invalid_argument("VOP3P integer clamp is not implemented");
    }
    applyVop3pSourceModifiers(instruction, opSel, opSelHi, neg, negHi);
    if (instruction.op == RdnaOpcode::VMadMixloF16) {
        applyVop3pMixAbsModifiers(instruction);
        instruction.destination.sdwaSel = 4;
    } else if (instruction.op == RdnaOpcode::VMadMixhiF16) {
        applyVop3pMixAbsModifiers(instruction);
        instruction.destination.sdwaSel = 5;
    } else if (instruction.op == RdnaOpcode::VFmaF32) {
        applyVop3pMixAbsModifiers(instruction);
    }
    ReadRdnaLiteralOperands(code, wordIndex, instruction);
    return instruction;
}

RdnaInstruction DecodeRdnaVintrp(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    const auto word = code[wordIndex];
    const auto opcode = (word >> 16u) & 0x3u;
    const auto vdst = (word >> 18u) & 0xffu;
    const auto attr = (word >> 10u) & 0x3fu;
    const auto chan = (word >> 8u) & 0x3u;
    const auto vsrc = word & 0xffu;

    RdnaInstruction instruction;
    instruction.programCounter = programCounter;
    instruction.wordCount = 1;
    instruction.family = RdnaInstructionFamily::VINTRP;
    instruction.opcodeId = opcode;
    instruction.op = lookupVintrpOpcode(opcode);
    SetRdnaRawWords(instruction, code, wordIndex, 1);

    instruction.destination = DecodeRdnaVectorGpr(vdst);
    if (instruction.op == RdnaOpcode::VInterpMovF32) {
        instruction.source0.kind = RdnaOperandKind::IntegerInlineConstant;
        instruction.source0.value = vsrc & 0x3u;
        instruction.source0.signedVal = static_cast<std::int32_t>(instruction.source0.value);
    } else {
        instruction.source0 = DecodeRdnaVectorGpr(vsrc);
    }
    instruction.source1.kind = RdnaOperandKind::IntegerInlineConstant;
    instruction.source1.value = attr;
    instruction.source1.signedVal = static_cast<std::int32_t>(attr);
    instruction.source2.kind = RdnaOperandKind::IntegerInlineConstant;
    instruction.source2.value = chan;
    instruction.source2.signedVal = static_cast<std::int32_t>(chan);
    instruction.sourceCount = 3;
    return instruction;
}

}
