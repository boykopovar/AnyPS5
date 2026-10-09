#include "SpirvBackend/SpirvAnalysis.hpp"
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace ShaderRecompiler {

namespace {

bool UniformSource(const IrValue& value) {
    switch (value.Opcode()) {
    case IrOpcode::Ballot:
    case IrOpcode::ReadFirstLane:
    case IrOpcode::ReadLane:
    case IrOpcode::GetUserData:
    case IrOpcode::GetShaderBase:
    case IrOpcode::ReadConst:
    case IrOpcode::ReadConstBuffer:
    case IrOpcode::MeshDrawParameter:
    case IrOpcode::MeshArgument:
    case IrOpcode::GetSrtResource:
    case IrOpcode::GetBufferResource:
    case IrOpcode::GetAddressResource:
    case IrOpcode::GetScratchResource:
    case IrOpcode::GetImageResource:
    case IrOpcode::GetSamplerResource:
        return true;
    default:
        return false;
    }
}

bool LaneSource(const IrProgram& program, const IrValue& value) {
    switch (value.Opcode()) {
    case IrOpcode::LaneId:
    case IrOpcode::GetAttribute:
    case IrOpcode::GetInterpolationParameter:
    case IrOpcode::GetInterpolationParameterF16:
    case IrOpcode::GetTessellationAttribute:
    case IrOpcode::DppMoveU32:
    case IrOpcode::DppUpdateU32:
    case IrOpcode::Permlane16U32:
    case IrOpcode::PermuteU32:
    case IrOpcode::BpermuteU32:
    case IrOpcode::SwizzleU32:
    case IrOpcode::WriteLane:
    case IrOpcode::DataAppend:
    case IrOpcode::DataConsume:
        return true;
    case IrOpcode::GetBuiltin: {
        const auto* kind = value.Argument(0)->Resolve();
        return kind == nullptr || !kind->HasImmediate() || static_cast<StageInputKind>(kind->ImmediateU32()) != StageInputKind::WorkgroupId;
    }
    default:
        break;
    }
    const auto address = AddressOpcodeInfoOf(value.Opcode()).access;
    if (address == AddressAccess::Read) {
        const auto index = value.Flags<MemoryFlags>().index;
        const auto& memory = program.Resources().memoryInfo;
        if (index < memory.size() && memory[index].kind == ResourceKind::ScalarAddress) return false;
    }
    return address != AddressAccess::None || BufferAccessOf(value.Opcode()) != BufferAccess::None || SharedAccessOf(value.Opcode()) != SharedAccess::None || ImageOpcodeInfoOf(value.Opcode()).access != ImageAccess::None;
}

}

namespace {

class AddressBound {
public:
    std::optional<std::uint64_t> Of(const IrValue* value) {
        if (value == nullptr) return std::nullopt;
        value = value->Resolve();
        if (value == nullptr) return std::nullopt;
        if (value->HasImmediate()) return static_cast<std::uint64_t>(value->ImmediateU32());
        if (const auto known = bounds.find(value); known != bounds.end()) return known->second;
        if (!visiting.insert(value).second || visiting.size() > MaxDepth) return std::nullopt;
        const auto result = compute(*value);
        visiting.erase(value);
        bounds.emplace(value, result);
        return result;
    }

private:
    static constexpr std::size_t MaxDepth = 256;
    static constexpr std::uint64_t Max32 = 0xffffffffull;

    static std::optional<std::uint64_t> fits(std::uint64_t value) {
        return value <= Max32 ? std::optional<std::uint64_t>(value) : std::nullopt;
    }
    static std::uint64_t ones(std::uint64_t value) {
        std::uint64_t mask = 0;
        while (mask < value) mask = (mask << 1u) | 1u;
        return mask;
    }

    std::optional<std::uint64_t> compute(const IrValue& value) {
        const auto argument = [&](std::size_t index) { return index < value.ArgumentCount() ? Of(value.Argument(index)) : std::nullopt; };
        switch (value.Opcode()) {
        case IrOpcode::LaneId:
            return 63u;
        case IrOpcode::IAdd32: {
            const auto a = argument(0), b = argument(1);
            if (!a || !b) return std::nullopt;
            return fits(*a + *b);
        }
        case IrOpcode::IMul32: {
            const auto a = argument(0), b = argument(1);
            if (!a || !b || (*b != 0 && *a > Max32 / *b)) return std::nullopt;
            return *a * *b;
        }
        case IrOpcode::ShiftLeftLogical32:
        case IrOpcode::IShiftLeft32: {
            const auto a = argument(0), b = argument(1);
            if (!a || !b || *b >= 32u) return std::nullopt;
            return fits(*a << *b);
        }
        case IrOpcode::ShiftRightLogical32:
        case IrOpcode::IShiftRightLogical32:
            return argument(0);
        case IrOpcode::BitwiseAnd32:
        case IrOpcode::IAnd32: {
            const auto a = argument(0), b = argument(1);
            if (a && b) return std::min(*a, *b);
            return a ? a : b;
        }
        case IrOpcode::BitwiseOr32:
        case IrOpcode::IOr32:
        case IrOpcode::BitwiseXor32:
        case IrOpcode::IXor32: {
            const auto a = argument(0), b = argument(1);
            if (!a || !b) return std::nullopt;
            return ones(std::max(*a, *b));
        }
        case IrOpcode::UMin32: {
            const auto a = argument(0), b = argument(1);
            if (a && b) return std::min(*a, *b);
            return a ? a : b;
        }
        case IrOpcode::UMax32: {
            const auto a = argument(0), b = argument(1);
            if (!a || !b) return std::nullopt;
            return std::max(*a, *b);
        }
        case IrOpcode::SelectU32: {
            const auto a = argument(1), b = argument(2);
            if (!a || !b) return std::nullopt;
            return std::max(*a, *b);
        }
        case IrOpcode::BitFieldUExtract: {
            const auto width = argument(2);
            if (width && *width < 32u) return (1ull << *width) - 1u;
            return argument(0);
        }
        case IrOpcode::Phi: {
            std::uint64_t result = 0;
            for (std::size_t index = 0; index < value.ArgumentCount(); ++index) {
                const auto incoming = Of(value.Argument(index));
                if (!incoming) return std::nullopt;
                result = std::max(result, *incoming);
            }
            return result;
        }
        default:
            return std::nullopt;
        }
    }

    std::unordered_map<const IrValue*, std::optional<std::uint64_t>> bounds;
    std::unordered_set<const IrValue*> visiting;
};

std::uint32_t SharedAccessDwords(IrOpcode opcode) {
    switch (opcode) {
    case IrOpcode::LoadSharedU8:
    case IrOpcode::LoadSharedU16:
    case IrOpcode::LoadSharedU32:
    case IrOpcode::WriteSharedU8:
    case IrOpcode::WriteSharedU16:
    case IrOpcode::WriteSharedU32:
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
        return 1u;
    case IrOpcode::LoadSharedU32x2:
    case IrOpcode::WriteSharedU32x2:
        return 2u;
    case IrOpcode::LoadSharedU32x3:
    case IrOpcode::WriteSharedU32x3:
        return 3u;
    case IrOpcode::LoadSharedU32x4:
    case IrOpcode::WriteSharedU32x4:
        return 4u;
    default:
        return 0u;
    }
}

}

std::uint32_t FunctionLdsDwords(const IrProgram& program) {
    AddressBound bound;
    std::uint64_t needed = 0;
    for (const IrBlock* block : program.BlockOrder()) {
        for (const IrValue* inst : block->Instructions()) {
            if (SharedAccessOf(inst->Opcode()) == SharedAccess::None) continue;
            const auto index = inst->Flags<MemoryFlags>().index;
            if (index >= program.Resources().memoryInfo.size()) return FunctionLdsDwordLimit;
            const auto& memory = program.Resources().memoryInfo[index];
            if (memory.kind != ResourceKind::Lds) continue;
            const auto dwords = SharedAccessDwords(inst->Opcode());
            if (dwords == 0u || inst->ArgumentCount() == 0) return FunctionLdsDwordLimit;
            const auto address = bound.Of(inst->Argument(0));
            if (!address || *address + memory.offset > 0xffffffffull) return FunctionLdsDwordLimit;
            needed = std::max(needed, ((*address + memory.offset) >> 2u) + dwords);
            if (needed >= FunctionLdsDwordLimit) return FunctionLdsDwordLimit;
        }
    }
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(std::max<std::uint64_t>((needed + 63u) & ~63ull, 64u), FunctionLdsDwordLimit));
}

