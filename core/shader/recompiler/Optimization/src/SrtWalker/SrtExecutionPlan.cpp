#include "Optimization/SrtWalker/SrtExecutionPlan.hpp"
#include "Optimization/SrtWalker/SrtAddressArithmetic.hpp"
#include "Optimization/SrtWalker/SrtInstructionPredicates.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace ShaderRecompiler::Detail {

namespace {

class Compiler {
public:
    explicit Compiler(const IrResourcePlan& program) : program(program) {}

    std::shared_ptr<const SrtExecutionPlan> Run() {
        if (!program.srtPlanComplete || program.requiresSpecializationMemory || std::ranges::any_of(program.cleanFlatSlots, [](auto value) { return value != 0; })) return nullptr;
        result.active.assign(program.descriptorSources.size(), 1);
        if (!program.controlFlow.empty()) {
            for (const auto& block : program.controlFlow) {
                if (block.condition != nullptr) return nullptr;
                for (const auto source : block.sources) {
                    if (source >= result.active.size()) return nullptr;
                    result.active[source] = 0;
                }
            }
            std::vector<std::uint8_t> visited(program.controlFlow.size());
            std::vector<std::uint32_t> pending{0};
            while (!pending.empty()) {
                const auto index = pending.back();
                pending.pop_back();
                if (index >= visited.size()) return nullptr;
                if (visited[index]) continue;
                visited[index] = 1;
                const auto& block = program.controlFlow[index];
                for (const auto source : block.sources) result.active[source] = 1;
                pending.insert(pending.end(), block.successors.begin(), block.successors.end());
            }
        }
        for (const auto sourceIndex : program.materializationSources) {
            if (sourceIndex >= program.descriptorSources.size()) return nullptr;
            const auto& source = program.descriptorSources[sourceIndex];
            SrtExecutionPlan::Descriptor descriptor;
            descriptor.count = source.dwordCount;
            if (descriptor.count > descriptor.words.size()) return nullptr;
            for (std::uint32_t i = 0; i < descriptor.count; ++i) {
                const auto index = result.active[sourceIndex] ? compile(source.dwords[i]) : constant(0);
                if (!index) return nullptr;
                descriptor.words[i] = *index;
            }
            result.descriptors.push_back(descriptor);
        }
        result.flat.resize(program.srtReads.size());
        for (std::size_t slot = 0; slot < program.srtReads.size(); ++slot) {
            const auto& read = program.srtReads[slot];
            if (read.value == nullptr || read.flatOffset != slot) return nullptr;
            leaf = read.flatOffset < program.pureFlatSlots.size() && program.pureFlatSlots[read.flatOffset] != 0 ? read.value->Resolve() : nullptr;
            leafSlot = read.flatOffset;
            const auto index = compile(read.value);
            if (!index) return nullptr;
            result.flat[read.flatOffset] = *index;
        }
        return std::make_shared<const SrtExecutionPlan>(std::move(result));
    }

private:
    std::optional<std::uint32_t> constant(std::uint64_t value) {
        const auto index = static_cast<std::uint32_t>(result.operations.size());
        result.operations.push_back({IrOpcode::Void, {}, value});
        return index;
    }

    std::optional<std::uint32_t> compile(IrValue* raw) {
        if (raw == nullptr) return {};
        const auto* value = raw->Resolve();
        if (const auto found = indices.find(value); found != indices.end()) return found->second;
        if (result.operations.size() >= 4096 || visiting.size() >= 256 || !visiting.insert(value).second) return {};
        const auto index = compileValue(*value);
        visiting.erase(value);
        if (index) indices.emplace(value, *index);
        return index;
    }

