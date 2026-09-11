#ifndef CODEGEN_X86_X64OPCODECONSTANTS_HPP
#define CODEGEN_X86_X64OPCODECONSTANTS_HPP

#include <cstdint>

namespace Codegen::X64OpcodeConstants {

inline constexpr std::uint8_t PrefixLock = 0xF0;
inline constexpr std::uint8_t PrefixRepne = 0xF2;
inline constexpr std::uint8_t PrefixRep = 0xF3;
inline constexpr std::uint8_t PrefixSegCs = 0x2E;
inline constexpr std::uint8_t PrefixSegSs = 0x36;
inline constexpr std::uint8_t PrefixSegDs = 0x3E;
inline constexpr std::uint8_t PrefixSegEs = 0x26;
inline constexpr std::uint8_t PrefixSegFs = 0x64;
inline constexpr std::uint8_t PrefixSegGs = 0x65;
inline constexpr std::uint8_t PrefixOperandSize = 0x66;
inline constexpr std::uint8_t PrefixAddressSize = 0x67;

inline constexpr std::uint8_t RexMin = 0x40;
inline constexpr std::uint8_t RexMax = 0x4F;
inline constexpr std::uint8_t RexWBit = 0x08;

inline constexpr std::uint8_t TwoByteOpcodeEscape = 0x0F;

inline constexpr std::uint8_t OneByteModRmRangeAMin = 0x00;
inline constexpr std::uint8_t OneByteModRmRangeAMax = 0x03;
inline constexpr std::uint8_t OneByteModRmRangeBMin = 0x08;
inline constexpr std::uint8_t OneByteModRmRangeBMax = 0x0B;
inline constexpr std::uint8_t OneByteModRmRangeCMin = 0x10;
inline constexpr std::uint8_t OneByteModRmRangeCMax = 0x13;
inline constexpr std::uint8_t OneByteModRmRangeDMin = 0x18;
inline constexpr std::uint8_t OneByteModRmRangeDMax = 0x1B;
inline constexpr std::uint8_t OneByteModRmRangeEMin = 0x20;
inline constexpr std::uint8_t OneByteModRmRangeEMax = 0x23;
inline constexpr std::uint8_t OneByteModRmRangeFMin = 0x28;
inline constexpr std::uint8_t OneByteModRmRangeFMax = 0x2B;
inline constexpr std::uint8_t OneByteModRmRangeGMin = 0x30;
inline constexpr std::uint8_t OneByteModRmRangeGMax = 0x33;
inline constexpr std::uint8_t OneByteModRmRangeHMin = 0x38;
inline constexpr std::uint8_t OneByteModRmRangeHMax = 0x3B;
inline constexpr std::uint8_t OneByteArpl = 0x62;
inline constexpr std::uint8_t OneByteMovsxd = 0x63;
inline constexpr std::uint8_t OneByteModRmRangeIMin = 0x84;
inline constexpr std::uint8_t OneByteModRmRangeIMax = 0x8F;
inline constexpr std::uint8_t OneByteGrp2Rm8Imm8 = 0xC0;
inline constexpr std::uint8_t OneByteGrp2RmImm8 = 0xC1;
inline constexpr std::uint8_t OneByteVex3 = 0xC4;
inline constexpr std::uint8_t OneByteVex2 = 0xC5;
inline constexpr std::uint8_t OneByteMovImm8 = 0xC6;
inline constexpr std::uint8_t OneByteMovImm32 = 0xC7;
inline constexpr std::uint8_t OneByteShiftRm8By1 = 0xD0;
inline constexpr std::uint8_t OneByteShiftRmBy1 = 0xD1;
inline constexpr std::uint8_t OneByteShiftRm8ByCl = 0xD2;
inline constexpr std::uint8_t OneByteShiftRmByCl = 0xD3;
inline constexpr std::uint8_t OneByteTestGrp3Rm8 = 0xF6;
inline constexpr std::uint8_t OneByteTestGrp3Rm = 0xF7;
inline constexpr std::uint8_t OneByteIncDecRm8 = 0xFE;
inline constexpr std::uint8_t OneByteGrp5Rm = 0xFF;
inline constexpr std::uint8_t OneByteModRmRangeJMin = 0x88;
inline constexpr std::uint8_t OneByteModRmRangeJMax = 0x8B;
inline constexpr std::uint8_t OneByteImulRm32Imm32 = 0x69;
inline constexpr std::uint8_t OneByteImulRm32Imm8 = 0x6B;
inline constexpr std::uint8_t OneByteX87Min = 0xD8;
inline constexpr std::uint8_t OneByteX87Max = 0xDF;

inline constexpr std::uint8_t OneByteImm8AluOrGrp3 = 0xC6;
inline constexpr std::uint8_t OneByteImm8AluCmp = 0x80;
inline constexpr std::uint8_t OneByteImm8AddAl = 0x04;
inline constexpr std::uint8_t OneByteImm8OrAl = 0x0C;
inline constexpr std::uint8_t OneByteImm8AdcAl = 0x14;
inline constexpr std::uint8_t OneByteImm8SbbAl = 0x1C;
inline constexpr std::uint8_t OneByteImm8AndAl = 0x24;
inline constexpr std::uint8_t OneByteImm8SubAl = 0x2C;
inline constexpr std::uint8_t OneByteImm8XorAl = 0x34;
inline constexpr std::uint8_t OneByteImm8CmpAl = 0x3C;
inline constexpr std::uint8_t OneByteMovImm8RegMin = 0xB0;
inline constexpr std::uint8_t OneByteMovImm8RegMax = 0xB7;
inline constexpr std::uint8_t OneBytePushImm8 = 0x6A;
inline constexpr std::uint8_t OneByteTestAlImm8 = 0xA8;

inline constexpr std::uint8_t OneByteImm32AluCmp = 0x81;
inline constexpr std::uint8_t OneByteImm32AddEax = 0x05;
inline constexpr std::uint8_t OneByteImm32OrEax = 0x0D;
inline constexpr std::uint8_t OneByteImm32AdcEax = 0x15;
inline constexpr std::uint8_t OneByteImm32SbbEax = 0x1D;
inline constexpr std::uint8_t OneByteImm32AndEax = 0x25;
inline constexpr std::uint8_t OneByteImm32SubEax = 0x2D;
inline constexpr std::uint8_t OneByteImm32XorEax = 0x35;
inline constexpr std::uint8_t OneByteImm32CmpEax = 0x3D;
inline constexpr std::uint8_t OneBytePushImm32 = 0x68;
inline constexpr std::uint8_t OneByteTestEaxImm32 = 0xA9;
inline constexpr std::uint8_t OneByteMovImm32RegMin = 0xB8;
inline constexpr std::uint8_t OneByteMovImm32RegMax = 0xBF;

inline constexpr std::uint8_t OneByteImm8Grp1 = 0x83;

inline constexpr std::uint8_t OneByteJccRel8Min = 0x70;
inline constexpr std::uint8_t OneByteJccRel8Max = 0x7F;

inline constexpr std::uint8_t OneByteCallRel32 = 0xE8;
inline constexpr std::uint8_t OneByteJmpRel32 = 0xE9;
inline constexpr std::uint8_t OneByteJmpRel8 = 0xEB;
inline constexpr std::uint8_t OneByteJrcxz = 0xE3;
inline constexpr std::uint8_t OneByteLoop = 0xE0;
inline constexpr std::uint8_t OneByteLoopMax = 0xE2;
inline constexpr std::uint8_t OneByteInOutImm8Min = 0xE4;
inline constexpr std::uint8_t OneByteInOutImm8Max = 0xE7;

inline constexpr std::uint8_t TwoByteJccRel32Min = 0x80;
inline constexpr std::uint8_t TwoByteJccRel32Max = 0x8F;
inline constexpr std::uint8_t TwoByteMovImm8ModRm = 0xBA;
inline constexpr std::uint8_t TwoByteCmovRangeMin = 0x40;
inline constexpr std::uint8_t TwoByteCmovRangeMax = 0x4F;
inline constexpr std::uint8_t TwoByteModRmRangeAMin = 0xA3;
inline constexpr std::uint8_t TwoByteModRmRangeAMax = 0xAB;
inline constexpr std::uint8_t TwoByteModRmRangeBMin = 0xB0;
inline constexpr std::uint8_t TwoByteModRmRangeBMax = 0xB7;
inline constexpr std::uint8_t TwoByteModRmRangeCMin = 0xBC;
inline constexpr std::uint8_t TwoByteModRmRangeCMax = 0xBF;
inline constexpr std::uint8_t TwoByteModRmRangeDMin = 0x10;
inline constexpr std::uint8_t TwoByteModRmRangeDMax = 0x17;
inline constexpr std::uint8_t TwoByteModRmRangeEMin = 0x28;
inline constexpr std::uint8_t TwoByteModRmRangeEMax = 0x2F;
inline constexpr std::uint8_t TwoByteModRmRangeFMin = 0x38;
inline constexpr std::uint8_t TwoByteModRmRangeFMax = 0x3A;
inline constexpr std::uint8_t TwoByteModRmRangeGMin = 0x54;
inline constexpr std::uint8_t TwoByteModRmRangeGMax = 0x7F;
inline constexpr std::uint8_t TwoByteModRmRangeHMin = 0xD0;
inline constexpr std::uint8_t TwoByteModRmRangeHMax = 0xFE;
inline constexpr std::uint8_t TwoByteNopModRm = 0x1F;
inline constexpr std::uint8_t TwoByteEndbr = 0x1E;
inline constexpr std::uint8_t TwoBytePrefetchGrpMin = 0x18;
inline constexpr std::uint8_t TwoBytePrefetchGrpMax = 0x1D;
inline constexpr std::uint8_t TwoByteSetccMin = 0x90;
inline constexpr std::uint8_t TwoByteSetccMax = 0x9F;
inline constexpr std::uint8_t TwoByteImulRmModRm = 0xAF;
inline constexpr std::uint8_t TwoByteMovdMovq = 0x7E;
inline constexpr std::uint8_t TwoByteMovqMmxXmm = 0x6F;
inline constexpr std::uint8_t TwoByteMovdqu = 0x7F;
inline constexpr std::uint8_t TwoByteGrp7 = 0x01;
inline constexpr std::uint8_t TwoByteShiftImm8Min = 0x70;
inline constexpr std::uint8_t TwoByteShiftImm8Max = 0x73;
inline constexpr std::uint8_t ThreeByteEscape38 = 0x38;
inline constexpr std::uint8_t ThreeByteEscape3A = 0x3A;

inline constexpr std::uint8_t TwoByteModRmRangeIMin = 0x50;
inline constexpr std::uint8_t TwoByteModRmRangeIMax = 0x53;
inline constexpr std::uint8_t TwoByteShldImm8 = 0xA4;
inline constexpr std::uint8_t TwoByteShldCl = 0xA5;
inline constexpr std::uint8_t TwoByteShrdImm8 = 0xAC;
inline constexpr std::uint8_t TwoByteShrdCl = 0xAD;
inline constexpr std::uint8_t TwoByteGrp15 = 0xAE;
inline constexpr std::uint8_t TwoByteXadd = 0xC1;
inline constexpr std::uint8_t TwoBytePextrw = 0xC5;
inline constexpr std::uint8_t TwoByteShufpsImm8 = 0xC2;
inline constexpr std::uint8_t TwoByteShufpdImm8 = 0xC6;
inline constexpr std::uint8_t TwoByteGrp9 = 0xC7;
inline constexpr std::uint8_t TwoByteExtrqInsertqImm8Imm8 = 0x78;
inline constexpr std::uint8_t TwoByteExtrqInsertqModRm = 0x79;

inline constexpr std::uint8_t VexNoModRmMin = 0x77;
inline constexpr std::uint8_t VexNoModRmMax = 0x77;

inline constexpr std::uint8_t EvexPrefix = 0x62;
inline constexpr std::size_t EvexPrefixLength = 4;
inline constexpr std::uint8_t EvexMapMask = 0x03;
inline constexpr std::uint8_t EvexMap0F3A = 0x03;
inline constexpr std::uint8_t EvexMap0F38 = 0x02;

inline constexpr std::size_t ImmSizeNone = 0;
inline constexpr std::size_t ImmSize8 = 1;
inline constexpr std::size_t ImmSize16 = 2;
inline constexpr std::size_t ImmSize32 = 4;
inline constexpr std::size_t ImmSize64 = 8;

inline constexpr std::uint8_t ModRmModShift = 6;
inline constexpr std::uint8_t ModRmModMask = 0x03;
inline constexpr std::uint8_t ModRmRegShift = 3;
inline constexpr std::uint8_t ModRmRegMask = 0x07;
inline constexpr std::uint8_t ModRmRmMask = 0x07;
inline constexpr std::uint8_t ModRmModIndirect = 0x00;
inline constexpr std::uint8_t ModRmModDisp8 = 0x01;
inline constexpr std::uint8_t ModRmModDisp32 = 0x02;
inline constexpr std::uint8_t ModRmModRegister = 0x03;
inline constexpr std::uint8_t ModRmRmSibPresent = 0x04;
inline constexpr std::uint8_t ModRmRmRipRelative = 0x05;

inline constexpr std::uint8_t Grp3RegTestMax = 0x01;
inline constexpr std::uint8_t SibBaseMask = 0x07;
inline constexpr std::uint8_t SibBaseDisp32 = 0x05;

inline constexpr std::size_t Disp8Size = 1;
inline constexpr std::size_t Disp32Size = 4;

inline constexpr std::size_t Rel32InstructionLength = 5;

inline constexpr std::size_t Vex2PrefixLength = 2;
inline constexpr std::size_t Vex3PrefixLength = 3;
inline constexpr std::uint8_t Vex3MapMask = 0x1F;
inline constexpr std::uint8_t Vex3Map0F3A = 0x03;

}

#endif
