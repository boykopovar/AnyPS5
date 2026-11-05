#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/ConstantFolder.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include <cstdio>
#include <pthread.h>
#include <string>
#include <stdexcept>

using namespace ShaderRecompiler;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void CheckDiamond(unsigned mode) {
    IrProgram program;
    for (std::uint32_t id = 0; id < 4u; ++id) {
        auto& block = program.CreateBlock();
        program.BlockOrder().push_back(&block);
        BlockInfo info;
        info.id = id;
        info.terminator.kind = TerminatorKind::Return;
        program.Metadata().blockInfo.push_back(info);
    }
    auto& entry = *program.BlockOrder()[0];
    auto& left = *program.BlockOrder()[1];
    auto& right = *program.BlockOrder()[2];
    auto& merge = *program.BlockOrder()[3];
    program.SetEntryBlock(entry);
    entry.AddBranch(&left);
    entry.AddBranch(&right);
    left.AddBranch(&merge);
    right.AddBranch(&merge);
    auto& entryInfo = program.Metadata().blockInfo[0];
    entryInfo.terminator.kind = TerminatorKind::ConditionalBranch;
    entryInfo.terminator.trueBlock = 1u;
    entryInfo.terminator.falseBlock = 2u;
    for (std::uint32_t id = 1; id < 3u; ++id) {
        program.Metadata().blockInfo[id].terminator.kind = TerminatorKind::Branch;
        program.Metadata().blockInfo[id].terminator.trueBlock = 3u;
    }
    IrBuilder builder(program);
    builder.SetInsertionPoint(entry);
    auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    auto& limit = builder.Constant(16u);
    auto& condition = builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {&lane, &limit});
    entryInfo.condition = &condition;
    auto& one = builder.Constant(1u);
    auto& two = builder.Constant(2u);
    IrValue* first = &lane;
    IrValue* second = &lane;
    if (mode == 0u || mode == 1u) {
        builder.SetInsertionPoint(left);
        first = mode == 0u ? &builder.Emit(IrOpcode::SelectU32, IrType::U32, {&condition, &one, &two}) : &builder.Emit(IrOpcode::LaneId, IrType::U32, {});
        builder.SetInsertionPoint(right);
        second = mode == 0u ? &builder.Emit(IrOpcode::SelectU32, IrType::U32, {&condition, &one, &two}) : &builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    } else if (mode == 3u || mode == 4u) {
        first = &one;
        second = &program.CreateValue(IrOpcode::Void, IrType::U32);
        second->SetImmediateU32(mode == 3u ? 1u : 2u);
    }
    builder.SetInsertionPoint(merge);
    auto& phi = builder.Emit(IrOpcode::Phi, IrType::U32, {});
    phi.AddPhiOperand(&left, first);
    phi.AddPhiOperand(&right, second);
    auto& reference = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&phi});
    ValidateProgram(program, true);
    ConstantFolder{}.Fold(program);
    DeadCodeEliminator{}.RemoveIdentities(program);
    DeadCodeEliminator{}.Eliminate(program);
    ValidateProgram(program, true);
    const auto* result = reference.Argument(0)->Resolve();
    if (mode == 0u || mode == 1u || mode == 4u) {
        Require(result == &phi && phi.Argument(0) == first && phi.Argument(1) == second, "phi lost distinct branch definitions or constants");
    } else {
        Require(result == first, "phi did not fold identical definitions or equal constants");
    }
}

