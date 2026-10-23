#include "Optimization/SrtWalker/SrtEvaluator.hpp"
#include "Optimization/SrtWalker/SrtAddressArithmetic.hpp"
#include "Optimization/SrtWalker/SrtInstructionPredicates.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ShaderRecompiler::Detail {

namespace {

struct DenseValuePool {
    std::vector<std::unique_ptr<std::vector<DenseValues::Slot>>> arrays;
    std::vector<std::vector<DenseValues::Slot>*> free;
    std::uint64_t stamp = 0;
};

DenseValuePool& denseValuePool() {
    struct DenseValuePoolStorage {};
    return HostThreadLocal<DenseValuePool, DenseValuePoolStorage>();
}

}

DenseValues::DenseValues(std::size_t values) : _size(values) {
    if (values == 0u) return;
    auto& pool = denseValuePool();
    if (pool.free.empty()) {
        pool.arrays.push_back(std::make_unique<std::vector<Slot>>());
        _array = pool.arrays.back().get();
    } else {
        _array = pool.free.back();
        pool.free.pop_back();
    }
    if (_array->size() < values) _array->resize(values);
    _slots = _array->data();
    pool.stamp += 2u;
    _stamp = pool.stamp;
}

DenseValues::~DenseValues() {
    if (_array == nullptr) return;
    denseValuePool().free.push_back(_array);
}

template<typename TNode>
bool BasicEvaluator<TNode>::Evaluate(TNode value, std::uint32_t& result) {
    std::uint64_t wide = 0;
    if (!EvaluateWide(value, wide)) {
        return false;
    }
    result = static_cast<std::uint32_t>(wide);
    return true;
}

template<typename TNode>
bool BasicEvaluator<TNode>::EvaluateWide(TNode value, std::uint64_t& result) {
    if (value.HasImmediate()) {
        const auto bits = value.ImmediateBits();
        switch (value.Type()) {
            case IrType::Bool: result = bits != 0u ? 1u : 0u; return true;
            case IrType::U8: result = static_cast<std::uint8_t>(bits); return true;
            case IrType::U16: result = static_cast<std::uint16_t>(bits); return true;
            case IrType::U32: result = static_cast<std::uint32_t>(bits); return true;
            case IrType::U64: result = bits; return true;
            case IrType::F32: result = Float32Bits(std::bit_cast<float>(static_cast<std::uint32_t>(bits))); return true;
            default: return false;
        }
    }
    if (value.Opcode() == IrOpcode::Void) {
        return false;
    }
    const TNode& inst = value;
    if (_masked && IsRuntimeSelect(inst.Opcode()) && inst.ArgumentCount() == 3 && ActiveMaskImplies(_activeMask, inst.Argument(0))) {
        return EvaluateWide(inst.Argument(1), result);
    }
    if (inst.Id() < _dense.Size()) {
        auto& slot = _dense.At(inst.Id());
        if (slot.stamp == _dense.Done()) {
            result = slot.value;
            return true;
        }
        if (slot.stamp == _dense.Visiting()) {
            return false;
        }
        slot.stamp = _dense.Visiting();
        std::uint64_t out = 0;
        if (!EvaluateInst(inst, out)) {
            slot.stamp = 0;
            static const bool debug = std::getenv("APS5_SRT_DEBUG") != nullptr;
            if (debug) std::fprintf(stderr, "[srt] cannot evaluate %s (%zu arguments)\n", std::string(IrOpcodeName(inst.Opcode())).c_str(), inst.ArgumentCount());
            return false;
        }
        slot = {_dense.Done(), out};
        result = out;
        return true;
    }
    if (_cache.Find(inst.Value(), result)) {
        return true;
    }
    if (std::find(_visiting.begin(), _visiting.end(), inst.Value()) != _visiting.end()) {
        return false;
    }
    _visiting.push_back(inst.Value());
    std::uint64_t out = 0;
    const bool evaluated = EvaluateInst(inst, out);
    _visiting.pop_back();
    if (!evaluated) {
        static const bool debug = std::getenv("APS5_SRT_DEBUG") != nullptr;
        if (debug) std::fprintf(stderr, "[srt] cannot evaluate %s (%zu arguments)\n", std::string(IrOpcodeName(inst.Opcode())).c_str(), inst.ArgumentCount());
        return false;
    }
    _cache.Insert(inst.Value(), out);
    result = out;
    return true;
}

