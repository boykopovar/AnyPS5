#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include <spirv/unified1/GLSL.std.450.h>
#include "PipelineSpecialization.hpp"
#include <bit>
#include <utility>
#include <iterator>
#include <initializer_list>
#include <vector>

namespace ShaderRecompiler {

namespace {

std::uint32_t LoadFaultWord(SpirvEmitterState& state, std::uint32_t index) {
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), value, BdaWord(state, state.faultBufferVariable, ConstantU32(state, index)));
    return value;
}

std::uint32_t LoadFaultAddress(SpirvEmitterState& state, std::uint32_t index) {
    const auto u64 = TypeScalarU64(state);
    const auto low = Unary(state, spv::OpUConvert, u64, LoadFaultWord(state, index));
    const auto high = Unary(state, spv::OpUConvert, u64, LoadFaultWord(state, index + 1u));
    return Binary(state, spv::OpBitwiseOr, u64, low, Binary(state, spv::OpShiftLeftLogical, u64, high, BdaConstant(state, 32u)));
}

std::uint32_t StorePagePointer(SpirvEmitterState& state, std::uint32_t pool, std::uint32_t word) {
    const auto u64 = TypeScalarU64(state);
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpConvertUToPtr, TypePhysicalU32Pointer(state), pointer, Binary(state, spv::OpIAdd, u64, pool, Binary(state, spv::OpShiftLeftLogical, u64, word, BdaConstant(state, 2u))));
    return pointer;
}

std::uint32_t StorePageWordOf(SpirvEmitterState& state, std::uint32_t slots, std::uint32_t slot, std::uint32_t first, std::uint32_t stride) {
    const auto u64 = TypeScalarU64(state);
    const auto wide = [&](std::uint32_t value) { return Unary(state, spv::OpUConvert, u64, value); };
    const auto base = Binary(state, spv::OpIMul, u64, wide(slots), BdaConstant(state, first));
    return Binary(state, spv::OpIAdd, u64, base, Binary(state, spv::OpIMul, u64, wide(slot), BdaConstant(state, stride)));
}

