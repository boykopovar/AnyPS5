#include <cstdio>
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <bit>
#include <limits>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

namespace {

RdnaOperand d16Half(RdnaOperand operand, RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::BufferLoadUbyteD16:
        case RdnaOpcode::BufferLoadSbyteD16:
        case RdnaOpcode::BufferLoadShortD16:
        case RdnaOpcode::FlatLoadUbyteD16:
        case RdnaOpcode::FlatLoadSbyteD16:
        case RdnaOpcode::FlatLoadShortD16:
        case RdnaOpcode::DsReadU8D16:
        case RdnaOpcode::DsReadI8D16:
        case RdnaOpcode::DsReadU16D16:
        case RdnaOpcode::BufferLoadFormatD16X:
        case RdnaOpcode::TbufferLoadFormatD16X: operand.sdwaSel = 4u; break;
        case RdnaOpcode::BufferLoadUbyteD16Hi:
        case RdnaOpcode::BufferLoadSbyteD16Hi:
        case RdnaOpcode::BufferLoadShortD16Hi:
        case RdnaOpcode::BufferLoadFormatD16HiX:
        case RdnaOpcode::BufferStoreByteD16Hi:
        case RdnaOpcode::BufferStoreShortD16Hi:
        case RdnaOpcode::FlatLoadUbyteD16Hi:
        case RdnaOpcode::FlatLoadSbyteD16Hi:
        case RdnaOpcode::FlatLoadShortD16Hi:
        case RdnaOpcode::FlatStoreByteD16Hi:
        case RdnaOpcode::FlatStoreShortD16Hi:
        case RdnaOpcode::DsReadU8D16Hi:
        case RdnaOpcode::DsReadI8D16Hi:
        case RdnaOpcode::DsReadU16D16Hi:
        case RdnaOpcode::DsWriteB8D16Hi:
        case RdnaOpcode::DsWriteB16D16Hi: operand.sdwaSel = 5u; break;
        default: break;
    }
    return operand;
}

struct MemoryOpcodeInfo {
    std::uint32_t encoding;
    RdnaOpcode opcode;
    std::uint32_t dataDwords;
    std::uint32_t dataBits;
    bool dataSigned;
    bool typed;
    bool formatted;
};

constexpr MemoryOpcodeInfo smemOpcodes[] = {
    {0x00u, RdnaOpcode::SLoadDword, 1, 32, false, false, false},
    {0x01u, RdnaOpcode::SLoadDwordx2, 2, 32, false, false, false},
    {0x02u, RdnaOpcode::SLoadDwordx4, 4, 32, false, false, false},
    {0x03u, RdnaOpcode::SLoadDwordx8, 8, 32, false, false, false},
    {0x04u, RdnaOpcode::SLoadDwordx16, 16, 32, false, false, false},
    {0x05u, RdnaOpcode::SScratchLoadDword, 1, 32, false, false, false},
    {0x06u, RdnaOpcode::SScratchLoadDwordx2, 2, 32, false, false, false},
    {0x07u, RdnaOpcode::SScratchLoadDwordx4, 4, 32, false, false, false},
    {0x08u, RdnaOpcode::SBufferLoadDword, 1, 32, false, false, false},
    {0x09u, RdnaOpcode::SBufferLoadDwordx2, 2, 32, false, false, false},
    {0x0au, RdnaOpcode::SBufferLoadDwordx4, 4, 32, false, false, false},
    {0x0bu, RdnaOpcode::SBufferLoadDwordx8, 8, 32, false, false, false},
    {0x0cu, RdnaOpcode::SBufferLoadDwordx16, 16, 32, false, false, false},
};

constexpr MemoryOpcodeInfo mubufOpcodes[] = {
    {0x00u, RdnaOpcode::BufferLoadFormatX, 1, 32, false, false, true},
    {0x01u, RdnaOpcode::BufferLoadFormatXy, 2, 32, false, false, true},
    {0x02u, RdnaOpcode::BufferLoadFormatXyz, 3, 32, false, false, true},
    {0x03u, RdnaOpcode::BufferLoadFormatXyzw, 4, 32, false, false, true},
    {0x04u, RdnaOpcode::BufferStoreFormatX, 1, 32, false, false, true},
    {0x05u, RdnaOpcode::BufferStoreFormatXy, 2, 32, false, false, true},
    {0x06u, RdnaOpcode::BufferStoreFormatXyz, 3, 32, false, false, true},
    {0x07u, RdnaOpcode::BufferStoreFormatXyzw, 4, 32, false, false, true},
    {0x08u, RdnaOpcode::BufferLoadUbyte, 1, 8, false, false, false},
    {0x09u, RdnaOpcode::BufferLoadSbyte, 1, 8, true, false, false},
    {0x0au, RdnaOpcode::BufferLoadUshort, 1, 16, false, false, false},
    {0x0bu, RdnaOpcode::BufferLoadSshort, 1, 16, true, false, false},
    {0x0cu, RdnaOpcode::BufferLoadDword, 1, 32, false, false, false},
    {0x0du, RdnaOpcode::BufferLoadDwordx2, 2, 32, false, false, false},
    {0x0eu, RdnaOpcode::BufferLoadDwordx4, 4, 32, false, false, false},
    {0x0fu, RdnaOpcode::BufferLoadDwordx3, 3, 32, false, false, false},
    {0x18u, RdnaOpcode::BufferStoreByte, 1, 8, false, false, false},
    {0x1au, RdnaOpcode::BufferStoreShort, 1, 16, false, false, false},
    {0x1cu, RdnaOpcode::BufferStoreDword, 1, 32, false, false, false},
    {0x1du, RdnaOpcode::BufferStoreDwordx2, 2, 32, false, false, false},
    {0x1eu, RdnaOpcode::BufferStoreDwordx4, 4, 32, false, false, false},
    {0x1fu, RdnaOpcode::BufferStoreDwordx3, 3, 32, false, false, false},
    {0x20u, RdnaOpcode::BufferLoadUbyteD16, 1, 8, false, false, false},
    {0x21u, RdnaOpcode::BufferLoadUbyteD16Hi, 1, 8, false, false, false},
    {0x22u, RdnaOpcode::BufferLoadSbyteD16, 1, 8, true, false, false},
    {0x23u, RdnaOpcode::BufferLoadSbyteD16Hi, 1, 8, true, false, false},
    {0x24u, RdnaOpcode::BufferLoadShortD16, 1, 16, false, false, false},
    {0x25u, RdnaOpcode::BufferLoadShortD16Hi, 1, 16, false, false, false},
    {0x26u, RdnaOpcode::BufferLoadFormatD16HiX, 1, 32, false, false, true},
    {0x27u, RdnaOpcode::BufferStoreFormatD16HiX, 1, 32, false, false, true},
    {0x19u, RdnaOpcode::BufferStoreByteD16Hi, 1, 8, false, false, false},
    {0x1bu, RdnaOpcode::BufferStoreShortD16Hi, 1, 16, false, false, false},
    {0x30u, RdnaOpcode::BufferAtomicSwap, 1, 32, false, false, false},
    {0x31u, RdnaOpcode::BufferAtomicCmpswap, 1, 32, false, false, false},
    {0x32u, RdnaOpcode::BufferAtomicAdd, 1, 32, false, false, false},
    {0x33u, RdnaOpcode::BufferAtomicSub, 1, 32, false, false, false},
    {0x34u, RdnaOpcode::BufferAtomicCsub, 1, 32, false, false, false},
    {0x35u, RdnaOpcode::BufferAtomicSmin, 1, 32, false, false, false},
    {0x36u, RdnaOpcode::BufferAtomicUmin, 1, 32, false, false, false},
    {0x37u, RdnaOpcode::BufferAtomicSmax, 1, 32, false, false, false},
    {0x38u, RdnaOpcode::BufferAtomicUmax, 1, 32, false, false, false},
    {0x39u, RdnaOpcode::BufferAtomicAnd, 1, 32, false, false, false},
    {0x3au, RdnaOpcode::BufferAtomicOr, 1, 32, false, false, false},
    {0x3bu, RdnaOpcode::BufferAtomicXor, 1, 32, false, false, false},
    {0x3fu, RdnaOpcode::BufferAtomicFmin, 1, 32, false, false, false},
    {0x40u, RdnaOpcode::BufferAtomicFmax, 1, 32, false, false, false},
    {0x50u, RdnaOpcode::BufferAtomicSwapX2, 2, 32, false, false, false},
    {0x5au, RdnaOpcode::BufferAtomicOrX2, 2, 32, false, false, false},
    {0x3cu, RdnaOpcode::BufferAtomicInc, 1, 32, false, false, false},
    {0x3du, RdnaOpcode::BufferAtomicDec, 1, 32, false, false, false},
    {0x51u, RdnaOpcode::BufferAtomicCmpswapX2, 2, 32, false, false, false},
    {0x52u, RdnaOpcode::BufferAtomicAddX2, 2, 32, false, false, false},
    {0x53u, RdnaOpcode::BufferAtomicSubX2, 2, 32, false, false, false},
    {0x55u, RdnaOpcode::BufferAtomicSminX2, 2, 32, false, false, false},
    {0x56u, RdnaOpcode::BufferAtomicUminX2, 2, 32, false, false, false},
    {0x57u, RdnaOpcode::BufferAtomicSmaxX2, 2, 32, false, false, false},
    {0x58u, RdnaOpcode::BufferAtomicUmaxX2, 2, 32, false, false, false},
    {0x59u, RdnaOpcode::BufferAtomicAndX2, 2, 32, false, false, false},
    {0x5bu, RdnaOpcode::BufferAtomicXorX2, 2, 32, false, false, false},
    {0x3eu, RdnaOpcode::BufferAtomicFcmpswap, 1, 32, false, false, false},
    {0x5cu, RdnaOpcode::BufferAtomicIncX2, 2, 32, false, false, false},
    {0x5du, RdnaOpcode::BufferAtomicDecX2, 2, 32, false, false, false},
    {0x5eu, RdnaOpcode::BufferAtomicFcmpswapX2, 2, 32, false, false, false},
    {0x5fu, RdnaOpcode::BufferAtomicFminX2, 2, 32, false, false, false},
    {0x60u, RdnaOpcode::BufferAtomicFmaxX2, 2, 32, false, false, false},
    {0x80u, RdnaOpcode::BufferLoadFormatD16X, 1, 32, false, false, true},
    {0x81u, RdnaOpcode::BufferLoadFormatD16Xy, 2, 32, false, false, true},
    {0x82u, RdnaOpcode::BufferLoadFormatD16Xyz, 3, 32, false, false, true},
    {0x83u, RdnaOpcode::BufferLoadFormatD16Xyzw, 4, 32, false, false, true},
    {0x84u, RdnaOpcode::BufferStoreFormatD16X, 1, 32, false, false, true},
    {0x85u, RdnaOpcode::BufferStoreFormatD16Xy, 2, 32, false, false, true},
    {0x86u, RdnaOpcode::BufferStoreFormatD16Xyz, 3, 32, false, false, true},
    {0x87u, RdnaOpcode::BufferStoreFormatD16Xyzw, 4, 32, false, false, true},
};