template<typename TNode>
float BasicEvaluator<TNode>::Float32(std::uint64_t bits) { return std::bit_cast<float>(static_cast<std::uint32_t>(bits)); }

template<typename TNode>
std::uint64_t BasicEvaluator<TNode>::Float32Bits(float value) { return std::bit_cast<std::uint32_t>(value); }

template<typename TNode>
bool BasicEvaluator<TNode>::Arg(const TNode& inst, std::size_t index, std::uint64_t& result) { return EvaluateWide(inst.Argument(index), result); }

template<typename TNode>
bool BasicEvaluator<TNode>::EvaluatePhi(const TNode& inst, std::uint64_t& result) {
    IrValue* value = ResolveInvariantPhi(_program, inst.Value());
    return value != nullptr && EvaluateWide(TNode::From(_program, value), result);
}

template<typename TNode>
bool BasicEvaluator<TNode>::EvaluateExtract(const TNode& inst, std::uint64_t& result) {
    const TNode index = inst.Argument(1);
    if (!index.HasImmediate() || index.Type() != IrType::U32) {
        return false;
    }
    const auto component = static_cast<std::uint32_t>(index.ImmediateBits());
    if (component >= 2u) {
        return false;
    }
    if (inst.Opcode() == IrOpcode::CompositeExtractU64) {
        std::uint64_t packed = 0;
        if (!Arg(inst, 0, packed)) {
            return false;
        }
        result = static_cast<std::uint32_t>(packed >> (component * 32u));
        return true;
    }
    const TNode source = inst.Argument(0);
    if (source.Opcode() == IrOpcode::Void) {
        return false;
    }
    if (source.Opcode() == IrOpcode::CompositeConstructU32x2) {
        return EvaluateWide(source.Argument(component), result);
    }
    if (source.Opcode() == IrOpcode::IAddCarry32) {
        std::uint64_t lhs = 0;
        std::uint64_t rhs = 0;
        if (!Arg(source, 0, lhs) || !Arg(source, 1, rhs)) {
            return false;
        }
        const auto sum = static_cast<std::uint64_t>(static_cast<std::uint32_t>(lhs)) + static_cast<std::uint32_t>(rhs);
        result = component == 0u ? static_cast<std::uint32_t>(sum) : static_cast<std::uint32_t>(sum >> 32u);
        return true;
    }
    return false;
}