namespace {

struct LaneAffine {
    std::uint64_t stride = 0;
    std::uint64_t constant = 0;
};

std::optional<LaneAffine> LaneAffineOf(const IrValue* value, std::uint32_t depth = 0) {
    if (value == nullptr || depth > 64u) return std::nullopt;
    value = value->Resolve();
    if (value == nullptr) return std::nullopt;
    if (value->HasImmediate()) return LaneAffine{0u, value->ImmediateU32()};
    const auto argument = [&](std::size_t index) { return index < value->ArgumentCount() ? LaneAffineOf(value->Argument(index), depth + 1u) : std::nullopt; };
    switch (value->Opcode()) {
    case IrOpcode::LaneId:
        return LaneAffine{1u, 0u};
    case IrOpcode::IAdd32: {
        const auto a = argument(0), b = argument(1);
        if (!a || !b) return std::nullopt;
        return LaneAffine{a->stride + b->stride, a->constant + b->constant};
    }
    case IrOpcode::IMul32: {
        const auto a = argument(0), b = argument(1);
        if (!a || !b) return std::nullopt;
        if (a->stride == 0u) return LaneAffine{b->stride * a->constant, b->constant * a->constant};
        if (b->stride == 0u) return LaneAffine{a->stride * b->constant, a->constant * b->constant};
        return std::nullopt;
    }
    case IrOpcode::ShiftLeftLogical32:
    case IrOpcode::IShiftLeft32: {
        const auto a = argument(0), b = argument(1);
        if (!a || !b || b->stride != 0u || b->constant >= 32u) return std::nullopt;
        return LaneAffine{a->stride << b->constant, a->constant << b->constant};
    }
    default:
        return std::nullopt;
    }
}

}