    std::optional<std::uint32_t> compileValue(const IrValue& value) {
        if (value.HasImmediate()) {
            switch (value.Type()) {
                case IrType::Bool: return constant(value.ImmediateBool());
                case IrType::U8: return constant(value.ImmediateU8());
                case IrType::U16: return constant(value.ImmediateU16());
                case IrType::U32: return constant(value.ImmediateU32());
                case IrType::U64: return constant(value.ImmediateU64());
                case IrType::F32: return constant(std::bit_cast<std::uint32_t>(value.ImmediateF32()));
                default: return {};
            }
        }
        SrtExecutionPlan::Operation operation;
        operation.opcode = value.Opcode();
        const auto argument = [&](std::size_t source, std::size_t target) {
            if (source >= value.ArgumentCount()) return false;
            const auto index = compile(value.Argument(source));
            if (!index) return false;
            operation.arguments[target] = *index;
            return true;
        };
        switch (operation.opcode) {
            case IrOpcode::GetUserData: {
                if (value.ArgumentCount() != 1) return {};
                const auto reg = RegIndex(static_cast<ScalarReg>(value.Argument(0)->Register().index));
                if (reg < program.userDataBase) return {};
                operation.immediate = reg - program.userDataBase;
                break;
            }
            case IrOpcode::GetShaderBase: break;
            case IrOpcode::BitCastU32F32:
            case IrOpcode::BitCastF32U32:
                return value.ArgumentCount() == 1 ? compile(value.Argument(0)) : std::nullopt;
            case IrOpcode::ReadConst: {
                if (value.ArgumentCount() != 2) return {};
                const auto* slot = value.Argument(1)->Resolve();
                if (!slot->HasImmediate() || slot->Type() != IrType::U32 || slot->ImmediateU32() >= program.srtReads.size()) return {};
                return compile(program.srtReads[slot->ImmediateU32()].value);
            }
            case IrOpcode::LoadAddressU32:
            case IrOpcode::ReadConstBuffer: {
                if (!IsRawRead(program, value) || value.ArgumentCount() != IrOpcodeOperandCount(operation.opcode)) return {};
                const auto* handle = value.Argument(0)->Resolve();
                const bool buffer = operation.opcode == IrOpcode::ReadConstBuffer;
                if (handle->Opcode() == IrOpcode::Void || handle->ArgumentCount() < 2 || (buffer && handle->ArgumentCount() != 4)) return {};
                for (std::size_t i = 0; i < 2; ++i) {
                    const auto index = compile(handle->Argument(i));
                    if (!index) return {};
                    operation.arguments[i] = *index;
                }
                if (!argument(1, 2)) return {};
                if (buffer) {
                    for (std::size_t i = 2; i < 4; ++i) {
                        const auto index = compile(handle->Argument(i));
                        if (!index) return {};
                        operation.arguments[i + 1] = *index;
                    }
                }
                operation.immediate = program.memoryInfo[value.Flags<MemoryFlags>().index].offset;
                if (&value == leaf) operation.leafSlot = leafSlot;
                break;
            }
            case IrOpcode::CompositeExtractU32x2:
            case IrOpcode::CompositeExtractU64: {
                if (value.ArgumentCount() != 2) return {};
                const auto* component = value.Argument(1)->Resolve();
                if (!component->HasImmediate() || component->Type() != IrType::U32 || component->ImmediateU32() >= 2) return {};
                operation.immediate = component->ImmediateU32() * 32u;
                const auto* source = value.Argument(0)->Resolve();
                if (operation.opcode == IrOpcode::CompositeExtractU32x2) {
                    if (source->ArgumentCount() != 2) return {};
                    if (source->Opcode() == IrOpcode::CompositeConstructU32x2) return compile(source->Argument(component->ImmediateU32()));
                    if (source->Opcode() != IrOpcode::IAddCarry32) return {};
                    for (std::size_t i = 0; i < 2; ++i) {
                        const auto index = compile(source->Argument(i));
                        if (!index) return {};
                        operation.arguments[i] = *index;
                    }
                } else if (!argument(0, 0)) return {};
                break;
            }
            case IrOpcode::BitwiseNot32:
                if (!argument(0, 0)) return {};
                break;
            case IrOpcode::CompositeConstructU64:
            case IrOpcode::IAdd32: case IrOpcode::IAdd64:
            case IrOpcode::ISub32: case IrOpcode::ISub64:
            case IrOpcode::IMul32: case IrOpcode::IMul64:
            case IrOpcode::UMin32:
            case IrOpcode::BitwiseAnd32: case IrOpcode::BitwiseAnd64:
            case IrOpcode::BitwiseOr32: case IrOpcode::BitwiseXor32:
            case IrOpcode::ShiftLeftLogical32: case IrOpcode::ShiftLeftLogical64:
            case IrOpcode::ShiftRightLogical32: case IrOpcode::ShiftRightLogical64:
                if (!argument(0, 0) || !argument(1, 1)) return {};
                break;
            default: return {};
        }
        const auto index = static_cast<std::uint32_t>(result.operations.size());
        result.operations.push_back(operation);
        return index;
    }

    const IrResourcePlan& program;
    SrtExecutionPlan result;
    std::unordered_map<const IrValue*, std::uint32_t> indices;
    std::unordered_set<const IrValue*> visiting;
    const IrValue* leaf = nullptr;
    std::uint32_t leafSlot = 0;
};

}

std::shared_ptr<const SrtExecutionPlan> CompileSrtExecutionPlan(const IrResourcePlan& program) {
    return Compiler(program).Run();
}