template<typename TNode>
bool BasicEvaluator<TNode>::EvaluateRawRead(const TNode& inst, std::uint64_t& result) {
    const auto flags = inst.template Flags<MemoryFlags>();
    if (flags.index >= _program.memoryInfo.size()) {
        return false;
    }
    const auto& mem = _program.memoryInfo[flags.index];
    const TNode handle = inst.Argument(0);
    if (handle.Opcode() == IrOpcode::Void) {
        return false;
    }
    std::uint64_t low = 0;
    std::uint64_t high = 0;
    std::uint64_t offset = 0;
    std::int32_t origin = -1;
    const auto baseWord = [&] {
        const TNode source = handle.Argument(0);
        if (_runtime.userDataUse == nullptr || source.HasImmediate() || source.Opcode() != IrOpcode::GetUserData) return Arg(handle, 0, low);
        const auto reg = RegIndex(static_cast<ScalarReg>(source.Argument(0).RegisterIndex()));
        if (reg < _program.userDataBase || reg - _program.userDataBase >= _runtime.userData.size() || reg - _program.userDataBase >= SrtUserDataUse::TrackedWords) return Arg(handle, 0, low);
        origin = static_cast<std::int32_t>(reg - _program.userDataBase);
        low = _runtime.userData[reg - _program.userDataBase];
        _runtime.userDataUse->asBase |= std::uint64_t {1} << origin;
        return true;
    };
    if (!baseWord() || !Arg(handle, 1, high) || !Arg(inst, 1, offset)) {
        return false;
    }
    const auto base = ((high << 32u) | static_cast<std::uint32_t>(low)) & AddressMask;
    const auto immediate = static_cast<std::int64_t>(static_cast<std::int32_t>(mem.offset));
    std::uint64_t address = 0;
    if (inst.Opcode() == IrOpcode::ReadConstBuffer) {
        std::uint64_t records = 0;
        std::uint64_t word3 = 0;
        if (handle.ArgumentCount() != 4u || !Arg(handle, 2, records) || !Arg(handle, 3, word3)) {
            return false;
        }
        if (immediate < 0) {
            return false;
        }
        const auto byteOffset = static_cast<std::uint64_t>(immediate) + static_cast<std::uint32_t>(offset);
        const auto aligned = byteOffset & ~std::uint64_t {3};
        const auto stride = (static_cast<std::uint32_t>(high) >> 16u) & 0x3fffu;
        const auto size = stride == 0u ? static_cast<std::uint64_t>(static_cast<std::uint32_t>(records)) : static_cast<std::uint64_t>(stride) * static_cast<std::uint32_t>(records);
        if (aligned > size || size - aligned < sizeof(std::uint32_t)) {
            result = 0;
            return true;
        }
        address = ((base & ~std::uint64_t {3}) + byteOffset) & ~std::uint64_t {3};
    } else {
        const auto relative = (immediate & ~std::int64_t {3}) + static_cast<std::int64_t>(static_cast<std::uint32_t>(offset) & ~3u);
        if (!AddSignedAddress(base & ~std::uint64_t {3}, relative, address)) {
            return false;
        }
    }
    if (_nullRoots != nullptr && base == 0 && !LoadedFromMemory(*handle.Value())) {
        ++*_nullRoots;
        result = 0;
        return true;
    }
    if (_runtime.userDataUse != nullptr) _runtime.userDataUse->reads.emplace_back(address, origin);
    if (_inaccessible != nullptr && _runtime.accessible != nullptr && !_runtime.accessible(_runtime.userContext, address, sizeof(std::uint32_t)) && LoadedFromMemory(*handle.Value())) {
        *_inaccessible = {inst.Value(), address};
        return false;
    }
    if (auto* trace = _runtime.readTrace; trace != nullptr) {
        if (inst.Value() == trace->leaf) trace->leaves.emplace_back(trace->leafSlot, address);
        else trace->otherReads.push_back(address);
    }
    std::uint32_t word = 0;
    if (_runtime.readMemory != nullptr) {
        if (!_runtime.readMemory(_runtime.userContext, address, &word)) {
            return false;
        }
    } else {
        std::memcpy(&word, reinterpret_cast<const void*>(address), sizeof(word));
    }
    result = word;
    return true;
}

namespace {

std::optional<std::size_t> evaluatedArguments(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::BitCastU32F32:
        case IrOpcode::BitCastF32U32:
        case IrOpcode::ConvertF32U32:
        case IrOpcode::ConvertU32F32:
        case IrOpcode::FPTrunc32:
        case IrOpcode::FPIsNan32:
        case IrOpcode::BitwiseNot32:
        case IrOpcode::BitCount32:
        case IrOpcode::FindILsb32:
        case IrOpcode::FindUMsb32:
        case IrOpcode::LogicalNot: return 1u;
        case IrOpcode::CompositeConstructU64:
        case IrOpcode::IAdd32:
        case IrOpcode::IAdd64:
        case IrOpcode::ISub32:
        case IrOpcode::ISub64:
        case IrOpcode::IMul32:
        case IrOpcode::IMul64:
        case IrOpcode::SMin32:
        case IrOpcode::UMin32:
        case IrOpcode::FPMul32:
        case IrOpcode::FPOrdLessThanEqual32:
        case IrOpcode::FPOrdGreaterThanEqual32:
        case IrOpcode::FPUnordLessThanEqual32:
        case IrOpcode::FPUnordGreaterThanEqual32:
        case IrOpcode::BitwiseAnd32:
        case IrOpcode::BitwiseAnd64:
        case IrOpcode::BitwiseOr32:
        case IrOpcode::BitwiseXor32:
        case IrOpcode::ShiftLeftLogical32:
        case IrOpcode::ShiftLeftLogical64:
        case IrOpcode::ShiftRightLogical32:
        case IrOpcode::ShiftRightLogical64:
        case IrOpcode::ShiftRightArithmetic32:
        case IrOpcode::ShiftRightArithmetic64:
        case IrOpcode::IEqual32:
        case IrOpcode::INotEqual32:
        case IrOpcode::SLessThan32:
        case IrOpcode::ULessThan32:
        case IrOpcode::UGreaterThan32:
        case IrOpcode::LogicalAnd:
        case IrOpcode::LogicalOr:
        case IrOpcode::LogicalXor: return 2u;
        case IrOpcode::BitFieldUExtract:
        case IrOpcode::BitFieldSExtract:
        case IrOpcode::SelectU32:
        case IrOpcode::SelectU1:
        case IrOpcode::SelectF32: return 3u;
        case IrOpcode::BitFieldInsert: return 4u;
        default: return std::nullopt;
    }
}

