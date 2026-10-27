#define SPV_ENABLE_UTILITY_CODE
#include <spirv/unified1/spirv.hpp>
#undef SPV_ENABLE_UTILITY_CODE
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "SpirvBackend/SpirvEmitter.hpp"
#include "PipelineSpecialization.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace ShaderRecompiler;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct LoadCase {
    ResourceKind kind;
    IrOpcode opcode;
    IrType type;
    std::uint32_t bits = 32;
    std::uint32_t components = 1;
};

std::vector<std::uint32_t> emitLoad(const LoadCase& test, bool active, std::uint32_t offset, std::uint32_t records, bool robust, bool exact = false, bool valid = true) {
    IrProgram program;
    program.SetWaveSize(32);
    program.Resources().stage = IrShaderStage::Compute;
    program.Resources().srtPlanComplete = true;
    program.Resources().resourceTrackingComplete = true;
    program.Metadata().shaderInfoComplete = true;
    program.Metadata().bindingLayoutComplete = true;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    program.Metadata().blockInfo.emplace_back();
    const auto constant = [&](std::uint32_t number) -> IrValue& {
        auto& value = program.CreateValue(IrOpcode::Void, IrType::U32);
        value.SetImmediateU32(number);
        return value;
    };
    const auto emit = [&](IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments) -> IrValue& {
        auto& value = program.CreateValue(opcode, type);
        for (auto* argument : arguments) value.AddArgument(argument);
        block.AppendInstruction(&value);
        return value;
    };
    auto& enabled = program.CreateValue(IrOpcode::Void, IrType::Bool);
    enabled.SetImmediateBool(active);
    auto& zero = constant(0);
    auto& address = constant(offset);
    MemoryInfo memory;
    memory.kind = test.kind;
    memory.dataBits = test.bits;
    memory.dataDwords = test.components;
    memory.componentCount = test.components;
    program.Resources().memoryInfo.push_back(memory);
    if (test.kind == ResourceKind::Scratch) {
        program.Info().scratchDwords = 16;
        auto& handle = emit(IrOpcode::GetScratchResource, IrType::AddressResource, {});
        emit(test.opcode, test.type, {&handle, &address, &zero, &enabled});
    } else if (test.kind == ResourceKind::Buffer) {
        program.Resources().memoryInfo.front().coherent = true;
        program.Info().buffers.emplace_back().read = true;
        program.Metadata().bindings.descriptors.push_back({DescriptorBindingKind::Buffers, {0}});
        program.Metadata().bindings.pushDataStartDword = 0;
        program.Metadata().bindings.memoryOffsetCount = 1;
        auto& size = constant(records);
        auto& base = constant(valid ? 0x100u : 0u);
        auto& handle = emit(IrOpcode::GetBufferResource, IrType::BufferResource, {&base, &zero, &size, &zero});
        emit(test.opcode, test.type, {&handle, &zero, &address, &zero, &enabled});
    } else {
        if (test.kind == ResourceKind::Gds) program.Metadata().bindings.descriptors.push_back({DescriptorBindingKind::Gds, {0}});
        emit(test.opcode, test.type, {&address, &enabled});
    }
    ShaderComputeInputInfo compute;
    compute.threadsNum[0] = 32;
    compute.threadsNum[1] = 1;
    compute.threadsNum[2] = 1;
    compute.ldsSizeDwords = 16;
    compute.scratchSizeDwords = 16;
    compute.hostSubgroupSize = 32;
    compute.waveSize = 32;
    ShaderStageInputInfo inputs;
    inputs.compute = &compute;
    CompiledBindingLayout bindings;
    bindings.layout = program.Metadata().bindings;
    SpirvTargetOptions target{};
    target.vulkanVersion = 0x00401000;
    target.spirvVersion = 0x00010300;
    target.subgroupSize = 32;
    target.robustBufferAccess2 = robust;
    target.exactRobustLoads = exact;
    return SpirvEmitter().Emit(program, inputs, bindings, target);
}