bool SrtExecutionPlan::Evaluate(const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flattened, std::vector<std::uint8_t>& activeSources) const {
    std::array<std::uint64_t, 256> local;
    std::vector<std::uint64_t> overflow;
    if (operations.size() > local.size()) overflow.resize(operations.size());
    auto* values = overflow.empty() ? local.data() : overflow.data();
    for (std::size_t index = 0; index < operations.size(); ++index) {
        const auto& operation = operations[index];
        const auto arg = [&](std::size_t i) { return values[operation.arguments[i]]; };
        const auto u32 = [&](std::size_t i) { return static_cast<std::uint32_t>(arg(i)); };
        auto& out = values[index];
        switch (operation.opcode) {
            case IrOpcode::Void: out = operation.immediate; break;
            case IrOpcode::GetUserData:
                if (operation.immediate >= runtime.userData.size()) return false;
                out = runtime.userData[operation.immediate];
                break;
            case IrOpcode::GetShaderBase: out = runtime.shaderBase; break;
            case IrOpcode::CompositeConstructU64: out = u32(0) | (std::uint64_t{u32(1)} << 32u); break;
            case IrOpcode::CompositeExtractU64: out = static_cast<std::uint32_t>(arg(0) >> operation.immediate); break;
            case IrOpcode::CompositeExtractU32x2: out = static_cast<std::uint32_t>((std::uint64_t{u32(0)} + u32(1)) >> operation.immediate); break;
            case IrOpcode::IAdd32: out = u32(0) + u32(1); break;
            case IrOpcode::IAdd64: out = arg(0) + arg(1); break;
            case IrOpcode::ISub32: out = u32(0) - u32(1); break;
            case IrOpcode::ISub64: out = arg(0) - arg(1); break;
            case IrOpcode::IMul32: out = u32(0) * u32(1); break;
            case IrOpcode::IMul64: out = arg(0) * arg(1); break;
            case IrOpcode::UMin32: out = std::min(u32(0), u32(1)); break;
            case IrOpcode::BitwiseAnd32: out = u32(0) & u32(1); break;
            case IrOpcode::BitwiseAnd64: out = arg(0) & arg(1); break;
            case IrOpcode::BitwiseOr32: out = u32(0) | u32(1); break;
            case IrOpcode::BitwiseXor32: out = u32(0) ^ u32(1); break;
            case IrOpcode::BitwiseNot32: out = ~u32(0); break;
            case IrOpcode::ShiftLeftLogical32: out = u32(0) << (arg(1) & 31u); break;
            case IrOpcode::ShiftLeftLogical64: out = arg(0) << (arg(1) & 63u); break;
            case IrOpcode::ShiftRightLogical32: out = u32(0) >> (arg(1) & 31u); break;
            case IrOpcode::ShiftRightLogical64: out = arg(0) >> (arg(1) & 63u); break;
            case IrOpcode::LoadAddressU32:
            case IrOpcode::ReadConstBuffer: {
                const auto base = ((arg(1) << 32u) | u32(0)) & AddressMask;
                const auto immediate = static_cast<std::int64_t>(static_cast<std::int32_t>(operation.immediate));
                std::uint64_t address = 0;
                if (operation.opcode == IrOpcode::ReadConstBuffer) {
                    if (immediate < 0) return false;
                    const auto byteOffset = static_cast<std::uint64_t>(immediate) + u32(2);
                    const auto aligned = byteOffset & ~std::uint64_t{3};
                    const auto stride = (u32(1) >> 16u) & 0x3fffu;
                    const auto size = stride == 0 ? std::uint64_t{u32(3)} : std::uint64_t{stride} * u32(3);
                    if (aligned > size || size - aligned < 4) return false;
                    address = ((base & ~std::uint64_t{3}) + byteOffset) & ~std::uint64_t{3};
                } else if (!AddSignedAddress(base & ~std::uint64_t{3}, (immediate & ~std::int64_t{3}) + (u32(2) & ~3u), address)) return false;
                if (auto* trace = runtime.readTrace; trace != nullptr) {
                    if (operation.leafSlot != ~0u) trace->leaves.emplace_back(operation.leafSlot, address);
                    else trace->otherReads.push_back(address);
                }
                std::uint32_t word = 0;
                if (runtime.readMemory != nullptr) {
                    if (!runtime.readMemory(runtime.userContext, address, &word)) return false;
                } else std::memcpy(&word, reinterpret_cast<const void*>(address), sizeof(word));
                out = word;
                break;
            }
            default: return false;
        }
    }
    results.resize(descriptors.size());
    for (std::size_t i = 0; i < descriptors.size(); ++i) {
        results[i] = {};
        results[i].dwordCount = descriptors[i].count;
        for (std::uint32_t word = 0; word < descriptors[i].count; ++word) results[i].dwords[word] = static_cast<std::uint32_t>(values[descriptors[i].words[word]]);
    }
    flattened.resize(flat.size());
    for (std::size_t i = 0; i < flat.size(); ++i) flattened[i] = static_cast<std::uint32_t>(values[flat[i]]);
    activeSources = active;
    return true;
}

}