void CheckUnobservedPhiCycles() {
    IrProgram empty;
    DeadCodeEliminator{}.EliminateUnobservedPhis(empty);
    IrProgram program;
    for (std::uint32_t id = 0; id < 4u; ++id) {
        auto& block = program.CreateBlock();
        program.BlockOrder().push_back(&block);
        BlockInfo info;
        info.id = id;
        info.terminator.kind = TerminatorKind::Return;
        program.Metadata().blockInfo.push_back(info);
    }
    auto& entry = *program.BlockOrder()[0];
    auto& header = *program.BlockOrder()[1];
    auto& latch = *program.BlockOrder()[2];
    auto& exit = *program.BlockOrder()[3];
    program.SetEntryBlock(entry);
    entry.AddBranch(&header);
    header.AddBranch(&latch);
    header.AddBranch(&exit);
    latch.AddBranch(&header);
    program.Metadata().blockInfo[0].terminator.kind = TerminatorKind::Branch;
    program.Metadata().blockInfo[0].terminator.trueBlock = 1u;
    auto& headerInfo = program.Metadata().blockInfo[1];
    headerInfo.terminator.kind = TerminatorKind::ConditionalBranch;
    headerInfo.terminator.trueBlock = 2u;
    headerInfo.terminator.falseBlock = 3u;
    program.Metadata().blockInfo[2].terminator.kind = TerminatorKind::Branch;
    program.Metadata().blockInfo[2].terminator.trueBlock = 1u;
    IrBuilder builder(program);
    builder.SetInsertionPoint(entry);
    auto& zero = builder.Constant(0u);
    auto& producer = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    const auto phi = [&](IrBlock& block, IrType type = IrType::U32) -> IrValue& {
        auto& value = program.CreateValue(IrOpcode::Phi, type);
        block.AppendInstruction(&value);
        return value;
    };
    auto& first = phi(header);
    auto& second = phi(latch);
    auto& tail = phi(header);
    auto& self = phi(header);
    auto& liveFirst = phi(header);
    auto& liveSecond = phi(latch);
    auto& condition = phi(header, IrType::Bool);
    first.AddPhiOperand(&entry, &producer);
    first.AddPhiOperand(&latch, &second);
    second.AddPhiOperand(&header, &first);
    tail.AddPhiOperand(&entry, &zero);
    tail.AddPhiOperand(&latch, &second);
    self.AddPhiOperand(&entry, &zero);
    self.AddPhiOperand(&latch, &self);
    liveFirst.AddPhiOperand(&entry, &zero);
    liveFirst.AddPhiOperand(&latch, &liveSecond);
    liveSecond.AddPhiOperand(&header, &liveFirst);
    condition.AddPhiOperand(&entry, &builder.ConstantBool(true));
    condition.AddPhiOperand(&latch, &condition);
    headerInfo.condition = &condition;
    builder.SetInsertionPoint(header);
    static_cast<void>(builder.Emit(IrOpcode::Reference, IrType::Void, {&condition}));
    builder.SetInsertionPoint(latch);
    auto& store = builder.Emit(IrOpcode::StoreVectorWindow, IrType::Void, {&zero, &liveSecond});
    ValidateProgram(program, true);
    DeadCodeEliminator{}.EliminateUnobservedPhis(program);
    ValidateProgram(program, true);
    Require(first.Parent() == nullptr && second.Parent() == nullptr && tail.Parent() == nullptr && self.Parent() == nullptr, "phi cycles: an unobserved cycle or its tail survived");
    Require(!first.HasUses() && !second.HasUses() && !tail.HasUses() && !self.HasUses(), "phi cycles: a removed cycle retained uses");
    Require(liveFirst.Parent() == &header && liveSecond.Parent() == &latch && liveFirst.Argument(1) == &liveSecond && liveSecond.Argument(0) == &liveFirst, "phi cycles: an observed cycle was changed");
    Require(store.Parent() == &latch && store.Argument(1) == &liveSecond, "phi cycles: an observable store was changed");
    Require(condition.Parent() == &header && headerInfo.condition == &condition && condition.Argument(1) == &condition, "phi cycles: a branch condition was changed");
    DeadCodeEliminator{}.Eliminate(program);
    Require(producer.Parent() == nullptr && !producer.HasUses(), "phi cycles: ordinary DCE retained a released producer");
    DeadCodeEliminator{}.EliminateUnobservedPhis(program);
    ValidateProgram(program, true);
    Require(entry.Instructions().empty() && header.Instructions().size() == 3u && latch.Instructions().size() == 2u, "phi cycles: repeated elimination changed the live graph");
}

void CheckLongAncillaryForwarding() {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.BlockOrder().push_back(&block);
    BlockInfo info;
    info.id = 0u;
    info.terminator.kind = TerminatorKind::Return;
    program.Metadata().blockInfo.push_back(info);
    program.SetEntryBlock(block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& ancillary = builder.Emit(IrOpcode::GetBuiltin, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(StageInputKind::PackedAncillary)), &builder.Constant(0u)});
    auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    auto& condition = builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {&lane, &builder.Constant(16u)});
    auto& zero = builder.Constant(0u);
    IrValue* forwarded = &ancillary;
    for (std::uint32_t link = 0; link < 20000u; ++link) forwarded = &builder.Emit(IrOpcode::SelectU32, IrType::U32, {&condition, &zero, forwarded});
    auto& field = builder.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {forwarded, &builder.Constant(8u), &builder.Constant(4u)});
    static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&field}));
    struct Run {
        IrProgram& program;
        std::string error;
    } run{program, {}};
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, 256u * 1024u);
    pthread_t thread;
    const auto body = +[](void* argument) -> void* {
        auto& state = *static_cast<Run*>(argument);
        try {
            ConstantFolder{}.Fold(state.program);
        } catch (const std::exception& error) {
            state.error = error.what();
        }
        return nullptr;
    };
    Require(pthread_create(&thread, &attributes, body, &run) == 0, "long ancillary forwarding: cannot start the folding thread");
    pthread_join(thread, nullptr);
    pthread_attr_destroy(&attributes);
    Require(run.error.empty(), "long ancillary forwarding: folding failed");
}

}

int main() {
    try {
        for (unsigned mode = 0; mode < 5u; ++mode) CheckDiamond(mode);
        CheckLongAncillaryForwarding();
        CheckUnobservedPhiCycles();
        std::puts("constant phi dominance tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
