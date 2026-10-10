#include "IntermediateRepresentation/IrOpcode.hpp"
#include <array>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

constexpr std::size_t MaxOpcodeArgs = 13;

struct OpcodeMeta {
    std::string_view name;
    IrType type = IrType::Void;
    std::array<IrType, MaxOpcodeArgs> args{};
    std::size_t argCount = 0;
};

template <typename... TArgs>
consteval OpcodeMeta makeMeta(std::string_view name, IrType type, TArgs... args) {
    static_assert(sizeof...(TArgs) <= MaxOpcodeArgs);
    OpcodeMeta meta{.name = name, .type = type, .argCount = sizeof...(TArgs)};
    meta.args.fill(IrType::Void);
    std::size_t index = 0;
    ((meta.args[index++] = args), ...);
    return meta;
}

constexpr IrType Void = IrType::Void;
constexpr IrType Label = IrType::Label;
constexpr IrType Opaque = IrType::Opaque;
constexpr IrType ScalarReg = IrType::ScalarReg;
constexpr IrType VectorReg = IrType::VectorReg;
constexpr IrType U1 = IrType::U1;
constexpr IrType U8 = IrType::U8;
constexpr IrType U16 = IrType::U16;
constexpr IrType U32 = IrType::U32;
constexpr IrType S32 = IrType::S32;
constexpr IrType U64 = IrType::U64;
constexpr IrType F16 = IrType::F16;
constexpr IrType F32 = IrType::F32;
constexpr IrType U32x2 = IrType::U32x2;
constexpr IrType U32x3 = IrType::U32x3;
constexpr IrType U32x4 = IrType::U32x4;
constexpr IrType F32x2 = IrType::F32x2;
constexpr IrType SrtResource = IrType::SrtResource;
constexpr IrType BufferResource = IrType::BufferResource;
constexpr IrType AddressResource = IrType::AddressResource;
constexpr IrType ImageResource = IrType::ImageResource;
constexpr IrType SamplerResource = IrType::SamplerResource;
constexpr IrType ImageAddress = IrType::ImageAddress;

