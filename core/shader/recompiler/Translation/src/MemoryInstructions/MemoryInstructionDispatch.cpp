#include "Translation/MemoryInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

namespace {

const char* globalWaveSyncName(RdnaOpcode opcode) {
    switch (opcode) {
    case RdnaOpcode::DsGwsInit: return "ds_gws_init";
    case RdnaOpcode::DsGwsSemaV: return "ds_gws_sema_v";
    case RdnaOpcode::DsGwsSemaBr: return "ds_gws_sema_br";
    case RdnaOpcode::DsGwsSemaP: return "ds_gws_sema_p";
    case RdnaOpcode::DsGwsSemaReleaseAll: return "ds_gws_sema_release_all";
    case RdnaOpcode::DsGwsBarrier: return "ds_gws_barrier";
    default: throw std::logic_error("opcode is not a global wave sync instruction");
    }
}

}

void TranslateMemoryInstruction(IrBuilder& builder, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateMemoryInstruction not implemented");
}

bool TranslationContext::emitMemory(const RdnaInstruction& inst) {
    switch (inst.op) {
    case RdnaOpcode::SGl1Inv:
    case RdnaOpcode::SDcacheInv:
    case RdnaOpcode::SDcacheWb:
    case RdnaOpcode::SAtcProbe:
    case RdnaOpcode::SAtcProbeBuffer:
    case RdnaOpcode::SDcacheDiscard:
    case RdnaOpcode::SDcacheDiscardX2:
    case RdnaOpcode::BufferGl0Inv:
    case RdnaOpcode::BufferGl1Inv:
        emitControlNop();
        return true;
    case RdnaOpcode::SMemtime:
        writeU32Pair(inst.destination, extractU64(IrU64(ir.Emit(IrOpcode::ShaderClock, IrType::U64, {}))));
        return true;
    case RdnaOpcode::SMemrealtime:
        writeU32Pair(inst.destination, extractU64(IrU64(ir.Emit(IrOpcode::RealtimeClock, IrType::U64, {}))));
        return true;
    case RdnaOpcode::SLoadDword:
    case RdnaOpcode::SLoadDwordx2:
    case RdnaOpcode::SLoadDwordx4:
    case RdnaOpcode::SLoadDwordx8:
    case RdnaOpcode::SLoadDwordx16:
        return sLoad(inst, true);
    case RdnaOpcode::SBufferLoadDword:
    case RdnaOpcode::SBufferLoadDwordx2:
    case RdnaOpcode::SBufferLoadDwordx4:
    case RdnaOpcode::SBufferLoadDwordx8:
    case RdnaOpcode::SBufferLoadDwordx16:
        return sLoad(inst, false);
    case RdnaOpcode::SScratchLoadDword:
    case RdnaOpcode::SScratchLoadDwordx2:
    case RdnaOpcode::SScratchLoadDwordx4:
        return sScratchLoad(inst);
    case RdnaOpcode::SGetWaveidInWorkgroup: {
        if (program.Resources().stage != IrShaderStage::Compute) {
            throw std::runtime_error("s_get_waveid_in_workgroup is supported only in compute shaders, at pc " + std::to_string(inst.programCounter));
        }
        IrValue& localIndex = ir.Emit(IrOpcode::GetBuiltin, IrOpcodeType(IrOpcode::GetBuiltin), {&ir.Constant(static_cast<std::uint32_t>(StageInputKind::LocalInvocationIndex)), &ir.Constant(0u)});
        writeOperand(inst.destination, &ir.Emit(IrOpcode::UDiv32, IrOpcodeType(IrOpcode::UDiv32), {&localIndex, &ir.Constant(program.WaveSize())}));
        return true;
    }

    case RdnaOpcode::BufferLoadFormatX:
    case RdnaOpcode::BufferLoadFormatXy:
    case RdnaOpcode::BufferLoadFormatXyz:
    case RdnaOpcode::BufferLoadFormatXyzw:
    case RdnaOpcode::BufferLoadUbyte:
    case RdnaOpcode::BufferLoadSbyte:
    case RdnaOpcode::BufferLoadUshort:
    case RdnaOpcode::BufferLoadSshort:
    case RdnaOpcode::BufferLoadUbyteD16:
    case RdnaOpcode::BufferLoadUbyteD16Hi:
    case RdnaOpcode::BufferLoadSbyteD16:
    case RdnaOpcode::BufferLoadSbyteD16Hi:
    case RdnaOpcode::BufferLoadShortD16:
    case RdnaOpcode::BufferLoadShortD16Hi:
    case RdnaOpcode::BufferLoadDword:
    case RdnaOpcode::BufferLoadDwordx2:
    case RdnaOpcode::BufferLoadDwordx3:
    case RdnaOpcode::BufferLoadDwordx4:
    case RdnaOpcode::TbufferLoadFormatX:
    case RdnaOpcode::TbufferLoadFormatXy:
    case RdnaOpcode::TbufferLoadFormatXyz:
    case RdnaOpcode::TbufferLoadFormatXyzw:
        return bufferLoad(inst);
    case RdnaOpcode::BufferLoadFormatD16X:
    case RdnaOpcode::BufferLoadFormatD16Xy:
    case RdnaOpcode::BufferLoadFormatD16Xyz:
    case RdnaOpcode::BufferLoadFormatD16Xyzw:
    case RdnaOpcode::BufferLoadFormatD16HiX:
    case RdnaOpcode::TbufferLoadFormatD16X:
    case RdnaOpcode::TbufferLoadFormatD16Xy:
    case RdnaOpcode::TbufferLoadFormatD16Xyz:
    case RdnaOpcode::TbufferLoadFormatD16Xyzw:
        return bufferLoadFormatD16(inst);

    case RdnaOpcode::BufferStoreFormatX:
    case RdnaOpcode::BufferStoreFormatXy:
    case RdnaOpcode::BufferStoreFormatXyz:
    case RdnaOpcode::BufferStoreFormatXyzw:
    case RdnaOpcode::BufferStoreByte:
    case RdnaOpcode::BufferStoreShort:
    case RdnaOpcode::BufferStoreByteD16Hi:
    case RdnaOpcode::BufferStoreShortD16Hi:
    case RdnaOpcode::BufferStoreDword:
    case RdnaOpcode::BufferStoreDwordx2:
    case RdnaOpcode::BufferStoreDwordx3:
    case RdnaOpcode::BufferStoreDwordx4:
    case RdnaOpcode::TbufferStoreFormatX:
    case RdnaOpcode::TbufferStoreFormatXy:
    case RdnaOpcode::TbufferStoreFormatXyz:
    case RdnaOpcode::TbufferStoreFormatXyzw:
        return bufferStore(inst);
    case RdnaOpcode::TbufferStoreFormatD16X:
    case RdnaOpcode::TbufferStoreFormatD16Xy:
    case RdnaOpcode::TbufferStoreFormatD16Xyz:
    case RdnaOpcode::TbufferStoreFormatD16Xyzw:
    case RdnaOpcode::BufferStoreFormatD16X:
    case RdnaOpcode::BufferStoreFormatD16Xy:
    case RdnaOpcode::BufferStoreFormatD16Xyz:
    case RdnaOpcode::BufferStoreFormatD16Xyzw:
    case RdnaOpcode::BufferStoreFormatD16HiX:
        return bufferStoreFormatD16(inst);

    case RdnaOpcode::BufferAtomicSwap:
        return bufferAtomic(inst, IrOpcode::BufferAtomicSwap32);
    case RdnaOpcode::BufferAtomicCmpswap:
        return bufferAtomic(inst, IrOpcode::BufferAtomicCmpSwap32);
    case RdnaOpcode::BufferAtomicSwapX2:
        return bufferAtomic(inst, IrOpcode::BufferAtomicSwap64);
    case RdnaOpcode::BufferAtomicAdd:
        return bufferAtomic(inst, IrOpcode::BufferAtomicIAdd32);
    case RdnaOpcode::BufferAtomicSub:
        return bufferAtomic(inst, IrOpcode::BufferAtomicISub32);
    case RdnaOpcode::BufferAtomicCsub:
        if (!inst.glc) {
            throw std::runtime_error("buffer_atomic_csub without glc is not supported, at pc " + std::to_string(inst.programCounter));
        }
        return bufferAtomic(inst, IrOpcode::BufferAtomicUSubSat32);
    case RdnaOpcode::BufferAtomicSmin:
        return bufferAtomic(inst, IrOpcode::BufferAtomicSMin32);
    case RdnaOpcode::BufferAtomicUmin:
        return bufferAtomic(inst, IrOpcode::BufferAtomicUMin32);
    case RdnaOpcode::BufferAtomicSmax:
        return bufferAtomic(inst, IrOpcode::BufferAtomicSMax32);
    case RdnaOpcode::BufferAtomicUmax:
        return bufferAtomic(inst, IrOpcode::BufferAtomicUMax32);
    case RdnaOpcode::BufferAtomicAnd:
        return bufferAtomic(inst, IrOpcode::BufferAtomicAnd32);
    case RdnaOpcode::BufferAtomicOr:
        return bufferAtomic(inst, IrOpcode::BufferAtomicOr32);
    case RdnaOpcode::BufferAtomicOrX2:
        return bufferAtomic(inst, IrOpcode::BufferAtomicOr64);
    case RdnaOpcode::BufferAtomicInc:
        return bufferAtomic(inst, IrOpcode::BufferAtomicInc32);
    case RdnaOpcode::BufferAtomicDec:
        return bufferAtomic(inst, IrOpcode::BufferAtomicDec32);
    case RdnaOpcode::BufferAtomicCmpswapX2:
        return bufferAtomic(inst, IrOpcode::BufferAtomicCmpSwap64);
    case RdnaOpcode::BufferAtomicAddX2:
        return bufferAtomic(inst, IrOpcode::BufferAtomicIAdd64);
    case RdnaOpcode::BufferAtomicSubX2:
        return bufferAtomic(inst, IrOpcode::BufferAtomicISub64);
    case RdnaOpcode::BufferAtomicSminX2:
        return bufferAtomic(inst, IrOpcode::BufferAtomicSMin64);
    case RdnaOpcode::BufferAtomicUminX2:
        return bufferAtomic(inst, IrOpcode::BufferAtomicUMin64);
    case RdnaOpcode::BufferAtomicSmaxX2:
        return bufferAtomic(inst, IrOpcode::BufferAtomicSMax64);
    case RdnaOpcode::BufferAtomicUmaxX2:
        return bufferAtomic(inst, IrOpcode::BufferAtomicUMax64);
    case RdnaOpcode::BufferAtomicAndX2:
        return bufferAtomic(inst, IrOpcode::BufferAtomicAnd64);
    case RdnaOpcode::BufferAtomicXorX2:
        return bufferAtomic(inst, IrOpcode::BufferAtomicXor64);
    case RdnaOpcode::BufferAtomicFcmpswap:
        return bufferAtomic(inst, IrOpcode::BufferAtomicFCmpSwap32);
    case RdnaOpcode::BufferAtomicFcmpswapX2:
        return bufferAtomic(inst, IrOpcode::BufferAtomicFCmpSwap64);
    case RdnaOpcode::BufferAtomicFminX2:
        return bufferAtomic(inst, IrOpcode::BufferAtomicFMin64);
    case RdnaOpcode::BufferAtomicFmaxX2:
        return bufferAtomic(inst, IrOpcode::BufferAtomicFMax64);
    case RdnaOpcode::BufferAtomicIncX2:
        return bufferAtomic(inst, IrOpcode::BufferAtomicInc64);
    case RdnaOpcode::BufferAtomicDecX2:
        return bufferAtomic(inst, IrOpcode::BufferAtomicDec64);
    case RdnaOpcode::BufferAtomicXor:
        return bufferAtomic(inst, IrOpcode::BufferAtomicXor32);
    case RdnaOpcode::BufferAtomicFmin:
        return bufferAtomic(inst, IrOpcode::BufferAtomicFMin32);
    case RdnaOpcode::BufferAtomicFmax:
        return bufferAtomic(inst, IrOpcode::BufferAtomicFMax32);

    case RdnaOpcode::FlatLoadUbyte:
    case RdnaOpcode::FlatLoadSbyte:
    case RdnaOpcode::FlatLoadUshort:
    case RdnaOpcode::FlatLoadSshort:
    case RdnaOpcode::FlatLoadUbyteD16:
    case RdnaOpcode::FlatLoadUbyteD16Hi:
    case RdnaOpcode::FlatLoadSbyteD16:
    case RdnaOpcode::FlatLoadSbyteD16Hi:
    case RdnaOpcode::FlatLoadShortD16:
    case RdnaOpcode::FlatLoadShortD16Hi:
    case RdnaOpcode::FlatLoadDword:
    case RdnaOpcode::FlatLoadDwordx2:
    case RdnaOpcode::FlatLoadDwordx3:
    case RdnaOpcode::FlatLoadDwordx4:
        return flatLoad(inst);
    case RdnaOpcode::GlobalLoadDwordAddtid:
        return globalAddtid(inst, false);
    case RdnaOpcode::GlobalStoreDwordAddtid:
        return globalAddtid(inst, true);
    case RdnaOpcode::FlatStoreByte:
    case RdnaOpcode::FlatStoreShort:
    case RdnaOpcode::FlatStoreByteD16Hi:
    case RdnaOpcode::FlatStoreShortD16Hi:
    case RdnaOpcode::FlatStoreDword:
    case RdnaOpcode::FlatStoreDwordx2:
    case RdnaOpcode::FlatStoreDwordx3:
    case RdnaOpcode::FlatStoreDwordx4:
        return flatStore(inst);
    case RdnaOpcode::FlatAtomicSwap:
        return flatAtomic(inst, IrOpcode::AddressAtomicSwap32);
    case RdnaOpcode::FlatAtomicCmpswap:
        return flatAtomic(inst, IrOpcode::AddressAtomicCmpSwap32);
    case RdnaOpcode::FlatAtomicAdd:
        return flatAtomic(inst, IrOpcode::AddressAtomicIAdd32);
    case RdnaOpcode::FlatAtomicSub:
        return flatAtomic(inst, IrOpcode::AddressAtomicISub32);
    case RdnaOpcode::GlobalAtomicCsub:
        if (!inst.glc) {
            throw std::runtime_error("global_atomic_csub without glc is not supported, at pc " + std::to_string(inst.programCounter));
        }
        return flatAtomic(inst, IrOpcode::AddressAtomicUSubSat32);
    case RdnaOpcode::FlatAtomicSmin:
        return flatAtomic(inst, IrOpcode::AddressAtomicSMin32);
    case RdnaOpcode::FlatAtomicUmin:
        return flatAtomic(inst, IrOpcode::AddressAtomicUMin32);
    case RdnaOpcode::FlatAtomicSmax:
        return flatAtomic(inst, IrOpcode::AddressAtomicSMax32);
    case RdnaOpcode::FlatAtomicUmax:
        return flatAtomic(inst, IrOpcode::AddressAtomicUMax32);
    case RdnaOpcode::FlatAtomicAnd:
        return flatAtomic(inst, IrOpcode::AddressAtomicAnd32);
    case RdnaOpcode::FlatAtomicOr:
        return flatAtomic(inst, IrOpcode::AddressAtomicOr32);
    case RdnaOpcode::FlatAtomicXor:
        return flatAtomic(inst, IrOpcode::AddressAtomicXor32);
    case RdnaOpcode::FlatAtomicInc:
        return flatAtomic(inst, IrOpcode::AddressAtomicInc32);
    case RdnaOpcode::FlatAtomicDec:
        return flatAtomic(inst, IrOpcode::AddressAtomicDec32);
    case RdnaOpcode::FlatAtomicSwapX2:
        return flatAtomic(inst, IrOpcode::AddressAtomicSwap64);
    case RdnaOpcode::FlatAtomicCmpswapX2:
        return flatAtomic(inst, IrOpcode::AddressAtomicCmpSwap64);
    case RdnaOpcode::FlatAtomicAddX2:
        return flatAtomic(inst, IrOpcode::AddressAtomicIAdd64);
    case RdnaOpcode::FlatAtomicSubX2:
        return flatAtomic(inst, IrOpcode::AddressAtomicISub64);
    case RdnaOpcode::FlatAtomicSminX2:
        return flatAtomic(inst, IrOpcode::AddressAtomicSMin64);
    case RdnaOpcode::FlatAtomicUminX2:
        return flatAtomic(inst, IrOpcode::AddressAtomicUMin64);
    case RdnaOpcode::FlatAtomicSmaxX2:
        return flatAtomic(inst, IrOpcode::AddressAtomicSMax64);
    case RdnaOpcode::FlatAtomicUmaxX2:
        return flatAtomic(inst, IrOpcode::AddressAtomicUMax64);
    case RdnaOpcode::FlatAtomicAndX2:
        return flatAtomic(inst, IrOpcode::AddressAtomicAnd64);
    case RdnaOpcode::FlatAtomicOrX2:
        return flatAtomic(inst, IrOpcode::AddressAtomicOr64);
    case RdnaOpcode::FlatAtomicXorX2:
        return flatAtomic(inst, IrOpcode::AddressAtomicXor64);
    case RdnaOpcode::FlatAtomicFcmpswap:
        return flatAtomic(inst, IrOpcode::AddressAtomicFCmpSwap32);
    case RdnaOpcode::FlatAtomicFmin:
        return flatAtomic(inst, IrOpcode::AddressAtomicFMin32);
    case RdnaOpcode::FlatAtomicFmax:
        return flatAtomic(inst, IrOpcode::AddressAtomicFMax32);
    case RdnaOpcode::FlatAtomicFcmpswapX2:
        return flatAtomic(inst, IrOpcode::AddressAtomicFCmpSwap64);
    case RdnaOpcode::FlatAtomicFminX2:
        return flatAtomic(inst, IrOpcode::AddressAtomicFMin64);
    case RdnaOpcode::FlatAtomicFmaxX2:
        return flatAtomic(inst, IrOpcode::AddressAtomicFMax64);
    case RdnaOpcode::FlatAtomicIncX2:
        return flatAtomic(inst, IrOpcode::AddressAtomicInc64);
    case RdnaOpcode::FlatAtomicDecX2:
        return flatAtomic(inst, IrOpcode::AddressAtomicDec64);

    case RdnaOpcode::DsAddU32:
        return dsAtomic(inst, IrOpcode::SharedAtomicIAdd32, false);
    case RdnaOpcode::DsAddRtnU32:
        return dsAtomic(inst, IrOpcode::SharedAtomicIAdd32, true);
    case RdnaOpcode::DsSubU32:
        return dsAtomic(inst, IrOpcode::SharedAtomicISub32, false);
    case RdnaOpcode::DsSubRtnU32:
        return dsAtomic(inst, IrOpcode::SharedAtomicISub32, true);
    case RdnaOpcode::DsIncRtnU32:
        return dsAtomic(inst, IrOpcode::SharedAtomicInc32, true);
    case RdnaOpcode::DsDecRtnU32:
        return dsAtomic(inst, IrOpcode::SharedAtomicDec32, true);
    case RdnaOpcode::DsMinI32:
        return dsAtomic(inst, IrOpcode::SharedAtomicSMin32, false);
    case RdnaOpcode::DsMinRtnI32:
        return dsAtomic(inst, IrOpcode::SharedAtomicSMin32, true);
    case RdnaOpcode::DsMaxI32:
        return dsAtomic(inst, IrOpcode::SharedAtomicSMax32, false);
    case RdnaOpcode::DsMaxRtnI32:
        return dsAtomic(inst, IrOpcode::SharedAtomicSMax32, true);
    case RdnaOpcode::DsMinU32:
        return dsAtomic(inst, IrOpcode::SharedAtomicUMin32, false);
    case RdnaOpcode::DsMinRtnU32:
        return dsAtomic(inst, IrOpcode::SharedAtomicUMin32, true);
    case RdnaOpcode::DsMaxU32:
        return dsAtomic(inst, IrOpcode::SharedAtomicUMax32, false);
    case RdnaOpcode::DsMaxRtnU32:
        return dsAtomic(inst, IrOpcode::SharedAtomicUMax32, true);
    case RdnaOpcode::DsAndB32:
        return dsAtomic(inst, IrOpcode::SharedAtomicAnd32, false);
    case RdnaOpcode::DsAndRtnB32:
        return dsAtomic(inst, IrOpcode::SharedAtomicAnd32, true);
    case RdnaOpcode::DsOrB32:
        return dsAtomic(inst, IrOpcode::SharedAtomicOr32, false);
    case RdnaOpcode::DsOrRtnB32:
        return dsAtomic(inst, IrOpcode::SharedAtomicOr32, true);
    case RdnaOpcode::DsXorB32:
        return dsAtomic(inst, IrOpcode::SharedAtomicXor32, false);
    case RdnaOpcode::DsXorRtnB32:
        return dsAtomic(inst, IrOpcode::SharedAtomicXor32, true);
    case RdnaOpcode::DsWrxchgRtnB32:
        return dsAtomic(inst, IrOpcode::SharedAtomicSwap32, true);

    case RdnaOpcode::DsMinF32:
        return dsAtomic(inst, IrOpcode::SharedAtomicFMin32, false);
    case RdnaOpcode::DsMinRtnF32:
        return dsAtomic(inst, IrOpcode::SharedAtomicFMin32, true);
    case RdnaOpcode::DsMaxF32:
        return dsAtomic(inst, IrOpcode::SharedAtomicFMax32, false);
    case RdnaOpcode::DsMaxRtnF32:
        return dsAtomic(inst, IrOpcode::SharedAtomicFMax32, true);
    case RdnaOpcode::DsRsubU32:
        return dsAtomic(inst, IrOpcode::SharedAtomicRsub32, false);
    case RdnaOpcode::DsRsubRtnU32:
        return dsAtomic(inst, IrOpcode::SharedAtomicRsub32, true);
    case RdnaOpcode::DsIncU32:
        return dsAtomic(inst, IrOpcode::SharedAtomicInc32, false);
    case RdnaOpcode::DsDecU32:
        return dsAtomic(inst, IrOpcode::SharedAtomicDec32, false);
    case RdnaOpcode::DsAddF32:
        return dsAtomic(inst, IrOpcode::SharedAtomicFAdd32, false);
    case RdnaOpcode::DsAddRtnF32:
        return dsAtomic(inst, IrOpcode::SharedAtomicFAdd32, true);
    case RdnaOpcode::DsMskorB32:
        return dsAtomic2(inst, IrOpcode::SharedAtomicMskor32, false);
    case RdnaOpcode::DsMskorRtnB32:
        return dsAtomic2(inst, IrOpcode::SharedAtomicMskor32, true);
    case RdnaOpcode::DsCmpstB32:
        return dsAtomic2(inst, IrOpcode::SharedAtomicCmpst32, false);
    case RdnaOpcode::DsCmpstRtnB32:
        return dsAtomic2(inst, IrOpcode::SharedAtomicCmpst32, true);
    case RdnaOpcode::DsCmpstF32:
        return dsAtomic2(inst, IrOpcode::SharedAtomicCmpstF32, false);
    case RdnaOpcode::DsCmpstRtnF32:
        return dsAtomic2(inst, IrOpcode::SharedAtomicCmpstF32, true);
    case RdnaOpcode::DsWrapRtnB32:
        return dsAtomic2(inst, IrOpcode::SharedAtomicWrap32, true);
    case RdnaOpcode::DsAddU64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicIAdd64, false);
    case RdnaOpcode::DsAddRtnU64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicIAdd64, true);
    case RdnaOpcode::DsSubU64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicISub64, false);
    case RdnaOpcode::DsSubRtnU64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicISub64, true);
    case RdnaOpcode::DsRsubU64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicRsub64, false);
    case RdnaOpcode::DsRsubRtnU64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicRsub64, true);
    case RdnaOpcode::DsIncU64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicInc64, false);
    case RdnaOpcode::DsIncRtnU64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicInc64, true);
    case RdnaOpcode::DsDecU64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicDec64, false);
    case RdnaOpcode::DsDecRtnU64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicDec64, true);
    case RdnaOpcode::DsMinI64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicSMin64, false);
    case RdnaOpcode::DsMinRtnI64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicSMin64, true);
    case RdnaOpcode::DsMaxI64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicSMax64, false);
    case RdnaOpcode::DsMaxRtnI64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicSMax64, true);
    case RdnaOpcode::DsMinU64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicUMin64, false);
    case RdnaOpcode::DsMinRtnU64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicUMin64, true);
    case RdnaOpcode::DsMaxU64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicUMax64, false);
    case RdnaOpcode::DsMaxRtnU64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicUMax64, true);
    case RdnaOpcode::DsAndB64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicAnd64, false);
    case RdnaOpcode::DsAndRtnB64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicAnd64, true);
    case RdnaOpcode::DsOrB64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicOr64, false);
    case RdnaOpcode::DsOrRtnB64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicOr64, true);
    case RdnaOpcode::DsXorB64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicXor64, false);
    case RdnaOpcode::DsXorRtnB64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicXor64, true);
    case RdnaOpcode::DsMskorB64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicMskor64, false);
    case RdnaOpcode::DsMskorRtnB64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicMskor64, true);
    case RdnaOpcode::DsCmpstB64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicCmpst64, false);
    case RdnaOpcode::DsCmpstRtnB64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicCmpst64, true);
    case RdnaOpcode::DsCmpstF64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicCmpstF64, false);
    case RdnaOpcode::DsCmpstRtnF64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicCmpstF64, true);
    case RdnaOpcode::DsMinF64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicFMin64, false);
    case RdnaOpcode::DsMinRtnF64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicFMin64, true);
    case RdnaOpcode::DsMaxF64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicFMax64, false);
    case RdnaOpcode::DsMaxRtnF64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicFMax64, true);
    case RdnaOpcode::DsWrxchgRtnB64:
        return dsAtomic64(inst, IrOpcode::SharedAtomicSwap64, true);
    case RdnaOpcode::DsCondxchg32RtnB64:
        return dsCondxchg32(inst);
    case RdnaOpcode::DsNop:
        emitControlNop();
        return true;
    case RdnaOpcode::DsSwizzleB32:
        return dsSwizzleB32(inst);
    case RdnaOpcode::DsBpermuteB32:
        return dsBpermuteB32(inst);
    case RdnaOpcode::DsPermuteB32:
        return dsPermuteB32(inst);
    case RdnaOpcode::DsConsume:
        return dsAppendConsume(inst, IrOpcode::DataConsume);
    case RdnaOpcode::DsAppend:
        return dsAppendConsume(inst, IrOpcode::DataAppend);
    case RdnaOpcode::DsGwsInit:
    case RdnaOpcode::DsGwsSemaV:
    case RdnaOpcode::DsGwsSemaBr:
    case RdnaOpcode::DsGwsSemaP:
    case RdnaOpcode::DsGwsSemaReleaseAll:
    case RdnaOpcode::DsGwsBarrier:
        throw std::runtime_error(std::string(globalWaveSyncName(inst.op)) + " at pc " + std::to_string(inst.programCounter) + ": global wave sync barriers and semaphores shared by waves of different workgroups are not modeled");
    case RdnaOpcode::DsOrderedCount:
        throw std::runtime_error("ds_ordered_count at pc " + std::to_string(inst.programCounter) + ": GDS counters updated in wave launch order are not modeled");
    case RdnaOpcode::DsWriteAddtidB32:
        return dsAddtid(inst, true);
    case RdnaOpcode::DsReadAddtidB32:
        return dsAddtid(inst, false);

    case RdnaOpcode::DsRead2B32:
    case RdnaOpcode::DsRead2st64B32:
    case RdnaOpcode::DsRead2B64:
    case RdnaOpcode::DsRead2st64B64:
        return dsRead2(inst);
    case RdnaOpcode::DsReadI8:
    case RdnaOpcode::DsReadU8:
    case RdnaOpcode::DsReadI16:
    case RdnaOpcode::DsReadU16:
    case RdnaOpcode::DsReadU16D16:
    case RdnaOpcode::DsReadU16D16Hi:
    case RdnaOpcode::DsReadU8D16:
    case RdnaOpcode::DsReadU8D16Hi:
    case RdnaOpcode::DsReadI8D16:
    case RdnaOpcode::DsReadI8D16Hi:
    case RdnaOpcode::DsReadB32:
    case RdnaOpcode::DsReadB64:
    case RdnaOpcode::DsReadB96:
    case RdnaOpcode::DsReadB128:
        return dsRead(inst);
    case RdnaOpcode::DsWrite2B32:
    case RdnaOpcode::DsWrite2st64B32:
    case RdnaOpcode::DsWrite2B64:
    case RdnaOpcode::DsWrite2st64B64:
        return dsWrite2(inst);
    case RdnaOpcode::DsWrxchg2RtnB32:
    case RdnaOpcode::DsWrxchg2st64RtnB32:
    case RdnaOpcode::DsWrxchg2RtnB64:
    case RdnaOpcode::DsWrxchg2st64RtnB64:
        return dsWrxchg2(inst);
    case RdnaOpcode::DsWriteB8:
    case RdnaOpcode::DsWriteB16:
    case RdnaOpcode::DsWriteB16D16Hi:
    case RdnaOpcode::DsWriteB8D16Hi:
    case RdnaOpcode::DsWriteB32:
    case RdnaOpcode::DsWriteB64:
    case RdnaOpcode::DsWriteB96:
    case RdnaOpcode::DsWriteB128:
        return dsWrite(inst);

    case RdnaOpcode::ImageSample:
    case RdnaOpcode::ImageSampleLz:
    case RdnaOpcode::ImageSampleBCl:
    case RdnaOpcode::ImageSampleBClO:
    case RdnaOpcode::ImageSampleBO:
    case RdnaOpcode::ImageSampleC:
    case RdnaOpcode::ImageSampleCB:
    case RdnaOpcode::ImageSampleCBCl:
    case RdnaOpcode::ImageSampleCBClO:
    case RdnaOpcode::ImageSampleCBO:
    case RdnaOpcode::ImageSampleCCl:
    case RdnaOpcode::ImageSampleCClO:
    case RdnaOpcode::ImageSampleCD:
    case RdnaOpcode::ImageSampleCDCl:
    case RdnaOpcode::ImageSampleCDClO:
    case RdnaOpcode::ImageSampleCDO:
    case RdnaOpcode::ImageSampleCL:
    case RdnaOpcode::ImageSampleCLzO:
    case RdnaOpcode::ImageSampleCLO:
    case RdnaOpcode::ImageSampleCO:
    case RdnaOpcode::ImageSampleCl:
    case RdnaOpcode::ImageSampleClO:
    case RdnaOpcode::ImageSampleD:
    case RdnaOpcode::ImageSampleDCl:
    case RdnaOpcode::ImageSampleDO:
    case RdnaOpcode::ImageSampleLzO:
    case RdnaOpcode::ImageSampleO:
    case RdnaOpcode::ImageSampleCd:
    case RdnaOpcode::ImageSampleCdCl:
    case RdnaOpcode::ImageSampleCCd:
    case RdnaOpcode::ImageSampleCCdCl:
    case RdnaOpcode::ImageSampleCdO:
    case RdnaOpcode::ImageSampleCdClO:
    case RdnaOpcode::ImageSampleCCdO:
    case RdnaOpcode::ImageSampleCCdClO:
    case RdnaOpcode::ImageSampleDG16:
    case RdnaOpcode::ImageSampleDClG16:
    case RdnaOpcode::ImageSampleCDG16:
    case RdnaOpcode::ImageSampleCDClG16:
    case RdnaOpcode::ImageSampleDOG16:
    case RdnaOpcode::ImageSampleDClOG16:
    case RdnaOpcode::ImageSampleCDOG16:
    case RdnaOpcode::ImageSampleCDClOG16:
    case RdnaOpcode::ImageSampleCdG16:
    case RdnaOpcode::ImageSampleCdClG16:
    case RdnaOpcode::ImageSampleCCdG16:
    case RdnaOpcode::ImageSampleCCdClG16:
    case RdnaOpcode::ImageSampleCdOG16:
    case RdnaOpcode::ImageSampleCdClOG16:
    case RdnaOpcode::ImageSampleCCdOG16:
    case RdnaOpcode::ImageSampleCCdClOG16:
        return imageSample(inst);
    case RdnaOpcode::ImageGather4Lz:
    case RdnaOpcode::ImageGather4C:
    case RdnaOpcode::ImageGather4CLz:
    case RdnaOpcode::ImageGather4LzO:
    case RdnaOpcode::ImageGather4CO:
    case RdnaOpcode::ImageGather4CLzO:
    case RdnaOpcode::ImageGather4h:
    case RdnaOpcode::ImageGather4:
    case RdnaOpcode::ImageGather4B:
    case RdnaOpcode::ImageGather4BCl:
    case RdnaOpcode::ImageGather4BClO:
    case RdnaOpcode::ImageGather4BO:
    case RdnaOpcode::ImageGather4Cl:
    case RdnaOpcode::ImageGather4ClO:
    case RdnaOpcode::ImageGather4CB:
    case RdnaOpcode::ImageGather4CBCl:
    case RdnaOpcode::ImageGather4CBClO:
    case RdnaOpcode::ImageGather4CBO:
    case RdnaOpcode::ImageGather4CCl:
    case RdnaOpcode::ImageGather4CClO:
    case RdnaOpcode::ImageGather4CL:
    case RdnaOpcode::ImageGather4CLO:
    case RdnaOpcode::ImageGather4L:
    case RdnaOpcode::ImageGather4LO:
    case RdnaOpcode::ImageGather4O:
        return imageGather(inst);
    case RdnaOpcode::ImageAtomicSwap:
        return imageAtomic(inst, IrOpcode::ImageAtomicSwap32);
    case RdnaOpcode::ImageAtomicAdd:
        return imageAtomic(inst, IrOpcode::ImageAtomicIAdd32);
    case RdnaOpcode::ImageAtomicUmin:
        return imageAtomic(inst, IrOpcode::ImageAtomicUMin32);
    case RdnaOpcode::ImageAtomicUmax:
        return imageAtomic(inst, IrOpcode::ImageAtomicUMax32);
    case RdnaOpcode::ImageAtomicAnd:
        return imageAtomic(inst, IrOpcode::ImageAtomicAnd32);
    case RdnaOpcode::ImageAtomicOr:
        return imageAtomic(inst, IrOpcode::ImageAtomicOr32);
    case RdnaOpcode::ImageAtomicXor:
        return imageAtomic(inst, IrOpcode::ImageAtomicXor32);
    case RdnaOpcode::ImageAtomicCmpswap:
        return imageAtomic(inst, IrOpcode::ImageAtomicCmpSwap32);
    case RdnaOpcode::ImageAtomicSub:
        return imageAtomic(inst, IrOpcode::ImageAtomicISub32);
    case RdnaOpcode::ImageAtomicSmin:
        return imageAtomic(inst, IrOpcode::ImageAtomicSMin32);
    case RdnaOpcode::ImageAtomicSmax:
        return imageAtomic(inst, IrOpcode::ImageAtomicSMax32);
    case RdnaOpcode::ImageAtomicInc:
        return imageAtomic(inst, IrOpcode::ImageAtomicInc32);
    case RdnaOpcode::ImageAtomicDec:
        return imageAtomic(inst, IrOpcode::ImageAtomicDec32);
    case RdnaOpcode::ImageAtomicFcmpswap:
        return imageAtomic(inst, IrOpcode::ImageAtomicFCmpSwap32);
    case RdnaOpcode::ImageAtomicFmin:
        return imageAtomic(inst, IrOpcode::ImageAtomicFMin32);
    case RdnaOpcode::ImageAtomicFmax:
        return imageAtomic(inst, IrOpcode::ImageAtomicFMax32);
    case RdnaOpcode::ImageLoad:
    case RdnaOpcode::ImageLoadMip:
    case RdnaOpcode::ImageLoadPck:
    case RdnaOpcode::ImageLoadPckSgn:
    case RdnaOpcode::ImageLoadMipPck:
    case RdnaOpcode::ImageLoadMipPckSgn:
        return imageLoad(inst);
    case RdnaOpcode::ImageMsaaLoad:
        return imageMsaaLoad(inst);
    case RdnaOpcode::ImageStore:
    case RdnaOpcode::ImageStoreMip:
    case RdnaOpcode::ImageStorePck:
    case RdnaOpcode::ImageStoreMipPck:
        return imageStore(inst);
    case RdnaOpcode::ImageGetResinfo:
        return imageGetResinfo(inst);
    case RdnaOpcode::ImageGetLod:
        return imageGetLod(inst);
    case RdnaOpcode::ImageBvhIntersectRay:
    case RdnaOpcode::ImageBvh64IntersectRay:
        return imageBvhIntersectRay(inst);

    default:
        return false;
    }
}

void TranslateMemoryInstruction(TranslationContext& context, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateMemoryInstruction not implemented");
}

}