constexpr MemoryOpcodeInfo mtbufOpcodes[] = {
    {0x00u, RdnaOpcode::TbufferLoadFormatX, 1, 32, false, true, true},
    {0x01u, RdnaOpcode::TbufferLoadFormatXy, 2, 32, false, true, true},
    {0x02u, RdnaOpcode::TbufferLoadFormatXyz, 3, 32, false, true, true},
    {0x03u, RdnaOpcode::TbufferLoadFormatXyzw, 4, 32, false, true, true},
    {0x04u, RdnaOpcode::TbufferStoreFormatX, 1, 32, false, true, true},
    {0x05u, RdnaOpcode::TbufferStoreFormatXy, 2, 32, false, true, true},
    {0x06u, RdnaOpcode::TbufferStoreFormatXyz, 3, 32, false, true, true},
    {0x07u, RdnaOpcode::TbufferStoreFormatXyzw, 4, 32, false, true, true},
    {0x08u, RdnaOpcode::TbufferLoadFormatD16X, 1, 32, false, true, true},
    {0x09u, RdnaOpcode::TbufferLoadFormatD16Xy, 2, 32, false, true, true},
    {0x0au, RdnaOpcode::TbufferLoadFormatD16Xyz, 3, 32, false, true, true},
    {0x0bu, RdnaOpcode::TbufferLoadFormatD16Xyzw, 4, 32, false, true, true},
    {0x0cu, RdnaOpcode::TbufferStoreFormatD16X, 1, 32, false, true, true},
    {0x0du, RdnaOpcode::TbufferStoreFormatD16Xy, 2, 32, false, true, true},
    {0x0eu, RdnaOpcode::TbufferStoreFormatD16Xyz, 3, 32, false, true, true},
    {0x0fu, RdnaOpcode::TbufferStoreFormatD16Xyzw, 4, 32, false, true, true},
};

constexpr MemoryOpcodeInfo flatOpcodes[] = {
    {0x08u, RdnaOpcode::FlatLoadUbyte, 1, 8, false, false, false},
    {0x09u, RdnaOpcode::FlatLoadSbyte, 1, 8, true, false, false},
    {0x0au, RdnaOpcode::FlatLoadUshort, 1, 16, false, false, false},
    {0x0bu, RdnaOpcode::FlatLoadSshort, 1, 16, true, false, false},
    {0x0cu, RdnaOpcode::FlatLoadDword, 1, 32, false, false, false},
    {0x0du, RdnaOpcode::FlatLoadDwordx2, 2, 32, false, false, false},
    {0x0eu, RdnaOpcode::FlatLoadDwordx4, 4, 32, false, false, false},
    {0x0fu, RdnaOpcode::FlatLoadDwordx3, 3, 32, false, false, false},
    {0x16u, RdnaOpcode::GlobalLoadDwordAddtid, 1, 32, false, false, false},
    {0x17u, RdnaOpcode::GlobalStoreDwordAddtid, 1, 32, false, false, false},
    {0x18u, RdnaOpcode::FlatStoreByte, 1, 8, false, false, false},
    {0x1au, RdnaOpcode::FlatStoreShort, 1, 16, false, false, false},
    {0x1cu, RdnaOpcode::FlatStoreDword, 1, 32, false, false, false},
    {0x1du, RdnaOpcode::FlatStoreDwordx2, 2, 32, false, false, false},
    {0x1eu, RdnaOpcode::FlatStoreDwordx4, 4, 32, false, false, false},
    {0x1fu, RdnaOpcode::FlatStoreDwordx3, 3, 32, false, false, false},
    {0x20u, RdnaOpcode::FlatLoadUbyteD16, 1, 8, false, false, false},
    {0x21u, RdnaOpcode::FlatLoadUbyteD16Hi, 1, 8, false, false, false},
    {0x22u, RdnaOpcode::FlatLoadSbyteD16, 1, 8, true, false, false},
    {0x23u, RdnaOpcode::FlatLoadSbyteD16Hi, 1, 8, true, false, false},
    {0x24u, RdnaOpcode::FlatLoadShortD16, 1, 16, false, false, false},
    {0x25u, RdnaOpcode::FlatLoadShortD16Hi, 1, 16, false, false, false},
    {0x19u, RdnaOpcode::FlatStoreByteD16Hi, 1, 8, false, false, false},
    {0x1bu, RdnaOpcode::FlatStoreShortD16Hi, 1, 16, false, false, false},
    {0x30u, RdnaOpcode::FlatAtomicSwap, 1, 32, false, false, false},
    {0x31u, RdnaOpcode::FlatAtomicCmpswap, 1, 32, false, false, false},
    {0x32u, RdnaOpcode::FlatAtomicAdd, 1, 32, false, false, false},
    {0x33u, RdnaOpcode::FlatAtomicSub, 1, 32, false, false, false},
    {0x34u, RdnaOpcode::GlobalAtomicCsub, 1, 32, false, false, false},
    {0x35u, RdnaOpcode::FlatAtomicSmin, 1, 32, false, false, false},
    {0x36u, RdnaOpcode::FlatAtomicUmin, 1, 32, false, false, false},
    {0x37u, RdnaOpcode::FlatAtomicSmax, 1, 32, false, false, false},
    {0x38u, RdnaOpcode::FlatAtomicUmax, 1, 32, false, false, false},
    {0x39u, RdnaOpcode::FlatAtomicAnd, 1, 32, false, false, false},
    {0x3au, RdnaOpcode::FlatAtomicOr, 1, 32, false, false, false},
    {0x3bu, RdnaOpcode::FlatAtomicXor, 1, 32, false, false, false},
    {0x3cu, RdnaOpcode::FlatAtomicInc, 1, 32, false, false, false},
    {0x3du, RdnaOpcode::FlatAtomicDec, 1, 32, false, false, false},
    {0x50u, RdnaOpcode::FlatAtomicSwapX2, 2, 32, false, false, false},
    {0x51u, RdnaOpcode::FlatAtomicCmpswapX2, 2, 32, false, false, false},
    {0x52u, RdnaOpcode::FlatAtomicAddX2, 2, 32, false, false, false},
    {0x53u, RdnaOpcode::FlatAtomicSubX2, 2, 32, false, false, false},
    {0x55u, RdnaOpcode::FlatAtomicSminX2, 2, 32, false, false, false},
    {0x56u, RdnaOpcode::FlatAtomicUminX2, 2, 32, false, false, false},
    {0x57u, RdnaOpcode::FlatAtomicSmaxX2, 2, 32, false, false, false},
    {0x58u, RdnaOpcode::FlatAtomicUmaxX2, 2, 32, false, false, false},
    {0x59u, RdnaOpcode::FlatAtomicAndX2, 2, 32, false, false, false},
    {0x5au, RdnaOpcode::FlatAtomicOrX2, 2, 32, false, false, false},
    {0x5bu, RdnaOpcode::FlatAtomicXorX2, 2, 32, false, false, false},
    {0x3eu, RdnaOpcode::FlatAtomicFcmpswap, 1, 32, false, false, false},
    {0x3fu, RdnaOpcode::FlatAtomicFmin, 1, 32, false, false, false},
    {0x40u, RdnaOpcode::FlatAtomicFmax, 1, 32, false, false, false},
    {0x5eu, RdnaOpcode::FlatAtomicFcmpswapX2, 2, 32, false, false, false},
    {0x5fu, RdnaOpcode::FlatAtomicFminX2, 2, 32, false, false, false},
    {0x60u, RdnaOpcode::FlatAtomicFmaxX2, 2, 32, false, false, false},
    {0x5cu, RdnaOpcode::FlatAtomicIncX2, 2, 32, false, false, false},
    {0x5du, RdnaOpcode::FlatAtomicDecX2, 2, 32, false, false, false},
};