constexpr std::array<OpcodeMeta, static_cast<std::size_t>(IrOpcode::Count)> MetaTable = {{
    makeMeta("Identity", Opaque, Opaque),
    makeMeta("Phi", Opaque),
    makeMeta("GetRegister", Opaque, Opaque),
    makeMeta("SetRegister", Void, Opaque, Opaque),
    makeMeta("IAdd32", U32, U32, U32),
    makeMeta("ISub32", U32, U32, U32),
    makeMeta("IMul32", U32, U32, U32),
    makeMeta("IDiv32", S32, S32, S32),
    makeMeta("IMod32", S32, S32, S32),
    makeMeta("IAnd32", S32, S32, S32),
    makeMeta("IOr32", S32, S32, S32),
    makeMeta("IXor32", S32, S32, S32),
    makeMeta("IShiftLeft32", S32, S32, S32),
    makeMeta("IShiftRightLogical32", S32, S32, S32),
    makeMeta("IShiftRightArithmetic32", S32, S32, S32),
    makeMeta("INegate32", S32, S32),
    makeMeta("INot32", S32, S32),
    makeMeta("FAdd32", F32, F32, F32),
    makeMeta("FSub32", F32, F32, F32),
    makeMeta("FMul32", F32, F32, F32),
    makeMeta("FDiv32", F32, F32, F32),
    makeMeta("FNegate32", F32, F32),
    makeMeta("FAbs32", F32, F32),
    makeMeta("FFma32", F32, F32, F32, F32),
    makeMeta("FSqrt32", F32, F32),
    makeMeta("FRcp32", F32, F32),
    makeMeta("FRsq32", F32, F32),
    makeMeta("FMin32", F32, F32, F32),
    makeMeta("FMax32", F32, F32, F32),
    makeMeta("FFloor32", F32, F32),
    makeMeta("FCeil32", F32, F32),
    makeMeta("FFract32", F32, F32),
    makeMeta("ConvertFToI32", S32, F32),
    makeMeta("ConvertIToF32", F32, S32),
    makeMeta("ConvertFToU32", U32, F32),
    makeMeta("ConvertUToF32", F32, U32),
    makeMeta("CompareEqual", U1, S32, S32),
    makeMeta("CompareNotEqual", U1, S32, S32),
    makeMeta("CompareLessThan", U1, S32, S32),
    makeMeta("CompareGreaterThan", U1, S32, S32),
    makeMeta("Select", Opaque, U1, Opaque, Opaque),
    makeMeta("Branch", Void, Label),
    makeMeta("BranchConditional", Void, U1, Label, Label),
    makeMeta("Loop", Void, Label),
    makeMeta("LoopMerge", Void, Label, Label),
    makeMeta("Return", Void),
    makeMeta("Unreachable", Void),
    makeMeta("LoadBuffer", U32, BufferResource, U32),
    makeMeta("StoreBuffer", Void, BufferResource, U32, U32),
    makeMeta("LoadImage", U32x4, ImageResource, ImageAddress),
    makeMeta("StoreImage", Void, ImageResource, ImageAddress, U32x4),
    makeMeta("ImageSampleImplicitLod", U32x4, ImageResource, SamplerResource, ImageAddress),
    makeMeta("ImageSampleExplicitLod", U32x4, ImageResource, SamplerResource, ImageAddress, F32),
    makeMeta("LoadShared", U32, U32),
    makeMeta("StoreShared", Void, U32, U32),
    makeMeta("Barrier", Void),
    makeMeta("EmitVertex", Void),
    makeMeta("Discard", Void),
    makeMeta("ReadFirstLane", U32, U32, U1),
    makeMeta("ReadLane", U32, U32, U32),
    makeMeta("Ballot", U32x4, U1),
    makeMeta("Void", Void),
    makeMeta("Reference", Void, U1),
    makeMeta("ReferenceU32", Void, U32),
    makeMeta("GetUserData", U32, ScalarReg),
    makeMeta("GetShaderBase", U64),
    makeMeta("ShaderClock", U64),
    makeMeta("RealtimeClock", U64),
    makeMeta("MeshDrawParameter", U32, U32),
    makeMeta("MeshArgument", U32, U32),
    makeMeta("MeshAllocate", Void, U32),
    makeMeta("TessellationBase", U32, U32),
    makeMeta("GetTessellationAttribute", U32, U32, U32, U1),
    makeMeta("SetTessellationAttribute", Void, U32, U32, U32, U1),
    makeMeta("GetBuiltin", U32, U32, U32),
    makeMeta("GetThreadBitScalarRegister", U1, ScalarReg),
    makeMeta("SetThreadBitScalarRegister", Void, ScalarReg, U1),
    makeMeta("GetScalarMaskTag", U1, ScalarReg),
    makeMeta("SetScalarMaskTag", Void, ScalarReg, U1),
    makeMeta("GetScalarRegister", U32, ScalarReg),
    makeMeta("SetScalarRegister", Void, ScalarReg, U32),
    makeMeta("GetVectorRegister", U32, VectorReg),
    makeMeta("SetVectorRegister", Void, VectorReg, U32),
    makeMeta("GetGotoVariable", U1, U32),
    makeMeta("SetGotoVariable", Void, U32, U1),
    makeMeta("GetScc", U1),
    makeMeta("SetScc", Void, U1),
    makeMeta("GetExec", U1),
    makeMeta("SetExec", Void, U1),
    makeMeta("GetExecLo", U32),
    makeMeta("SetExecLo", Void, U32),
    makeMeta("GetExecHi", U32),
    makeMeta("SetExecHi", Void, U32),
    makeMeta("GetVcc", U1),
    makeMeta("SetVcc", Void, U1),
    makeMeta("GetVccLo", U32),
    makeMeta("SetVccLo", Void, U32),
    makeMeta("GetVccHi", U32),
    makeMeta("SetVccHi", Void, U32),
    makeMeta("GetM0", U32),
    makeMeta("SetM0", Void, U32),
    makeMeta("UndefU1", U1),
    makeMeta("UndefU8", U8),
    makeMeta("UndefU16", U16),
    makeMeta("UndefU32", U32),
    makeMeta("UndefU64", U64),
    makeMeta("BitCastU16F16", U16, F16),
    makeMeta("BitCastF16U16", F16, U16),
    makeMeta("BitCastU32F32", U32, F32),
    makeMeta("BitCastF32U32", F32, U32),
    makeMeta("ConvertU16U32", U16, U32),
    makeMeta("ConvertU32U16", U32, U16),
    makeMeta("ConvertU8U32", U8, U32),
    makeMeta("ConvertU32U8", U32, U8),
    makeMeta("ConvertF32F16", F32, F16),
    makeMeta("ConvertF16F32", F16, F32),
    makeMeta("ConvertS32F32", U32, F32),
    makeMeta("ConvertU32F32", U32, F32),
    makeMeta("ConvertF32S32", F32, U32),
    makeMeta("ConvertF32U32", F32, U32),
    makeMeta("CompositeConstructU64", U64, U32, U32),
    makeMeta("CompositeConstructU32x2", U32x2, U32, U32),
    makeMeta("CompositeConstructU32x3", U32x3, U32, U32, U32),
    makeMeta("CompositeConstructF32x2", F32x2, F32, F32),
    makeMeta("CompositeConstructU32x4", U32x4, U32, U32, U32, U32),
    makeMeta("CompositeExtractU64", U32, U64, U32),
    makeMeta("CompositeExtractU32x2", U32, U32x2, U32),
    makeMeta("CompositeExtractU32x3", U32, U32x3, U32),
    makeMeta("CompositeExtractU32x4", U32, U32x4, U32),
    makeMeta("PackHalf2x16", U32, F32x2),
    makeMeta("PackFloat2x16Rtz", U32, F32, F32),
    makeMeta("FPAbs32", F32, F32),
    makeMeta("FPNeg32", F32, F32),
    makeMeta("FPSaturate32", F32, F32),
    makeMeta("BitFieldInsert", U32, U32, U32, U32, U32),
    makeMeta("BitFieldUExtract", U32, U32, U32, U32),
    makeMeta("BitFieldSExtract", U32, U32, U32, U32),
    makeMeta("DppMoveU32", U32, U32, U1),
    makeMeta("DppUpdateU32", U32, U32, U32, U1),
    makeMeta("WqmU64", U64, U64),
    makeMeta("SelectU1", U1, U1, U1, U1),
    makeMeta("SelectF32", F32, U1, F32, F32),
    makeMeta("IAdd64", U64, U64, U64),
    makeMeta("IAddCarry32", U32x2, U32, U32),
    makeMeta("ISub64", U64, U64, U64),
    makeMeta("IMul64", U64, U64, U64),
    makeMeta("UDiv32", U32, U32, U32),
    makeMeta("SMulHi", U32, U32, U32),
    makeMeta("UMulHi", U32, U32, U32),
    makeMeta("IAbs32", U32, U32),
    makeMeta("ShiftLeftLogical32", U32, U32, U32),
    makeMeta("ShiftLeftLogical64", U64, U64, U32),
    makeMeta("ShiftRightLogical32", U32, U32, U32),
    makeMeta("ShiftRightLogical64", U64, U64, U32),
    makeMeta("ShiftRightArithmetic32", U32, U32, U32),
    makeMeta("ShiftRightArithmetic64", U64, U64, U32),
    makeMeta("BitwiseAnd32", U32, U32, U32),
    makeMeta("BitwiseAnd64", U64, U64, U64),
    makeMeta("BitwiseOr32", U32, U32, U32),
    makeMeta("BitwiseXor32", U32, U32, U32),
    makeMeta("BitwiseNot32", U32, U32),
    makeMeta("BitReverse32", U32, U32),
    makeMeta("BitCount32", U32, U32),
    makeMeta("BitCount64", U32, U64),
    makeMeta("FindUMsb32", U32, U32),
    makeMeta("FindUMsb64", U32, U64),
    makeMeta("FindILsb32", U32, U32),
    makeMeta("SMin32", U32, U32, U32),
    makeMeta("UMin32", U32, U32, U32),
    makeMeta("SMax32", U32, U32, U32),
    makeMeta("UMax32", U32, U32, U32),
    makeMeta("SMinTri32", U32, U32, U32, U32),
    makeMeta("UMinTri32", U32, U32, U32, U32),
    makeMeta("SMaxTri32", U32, U32, U32, U32),
    makeMeta("UMaxTri32", U32, U32, U32, U32),
    makeMeta("SMedTri32", U32, U32, U32, U32),
    makeMeta("UMedTri32", U32, U32, U32, U32),
    makeMeta("SLessThan32", U1, U32, U32),
    makeMeta("SLessThan64", U1, U64, U64),
    makeMeta("ULessThan32", U1, U32, U32),
    makeMeta("ULessThan64", U1, U64, U64),
    makeMeta("IEqual32", U1, U32, U32),
    makeMeta("IEqual64", U1, U64, U64),
    makeMeta("SLessThanEqual32", U1, U32, U32),
    makeMeta("ULessThanEqual32", U1, U32, U32),
    makeMeta("SGreaterThan32", U1, U32, U32),
    makeMeta("UGreaterThan32", U1, U32, U32),
    makeMeta("UGreaterThan64", U1, U64, U64),
    makeMeta("INotEqual32", U1, U32, U32),
    makeMeta("INotEqual64", U1, U64, U64),
    makeMeta("SGreaterThanEqual32", U1, U32, U32),
    makeMeta("UGreaterThanEqual32", U1, U32, U32),
    makeMeta("LogicalOr", U1, U1, U1),
    makeMeta("LogicalAnd", U1, U1, U1),
    makeMeta("LogicalXor", U1, U1, U1),
    makeMeta("LogicalNot", U1, U1),
    makeMeta("FPOrdEqual32", U1, F32, F32),
    makeMeta("FPUnordEqual32", U1, F32, F32),
    makeMeta("FPOrdNotEqual32", U1, F32, F32),
    makeMeta("FPUnordNotEqual32", U1, F32, F32),
    makeMeta("FPOrdLessThan32", U1, F32, F32),
    makeMeta("FPUnordLessThan32", U1, F32, F32),
    makeMeta("FPOrdGreaterThan32", U1, F32, F32),
    makeMeta("FPUnordGreaterThan32", U1, F32, F32),
    makeMeta("FPOrdLessThanEqual32", U1, F32, F32),
    makeMeta("FPUnordLessThanEqual32", U1, F32, F32),
    makeMeta("FPOrdGreaterThanEqual32", U1, F32, F32),
    makeMeta("FPUnordGreaterThanEqual32", U1, F32, F32),
    makeMeta("FPIsNan32", U1, F32),
    makeMeta("FPCmpClass32", U1, F32, U32),
    makeMeta("FPAdd32", F32, F32, F32),
    makeMeta("FPSub32", F32, F32, F32),
    makeMeta("FPFma32", F32, F32, F32, F32),
    makeMeta("FPMad32", F32, F32, F32, F32),
    makeMeta("FPMul32", F32, F32, F32),
    makeMeta("FPMin32", F32, F32, F32),
    makeMeta("FPMax32", F32, F32, F32),
    makeMeta("FPMinTri32", F32, F32, F32, F32),
    makeMeta("FPMaxTri32", F32, F32, F32, F32),
    makeMeta("FPMedTri32", F32, F32, F32, F32),
    makeMeta("FPRecip32", F32, F32),
    makeMeta("FPRecipIFlag32", F32, F32),
    makeMeta("FPRecipSqrt32", F32, F32),
    makeMeta("FPSqrt", F32, F32),
    makeMeta("FPSin", F32, F32),
    makeMeta("FPCos", F32, F32),
    makeMeta("FPExp2", F32, F32),
    makeMeta("FPLog2", F32, F32),
    makeMeta("FPRoundEven32", F32, F32),
    makeMeta("FPFloor32", F32, F32),
    makeMeta("FPCeil32", F32, F32),
    makeMeta("FPTrunc32", F32, F32),
    makeMeta("FPFract32", F32, F32),
    makeMeta("FPAdd64", U64, U64, U64),
    makeMeta("FPMul64", U64, U64, U64),
    makeMeta("FPFma64", U64, U64, U64, U64),
    makeMeta("FPFmaScale64", U64, U64, U64, U64, U32),
    makeMeta("FPMin64", U64, U64, U64),
    makeMeta("FPMax64", U64, U64, U64),
    makeMeta("FPSaturate64", U64, U64),
    makeMeta("FPLdexp64", U64, U64, U32),
    makeMeta("FPRoundEven64", U64, U64),
    makeMeta("FPFloor64", U64, U64),
    makeMeta("FPCeil64", U64, U64),
    makeMeta("FPTrunc64", U64, U64),
    makeMeta("FPFract64", U64, U64),
    makeMeta("FPFrexpMant64", U64, U64),
    makeMeta("FPFrexpExp64", U32, U64),
    makeMeta("FPRcp64", U64, U64),
    makeMeta("FPRsq64", U64, U64),
    makeMeta("FPSqrt64", U64, U64),
    makeMeta("FPTrigPreop64", U64, U64, U32),
    makeMeta("FPDot2F32F16", U32, U32, U32, U32),
    makeMeta("FPInterpolateF32", F32, F32, F32, F32, U32),
    makeMeta("FPInterpolateF16", F32, F32, F32, F32, U32),
    makeMeta("ConvertF32F64", F32, U64),
    makeMeta("ConvertF64F32", U64, F32),
    makeMeta("ConvertF64S32", U64, U32),
    makeMeta("ConvertF64U32", U64, U32),
    makeMeta("ConvertS32F64", U32, U64),
    makeMeta("ConvertU32F64", U32, U64),
    makeMeta("LaneId", U32),
    makeMeta("WriteLane", U32, U32, U32, U32),
    makeMeta("Permlane16U32", U32, U32, U32, U32, U1, U32),
    makeMeta("BpermuteU32", U32, U32, U32, U1),
    makeMeta("PermuteU32", U32, U32, U32, U1),
    makeMeta("GetSrtResource", SrtResource),
    makeMeta("GetBufferResource", BufferResource, U32, U32, U32, U32),
    makeMeta("GetAddressResource", AddressResource, U32, U32),
    makeMeta("GetScratchResource", AddressResource),
    makeMeta("GetImageResource", ImageResource, U32, U32, U32, U32, U32, U32, U32, U32),
    makeMeta("GetSamplerResource", SamplerResource, U32, U32, U32, U32),
    makeMeta("MakeImageAddress", ImageAddress, U32, U32, U32, U32, U32, U32, U32, U32, U32, U32, U32, U32, U32),
    makeMeta("ReadConst", U32, SrtResource, U32),
    makeMeta("ReadConstBuffer", U32, BufferResource, U32),
    makeMeta("LoadAddressU8", U8, AddressResource, U32, U32, U1),
    makeMeta("LoadAddressU16", U16, AddressResource, U32, U32, U1),
    makeMeta("LoadAddressU32", U32, AddressResource, U32, U32, U1),
    makeMeta("LoadAddressU32x2", U32x2, AddressResource, U32, U32, U1),
    makeMeta("LoadAddressU32x3", U32x3, AddressResource, U32, U32, U1),
    makeMeta("LoadAddressU32x4", U32x4, AddressResource, U32, U32, U1),
    makeMeta("StoreAddressU8", Void, AddressResource, U32, U32, U8, U1),
    makeMeta("StoreAddressU16", Void, AddressResource, U32, U32, U16, U1),
    makeMeta("StoreAddressU32", Void, AddressResource, U32, U32, U32, U1),
    makeMeta("StoreAddressU32x2", Void, AddressResource, U32, U32, U32x2, U1),
    makeMeta("StoreAddressU32x3", Void, AddressResource, U32, U32, U32x3, U1),
    makeMeta("StoreAddressU32x4", Void, AddressResource, U32, U32, U32x4, U1),
    makeMeta("AddressAtomicSwap32", U32, AddressResource, U32, U32, U32, U1),
    makeMeta("AddressAtomicCmpSwap32", U32, AddressResource, U32, U32, U32, U32, U1),
    makeMeta("AddressAtomicIAdd32", U32, AddressResource, U32, U32, U32, U1),
    makeMeta("AddressAtomicISub32", U32, AddressResource, U32, U32, U32, U1),
    makeMeta("AddressAtomicSMin32", U32, AddressResource, U32, U32, U32, U1),
    makeMeta("AddressAtomicUMin32", U32, AddressResource, U32, U32, U32, U1),
    makeMeta("AddressAtomicSMax32", U32, AddressResource, U32, U32, U32, U1),
    makeMeta("AddressAtomicUMax32", U32, AddressResource, U32, U32, U32, U1),
    makeMeta("AddressAtomicAnd32", U32, AddressResource, U32, U32, U32, U1),
    makeMeta("AddressAtomicOr32", U32, AddressResource, U32, U32, U32, U1),
    makeMeta("AddressAtomicXor32", U32, AddressResource, U32, U32, U32, U1),
    makeMeta("AddressAtomicInc32", U32, AddressResource, U32, U32, U32, U1),
    makeMeta("AddressAtomicDec32", U32, AddressResource, U32, U32, U32, U1),
    makeMeta("AddressAtomicUSubSat32", U32, AddressResource, U32, U32, U32, U1),
    makeMeta("AddressAtomicSwap64", U64, AddressResource, U32, U32, U64, U1),
    makeMeta("AddressAtomicCmpSwap64", U64, AddressResource, U32, U32, U64, U64, U1),
    makeMeta("AddressAtomicIAdd64", U64, AddressResource, U32, U32, U64, U1),
    makeMeta("AddressAtomicISub64", U64, AddressResource, U32, U32, U64, U1),
    makeMeta("AddressAtomicSMin64", U64, AddressResource, U32, U32, U64, U1),
    makeMeta("AddressAtomicUMin64", U64, AddressResource, U32, U32, U64, U1),
    makeMeta("AddressAtomicSMax64", U64, AddressResource, U32, U32, U64, U1),
    makeMeta("AddressAtomicUMax64", U64, AddressResource, U32, U32, U64, U1),
    makeMeta("AddressAtomicAnd64", U64, AddressResource, U32, U32, U64, U1),
    makeMeta("AddressAtomicOr64", U64, AddressResource, U32, U32, U64, U1),
    makeMeta("AddressAtomicXor64", U64, AddressResource, U32, U32, U64, U1),
    makeMeta("AddressAtomicFCmpSwap32", U32, AddressResource, U32, U32, U32, U32, U1),
    makeMeta("AddressAtomicFMin32", U32, AddressResource, U32, U32, U32, U1),
    makeMeta("AddressAtomicFMax32", U32, AddressResource, U32, U32, U32, U1),
    makeMeta("AddressAtomicFCmpSwap64", U64, AddressResource, U32, U32, U64, U64, U1),
    makeMeta("AddressAtomicFMin64", U64, AddressResource, U32, U32, U64, U1),
    makeMeta("AddressAtomicFMax64", U64, AddressResource, U32, U32, U64, U1),
    makeMeta("AddressAtomicInc64", U64, AddressResource, U32, U32, U64, U1),
    makeMeta("AddressAtomicDec64", U64, AddressResource, U32, U32, U64, U1),
    makeMeta("LoadBufferU8", U8, BufferResource, U32, U32, U32, U1),
    makeMeta("LoadBufferU16", U16, BufferResource, U32, U32, U32, U1),
    makeMeta("LoadBufferU32", U32, BufferResource, U32, U32, U32, U1),
    makeMeta("LoadBufferU32x2", U32x2, BufferResource, U32, U32, U32, U1),
    makeMeta("LoadBufferU32x3", U32x3, BufferResource, U32, U32, U32, U1),
    makeMeta("LoadBufferU32x4", U32x4, BufferResource, U32, U32, U32, U1),
    makeMeta("StoreBufferU8", Void, BufferResource, U32, U32, U32, U8, U1),
    makeMeta("StoreBufferU16", Void, BufferResource, U32, U32, U32, U16, U1),
    makeMeta("StoreBufferU32", Void, BufferResource, U32, U32, U32, U32, U1),
    makeMeta("StoreBufferU32x2", Void, BufferResource, U32, U32, U32, U32x2, U1),
    makeMeta("StoreBufferU32x3", Void, BufferResource, U32, U32, U32, U32x3, U1),
    makeMeta("StoreBufferU32x4", Void, BufferResource, U32, U32, U32, U32x4, U1),
    makeMeta("BufferAtomicSwap32", U32, BufferResource, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicCmpSwap32", U32, BufferResource, U32, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicSwap64", U64, BufferResource, U32, U32, U32, U64, U1),
    makeMeta("BufferAtomicIAdd32", U32, BufferResource, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicISub32", U32, BufferResource, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicSMin32", U32, BufferResource, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicUMin32", U32, BufferResource, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicSMax32", U32, BufferResource, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicUMax32", U32, BufferResource, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicAnd32", U32, BufferResource, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicOr32", U32, BufferResource, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicOr64", U64, BufferResource, U32, U32, U32, U64, U1),
    makeMeta("BufferAtomicXor32", U32, BufferResource, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicFMin32", U32, BufferResource, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicFMax32", U32, BufferResource, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicInc32", U32, BufferResource, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicDec32", U32, BufferResource, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicUSubSat32", U32, BufferResource, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicIAdd64", U64, BufferResource, U32, U32, U32, U64, U1),
    makeMeta("BufferAtomicISub64", U64, BufferResource, U32, U32, U32, U64, U1),
    makeMeta("BufferAtomicSMin64", U64, BufferResource, U32, U32, U32, U64, U1),
    makeMeta("BufferAtomicUMin64", U64, BufferResource, U32, U32, U32, U64, U1),
    makeMeta("BufferAtomicSMax64", U64, BufferResource, U32, U32, U32, U64, U1),
    makeMeta("BufferAtomicUMax64", U64, BufferResource, U32, U32, U32, U64, U1),
    makeMeta("BufferAtomicAnd64", U64, BufferResource, U32, U32, U32, U64, U1),
    makeMeta("BufferAtomicXor64", U64, BufferResource, U32, U32, U32, U64, U1),
    makeMeta("BufferAtomicCmpSwap64", U64, BufferResource, U32, U32, U32, U64, U64, U1),
    makeMeta("BufferAtomicFCmpSwap32", U32, BufferResource, U32, U32, U32, U32, U32, U1),
    makeMeta("BufferAtomicFCmpSwap64", U64, BufferResource, U32, U32, U32, U64, U64, U1),
    makeMeta("BufferAtomicFMin64", U64, BufferResource, U32, U32, U32, U64, U1),
    makeMeta("BufferAtomicFMax64", U64, BufferResource, U32, U32, U32, U64, U1),
    makeMeta("BufferAtomicInc64", U64, BufferResource, U32, U32, U32, U64, U1),
    makeMeta("BufferAtomicDec64", U64, BufferResource, U32, U32, U32, U64, U1),
    makeMeta("LoadSharedU8", U8, U32, U1),
    makeMeta("LoadSharedU16", U16, U32, U1),
    makeMeta("LoadSharedU32", U32, U32, U1),
    makeMeta("LoadSharedU32x2", U32x2, U32, U1),
    makeMeta("LoadSharedU32x3", U32x3, U32, U1),
    makeMeta("LoadSharedU32x4", U32x4, U32, U1),
    makeMeta("WriteSharedU8", Void, U32, U8, U1),
    makeMeta("WriteSharedU16", Void, U32, U16, U1),
    makeMeta("WriteSharedU32", Void, U32, U32, U1),
    makeMeta("WriteSharedU32x2", Void, U32, U32, U32, U1),
    makeMeta("WriteSharedU32x3", Void, U32, U32, U32, U32, U1),
    makeMeta("WriteSharedU32x4", Void, U32, U32, U32, U32, U32, U1),
    makeMeta("SharedAtomicFMin32", U32, U32, U32, U1),
    makeMeta("SharedAtomicFMax32", U32, U32, U32, U1),
    makeMeta("SharedAtomicSwap32", U32, U32, U32, U1),
    makeMeta("SharedAtomicIAdd32", U32, U32, U32, U1),
    makeMeta("SharedAtomicISub32", U32, U32, U32, U1),
    makeMeta("SharedAtomicInc32", U32, U32, U32, U1),
    makeMeta("SharedAtomicDec32", U32, U32, U32, U1),
    makeMeta("SharedAtomicSMin32", U32, U32, U32, U1),
    makeMeta("SharedAtomicUMin32", U32, U32, U32, U1),
    makeMeta("SharedAtomicSMax32", U32, U32, U32, U1),
    makeMeta("SharedAtomicUMax32", U32, U32, U32, U1),
    makeMeta("SharedAtomicAnd32", U32, U32, U32, U1),
    makeMeta("SharedAtomicOr32", U32, U32, U32, U1),
    makeMeta("SharedAtomicXor32", U32, U32, U32, U1),
    makeMeta("SharedAtomicRsub32", U32, U32, U32, U1),
    makeMeta("SharedAtomicFAdd32", U32, U32, U32, U1),
    makeMeta("SharedAtomicCmpst32", U32, U32, U32, U32, U1),
    makeMeta("SharedAtomicCmpstF32", U32, U32, U32, U32, U1),
    makeMeta("SharedAtomicMskor32", U32, U32, U32, U32, U1),
    makeMeta("SharedAtomicWrap32", U32, U32, U32, U32, U1),
    makeMeta("SharedAtomicSwap64", U64, U32, U64, U1),
    makeMeta("SharedAtomicIAdd64", U64, U32, U64, U1),
    makeMeta("SharedAtomicISub64", U64, U32, U64, U1),
    makeMeta("SharedAtomicRsub64", U64, U32, U64, U1),
    makeMeta("SharedAtomicInc64", U64, U32, U64, U1),
    makeMeta("SharedAtomicDec64", U64, U32, U64, U1),
    makeMeta("SharedAtomicSMin64", U64, U32, U64, U1),
    makeMeta("SharedAtomicUMin64", U64, U32, U64, U1),
    makeMeta("SharedAtomicSMax64", U64, U32, U64, U1),
    makeMeta("SharedAtomicUMax64", U64, U32, U64, U1),
    makeMeta("SharedAtomicAnd64", U64, U32, U64, U1),
    makeMeta("SharedAtomicOr64", U64, U32, U64, U1),
    makeMeta("SharedAtomicXor64", U64, U32, U64, U1),
    makeMeta("SharedAtomicFMin64", U64, U32, U64, U1),
    makeMeta("SharedAtomicFMax64", U64, U32, U64, U1),
    makeMeta("SharedAtomicCmpst64", U64, U32, U64, U64, U1),
    makeMeta("SharedAtomicCmpstF64", U64, U32, U64, U64, U1),
    makeMeta("SharedAtomicMskor64", U64, U32, U64, U64, U1),
    makeMeta("DataAppend", U32, U32, U1, U32, U32),
    makeMeta("DataConsume", U32, U32, U1, U32, U32),
    makeMeta("SwizzleU32", U32, U32, U32, U1),
    makeMeta("ImageQueryDimensions", U32x4, ImageResource, ImageAddress),
    makeMeta("ImageQueryLod", U32x4, ImageResource, SamplerResource, ImageAddress),
    makeMeta("ImageRead", U32x4, ImageResource, ImageAddress, U1),
    makeMeta("ImageWrite", Void, ImageResource, ImageAddress, U32x4, U1),
    makeMeta("ImageSampleRaw", U32x4, ImageResource, SamplerResource, ImageAddress),
    makeMeta("ImageGatherRaw", U32x4, ImageResource, SamplerResource, ImageAddress),
    makeMeta("ImageAtomicSwap32", U32, ImageResource, ImageAddress, U32, U1),
    makeMeta("ImageAtomicIAdd32", U32, ImageResource, ImageAddress, U32, U1),
    makeMeta("ImageAtomicUMin32", U32, ImageResource, ImageAddress, U32, U1),
    makeMeta("ImageAtomicUMax32", U32, ImageResource, ImageAddress, U32, U1),
    makeMeta("ImageAtomicAnd32", U32, ImageResource, ImageAddress, U32, U1),
    makeMeta("ImageAtomicOr32", U32, ImageResource, ImageAddress, U32, U1),
    makeMeta("ImageAtomicXor32", U32, ImageResource, ImageAddress, U32, U1),
    makeMeta("ImageAtomicCmpSwap32", U32, ImageResource, ImageAddress, U32, U32, U1),
    makeMeta("ImageAtomicISub32", U32, ImageResource, ImageAddress, U32, U1),
    makeMeta("ImageAtomicSMin32", U32, ImageResource, ImageAddress, U32, U1),
    makeMeta("ImageAtomicSMax32", U32, ImageResource, ImageAddress, U32, U1),
    makeMeta("ImageAtomicInc32", U32, ImageResource, ImageAddress, U32, U1),
    makeMeta("ImageAtomicDec32", U32, ImageResource, ImageAddress, U32, U1),
    makeMeta("ImageAtomicFCmpSwap32", U32, ImageResource, ImageAddress, U32, U32, U1),
    makeMeta("ImageAtomicFMin32", U32, ImageResource, ImageAddress, U32, U1),
    makeMeta("ImageAtomicFMax32", U32, ImageResource, ImageAddress, U32, U1),
    makeMeta("ImageAtomicSwap64", U64, ImageResource, ImageAddress, U64, U1),
    makeMeta("ImageAtomicIAdd64", U64, ImageResource, ImageAddress, U64, U1),
    makeMeta("ImageAtomicISub64", U64, ImageResource, ImageAddress, U64, U1),
    makeMeta("ImageAtomicUMin64", U64, ImageResource, ImageAddress, U64, U1),
    makeMeta("ImageAtomicUMax64", U64, ImageResource, ImageAddress, U64, U1),
    makeMeta("ImageAtomicSMin64", U64, ImageResource, ImageAddress, U64, U1),
    makeMeta("ImageAtomicSMax64", U64, ImageResource, ImageAddress, U64, U1),
    makeMeta("ImageAtomicAnd64", U64, ImageResource, ImageAddress, U64, U1),
    makeMeta("ImageAtomicOr64", U64, ImageResource, ImageAddress, U64, U1),
    makeMeta("ImageAtomicXor64", U64, ImageResource, ImageAddress, U64, U1),
    makeMeta("ImageAtomicCmpSwap64", U64, ImageResource, ImageAddress, U64, U64, U1),
    makeMeta("GetAttribute", U32, U32, U32),
    makeMeta("GetInterpolationParameter", U32, U32, U32, U32),
    makeMeta("GetInterpolationParameterF16", F32, U32, U32, U32, U32),
    makeMeta("InterpolateHostP1", F32, U32, U32, F32),
    makeMeta("InterpolateHostP2", Void, U32, U32, F32, F32, U1),
    makeMeta("SetAttribute", Void, U32x4, U1),
    makeMeta("ControlNop", Void),
    makeMeta("Waitcnt", Void),
    makeMeta("Sendmsg", Void),
    makeMeta("TtraceData", Void),
    makeMeta("InstPrefetch", Void),
    makeMeta("SelectU32", U32, U1, U32, U32),
    makeMeta("ImageBvhIntersectRay", U32x4, U32x4, ImageAddress, U1),
}};