std::uint32_t DefineBdaStorePagePointer(SpirvEmitterState& state) {
    const auto u32 = TypeU32(state);
    const auto u64 = TypeScalarU64(state);
    const auto boolean = TypeBool(state);
    const auto constant = [&](std::uint32_t value) { return ConstantU32(state, value); };
    const auto binary = [&](std::uint32_t op, std::uint32_t type, std::uint32_t left, std::uint32_t right) { return Binary(state, op, type, left, right); };
    const auto store = [&](std::uint32_t variable, std::uint32_t value) { state.module.AddFunction(spv::OpStore, variable, value); };
    const auto load = [&](std::uint32_t type, std::uint32_t variable) {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, type, value, variable);
        return value;
    };
    const auto function = state.module.AllocateId();
    state.module.AddName(function, "bda_store_page_pointer");
    state.module.AddFunction(spv::OpFunction, u64, function, spv::FunctionControlDontInlineMask, state.module.Type(spv::OpTypeFunction, u64, u64, u32, u64, u32, boolean));
    const auto address = state.module.AllocateId();
    const auto bytes = state.module.AllocateId();
    const auto result = state.module.AllocateId();
    const auto permissions = state.module.AllocateId();
    const auto writes = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionParameter, u64, address);
    state.module.AddFunction(spv::OpFunctionParameter, u32, bytes);
    state.module.AddFunction(spv::OpFunctionParameter, u64, result);
    state.module.AddFunction(spv::OpFunctionParameter, u32, permissions);
    state.module.AddFunction(spv::OpFunctionParameter, boolean, writes);
    EmitLabel(state, state.module.AllocateId());
    const auto found = state.module.AllocateId();
    state.module.AddFunction(spv::OpVariable, TypePointer(state, spv::StorageClassFunction, u32), found, spv::StorageClassFunction);
    const auto counter = state.module.AllocateId();
    state.module.AddFunction(spv::OpVariable, TypePointer(state, spv::StorageClassFunction, u32), counter, spv::StorageClassFunction);
    const auto done = state.module.AllocateId();
    state.module.AddFunction(spv::OpVariable, TypePointer(state, spv::StorageClassFunction, boolean), done, spv::StorageClassFunction);
    store(found, constant(BdaAbi::StorePageNone));
    store(counter, constant(0u));
    store(done, ConstantBool(state, false));
    const auto scope = constant(spv::ScopeDevice);
    const auto relaxed = constant(spv::MemorySemanticsMaskNone);
    const auto flagged = [&](std::uint32_t flags) { return binary(spv::OpINotEqual, boolean, binary(spv::OpBitwiseAnd, u32, permissions, constant(flags)), constant(0u)); };
    const auto pool = LoadFaultAddress(state, BdaAbi::StorePagesWord);
    const auto base = LoadFaultAddress(state, BdaAbi::StorePageBaseWord);
    const auto slots = LoadFaultWord(state, BdaAbi::StorePageSlotsWord);
    const auto offset = Unary(state, spv::OpUConvert, u32, binary(spv::OpBitwiseAnd, u64, address, BdaConstant(state, BdaAbi::StorePageBytes - 1u)));
    const auto fits = binary(spv::OpULessThanEqual, boolean, binary(spv::OpIAdd, u32, offset, bytes), constant(BdaAbi::StorePageBytes));
    auto usable = binary(spv::OpLogicalAnd, boolean, binary(spv::OpINotEqual, boolean, pool, BdaConstant(state, 0u)), binary(spv::OpUGreaterThan, boolean, slots, constant(1u)));
    usable = binary(spv::OpLogicalAnd, boolean, usable, binary(spv::OpLogicalAnd, boolean, fits, binary(spv::OpUGreaterThanEqual, boolean, address, base)));
    EmitIfCondition(state, usable, [&] {
        const auto key = binary(spv::OpIAdd, u32, Unary(state, spv::OpUConvert, u32, binary(spv::OpShiftRightLogical, u64, binary(spv::OpISub, u64, address, base), BdaConstant(state, BdaAbi::StorePageShift))), constant(1u));
        const auto shift = binary(spv::OpISub, u32, constant(32u), EmitGlsl<GLSLstd450FindILsb, IrType::U32>(state, slots));
        const auto hash = binary(spv::OpShiftRightLogical, u32, binary(spv::OpIMul, u32, key, constant(0x9e3779b1u)), shift);
        const auto comparator = Select(state, u32, writes, constant(0u), key);
        const auto header = state.module.AllocateId();
        const auto body = state.module.AllocateId();
        const auto continuation = state.module.AllocateId();
        const auto merge = state.module.AllocateId();
        state.module.AddFunction(spv::OpBranch, header);
        EmitLabel(state, header);
        const auto probe = load(u32, counter);
        const auto searching = binary(spv::OpLogicalAnd, boolean, Unary(state, spv::OpLogicalNot, boolean, load(boolean, done)), binary(spv::OpULessThan, boolean, probe, constant(BdaAbi::StorePageProbes)));
        state.module.AddFunction(spv::OpLoopMerge, merge, continuation, spv::LoopControlMaskNone);
        state.module.AddFunction(spv::OpBranchConditional, searching, body, merge);
        EmitLabel(state, body);
        const auto slot = binary(spv::OpBitwiseAnd, u32, binary(spv::OpIAdd, u32, hash, probe), binary(spv::OpISub, u32, slots, constant(1u)));
        const auto previous = state.module.AllocateId();
        state.module.AddFunction(spv::OpAtomicCompareExchange, u32, previous, StorePagePointer(state, pool, Unary(state, spv::OpUConvert, u64, slot)), scope, relaxed, relaxed, key, comparator);
        const auto empty = binary(spv::OpIEqual, boolean, previous, constant(0u));
        const auto hit = binary(spv::OpLogicalOr, boolean, binary(spv::OpIEqual, boolean, previous, key), binary(spv::OpLogicalAnd, boolean, writes, empty));
        store(found, Select(state, u32, hit, slot, load(u32, found)));
        store(done, binary(spv::OpLogicalOr, boolean, hit, empty));
        state.module.AddFunction(spv::OpBranch, continuation);
        EmitLabel(state, continuation);
        store(counter, binary(spv::OpIAdd, u32, probe, constant(1u)));
        state.module.AddFunction(spv::OpBranch, header);
        EmitLabel(state, merge);
    });
    const auto slot = load(u32, found);
    const auto hit = binary(spv::OpINotEqual, boolean, slot, constant(BdaAbi::StorePageNone));
    EmitIfCondition(state, binary(spv::OpLogicalAnd, boolean, writes, binary(spv::OpLogicalAnd, boolean, fits, Unary(state, spv::OpLogicalNot, boolean, hit))), [&] {
        state.module.AddFunction(spv::OpAtomicStore, BdaWord(state, state.faultBufferVariable, constant(BdaAbi::StorePageOverflowWord)), scope, relaxed, constant(1u));
    });
    EmitIfCondition(state, binary(spv::OpLogicalAnd, boolean, writes, binary(spv::OpLogicalAnd, boolean, hit, flagged(BdaAbi::Masked))), [&] {
        const auto first = binary(spv::OpIAdd, u64, StorePageWordOf(state, slots, slot, 1u, BdaAbi::StoreMaskWords), Unary(state, spv::OpUConvert, u64, binary(spv::OpShiftRightLogical, u32, offset, constant(5u))));
        const auto ones = binary(spv::OpISub, u64, binary(spv::OpShiftLeftLogical, u64, BdaConstant(state, 1u), bytes), BdaConstant(state, 1u));
        const auto bits = binary(spv::OpShiftLeftLogical, u64, ones, binary(spv::OpBitwiseAnd, u32, offset, constant(31u)));
        const auto high = Unary(state, spv::OpUConvert, u32, binary(spv::OpShiftRightLogical, u64, bits, BdaConstant(state, 32u)));
        state.module.AddFunction(spv::OpAtomicOr, u32, state.module.AllocateId(), StorePagePointer(state, pool, first), scope, relaxed, Unary(state, spv::OpUConvert, u32, bits));
        EmitIfCondition(state, binary(spv::OpINotEqual, boolean, high, constant(0u)), [&] {
            state.module.AddFunction(spv::OpAtomicOr, u32, state.module.AllocateId(), StorePagePointer(state, pool, binary(spv::OpIAdd, u64, first, BdaConstant(state, 1u))), scope, relaxed, high);
        });
    });
    const auto page = binary(spv::OpIAdd, u64, pool, binary(spv::OpShiftLeftLogical, u64, StorePageWordOf(state, slots, slot, 1u + BdaAbi::StoreMaskWords, BdaAbi::StorePageWords), BdaConstant(state, 2u)));
    const auto redirected = Select(state, u64, binary(spv::OpLogicalAnd, boolean, hit, flagged(BdaAbi::Spill)), binary(spv::OpIAdd, u64, page, Unary(state, spv::OpUConvert, u64, offset)), result);
    const auto failed = Select(state, boolean, writes, Unary(state, spv::OpLogicalNot, boolean, hit), Unary(state, spv::OpLogicalNot, boolean, fits));
    state.module.AddFunction(spv::OpReturnValue, Select(state, u64, failed, BdaConstant(state, 0u), redirected));
    state.module.AddFunction(spv::OpFunctionEnd);
    return function;
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
    failFirst({
        {binary(spv::OpINotEqual, boolean, BdaLoadWord(state, at(7)), constant(0)), BdaAbi::FaultReason::InvalidTable},
        {binary(spv::OpUGreaterThanEqual, boolean, begin, finish), BdaAbi::FaultReason::InvalidTable},
        {binary(spv::OpUGreaterThan, boolean, end, finish), BdaAbi::FaultReason::Unmapped},
        {binary(spv::OpINotEqual, boolean, binary(spv::OpBitwiseAnd, u32, permissions, constant(permission)), constant(permission)), BdaAbi::FaultReason::Permission},
        {binary(spv::OpIEqual, boolean, base, BdaConstant(state, 0)), BdaAbi::FaultReason::InvalidTable},
        {binary(spv::OpULessThan, boolean, result, base), BdaAbi::FaultReason::Overflow},
        {binary(spv::OpULessThanEqual, boolean, deviceEnd, result), BdaAbi::FaultReason::Overflow},
    });
    const bool storePages = state.bdaStorePageFunction != 0;
    const auto flagged = [&](std::uint32_t flags) { return binary(spv::OpINotEqual, boolean, binary(spv::OpBitwiseAnd, u32, permissions, constant(flags)), constant(0u)); };
    if (rangeResult && storePages) fail(flagged(BdaAbi::Spill), BdaAbi::FaultReason::StorePages);
    if (rangeResult) {
        const auto last = binary(spv::OpUGreaterThanEqual, boolean, index, count);
        const auto next = BdaLoadAddress(state, binary(spv::OpIAdd, u32, constant(4), binary(spv::OpIMul, u32, Select(state, u32, last, binary(spv::OpISub, u32, index, constant(1)), index), constant(8))));
        const auto bound = Select(state, u64, binary(spv::OpLogicalOr, boolean, last, binary(spv::OpULessThan, boolean, finish, next)), finish, next);
        const auto range = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeConstruct, rangeType, range, begin, bound, base);
        leave(range);
    } else if (storePages) {
        const bool writes = (permission & BdaAbi::Write) != 0u;
        const auto paged = flagged(writes ? BdaAbi::Spill | BdaAbi::Masked : BdaAbi::Spill);
        const auto enabled = state.bdaStorePageSpecialization != 0 ? binary(spv::OpINotEqual, boolean, state.bdaStorePageSpecialization, constant(0u)) : ConstantBool(state, true);
        const auto pointer = EmitValueOrDefaultIfCondition(state, binary(spv::OpLogicalAnd, boolean, paged, enabled), u64, result, [&] {
            const auto value = state.module.AllocateId();
            state.module.AddFunction(spv::OpFunctionCall, u64, value, state.bdaStorePageFunction, address, bytes, result, permissions, ConstantBool(state, writes));
            return value;
        });
        const auto unpaged = binary(spv::OpLogicalAnd, boolean, paged, Unary(state, spv::OpLogicalNot, boolean, enabled));
        fail(binary(spv::OpLogicalOr, boolean, binary(spv::OpIEqual, boolean, pointer, BdaConstant(state, 0u)), unpaged), BdaAbi::FaultReason::StorePages);
        leave(pointer);
    } else {
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

static std::uint32_t DefineBdaNoteWrite(SpirvEmitterState& state) {
    const auto u32 = TypeU32(state);
    const auto boolean = TypeBool(state);
    const auto constant = [&](std::uint32_t value) { return ConstantU32(state, value); };
    const auto binary = [&](std::uint32_t op, std::uint32_t type, std::uint32_t left, std::uint32_t right) { return Binary(state, op, type, left, right); };
    const auto voidType = state.module.Type(spv::OpTypeVoid);
    const auto function = state.module.AllocateId();
    state.module.AddName(function, "note_bda_write");
    state.module.AddFunction(spv::OpFunction, voidType, function, spv::FunctionControlMaskNone, state.module.Type(spv::OpTypeFunction, voidType, TypeScalarU64(state)));
    const auto address = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionParameter, TypeScalarU64(state), address);
    EmitLabel(state, state.module.AllocateId());
    const auto done = state.module.AllocateId();
    state.module.AddFunction(spv::OpVariable, TypePointer(state, spv::StorageClassFunction, boolean), done, spv::StorageClassFunction);
    const auto counter = state.module.AllocateId();
    state.module.AddFunction(spv::OpVariable, TypePointer(state, spv::StorageClassFunction, u32), counter, spv::StorageClassFunction);
    state.module.AddFunction(spv::OpStore, done, ConstantBool(state, false));
    state.module.AddFunction(spv::OpStore, counter, constant(0u));
    const auto u64 = TypeScalarU64(state);
    const auto key = binary(spv::OpIAdd, u32, Unary(state, spv::OpUConvert, u32, binary(spv::OpShiftRightLogical, u64, address, BdaConstant(state, BdaAbi::WrittenChunkShift))), constant(1));
    const auto page = binary(spv::OpBitwiseAnd, u32, Unary(state, spv::OpUConvert, u32, binary(spv::OpShiftRightLogical, u64, address, BdaConstant(state, BdaAbi::WrittenPageShift))), constant(BdaAbi::WrittenChunkPages - 1u));
    const auto bit = binary(spv::OpShiftLeftLogical, u32, constant(1u), page);
    const auto hash = binary(spv::OpShiftRightLogical, u32, binary(spv::OpIMul, u32, key, constant(0x9e3779b1u)), constant(32u - static_cast<std::uint32_t>(std::countr_zero(BdaAbi::WrittenPageSlots))));
    const auto scope = constant(spv::ScopeDevice);
    const auto relaxed = constant(spv::MemorySemanticsMaskNone);
    const auto header = state.module.AllocateId();
    const auto body = state.module.AllocateId();
    const auto continuation = state.module.AllocateId();
    const auto merge = state.module.AllocateId();
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, header);
    const auto probe = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, u32, probe, counter);
    const auto found = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, boolean, found, done);
    const auto searching = binary(spv::OpLogicalAnd, boolean, Unary(state, spv::OpLogicalNot, boolean, found), binary(spv::OpULessThan, boolean, probe, constant(BdaAbi::WrittenPageProbes)));
    state.module.AddFunction(spv::OpLoopMerge, merge, continuation, spv::LoopControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, searching, body, merge);
    EmitLabel(state, body);
    const auto slot = binary(spv::OpBitwiseAnd, u32, binary(spv::OpIAdd, u32, hash, probe), constant(BdaAbi::WrittenPageSlots - 1u));
    const auto keyWord = binary(spv::OpIAdd, u32, constant(BdaAbi::WrittenSlotsWord), binary(spv::OpShiftLeftLogical, u32, slot, constant(1u)));
    const auto current = state.module.AllocateId();
    state.module.AddFunction(spv::OpAtomicLoad, u32, current, BdaWord(state, state.faultBufferVariable, keyWord), scope, relaxed);
    const auto same = binary(spv::OpIEqual, boolean, current, key);
    state.module.AddFunction(spv::OpStore, done, same);
    EmitIfCondition(state, Unary(state, spv::OpLogicalNot, boolean, same), [&] {
        const auto previous = state.module.AllocateId();
        state.module.AddFunction(spv::OpAtomicCompareExchange, u32, previous, BdaWord(state, state.faultBufferVariable, keyWord), scope, relaxed, relaxed, key, constant(0u));
        const auto claimed = binary(spv::OpIEqual, boolean, previous, constant(0u));
        state.module.AddFunction(spv::OpStore, done, binary(spv::OpLogicalOr, boolean, claimed, binary(spv::OpIEqual, boolean, previous, key)));
        EmitIfCondition(state, claimed, [&] {
            const auto index = state.module.AllocateId();
            state.module.AddFunction(spv::OpAtomicIAdd, u32, index, BdaWord(state, state.faultBufferVariable, constant(BdaAbi::WrittenCountWord)), scope, relaxed, constant(1u));
            state.module.AddFunction(spv::OpAtomicStore, BdaWord(state, state.faultBufferVariable, binary(spv::OpIAdd, u32, constant(BdaAbi::WrittenListWord), index)), scope, relaxed, slot);
        });
    });
    const auto holds = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, boolean, holds, done);
    EmitIfCondition(state, holds, [&] {
        const auto maskWord = binary(spv::OpIAdd, u32, keyWord, constant(1u));
        const auto mask = state.module.AllocateId();
        state.module.AddFunction(spv::OpAtomicLoad, u32, mask, BdaWord(state, state.faultBufferVariable, maskWord), scope, relaxed);
        EmitIfCondition(state, binary(spv::OpIEqual, boolean, binary(spv::OpBitwiseAnd, u32, mask, bit), constant(0u)), [&] {
            state.module.AddFunction(spv::OpAtomicOr, u32, state.module.AllocateId(), BdaWord(state, state.faultBufferVariable, maskWord), scope, relaxed, bit);
        });
    });
    state.module.AddFunction(spv::OpBranch, continuation);
    EmitLabel(state, continuation);
    state.module.AddFunction(spv::OpStore, counter, binary(spv::OpIAdd, u32, probe, constant(1u)));
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, merge);
    const auto noted = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, boolean, noted, done);
    EmitIfCondition(state, Unary(state, spv::OpLogicalNot, boolean, noted), [&] {
        state.module.AddFunction(spv::OpAtomicStore, BdaWord(state, state.faultBufferVariable, constant(BdaAbi::WrittenOverflowWord)), scope, relaxed, constant(1u));
    });
    state.module.AddFunction(spv::OpReturn);
    state.module.AddFunction(spv::OpFunctionEnd);
    return function;
}

void DefineBdaAnchorFunction(SpirvEmitterState& state) {
    if (state.bdaAnchorFunction == 0) state.bdaAnchorFunction = DefineBdaLookup(state, "bda_anchor_range", false, BdaAbi::Read, true);
}

void DefineGetBdaPointer(SpirvEmitterState& state) {
    if (state.faultBufferVariable != 0) DefineBdaFaultFunction(state);
    if (!state.program.Info().usesDma) return;
    if (state.program.Info().bdaWrites && state.faultBufferVariable != 0) {
        state.bdaStorePageFunction = DefineBdaStorePagePointer(state);
        if (StageOf(state) == IrShaderStage::Compute) state.bdaStorePageSpecialization = state.module.SpecializationConstant(TypeU32(state), PipelineSpecialization::StorePages, 0u);
    }
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
        state.bdaNoteWriteFunction = DefineBdaNoteWrite(state);
        DefineBdaByteWriteFunctions(state);
    }
}

}