std::optional<bool> evaluableAlone(const IrResourcePlan& program, const IrValue& value) {
    if (value.HasImmediate()) {
        switch (value.Type()) {
            case IrType::Bool:
            case IrType::U8:
            case IrType::U16:
            case IrType::U32:
            case IrType::U64:
            case IrType::F32: return true;
            default: return false;
        }
    }
    switch (value.Opcode()) {
        case IrOpcode::GetUserData:
        case IrOpcode::GetShaderBase: return true;
        case IrOpcode::ReadFirstLane: return value.ArgumentCount() < 2 ? std::optional(false) : std::nullopt;
        case IrOpcode::ReadConst: {
            const auto* slot = value.ArgumentCount() < 2 || value.Argument(1) == nullptr ? nullptr : value.Argument(1)->Resolve();
            if (slot == nullptr || !slot->HasImmediate() || slot->Type() != IrType::U32 || slot->ImmediateU32() >= program.srtReads.size()) return false;
            return std::nullopt;
        }
        case IrOpcode::LoadAddressU32:
        case IrOpcode::ReadConstBuffer: return IsRawRead(program, value);
        case IrOpcode::Phi: return std::nullopt;
        case IrOpcode::CompositeExtractU64:
        case IrOpcode::CompositeExtractU32x2: {
            if (value.ArgumentCount() < 2) return true;
            const auto* index = value.Argument(1) != nullptr ? value.Argument(1)->Resolve() : nullptr;
            if (index == nullptr || !index->HasImmediate() || index->Type() != IrType::U32 || index->ImmediateU32() >= 2u) return false;
            if (value.Opcode() == IrOpcode::CompositeExtractU64) return std::nullopt;
            const auto* source = value.Argument(0) != nullptr ? value.Argument(0)->Resolve() : nullptr;
            if (source == nullptr || (source->Opcode() != IrOpcode::CompositeConstructU32x2 && source->Opcode() != IrOpcode::IAddCarry32)) return false;
            return std::nullopt;
        }
        default: break;
    }
    const auto arguments = evaluatedArguments(value.Opcode());
    if (!arguments) return false;
    if (value.ArgumentCount() < *arguments) return true;
    return std::nullopt;
}

}

template<typename TNode>
bool BasicEvaluator<TNode>::MayEvaluate(const IrResourcePlan& program, IrValue* value, const IrValue* activeMask) {
    std::unordered_set<const IrValue*> firstLanes;
    return MayEvaluate(program, value, activeMask, firstLanes);
}

