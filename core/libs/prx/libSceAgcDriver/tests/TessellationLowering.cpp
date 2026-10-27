#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "Optimization/TessellationLowering.hpp"
#include "SpirvBackend/SpirvEmitter.hpp"
#include <spirv/unified1/spirv.hpp>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>

using namespace ShaderRecompiler;

namespace {

constexpr std::uint32_t InputControlPoints = 3u;
constexpr std::uint32_t LocalStride = 32u;
constexpr std::uint32_t PatchStride = InputControlPoints * LocalStride;
constexpr std::uint32_t ReadPc = 0x40u;

bool IsKind(const IrValue* value, TessellationAttribute kind) {
    return value->HasImmediate() && value->Type() == IrType::U32 && value->ImmediateU32() == static_cast<std::uint32_t>(kind);
}

struct Hull {
    IrProgram program;
    IrBuilder builder{program};
    IrBlock* block = nullptr;

    Hull() {
        program.Resources().stage = IrShaderStage::TessellationControl;
        program.Resources().memoryInfo.push_back(MemoryInfo{.kind = ResourceKind::Lds});
        static_cast<void>(newBlock());
    }

    IrBlock& newBlock() {
        auto& created = program.CreateBlock();
        program.BlockOrder().push_back(&created);
        BlockInfo info;
        info.id = created.Id();
        program.Metadata().blockInfo.push_back(info);
        if (block == nullptr) program.SetEntryBlock(created);
        block = &created;
        builder.SetInsertionPoint(created);
        return created;
    }

    IrValue& invocation() {
        return builder.Emit(IrOpcode::GetBuiltin, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(StageInputKind::InvocationId)), &builder.Constant(0u)});
    }

    IrValue& relativePatch() {
        return builder.Emit(IrOpcode::TessellationBase, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(TessellationBaseKind::RelativeIndex))});
    }

    IrValue& computed(std::uint32_t limit, std::uint32_t multiple) {
        auto& index = builder.Emit(IrOpcode::UMin32, IrType::U32, {&invocation(), &builder.Constant(limit)});
        return builder.IMul(index, builder.Constant(multiple));
    }

    IrValue& firstInvocation() {
        return builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {&invocation(), &builder.Constant(1u)});
    }

    IrValue& select(std::uint32_t firstLimit, std::uint32_t firstMultiple, IrValue& second) {
        return builder.Emit(IrOpcode::SelectU32, IrType::U32, {&firstInvocation(), &computed(firstLimit, firstMultiple), &second});
    }

    IrValue& inPatch(IrValue& offset) {
        return builder.IAdd(builder.IMul(relativePatch(), builder.Constant(PatchStride)), offset);
    }

    IrValue& read(IrValue& address) {
        auto& loaded = builder.Emit(IrOpcode::LoadSharedU32, IrType::U32, {&address, &builder.ConstantBool(true)}, MemoryFlags{0u, ReadPc});
        static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&loaded}));
        return loaded;
    }

    IrValue& read(IrValue& address, IrValue& predicate) {
        auto& loaded = builder.Emit(IrOpcode::LoadSharedU32, IrType::U32, {&address, &predicate}, MemoryFlags{0u, ReadPc});
        static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&loaded}));
        return loaded;
    }

    IrValue& complementedOwnLaneBit(IrValue& condition) {
        auto& ballot = builder.Emit(IrOpcode::Ballot, IrType::U32x4, {&condition});
        auto& word = builder.Emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &builder.Constant(0u)});
        auto& complement = builder.Emit(IrOpcode::BitwiseNot32, IrType::U32, {&word});
        auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
        auto& laneBit = builder.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&lane, &builder.Constant(31u)});
        auto& shifted = builder.Emit(IrOpcode::ShiftRightLogical32, IrType::U32, {&complement, &laneBit});
        auto& bit = builder.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&shifted, &builder.Constant(1u)});
        return builder.Emit(IrOpcode::INotEqual32, IrType::Bool, {&bit, &builder.Constant(0u)});
    }

    IrValue& merge(std::uint32_t firstMultiple, std::uint32_t secondMultiple) {
        IrBlock& entry = *block;
        IrBlock& first = newBlock();
        IrBlock& second = newBlock();
        IrBlock& together = newBlock();
        builder.SetInsertionPoint(entry);
        builder.BranchConditional(firstInvocation(), first, second);
        builder.SetInsertionPoint(first);
        auto& left = computed(1u, firstMultiple);
        builder.Branch(together);
        builder.SetInsertionPoint(second);
        auto& right = computed(1u, secondMultiple);
        builder.Branch(together);
        builder.SetInsertionPoint(together);
        auto& merged = builder.Emit(IrOpcode::Phi, IrType::U32, {});
        merged.AddPhiOperand(&first, &left);
        merged.AddPhiOperand(&second, &right);
        return merged;
    }
};

