#include "Optimization/SrtWalker/SrtEvaluator.hpp"
#include "Optimization/SrtWalker/SrtAddressArithmetic.hpp"
#include "Optimization/SrtWalker/SrtInstructionPredicates.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace ShaderRecompiler::Detail {

bool Evaluator::Evaluate(IrValue* value, std::uint32_t& result) {
    std::uint64_t wide = 0;
    if (!EvaluateWide(value, wide)) {
        return false;
    }
    result = static_cast<std::uint32_t>(wide);
    return true;
}

bool Evaluator::EvaluateWide(IrValue* raw, std::uint64_t& result) {
    IrValue* value = raw->Resolve();
    if (value->HasImmediate()) {
        switch (value->Type()) {
            case IrType::Bool: result = value->ImmediateBool() ? 1u : 0u; return true;
            case IrType::U8: result = value->ImmediateU8(); return true;
            case IrType::U16: result = value->ImmediateU16(); return true;
            case IrType::U32: result = value->ImmediateU32(); return true;
            case IrType::U64: result = value->ImmediateU64(); return true;
            case IrType::F32: result = Float32Bits(value->ImmediateF32()); return true;
            default: return false;
        }
    }
    if (value->Opcode() == IrOpcode::Void) {
        return false;
    }
    IrValue* inst = value;
    if (_activeMask != nullptr && IsRuntimeSelect(inst->Opcode()) && inst->ArgumentCount() == 3 && inst->Argument(0)->Resolve() == _activeMask) {
        return EvaluateWide(inst->Argument(1), result);
    }
    if (const auto found = _cache.find(inst); found != _cache.end()) {
        result = found->second;
        return true;
    }
    if (std::find(_visiting.begin(), _visiting.end(), inst) != _visiting.end()) {
        return false;
    }
    _visiting.push_back(inst);
    std::uint64_t out = 0;
    const bool evaluated = EvaluateInst(*inst, out);
    _visiting.pop_back();
    if (!evaluated) {
        static const bool debug = std::getenv("APS5_SRT_DEBUG") != nullptr;
        if (debug) std::fprintf(stderr, "[srt] cannot evaluate %s (%zu arguments)\n", std::string(IrOpcodeName(inst->Opcode())).c_str(), inst->ArgumentCount());
        return false;
    }
    _cache.emplace(inst, out);
    result = out;
    return true;
}

float Evaluator::Float32(std::uint64_t bits) { return std::bit_cast<float>(static_cast<std::uint32_t>(bits)); }

std::uint64_t Evaluator::Float32Bits(float value) { return std::bit_cast<std::uint32_t>(value); }

bool Evaluator::Arg(IrValue& inst, std::size_t index, std::uint64_t& result) { return EvaluateWide(inst.Argument(index), result); }

bool Evaluator::EvaluatePhi(IrValue& inst, std::uint64_t& result) {
    IrValue* value = ResolveInvariantPhi(_program, &inst);
    return value != nullptr && EvaluateWide(value, result);
}

bool Evaluator::EvaluateExtract(IrValue& inst, std::uint64_t& result) {
    IrValue* index = inst.Argument(1)->Resolve();
    if (!index->HasImmediate() || index->Type() != IrType::U32) {
        return false;
    }
    const auto component = index->ImmediateU32();
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
    IrValue* source = inst.Argument(0)->Resolve();
    if (source->Opcode() == IrOpcode::Void) {
        return false;
    }
    if (source->Opcode() == IrOpcode::CompositeConstructU32x2) {
        return EvaluateWide(source->Argument(component), result);
    }
    if (source->Opcode() == IrOpcode::IAddCarry32) {
        std::uint64_t lhs = 0;
        std::uint64_t rhs = 0;
        if (!Arg(*source, 0, lhs) || !Arg(*source, 1, rhs)) {
            return false;
        }
        const auto sum = static_cast<std::uint64_t>(static_cast<std::uint32_t>(lhs)) + static_cast<std::uint32_t>(rhs);
        result = component == 0u ? static_cast<std::uint32_t>(sum) : static_cast<std::uint32_t>(sum >> 32u);
        return true;
    }
    return false;
}

bool Evaluator::EvaluateRawRead(IrValue& inst, std::uint64_t& result) {
    const auto flags = inst.Flags<MemoryFlags>();
    if (flags.index >= _program.memoryInfo.size()) {
        return false;
    }
    const auto& mem = _program.memoryInfo[flags.index];
    IrValue* handle = inst.Argument(0)->Resolve();
    if (handle->Opcode() == IrOpcode::Void) {
        return false;
    }
    std::uint64_t low = 0;
    std::uint64_t high = 0;
    std::uint64_t offset = 0;
    if (!Arg(*handle, 0, low) || !Arg(*handle, 1, high) || !Arg(inst, 1, offset)) {
        return false;
    }
    const auto base = ((high << 32u) | static_cast<std::uint32_t>(low)) & AddressMask;
    const auto immediate = static_cast<std::int64_t>(static_cast<std::int32_t>(mem.offset));
    std::uint64_t address = 0;
    if (inst.Opcode() == IrOpcode::ReadConstBuffer) {
        std::uint64_t records = 0;
        std::uint64_t word3 = 0;
        if (handle->ArgumentCount() != 4u || !Arg(*handle, 2, records) || !Arg(*handle, 3, word3)) {
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

bool Evaluator::EvaluateInst(IrValue& inst, std::uint64_t& result) {
    std::uint64_t a = 0;
    std::uint64_t b = 0;
    std::uint64_t c = 0;
    const auto binary = [&]() { return Arg(inst, 0, a) && Arg(inst, 1, b); };
    const auto ternary = [&]() { return Arg(inst, 0, a) && Arg(inst, 1, b) && Arg(inst, 2, c); };
    switch (inst.Opcode()) {
        case IrOpcode::GetUserData: {
            const auto reg = RegIndex(static_cast<ScalarReg>(inst.Argument(0)->Register().index));
            if (reg < _program.userDataBase || reg - _program.userDataBase >= _runtime.userData.size()) {
                return false;
            }
            result = _runtime.userData[reg - _program.userDataBase];
            return true;
        }
        case IrOpcode::GetShaderBase: result = _runtime.shaderBase; return true;
        case IrOpcode::Phi: return EvaluatePhi(inst, result);
        case IrOpcode::ReadFirstLane: {
            Evaluator active(_program, _runtime, _cleanFlatSlots, _cleanEvaluator, inst.Argument(1));
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
            IrValue* slot = inst.Argument(1)->Resolve();
            if (!slot->HasImmediate() || slot->Type() != IrType::U32 || slot->ImmediateU32() >= _program.srtReads.size()) {
                return false;
            }
            if (slot->ImmediateU32() < _cleanFlatSlots.size() && _cleanFlatSlots[slot->ImmediateU32()] != 0u && _cleanEvaluator != nullptr) {
                return _cleanEvaluator->EvaluateWide(_program.srtReads[slot->ImmediateU32()].value, result);
            }
            return EvaluateWide(_program.srtReads[slot->ImmediateU32()].value, result);
        }
        case IrOpcode::LoadAddressU32:
        case IrOpcode::ReadConstBuffer:
            if (IsRawRead(_program, inst)) {
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

}