class AccessTrace {
public:
    AccessTrace(const std::vector<std::uint32_t>& module, std::uint32_t misalignment = 0)
        : words(module), misalignment(misalignment) {
        require(words.size() >= 5, "SPIR-V module header is missing");
        for (std::size_t offset = 5; offset < words.size();) {
            const auto count = words[offset] >> 16;
            require(count != 0 && count <= words.size() - offset, "SPIR-V instruction is truncated");
            const auto instruction = std::span(words).subspan(offset, count);
            const auto opcode = static_cast<spv::Op>(instruction[0] & 0xffff);
            bool result = false;
            bool type = false;
            spv::HasResultAndType(opcode, &result, &type);
            if (result) definitions.emplace(instruction[type ? 2 : 1], instruction);
            if (opcode == spv::OpName) names.emplace(instruction[1], std::string(reinterpret_cast<const char*>(instruction.data() + 2)));
            if (opcode == spv::OpDecorate && instruction[2] == spv::DecorationSpecId) specializations.emplace(instruction[1], instruction[3]);
            if (opcode == spv::OpLabel) labels.emplace(instruction[1], offset);
            if (opcode == spv::OpEntryPoint) entryFunction = instruction[2];
            offset += count;
        }
    }

    std::vector<std::uint32_t> Run() {
        std::size_t cursor = 0;
        bool entry = false;
        for (std::size_t offset = 5; offset < words.size(); offset += words[offset] >> 16) {
            const auto opcode = static_cast<spv::Op>(words[offset] & 0xffff);
            if (opcode == spv::OpFunction) entry = words[offset + 2] == entryFunction;
            if (entry && opcode == spv::OpLabel) { cursor = offset; break; }
        }
        require(cursor != 0, "SPIR-V entry block is missing");
        std::vector<std::uint32_t> reads;
        std::uint32_t previous = 0;
        std::uint32_t current = 0;
        for (std::size_t steps = 0; steps < 4096; ++steps) {
            const auto instruction = std::span(words).subspan(cursor, words[cursor] >> 16);
            const auto opcode = static_cast<spv::Op>(instruction[0] & 0xffff);
            if (opcode == spv::OpLabel) current = instruction[1];
            if (opcode == spv::OpPhi) {
                bool found = false;
                for (std::size_t index = 3; index + 1 < instruction.size(); index += 2) {
                    if (instruction[index + 1] == previous) { phiValues[instruction[2]] = instruction[index]; found = true; }
                }
                require(found, "SPIR-V phi has no incoming edge for the executed path");
            }
            if (opcode == spv::OpLoad && isGuestPointer(instruction[3])) {
                const auto pointer = definition(instruction[3]);
                require((pointer[0] & 0xffff) == spv::OpAccessChain, "guest read is not an indexed access");
                reads.push_back(value(pointer.back()));
                if (rootName(instruction[3]) == "scratch_dwords") require(pointerStorage(instruction[3]) == spv::StorageClassFunction, "scratch selection accessed non-private storage");
                if (rootName(instruction[3]).starts_with("buffers")) require(instruction.size() == 5 && (instruction[4] & spv::MemoryAccessVolatileMask), "coherent buffer read lost its volatile access");
            }
            if (opcode == spv::OpBranch || opcode == spv::OpBranchConditional) {
                const auto destination = opcode == spv::OpBranch ? instruction[1] : instruction[value(instruction[1]) ? 2 : 3];
                previous = current;
                cursor = labels.at(destination);
                continue;
            }
            if (opcode == spv::OpReturn) return reads;
            require(opcode != spv::OpFunctionCall && opcode != spv::OpSwitch && opcode != spv::OpLoopMerge, "access fixture unexpectedly requires a call, switch or loop");
            cursor += instruction.size();
            require(cursor < words.size(), "SPIR-V execution fell outside the function");
        }
        throw std::runtime_error("SPIR-V access trace exceeded its bounded instruction budget");
    }

private:
    std::span<const std::uint32_t> definition(std::uint32_t id) const {
        const auto found = definitions.find(id);
        require(found != definitions.end(), "SPIR-V access condition refers to an undefined value");
        return found->second;
    }

