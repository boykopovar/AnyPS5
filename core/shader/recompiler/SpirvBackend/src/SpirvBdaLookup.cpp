#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include <bit>
#include <stdexcept>
#include <utility>
#include <iterator>
#include <initializer_list>
#include <vector>

namespace ShaderRecompiler {

namespace {

void EmitBdaMarkWritten(SpirvEmitterState& state, std::uint32_t written, std::uint32_t begin, std::uint32_t address, std::uint32_t end) {
    if (state.faultBufferVariable == 0) throw std::runtime_error("BDA writes need the fault buffer for the written-page record");
    const auto u32 = TypeU32(state);
    const auto u64 = TypeScalarU64(state);
    const auto boolean = TypeBool(state);
    const auto constant = [&](std::uint32_t value) { return ConstantU32(state, value); };
    const auto binary = [&](std::uint32_t op, std::uint32_t type, std::uint32_t left, std::uint32_t right) { return Binary(state, op, type, left, right); };
    const auto scope = constant(spv::ScopeDevice);
    const auto relaxed = constant(spv::MemorySemanticsMaskNone);
    const auto block = binary(spv::OpBitwiseAnd, u32, written, constant((1u << BdaAbi::WrittenBlockBits) - 1u));
    const auto shift = Unary(state, spv::OpUConvert, u64, binary(spv::OpShiftRightLogical, u32, written, constant(BdaAbi::WrittenBlockBits)));
    const auto origin = binary(spv::OpShiftRightLogical, u64, begin, shift);
    const auto granule = [&](std::uint32_t at) { return Unary(state, spv::OpUConvert, u32, binary(spv::OpISub, u64, binary(spv::OpShiftRightLogical, u64, at, shift), origin)); };
    const auto first = granule(address);
    const auto last = granule(binary(spv::OpISub, u64, end, BdaConstant(state, 1u)));
    const auto maskOf = [&](std::uint32_t bit) { return binary(spv::OpShiftLeftLogical, u32, constant(1u), binary(spv::OpBitwiseAnd, u32, bit, constant(31u))); };
    const auto setBits = [&](std::uint32_t firstWord, std::uint32_t first, std::uint32_t lastWord, std::uint32_t last) {
        const auto shared = binary(spv::OpIEqual, boolean, firstWord, lastWord);
        const auto firstMask = binary(spv::OpBitwiseOr, u32, maskOf(first), Select(state, u32, shared, maskOf(last), constant(0u)));
        state.module.AddFunction(spv::OpAtomicOr, u32, state.module.AllocateId(), BdaWord(state, state.faultBufferVariable, firstWord), scope, relaxed, firstMask);
        EmitIfCondition(state, Unary(state, spv::OpLogicalNot, boolean, shared), [&] {
            state.module.AddFunction(spv::OpAtomicOr, u32, state.module.AllocateId(), BdaWord(state, state.faultBufferVariable, lastWord), scope, relaxed, maskOf(last));
        });
    };
    const auto blockWord = [&](std::uint32_t bit) { return binary(spv::OpIAdd, u32, block, binary(spv::OpShiftRightLogical, u32, bit, constant(5u))); };
    const auto pageShift = BdaConstant(state, BdaAbi::WrittenPageShift);
    const auto firstPage = Unary(state, spv::OpUConvert, u32, binary(spv::OpShiftRightLogical, u64, address, pageShift));
    const auto lastPage = Unary(state, spv::OpUConvert, u32, binary(spv::OpShiftRightLogical, u64, binary(spv::OpISub, u64, end, BdaConstant(state, 1u)), pageShift));
    const auto mapWord = [&](std::uint32_t page) {
        return binary(spv::OpIAdd, u32, constant(BdaAbi::WrittenPageMapWord), binary(spv::OpBitwiseAnd, u32, binary(spv::OpShiftRightLogical, u32, page, constant(5u)), constant(BdaAbi::WrittenPageMapWords - 1u)));
    };
    state.module.AddFunction(spv::OpAtomicStore, BdaWord(state, state.faultBufferVariable, constant(BdaAbi::WrittenDirtyWord)), scope, relaxed, constant(1u));
    setBits(blockWord(first), first, blockWord(last), last);
    setBits(mapWord(firstPage), firstPage, mapWord(lastPage), lastPage);
}

// (u64 address, u32 bytes, u32 instruction) -> u64 device address of the range holding all the bytes,
std::uint32_t DefineBdaLookup(SpirvEmitterState& state, const char* name, bool recordFaults, std::uint32_t permission = BdaAbi::Read, bool rangeResult = false) {
    const auto u32 = TypeU32(state);
    const auto u64 = TypeScalarU64(state);
    const auto boolean = TypeBool(state);
    const auto constant = [&](std::uint32_t value) { return ConstantU32(state, value); };
    const auto binary = [&](std::uint32_t op, std::uint32_t type, std::uint32_t left, std::uint32_t right) { return Binary(state, op, type, left, right); };
    const auto function = state.module.AllocateId();
    state.module.AddName(function, name);
    const auto rangeType = rangeResult ? state.module.Type(spv::OpTypeVector, u64, 3u) : 0u;
    const auto resultType = rangeResult ? rangeType : u64;
    const auto zero = rangeResult ? state.module.Constant(spv::OpConstantComposite, rangeType, BdaConstant(state, 0u), BdaConstant(state, 0u), BdaConstant(state, 0u)) : BdaConstant(state, 0u);
    state.module.AddFunction(spv::OpFunction, resultType, function, rangeResult ? spv::FunctionControlMaskNone : spv::FunctionControlDontInlineMask, state.module.Type(spv::OpTypeFunction, resultType, u64, u32, u32));
    const auto address = state.module.AllocateId();
    const auto bytes = state.module.AllocateId();
    const auto instruction = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionParameter, u64, address);
    state.module.AddFunction(spv::OpFunctionParameter, u32, bytes);
    state.module.AddFunction(spv::OpFunctionParameter, u32, instruction);
    EmitLabel(state, state.module.AllocateId());
    const auto outer = state.module.AllocateId();
    const auto outerBody = state.module.AllocateId();
    const auto outerContinue = state.module.AllocateId();
    const auto outerMerge = state.module.AllocateId();
    state.module.AddFunction(spv::OpBranch, outer);
    EmitLabel(state, outer);
    state.module.AddFunction(spv::OpLoopMerge, outerMerge, outerContinue, spv::LoopControlMaskNone);
    state.module.AddFunction(spv::OpBranch, outerBody);
    EmitLabel(state, outerBody);
    std::vector<std::uint32_t> exits{spv::OpPhi, resultType, state.module.AllocateId()};
    const auto leave = [&](std::uint32_t value) {
        exits.push_back(value);
        exits.push_back(state.currentLabel);
        state.module.AddFunction(spv::OpBranch, outerMerge);
    };
    const auto leaveIf = [&](std::uint32_t condition, std::uint32_t reason) {
        const auto failed = state.module.AllocateId();
        const auto next = state.module.AllocateId();
        state.module.AddFunction(spv::OpSelectionMerge, next, spv::SelectionControlMaskNone);
        state.module.AddFunction(spv::OpBranchConditional, condition, failed, next);
        EmitLabel(state, failed);
        if (recordFaults) RecordBdaFaultReason(state, address, bytes, instruction, reason);
        leave(zero);
        EmitLabel(state, next);
    };
    const auto fail = [&](std::uint32_t condition, BdaAbi::FaultReason reason) { leaveIf(condition, recordFaults ? constant(static_cast<std::uint32_t>(reason)) : 0u); };
    const auto failFirst = [&](std::initializer_list<std::pair<std::uint32_t, BdaAbi::FaultReason>> checks) {
        auto reason = constant(0u);
        for (auto check = std::rbegin(checks); check != std::rend(checks); ++check) {
            reason = Select(state, u32, check->first, constant(static_cast<std::uint32_t>(check->second)), reason);
        }
        leaveIf(binary(spv::OpINotEqual, boolean, reason, constant(0u)), reason);
    };
    const auto length = state.module.AllocateId();
    state.module.AddFunction(spv::OpArrayLength, u32, length, state.bdaPagetableVariable, 0u);
    fail(binary(spv::OpULessThan, boolean, length, constant(4)), BdaAbi::FaultReason::InvalidTable);
    const auto count = BdaLoadWord(state, constant(1));
    const auto available = binary(spv::OpISub, u32, length, constant(4));
    const auto end = binary(spv::OpIAdd, u64, address, Unary(state, spv::OpUConvert, u64, bytes));
    failFirst({
        {binary(spv::OpINotEqual, boolean, BdaLoadWord(state, constant(0)), constant(BdaAbi::Version)), BdaAbi::FaultReason::InvalidTable},
        {binary(spv::OpINotEqual, boolean, BdaLoadWord(state, constant(2)), constant(sizeof(BdaAbi::Range))), BdaAbi::FaultReason::InvalidTable},
        {binary(spv::OpINotEqual, boolean, BdaLoadWord(state, constant(3)), constant(0)), BdaAbi::FaultReason::InvalidTable},
        {binary(spv::OpINotEqual, boolean, binary(spv::OpBitwiseAnd, u32, available, constant(7)), constant(0)), BdaAbi::FaultReason::InvalidTable},
        {binary(spv::OpINotEqual, boolean, count, binary(spv::OpShiftRightLogical, u32, available, constant(3))), BdaAbi::FaultReason::InvalidTable},
        {binary(spv::OpIEqual, boolean, bytes, constant(0)), BdaAbi::FaultReason::Overflow},
        {binary(spv::OpULessThanEqual, boolean, end, address), BdaAbi::FaultReason::Overflow},
    });
    const auto header = state.module.AllocateId();
    const auto body = state.module.AllocateId();
    const auto continuation = state.module.AllocateId();
    const auto merge = state.module.AllocateId();
    const auto entryLabel = state.currentLabel;
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, header);
    const auto lower = state.module.AllocateId();
    const auto upper = state.module.AllocateId();
    const auto newLow = state.module.AllocateId();
    const auto newHigh = state.module.AllocateId();
    state.module.AddFunction(spv::OpPhi, u32, lower, constant(0), entryLabel, newLow, continuation);
    state.module.AddFunction(spv::OpPhi, u32, upper, count, entryLabel, newHigh, continuation);
    const auto search = binary(spv::OpULessThan, boolean, lower, upper);
    state.module.AddFunction(spv::OpLoopMerge, merge, continuation, spv::LoopControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, search, body, merge);
    EmitLabel(state, body);
    const auto midpoint = binary(spv::OpIAdd, u32, lower, binary(spv::OpShiftRightLogical, u32, binary(spv::OpISub, u32, upper, lower), constant(1)));
    const auto entry = binary(spv::OpIAdd, u32, constant(4), binary(spv::OpIMul, u32, midpoint, constant(8)));
    const auto before = binary(spv::OpULessThan, boolean, address, BdaLoadAddress(state, entry));
    state.module.AddFunction(spv::OpSelect, u32, newLow, before, lower, binary(spv::OpIAdd, u32, midpoint, constant(1)));
    state.module.AddFunction(spv::OpSelect, u32, newHigh, before, midpoint, upper);
    state.module.AddFunction(spv::OpBranch, continuation);
    EmitLabel(state, continuation);
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, merge);
    const auto index = lower;
    fail(binary(spv::OpIEqual, boolean, index, constant(0)), BdaAbi::FaultReason::Unmapped);
    const auto selected = binary(spv::OpIAdd, u32, constant(4), binary(spv::OpIMul, u32, binary(spv::OpISub, u32, index, constant(1)), constant(8)));
    const auto at = [&](std::uint32_t offset) { return binary(spv::OpIAdd, u32, selected, constant(offset)); };
    const auto begin = BdaLoadAddress(state, selected);
    const auto finish = BdaLoadAddress(state, at(2));
    const auto base = BdaLoadAddress(state, at(4));
    const auto permissions = BdaLoadWord(state, at(6));
    const auto offset = binary(spv::OpISub, u64, address, begin);
    const auto result = binary(spv::OpIAdd, u64, base, offset);
    const auto deviceEnd = binary(spv::OpIAdd, u64, result, Unary(state, spv::OpUConvert, u64, bytes));
    const bool tracksWrites = (permission & BdaAbi::Write) != 0;
    const auto written = BdaLoadWord(state, at(7));
    failFirst({
        {tracksWrites ? ConstantBool(state, false) : binary(spv::OpIEqual, boolean, written, constant(BdaAbi::WrittenInvalid)), BdaAbi::FaultReason::InvalidTable},
        {binary(spv::OpUGreaterThanEqual, boolean, begin, finish), BdaAbi::FaultReason::InvalidTable},
        {binary(spv::OpUGreaterThan, boolean, end, finish), BdaAbi::FaultReason::Unmapped},
        {binary(spv::OpINotEqual, boolean, binary(spv::OpBitwiseAnd, u32, permissions, constant(permission)), constant(permission)), BdaAbi::FaultReason::Permission},
        {tracksWrites ? binary(spv::OpIEqual, boolean, written, constant(0)) : ConstantBool(state, false), BdaAbi::FaultReason::InvalidTable},
        {binary(spv::OpIEqual, boolean, base, BdaConstant(state, 0)), BdaAbi::FaultReason::InvalidTable},
        {binary(spv::OpULessThan, boolean, result, base), BdaAbi::FaultReason::Overflow},
        {binary(spv::OpULessThanEqual, boolean, deviceEnd, result), BdaAbi::FaultReason::Overflow},
    });
    if (rangeResult) {
        const auto last = binary(spv::OpUGreaterThanEqual, boolean, index, count);
        const auto next = BdaLoadAddress(state, binary(spv::OpIAdd, u32, constant(4), binary(spv::OpIMul, u32, Select(state, u32, last, binary(spv::OpISub, u32, index, constant(1)), index), constant(8))));
        const auto bound = Select(state, u64, binary(spv::OpLogicalOr, boolean, last, binary(spv::OpULessThan, boolean, finish, next)), finish, next);
        const auto range = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeConstruct, rangeType, range, begin, bound, base);
        leave(range);
    } else {
        if (tracksWrites) EmitBdaMarkWritten(state, written, begin, address, end);
        leave(result);
    }
    EmitLabel(state, outerContinue);
    state.module.AddFunction(spv::OpBranch, outer);
    EmitLabel(state, outerMerge);
    state.module.AddFunction(exits);
    state.module.AddFunction(spv::OpReturnValue, exits[2]);
    state.module.AddFunction(spv::OpFunctionEnd);
    return function;
}

}