std::unordered_map<const IrValue*, std::uint32_t> FunctionLdsLaneAddresses(const IrProgram& program) {
    std::unordered_map<const IrValue*, std::uint32_t> addresses;
    if (FunctionLdsDwords(program) >= FunctionLdsDwordLimit) return {};
    std::optional<std::uint64_t> stride;
    for (const IrBlock* block : program.BlockOrder()) {
        for (const IrValue* inst : block->Instructions()) {
            if (SharedAccessOf(inst->Opcode()) == SharedAccess::None) continue;
            const auto& memory = program.Resources().memoryInfo[inst->Flags<MemoryFlags>().index];
            if (memory.kind != ResourceKind::Lds) continue;
            const auto affine = LaneAffineOf(inst->Argument(0));
            if (!affine || affine->stride % 4u != 0u || (stride && *stride != affine->stride)) return {};
            stride = affine->stride;
            if (affine->constant + memory.offset > 0xffffffffull) return {};
            addresses.emplace(inst, static_cast<std::uint32_t>(affine->constant));
        }
    }
    if (!stride || *stride == 0u) return {};
    return addresses;
}

SpirvRequirements AnalyzeProgramRequirements(const IrProgram& program) {
    SpirvRequirements requirements {};
    for (const IrBlock* block : program.BlockOrder()) {
        for (const IrValue* inst : block->Instructions()) {
            const auto addressAccess = AddressOpcodeInfoOf(inst->Opcode()).access;
            if ((BufferAccessOf(inst->Opcode()) == BufferAccess::Atomic || addressAccess == AddressAccess::Atomic) && inst->Type() == IrType::U64) {
                requirements.bufferInt64Atomics = true;
            }
            if (IsImageAtomic64Opcode(inst->Opcode())) {
                requirements.imageInt64Atomics = true;
            }
            if (IsFloat64Opcode(inst->Opcode())) {
                requirements.float64 = true;
            }
            if (addressAccess != AddressAccess::None) {
                const auto memoryIndex = inst->Flags<MemoryFlags>().index;
                if (memoryIndex >= program.Resources().memoryInfo.size()) {
                    throw std::runtime_error("address operation has invalid memory metadata");
                }
                if (program.Resources().memoryInfo.at(memoryIndex).kind == ResourceKind::Scratch) {
                    if (program.Info().scratchDwords == 0u) {
                        throw std::runtime_error("scratch operation has no per-thread storage");
                    }
                    requirements.functionScratch = true;
                }
            }
            if (BufferAccessOf(inst->Opcode()) != BufferAccess::None) {
                const auto memoryIndex = inst->Flags<MemoryFlags>().index;
                if (memoryIndex >= program.Resources().memoryInfo.size()) {
                    throw std::runtime_error("buffer operation has invalid memory metadata");
                }
                const auto& memory = program.Resources().memoryInfo.at(memoryIndex);
                requirements.coherentBuffers = requirements.coherentBuffers || (memory.coherent && !memory.gpuDescriptor);
                if (!memory.planningOnly) {
                    requirements.subgroupLocalInvocationId = requirements.subgroupLocalInvocationId || program.Resources().stage == IrShaderStage::Compute;
                }
            }
            const auto sharedAccess = SharedAccessOf(inst->Opcode());
            if (sharedAccess == SharedAccess::Atomic && inst->Type() == IrType::U64) {
                requirements.sharedInt64Atomics = true;
            }
            if (sharedAccess != SharedAccess::None) {
                const auto index = inst->Flags<MemoryFlags>().index;
                if (index >= program.Resources().memoryInfo.size()) {
                    throw std::runtime_error("shared operation has invalid memory metadata");
                }
                const auto kind = program.Resources().memoryInfo.at(index).kind;
                if (kind != ResourceKind::Lds && kind != ResourceKind::Gds) {
                    throw std::runtime_error("shared operation has invalid resource kind");
                }
                if (program.Resources().stage != IrShaderStage::Compute && program.Resources().stage != IrShaderStage::Mesh && kind == ResourceKind::Lds) {
                    requirements.functionLds = true;
                } else if (sharedAccess == SharedAccess::Atomic && inst->Type() == IrType::U64 && kind == ResourceKind::Lds) {
                    requirements.ldsLock = true;
                    requirements.subgroupBallot = true;
                }
                if (sharedAccess == SharedAccess::Append || sharedAccess == SharedAccess::Consume) {
                    requirements.subgroupBallot = true;
                    requirements.subgroupShuffle = true;
                    requirements.subgroupLocalInvocationId = true;
                }
            }
            switch (inst->Opcode()) {
            case IrOpcode::Ballot:
                requirements.subgroupBallot = true;
                break;
            case IrOpcode::DppMoveU32:
            case IrOpcode::ReadFirstLane:
            case IrOpcode::ReadLane: {
                requirements.subgroupBallot = true;
                requirements.subgroupShuffle = true;
                if (inst->Opcode() == IrOpcode::DppMoveU32) {
                    requirements.subgroupLocalInvocationId = true;
                }
                break;
            }
            case IrOpcode::DppUpdateU32:
            case IrOpcode::WriteLane: {
                requirements.subgroupBallot = true;
                requirements.subgroupLocalInvocationId = true;
                break;
            }
            case IrOpcode::Permlane16U32: {
                requirements.subgroupBallot = true;
                requirements.subgroupShuffle = true;
                requirements.subgroupLocalInvocationId = true;
                break;
            }
            case IrOpcode::SwizzleU32:
            case IrOpcode::PermuteU32:
            case IrOpcode::BpermuteU32: {
                requirements.subgroupBallot = true;
                requirements.subgroupShuffle = true;
                requirements.subgroupLocalInvocationId = true;
                break;
            }
            case IrOpcode::LaneId:
                requirements.subgroupLocalInvocationId = requirements.subgroupLocalInvocationId || program.Resources().stage != IrShaderStage::TessellationControl;
                break;
            case IrOpcode::ImageQueryLod:
                requirements.computeDerivatives = true;
                break;
            case IrOpcode::ImageGatherRaw:
                requirements.imageGatherExtended = true;
                break;
            case IrOpcode::SetAttribute: {
                const auto index = inst->Flags<ExportFlags>().index;
                if (index >= program.Metadata().exportInfo.size()) {
                    throw std::runtime_error("attribute export has invalid metadata");
                }
                if (program.Resources().stage == IrShaderStage::Pixel && program.Metadata().exportInfo.at(index).vm) {
                    requirements.pixelValidMask = true;
                }
                break;
            }
            default:
                break;
            }
        }
    }
    for (const auto& info : program.Metadata().blockInfo) {
        if (info.terminator.kind == TerminatorKind::ConditionalBranch && IsWaveMaskBranch(info.terminator.condition)) {
            requirements.subgroupBallot = true;
        }
    }
    if (requirements.functionLds) {
        requirements.functionLdsDwords = FunctionLdsDwords(program);
        requirements.functionLdsAddresses = FunctionLdsLaneAddresses(program);
        if (!requirements.functionLdsAddresses.empty()) {
            std::uint64_t needed = 0;
            for (const auto& [inst, address] : requirements.functionLdsAddresses) {
                const auto offset = program.Resources().memoryInfo[inst->Flags<MemoryFlags>().index].offset;
                needed = std::max<std::uint64_t>(needed, ((std::uint64_t{address} + offset) >> 2u) + SharedAccessDwords(inst->Opcode()));
            }
            requirements.functionLdsDwords = static_cast<std::uint32_t>(std::max<std::uint64_t>((needed + 63u) & ~63ull, 64u));
        }
    }
    requirements.coherentBufferAlias = requirements.coherentBuffers && !program.Metadata().invalidatesCaches;
    return requirements;
}