    std::string rootName(std::uint32_t id) const {
        const auto instruction = definition(id);
        if ((instruction[0] & 0xffff) == spv::OpAccessChain) return rootName(instruction[3]);
        const auto found = names.find(id);
        return found == names.end() ? std::string() : found->second;
    }

    std::uint32_t pointerStorage(std::uint32_t id) const {
        const auto instruction = definition(id);
        if ((instruction[0] & 0xffff) == spv::OpAccessChain) return pointerStorage(instruction[3]);
        require((instruction[0] & 0xffff) == spv::OpVariable, "access pointer has no variable root");
        return instruction[3];
    }

    bool isGuestPointer(std::uint32_t id) const {
        const auto name = rootName(id);
        return name == "lds_dwords" || name == "gds" || name == "scratch_dwords" || name.starts_with("buffers");
    }

    std::uint32_t value(std::uint32_t id) const {
        const auto instruction = definition(id);
        const auto opcode = static_cast<spv::Op>(instruction[0] & 0xffff);
        const auto argument = [&](std::size_t index) { return value(instruction[index]); };
        switch (opcode) {
        case spv::OpConstantTrue: return 1;
        case spv::OpConstantFalse: case spv::OpConstantNull: return 0;
        case spv::OpConstant: return instruction[3];
        case spv::OpSpecConstant: {
            const auto spec = specializations.at(id);
            if (spec == PipelineSpecialization::BufferBase) return 0;
            if (spec == PipelineSpecialization::BufferBase + 1) return 0x30001000;
            if (spec == PipelineSpecialization::BufferBase + 3) return misalignment;
            if (spec == PipelineSpecialization::PushDataOffset) return 0;
            throw std::runtime_error("unexpected specialization in access fixture");
        }
        case spv::OpSelect: return argument(3) ? argument(4) : argument(5);
        case spv::OpPhi: return value(phiValues.at(id));
        case spv::OpIAdd: return argument(3) + argument(4);
        case spv::OpISub: return argument(3) - argument(4);
        case spv::OpIMul: return argument(3) * argument(4);
        case spv::OpShiftLeftLogical:
            require(argument(4) < 32, "undefined shift in access condition");
            return argument(3) << argument(4);
        case spv::OpShiftRightLogical:
            require(argument(4) < 32, "undefined shift in access condition");
            return argument(3) >> argument(4);
        case spv::OpBitwiseAnd: return argument(3) & argument(4);
        case spv::OpBitwiseOr: return argument(3) | argument(4);
        case spv::OpBitFieldUExtract: {
            const auto count = argument(5);
            require(count <= 32 && argument(4) <= 32 - count, "invalid bitfield in access condition");
            return (argument(3) >> argument(4)) & (count == 32 ? 0xffffffffu : (1u << count) - 1);
        }
        case spv::OpIEqual: return argument(3) == argument(4);
        case spv::OpINotEqual: return argument(3) != argument(4);
        case spv::OpULessThan: return argument(3) < argument(4);
        case spv::OpULessThanEqual: return argument(3) <= argument(4);
        case spv::OpUGreaterThanEqual: return argument(3) >= argument(4);
        case spv::OpLogicalAnd: return argument(3) && argument(4);
        case spv::OpLogicalOr: return argument(3) || argument(4);
        case spv::OpLogicalNot: return !argument(3);
        case spv::OpArrayLength:
            require(isGuestPointer(instruction[3]), "unknown array length in access fixture");
            return 16;
        case spv::OpLoad:
            require(!isGuestPointer(instruction[3]), "guest data unexpectedly controls a fixture access");
            require(rootName(instruction[3]) == "vsharp" || rootName(instruction[3]).find("subgroup") != std::string::npos, "unknown input to access condition");
            return 0;
        default: throw std::runtime_error("unsupported opcode in access condition: " + std::to_string(opcode));
        }
    }

