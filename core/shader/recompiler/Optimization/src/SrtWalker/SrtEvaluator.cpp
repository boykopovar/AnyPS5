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
    if (_masked && IsRuntimeSelect(inst.Opcode()) && inst.ArgumentCount() == 3 && inst.Argument(0) == _activeMask) {
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
    if (!Arg(handle, 0, low) || !Arg(handle, 1, high) || !Arg(inst, 1, offset)) {
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
            return false;
        }
        address = ((base & ~std::uint64_t {3}) + byteOffset) & ~std::uint64_t {3};
    } else {
        const auto relative = (immediate & ~std::int64_t {3}) + static_cast<std::int64_t>(static_cast<std::uint32_t>(offset) & ~3u);
        if (!AddSignedAddress(base & ~std::uint64_t {3}, relative, address)) {
            return false;
        }
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
            result = _runtime.userData[reg - _program.userDataBase];
            return true;
        }
        case IrOpcode::GetShaderBase: result = _runtime.shaderBase; return true;
        case IrOpcode::Phi: return EvaluatePhi(inst, result);
        case IrOpcode::ReadFirstLane: {
            const TNode mask = inst.Argument(1);
            BasicEvaluator active(_program, _runtime, _cleanFlatSlots, _cleanEvaluator, &mask);
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
            if (binary()) {
                result = (a != 0u) && (b != 0u) ? 1u : 0u;
                return true;
            }
            return false;
        case IrOpcode::LogicalOr:
            if (binary()) {
                result = (a != 0u) || (b != 0u) ? 1u : 0u;
                return true;
            }
            return false;
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
