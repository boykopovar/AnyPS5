#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/TessellationLowering.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <string>

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

Outcome Lower(IrProgram& program) {
    Outcome outcome;
    try {
        TessellationLowering{}.Lower(program, Tessellation);
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

    return passed ? 0 : 1;
}
