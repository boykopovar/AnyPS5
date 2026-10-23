#include "IntermediateRepresentation/IrProgram.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/MaskedSelectEliminator.hpp"
#include "StencilReplay.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <functional>
#include <initializer_list>
#include <utility>
#include <vector>

using namespace ShaderRecompiler;

namespace {

struct Builder {
    IrProgram program;
    IrBlock* block = nullptr;

    Builder() {
        block = &newBlock();
        program.SetEntryBlock(*block);
    }

    IrBlock& newBlock() {
        auto& created = program.CreateBlock();
        program.BlockOrder().push_back(&created);
        program.Metadata().blockInfo.push_back(BlockInfo{created.Id()});
        return created;
    }

    IrValue& constant(std::uint32_t value) {
        auto& created = program.CreateValue(IrOpcode::Void, IrType::U32);
        created.SetImmediateU32(value);
        return created;
    }

    IrValue& truth() {
        auto& created = program.CreateValue(IrOpcode::Void, IrType::Bool);
        created.SetImmediateBool(true);
        return created;
    }

    IrValue& emit(IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments, std::uint64_t flags = 0) {
        auto& created = program.CreateValue(opcode, type, flags);
        for (auto* argument : arguments) created.AddArgument(argument);
        block->AppendInstruction(&created);
        return created;
    }

    IrValue& lane() {
        return emit(IrOpcode::LaneId, IrType::U32, {});
    }

    IrValue& mask(std::uint32_t lanes) {
        return emit(IrOpcode::ULessThan32, IrType::Bool, {&lane(), &constant(lanes)});
    }

    IrValue& select(IrValue& condition, IrValue& active, IrValue& inactive) {
        return emit(IrOpcode::SelectU32, IrType::U32, {&condition, &active, &inactive});
    }

    IrValue& add(IrValue& value, std::uint32_t immediate) {
        return emit(IrOpcode::IAdd32, IrType::U32, {&value, &constant(immediate)});
    }

    IrValue& logicalAnd(IrValue& left, IrValue& right) {
        return emit(IrOpcode::LogicalAnd, IrType::Bool, {&left, &right});
    }

    IrValue& threadBit(IrValue& low, IrValue& high) {
        auto& laneId = lane();
        auto& word = select(emit(IrOpcode::ULessThan32, IrType::Bool, {&laneId, &constant(32u)}), low, high);
        auto& shift = emit(IrOpcode::BitwiseAnd32, IrType::U32, {&laneId, &constant(31u)});
        auto& shifted = emit(IrOpcode::ShiftRightLogical32, IrType::U32, {&word, &shift});
        auto& bit = emit(IrOpcode::BitwiseAnd32, IrType::U32, {&shifted, &constant(1u)});
        return emit(IrOpcode::INotEqual32, IrType::Bool, {&bit, &constant(0u)});
    }

    IrValue& ballotBit(IrValue& predicate) {
        auto& ballot = emit(IrOpcode::Ballot, IrType::U32x4, {&predicate});
        auto& low = emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &constant(0u)});
        auto& high = emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &constant(1u)});
        return threadBit(low, high);
    }

    std::pair<IrValue*, IrValue*> ballotWords(IrValue& predicate) {
        auto& ballot = emit(IrOpcode::Ballot, IrType::U32x4, {&predicate});
        return {&emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &constant(0u)}), &emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &constant(1u)})};
    }

    IrValue& wordAnd(IrValue& left, IrValue& right) {
        return emit(IrOpcode::BitwiseAnd32, IrType::U32, {&left, &right});
    }

    IrValue& phi(IrType type, IrBlock& predecessor, IrValue& incoming) {
        auto& created = program.CreateValue(IrOpcode::Phi, type);
        block->AppendInstruction(&created);
        created.AddPhiOperand(&predecessor, &incoming);
        return created;
    }

    void keep(IrValue& value) {
        (void)emit(IrOpcode::ReferenceU32, IrType::Void, {&value});
    }

    std::uint32_t eliminate() {
        return MaskedSelectEliminator{}.Eliminate(program).removedSelects;
    }
};

bool removed(const IrValue& value) {
    return value.Parent() == nullptr;
}

bool expect(const char* name, bool condition) {
    if (!condition) std::fprintf(stderr, "%s\n", name);
    return condition;
}

bool messageSafety(IrShaderStage stage, bool malformed = false, bool allocate = false) {
    IrProgram program;
    program.Resources().stage = stage;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    if (allocate) static_cast<void>(builder.Emit(IrOpcode::MeshAllocate, IrType::Void, {&builder.Constant(0u)}));
    else if (malformed) {
        auto& message = program.CreateValue(IrOpcode::Sendmsg, IrType::Void);
        message.AddArgument(&builder.Constant(0u));
        block.AppendInstruction(&message);
    }
    else static_cast<void>(builder.Emit(IrOpcode::Sendmsg, IrType::Void, {}));
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    return StencilReplaySafe(program);
}

bool run(const char* name, const std::function<bool()>& test) {
    try {
        return expect(name, test());
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: %s\n", name, error.what());
        return false;
    }
}

}

