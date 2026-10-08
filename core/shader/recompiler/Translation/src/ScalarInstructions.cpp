#include "Translation/ScalarInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

void TranslateScalarInstruction(IrBuilder& builder, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateScalarInstruction not implemented");
}

bool TranslationContext::emitScalar(const RdnaInstruction& inst) {
    switch (inst.op) {
    case RdnaOpcode::SMovB32:
    case RdnaOpcode::SMovkI32:
        movB32(inst, false);
        return true;
    case RdnaOpcode::SMovB64:
        sMovB64(inst);
        return true;
    case RdnaOpcode::SWqmB32:
        sWqm(inst, false);
        return true;
    case RdnaOpcode::SWqmB64:
        sWqm(inst, true);
        return true;
    case RdnaOpcode::SGetpcB64:
        sGetpcB64(inst);
        return true;
    case RdnaOpcode::SSetpcB64:
        return true;
    case RdnaOpcode::SSwappcB64:
    case RdnaOpcode::SCallB64:
        sSwappcB64(inst);
        return true;
    case RdnaOpcode::SSubvectorLoopBegin:
        sSubvectorLoop(inst, true);
        return true;
    case RdnaOpcode::SSubvectorLoopEnd:
        sSubvectorLoop(inst, false);
        return true;
    case RdnaOpcode::SCselectB32:
        sCselectB32(inst);
        return true;
    case RdnaOpcode::SCselectB64:
        scalarSelectMask64(inst);
        return true;
    case RdnaOpcode::SCmovB32: {
        const IrU32 source = readU32(sourceAt(inst, 0u));
        const IrU32 previous = readU32(inst.destination);
        const IrU32 result(ir.Select(ir.GetScc(), source.Value(), previous.Value()));
        writeRawU32(inst.destination, result);
        return true;
    }
    case RdnaOpcode::SCmovB64:
        scalarSelect64(inst, inst.destination);
        return true;
    case RdnaOpcode::SSetregB32:
        if ((inst.source1.value & 0x3fu) == 1u) {
            throw std::runtime_error("s_setreg_b32 at pc " + std::to_string(inst.programCounter) + " writes MODE from an SGPR: runtime MODE changes are not implemented");
        }
        emitControlNop();
        return true;
    case RdnaOpcode::SVersion:
        emitControlNop();
        return true;
    case RdnaOpcode::SSetregImm32B32: {
        const std::uint32_t field = inst.source1.value;
        const std::uint32_t offset = (field >> 6u) & 0x1fu;
        const std::uint32_t size = ((field >> 11u) & 0x1fu) + 1u;
        const std::uint64_t written = static_cast<std::uint64_t>(inst.source0.value) & ((std::uint64_t{1} << size) - 1u);
        if ((field & 0x3fu) != 1u || offset + size > 4u) {
            throw std::runtime_error("s_setreg_imm32_b32 at pc " + std::to_string(inst.programCounter) + " writes a hardware register field other than the MODE rounding fields");
        }
        const std::uint32_t initial = floatMode.has_value() ? floatMode->floatMode : 0u;
        if (written != ((initial >> offset) & ((1u << size) - 1u))) {
            throw std::runtime_error("s_setreg_imm32_b32 at pc " + std::to_string(inst.programCounter) + " changes the initial MODE rounding fields: runtime MODE changes are not implemented");
        }
        emitControlNop();
        return true;
    }
    case RdnaOpcode::SGetregB32: {
        const std::uint32_t field = inst.source0.value;
        const std::uint32_t offset = (field >> 6u) & 0x1fu;
        const std::uint32_t size = ((field >> 11u) & 0x1fu) + 1u;
        if ((field & 0x3fu) != 1u || offset + size > 4u) {
            throw std::runtime_error("s_getreg_b32 at pc " + std::to_string(inst.programCounter) + " reads hardware register " + std::to_string(field & 0x3fu) + " bits " + std::to_string(offset) + ".." + std::to_string(offset + size - 1u) + ": only the MODE round mode fields are modeled");
        }
        const std::uint32_t initial = floatMode.has_value() ? floatMode->floatMode : 0u;
        writeRawU32(inst.destination, IrU32(ir.Constant((initial >> offset) & ((1u << size) - 1u))));
        return true;
    }
    case RdnaOpcode::SCmovkI32: {
        const IrU32 previous = readU32(inst.destination);
        writeRawU32(inst.destination, IrU32(ir.Select(ir.GetScc(), ir.Constant(inst.source0.value), previous.Value())));
        return true;
    }
    case RdnaOpcode::SWaitcnt:
        emitWaitcnt();
        return true;
    case RdnaOpcode::SAndSaveexecB32:
        sSaveexec(inst, IrOpcode::LogicalAnd, false, false, false);
        return true;
    case RdnaOpcode::SOrSaveexecB32:
        sSaveexec(inst, IrOpcode::LogicalOr, false, false, false);
        return true;
    case RdnaOpcode::SXorSaveexecB32:
        sSaveexec(inst, IrOpcode::LogicalXor, false, false, false);
        return true;
    case RdnaOpcode::SAndn1SaveexecB32:
        sSaveexec(inst, IrOpcode::LogicalAnd, false, true, false);
        return true;
    case RdnaOpcode::SAndn2SaveexecB32:
        sSaveexec(inst, IrOpcode::LogicalAnd, true, false, false);
        return true;
    case RdnaOpcode::SOrn1SaveexecB32:
        sSaveexec(inst, IrOpcode::LogicalOr, false, true, false);
        return true;
    case RdnaOpcode::SOrn2SaveexecB32:
        sSaveexec(inst, IrOpcode::LogicalOr, true, false, false);
        return true;
    case RdnaOpcode::SNandSaveexecB32:
        sSaveexec(inst, IrOpcode::LogicalAnd, false, false, false, true);
        return true;
    case RdnaOpcode::SNorSaveexecB32:
        sSaveexec(inst, IrOpcode::LogicalOr, false, false, false, true);
        return true;
    case RdnaOpcode::SXnorSaveexecB32:
        sSaveexec(inst, IrOpcode::LogicalXor, false, false, false, true);
        return true;
    case RdnaOpcode::SAndn1WrexecB32:
        sSaveexec(inst, IrOpcode::LogicalAnd, false, true, false, false, true);
        return true;
    case RdnaOpcode::SAndn2WrexecB32:
        sSaveexec(inst, IrOpcode::LogicalAnd, true, false, false, false, true);
        return true;
    case RdnaOpcode::SAndSaveexecB64:
        sSaveexec(inst, IrOpcode::LogicalAnd, false, false, true);
        return true;
    case RdnaOpcode::SOrSaveexecB64:
        sSaveexec(inst, IrOpcode::LogicalOr, false, false, true);
        return true;
    case RdnaOpcode::SXorSaveexecB64:
        sSaveexec(inst, IrOpcode::LogicalXor, false, false, true);
        return true;
    case RdnaOpcode::SAndn2SaveexecB64:
        sSaveexec(inst, IrOpcode::LogicalAnd, true, false, true);
        return true;
    case RdnaOpcode::SAndn1SaveexecB64:
        sSaveexec(inst, IrOpcode::LogicalAnd, false, true, true);
        return true;
    case RdnaOpcode::SOrn2SaveexecB64:
        sSaveexec(inst, IrOpcode::LogicalOr, true, false, true);
        return true;
    case RdnaOpcode::SNandSaveexecB64:
        sSaveexec(inst, IrOpcode::LogicalAnd, false, false, true, true);
        return true;
    case RdnaOpcode::SNorSaveexecB64:
        sSaveexec(inst, IrOpcode::LogicalOr, false, false, true, true);
        return true;
    case RdnaOpcode::SXnorSaveexecB64:
        sSaveexec(inst, IrOpcode::LogicalXor, false, false, true, true);
        return true;
    case RdnaOpcode::SOrn1SaveexecB64:
        sSaveexec(inst, IrOpcode::LogicalOr, false, true, true);
        return true;
    case RdnaOpcode::SAndn1WrexecB64:
        sSaveexec(inst, IrOpcode::LogicalAnd, false, true, true, false, true);
        return true;
    case RdnaOpcode::SAndn2WrexecB64:
        sSaveexec(inst, IrOpcode::LogicalAnd, true, false, true, false, true);
        return true;
    case RdnaOpcode::SAddU32:
        addU32(inst, false, false);
        return true;
    case RdnaOpcode::SAddcU32:
        addU32(inst, false, true);
        return true;
    case RdnaOpcode::SSubU32:
        subU32(inst, false, false);
        return true;
    case RdnaOpcode::SSubbU32:
        subbU32(inst, false, false);
        return true;
    case RdnaOpcode::SAbsdiffI32:
        sAbsdiffI32(inst);
        return true;
    case RdnaOpcode::SAddI32:
        sAddSubI32(inst, false);
        return true;
    case RdnaOpcode::SSubI32:
        sAddSubI32(inst, true);
        return true;
    case RdnaOpcode::SLshl1AddU32:
        sLshlAddU32(inst, 1u);
        return true;
    case RdnaOpcode::SLshl2AddU32:
        sLshlAddU32(inst, 2u);
        return true;
    case RdnaOpcode::SLshl3AddU32:
        sLshlAddU32(inst, 3u);
        return true;
    case RdnaOpcode::SLshl4AddU32:
        sLshlAddU32(inst, 4u);
        return true;
    case RdnaOpcode::SMinI32:
        scalarMinMax32(inst, IrOpcode::SMin32, IrOpcode::SLessThan32);
        return true;
    case RdnaOpcode::SMaxI32:
        scalarMinMax32(inst, IrOpcode::SMax32, IrOpcode::SGreaterThan32);
        return true;
    case RdnaOpcode::SMinU32:
        scalarMinMax32(inst, IrOpcode::UMin32, IrOpcode::ULessThan32);
        return true;
    case RdnaOpcode::SMaxU32:
        scalarMinMax32(inst, IrOpcode::UMax32, IrOpcode::UGreaterThan32);
        return true;
    case RdnaOpcode::SCmpEqU32:
    case RdnaOpcode::SCmpEqI32:
        emitIntegerCompare(inst, IrOpcode::IEqual32, IrType::U32, true, false);
        return true;
    case RdnaOpcode::SCmpLgU32:
    case RdnaOpcode::SCmpLgI32:
        emitIntegerCompare(inst, IrOpcode::INotEqual32, IrType::U32, true, false);
        return true;
    case RdnaOpcode::SCmpGtU32:
        emitIntegerCompare(inst, IrOpcode::UGreaterThan32, IrType::U32, true, false);
        return true;
    case RdnaOpcode::SCmpGeU32:
        emitIntegerCompare(inst, IrOpcode::UGreaterThanEqual32, IrType::U32, true, false);
        return true;
    case RdnaOpcode::SCmpLtU32:
        emitIntegerCompare(inst, IrOpcode::ULessThan32, IrType::U32, true, false);
        return true;
    case RdnaOpcode::SCmpLeU32:
        emitIntegerCompare(inst, IrOpcode::ULessThanEqual32, IrType::U32, true, false);
        return true;
    case RdnaOpcode::SCmpGtI32:
        emitIntegerCompare(inst, IrOpcode::SGreaterThan32, IrType::U32, true, false);
        return true;
    case RdnaOpcode::SCmpGeI32:
        emitIntegerCompare(inst, IrOpcode::SGreaterThanEqual32, IrType::U32, true, false);
        return true;
    case RdnaOpcode::SCmpLtI32:
        emitIntegerCompare(inst, IrOpcode::SLessThan32, IrType::U32, true, false);
        return true;
    case RdnaOpcode::SCmpLeI32:
        emitIntegerCompare(inst, IrOpcode::SLessThanEqual32, IrType::U32, true, false);
        return true;
    case RdnaOpcode::SCmpEqU64:
        emitIntegerCompare(inst, IrOpcode::IEqual64, IrType::U64, true, false);
        return true;
    case RdnaOpcode::SCmpLgU64:
        emitIntegerCompare(inst, IrOpcode::INotEqual64, IrType::U64, true, false);
        return true;
    case RdnaOpcode::SAndB64:
        return sU64Mask(inst, IrOpcode::LogicalAnd, IrOpcode::BitwiseAnd32, false, false, false);
    case RdnaOpcode::SAndn2B64:
        return sU64Mask(inst, IrOpcode::LogicalAnd, IrOpcode::BitwiseAnd32, true, false, false);
    case RdnaOpcode::SOrB64:
        return sU64Mask(inst, IrOpcode::LogicalOr, IrOpcode::BitwiseOr32, false, false, false);
    case RdnaOpcode::SOrn2B64:
        return sU64Mask(inst, IrOpcode::LogicalOr, IrOpcode::BitwiseOr32, true, false, false);
    case RdnaOpcode::SXorB64:
        return sU64Mask(inst, IrOpcode::LogicalXor, IrOpcode::BitwiseXor32, false, false, false);
    case RdnaOpcode::SNandB64:
        return sU64Mask(inst, IrOpcode::LogicalAnd, IrOpcode::BitwiseAnd32, false, true, false);
    case RdnaOpcode::SNorB64:
        return sU64Mask(inst, IrOpcode::LogicalOr, IrOpcode::BitwiseOr32, false, true, false);
    case RdnaOpcode::SXnorB64:
        return sU64Mask(inst, IrOpcode::LogicalXor, IrOpcode::BitwiseXor32, false, true, false);
    case RdnaOpcode::SNotB64:
        return sU64Mask(inst, IrOpcode::LogicalAnd, IrOpcode::BitwiseAnd32, false, false, true);
    case RdnaOpcode::SAbsI32:
        return simpleInteger(inst, IrOpcode::IAbs32, IrType::U32, false, false, true);
    case RdnaOpcode::SMulI32:
    case RdnaOpcode::SMulkI32:
        return simpleInteger(inst, IrOpcode::IMul32, IrType::U32, false, false, false);
    case RdnaOpcode::SMulHiU32:
        return simpleInteger(inst, IrOpcode::UMulHi, IrType::U32, false, false, false);
    case RdnaOpcode::SMulHiI32:
        return simpleInteger(inst, IrOpcode::SMulHi, IrType::U32, false, false, false);
    case RdnaOpcode::SAndB32:
        return simpleInteger(inst, IrOpcode::BitwiseAnd32, IrType::U32, false, false, true);
    case RdnaOpcode::SOrB32:
        return simpleInteger(inst, IrOpcode::BitwiseOr32, IrType::U32, false, false, true);
    case RdnaOpcode::SXorB32:
        return simpleInteger(inst, IrOpcode::BitwiseXor32, IrType::U32, false, false, true);
    case RdnaOpcode::SNotB32:
        return simpleInteger(inst, IrOpcode::BitwiseNot32, IrType::U32, false, false, true);
    case RdnaOpcode::SBrevB32:
        return simpleInteger(inst, IrOpcode::BitReverse32, IrType::U32, false, false, false);
    case RdnaOpcode::SBrevB64: {
        const auto source = readU32Pair(sourceAt(inst, 0u));
        const IrU32 low(ir.Emit(IrOpcode::BitReverse32, IrType::U32, {&source[1].Value()}));
        const IrU32 high(ir.Emit(IrOpcode::BitReverse32, IrType::U32, {&source[0].Value()}));
        writeU32Pair(inst.destination, {low, high});
        return true;
    }
    case RdnaOpcode::SSextI32I8:
    case RdnaOpcode::SSextI32I16: {
        const auto source = readU32(sourceAt(inst, 0u));
        const auto width = inst.op == RdnaOpcode::SSextI32I8 ? 8u : 16u;
        auto& result = ir.Emit(IrOpcode::BitFieldSExtract, IrType::U32, {&source.Value(), &ir.Constant(0u), &ir.Constant(width)});
        writeOperand(inst.destination, &result);
        return true;
    }
    case RdnaOpcode::SBcnt0I32B32:
    case RdnaOpcode::SFf0I32B32: {
        const IrU32 source = readU32(sourceAt(inst, 0u));
        auto& inverted = ir.BitwiseNot(source.Value());
        const bool count = inst.op == RdnaOpcode::SBcnt0I32B32;
        auto& result = ir.Emit(count ? IrOpcode::BitCount32 : IrOpcode::FindILsb32, IrType::U32, {&inverted});
        writeOperand(inst.destination, &result);
        if (count) {
            ir.SetScc(ir.INotEqual(result, ir.Constant(0u)));
        }
        return true;
    }
    case RdnaOpcode::SBcnt0I32B64: {
        const std::array<IrU32, 2> source = extractU64(readU64(sourceAt(inst, 0u)));
        auto& low = ir.Emit(IrOpcode::BitCount32, IrType::U32, {&ir.BitwiseNot(source[0].Value())});
        auto& high = ir.Emit(IrOpcode::BitCount32, IrType::U32, {&ir.BitwiseNot(source[1].Value())});
        auto& result = ir.IAdd(low, high);
        writeOperand(inst.destination, &result);
        ir.SetScc(ir.INotEqual(result, ir.Constant(0u)));
        return true;
    }
    case RdnaOpcode::SBcnt1I32B32:
        return simpleInteger(inst, IrOpcode::BitCount32, IrType::U32, false, false, true);
    case RdnaOpcode::SBcnt1I32B64:
        return simpleInteger(inst, IrOpcode::BitCount64, IrType::U64, false, false, true);
    case RdnaOpcode::SFf1I32B32:
        return simpleInteger(inst, IrOpcode::FindILsb32, IrType::U32, false, false, false);
    case RdnaOpcode::SLshlB32:
        return simpleInteger(inst, IrOpcode::ShiftLeftLogical32, IrType::U32, false, true, true);
    case RdnaOpcode::SLshrB32:
        return simpleInteger(inst, IrOpcode::ShiftRightLogical32, IrType::U32, false, true, true);
    case RdnaOpcode::SAshrI32:
        return simpleInteger(inst, IrOpcode::ShiftRightArithmetic32, IrType::U32, false, true, true);
    case RdnaOpcode::SAshrI64:
        return simpleInteger(inst, IrOpcode::ShiftRightArithmetic64, IrType::U64, false, false, true);
    case RdnaOpcode::SLshlB64:
        return simpleInteger(inst, IrOpcode::ShiftLeftLogical64, IrType::U64, false, false, true);
    case RdnaOpcode::SLshrB64:
        return simpleInteger(inst, IrOpcode::ShiftRightLogical64, IrType::U64, false, false, true);
    case RdnaOpcode::SAndn2B32:
        return composedIntegerBinary(inst, IrOpcode::BitwiseAnd32, true, false, true);
    case RdnaOpcode::SOrn2B32:
        return composedIntegerBinary(inst, IrOpcode::BitwiseOr32, true, false, true);
    case RdnaOpcode::SNandB32:
        return composedIntegerBinary(inst, IrOpcode::BitwiseAnd32, false, true, true);
    case RdnaOpcode::SNorB32:
        return composedIntegerBinary(inst, IrOpcode::BitwiseOr32, false, true, true);
    case RdnaOpcode::SXnorB32:
        return composedIntegerBinary(inst, IrOpcode::BitwiseXor32, false, true, true);
    case RdnaOpcode::SFf0I32B64:
        return sFfI32B64(inst, true);
    case RdnaOpcode::SFf1I32B64:
        return sFfI32B64(inst, false);
    case RdnaOpcode::SFlbitI32B32:
        return vFfbh32(inst, false);
    case RdnaOpcode::SFlbitI32:
        return vFfbh32(inst, true);
    case RdnaOpcode::SFlbitI32B64:
        return sFlbitI32B64(inst, false);
    case RdnaOpcode::SFlbitI32I64:
        return sFlbitI32B64(inst, true);
    case RdnaOpcode::SBitset0B32:
        return sBitsetB32(inst, false);
    case RdnaOpcode::SBitset1B32:
        return sBitsetB32(inst, true);
    case RdnaOpcode::SBitset0B64:
        return sBitsetB64(inst, false);
    case RdnaOpcode::SBitset1B64:
        return sBitsetB64(inst, true);
    case RdnaOpcode::SBitreplicateB64B32:
        return sBitreplicateB64B32(inst);
    case RdnaOpcode::SQuadmaskB32:
        return sQuadmask(inst, false);
    case RdnaOpcode::SQuadmaskB64:
        return sQuadmask(inst, true);
    case RdnaOpcode::SMovrelsB32:
    case RdnaOpcode::SMovrelsB64:
    case RdnaOpcode::SMovreldB32:
    case RdnaOpcode::SMovreldB64:
    case RdnaOpcode::SMovrelsd2B32:
        return sMovrel(inst);
    case RdnaOpcode::SBfmB32:
        return bfmB32(inst);
    case RdnaOpcode::SBfmB64:
        return sBfmB64(inst);
    case RdnaOpcode::SBfeU32:
        return sBfeU32(inst, false);
    case RdnaOpcode::SBfeI32:
        return sBfeU32(inst, true);
    case RdnaOpcode::SBfeU64:
        return sBfeU64(inst, false);
    case RdnaOpcode::SBfeI64:
        return sBfeU64(inst, true);
    case RdnaOpcode::SBitcmp0B32:
        return sBitcmpB32(inst, false);
    case RdnaOpcode::SBitcmp1B32:
        return sBitcmpB32(inst, true);
    case RdnaOpcode::SBitcmp0B64:
        return sBitcmpB64(inst, false);
    case RdnaOpcode::SBitcmp1B64:
        return sBitcmpB64(inst, true);
    case RdnaOpcode::SPackLlB32B16:
        return packB16(inst, false, false);
    case RdnaOpcode::SPackLhB32B16:
        return packB16(inst, false, true);
    case RdnaOpcode::SPackHhB32B16:
        return packB16(inst, true, true);
    case RdnaOpcode::SNop:
    case RdnaOpcode::SSleep:
    case RdnaOpcode::SWakeup:
    case RdnaOpcode::SSetprio:
    case RdnaOpcode::STrap:
    case RdnaOpcode::SClause:
    case RdnaOpcode::SIcacheInv:
    case RdnaOpcode::SIncperflevel:
    case RdnaOpcode::SDecperflevel:
        emitControlNop();
        return true;
    case RdnaOpcode::SRoundMode: {
        const std::uint32_t initial = floatMode.has_value() ? floatMode->floatMode : 0u;
        if ((inst.source0.value & 0xfu) != (initial & 0xfu)) {
            throw std::runtime_error("s_round_mode " + std::to_string(inst.source0.value) + " at pc " + std::to_string(inst.programCounter) + " changes the initial MODE rounding fields: runtime MODE changes are not implemented");
        }
        emitControlNop();
        return true;
    }
    case RdnaOpcode::SDenormMode:
        throw std::runtime_error("s_denorm_mode " + std::to_string(inst.source0.value) + " at pc " + std::to_string(inst.programCounter) + ": the recompiler does not model denormal modes");
    case RdnaOpcode::SSethalt:
        if ((inst.source0.value & 1u) != 0u) {
            throw std::runtime_error("s_sethalt " + std::to_string(inst.source0.value) + " at pc " + std::to_string(inst.programCounter) + " halts the wave until a debugger resumes it");
        }
        emitControlNop();
        return true;
    case RdnaOpcode::SSendmsghalt:
        throw std::runtime_error("s_sendmsghalt " + std::to_string(inst.source0.value) + " at pc " + std::to_string(inst.programCounter) + " halts the wave until a debugger resumes it");
    case RdnaOpcode::SCodeEnd:
        throw std::runtime_error("s_code_end at pc " + std::to_string(inst.programCounter) + " is reached: it marks the end of the code and raises an illegal instruction exception");
    case RdnaOpcode::SRfeB64:
        throw std::runtime_error("s_rfe_b64 at pc " + std::to_string(inst.programCounter) + " returns from a trap handler, and recompiled shaders run without one");
    case RdnaOpcode::SWaitcntDepctr:
    case RdnaOpcode::SWaitIdle:
        emitWaitcnt();
        return true;
    case RdnaOpcode::SBarrier:
        sBarrier();
        return true;
    case RdnaOpcode::SSendmsg:
        sSendmsg(inst);
        return true;
    case RdnaOpcode::STtracedata:
        sTtracedata();
        return true;
    case RdnaOpcode::SInstPrefetch:
        sInstPrefetch();
        return true;
    case RdnaOpcode::SCbranchCdbg:
        return true;
    case RdnaOpcode::SBranch:
    case RdnaOpcode::SCbranchScc0:
    case RdnaOpcode::SCbranchScc1:
    case RdnaOpcode::SCbranchVccz:
    case RdnaOpcode::SCbranchVccnz:
    case RdnaOpcode::SCbranchExecz:
    case RdnaOpcode::SCbranchExecnz:
    case RdnaOpcode::SEndpgm:
        return true;
    default:
        return false;
    }
}

void TranslateScalarInstruction(TranslationContext& context, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateScalarInstruction not implemented");
}

}