const OpcodeMeta& metaOf(IrOpcode opcode) {
    const std::size_t index = static_cast<std::size_t>(opcode);
    if (index >= MetaTable.size()) {
        throw std::out_of_range("IrOpcode value is out of the known opcode range");
    }
    return MetaTable[index];
}

}

std::size_t IrOpcodeOperandCount(IrOpcode opcode) {
    return metaOf(opcode).argCount;
}

IrType IrOpcodeType(IrOpcode opcode) {
    return metaOf(opcode).type;
}

IrType IrOpcodeArgumentType(IrOpcode opcode, std::size_t index) {
    const OpcodeMeta& meta = metaOf(opcode);
    if (index >= meta.argCount) {
        throw std::out_of_range("IrOpcode argument index is out of range for this opcode");
    }
    return meta.args[index];
}

BufferAccess BufferAccessOf(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::LoadBuffer:
        case IrOpcode::ReadConstBuffer:
        case IrOpcode::LoadBufferU8:
        case IrOpcode::LoadBufferU16:
        case IrOpcode::LoadBufferU32:
        case IrOpcode::LoadBufferU32x2:
        case IrOpcode::LoadBufferU32x3:
        case IrOpcode::LoadBufferU32x4:
            return BufferAccess::Read;
        case IrOpcode::StoreBuffer:
        case IrOpcode::StoreBufferU8:
        case IrOpcode::StoreBufferU16:
        case IrOpcode::StoreBufferU32:
        case IrOpcode::StoreBufferU32x2:
        case IrOpcode::StoreBufferU32x3:
        case IrOpcode::StoreBufferU32x4:
            return BufferAccess::Write;
        case IrOpcode::BufferAtomicSwap32:
        case IrOpcode::BufferAtomicCmpSwap32:
        case IrOpcode::BufferAtomicSwap64:
        case IrOpcode::BufferAtomicIAdd32:
        case IrOpcode::BufferAtomicISub32:
        case IrOpcode::BufferAtomicSMin32:
        case IrOpcode::BufferAtomicUMin32:
        case IrOpcode::BufferAtomicSMax32:
        case IrOpcode::BufferAtomicUMax32:
        case IrOpcode::BufferAtomicAnd32:
        case IrOpcode::BufferAtomicOr32:
        case IrOpcode::BufferAtomicOr64:
        case IrOpcode::BufferAtomicXor32:
        case IrOpcode::BufferAtomicFMin32:
        case IrOpcode::BufferAtomicFMax32:
        case IrOpcode::BufferAtomicInc32:
        case IrOpcode::BufferAtomicDec32:
        case IrOpcode::BufferAtomicUSubSat32:
        case IrOpcode::BufferAtomicIAdd64:
        case IrOpcode::BufferAtomicISub64:
        case IrOpcode::BufferAtomicSMin64:
        case IrOpcode::BufferAtomicUMin64:
        case IrOpcode::BufferAtomicSMax64:
        case IrOpcode::BufferAtomicUMax64:
        case IrOpcode::BufferAtomicAnd64:
        case IrOpcode::BufferAtomicXor64:
        case IrOpcode::BufferAtomicCmpSwap64:
        case IrOpcode::BufferAtomicFCmpSwap32:
        case IrOpcode::BufferAtomicFCmpSwap64:
        case IrOpcode::BufferAtomicFMin64:
        case IrOpcode::BufferAtomicFMax64:
        case IrOpcode::BufferAtomicInc64:
        case IrOpcode::BufferAtomicDec64:
            return BufferAccess::Atomic;
        default:
            return BufferAccess::None;
    }
}