constexpr MemoryOpcodeInfo dsOpcodes[] = {
    {0x00u, RdnaOpcode::DsAddU32, 1, 32, false, false, false},
    {0x02u, RdnaOpcode::DsRsubU32, 1, 32, false, false, false},
    {0x03u, RdnaOpcode::DsIncU32, 1, 32, false, false, false},
    {0x04u, RdnaOpcode::DsDecU32, 1, 32, false, false, false},
    {0x0cu, RdnaOpcode::DsMskorB32, 1, 32, false, false, false},
    {0x10u, RdnaOpcode::DsCmpstB32, 1, 32, false, false, false},
    {0x11u, RdnaOpcode::DsCmpstF32, 1, 32, false, false, false},
    {0x14u, RdnaOpcode::DsNop, 1, 32, false, false, false},
    {0x15u, RdnaOpcode::DsAddF32, 1, 32, false, false, false},
    {0x18u, RdnaOpcode::DsGwsSemaReleaseAll, 1, 32, false, false, false},
    {0x19u, RdnaOpcode::DsGwsInit, 1, 32, false, false, false},
    {0x1au, RdnaOpcode::DsGwsSemaV, 1, 32, false, false, false},
    {0x1bu, RdnaOpcode::DsGwsSemaBr, 1, 32, false, false, false},
    {0x1cu, RdnaOpcode::DsGwsSemaP, 1, 32, false, false, false},
    {0x1du, RdnaOpcode::DsGwsBarrier, 1, 32, false, false, false},
    {0x22u, RdnaOpcode::DsRsubRtnU32, 1, 32, false, false, false},
    {0x2cu, RdnaOpcode::DsMskorRtnB32, 1, 32, false, false, false},
    {0x30u, RdnaOpcode::DsCmpstRtnB32, 1, 32, false, false, false},
    {0x31u, RdnaOpcode::DsCmpstRtnF32, 1, 32, false, false, false},
    {0x32u, RdnaOpcode::DsMinRtnF32, 1, 32, false, false, false},
    {0x33u, RdnaOpcode::DsMaxRtnF32, 1, 32, false, false, false},
    {0x34u, RdnaOpcode::DsWrapRtnB32, 1, 32, false, false, false},
    {0x55u, RdnaOpcode::DsAddRtnF32, 1, 32, false, false, false},
    {0x40u, RdnaOpcode::DsAddU64, 2, 32, false, false, false},
    {0x41u, RdnaOpcode::DsSubU64, 2, 32, false, false, false},
    {0x42u, RdnaOpcode::DsRsubU64, 2, 32, false, false, false},
    {0x43u, RdnaOpcode::DsIncU64, 2, 32, false, false, false},
    {0x44u, RdnaOpcode::DsDecU64, 2, 32, false, false, false},
    {0x45u, RdnaOpcode::DsMinI64, 2, 32, false, false, false},
    {0x46u, RdnaOpcode::DsMaxI64, 2, 32, false, false, false},
    {0x47u, RdnaOpcode::DsMinU64, 2, 32, false, false, false},
    {0x48u, RdnaOpcode::DsMaxU64, 2, 32, false, false, false},
    {0x49u, RdnaOpcode::DsAndB64, 2, 32, false, false, false},
    {0x4au, RdnaOpcode::DsOrB64, 2, 32, false, false, false},
    {0x4bu, RdnaOpcode::DsXorB64, 2, 32, false, false, false},
    {0x4cu, RdnaOpcode::DsMskorB64, 2, 32, false, false, false},
    {0x50u, RdnaOpcode::DsCmpstB64, 2, 32, false, false, false},
    {0x51u, RdnaOpcode::DsCmpstF64, 2, 32, false, false, false},
    {0x52u, RdnaOpcode::DsMinF64, 2, 32, false, false, false},
    {0x53u, RdnaOpcode::DsMaxF64, 2, 32, false, false, false},
    {0x60u, RdnaOpcode::DsAddRtnU64, 2, 32, false, false, false},
    {0x61u, RdnaOpcode::DsSubRtnU64, 2, 32, false, false, false},
    {0x62u, RdnaOpcode::DsRsubRtnU64, 2, 32, false, false, false},
    {0x63u, RdnaOpcode::DsIncRtnU64, 2, 32, false, false, false},
    {0x64u, RdnaOpcode::DsDecRtnU64, 2, 32, false, false, false},
    {0x65u, RdnaOpcode::DsMinRtnI64, 2, 32, false, false, false},
    {0x66u, RdnaOpcode::DsMaxRtnI64, 2, 32, false, false, false},
    {0x67u, RdnaOpcode::DsMinRtnU64, 2, 32, false, false, false},
    {0x68u, RdnaOpcode::DsMaxRtnU64, 2, 32, false, false, false},
    {0x69u, RdnaOpcode::DsAndRtnB64, 2, 32, false, false, false},
    {0x6au, RdnaOpcode::DsOrRtnB64, 2, 32, false, false, false},
    {0x6bu, RdnaOpcode::DsXorRtnB64, 2, 32, false, false, false},
    {0x6cu, RdnaOpcode::DsMskorRtnB64, 2, 32, false, false, false},
    {0x6du, RdnaOpcode::DsWrxchgRtnB64, 2, 32, false, false, false},
    {0x70u, RdnaOpcode::DsCmpstRtnB64, 2, 32, false, false, false},
    {0x71u, RdnaOpcode::DsCmpstRtnF64, 2, 32, false, false, false},
    {0x72u, RdnaOpcode::DsMinRtnF64, 2, 32, false, false, false},
    {0x73u, RdnaOpcode::DsMaxRtnF64, 2, 32, false, false, false},
    {0x80u, RdnaOpcode::DsAddSrc2U32, 1, 32, false, false, false},
    {0x81u, RdnaOpcode::DsSubSrc2U32, 1, 32, false, false, false},
    {0x82u, RdnaOpcode::DsRsubSrc2U32, 1, 32, false, false, false},
    {0x83u, RdnaOpcode::DsIncSrc2U32, 1, 32, false, false, false},
    {0x84u, RdnaOpcode::DsDecSrc2U32, 1, 32, false, false, false},
    {0x85u, RdnaOpcode::DsMinSrc2I32, 1, 32, false, false, false},
    {0x86u, RdnaOpcode::DsMaxSrc2I32, 1, 32, false, false, false},
    {0x87u, RdnaOpcode::DsMinSrc2U32, 1, 32, false, false, false},
    {0x88u, RdnaOpcode::DsMaxSrc2U32, 1, 32, false, false, false},
    {0x89u, RdnaOpcode::DsAndSrc2B32, 1, 32, false, false, false},
    {0x8au, RdnaOpcode::DsOrSrc2B32, 1, 32, false, false, false},
    {0x8bu, RdnaOpcode::DsXorSrc2B32, 1, 32, false, false, false},
    {0x8du, RdnaOpcode::DsWriteSrc2B32, 1, 32, false, false, false},
    {0x92u, RdnaOpcode::DsMinSrc2F32, 1, 32, false, false, false},
    {0x93u, RdnaOpcode::DsMaxSrc2F32, 1, 32, false, false, false},
    {0x95u, RdnaOpcode::DsAddSrc2F32, 1, 32, false, false, false},
    {0x01u, RdnaOpcode::DsSubU32, 1, 32, false, false, false},
    {0x05u, RdnaOpcode::DsMinI32, 1, 32, false, false, false},
    {0x06u, RdnaOpcode::DsMaxI32, 1, 32, false, false, false},
    {0x07u, RdnaOpcode::DsMinU32, 1, 32, false, false, false},
    {0x08u, RdnaOpcode::DsMaxU32, 1, 32, false, false, false},
    {0x09u, RdnaOpcode::DsAndB32, 1, 32, false, false, false},
    {0x0au, RdnaOpcode::DsOrB32, 1, 32, false, false, false},
    {0x0bu, RdnaOpcode::DsXorB32, 1, 32, false, false, false},
    {0x0du, RdnaOpcode::DsWriteB32, 1, 32, false, false, false},
    {0x0eu, RdnaOpcode::DsWrite2B32, 2, 32, false, false, false},
    {0x0fu, RdnaOpcode::DsWrite2st64B32, 2, 32, false, false, false},
    {0x12u, RdnaOpcode::DsMinF32, 1, 32, false, false, false},
    {0x13u, RdnaOpcode::DsMaxF32, 1, 32, false, false, false},
    {0x1eu, RdnaOpcode::DsWriteB8, 1, 8, false, false, false},
    {0x1fu, RdnaOpcode::DsWriteB16, 1, 16, false, false, false},
    {0x20u, RdnaOpcode::DsAddRtnU32, 1, 32, false, false, false},
    {0x21u, RdnaOpcode::DsSubRtnU32, 1, 32, false, false, false},
    {0x23u, RdnaOpcode::DsIncRtnU32, 1, 32, false, false, false},
    {0x24u, RdnaOpcode::DsDecRtnU32, 1, 32, false, false, false},
    {0x25u, RdnaOpcode::DsMinRtnI32, 1, 32, false, false, false},
    {0x26u, RdnaOpcode::DsMaxRtnI32, 1, 32, false, false, false},
    {0x27u, RdnaOpcode::DsMinRtnU32, 1, 32, false, false, false},
    {0x28u, RdnaOpcode::DsMaxRtnU32, 1, 32, false, false, false},
    {0x29u, RdnaOpcode::DsAndRtnB32, 1, 32, false, false, false},
    {0x2au, RdnaOpcode::DsOrRtnB32, 1, 32, false, false, false},
    {0x2bu, RdnaOpcode::DsXorRtnB32, 1, 32, false, false, false},
    {0x2du, RdnaOpcode::DsWrxchgRtnB32, 1, 32, false, false, false},
    {0x2eu, RdnaOpcode::DsWrxchg2RtnB32, 2, 32, false, false, false},
    {0x2fu, RdnaOpcode::DsWrxchg2st64RtnB32, 2, 32, false, false, false},
    {0x35u, RdnaOpcode::DsSwizzleB32, 1, 32, false, false, false},
    {0x36u, RdnaOpcode::DsReadB32, 1, 32, false, false, false},
    {0x37u, RdnaOpcode::DsRead2B32, 2, 32, false, false, false},
    {0x38u, RdnaOpcode::DsRead2st64B32, 2, 32, false, false, false},
    {0x39u, RdnaOpcode::DsReadI8, 1, 8, true, false, false},
    {0x3au, RdnaOpcode::DsReadU8, 1, 8, false, false, false},
    {0x3bu, RdnaOpcode::DsReadI16, 1, 16, true, false, false},
    {0x3cu, RdnaOpcode::DsReadU16, 1, 16, false, false, false},
    {0x3du, RdnaOpcode::DsConsume, 1, 32, false, false, false},
    {0x3eu, RdnaOpcode::DsAppend, 1, 32, false, false, false},
    {0x3fu, RdnaOpcode::DsOrderedCount, 1, 32, false, false, false},
    {0x4du, RdnaOpcode::DsWriteB64, 2, 32, false, false, false},
    {0x4eu, RdnaOpcode::DsWrite2B64, 4, 32, false, false, false},
    {0x4fu, RdnaOpcode::DsWrite2st64B64, 4, 32, false, false, false},
    {0x6eu, RdnaOpcode::DsWrxchg2RtnB64, 4, 32, false, false, false},
    {0x6fu, RdnaOpcode::DsWrxchg2st64RtnB64, 4, 32, false, false, false},
    {0x76u, RdnaOpcode::DsReadB64, 2, 32, false, false, false},
    {0x77u, RdnaOpcode::DsRead2B64, 4, 32, false, false, false},
    {0x78u, RdnaOpcode::DsRead2st64B64, 4, 32, false, false, false},
    {0x7eu, RdnaOpcode::DsCondxchg32RtnB64, 2, 32, false, false, false},
    {0xa0u, RdnaOpcode::DsWriteB8D16Hi, 1, 8, false, false, false},
    {0xa1u, RdnaOpcode::DsWriteB16D16Hi, 1, 16, false, false, false},
    {0xa2u, RdnaOpcode::DsReadU8D16, 1, 8, false, false, false},
    {0xa3u, RdnaOpcode::DsReadU8D16Hi, 1, 8, false, false, false},
    {0xa4u, RdnaOpcode::DsReadI8D16, 1, 8, true, false, false},
    {0xa5u, RdnaOpcode::DsReadI8D16Hi, 1, 8, true, false, false},
    {0xa6u, RdnaOpcode::DsReadU16D16, 1, 16, false, false, false},
    {0xa7u, RdnaOpcode::DsReadU16D16Hi, 1, 16, false, false, false},
    {0xb0u, RdnaOpcode::DsWriteAddtidB32, 1, 32, false, false, false},
    {0xb1u, RdnaOpcode::DsReadAddtidB32, 1, 32, false, false, false},
    {0xb2u, RdnaOpcode::DsPermuteB32, 1, 32, false, false, false},
    {0xb3u, RdnaOpcode::DsBpermuteB32, 1, 32, false, false, false},
    {0xdeu, RdnaOpcode::DsWriteB96, 3, 32, false, false, false},
    {0xdfu, RdnaOpcode::DsWriteB128, 4, 32, false, false, false},
    {0xfeu, RdnaOpcode::DsReadB96, 3, 32, false, false, false},
    {0xffu, RdnaOpcode::DsReadB128, 4, 32, false, false, false},
};