struct Outcome {
    bool lowered = false;
    std::string reason;
    const IrValue* emitted = nullptr;
};

const ShaderTessellationInputInfo Tessellation{.inputControlPoints = InputControlPoints, .outputControlPoints = InputControlPoints, .patchesPerGroup = 1u};
const ShaderTessellationInputInfo GroupedTessellation{.inputControlPoints = InputControlPoints, .outputControlPoints = InputControlPoints, .patchesPerGroup = 1u, .primitiveGroupSize = 1u};

Outcome Lower(IrProgram& program, const ShaderTessellationInputInfo& tessellation = Tessellation) {
    Outcome outcome;
    try {
        TessellationLowering{}.Lower(program, tessellation);
    } catch (const std::exception& error) {
        outcome.reason = error.what();
        return outcome;
    }
    outcome.lowered = true;
    for (IrBlock* block : program.BlockOrder()) {
        for (IrValue* inst : block->Instructions()) {
            if (inst->Opcode() == IrOpcode::GetTessellationAttribute && IsKind(inst->Argument(0), TessellationAttribute::ControlInput)) {
                outcome.emitted = inst->Argument(1)->Resolve();
            }
        }
    }
    return outcome;
}

bool LoweredAt(IrProgram& program, IrValue& address) {
    const Outcome outcome = Lower(program);
    if (!outcome.lowered) std::fprintf(stderr, "refused: %s\n", outcome.reason.c_str());
    return outcome.lowered && outcome.emitted == address.Resolve();
}

bool RefusedBecause(IrProgram& program, const char* reason) {
    const Outcome outcome = Lower(program);
    if (outcome.lowered) std::fprintf(stderr, "lowered instead of refusing: %s\n", reason);
    return !outcome.lowered && outcome.reason.find(reason) != std::string::npos;
}

std::uint32_t EmittedEvaluationControlPoint(std::uint32_t address) {
    constexpr std::uint32_t ControlStride = 16u;
    IrProgram program;
    IrBuilder builder{program};
    program.SetWaveSize(64);
    program.Resources().stage = IrShaderStage::TessellationEvaluation;
    program.Resources().srtPlanComplete = true;
    program.Resources().resourceTrackingComplete = true;
    program.Metadata().shaderInfoComplete = true;
    program.Metadata().bindingLayoutComplete = true;
    program.Metadata().tessellationControlStride = ControlStride;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    BlockInfo info;
    info.id = block.Id();
    program.Metadata().blockInfo.push_back(info);
    builder.SetInsertionPoint(block);
    auto& read = builder.Emit(IrOpcode::GetTessellationAttribute, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(TessellationAttribute::EvaluationInput)), &builder.Constant(address), &builder.ConstantBool(true)});
    static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&read}));
    ShaderVertexInputInfo vertex;
    vertex.tess.inputControlPoints = InputControlPoints;
    vertex.tess.outputControlPoints = InputControlPoints;
    vertex.tess.domain = 1u;
    vertex.tess.partitioning = 2u;
    vertex.tess.outputTopology = 2u;
    ShaderStageInputInfo inputs;
    inputs.vertex = &vertex;
    CompiledBindingLayout bindings;
    bindings.layout = program.Metadata().bindings;
    SpirvTargetOptions target{};
    target.vulkanVersion = 0x00401000;
    target.spirvVersion = 0x00010300;
    target.subgroupSize = 64;
    const auto words = SpirvEmitter().Emit(program, inputs, bindings, target);
    std::unordered_map<std::uint32_t, std::uint32_t> constants;
    for (std::size_t at = 5; at < words.size(); at += words[at] >> 16u) {
        const auto count = words[at] >> 16u;
        if (count == 0u || at + count > words.size()) throw std::runtime_error("the domain program compiled to a malformed SPIR-V instruction");
        const auto opcode = words[at] & 0xffffu;
        if (opcode == spv::OpConstant && count == 4u) constants[words[at + 2u]] = words[at + 3u];
        if (opcode == spv::OpAccessChain && count == 7u) return constants.at(words[at + 4u]);
    }
    throw std::runtime_error("the domain program reads no control point attribute");
}

enum class Conjunction { None, Before, After };