std::uint32_t BufferComponentCount(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::BufferAtomicSwap64:
        case IrOpcode::BufferAtomicOr64:
        case IrOpcode::BufferAtomicIAdd64:
        case IrOpcode::BufferAtomicISub64:
        case IrOpcode::BufferAtomicSMin64:
        case IrOpcode::BufferAtomicUMin64:
        case IrOpcode::BufferAtomicSMax64:
        case IrOpcode::BufferAtomicUMax64:
        case IrOpcode::BufferAtomicAnd64:
        case IrOpcode::BufferAtomicXor64:
        case IrOpcode::BufferAtomicCmpSwap64:
        case IrOpcode::BufferAtomicFCmpSwap64:
        case IrOpcode::BufferAtomicFMin64:
        case IrOpcode::BufferAtomicFMax64:
        case IrOpcode::BufferAtomicInc64:
        case IrOpcode::BufferAtomicDec64:
        case IrOpcode::LoadBufferU32x2:
        case IrOpcode::StoreBufferU32x2:
            return 2u;
        case IrOpcode::LoadBufferU32x3:
        case IrOpcode::StoreBufferU32x3:
            return 3u;
        case IrOpcode::LoadBufferU32x4:
        case IrOpcode::StoreBufferU32x4:
            return 4u;
        default:
            return BufferAccessOf(opcode) == BufferAccess::None ? 0u : 1u;
    }
}