bool IsWaveMaskBranch(BranchCondition condition) {
    return condition == BranchCondition::ExecZero || condition == BranchCondition::ExecNonZero || condition == BranchCondition::VccZero || condition == BranchCondition::VccNonZero;
}

std::unordered_set<const IrValue*> WaveUniformValues(const IrProgram& program) {
    std::unordered_map<const IrValue*, bool> varying;
    const auto isVarying = [&](const IrValue* value) {
        const auto* resolved = value->Resolve();
        if (resolved == nullptr || resolved->HasImmediate()) return false;
        const auto found = varying.find(resolved);
        return found != varying.end() && found->second;
    };
    bool changed = true;
    while (changed) {
        changed = false;
        for (const IrBlock* block : program.BlockOrder()) {
            for (const IrValue* inst : block->Instructions()) {
                bool lanes = !UniformSource(*inst) && LaneSource(program, *inst);
                for (std::size_t index = 0; !lanes && !UniformSource(*inst) && index < inst->ArgumentCount(); index++) {
                    if (inst->Argument(index) != nullptr) lanes = isVarying(inst->Argument(index));
                }
                auto& entry = varying[inst];
                if (lanes && !entry) {
                    entry = true;
                    changed = true;
                }
            }
        }
    }
    std::unordered_set<const IrValue*> uniform;
    for (const auto& [value, lanes] : varying) {
        if (!lanes) uniform.insert(value);
    }
    return uniform;
}

}
