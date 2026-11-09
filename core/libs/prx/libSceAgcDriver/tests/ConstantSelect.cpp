#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/ConstantFolder.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include <cstdint>
#include <cstdio>
#include <stdexcept>

using namespace ShaderRecompiler;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Program {
    IrProgram program;
    IrBuilder builder{program};
    IrValue* lane = nullptr;
    IrValue* condition = nullptr;
    IrValue* other = nullptr;
    std::uint32_t values = 0;

    Program() {
        auto& block = program.CreateBlock();
        program.BlockOrder().push_back(&block);
        BlockInfo info;
        info.id = 0;
        info.terminator.kind = TerminatorKind::Return;
        program.Metadata().blockInfo.push_back(info);
        program.SetEntryBlock(block);
        builder.SetInsertionPoint(block);
        lane = &builder.Emit(IrOpcode::LaneId, IrType::U32, {});
        condition = &builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {lane, &builder.Constant(16u)});
        other = &builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {lane, &builder.Constant(8u)});
    }

    IrValue& value() {
        return builder.Emit(IrOpcode::IAdd32, IrType::U32, {lane, &builder.Constant(++values)});
    }

    IrValue& select(IrValue& selector, IrValue& onTrue, IrValue& onFalse) {
        return builder.Emit(IrOpcode::SelectU32, IrType::U32, {&selector, &onTrue, &onFalse});
    }

    const IrValue* folded(IrValue& result) {
        auto& reference = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&result});
        ValidateProgram(program, true);
        ConstantFolder{}.Fold(program);
        DeadCodeEliminator{}.RemoveIdentities(program);
        DeadCodeEliminator{}.Eliminate(program);
        ValidateProgram(program, true);
        return reference.Argument(0)->Resolve();
    }
};

bool Selects(const IrValue* value, const IrValue* selector, const IrValue* onTrue, const IrValue* onFalse) {
    return value->Opcode() == IrOpcode::SelectU32 && value->Argument(0)->Resolve() == selector && value->Argument(1)->Resolve() == onTrue && value->Argument(2)->Resolve() == onFalse;
}

void CheckFalseArm() {
    Program p;
    auto& kept = p.value();
    auto& dropped = p.value();
    auto& fallback = p.value();
    auto& inner = p.select(*p.condition, dropped, fallback);
    const auto* result = p.folded(p.select(*p.condition, kept, inner));
    Require(Selects(result, p.condition, &kept, &fallback), "a select in the false arm of a select on the same condition was not folded to its false arm");
    Require(!inner.HasUses(), "the folded inner select is still used");
}

void CheckTrueArm() {
    Program p;
    auto& kept = p.value();
    auto& dropped = p.value();
    auto& fallback = p.value();
    const auto* result = p.folded(p.select(*p.condition, p.select(*p.condition, kept, dropped), fallback));
    Require(Selects(result, p.condition, &kept, &fallback), "a select in the true arm of a select on the same condition was not folded to its true arm");
}

void CheckChain() {
    Program p;
    auto& last = p.value();
    auto& fallback = p.value();
    auto* chain = &p.select(*p.condition, p.value(), fallback);
    chain = &p.select(*p.condition, p.value(), *chain);
    const auto* result = p.folded(p.select(*p.condition, last, *chain));
    Require(Selects(result, p.condition, &last, &fallback), "a chain of selects on one condition did not fold to the last write and the fallback");
}

void CheckEqualArms() {
    Program p;
    auto& same = p.value();
    const auto* result = p.folded(p.select(*p.condition, same, p.select(*p.condition, p.value(), same)));
    Require(result == &same, "a select whose arms become equal after folding was not replaced by that value");
}

void CheckOtherCondition() {
    Program p;
    auto& kept = p.value();
    auto& inner = p.select(*p.other, p.value(), p.value());
    const auto* result = p.folded(p.select(*p.condition, kept, inner));
    Require(Selects(result, p.condition, &kept, &inner), "a select nested under a different condition was folded");
}

}

int main() {
    try {
        CheckFalseArm();
        CheckTrueArm();
        CheckChain();
        CheckEqualArms();
        CheckOtherCondition();
        std::puts("constant select tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