int main() {
    bool passed = true;

    passed &= run("a write read only under its own exec must lose its select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& next = b.select(exec, b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() == 1u && removed(written) && !removed(next);
    });

    passed &= run("a write read under a wider exec must keep its select", [] {
        Builder b;
        auto& wide = b.mask(32u);
        auto& exec = b.logicalAnd(wide, b.mask(16u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& next = b.select(wide, b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() == 0u && !removed(written);
    });

    passed &= run("a write read under a narrower exec must lose its select", [] {
        Builder b;
        auto& exec = b.mask(32u);
        auto& narrow = b.logicalAnd(exec, b.mask(16u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& next = b.select(narrow, b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() == 1u && removed(written);
    });

    passed &= run("a write whose old lanes flow into the next write must keep its select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& next = b.select(exec, b.add(old, 2u), written);
        b.keep(next);
        return b.eliminate() == 0u && !removed(written);
    });

    passed &= run("a chain of writes under one exec must collapse once the last one goes", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& first = b.select(exec, b.add(old, 1u), old);
        auto& second = b.select(exec, b.add(first, 2u), first);
        auto& sink = b.select(exec, b.add(second, 3u), old);
        b.keep(sink);
        return b.eliminate() == 2u && removed(first) && removed(second) && !removed(sink);
    });

    passed &= run("a write read through thousands of lane-local values still loses its select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        IrValue* value = &written;
        for (std::uint32_t step = 0; step < 6000u; ++step) value = &b.add(*value, step);
        b.keep(b.select(exec, *value, old));
        return b.eliminate() == 1u && removed(written);
    });

    passed &= run("thousands of lane-local values ending in a wider read keep the select", [] {
        Builder b;
        auto& wide = b.mask(32u);
        auto& exec = b.logicalAnd(wide, b.mask(16u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        IrValue* value = &written;
        for (std::uint32_t step = 0; step < 6000u; ++step) value = &b.add(*value, step);
        b.keep(b.select(wide, *value, old));
        return b.eliminate() == 0u && !removed(written);
    });

    passed &= run("cross-lane reads must keep the select", [] {
        bool ok = true;
        for (const IrOpcode opcode : {IrOpcode::DppMoveU32, IrOpcode::BpermuteU32, IrOpcode::Permlane16U32, IrOpcode::SwizzleU32}) {
            Builder b;
            auto& exec = b.mask(16u);
            auto& old = b.lane();
            auto& written = b.select(exec, b.add(old, 1u), old);
            auto& read = b.emit(opcode, IrType::U32, {&written, &exec});
            b.keep(b.select(exec, read, old));
            ok &= b.eliminate() == 0u && !removed(written);
        }
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& ballot = b.emit(IrOpcode::Ballot, IrType::U32x4, {&b.emit(IrOpcode::INotEqual32, IrType::Bool, {&written, &b.constant(0u)})});
        b.keep(b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &b.constant(0u)}));
        return ok && b.eliminate() == 0u && !removed(written);
    });

    passed &= run("a first-lane read guarded at both ends loses the source select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& narrow = b.logicalAnd(exec, b.mask(8u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& read = b.emit(IrOpcode::ReadFirstLane, IrType::U32, {&written, &narrow});
        b.keep(b.select(narrow, read, old));
        return b.eliminate() == 1u && removed(written);
    });

    passed &= run("an unguarded first-lane result keeps the source select for empty exec", [] {
        Builder b;
        auto& exec = b.emit(IrOpcode::INotEqual32, IrType::Bool, {&b.lane(), &b.constant(0u)});
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& empty = b.logicalAnd(exec, b.mask(0u));
        b.keep(b.emit(IrOpcode::ReadFirstLane, IrType::U32, {&written, &empty}));
        return b.eliminate() == 0u && !removed(written);
    });

    passed &= run("a first-lane result read under the write mask keeps the select when read exec is narrower", [] {
        Builder b;
        auto& exec = b.emit(IrOpcode::INotEqual32, IrType::Bool, {&b.lane(), &b.constant(0u)});
        auto& empty = b.logicalAnd(exec, b.mask(0u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& read = b.emit(IrOpcode::ReadFirstLane, IrType::U32, {&written, &empty});
        b.keep(b.select(exec, read, old));
        return b.eliminate() == 0u && !removed(written);
    });

    passed &= run("a first-lane read under a wider exec keeps the select", [] {
        Builder b;
        auto& wide = b.mask(32u);
        auto& exec = b.logicalAnd(wide, b.mask(16u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& read = b.emit(IrOpcode::ReadFirstLane, IrType::U32, {&written, &wide});
        b.keep(b.select(wide, read, old));
        return b.eliminate() == 0u && !removed(written);
    });

    const auto selectedExec = [](bool negate, bool widerArm) {
        Builder b;
        auto& wide = b.mask(32u);
        auto& exec = b.logicalAnd(wide, b.mask(16u));
        auto& empty = b.logicalAnd(exec, b.mask(0u));
        auto& selector = negate ? b.emit(IrOpcode::LogicalNot, IrType::Bool, {&exec}) : exec;
        auto& later = b.emit(IrOpcode::SelectU1, IrType::Bool, {&selector, negate ? (widerArm ? &wide : &empty) : &wide, negate ? &wide : (widerArm ? &wide : &empty)});
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        b.keep(b.select(later, b.add(written, 2u), old));
        return std::pair{b.eliminate(), removed(written)};
    };

    passed &= run("selected boolean exec implies the write mask through its selector", [&] {
        for (bool negate : {false, true}) {
            const auto [count, gone] = selectedExec(negate, false);
            if (count != 1u || !gone) return false;
        }
        return true;
    });

    passed &= run("selected boolean exec with a wider unguarded arm keeps the select", [&] {
        for (bool negate : {false, true}) {
            const auto [count, gone] = selectedExec(negate, true);
            if (count != 0u || gone) return false;
        }
        return true;
    });

    const auto rebuiltExec = [](bool mismatchedHigh) {
        Builder b;
        const auto [low, high] = b.ballotWords(b.mask(16u));
        const auto [wideLow, wideHigh] = b.ballotWords(b.mask(48u));
        auto& exec = b.threadBit(*low, *high);
        auto& later = b.threadBit(*low, mismatchedHigh ? *wideHigh : *high);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        b.keep(b.select(later, b.add(written, 2u), old));
        return std::pair{b.eliminate(), removed(written)};
    };

    passed &= run("an exec rebuilt from the same words implies the write mask", [&] {
        const auto [count, gone] = rebuiltExec(false);
        return count >= 1u && gone;
    });

    passed &= run("matching low mask words alone do not imply a wave64 write mask", [&] {
        const auto [count, gone] = rebuiltExec(true);
        return count == 0u && !gone;
    });

    const auto firstLaneInLoop = [](bool carriedResult) {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        IrBlock& entry = *b.block;
        IrBlock& loop = b.newBlock();
        entry.AddBranch(&loop);
        loop.AddBranch(&loop);
        b.block = &loop;
        auto& loopExec = b.logicalAnd(exec, b.mask(8u));
        auto& read = b.emit(IrOpcode::ReadFirstLane, IrType::U32, {&written, &loopExec});
        if (carriedResult) {
            auto& carried = b.phi(IrType::U32, entry, old);
            carried.AddPhiOperand(&loop, &read);
            b.keep(b.select(loopExec, carried, old));
        } else {
            b.keep(b.select(loopExec, read, old));
        }
        return std::pair{b.eliminate(), removed(written)};
    };

    passed &= run("a loop-local first-lane result guarded in its own iteration loses the source select", [&] {
        const auto [count, gone] = firstLaneInLoop(false);
        return count == 1u && gone;
    });

    passed &= run("a first-lane result carried to another loop iteration keeps the source select", [&] {
        const auto [count, gone] = firstLaneInLoop(true);
        return count == 0u && !gone;
    });

    passed &= run("a branch condition derived from the write keeps the select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& taken = b.emit(IrOpcode::INotEqual32, IrType::Bool, {&written, &b.constant(0u)});
        (void)b.emit(IrOpcode::Reference, IrType::Void, {&taken});
        return b.eliminate() == 0u && !removed(written);
    });

    passed &= run("a compare masked by the same exec must not keep the select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& compare = b.logicalAnd(exec, b.emit(IrOpcode::INotEqual32, IrType::Bool, {&written, &b.constant(0u)}));
        auto& ballot = b.emit(IrOpcode::Ballot, IrType::U32x4, {&compare});
        b.keep(b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &b.constant(0u)}));
        return b.eliminate() == 1u && removed(written);
    });

    passed &= run("an exec rebuilt from a ballot of a narrower mask implies the write's exec", [] {
        Builder b;
        auto& exec = b.mask(48u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& later = b.ballotBit(b.logicalAnd(exec, b.mask(40u)));
        auto& next = b.select(later, b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() == 1u && removed(written);
    });

    passed &= run("an exec rebuilt from a ballot of a wider mask keeps the select", [] {
        Builder b;
        auto& wide = b.mask(48u);
        auto& exec = b.logicalAnd(wide, b.mask(40u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& later = b.ballotBit(wide);
        b.keep(b.select(later, b.add(written, 2u), old));
        return b.eliminate() == 0u && !removed(written);
    });

    passed &= run("a select under the full wave's exec must go whatever reads it", [] {
        Builder b;
        auto& full = b.threadBit(b.constant(0xffffffffu), b.constant(0xffffffffu));
        auto& old = b.lane();
        auto& written = b.select(full, b.add(old, 1u), old);
        b.keep(written);
        return b.eliminate() != 0u && removed(written);
    });

    passed &= run("a partial wave's exec is not the full wave", [] {
        Builder b;
        auto& partial = b.threadBit(b.constant(0xffffffffu), b.constant(0x0000ffffu));
        auto& old = b.lane();
        auto& written = b.select(partial, b.add(old, 1u), old);
        b.keep(written);
        return b.eliminate() == 0u && !removed(written);
    });

    const auto diamond = [](bool wideRead) {
        Builder b;
        auto& wide = b.mask(48u);
        auto& exec = b.logicalAnd(wide, b.mask(16u));
        auto& old = b.lane();
        IrBlock& head = *b.block;
        IrBlock& body = b.newBlock();
        IrBlock& merge = b.newBlock();
        head.AddBranch(&body);
        head.AddBranch(&merge);
        body.AddBranch(&merge);
        b.block = &body;
        auto& written = b.select(exec, b.add(old, 1u), old);
        b.block = &merge;
        auto& phi = b.program.CreateValue(IrOpcode::Phi, IrType::U32);
        merge.AppendInstruction(&phi);
        phi.AddPhiOperand(&head, &old);
        phi.AddPhiOperand(&body, &written);
        b.keep(b.select(wideRead ? wide : exec, b.add(phi, 2u), old));
        return std::pair{b.eliminate(), removed(written)};
    };

    passed &= run("a write merged by a phi and read under its own exec must lose its select", [&] {
        const auto [count, gone] = diamond(false);
        return count == 1u && gone;
    });

    passed &= run("a write merged by a phi and read under the wider exec must keep its select", [&] {
        const auto [count, gone] = diamond(true);
        return count == 0u && !gone;
    });

    const auto mergedExec = [](bool wideBranch, bool loopMerge) {
        Builder b;
        auto& wide = b.mask(48u);
        auto& exec = b.logicalAnd(wide, b.mask(32u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        IrBlock& head = *b.block;
        IrBlock& body = b.newBlock();
        IrBlock& merge = b.newBlock();
        head.AddBranch(&body);
        head.AddBranch(&merge);
        body.AddBranch(&merge);
        b.block = &body;
        auto& killed = b.logicalAnd(wideBranch ? wide : exec, b.mask(16u));
        auto& phi = b.program.CreateValue(IrOpcode::Phi, IrType::Bool);
        merge.AppendInstruction(&phi);
        phi.AddPhiOperand(&head, &exec);
        phi.AddPhiOperand(&body, &killed);
        if (loopMerge) {
            IrBlock& exit = b.newBlock();
            merge.AddBranch(&merge);
            merge.AddBranch(&exit);
            phi.AddPhiOperand(&merge, &phi);
            b.block = &exit;
        } else {
            b.block = &merge;
        }
        b.keep(b.select(phi, b.add(written, 2u), old));
        return std::pair{b.eliminate(), removed(written)};
    };

    passed &= run("an exec merged by a phi whose every incoming exec implies the write's exec drops the select", [&] {
        const auto [count, gone] = mergedExec(false, false);
        return count == 1u && gone;
    });

    passed &= run("an exec merged by a phi with a wider incoming exec keeps the select", [&] {
        const auto [count, gone] = mergedExec(true, false);
        return count == 0u && !gone;
    });

    passed &= run("an exec carried around a loop whose every incoming exec implies the write's exec drops the select", [&] {
        const auto [count, gone] = mergedExec(false, true);
        return count == 1u && gone;
    });

    passed &= run("an exec carried around a loop with a wider incoming exec keeps the select", [&] {
        const auto [count, gone] = mergedExec(true, true);
        return count == 0u && !gone;
    });

    const auto mergedWords = [](bool wideBranch) {
        Builder b;
        auto& wide = b.mask(48u);
        auto& exec = b.logicalAnd(wide, b.mask(40u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& saved = b.emit(IrOpcode::Ballot, IrType::U32x4, {&exec});
        auto& savedLow = b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&saved, &b.constant(0u)});
        auto& savedHigh = b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&saved, &b.constant(1u)});
        IrBlock& head = *b.block;
        IrBlock& body = b.newBlock();
        IrBlock& merge = b.newBlock();
        head.AddBranch(&body);
        head.AddBranch(&merge);
        body.AddBranch(&merge);
        b.block = &body;
        auto& survived = b.emit(IrOpcode::Ballot, IrType::U32x4, {&b.logicalAnd(wideBranch ? wide : exec, b.mask(24u))});
        auto& survivedLow = b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&survived, &b.constant(0u)});
        auto& survivedHigh = b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&survived, &b.constant(1u)});
        auto& low = b.program.CreateValue(IrOpcode::Phi, IrType::U32);
        auto& high = b.program.CreateValue(IrOpcode::Phi, IrType::U32);
        merge.AppendInstruction(&low);
        merge.AppendInstruction(&high);
        low.AddPhiOperand(&head, &savedLow);
        low.AddPhiOperand(&body, &survivedLow);
        high.AddPhiOperand(&head, &savedHigh);
        high.AddPhiOperand(&body, &survivedHigh);
        b.block = &merge;
        b.keep(b.select(b.threadBit(low, high), b.add(written, 2u), old));
        return std::pair{b.eliminate(), removed(written)};
    };

    passed &= run("an exec rebuilt from mask words merged by phis implies the write's exec", [&] {
        const auto [count, gone] = mergedWords(false);
        return count == 1u && gone;
    });

    passed &= run("an exec rebuilt from merged mask words with a wider incoming ballot keeps the select", [&] {
        const auto [count, gone] = mergedWords(true);
        return count == 0u && !gone;
    });

    const auto chained = [](bool wideRoot, std::uint32_t kills) {
        Builder b;
        auto& wide = b.mask(48u);
        auto& exec = b.logicalAnd(wide, b.mask(32u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        IrValue* later = wideRoot ? &wide : &exec;
        for (std::uint32_t kill = 0; kill < kills; ++kill) later = &b.logicalAnd(*later, b.mask(31u - kill % 24u));
        b.keep(b.select(*later, b.add(written, 2u), old));
        return std::pair{b.eliminate(), removed(written)};
    };

    passed &= run("an exec narrowed by a long chain of kills still implies the write's exec", [&] {
        const auto [count, gone] = chained(false, 24u);
        return count == 1u && gone;
    });

    passed &= run("a long chain of kills rooted at a wider exec keeps the select", [&] {
        const auto [count, gone] = chained(true, 24u);
        return count == 0u && !gone;
    });

    passed &= run("an exec narrowed by hundreds of kills still implies the write's exec", [&] {
        const auto [count, gone] = chained(false, 300u);
        return count == 1u && gone;
    });

    passed &= run("hundreds of kills rooted at a wider exec keep the select", [&] {
        const auto [count, gone] = chained(true, 300u);
        return count == 0u && !gone;
    });

    passed &= run("a write in a loop must keep its select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        IrBlock& head = *b.block;
        IrBlock& loop = b.newBlock();
        IrBlock& exit = b.newBlock();
        head.AddBranch(&loop);
        loop.AddBranch(&loop);
        loop.AddBranch(&exit);
        b.block = &loop;
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& next = b.select(exec, b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() == 0u && !removed(written);
    });

    const auto laterLoop = [](bool wideRead) {
        Builder b;
        auto& wide = b.mask(48u);
        auto& exec = b.logicalAnd(wide, b.mask(16u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        IrBlock& head = *b.block;
        IrBlock& loop = b.newBlock();
        head.AddBranch(&loop);
        loop.AddBranch(&loop);
        b.block = &loop;
        b.keep(b.select(wideRead ? wide : exec, b.add(written, 2u), old));
        return std::pair{b.eliminate(), removed(written)};
    };

    passed &= run("a write read inside a later loop under its own exec loses its select", [&] {
        const auto [count, gone] = laterLoop(false);
        return count == 1u && gone;
    });

    passed &= run("a write read inside a later loop under a wider exec keeps its select", [&] {
        const auto [count, gone] = laterLoop(true);
        return count == 0u && !gone;
    });

    enum class Latch { Narrowed, Revived };
    const auto carried = [](Latch latch, bool readInside) {
        Builder b;
        auto& wide = b.mask(48u);
        auto& exec = b.logicalAnd(wide, b.mask(40u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        const auto [savedLow, savedHigh] = b.ballotWords(exec);
        IrBlock& entry = *b.block;
        IrBlock& header = b.newBlock();
        IrBlock& exit = b.newBlock();
        entry.AddBranch(&header);
        header.AddBranch(&header);
        header.AddBranch(&exit);
        b.block = &header;
        auto& low = b.phi(IrType::U32, entry, *savedLow);
        auto& high = b.phi(IrType::U32, entry, *savedHigh);
        auto& value = b.phi(IrType::U32, entry, written);
        auto& loopExec = b.threadBit(low, high);
        auto& next = b.select(loopExec, b.add(value, 2u), value);
        if (readInside) b.keep(b.select(loopExec, b.add(value, 3u), old));
        if (latch == Latch::Narrowed) {
            const auto [stayLow, stayHigh] = b.ballotWords(b.mask(24u));
            low.AddPhiOperand(&header, &b.wordAnd(low, *stayLow));
            high.AddPhiOperand(&header, &b.wordAnd(high, *stayHigh));
        } else {
            const auto [revivedLow, revivedHigh] = b.ballotWords(wide);
            low.AddPhiOperand(&header, revivedLow);
            high.AddPhiOperand(&header, revivedHigh);
        }
        value.AddPhiOperand(&header, &next);
        b.block = &exit;
        b.keep(b.select(b.threadBit(*savedLow, *savedHigh), b.add(value, 4u), old));
        return std::pair{b.eliminate(), removed(written)};
    };

    passed &= run("a write carried through a loop whose exec is saved, narrowed and restored loses its select", [&] {
        const auto [count, gone] = carried(Latch::Narrowed, false);
        return count == 1u && gone;
    });

    passed &= run("a write read inside a loop under the loop's narrowed exec loses its select", [&] {
        const auto [count, gone] = carried(Latch::Narrowed, true);
        return count == 1u && gone;
    });

    passed &= run("a lane that becomes active again inside a loop and reads the write keeps the select", [&] {
        const auto [count, gone] = carried(Latch::Revived, true);
        return count == 0u && !gone;
    });

    passed &= run("a lane that becomes active again inside a loop but never reads the write lets the select go", [&] {
        const auto [count, gone] = carried(Latch::Revived, false);
        return count == 1u && gone;
    });

    passed &= run("an exec proven only by assuming a loop phi is forgotten once that phi turns out wider", [] {
        Builder b;
        auto& wide = b.mask(48u);
        auto& exec = b.logicalAnd(wide, b.mask(40u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        const auto [savedLow, savedHigh] = b.ballotWords(exec);
        IrBlock& entry = *b.block;
        IrBlock& header = b.newBlock();
        IrBlock& kill = b.newBlock();
        IrBlock& revive = b.newBlock();
        IrBlock& latch = b.newBlock();
        IrBlock& exit = b.newBlock();
        entry.AddBranch(&header);
        header.AddBranch(&kill);
        header.AddBranch(&revive);
        kill.AddBranch(&latch);
        revive.AddBranch(&latch);
        latch.AddBranch(&header);
        latch.AddBranch(&exit);
        b.block = &header;
        auto& low = b.phi(IrType::U32, entry, *savedLow);
        auto& high = b.phi(IrType::U32, entry, *savedHigh);
        (void)b.select(b.threadBit(low, high), written, old);
        b.block = &kill;
        const auto [stayLow, stayHigh] = b.ballotWords(b.mask(24u));
        auto& killedLow = b.wordAnd(low, *stayLow);
        auto& killedHigh = b.wordAnd(high, *stayHigh);
        b.keep(b.select(b.threadBit(killedLow, killedHigh), written, old));
        b.block = &revive;
        const auto [revivedLow, revivedHigh] = b.ballotWords(wide);
        b.block = &latch;
        auto& mergedLow = b.phi(IrType::U32, kill, killedLow);
        auto& mergedHigh = b.phi(IrType::U32, kill, killedHigh);
        mergedLow.AddPhiOperand(&revive, revivedLow);
        mergedHigh.AddPhiOperand(&revive, revivedHigh);
        low.AddPhiOperand(&latch, &mergedLow);
        high.AddPhiOperand(&latch, &mergedHigh);
        return b.eliminate() == 0u && !removed(written);
    });

    passed &= run("a write mask computed inside an earlier loop does not let that loop's phis stand for it", [] {
        Builder b;
        auto& old = b.lane();
        IrBlock& entry = *b.block;
        IrBlock& header = b.newBlock();
        IrBlock& exit = b.newBlock();
        entry.AddBranch(&header);
        header.AddBranch(&header);
        header.AddBranch(&exit);
        b.block = &header;
        auto& low = b.phi(IrType::U32, entry, b.constant(0u));
        auto& high = b.phi(IrType::U32, entry, b.constant(0u));
        auto& bound = b.phi(IrType::U32, entry, b.constant(64u));
        auto& exec = b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.lane(), &bound});
        const auto [stayLow, stayHigh] = b.ballotWords(exec);
        low.AddPhiOperand(&header, stayLow);
        high.AddPhiOperand(&header, stayHigh);
        bound.AddPhiOperand(&header, &b.emit(IrOpcode::ISub32, IrType::U32, {&bound, &b.constant(8u)}));
        b.block = &exit;
        auto& written = b.select(exec, b.add(old, 1u), old);
        b.keep(b.select(b.threadBit(low, high), b.add(written, 2u), old));
        return b.eliminate() == 0u && !removed(written);
    });

    const auto wholeQuads = [](Builder& b, IrValue& live) -> IrValue& {
        auto& ballot = b.emit(IrOpcode::Ballot, IrType::U32x4, {&live});
        auto& allLow = b.emit(IrOpcode::Ballot, IrType::U32x4, {&b.truth()});
        auto& allHigh = b.emit(IrOpcode::Ballot, IrType::U32x4, {&b.truth()});
        auto& low = b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &b.constant(0u)}), &b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&allLow, &b.constant(0u)})});
        auto& high = b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &b.constant(1u)}), &b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&allHigh, &b.constant(1u)})});
        auto& wqm = b.emit(IrOpcode::WqmU64, IrType::U64, {&b.emit(IrOpcode::CompositeConstructU64, IrType::U64, {&low, &high})});
        return b.threadBit(b.emit(IrOpcode::CompositeExtractU64, IrType::U32, {&wqm, &b.constant(0u)}), b.emit(IrOpcode::CompositeExtractU64, IrType::U32, {&wqm, &b.constant(1u)}));
    };

    const auto sample = [&](IrShaderStage stage, std::uint32_t sampleFlags, IrOpcode opcode = IrOpcode::ImageSampleRaw, bool wholeQuadExec = false) {
        Builder b;
        b.program.Resources().stage = stage;
        MemoryInfo memory;
        memory.kind = ResourceKind::Image;
        memory.imageSampleFlags = sampleFlags;
        b.program.Resources().memoryInfo.push_back(memory);
        auto& exec = wholeQuadExec ? wholeQuads(b, b.mask(16u)) : b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& address = b.emit(IrOpcode::MakeImageAddress, IrType::ImageAddress, {&written, &old});
        auto& image = b.emit(IrOpcode::GetImageResource, IrType::ImageResource, {});
        auto& sampler = b.emit(IrOpcode::GetSamplerResource, IrType::SamplerResource, {});
        MemoryFlags flags{0u, 0u};
        std::uint64_t bits = 0;
        std::memcpy(&bits, &flags, sizeof(flags));
        auto& sampled = b.emit(opcode, IrType::U32x4, {&image, &sampler, &address}, bits);
        b.keep(b.select(exec, b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&sampled, &b.constant(0u)}), old));
        b.eliminate();
        return removed(written);
    };

    passed &= run("a vertex message emitted as void is replay-safe", [&] { return messageSafety(IrShaderStage::Vertex); });
    passed &= run("pixel message classification remains unchanged", [&] { return !messageSafety(IrShaderStage::Pixel); });
    passed &= run("compute message classification remains unchanged", [&] { return !messageSafety(IrShaderStage::Compute); });
    passed &= run("a message with operands is refused", [&] { return !messageSafety(IrShaderStage::Vertex, true); });
    passed &= run("mesh allocation remains refused", [&] { return !messageSafety(IrShaderStage::Vertex, false, true); });

    passed &= run("guard implication preserves a mask within its whole-quad expansion", [&] {
        Builder b;
        auto& live = b.mask(16u);
        auto& quads = wholeQuads(b, live);
        return GuardImplies(b.program, &live, &quads) && !GuardImplies(b.program, &quads, &live);
    });

    passed &= run("guard implication intersects both paths through a phi", [&] {
        Builder b;
        auto& entry = *b.block;
        auto& live = b.mask(16u);
        auto& quads = wholeQuads(b, live);
        auto& left = b.newBlock();
        auto& right = b.newBlock();
        auto& merge = b.newBlock();
        entry.AddBranch(&left);
        entry.AddBranch(&right);
        left.AddBranch(&merge);
        right.AddBranch(&merge);
        b.block = &merge;
        auto& subset = b.phi(IrType::Bool, left, live);
        subset.AddPhiOperand(&right, &b.logicalAnd(live, b.mask(8u)));
        auto& wider = b.phi(IrType::Bool, left, live);
        wider.AddPhiOperand(&right, &b.truth());
        return GuardImplies(b.program, &subset, &quads) && !GuardImplies(b.program, &wider, &quads);
    });

    passed &= run("guard implication handles whole-quad masks intersected with own active ballots", [&] {
        Builder b;
        auto& live = b.mask(16u);
        const auto [low, high] = b.ballotWords(live);
        auto& wqm = b.emit(IrOpcode::WqmU64, IrType::U64, {&b.emit(IrOpcode::CompositeConstructU64, IrType::U64, {low, high})});
        auto& wideLow = b.emit(IrOpcode::CompositeExtractU64, IrType::U32, {&wqm, &b.constant(0u)});
        auto& wideHigh = b.emit(IrOpcode::CompositeExtractU64, IrType::U32, {&wqm, &b.constant(1u)});
        const auto [allLow, allHigh] = b.ballotWords(b.truth());
        auto& guard = b.threadBit(b.wordAnd(wideLow, *allLow), b.wordAnd(*allHigh, wideHigh));
        auto& narrower = b.logicalAnd(live, b.mask(8u));
        return GuardImplies(b.program, &live, &guard) && GuardImplies(b.program, &narrower, &guard) && !GuardImplies(b.program, &guard, &live);
    });

    passed &= run("a paired mask phi with a wider incoming arm does not imply a narrower write mask", [] {
        Builder b;
        auto& entry = *b.block;
        auto& live = b.mask(16u);
        const auto [low, high] = b.ballotWords(live);
        auto& left = b.newBlock();
        auto& right = b.newBlock();
        auto& merge = b.newBlock();
        entry.AddBranch(&left);
        entry.AddBranch(&right);
        left.AddBranch(&merge);
        right.AddBranch(&merge);
        b.block = &merge;
        auto& lowPhi = b.phi(IrType::U32, left, *low);
        lowPhi.AddPhiOperand(&right, &b.constant(0xffffffffu));
        auto& highPhi = b.phi(IrType::U32, left, *high);
        highPhi.AddPhiOperand(&right, &b.constant(0xffffffffu));
        auto& merged = b.threadBit(lowPhi, highPhi);
        return !GuardImplies(b.program, &merged, &live);
    });

    passed &= run("a reconverged active ballot dominates a later mask observation", [] {
        Builder b;
        auto& entry = *b.block;
        auto& left = b.newBlock();
        auto& right = b.newBlock();
        auto& merge = b.newBlock();
        auto& exit = b.newBlock();
        entry.AddBranch(&left);
        entry.AddBranch(&right);
        left.AddBranch(&merge);
        right.AddBranch(&merge);
        merge.AddBranch(&exit);
        b.block = &merge;
        auto& active = b.ballotBit(b.truth());
        b.block = &exit;
        auto& observed = b.mask(16u);
        return GuardImplies(b.program, &observed, &active);
    });

    passed &= run("an active ballot from one branch does not dominate a merged observation", [] {
        Builder b;
        auto& entry = *b.block;
        auto& left = b.newBlock();
        auto& right = b.newBlock();
        auto& merge = b.newBlock();
        entry.AddBranch(&left);
        entry.AddBranch(&right);
        left.AddBranch(&merge);
        right.AddBranch(&merge);
        b.block = &left;
        auto& active = b.ballotBit(b.truth());
        b.block = &merge;
        auto& observed = b.mask(16u);
        return !GuardImplies(b.program, &observed, &active);
    });

    passed &= run("a later ballot in a reconverged block cannot justify an earlier observation", [] {
        Builder b;
        auto& entry = *b.block;
        auto& left = b.newBlock();
        auto& right = b.newBlock();
        auto& merge = b.newBlock();
        entry.AddBranch(&left);
        entry.AddBranch(&right);
        left.AddBranch(&merge);
        right.AddBranch(&merge);
        b.block = &merge;
        auto& observed = b.mask(16u);
        auto& active = b.ballotBit(b.truth());
        return !GuardImplies(b.program, &observed, &active);
    });

    passed &= run("guard implication refuses a loop-carried condition", [] {
        Builder b;
        auto& entry = *b.block;
        auto& header = b.newBlock();
        entry.AddBranch(&header);
        header.AddBranch(&header);
        b.block = &header;
        auto& condition = b.mask(8u);
        auto& consequence = b.mask(16u);
        return !GuardImplies(b.program, &condition, &consequence);
    });

    passed &= run("stencil replay refuses neighbor-lane operations even without LDS storage", [] {
        for (const auto opcode : {IrOpcode::PermuteU32, IrOpcode::BpermuteU32, IrOpcode::SwizzleU32, IrOpcode::ReadLane, IrOpcode::ReadFirstLane, IrOpcode::DppMoveU32, IrOpcode::DppUpdateU32, IrOpcode::Permlane16U32, IrOpcode::WqmU64}) {
            Builder b;
            b.emit(opcode, IrType::U32, {});
            if (StencilReplaySafe(b.program)) return false;
        }
        return true;
    });

    const auto exportUnder = [](Builder& b, IrValue& guard, bool stencil, bool live) {
        ExportInfo info{};
        info.kind = stencil ? ExportTargetKind::MrtZ : ExportTargetKind::Mrt;
        info.en = stencil ? 2u : 15u;
        info.vm = live;
        ExportFlags flags{static_cast<std::uint32_t>(b.program.Metadata().exportInfo.size())};
        b.program.Metadata().exportInfo.push_back(info);
        std::uint64_t bits = 0;
        std::memcpy(&bits, &flags, sizeof(flags));
        auto& zero = b.constant(0u);
        auto& value = b.emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {&zero, &zero, &zero, &zero});
        b.emit(IrOpcode::SetAttribute, IrType::Void, {&value, &guard}, bits);
    };

    const auto pixelPredicate = [](Builder& b) -> IrValue& {
        auto& value = b.emit(IrOpcode::GetBuiltin, IrType::U32, {&b.constant(static_cast<std::uint32_t>(StageInputKind::FragCoord)), &b.constant(0u)});
        return b.emit(IrOpcode::INotEqual32, IrType::Bool, {&value, &b.constant(0u)});
    };

    const auto helperPredicate = [](Builder& b, bool helper) -> IrValue& {
        auto& value = b.emit(IrOpcode::GetBuiltin, IrType::U32, {&b.constant(static_cast<std::uint32_t>(StageInputKind::HelperInvocation)), &b.constant(0u)});
        return b.emit(helper ? IrOpcode::INotEqual32 : IrOpcode::IEqual32, IrType::Bool, {&value, &b.constant(0u)});
    };

    const auto stencilByMask = [](Builder& b, IrValue& mask) {
        auto& zero = b.constant(0u);
        auto& reference = b.select(mask, zero, b.constant(255u));
        ExportInfo info{};
        info.kind = ExportTargetKind::MrtZ;
        info.en = 2u;
        info.vm = true;
        ExportFlags flags{static_cast<std::uint32_t>(b.program.Metadata().exportInfo.size())};
        b.program.Metadata().exportInfo.push_back(info);
        std::uint64_t bits = 0;
        std::memcpy(&bits, &flags, sizeof(flags));
        auto& value = b.emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {&zero, &reference, &zero, &zero});
        b.emit(IrOpcode::SetAttribute, IrType::Void, {&value, &b.truth()}, bits);
    };

    passed &= run("finite stencil references selected by an undefined value cannot be replayed", [] {
        Builder b;
        b.program.Resources().stage = IrShaderStage::Pixel;
        auto& undefined = b.emit(IrOpcode::UndefU1, IrType::Bool, {});
        auto& zero = b.constant(0u);
        auto& reference = b.select(undefined, zero, b.constant(255u));
        ExportInfo info{};
        info.kind = ExportTargetKind::MrtZ;
        info.en = 2u;
        info.vm = true;
        b.program.Metadata().exportInfo.push_back(info);
        ExportFlags flags{0u};
        std::uint64_t bits = 0;
        std::memcpy(&bits, &flags, sizeof(flags));
        auto& value = b.emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {&zero, &reference, &zero, &zero});
        b.emit(IrOpcode::SetAttribute, IrType::Void, {&value, &b.truth()}, bits);
        return StencilReferenceValues(b.program) == std::vector<std::uint32_t>{0u, 255u} && !StencilReplaySafe(b.program);
    });

    passed &= run("an unprovable stencil reference replays every eight-bit value", [] {
        Builder b;
        b.program.Resources().stage = IrShaderStage::Pixel;
        auto& zero = b.constant(0u);
        auto& reference = b.emit(IrOpcode::UndefU32, IrType::U32, {});
        ExportInfo info{};
        info.kind = ExportTargetKind::MrtZ;
        info.en = 2u;
        info.vm = true;
        b.program.Metadata().exportInfo.push_back(info);
        ExportFlags flags{0u};
        std::uint64_t bits = 0;
        std::memcpy(&bits, &flags, sizeof(flags));
        auto& value = b.emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {&zero, &reference, &zero, &zero});
        b.emit(IrOpcode::SetAttribute, IrType::Void, {&value, &b.truth()}, bits);
        const auto values = StencilReferenceValues(b.program);
        return values.size() == 256u && values.front() == 0u && values.back() == 255u;
    });

    passed &= run("a proven stencil reference replays its eight-bit value", [] {
        Builder b;
        b.program.Resources().stage = IrShaderStage::Pixel;
        auto& zero = b.constant(0u);
        ExportInfo info{};
        info.kind = ExportTargetKind::MrtZ;
        info.en = 2u;
        info.vm = true;
        b.program.Metadata().exportInfo.push_back(info);
        ExportFlags flags{0u};
        std::uint64_t bits = 0;
        std::memcpy(&bits, &flags, sizeof(flags));
        auto& value = b.emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {&zero, &b.constant(300u), &zero, &zero});
        b.emit(IrOpcode::SetAttribute, IrType::Void, {&value, &b.truth()}, bits);
        return StencilReferenceValues(b.program) == std::vector<std::uint32_t>{44u};
    });

    passed &= run("stencil replay admits own-lane masks and their whole-quad expansion as export guards", [&] {
        Builder b;
        b.program.Resources().stage = IrShaderStage::Pixel;
        auto& quads = wholeQuads(b, b.logicalAnd(pixelPredicate(b), helperPredicate(b, false)));
        exportUnder(b, quads, true, true);
        return StencilReplaySafe(b.program);
    });

    passed &= run("helper participation cannot choose a kept lane's stencil through whole-quad expansion", [&] {
        Builder b;
        b.program.Resources().stage = IrShaderStage::Pixel;
        stencilByMask(b, wholeQuads(b, helperPredicate(b, true)));
        return StencilReferenceValues(b.program) == std::vector<std::uint32_t>{0u, 255u} && !StencilReplaySafe(b.program);
    });

    passed &= run("nested active ballots do not erase helper uncertainty before whole-quad expansion", [&] {
        Builder b;
        b.program.Resources().stage = IrShaderStage::Pixel;
        auto& active = b.ballotBit(b.truth());
        stencilByMask(b, wholeQuads(b, b.logicalAnd(active, helperPredicate(b, true))));
        return !StencilReplaySafe(b.program);
    });

    passed &= run("an uncovered helper coordinate predicate cannot choose a real stencil through a ballot", [&] {
        Builder b;
        b.program.Resources().stage = IrShaderStage::Pixel;
        auto& bits = b.emit(IrOpcode::GetBuiltin, IrType::U32, {&b.constant(static_cast<std::uint32_t>(StageInputKind::FragCoord)), &b.constant(0u)});
        auto& coordinate = b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&bits});
        auto& boundary = b.program.CreateValue(IrOpcode::Void, IrType::F32);
        boundary.SetImmediateF32(1.0f);
        auto& predicate = b.emit(IrOpcode::FPOrdGreaterThan32, IrType::Bool, {&coordinate, &boundary});
        stencilByMask(b, wholeQuads(b, predicate));
        return !StencilReplaySafe(b.program);
    });

    passed &= run("explicit helper exclusion is preserved through nested ballot mask plumbing", [&] {
        Builder b;
        b.program.Resources().stage = IrShaderStage::Pixel;
        auto& real = b.ballotBit(helperPredicate(b, false));
        stencilByMask(b, wholeQuads(b, b.logicalAnd(real, pixelPredicate(b))));
        return StencilReplaySafe(b.program);
    });

    passed &= run("read-only BDA reads can be replayed and BDA writes cannot", [] {
        Builder b;
        b.program.Info().usesDma = true;
        if (!StencilReplaySafe(b.program)) return false;
        b.program.Info().bdaWrites = true;
        return !StencilReplaySafe(b.program);
    });

    passed &= run("only guarded read-only SRT diagnostics can be replayed", [] {
        Builder b;
        b.program.Info().usesFaultBuffer = true;
        if (StencilReplaySafe(b.program)) return false;
        b.program.Resources().guardedSrtSlots.push_back(0u);
        if (!StencilReplaySafe(b.program)) return false;
        b.program.Metadata().blockInfo.front().terminator.loopHeader = true;
        if (StencilReplaySafe(b.program)) return false;
        b.program.Metadata().blockInfo.front().terminator.loopHeader = false;
        b.emit(IrOpcode::Loop, IrType::Void, {});
        return !StencilReplaySafe(b.program);
    });

    passed &= run("guarded SRT replay refuses an unannotated control-flow cycle", [] {
        Builder b;
        b.program.Info().usesFaultBuffer = true;
        b.program.Resources().guardedSrtSlots.push_back(0u);
        auto& entry = *b.block;
        auto& cycle = b.newBlock();
        entry.AddBranch(&cycle);
        cycle.AddBranch(&cycle);
        auto& metadata = b.program.Metadata().blockInfo;
        metadata[0u].terminator.kind = TerminatorKind::Branch;
        metadata[0u].terminator.trueBlock = cycle.Id();
        metadata[1u].terminator.kind = TerminatorKind::Branch;
        metadata[1u].terminator.trueBlock = cycle.Id();
        return !StencilReplaySafe(b.program);
    });

    passed &= run("guarded SRT replay refuses missing control-flow targets", [] {
        Builder b;
        b.program.Info().usesFaultBuffer = true;
        b.program.Resources().guardedSrtSlots.push_back(0u);
        auto& term = b.program.Metadata().blockInfo.front().terminator;
        term.kind = TerminatorKind::Branch;
        term.trueBlock = 1024u;
        return !StencilReplaySafe(b.program);
    });

    passed &= run("a captured active ballot cannot provide helper texture coordinates", [&] {
        Builder b;
        b.program.Resources().stage = IrShaderStage::Pixel;
        auto& active = b.ballotBit(b.truth());
        auto& helperValue = b.emit(IrOpcode::GetBuiltin, IrType::U32, {&b.constant(static_cast<std::uint32_t>(StageInputKind::HelperInvocation)), &b.constant(0u)});
        auto& helper = b.emit(IrOpcode::INotEqual32, IrType::Bool, {&helperValue, &b.constant(0u)});
        auto& coordinate = b.select(b.logicalAnd(active, helper), b.constant(1u), b.constant(0u));
        auto& address = b.emit(IrOpcode::MakeImageAddress, IrType::ImageAddress, {&coordinate});
        auto& image = b.emit(IrOpcode::GetImageResource, IrType::ImageResource, {});
        auto& sampler = b.emit(IrOpcode::GetSamplerResource, IrType::SamplerResource, {});
        b.emit(IrOpcode::ImageSampleRaw, IrType::U32x4, {&image, &sampler, &address});
        exportUnder(b, b.truth(), true, true);
        return !StencilReplaySafe(b.program);
    });

    const auto optionalPixelBody = [&](bool derivativeConsumer, IrOpcode observer = IrOpcode::ImageSampleRaw) {
        Builder b;
        b.program.Resources().stage = IrShaderStage::Pixel;
        auto& entry = *b.block;
        auto& predicate = pixelPredicate(b);
        auto& zero = b.constant(0u);
        auto& condition = b.emit(IrOpcode::LogicalNot, IrType::Bool, {&predicate});
        auto& body = b.newBlock();
        auto& merge = b.newBlock();
        entry.AddBranch(&body);
        entry.AddBranch(&merge);
        body.AddBranch(&merge);
        auto& entryInfo = b.program.Metadata().blockInfo[0u];
        entryInfo.terminator.kind = TerminatorKind::ConditionalBranch;
        entryInfo.terminator.condition = BranchCondition::ExecZero;
        entryInfo.terminator.trueBlock = merge.Id();
        entryInfo.terminator.falseBlock = body.Id();
        entryInfo.condition = &condition;
        auto& bodyInfo = b.program.Metadata().blockInfo[1u];
        bodyInfo.terminator.kind = TerminatorKind::Branch;
        bodyInfo.terminator.trueBlock = merge.Id();
        b.block = &body;
        auto& written = b.select(predicate, b.constant(42u), zero);
        b.block = &merge;
        auto& merged = b.phi(IrType::U32, entry, zero);
        merged.AddPhiOperand(&body, &written);
        if (derivativeConsumer) {
            auto& address = b.emit(IrOpcode::MakeImageAddress, IrType::ImageAddress, {&merged});
            auto& image = b.emit(IrOpcode::GetImageResource, IrType::ImageResource, {});
            auto& sampler = b.emit(IrOpcode::GetSamplerResource, IrType::SamplerResource, {});
            b.emit(observer, IrType::U32x4, {&image, &sampler, &address});
        } else b.keep(merged);
        exportUnder(b, b.truth(), true, true);
        return StencilReplaySafe(b.program);
    };

    passed &= run("an optional wave body preserves inactive values consumed only within a lane", [&] { return optionalPixelBody(false); });
    passed &= run("an optional wave body cannot change helper coordinates for later implicit samples", [&] { return !optionalPixelBody(true); });
    passed &= run("an optional wave body cannot change helper coordinates for LOD queries", [&] { return !optionalPixelBody(true, IrOpcode::ImageQueryLod); });

    passed &= run("varying attribute sources cannot be treated as uniform any-active predicates", [&] {
        for (const auto opcode : {IrOpcode::GetAttribute, IrOpcode::GetInterpolationParameter, IrOpcode::GetInterpolationParameterF16, IrOpcode::GetTessellationAttribute}) {
            Builder b;
            b.program.Resources().stage = IrShaderStage::Pixel;
            auto& entry = *b.block;
            auto& varying = b.emit(opcode, IrType::U32, {&b.constant(0u), &b.constant(0u), &b.constant(0u)});
            auto& predicate = b.emit(IrOpcode::INotEqual32, IrType::Bool, {&varying, &b.constant(0u)});
            auto& condition = b.emit(IrOpcode::LogicalNot, IrType::Bool, {&predicate});
            auto& left = b.newBlock();
            auto& right = b.newBlock();
            entry.AddBranch(&left);
            entry.AddBranch(&right);
            auto& info = b.program.Metadata().blockInfo[0u];
            info.terminator.kind = TerminatorKind::ConditionalBranch;
            info.terminator.condition = BranchCondition::ExecZero;
            info.terminator.trueBlock = left.Id();
            info.terminator.falseBlock = right.Id();
            info.condition = &condition;
            b.block = &left;
            exportUnder(b, b.truth(), true, true);
            b.block = &right;
            auto& address = b.emit(IrOpcode::MakeImageAddress, IrType::ImageAddress, {&varying});
            auto& image = b.emit(IrOpcode::GetImageResource, IrType::ImageResource, {});
            auto& sampler = b.emit(IrOpcode::GetSamplerResource, IrType::SamplerResource, {});
            b.emit(IrOpcode::ImageSampleRaw, IrType::U32x4, {&image, &sampler, &address});
            exportUnder(b, b.truth(), true, true);
            if (StencilReplaySafe(b.program)) return false;
        }
        return true;
    });

    const auto anyActiveGate = [&](bool unrelatedLiveMask) {
        Builder b;
        b.program.Resources().stage = IrShaderStage::Pixel;
        auto& entry = *b.block;
        const auto [low, high] = b.ballotWords(pixelPredicate(b));
        auto& own = b.threadBit(*low, *high);
        auto& words = b.emit(IrOpcode::BitwiseOr32, IrType::U32, {low, high});
        auto& any = b.emit(IrOpcode::INotEqual32, IrType::Bool, {&words, &b.constant(0u)});
        auto& condition = b.emit(IrOpcode::LogicalNot, IrType::Bool, {&any});
        auto& killed = b.newBlock();
        auto& kept = b.newBlock();
        entry.AddBranch(&killed);
        entry.AddBranch(&kept);
        auto& info = b.program.Metadata().blockInfo[0u];
        info.terminator.kind = TerminatorKind::ConditionalBranch;
        info.terminator.condition = BranchCondition::SccZero;
        info.terminator.trueBlock = killed.Id();
        info.terminator.falseBlock = kept.Id();
        info.condition = &condition;
        b.block = &killed;
        auto& falseValue = b.program.CreateValue(IrOpcode::Void, IrType::Bool);
        falseValue.SetImmediateBool(false);
        exportUnder(b, falseValue, true, true);
        b.block = &kept;
        exportUnder(b, unrelatedLiveMask ? b.truth() : own, true, true);
        return StencilReplaySafe(b.program);
    };

    passed &= run("any-active early return is stable for the source mask's kept quads", [&] { return anyActiveGate(false); });
    passed &= run("an any-active early return cannot control unrelated kept invocations", [&] { return !anyActiveGate(true); });

    passed &= run("kept invocations must be covered by the stencil export guard", [&] {
        Builder b;
        auto& written = b.mask(16u);
        exportUnder(b, written, true, false);
        exportUnder(b, b.logicalAnd(written, b.mask(8u)), false, true);
        if (!StencilReferenceDefinedForKeptInvocations(b.program)) return false;
        exportUnder(b, b.truth(), false, true);
        return !StencilReferenceDefinedForKeptInvocations(b.program);
    });

    passed &= run("an export on one branch does not cover a kept return on another", [&] {
        Builder b;
        auto& entry = *b.block;
        auto& left = b.newBlock();
        auto& right = b.newBlock();
        auto& merge = b.newBlock();
        entry.AddBranch(&left);
        entry.AddBranch(&right);
        left.AddBranch(&merge);
        right.AddBranch(&merge);
        auto& metadata = b.program.Metadata().blockInfo;
        metadata[0].terminator = {TerminatorKind::ConditionalBranch, BranchCondition::Unknown, left.Id(), right.Id()};
        metadata[1].terminator = {TerminatorKind::Branch, BranchCondition::Always, merge.Id()};
        metadata[2].terminator = {TerminatorKind::Branch, BranchCondition::Always, merge.Id()};
        b.block = &left;
        exportUnder(b, b.truth(), true, false);
        b.block = &merge;
        exportUnder(b, b.truth(), false, true);
        return !StencilReferenceDefinedForKeptInvocations(b.program);
    });

    passed &= run("an implicit-LOD pixel sample reads its quad's coordinates and keeps the select", [&] {
        return !sample(IrShaderStage::Pixel, 0u);
    });

    passed &= run("an implicit-LOD pixel sample under whole quad mode reads no masked quad and drops the select", [&] {
        return sample(IrShaderStage::Pixel, 0u, IrOpcode::ImageSampleRaw, true);
    });
    passed &= run("an explicit-LOD sample reads only its lane and drops the select", [&] {
        return sample(IrShaderStage::Pixel, RdnaImageSampleFlagLevelZero) && sample(IrShaderStage::Mesh, 0u);
    });

    passed &= run("a pixel gather without an LOD of its own takes it from the quad and keeps the select", [&] {
        return !sample(IrShaderStage::Pixel, 0u, IrOpcode::ImageGatherRaw) && !sample(IrShaderStage::Pixel, RdnaImageSampleFlagBias, IrOpcode::ImageGatherRaw);
    });

    passed &= run("a gather with an explicit LOD reads only its lane and drops the select", [&] {
        return sample(IrShaderStage::Pixel, RdnaImageSampleFlagLevelZero, IrOpcode::ImageGatherRaw) && sample(IrShaderStage::Pixel, RdnaImageSampleFlagLod, IrOpcode::ImageGatherRaw) &&
               sample(IrShaderStage::Mesh, 0u, IrOpcode::ImageGatherRaw);
    });

    passed &= run("an image size query reads only its lane's level and drops the select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& address = b.emit(IrOpcode::MakeImageAddress, IrType::ImageAddress, {&written});
        auto& size = b.emit(IrOpcode::ImageQueryDimensions, IrType::U32x4, {&b.emit(IrOpcode::GetImageResource, IrType::ImageResource, {}), &address});
        b.keep(b.select(exec, b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&size, &b.constant(0u)}), old));
        return b.eliminate() == 1u && removed(written);
    });

    const auto guarded = [](IrOpcode opcode, bool sameExec, bool asActive) {
        Builder b;
        auto& wide = b.mask(48u);
        auto& exec = b.logicalAnd(wide, b.mask(16u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        IrValue& active = asActive ? b.emit(IrOpcode::INotEqual32, IrType::Bool, {&written, &b.constant(0u)}) : sameExec ? exec : wide;
        switch (opcode) {
            case IrOpcode::WriteSharedU32:
                b.emit(opcode, IrType::Void, {&old, &written, &active});
                break;
            case IrOpcode::LoadBufferU32:
                b.keep(b.select(exec, b.emit(opcode, IrType::U32, {&b.emit(IrOpcode::GetBufferResource, IrType::BufferResource, {}), &written, &b.constant(0u), &b.constant(0u), &active}), old));
                break;
            case IrOpcode::ImageRead: {
                auto& address = b.emit(IrOpcode::MakeImageAddress, IrType::ImageAddress, {&written, &old});
                auto& image = b.emit(IrOpcode::GetImageResource, IrType::ImageResource, {});
                auto& read = b.emit(opcode, IrType::U32x4, {&image, &address, &active});
                b.keep(b.select(exec, b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&read, &b.constant(0u)}), old));
                break;
            }
            case IrOpcode::ImageWrite: {
                auto& address = b.emit(IrOpcode::MakeImageAddress, IrType::ImageAddress, {&old, &old});
                auto& texel = b.emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {&written, &old, &old, &old});
                b.emit(opcode, IrType::Void, {&b.emit(IrOpcode::GetImageResource, IrType::ImageResource, {}), &address, &texel, &active});
                break;
            }
            default:
                b.emit(opcode, IrType::Void, {&b.emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {&written, &old, &old, &old}), &active});
                break;
        }
        b.eliminate();
        return removed(written);
    };

    passed &= run("a store, load or export whose exec implies the write's exec drops the select", [&] {
        return guarded(IrOpcode::WriteSharedU32, true, false) && guarded(IrOpcode::LoadBufferU32, true, false) && guarded(IrOpcode::ImageRead, true, false) && guarded(IrOpcode::SetAttribute, true, false);
    });

    passed &= run("a store, load or export under a wider exec keeps the select", [&] {
        return !guarded(IrOpcode::WriteSharedU32, false, false) && !guarded(IrOpcode::LoadBufferU32, false, false) && !guarded(IrOpcode::ImageRead, false, false) && !guarded(IrOpcode::SetAttribute, false, false);
    });

    passed &= run("an image write under the write's exec drops the select, under a wider exec or as its exec keeps it", [&] {
        return guarded(IrOpcode::ImageWrite, true, false) && !guarded(IrOpcode::ImageWrite, false, false) && !guarded(IrOpcode::ImageWrite, true, true);
    });

    passed &= run("a value used as a store's exec keeps the select", [&] {
        return !guarded(IrOpcode::WriteSharedU32, true, true);
    });

    passed &= run("an atomic keeps the select", [&] {
        return !guarded(IrOpcode::SharedAtomicIAdd32, true, false);
    });

    passed &= run("a write under whole quad mode read only by the lanes it was widened from drops the select in every stage", [&] {
        const auto widened = [&](IrShaderStage stage) {
            Builder b;
            b.program.Resources().stage = stage;
            auto& live = b.mask(16u);
            auto& quads = wholeQuads(b, live);
            auto& old = b.lane();
            auto& written = b.select(quads, b.add(old, 1u), old);
            auto& next = b.select(b.logicalAnd(live, b.mask(8u)), b.add(written, 2u), old);
            b.keep(next);
            return b.eliminate() != 0u && removed(written) && !removed(next);
        };
        return widened(IrShaderStage::Pixel) && widened(IrShaderStage::Compute);
    });

    passed &= run("a write under whole quad mode read by a lane outside the widened mask keeps the select in compute", [&] {
        Builder b;
        b.program.Resources().stage = IrShaderStage::Compute;
        auto& live = b.mask(16u);
        auto& quads = wholeQuads(b, live);
        auto& old = b.lane();
        auto& written = b.select(quads, b.add(old, 1u), old);
        auto& next = b.select(b.mask(32u), b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() == 0u && !removed(written);
    });

    passed &= run("a write under whole quad mode read only by the lanes it was widened from drops the select", [&] {
        Builder b;
        auto& live = b.mask(16u);
        auto& quads = wholeQuads(b, live);
        auto& old = b.lane();
        auto& written = b.select(quads, b.add(old, 1u), old);
        auto& next = b.select(b.logicalAnd(live, b.mask(8u)), b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() != 0u && removed(written) && !removed(next);
    });

    passed &= run("a write under whole quad mode read by other lanes keeps the select", [&] {
        Builder b;
        auto& live = b.mask(16u);
        auto& quads = wholeQuads(b, live);
        auto& old = b.lane();
        auto& written = b.select(quads, b.add(old, 1u), old);
        auto& next = b.select(b.mask(32u), b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() == 0u && !removed(written);
    });

    const auto shaded = [](std::uint32_t units) {
        Builder b;
        b.program.Resources().stage = IrShaderStage::Pixel;
        MemoryInfo memory;
        memory.kind = ResourceKind::Image;
        memory.imageSampleFlags = RdnaImageSampleFlagLevelZero;
        b.program.Resources().memoryInfo.push_back(memory);
        MemoryFlags flags{0u, 0u};
        std::uint64_t bits = 0;
        std::memcpy(&bits, &flags, sizeof(flags));
        auto& exec = b.mask(48u);
        auto& image = b.emit(IrOpcode::GetImageResource, IrType::ImageResource, {});
        auto& sampler = b.emit(IrOpcode::GetSamplerResource, IrType::SamplerResource, {});
        IrValue* color = &b.lane();
        IrValue* address = nullptr;
        std::uint32_t kept = 0;
        std::vector<IrValue*> compared;
        for (std::uint32_t unit = 0; unit < units; ++unit) {
            auto& sum = b.add(*color, unit);
            auto& low = b.emit(IrOpcode::ULessThan32, IrType::Bool, {&sum, &b.constant(100u + unit)});
            compared.push_back(&b.select(low, sum, b.add(*color, 1u)));
            address = &b.emit(IrOpcode::MakeImageAddress, IrType::ImageAddress, {compared.back(), color});
            auto& gathered = b.emit(IrOpcode::ImageGatherRaw, IrType::U32x4, {&image, &sampler, address}, bits);
            color = &b.select(exec, b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&gathered, &b.constant(0u)}), *color);
        }
        b.emit(IrOpcode::ImageWrite, IrType::Void, {&image, address, &b.emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {color, color, color, color}), &exec});
        const auto stats = MaskedSelectEliminator{}.Eliminate(b.program);
        for (const IrValue* select : compared) kept += removed(*select) ? 0u : 1u;
        return std::pair{stats, kept};
    };

    passed &= run("selects that each reach the rest of the program are settled in steps that grow with the program, not with its square", [&] {
        const auto [small, smallKept] = shaded(500u);
        const auto [large, largeKept] = shaded(4000u);
        return small.removedSelects == 500u && smallKept == 500u && large.removedSelects == 4000u && largeKept == 4000u && small.steps != 0u && large.steps <= 9u * small.steps;
    });

    return passed ? 0 : 1;
}