SharedAccess SharedAccessOf(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::LoadShared:
        case IrOpcode::LoadSharedU8:
        case IrOpcode::LoadSharedU16:
        case IrOpcode::LoadSharedU32:
        case IrOpcode::LoadSharedU32x2:
        case IrOpcode::LoadSharedU32x3:
        case IrOpcode::LoadSharedU32x4:
            return SharedAccess::Read;
        case IrOpcode::StoreShared:
        case IrOpcode::WriteSharedU8:
        case IrOpcode::WriteSharedU16:
        case IrOpcode::WriteSharedU32:
        case IrOpcode::WriteSharedU32x2:
        case IrOpcode::WriteSharedU32x3:
        case IrOpcode::WriteSharedU32x4:
            return SharedAccess::Write;
        case IrOpcode::SharedAtomicFMin32:
        case IrOpcode::SharedAtomicFMax32:
        case IrOpcode::SharedAtomicSwap32:
        case IrOpcode::SharedAtomicIAdd32:
        case IrOpcode::SharedAtomicISub32:
        case IrOpcode::SharedAtomicInc32:
        case IrOpcode::SharedAtomicDec32:
        case IrOpcode::SharedAtomicSMin32:
        case IrOpcode::SharedAtomicUMin32:
        case IrOpcode::SharedAtomicSMax32:
        case IrOpcode::SharedAtomicUMax32:
        case IrOpcode::SharedAtomicAnd32:
        case IrOpcode::SharedAtomicOr32:
        case IrOpcode::SharedAtomicXor32:
        case IrOpcode::SharedAtomicRsub32:
        case IrOpcode::SharedAtomicFAdd32:
        case IrOpcode::SharedAtomicCmpst32:
        case IrOpcode::SharedAtomicCmpstF32:
        case IrOpcode::SharedAtomicMskor32:
        case IrOpcode::SharedAtomicWrap32:
        case IrOpcode::SharedAtomicSwap64:
        case IrOpcode::SharedAtomicIAdd64:
        case IrOpcode::SharedAtomicISub64:
        case IrOpcode::SharedAtomicRsub64:
        case IrOpcode::SharedAtomicInc64:
        case IrOpcode::SharedAtomicDec64:
        case IrOpcode::SharedAtomicSMin64:
        case IrOpcode::SharedAtomicUMin64:
        case IrOpcode::SharedAtomicSMax64:
        case IrOpcode::SharedAtomicUMax64:
        case IrOpcode::SharedAtomicAnd64:
        case IrOpcode::SharedAtomicOr64:
        case IrOpcode::SharedAtomicXor64:
        case IrOpcode::SharedAtomicFMin64:
        case IrOpcode::SharedAtomicFMax64:
        case IrOpcode::SharedAtomicCmpst64:
        case IrOpcode::SharedAtomicCmpstF64:
        case IrOpcode::SharedAtomicMskor64:
            return SharedAccess::Atomic;
        case IrOpcode::DataAppend:
            return SharedAccess::Append;
        case IrOpcode::DataConsume:
            return SharedAccess::Consume;
        default:
            return SharedAccess::None;
    }
}