    const std::vector<std::uint32_t>& words;
    std::uint32_t misalignment;
    std::uint32_t entryFunction = 0;
    std::map<std::uint32_t, std::span<const std::uint32_t>> definitions;
    std::map<std::uint32_t, std::string> names;
    std::map<std::uint32_t, std::uint32_t> specializations;
    std::map<std::uint32_t, std::size_t> labels;
    std::map<std::uint32_t, std::uint32_t> phiValues;
};

constexpr std::uint32_t Unreachable = 0xfffffffeu;

std::uint32_t ldsLength(const std::vector<std::uint32_t>& words) {
    std::map<std::uint32_t, std::span<const std::uint32_t>> definitions;
    std::uint32_t variable = 0;
    for (std::size_t offset = 5; offset < words.size(); offset += words[offset] >> 16) {
        const auto instruction = std::span(words).subspan(offset, words[offset] >> 16);
        const auto opcode = static_cast<spv::Op>(instruction[0] & 0xffff);
        if (opcode == spv::OpName && std::string(reinterpret_cast<const char*>(instruction.data() + 2)) == "lds_dwords") variable = instruction[1];
        bool result = false;
        bool type = false;
        spv::HasResultAndType(opcode, &result, &type);
        if (result) definitions.emplace(instruction[type ? 2 : 1], instruction);
    }
    require(variable != 0, "LDS array is missing");
    const auto pointer = definitions.at(definitions.at(variable)[1]);
    const auto array = definitions.at(pointer[3]);
    require((array[0] & 0xffff) == spv::OpTypeArray, "LDS variable is not an array");
    return definitions.at(array[3])[3];
}

void expectReads(const LoadCase& test, bool robust, bool active, std::uint32_t offset, std::vector<std::uint32_t> expected, std::uint32_t records = 64, std::uint32_t misalignment = 0, bool valid = true) {
    const auto module = emitLoad(test, active, offset, records, robust, false, valid);
    const auto actual = AccessTrace(module, misalignment).Run();
    if (actual != expected) {
        std::string detail = std::string(robust ? "robust" : "plain") + " guest reads:";
        for (const auto index : actual) detail += " " + std::to_string(index);
        detail += "; expected:";
        for (const auto index : expected) detail += " " + std::to_string(index);
        throw std::runtime_error(detail);
    }
}

void ldsLoads(IrOpcode opcode, IrType type, std::uint32_t bits, std::uint32_t components) {
    const LoadCase test{ResourceKind::Lds, opcode, type, bits, components};
    require(ldsLength(emitLoad(test, true, 8, 64, false)) == 16u, "LDS array declares more than the program's dwords");
    for (const bool robust : {false, true}) {
        expectReads(test, robust, false, 8, {});
        expectReads(test, robust, true, 84, {});
        std::vector<std::uint32_t> indices;
        for (std::uint32_t index = 0; index < components; ++index) indices.push_back(2 + index);
        expectReads(test, robust, true, 8, indices);
        expectReads(test, robust, true, 60, {15});
    }
}

}