template <std::size_t Size>
const MemoryOpcodeInfo& lookupOpcode(const MemoryOpcodeInfo (&table)[Size], std::uint32_t encoding, const char* notSupportedReason) {
    for (const auto& entry : table) {
        if (entry.encoding == encoding) {
            return entry;
        }
    }
    throw std::runtime_error(notSupportedReason);
}

std::uint32_t signExtend(std::uint32_t value, std::uint32_t bits) {
    if (bits == 0u || bits >= 32u) {
        return value;
    }
    const std::uint32_t sign = 1u << (bits - 1u);
    return (value ^ sign) - sign;
}

std::uint32_t floatBits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

void applyMemoryInfo(RdnaInstruction& instruction, const MemoryOpcodeInfo& info) {
    instruction.op = info.opcode;
    instruction.dataDwordCount = info.dataDwords;
    instruction.dataBits = info.dataBits;
    instruction.dataSigned = info.dataSigned;
    instruction.typed = info.typed;
    instruction.formatted = info.formatted;
}

RdnaOperand vectorRegister(std::uint32_t reg) {
    RdnaOperand operand{};
    operand.kind = RdnaOperandKind::VectorRegister;
    operand.reg = reg;
    return operand;
}

RdnaOperand scalarSource(std::uint32_t code) {
    RdnaOperand operand{};
    if (code <= 105u) {
        operand.kind = RdnaOperandKind::ScalarRegister;
        operand.reg = code;
        return operand;
    }
    if (code >= 128u && code <= 192u) {
        operand.kind = RdnaOperandKind::IntegerInlineConstant;
        operand.signedVal = static_cast<std::int32_t>(code - 128u);
        operand.value = static_cast<std::uint32_t>(operand.signedVal);
        return operand;
    }
    if (code >= 193u && code <= 208u) {
        operand.kind = RdnaOperandKind::IntegerInlineConstant;
        operand.signedVal = 192 - static_cast<std::int32_t>(code);
        operand.value = static_cast<std::uint32_t>(operand.signedVal);
        return operand;
    }
    if (code >= 240u && code <= 247u) {
        constexpr float values[] = {0.5f, -0.5f, 1.0f, -1.0f, 2.0f, -2.0f, 4.0f, -4.0f};
        operand.kind = RdnaOperandKind::FloatInlineConstant;
        operand.value = floatBits(values[code - 240u]);
        return operand;
    }
    switch (code) {
        case 106u: operand.kind = RdnaOperandKind::VccLo; return operand;
        case 107u: operand.kind = RdnaOperandKind::VccHi; return operand;
        case 124u: operand.kind = RdnaOperandKind::M0; return operand;
        case 125u: operand.kind = RdnaOperandKind::Null; return operand;
        case 126u: operand.kind = RdnaOperandKind::ExecLo; return operand;
        case 127u: operand.kind = RdnaOperandKind::ExecHi; return operand;
        case 239u: operand.kind = RdnaOperandKind::PopsExitingWaveId; return operand;
        case 248u: operand.kind = RdnaOperandKind::FloatInlineConstant; operand.value = floatBits(0.15915494309189535f); return operand;
        case 251u: operand.kind = RdnaOperandKind::VccZ; return operand;
        case 252u: operand.kind = RdnaOperandKind::ExecZ; return operand;
        case 253u: operand.kind = RdnaOperandKind::Scc; return operand;
        default: throw std::runtime_error("unsupported scalar source operand");
    }
}