std::uint32_t SharedComponentCount(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::LoadSharedU32x2:
        case IrOpcode::WriteSharedU32x2:
        case IrOpcode::SharedAtomicSwap64:
        case IrOpcode::SharedAtomicIAdd64:
        case IrOpcode::SharedAtomicISub64:
        case IrOpcode::SharedAtomicRsub64:
        case IrOpcode::SharedAtomicInc64:
        case IrOpcode::SharedAtomicDec64:
        case IrOpcode::SharedAtomicSMin64:
        case IrOpcode::SharedAtomicUMin64:
        case IrOpcode::SharedAtomicSMax64:
        case IrOpcode::SharedAtomicUMax64:
        case IrOpcode::SharedAtomicAnd64:
        case IrOpcode::SharedAtomicOr64:
        case IrOpcode::SharedAtomicXor64:
        case IrOpcode::SharedAtomicFMin64:
        case IrOpcode::SharedAtomicFMax64:
        case IrOpcode::SharedAtomicCmpst64:
        case IrOpcode::SharedAtomicCmpstF64:
        case IrOpcode::SharedAtomicMskor64:
            return 2u;
        case IrOpcode::LoadSharedU32x3:
        case IrOpcode::WriteSharedU32x3:
            return 3u;
        case IrOpcode::LoadSharedU32x4:
        case IrOpcode::WriteSharedU32x4:
            return 4u;
        default:
            return SharedAccessOf(opcode) == SharedAccess::None ? 0u : 1u;
    }
}