void DefineBdaAnchorFunction(SpirvEmitterState& state) {
    if (state.bdaAnchorFunction == 0) state.bdaAnchorFunction = DefineBdaLookup(state, "bda_anchor_range", false, BdaAbi::Read, true);
}

void DefineGetBdaPointer(SpirvEmitterState& state) {
    if (state.faultBufferVariable != 0) DefineBdaFaultFunction(state);
    if (!state.program.Info().usesDma) return;
    state.bdaPointerFunction = DefineBdaLookup(state, "get_bda_pointer", true);
    if (!BdaByteReadsForced()) state.bdaProbeFunction = DefineBdaLookup(state, "probe_bda_pointer", false);
    DefineBdaDwordReadFunctions(state);
    DefineBdaSpanReadFunctions(state);
    CollectBdaAnchors(state);
    if (!state.bdaAnchors.empty()) DefineBdaAnchorFunction(state);
    if (state.program.Info().bdaWrites) {
        state.bdaWritePointerFunction = DefineBdaLookup(state, "get_bda_write_pointer", true, BdaAbi::Write);
        if (!BdaByteReadsForced()) state.bdaWriteProbeFunction = DefineBdaLookup(state, "probe_bda_write_pointer", false, BdaAbi::Write);
        state.bdaAtomicPointerFunction = DefineBdaLookup(state, "get_bda_atomic_pointer", true, BdaAbi::Read | BdaAbi::Write);
        DefineBdaByteWriteFunctions(state);
    }
}

}