RdnaOperand scalarDestination(std::uint32_t code) {
    RdnaOperand operand{};
    if (code <= 105u) {
        operand.kind = RdnaOperandKind::ScalarRegister;
        operand.reg = code;
        return operand;
    }
    switch (code) {
        case 106u: operand.kind = RdnaOperandKind::VccLo; return operand;
        case 107u: operand.kind = RdnaOperandKind::VccHi; return operand;
        case 124u: operand.kind = RdnaOperandKind::M0; return operand;
        case 125u: operand.kind = RdnaOperandKind::Null; return operand;
        case 126u: operand.kind = RdnaOperandKind::ExecLo; return operand;
        case 127u: operand.kind = RdnaOperandKind::ExecHi; return operand;
        default: throw std::runtime_error("unsupported scalar destination operand");
    }
}

RdnaOperand scalarDescriptorBase(std::uint32_t reg, std::uint32_t registerCount, const char* reason) {
    const auto operand = scalarSource(reg);
    if (registerCount == 2u && operand.kind == RdnaOperandKind::VccLo) {
        return operand;
    }
    if (operand.kind != RdnaOperandKind::ScalarRegister || reg + (registerCount - 1u) > 105u) {
        throw std::runtime_error(reason);
    }
    return operand;
}

bool isDsWriteOpcode(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::DsWriteB8:
        case RdnaOpcode::DsWriteB16:
        case RdnaOpcode::DsWriteB16D16Hi:
        case RdnaOpcode::DsWriteB8D16Hi:
        case RdnaOpcode::DsWrite2B32:
        case RdnaOpcode::DsWrite2st64B32:
        case RdnaOpcode::DsWrite2B64:
        case RdnaOpcode::DsWrite2st64B64:
        case RdnaOpcode::DsWriteB32:
        case RdnaOpcode::DsWriteB64:
        case RdnaOpcode::DsWriteB96:
        case RdnaOpcode::DsWriteB128: return true;
        default: return false;
    }
}

bool isDsGwsOpcode(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::DsGwsInit:
        case RdnaOpcode::DsGwsSemaV:
        case RdnaOpcode::DsGwsSemaBr:
        case RdnaOpcode::DsGwsSemaP:
        case RdnaOpcode::DsGwsSemaReleaseAll:
        case RdnaOpcode::DsGwsBarrier: return true;
        default: return false;
    }
}

bool isDsAtomicOpcode(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::DsAddU32:
        case RdnaOpcode::DsAddRtnU32:
        case RdnaOpcode::DsSubU32:
        case RdnaOpcode::DsSubRtnU32:
        case RdnaOpcode::DsIncRtnU32:
        case RdnaOpcode::DsDecRtnU32:
        case RdnaOpcode::DsMinI32:
        case RdnaOpcode::DsMinRtnI32:
        case RdnaOpcode::DsMaxI32:
        case RdnaOpcode::DsMaxRtnI32:
        case RdnaOpcode::DsMinU32:
        case RdnaOpcode::DsMinRtnU32:
        case RdnaOpcode::DsMaxU32:
        case RdnaOpcode::DsMaxRtnU32:
        case RdnaOpcode::DsAndB32:
        case RdnaOpcode::DsAndRtnB32:
        case RdnaOpcode::DsOrB32:
        case RdnaOpcode::DsOrRtnB32:
        case RdnaOpcode::DsXorB32:
        case RdnaOpcode::DsXorRtnB32:
        case RdnaOpcode::DsWrxchgRtnB32:
        case RdnaOpcode::DsRsubU32:
        case RdnaOpcode::DsIncU32:
        case RdnaOpcode::DsDecU32:
        case RdnaOpcode::DsAddF32:
        case RdnaOpcode::DsRsubRtnU32:
        case RdnaOpcode::DsMinRtnF32:
        case RdnaOpcode::DsMaxRtnF32:
        case RdnaOpcode::DsAddRtnF32:
        case RdnaOpcode::DsAddU64:
        case RdnaOpcode::DsSubU64:
        case RdnaOpcode::DsRsubU64:
        case RdnaOpcode::DsIncU64:
        case RdnaOpcode::DsDecU64:
        case RdnaOpcode::DsMinI64:
        case RdnaOpcode::DsMaxI64:
        case RdnaOpcode::DsMinU64:
        case RdnaOpcode::DsMaxU64:
        case RdnaOpcode::DsAndB64:
        case RdnaOpcode::DsOrB64:
        case RdnaOpcode::DsXorB64:
        case RdnaOpcode::DsMinF64:
        case RdnaOpcode::DsMaxF64:
        case RdnaOpcode::DsAddRtnU64:
        case RdnaOpcode::DsSubRtnU64:
        case RdnaOpcode::DsRsubRtnU64:
        case RdnaOpcode::DsIncRtnU64:
        case RdnaOpcode::DsDecRtnU64:
        case RdnaOpcode::DsMinRtnI64:
        case RdnaOpcode::DsMaxRtnI64:
        case RdnaOpcode::DsMinRtnU64:
        case RdnaOpcode::DsMaxRtnU64:
        case RdnaOpcode::DsAndRtnB64:
        case RdnaOpcode::DsOrRtnB64:
        case RdnaOpcode::DsXorRtnB64:
        case RdnaOpcode::DsWrxchgRtnB64:
        case RdnaOpcode::DsCondxchg32RtnB64:
        case RdnaOpcode::DsMinRtnF64:
        case RdnaOpcode::DsMaxRtnF64:
            return true;
        default: return false;
    }
}