bool LowersUnderComplementedBallot(Conjunction conjunction) {
    Hull hull;
    hull.program.SetWaveSize(32);
    auto& first = hull.firstInvocation();
    auto& notFirst = hull.builder.Emit(IrOpcode::LogicalNot, IrType::Bool, {&first});
    auto& own = hull.complementedOwnLaneBit(notFirst);
    auto& other = hull.builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {&hull.invocation(), &hull.builder.Constant(2u)});
    IrValue* predicate = &own;
    if (conjunction == Conjunction::Before) predicate = &hull.builder.Emit(IrOpcode::LogicalAnd, IrType::Bool, {&own, &other});
    if (conjunction == Conjunction::After) predicate = &hull.builder.Emit(IrOpcode::LogicalAnd, IrType::Bool, {&other, &own});
    auto& square = hull.builder.IMul(hull.invocation(), hull.invocation());
    auto& offset = hull.builder.Emit(IrOpcode::SelectU32, IrType::U32, {&notFirst, &square, &hull.computed(2u, LocalStride)});
    auto& address = hull.inPatch(offset);
    hull.read(address, *predicate);
    return LoweredAt(hull.program, address);
}

bool run(const char* name, const std::function<bool()>& body) {
    bool passed = false;
    try {
        passed = body();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: %s\n", name, error.what());
    }
    if (!passed) std::fprintf(stderr, "failed: %s\n", name);
    return passed;
}

}

int main() {
    bool passed = true;

    passed &= run("a hull read at a computed control-point index keeps the program's address", [] {
        Hull hull;
        auto& address = hull.inPatch(hull.computed(2u, LocalStride));
        hull.read(address);
        return LoweredAt(hull.program, address);
    });

    passed &= run("a hull read at a control point selected from data keeps the program's address", [] {
        Hull hull;
        auto& address = hull.inPatch(hull.select(2u, LocalStride, hull.computed(1u, LocalStride)));
        hull.read(address);
        return LoweredAt(hull.program, address);
    });

    passed &= run("a hull read at a control point merged by a phi keeps the program's address", [] {
        Hull hull;
        auto& address = hull.inPatch(hull.merge(LocalStride, LocalStride));
        hull.read(address);
        return LoweredAt(hull.program, address);
    });

    passed &= run("a selected control point at half a slot is refused", [] {
        Hull hull;
        auto& address = hull.inPatch(hull.select(2u, LocalStride, hull.computed(1u, LocalStride / 2u)));
        hull.read(address);
        return RefusedBecause(hull.program, "not a whole number of control points");
    });

    passed &= run("a control point merged by a phi at half a slot is refused", [] {
        Hull hull;
        auto& address = hull.inPatch(hull.merge(LocalStride, LocalStride / 2u));
        hull.read(address);
        return RefusedBecause(hull.program, "not a whole number of control points");
    });

    passed &= run("a selected control point at two different offsets is refused", [] {
        Hull hull;
        auto& offset = hull.builder.IAdd(hull.computed(1u, LocalStride), hull.builder.Constant(LocalStride));
        auto& address = hull.inPatch(hull.select(2u, LocalStride, offset));
        hull.read(address);
        return RefusedBecause(hull.program, "not a whole number of control points");
    });

    passed &= run("a hull read at a computed control-point index stays in its patch when the relative patch is also data", [] {
        Hull hull;
        auto& address = hull.inPatch(hull.computed(2u, LocalStride));
        hull.read(address);
        static_cast<void>(hull.builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&hull.relativePatch()}));
        const Outcome outcome = Lower(hull.program, GroupedTessellation);
        if (!outcome.lowered) std::fprintf(stderr, "refused: %s\n", outcome.reason.c_str());
        if (!outcome.lowered || outcome.emitted->Opcode() != IrOpcode::ISub32 || outcome.emitted->Argument(0)->Resolve() != address.Resolve()) return false;
        const IrValue* patch = outcome.emitted->Argument(1)->Resolve();
        return patch->Opcode() == IrOpcode::IMul32 && patch->Argument(1)->Resolve()->ImmediateU32() == PatchStride;
    });

    passed &= run("a hull read under its own lane's bit of a complemented ballot takes the select arm that bit implies", [] {
        return LowersUnderComplementedBallot(Conjunction::None);
    });

    passed &= run("a hull read under that bit and another condition takes the same select arm", [] {
        return LowersUnderComplementedBallot(Conjunction::Before);
    });

    passed &= run("a hull read under another condition and that bit takes the same select arm", [] {
        return LowersUnderComplementedBallot(Conjunction::After);
    });

    passed &= run("a domain read at an immediate address past the last control point reads the last control point", [] {
        return EmittedEvaluationControlPoint(InputControlPoints * 16u) == InputControlPoints - 1u;
    });

    return passed ? 0 : 1;
}