AddressOpcodeInfo AddressOpcodeInfoOf(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::LoadAddressU8:
            return {AddressAccess::Read, 8u};
        case IrOpcode::LoadAddressU16:
            return {AddressAccess::Read, 16u};
        case IrOpcode::LoadAddressU32:
            return {AddressAccess::Read, 32u};
        case IrOpcode::LoadAddressU32x2:
            return {AddressAccess::Read, 32u, 2u};
        case IrOpcode::LoadAddressU32x3:
            return {AddressAccess::Read, 32u, 3u};
        case IrOpcode::LoadAddressU32x4:
            return {AddressAccess::Read, 32u, 4u};
        case IrOpcode::StoreAddressU8:
            return {AddressAccess::Write, 8u};
        case IrOpcode::StoreAddressU16:
            return {AddressAccess::Write, 16u};
        case IrOpcode::StoreAddressU32:
            return {AddressAccess::Write, 32u};
        case IrOpcode::StoreAddressU32x2:
            return {AddressAccess::Write, 32u, 2u};
        case IrOpcode::StoreAddressU32x3:
            return {AddressAccess::Write, 32u, 3u};
        case IrOpcode::StoreAddressU32x4:
            return {AddressAccess::Write, 32u, 4u};
        case IrOpcode::AddressAtomicSwap32:
        case IrOpcode::AddressAtomicCmpSwap32:
        case IrOpcode::AddressAtomicIAdd32:
        case IrOpcode::AddressAtomicISub32:
        case IrOpcode::AddressAtomicSMin32:
        case IrOpcode::AddressAtomicUMin32:
        case IrOpcode::AddressAtomicSMax32:
        case IrOpcode::AddressAtomicUMax32:
        case IrOpcode::AddressAtomicAnd32:
        case IrOpcode::AddressAtomicOr32:
        case IrOpcode::AddressAtomicXor32:
        case IrOpcode::AddressAtomicInc32:
        case IrOpcode::AddressAtomicDec32:
        case IrOpcode::AddressAtomicUSubSat32:
        case IrOpcode::AddressAtomicFCmpSwap32:
        case IrOpcode::AddressAtomicFMin32:
        case IrOpcode::AddressAtomicFMax32:
            return {AddressAccess::Atomic, 32u};
        case IrOpcode::AddressAtomicSwap64:
        case IrOpcode::AddressAtomicCmpSwap64:
        case IrOpcode::AddressAtomicIAdd64:
        case IrOpcode::AddressAtomicISub64:
        case IrOpcode::AddressAtomicSMin64:
        case IrOpcode::AddressAtomicUMin64:
        case IrOpcode::AddressAtomicSMax64:
        case IrOpcode::AddressAtomicUMax64:
        case IrOpcode::AddressAtomicAnd64:
        case IrOpcode::AddressAtomicOr64:
        case IrOpcode::AddressAtomicXor64:
        case IrOpcode::AddressAtomicFCmpSwap64:
        case IrOpcode::AddressAtomicFMin64:
        case IrOpcode::AddressAtomicFMax64:
        case IrOpcode::AddressAtomicInc64:
        case IrOpcode::AddressAtomicDec64:
            return {AddressAccess::Atomic, 32u, 2u};
        default:
            return {};
    }
}