template<typename TNode>
bool BasicEvaluator<TNode>::MayEvaluate(const IrResourcePlan& program, IrValue* value, const IrValue* activeMask, std::unordered_set<const IrValue*>& firstLanes) {
    std::unordered_map<const IrValue*, bool> known;
    const auto takesActiveArm = [activeMask](const IrValue* current) {
        return activeMask != nullptr && IsRuntimeSelect(current->Opcode()) && current->ArgumentCount() == 3 && ActiveMaskImplies(activeMask, current->Argument(0));
    };
    const auto requirements = [&program, &takesActiveArm](IrValue* current) {
        std::vector<IrValue*> required;
        if (takesActiveArm(current)) {
            required.push_back(current->Argument(1) != nullptr ? current->Argument(1)->Resolve() : nullptr);
            return required;
        }
        if (current->Opcode() == IrOpcode::ReadConst) {
            required.push_back(program.srtReads[current->Argument(1)->Resolve()->ImmediateU32()].value);
            return required;
        }
        if (current->IsPhi()) {
            required.push_back(ResolveInvariantPhi(program, current));
            return required;
        }
        const auto resolved = [](IrValue* value) { return value != nullptr ? value->Resolve() : nullptr; };
        if (current->Opcode() == IrOpcode::CompositeExtractU64) {
            required.push_back(resolved(current->Argument(0)));
            return required;
        }
        if (current->Opcode() == IrOpcode::CompositeExtractU32x2) {
            auto* source = current->Argument(0)->Resolve();
            if (source->Opcode() == IrOpcode::CompositeConstructU32x2) {
                required.push_back(resolved(source->Argument(current->Argument(1)->Resolve()->ImmediateU32())));
            } else {
                required.push_back(resolved(source->Argument(0)));
                required.push_back(resolved(source->Argument(1)));
            }
            return required;
        }
        const auto arguments = *evaluatedArguments(current->Opcode());
        for (std::size_t index = 0; index < arguments; ++index) {
            auto* argument = current->Argument(index);
            required.push_back(argument != nullptr ? argument->Resolve() : nullptr);
        }
        return required;
    };
    std::unordered_set<const IrValue*> expanding;
    std::vector<std::pair<IrValue*, bool>> pending{{value->Resolve(), false}};
    while (!pending.empty()) {
        const auto [current, expanded] = pending.back();
        pending.pop_back();
        if (known.contains(current)) continue;
        if (current->Opcode() == IrOpcode::ReadFirstLane && current->ArgumentCount() >= 2) {
            auto* source = current->Argument(0) != nullptr ? current->Argument(0)->Resolve() : nullptr;
            const auto* mask = current->Argument(1) != nullptr ? current->Argument(1)->Resolve() : nullptr;
            if (!firstLanes.insert(current).second) {
                known.emplace(current, false);
                continue;
            }
            known.emplace(current, source != nullptr && MayEvaluate(program, source, mask, firstLanes));
            firstLanes.erase(current);
            continue;
        }
        if (const auto alone = takesActiveArm(current) ? std::nullopt : evaluableAlone(program, *current)) {
            known.emplace(current, *alone);
            continue;
        }
        if (!expanded && !expanding.insert(current).second) continue;
        const auto required = requirements(current);
        if (!expanded) {
            pending.emplace_back(current, true);
            for (auto* argument : required) {
                if (argument != nullptr) pending.emplace_back(argument, false);
            }
            continue;
        }
        known.emplace(current, std::ranges::all_of(required, [&known](const IrValue* argument) {
            const auto found = argument != nullptr ? known.find(argument) : known.end();
            return found != known.end() && found->second;
        }));
    }
    return known.at(value->Resolve());
}

template<typename TNode>
bool BasicEvaluator<TNode>::MayDecide(const IrResourcePlan& program, IrValue* value) {
    if (value == nullptr) return false;
    if (MayEvaluate(program, value)) return true;
    value = value->Resolve();
    const auto decides = [&program, value](std::size_t index) { return value->Argument(index) != nullptr && MayDecide(program, value->Argument(index)); };
    switch (value->Opcode()) {
        case IrOpcode::LogicalNot: return value->ArgumentCount() == 1u && decides(0);
        case IrOpcode::LogicalAnd:
        case IrOpcode::LogicalOr: return value->ArgumentCount() == 2u && (decides(0) || decides(1));
        default: return false;
    }
}