bool isDsSrc2Opcode(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::DsAddSrc2U32:
        case RdnaOpcode::DsSubSrc2U32:
        case RdnaOpcode::DsRsubSrc2U32:
        case RdnaOpcode::DsIncSrc2U32:
        case RdnaOpcode::DsDecSrc2U32:
        case RdnaOpcode::DsMinSrc2I32:
        case RdnaOpcode::DsMaxSrc2I32:
        case RdnaOpcode::DsMinSrc2U32:
        case RdnaOpcode::DsMaxSrc2U32:
        case RdnaOpcode::DsAndSrc2B32:
        case RdnaOpcode::DsOrSrc2B32:
        case RdnaOpcode::DsXorSrc2B32:
        case RdnaOpcode::DsWriteSrc2B32:
        case RdnaOpcode::DsMinSrc2F32:
        case RdnaOpcode::DsMaxSrc2F32:
        case RdnaOpcode::DsAddSrc2F32:
            return true;
        default: return false;
    }
}

std::uint32_t dsSourceCount(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::DsWrxchg2RtnB32:
        case RdnaOpcode::DsWrxchg2st64RtnB32:
        case RdnaOpcode::DsWrxchg2RtnB64:
        case RdnaOpcode::DsWrxchg2st64RtnB64:
        case RdnaOpcode::DsWrite2B32:
        case RdnaOpcode::DsWrite2st64B32:
        case RdnaOpcode::DsWrite2B64:
        case RdnaOpcode::DsWrite2st64B64:
        case RdnaOpcode::DsMskorB32:
        case RdnaOpcode::DsCmpstB32:
        case RdnaOpcode::DsCmpstF32:
        case RdnaOpcode::DsMskorRtnB32:
        case RdnaOpcode::DsCmpstRtnB32:
        case RdnaOpcode::DsCmpstRtnF32:
        case RdnaOpcode::DsWrapRtnB32:
        case RdnaOpcode::DsMskorB64:
        case RdnaOpcode::DsCmpstB64:
        case RdnaOpcode::DsCmpstF64:
        case RdnaOpcode::DsMskorRtnB64:
        case RdnaOpcode::DsCmpstRtnB64:
        case RdnaOpcode::DsCmpstRtnF64: return 3u;
        case RdnaOpcode::DsMinF32:
        case RdnaOpcode::DsMaxF32: return 2u;
        case RdnaOpcode::DsNop: return 0u;
        case RdnaOpcode::DsPermuteB32:
        case RdnaOpcode::DsBpermuteB32: return 2u;
        case RdnaOpcode::DsReadAddtidB32:
        case RdnaOpcode::DsConsume:
        case RdnaOpcode::DsAppend:
        case RdnaOpcode::DsGwsSemaV:
        case RdnaOpcode::DsGwsSemaP:
        case RdnaOpcode::DsGwsSemaReleaseAll: return 0u;
        default: return isDsWriteOpcode(opcode) || isDsAtomicOpcode(opcode) ? 2u : 1u;
    }
}

bool isFlatStoreOpcode(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::FlatStoreByte:
        case RdnaOpcode::FlatStoreShort:
        case RdnaOpcode::FlatStoreByteD16Hi:
        case RdnaOpcode::FlatStoreShortD16Hi:
        case RdnaOpcode::FlatStoreDword:
        case RdnaOpcode::FlatStoreDwordx2:
        case RdnaOpcode::FlatStoreDwordx3:
        case RdnaOpcode::FlatStoreDwordx4:
        case RdnaOpcode::GlobalStoreDwordAddtid: return true;
        default: return false;
    }
}

bool isFlatAtomicOpcode(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::FlatAtomicSwap:
        case RdnaOpcode::FlatAtomicCmpswap:
        case RdnaOpcode::FlatAtomicAdd:
        case RdnaOpcode::FlatAtomicSub:
        case RdnaOpcode::FlatAtomicSmin:
        case RdnaOpcode::FlatAtomicUmin:
        case RdnaOpcode::FlatAtomicSmax:
        case RdnaOpcode::FlatAtomicUmax:
        case RdnaOpcode::FlatAtomicAnd:
        case RdnaOpcode::FlatAtomicOr:
        case RdnaOpcode::FlatAtomicXor:
        case RdnaOpcode::FlatAtomicInc:
        case RdnaOpcode::FlatAtomicDec:
        case RdnaOpcode::FlatAtomicSwapX2:
        case RdnaOpcode::FlatAtomicCmpswapX2:
        case RdnaOpcode::FlatAtomicAddX2:
        case RdnaOpcode::FlatAtomicSubX2:
        case RdnaOpcode::FlatAtomicSminX2:
        case RdnaOpcode::FlatAtomicUminX2:
        case RdnaOpcode::FlatAtomicSmaxX2:
        case RdnaOpcode::FlatAtomicUmaxX2:
        case RdnaOpcode::FlatAtomicAndX2:
        case RdnaOpcode::FlatAtomicOrX2:
        case RdnaOpcode::FlatAtomicXorX2:
        case RdnaOpcode::FlatAtomicFcmpswap:
        case RdnaOpcode::FlatAtomicFmin:
        case RdnaOpcode::FlatAtomicFmax:
        case RdnaOpcode::FlatAtomicFcmpswapX2:
        case RdnaOpcode::FlatAtomicFminX2:
        case RdnaOpcode::FlatAtomicFmaxX2:
        case RdnaOpcode::FlatAtomicIncX2:
        case RdnaOpcode::FlatAtomicDecX2:
        case RdnaOpcode::GlobalAtomicCsub:
            return true;
        default: return false;
    }
}

void setRawWords(RdnaInstruction& instruction, std::span<const std::uint32_t> code, std::uint32_t wordIndex, std::uint32_t wordCount) {
    instruction.wordCount = wordCount;
    for (std::uint32_t i = 0; i < wordCount; ++i) {
        instruction.rawWords[i] = code[wordIndex + i];
    }
}

void requireTwoWords(std::span<const std::uint32_t> code, std::uint32_t wordIndex, std::uint32_t programCounter, const char* reason) {
    const std::size_t index = wordIndex;
    if (index >= code.size() || code.size() - index < 2u) {
        throw std::runtime_error(reason);
    }
    if (programCounter % 4u != 0u || programCounter > std::numeric_limits<std::uint32_t>::max() - 7u) {
        throw std::runtime_error("invalid memory instruction program counter");
    }
}

std::uint32_t toProgramCounter(std::uint32_t wordIndex) {
    if (wordIndex > std::numeric_limits<std::uint32_t>::max() / 4u) {
        throw std::runtime_error("memory instruction program counter overflow");
    }
    return wordIndex * 4u;
}

RdnaOpcode cacheControlOpcode(RdnaInstructionFamily family, std::uint32_t opcode) {
    if (family == RdnaInstructionFamily::SMEM) {
        switch (opcode) {
            case 0x1fu: return RdnaOpcode::SGl1Inv;
            case 0x20u: return RdnaOpcode::SDcacheInv;
            case 0x21u: return RdnaOpcode::SDcacheWb;
            case 0x26u: return RdnaOpcode::SAtcProbe;
            case 0x27u: return RdnaOpcode::SAtcProbeBuffer;
            case 0x28u: return RdnaOpcode::SDcacheDiscard;
            case 0x29u: return RdnaOpcode::SDcacheDiscardX2;
            default: return RdnaOpcode::Invalid;
        }
    }
    switch (opcode) {
        case 0x71u: return RdnaOpcode::BufferGl0Inv;
        case 0x72u: return RdnaOpcode::BufferGl1Inv;
        default: return RdnaOpcode::Invalid;
    }
}

RdnaInstruction cacheControlInstruction(RdnaInstructionFamily family, RdnaOpcode op, std::uint32_t opcode, std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    RdnaInstruction instruction{};
    instruction.programCounter = programCounter;
    instruction.family = family;
    instruction.opcodeId = opcode;
    instruction.op = op;
    setRawWords(instruction, code, wordIndex, 2u);
    return instruction;
}

}