int main() {
    unsigned failures = 0;
    const auto run = [&](std::string_view name, const auto& test) {
        try {
            test();
            std::cout << "guarded memory loads: " << name << " passed\n";
        } catch (const std::exception& error) {
            ++failures;
            std::cout << "guarded memory loads: " << name << ": " << error.what() << '\n';
        }
    };
    run("LDS word keeps its branch", [] { ldsLoads(IrOpcode::LoadSharedU32, IrType::U32, 32, 1); });
    run("LDS subword keeps its branch", [] {
        ldsLoads(IrOpcode::LoadSharedU8, IrType::U8, 8, 1);
        ldsLoads(IrOpcode::LoadSharedU16, IrType::U16, 16, 1);
    });
    run("LDS wide keeps its branch", [] { ldsLoads(IrOpcode::LoadSharedU32x4, IrType::U32x4, 32, 4); });
    run("GDS word", [] {
        const LoadCase test{ResourceKind::Gds, IrOpcode::LoadSharedU32, IrType::U32, 32, 1};
        expectReads(test, false, false, 8, {});
        expectReads(test, false, true, 64, {});
        expectReads(test, false, true, 8, {2});
        expectReads(test, true, false, 8, {Unreachable});
        expectReads(test, true, true, 64, {Unreachable});
        expectReads(test, true, true, 8, {2});
        expectReads(test, true, true, 60, {15});
    });
    run("coherent buffer bounds", [] {
        const LoadCase test{ResourceKind::Buffer, IrOpcode::LoadBufferU32, IrType::U32};
        for (const bool robust : {false, true}) expectReads(test, robust, false, 8, {});
        expectReads(test, false, true, 64, {}, 128);
        expectReads(test, false, true, 60, {}, 32);
        expectReads(test, false, true, 8, {}, 64, 0, false);
        expectReads(test, true, true, 64, {Unreachable, Unreachable}, 128);
        expectReads(test, true, true, 60, {Unreachable, Unreachable}, 32);
        expectReads(test, true, true, 8, {Unreachable, Unreachable}, 64, 0, false);
    });
    run("coherent buffer high word", [] {
        const LoadCase test{ResourceKind::Buffer, IrOpcode::LoadBufferU32, IrType::U32};
        expectReads(test, false, true, 8, {2});
        expectReads(test, false, true, 8, {2, 3}, 64, 1);
        expectReads(test, false, true, 60, {}, 64, 1);
        expectReads(test, true, true, 8, {2, 2});
        expectReads(test, true, true, 8, {2, 3}, 64, 1);
        expectReads(test, true, true, 60, {Unreachable, Unreachable}, 64, 1);
    });
    run("exact robust loads read what the checked variant reads", [] {
        const LoadCase test{ResourceKind::Buffer, IrOpcode::LoadBufferU32, IrType::U32};
        const auto effective = [](std::vector<std::uint32_t> reads) {
            for (auto& index : reads) if (index >= 16u) index = Unreachable;
            return reads;
        };
        unsigned compared = 0;
        for (const bool active : {false, true}) {
            for (const std::uint32_t offset : {0u, 4u, 8u, 52u, 56u, 60u, 64u, 68u, 4092u}) {
                for (const std::uint32_t records : {32u, 64u, 128u}) {
                    for (const std::uint32_t misalignment : {0u, 1u, 2u, 3u}) {
                        for (const bool valid : {false, true}) {
                            const auto checked = emitLoad(test, active, offset, records, true, false, valid);
                            const auto exact = emitLoad(test, active, offset, records, true, true, valid);
                            const auto expected = effective(AccessTrace(checked, misalignment).Run());
                            const auto actual = effective(AccessTrace(exact, misalignment).Run());
                            if (actual != expected) {
                                std::string detail = "exact reads differ at offset " + std::to_string(offset) + " records " + std::to_string(records) + " misalignment " + std::to_string(misalignment) + " active " + std::to_string(active) + " valid " + std::to_string(valid) + ":";
                                for (const auto index : actual) detail += " " + std::to_string(index);
                                detail += "; checked:";
                                for (const auto index : expected) detail += " " + std::to_string(index);
                                throw std::runtime_error(detail);
                            }
                            ++compared;
                        }
                    }
                }
            }
        }
        require(compared == 2u * 9u * 3u * 4u * 2u, "exact robust loads: the comparison grid shrank");
        const auto plain = emitLoad(test, true, 8, 64, false, true);
        require(AccessTrace(plain, 0).Run() == std::vector<std::uint32_t>({2}), "exact request without robustBufferAccess2 changed the plain variant");
    });
    run("private scratch selection", [] {
        const LoadCase test{ResourceKind::Scratch, IrOpcode::LoadAddressU32, IrType::U32};
        for (const bool robust : {false, true}) {
            expectReads(test, robust, false, 8, {0});
            expectReads(test, robust, true, 64, {0});
            expectReads(test, robust, true, 8, {2});
        }
    });
    if (failures != 0) {
        std::cerr << "guarded memory loads: a guest load reached a location the guarded program could not\n";
        return 1;
    }
    std::cout << "Guarded memory load tests passed\n";
    return 0;
}