template<typename TNode>
bool BasicEvaluator<TNode>::EvaluateInst(const TNode& inst, std::uint64_t& result) {
    std::uint64_t a = 0;
    std::uint64_t b = 0;
    std::uint64_t c = 0;
    const auto binary = [&]() { return Arg(inst, 0, a) && Arg(inst, 1, b); };
    const auto ternary = [&]() { return Arg(inst, 0, a) && Arg(inst, 1, b) && Arg(inst, 2, c); };
    switch (inst.Opcode()) {
        case IrOpcode::GetUserData: {
            const auto reg = RegIndex(static_cast<ScalarReg>(inst.Argument(0).RegisterIndex()));
            if (reg < _program.userDataBase || reg - _program.userDataBase >= _runtime.userData.size()) {
                return false;
            }
            if (_runtime.userDataUse != nullptr) _runtime.userDataUse->asValue |= reg - _program.userDataBase < SrtUserDataUse::TrackedWords ? std::uint64_t {1} << (reg - _program.userDataBase) : ~std::uint64_t {0};
            result = _runtime.userData[reg - _program.userDataBase];
            return true;
        }
        case IrOpcode::GetShaderBase: result = _runtime.shaderBase; return true;
        case IrOpcode::Phi: return EvaluatePhi(inst, result);
        case IrOpcode::ReadFirstLane: {
            for (const auto* outer = this; outer != nullptr; outer = outer->_outer) {
                if (outer->_firstLane == inst.Value()) return false;
            }
            const TNode mask = inst.Argument(1);
            BasicEvaluator active(_program, _runtime, _cleanFlatSlots, _cleanEvaluator, &mask);
            active._outer = this;
            active._firstLane = inst.Value();
            active.ReportInaccessibleReads(_inaccessible);
            active.ReportNullRootReads(_nullRoots);
            return active.EvaluateWide(inst.Argument(0), result);
        }
        case IrOpcode::BitCastU32F32:
        case IrOpcode::BitCastF32U32: return Arg(inst, 0, result);
        case IrOpcode::CompositeExtractU64:
        case IrOpcode::CompositeExtractU32x2: return EvaluateExtract(inst, result);
        case IrOpcode::CompositeConstructU64:
            if (!binary()) {
                return false;
            }
            result = static_cast<std::uint32_t>(a) | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(b)) << 32u);
            return true;
        case IrOpcode::ReadConst: {
            const TNode slotValue = inst.Argument(1);
            const auto slot = static_cast<std::uint32_t>(slotValue.ImmediateBits());
            if (!slotValue.HasImmediate() || slotValue.Type() != IrType::U32 || slot >= _program.srtReads.size()) {
                return false;
            }
            if (slot < _cleanFlatSlots.size() && _cleanFlatSlots[slot] != 0u && _cleanEvaluator != nullptr) {
                return _cleanEvaluator->EvaluateWide(TNode::SrtRead(_program, slot), result);
            }
            return EvaluateWide(TNode::SrtRead(_program, slot), result);
        }
        case IrOpcode::LoadAddressU32:
        case IrOpcode::ReadConstBuffer:
            if (IsRawRead(_program, inst.Opcode(), inst.template Flags<MemoryFlags>().index)) {
                return EvaluateRawRead(inst, result);
            }
            break;
        case IrOpcode::IAdd32:
            if (binary()) {
                result = static_cast<std::uint32_t>(a + b);
                return true;
            }
            return false;
        case IrOpcode::IAdd64:
            if (binary()) {
                result = a + b;
                return true;
            }
            return false;
        case IrOpcode::ISub32:
            if (binary()) {
                result = static_cast<std::uint32_t>(a - b);
                return true;
            }
            return false;
        case IrOpcode::ISub64:
            if (binary()) {
                result = a - b;
                return true;
            }
            return false;
        case IrOpcode::IMul32:
            if (binary()) {
                result = static_cast<std::uint32_t>(a * b);
                return true;
            }
            return false;
        case IrOpcode::IMul64:
            if (binary()) {
                result = a * b;
                return true;
            }
            return false;
        case IrOpcode::SMin32:
            if (binary()) {
                result = static_cast<std::uint32_t>(std::min(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)), std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(b))));
                return true;
            }
            return false;
        case IrOpcode::UMin32:
            if (binary()) {
                result = std::min(static_cast<std::uint32_t>(a), static_cast<std::uint32_t>(b));
                return true;
            }
            return false;
        case IrOpcode::ConvertF32U32:
            if (Arg(inst, 0, a)) {
                result = Float32Bits(static_cast<float>(static_cast<std::uint32_t>(a)));
                return true;
            }
            return false;
        case IrOpcode::ConvertU32F32:
            if (Arg(inst, 0, a)) {
                const auto value = Float32(a);
                if (!std::isfinite(value) || value < 0.0f || static_cast<double>(value) > 4294967295.0) {
                    return false;
                }
                result = static_cast<std::uint32_t>(value);
                return true;
            }
            return false;
        case IrOpcode::FPMul32:
            if (binary()) {
                result = Float32Bits(Float32(a) * Float32(b));
                return true;
            }
            return false;
        case IrOpcode::FPTrunc32:
            if (Arg(inst, 0, a)) {
                result = Float32Bits(std::trunc(Float32(a)));
                return true;
            }
            return false;
        case IrOpcode::FPIsNan32:
            if (Arg(inst, 0, a)) {
                result = std::isnan(Float32(a)) ? 1u : 0u;
                return true;
            }
            return false;
        case IrOpcode::FPOrdLessThanEqual32:
            if (binary()) {
                result = Float32(a) <= Float32(b) ? 1u : 0u;
                return true;
            }
            return false;
        case IrOpcode::FPOrdGreaterThanEqual32:
            if (binary()) {
                result = Float32(a) >= Float32(b) ? 1u : 0u;
                return true;
            }
            return false;
        case IrOpcode::FPUnordLessThanEqual32:
            if (binary()) {
                result = !(Float32(a) > Float32(b)) ? 1u : 0u;
                return true;
            }
            return false;
        case IrOpcode::FPUnordGreaterThanEqual32:
            if (binary()) {
                result = !(Float32(a) < Float32(b)) ? 1u : 0u;
                return true;
            }
            return false;
        case IrOpcode::BitwiseAnd32:
            if (binary()) {
                result = static_cast<std::uint32_t>(a & b);
                return true;
            }
            return false;
        case IrOpcode::BitwiseAnd64:
            if (binary()) {
                result = a & b;
                return true;
            }
            return false;
        case IrOpcode::BitwiseOr32:
            if (binary()) {
                result = static_cast<std::uint32_t>(a | b);
                return true;
            }
            return false;
        case IrOpcode::BitwiseXor32:
            if (binary()) {
                result = static_cast<std::uint32_t>(a ^ b);
                return true;
            }
            return false;
        case IrOpcode::BitwiseNot32:
            if (Arg(inst, 0, a)) {
                result = ~static_cast<std::uint32_t>(a);
                return true;
            }
            return false;
        case IrOpcode::BitCount32:
            if (Arg(inst, 0, a)) {
                result = static_cast<std::uint32_t>(std::popcount(static_cast<std::uint32_t>(a)));
                return true;
            }
            return false;
        case IrOpcode::FindILsb32:
            if (Arg(inst, 0, a)) {
                const auto val = static_cast<std::uint32_t>(a);
                result = val == 0u ? 0xffffffffu : static_cast<std::uint32_t>(std::countr_zero(val));
                return true;
            }
            return false;
        case IrOpcode::FindUMsb32:
            if (Arg(inst, 0, a)) {
                const auto val = static_cast<std::uint32_t>(a);
                result = val == 0u ? 0xffffffffu : static_cast<std::uint32_t>(31u - std::countl_zero(val));
                return true;
            }
            return false;
        case IrOpcode::ShiftLeftLogical32:
            if (binary()) {
                result = static_cast<std::uint32_t>(a) << (b & 31u);
                return true;
            }
            return false;
        case IrOpcode::ShiftLeftLogical64:
            if (binary()) {
                result = a << (b & 63u);
                return true;
            }
            return false;
        case IrOpcode::ShiftRightLogical32:
            if (binary()) {
                result = static_cast<std::uint32_t>(a) >> (b & 31u);
                return true;
            }
            return false;
        case IrOpcode::ShiftRightLogical64:
            if (binary()) {
                result = a >> (b & 63u);
                return true;
            }
            return false;
        case IrOpcode::ShiftRightArithmetic32:
            if (binary()) {
                result = static_cast<std::uint32_t>(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)) >> (b & 31u));
                return true;
            }
            return false;
        case IrOpcode::ShiftRightArithmetic64:
            if (binary()) {
                result = static_cast<std::uint64_t>(std::bit_cast<std::int64_t>(a) >> (b & 63u));
                return true;
            }
            return false;
        case IrOpcode::BitFieldUExtract:
            if (ternary()) {
                const auto offset = static_cast<std::uint32_t>(b);
                const auto width = static_cast<std::uint32_t>(c);
                if (offset > 32u || width > 32u - offset) {
                    return false;
                }
                const auto mask = width == 32u ? 0xffffffffu : width == 0u ? 0u : (std::uint32_t {1} << width) - 1u;
                result = width == 0u ? 0u : (static_cast<std::uint32_t>(a) >> offset) & mask;
                return true;
            }
            return false;
        case IrOpcode::BitFieldSExtract:
            if (ternary()) {
                const auto offset = static_cast<std::uint32_t>(b);
                const auto width = static_cast<std::uint32_t>(c);
                if (offset > 32u || width > 32u - offset) {
                    return false;
                }
                if (width == 0u) {
                    result = 0;
                    return true;
                }
                const auto mask = width == 32u ? 0xffffffffu : (std::uint32_t {1} << width) - 1u;
                auto bits = (static_cast<std::uint32_t>(a) >> offset) & mask;
                if (width < 32u && (bits & (std::uint32_t {1} << (width - 1u))) != 0u) {
                    bits |= ~mask;
                }
                result = bits;
                return true;
            }
            return false;
        case IrOpcode::BitFieldInsert: {
            std::uint64_t d = 0;
            if (!ternary() || !Arg(inst, 3, d)) {
                return false;
            }
            const auto offset = static_cast<std::uint32_t>(c);
            const auto width = static_cast<std::uint32_t>(d);
            if (offset > 32u || width > 32u - offset) {
                return false;
            }
            if (width == 0u) {
                result = static_cast<std::uint32_t>(a);
                return true;
            }
            const auto mask = width == 32u ? 0xffffffffu : ((std::uint32_t {1} << width) - 1u) << offset;
            result = (static_cast<std::uint32_t>(a) & ~mask) | ((static_cast<std::uint32_t>(b) << offset) & mask);
            return true;
        }
        case IrOpcode::SelectU32:
        case IrOpcode::SelectU1:
        case IrOpcode::SelectF32:
            if (ternary()) {
                result = a != 0u ? b : c;
                return true;
            }
            return false;
        case IrOpcode::IEqual32:
            if (binary()) {
                result = static_cast<std::uint32_t>(a) == static_cast<std::uint32_t>(b) ? 1u : 0u;
                return true;
            }
            return false;
        case IrOpcode::INotEqual32:
            if (binary()) {
                result = static_cast<std::uint32_t>(a) != static_cast<std::uint32_t>(b) ? 1u : 0u;
                return true;
            }
            return false;
        case IrOpcode::SLessThan32:
            if (binary()) {
                result = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)) < std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(b)) ? 1u : 0u;
                return true;
            }
            return false;
        case IrOpcode::ULessThan32:
            if (binary()) {
                result = static_cast<std::uint32_t>(a) < static_cast<std::uint32_t>(b) ? 1u : 0u;
                return true;
            }
            return false;
        case IrOpcode::UGreaterThan32:
            if (binary()) {
                result = static_cast<std::uint32_t>(a) > static_cast<std::uint32_t>(b) ? 1u : 0u;
                return true;
            }
            return false;
        case IrOpcode::LogicalAnd:
        case IrOpcode::LogicalOr: {
            const bool decisive = inst.Opcode() == IrOpcode::LogicalOr;
            const bool first = Arg(inst, 0, a);
            if (first && (a != 0u) == decisive) {
                result = decisive ? 1u : 0u;
                return true;
            }
            const bool second = Arg(inst, 1, b);
            if (second && (b != 0u) == decisive) {
                result = decisive ? 1u : 0u;
                return true;
            }
            if (!first || !second) return false;
            result = decisive ? 0u : 1u;
            return true;
        }
        case IrOpcode::LogicalXor:
            if (binary()) {
                result = (a != 0u) != (b != 0u) ? 1u : 0u;
                return true;
            }
            return false;
        case IrOpcode::LogicalNot:
            if (Arg(inst, 0, a)) {
                result = a == 0u ? 1u : 0u;
                return true;
            }
            return false;
        case IrOpcode::UndefU1:
        case IrOpcode::UndefU8:
        case IrOpcode::UndefU16:
        case IrOpcode::UndefU32:
        case IrOpcode::UndefU64: return false;
        default: break;
    }
    return false;
}

template class BasicEvaluator<IrNode>;
template class BasicEvaluator<CompactNode>;

}