RdnaInstruction DecodeRdnaSmem(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    requireTwoWords(code, wordIndex, programCounter, "truncated SMEM instruction");
    const std::size_t index = wordIndex;
    const auto word0 = code[index];
    const auto word1 = code[index + 1u];
    if ((word0 >> 26u) != 0x3Du) {
        throw std::runtime_error("instruction is not SMEM");
    }
    const auto opcode = (word0 >> 18u) & 0xFFu;
    const auto sdst = (word0 >> 6u) & 0x7Fu;
    const auto sbase = word0 & 0x3Fu;
    const auto soffsetCode = (word1 >> 25u) & 0x7Fu;
    if (opcode == 0x2au) {
        auto instruction = cacheControlInstruction(RdnaInstructionFamily::SMEM, RdnaOpcode::SGetWaveidInWorkgroup, opcode, programCounter, code, wordIndex);
        instruction.destination = scalarDestination(sdst);
        return instruction;
    }
    if (const auto cacheOp = cacheControlOpcode(RdnaInstructionFamily::SMEM, opcode); cacheOp != RdnaOpcode::Invalid) {
        return cacheControlInstruction(RdnaInstructionFamily::SMEM, cacheOp, opcode, programCounter, code, wordIndex);
    }
    if (opcode == 0x24u || opcode == 0x25u) {
        auto instruction = cacheControlInstruction(RdnaInstructionFamily::SMEM, opcode == 0x24u ? RdnaOpcode::SMemtime : RdnaOpcode::SMemrealtime, opcode, programCounter, code, wordIndex);
        instruction.destination = scalarDestination(sdst);
        return instruction;
    }
    const auto& info = lookupOpcode(smemOpcodes, opcode, "SMEM opcode is not supported");

    RdnaInstruction instruction{};
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::SMEM;
    instruction.opcodeId = opcode;
    instruction.glc = ((word0 >> 16u) & 1u) != 0u;
    instruction.memoryOffset = signExtend(word1 & 0x1FFFFFu, 21u);
    applyMemoryInfo(instruction, info);
    setRawWords(instruction, code, wordIndex, 2u);

    instruction.destination = scalarDestination(sdst);
    // The base pair may also be VCC (s106:107), which compilers use as a scratch address register.
    if (sbase * 2u == 106u) {
        RdnaOperand vcc{};
        vcc.kind = RdnaOperandKind::VccLo;
        instruction.source0 = vcc;
    } else if (sbase * 2u > 104u) {
        char reason[96];
        std::snprintf(reason, sizeof(reason), "SMEM base register range overflow (sbase s%u at pc 0x%x, words %08x %08x)", sbase * 2u, programCounter, word0, word1);
        throw std::runtime_error(reason);
    } else {
        instruction.source0 = scalarDescriptorBase(sbase * 2u, 2u, "SMEM base register range overflow");
    }
    instruction.source1 = scalarSource(soffsetCode);
    instruction.sourceCount = 2;
    return instruction;
}

RdnaInstruction DecodeRdnaMubuf(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    requireTwoWords(code, wordIndex, programCounter, "truncated MUBUF instruction");
    const std::size_t index = wordIndex;
    const auto word0 = code[index];
    const auto word1 = code[index + 1u];
    if ((word0 >> 26u) != 0x38u) {
        throw std::runtime_error("instruction is not MUBUF");
    }
    const auto opcode = ((word0 >> 18u) & 0x7Fu) | (((word0 >> 25u) & 1u) << 7u);
    const auto vdata = (word1 >> 8u) & 0xFFu;
    const auto vaddr = word1 & 0xFFu;
    const auto srsrc = (word1 >> 16u) & 0x1Fu;
    const auto soffsetCode = (word1 >> 24u) & 0xFFu;
    if (const auto cacheOp = cacheControlOpcode(RdnaInstructionFamily::MUBUF, opcode); cacheOp != RdnaOpcode::Invalid) {
        return cacheControlInstruction(RdnaInstructionFamily::MUBUF, cacheOp, opcode, programCounter, code, wordIndex);
    }
    const auto& info = lookupOpcode(mubufOpcodes, opcode, "MUBUF opcode is not supported");

    RdnaInstruction instruction{};
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::MUBUF;
    instruction.opcodeId = opcode;
    instruction.memoryOffset = word0 & 0xFFFu;
    instruction.offen = ((word0 >> 12u) & 1u) != 0u;
    instruction.idxen = ((word0 >> 13u) & 1u) != 0u;
    instruction.glc = ((word0 >> 14u) & 1u) != 0u;
    instruction.dlc = ((word0 >> 15u) & 1u) != 0u;
    instruction.slc = ((word1 >> 22u) & 1u) != 0u;
    applyMemoryInfo(instruction, info);
    setRawWords(instruction, code, wordIndex, 2u);

    instruction.destination = d16Half(vectorRegister(vdata), instruction.op);
    instruction.source0 = vectorRegister(vaddr);
    instruction.source1 = scalarDescriptorBase(srsrc * 4u, 4u, "MUBUF resource descriptor register range overflow");
    instruction.source2 = scalarSource(soffsetCode);
    instruction.sourceCount = 3;
    return instruction;
}

RdnaInstruction DecodeRdnaMtbuf(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    requireTwoWords(code, wordIndex, programCounter, "truncated MTBUF instruction");
    const std::size_t index = wordIndex;
    const auto word0 = code[index];
    const auto word1 = code[index + 1u];
    if ((word0 >> 26u) != 0x3Au) {
        throw std::runtime_error("instruction is not MTBUF");
    }
    const auto opcode = ((word0 >> 16u) & 0x7u) | (((word1 >> 21u) & 1u) << 3u);
    const auto dfmt = (word0 >> 19u) & 0xFu;
    const auto nfmt = (word0 >> 23u) & 0x7u;
    const auto vdata = (word1 >> 8u) & 0xFFu;
    const auto vaddr = word1 & 0xFFu;
    const auto srsrc = (word1 >> 16u) & 0x1Fu;
    const auto soffsetCode = (word1 >> 24u) & 0xFFu;
    const auto& info = lookupOpcode(mtbufOpcodes, opcode, "MTBUF opcode is not supported");

    RdnaInstruction instruction{};
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::MTBUF;
    instruction.opcodeId = opcode;
    instruction.dataFormat = dfmt;
    instruction.numberFormat = nfmt;
    instruction.memoryOffset = word0 & 0xFFFu;
    instruction.offen = ((word0 >> 12u) & 1u) != 0u;
    instruction.idxen = ((word0 >> 13u) & 1u) != 0u;
    instruction.glc = ((word0 >> 14u) & 1u) != 0u;
    instruction.dlc = ((word0 >> 15u) & 1u) != 0u;
    instruction.slc = ((word1 >> 22u) & 1u) != 0u;
    applyMemoryInfo(instruction, info);
    setRawWords(instruction, code, wordIndex, 2u);

    instruction.destination = d16Half(vectorRegister(vdata), instruction.op);
    instruction.source0 = vectorRegister(vaddr);
    instruction.source1 = scalarDescriptorBase(srsrc * 4u, 4u, "MTBUF resource descriptor register range overflow");
    instruction.source2 = scalarSource(soffsetCode);
    instruction.sourceCount = 3;
    return instruction;
}