ImageOpcodeInfo ImageOpcodeInfoOf(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::ImageQueryDimensions:
        case IrOpcode::ImageRead:
        case IrOpcode::LoadImage:
            return {ImageAccess::Read, ImageResourceClass::Sampled, false};
        case IrOpcode::ImageQueryLod:
        case IrOpcode::ImageSampleRaw:
        case IrOpcode::ImageGatherRaw:
        case IrOpcode::ImageSampleImplicitLod:
        case IrOpcode::ImageSampleExplicitLod:
            return {ImageAccess::Read, ImageResourceClass::Sampled, true};
        case IrOpcode::ImageWrite:
        case IrOpcode::StoreImage:
            return {ImageAccess::Write, ImageResourceClass::Storage, false};
        case IrOpcode::ImageAtomicSwap32:
        case IrOpcode::ImageAtomicIAdd32:
        case IrOpcode::ImageAtomicUMin32:
        case IrOpcode::ImageAtomicUMax32:
        case IrOpcode::ImageAtomicAnd32:
        case IrOpcode::ImageAtomicOr32:
        case IrOpcode::ImageAtomicXor32:
        case IrOpcode::ImageAtomicCmpSwap32:
        case IrOpcode::ImageAtomicISub32:
        case IrOpcode::ImageAtomicSMin32:
        case IrOpcode::ImageAtomicSMax32:
        case IrOpcode::ImageAtomicInc32:
        case IrOpcode::ImageAtomicDec32:
        case IrOpcode::ImageAtomicFCmpSwap32:
        case IrOpcode::ImageAtomicFMin32:
        case IrOpcode::ImageAtomicFMax32:
        case IrOpcode::ImageAtomicSwap64:
        case IrOpcode::ImageAtomicIAdd64:
        case IrOpcode::ImageAtomicISub64:
        case IrOpcode::ImageAtomicUMin64:
        case IrOpcode::ImageAtomicUMax64:
        case IrOpcode::ImageAtomicSMin64:
        case IrOpcode::ImageAtomicSMax64:
        case IrOpcode::ImageAtomicAnd64:
        case IrOpcode::ImageAtomicOr64:
        case IrOpcode::ImageAtomicXor64:
        case IrOpcode::ImageAtomicCmpSwap64:
            return {ImageAccess::Atomic, ImageResourceClass::Storage, false};
        default:
            return {};
    }
}

bool IsImageAtomic64Opcode(IrOpcode opcode) {
    return opcode >= IrOpcode::ImageAtomicSwap64 && opcode <= IrOpcode::ImageAtomicCmpSwap64;
}

bool IsFloat64Opcode(IrOpcode opcode) {
    return opcode >= IrOpcode::FPAdd64 && opcode <= IrOpcode::ConvertU32F64;
}

bool IrOpcodeHasSideEffects(IrOpcode opcode) {
    const BufferAccess bufferAccess = BufferAccessOf(opcode);
    if (bufferAccess == BufferAccess::Write || bufferAccess == BufferAccess::Atomic) {
        return true;
    }
    const SharedAccess sharedAccess = SharedAccessOf(opcode);
    if (sharedAccess == SharedAccess::Write || sharedAccess == SharedAccess::Atomic || sharedAccess == SharedAccess::Append || sharedAccess == SharedAccess::Consume) {
        return true;
    }
    const AddressAccess addressAccess = AddressOpcodeInfoOf(opcode).access;
    if (addressAccess == AddressAccess::Write || addressAccess == AddressAccess::Atomic) {
        return true;
    }
    const ImageAccess imageAccess = ImageOpcodeInfoOf(opcode).access;
    if (imageAccess == ImageAccess::Write || imageAccess == ImageAccess::Atomic) {
        return true;
    }
    switch (opcode) {
        case IrOpcode::SetRegister:
        case IrOpcode::Branch:
        case IrOpcode::BranchConditional:
        case IrOpcode::Loop:
        case IrOpcode::LoopMerge:
        case IrOpcode::Return:
        case IrOpcode::Unreachable:
        case IrOpcode::EmitVertex:
        case IrOpcode::Discard:
        case IrOpcode::Reference:
        case IrOpcode::ReferenceU32:
        case IrOpcode::SetTessellationAttribute:
        case IrOpcode::InterpolateHostP2:
        case IrOpcode::SetThreadBitScalarRegister:
        case IrOpcode::SetScalarMaskTag:
        case IrOpcode::SetScalarRegister:
        case IrOpcode::SetVectorRegister:
        case IrOpcode::SetGotoVariable:
        case IrOpcode::SetScc:
        case IrOpcode::SetExec:
        case IrOpcode::SetExecLo:
        case IrOpcode::SetExecHi:
        case IrOpcode::SetVcc:
        case IrOpcode::SetVccLo:
        case IrOpcode::SetVccHi:
        case IrOpcode::SetM0:
        case IrOpcode::MeshAllocate:
        case IrOpcode::Barrier:
        case IrOpcode::Waitcnt:
        case IrOpcode::Sendmsg:
        case IrOpcode::TtraceData:
        case IrOpcode::InstPrefetch:
        case IrOpcode::SetAttribute:
            return true;
        default:
            return false;
    }
}

std::string_view IrOpcodeName(IrOpcode opcode) {
    return metaOf(opcode).name;
}

}