RdnaInstruction DecodeRdnaFlat(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    requireTwoWords(code, wordIndex, programCounter, "truncated FLAT instruction");
    const std::size_t index = wordIndex;
    const auto word0 = code[index];
    const auto word1 = code[index + 1u];
    if ((word0 >> 26u) != 0x37u) {
        throw std::runtime_error("instruction is not FLAT");
    }
    const auto rawOffset = word0 & 0xFFFu;
    const auto dlc = ((word0 >> 12u) & 1u) != 0u;
    const auto lds = (word0 >> 13u) & 1u;
    const auto seg = (word0 >> 14u) & 0x3u;
    const auto glc = ((word0 >> 16u) & 1u) != 0u;
    const auto slc = ((word0 >> 17u) & 1u) != 0u;
    const auto opcode = (word0 >> 18u) & 0x7Fu;
    const auto vdst = (word1 >> 24u) & 0xFFu;
    const auto saddr = (word1 >> 16u) & 0x7Fu;
    const auto data = (word1 >> 8u) & 0xFFu;
    const auto addr = word1 & 0xFFu;
    const auto& info = lookupOpcode(flatOpcodes, opcode, "FLAT opcode is not supported");
    const bool atomic = isFlatAtomicOpcode(info.opcode);
    if (lds != 0u || (atomic && seg == 1u) || seg == 3u) {
        throw std::runtime_error("unsupported FLAT modifiers or segment");
    }
    if (info.opcode == RdnaOpcode::GlobalAtomicCsub && seg != 2u) {
        throw std::runtime_error("global_atomic_csub is available only in the global segment");
    }

    RdnaInstruction instruction{};
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::FLAT;
    instruction.opcodeId = opcode;
    instruction.memorySegment = seg;
    instruction.memoryOffset = seg == 0u ? (rawOffset & 0x7FFu) : signExtend(rawOffset, 12u);
    instruction.glc = glc;
    instruction.dlc = dlc;
    instruction.slc = slc;
    applyMemoryInfo(instruction, info);
    setRawWords(instruction, code, wordIndex, 2u);

    instruction.destination = d16Half(vectorRegister(isFlatStoreOpcode(instruction.op) ? data : vdst), instruction.op);
    if (instruction.op == RdnaOpcode::GlobalLoadDwordAddtid || instruction.op == RdnaOpcode::GlobalStoreDwordAddtid) {
        const std::string name = instruction.op == RdnaOpcode::GlobalLoadDwordAddtid ? "global_load_dword_addtid" : "global_store_dword_addtid";
        if (seg != 2u) {
            throw std::runtime_error(name + " is available only in the global segment");
        }
        instruction.source0 = scalarDescriptorBase(saddr, 2u, (name + " supports only an SGPR pair as base address").c_str());
        instruction.sourceCount = 1;
        return instruction;
    }
    instruction.source0 = vectorRegister(addr);
    if (seg == 0u || saddr == 0x7Du || saddr == 0x7Fu) {
        if (addr == 255u) {
            throw std::runtime_error("FLAT address register range overflow");
        }
        instruction.source1 = vectorRegister(addr + 1u);
    } else {
        instruction.source1 = scalarDescriptorBase(saddr, 2u, "FLAT scalar address register range overflow");
    }
    instruction.sourceCount = 2;
    if (atomic) {
        instruction.source2 = vectorRegister(data);
        instruction.sourceCount = 3;
        if (!glc) {
            instruction.destination = RdnaOperand{};
            instruction.destination.kind = RdnaOperandKind::None;
        }
    }
    return instruction;
}

RdnaInstruction DecodeRdnaDs(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    requireTwoWords(code, wordIndex, programCounter, "truncated DS instruction");
    const std::size_t index = wordIndex;
    const auto word0 = code[index];
    const auto word1 = code[index + 1u];
    if ((word0 >> 26u) != 0x36u) {
        throw std::runtime_error("instruction is not DS");
    }
    const auto opcode = (word0 >> 18u) & 0xFFu;
    const auto offset0 = word0 & 0xFFu;
    const auto offset1 = (word0 >> 8u) & 0xFFu;
    const auto gds = ((word0 >> 17u) & 1u) != 0u;
    const auto vdst = (word1 >> 24u) & 0xFFu;
    const auto data1 = (word1 >> 16u) & 0xFFu;
    const auto data0 = (word1 >> 8u) & 0xFFu;
    const auto addr = word1 & 0xFFu;
    const auto& info = lookupOpcode(dsOpcodes, opcode, "DS opcode is not supported");
    const auto combinedOffset = offset0 | (offset1 << 8u);
    if (info.opcode == RdnaOpcode::DsSwizzleB32 && combinedOffset >= 0xE000u) {
        throw std::runtime_error("DS swizzle FFT mode is not supported");
    }
    if (gds && (info.opcode == RdnaOpcode::DsSwizzleB32 || info.opcode == RdnaOpcode::DsBpermuteB32 || info.opcode == RdnaOpcode::DsPermuteB32 || info.opcode == RdnaOpcode::DsWriteAddtidB32 || info.opcode == RdnaOpcode::DsReadAddtidB32)) {
        throw std::runtime_error("DS lane operation is available only for LDS");
    }
    if (info.opcode == RdnaOpcode::DsWrxchg2RtnB32 || info.opcode == RdnaOpcode::DsWrxchg2st64RtnB32 || info.opcode == RdnaOpcode::DsWrxchg2RtnB64 || info.opcode == RdnaOpcode::DsWrxchg2st64RtnB64) {
        if (offset0 == offset1) {
            throw std::runtime_error("DS write exchange of one location through both offsets is not supported");
        }
        if (vdst + info.dataDwords > 256u) {
            throw std::runtime_error("DS write exchange destination register range overflow");
        }
    }
    if (info.opcode == RdnaOpcode::DsWriteAddtidB32 && data1 != 0u) {
        throw std::runtime_error("DS write addtid data1 operand is not supported");
    }
    if (info.opcode == RdnaOpcode::DsCondxchg32RtnB64) {
        if (data0 + 2u > 256u) {
            throw std::runtime_error("DS conditional exchange source register range overflow");
        }
        if (vdst + 2u > 256u) {
            throw std::runtime_error("DS conditional exchange destination register range overflow");
        }
    }
    if (info.opcode == RdnaOpcode::DsReadAddtidB32 && (data0 != 0u || data1 != 0u)) {
        throw std::runtime_error("DS read addtid data operands are not supported");
    }

    RdnaInstruction instruction{};
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::DS;
    instruction.opcodeId = opcode;
    instruction.gds = gds;
    instruction.memoryOffset = combinedOffset;
    applyMemoryInfo(instruction, info);
    setRawWords(instruction, code, wordIndex, 2u);

    if (instruction.op == RdnaOpcode::DsWrite2B32 || instruction.op == RdnaOpcode::DsRead2B32 || instruction.op == RdnaOpcode::DsWrxchg2RtnB32) {
        instruction.memoryOffset = offset0 * 4u;
        instruction.secondaryOffset = offset1 * 4u;
    } else if (instruction.op == RdnaOpcode::DsWrite2st64B32 || instruction.op == RdnaOpcode::DsRead2st64B32 || instruction.op == RdnaOpcode::DsWrxchg2st64RtnB32) {
        instruction.memoryOffset = offset0 * 256u;
        instruction.secondaryOffset = offset1 * 256u;
    } else if (instruction.op == RdnaOpcode::DsWrite2B64 || instruction.op == RdnaOpcode::DsRead2B64 || instruction.op == RdnaOpcode::DsWrxchg2RtnB64) {
        instruction.memoryOffset = offset0 * 8u;
        instruction.secondaryOffset = offset1 * 8u;
    } else if (instruction.op == RdnaOpcode::DsWrite2st64B64 || instruction.op == RdnaOpcode::DsRead2st64B64 || instruction.op == RdnaOpcode::DsWrxchg2st64RtnB64) {
        instruction.memoryOffset = offset0 * 512u;
        instruction.secondaryOffset = offset1 * 512u;
    } else if (isDsSrc2Opcode(instruction.op)) {
        if ((offset1 & 0x80u) != 0u) {
            throw std::runtime_error("DS src2 operation with the offset taken from the address is not supported");
        }
        instruction.memoryOffset = 0u;
        instruction.secondaryOffset = signExtend(combinedOffset & 0x7fffu, 15u) * 4u;
    }

    instruction.destination = d16Half(vectorRegister(vdst), instruction.op);
    instruction.source0 = vectorRegister(addr);
    instruction.source1 = isDsWriteOpcode(instruction.op) ? d16Half(vectorRegister(data0), instruction.op) : vectorRegister(data0);
    instruction.source2 = vectorRegister(data1);
    instruction.sourceCount = dsSourceCount(instruction.op);
    if (isDsGwsOpcode(instruction.op)) {
        instruction.destination.kind = RdnaOperandKind::None;
    }
    return instruction;
}

RdnaInstruction DecodeRdnaMemoryOp(std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    const auto programCounter = toProgramCounter(wordIndex);
    if (static_cast<std::size_t>(wordIndex) >= code.size()) {
        throw std::runtime_error("truncated memory instruction");
    }
    switch (code[wordIndex] >> 26u) {
        case 0x36u: return DecodeRdnaDs(programCounter, code, wordIndex);
        case 0x37u: return DecodeRdnaFlat(programCounter, code, wordIndex);
        case 0x38u: return DecodeRdnaMubuf(programCounter, code, wordIndex);
        case 0x3Au: return DecodeRdnaMtbuf(programCounter, code, wordIndex);
        case 0x3Du: return DecodeRdnaSmem(programCounter, code, wordIndex);
        default: throw std::runtime_error("instruction is not a memory operation");
    }
}

}
