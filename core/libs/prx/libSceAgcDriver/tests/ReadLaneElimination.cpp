#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ReadLaneEliminator.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <initializer_list>

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
        return created;
    }

    IrValue& constant(std::uint32_t value) {
        auto& created = program.CreateValue(IrOpcode::Void, IrType::U32);
        created.SetImmediateU32(value);
        return created;
    }

    IrValue& emit(IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments) {
        auto& created = program.CreateValue(opcode, type);
        for (auto* argument : arguments) created.AddArgument(argument);
        block->AppendInstruction(&created);
        return created;
    }

    IrValue& unknown() {
        return emit(IrOpcode::LaneId, IrType::U32, {});
    }

    void keep(IrValue& value) {
        static_cast<void>(emit(IrOpcode::ReferenceU32, IrType::Void, {&value}));
    }
};

struct Spill {
    Builder b;
    IrValue* stored = nullptr;
    IrValue* read = nullptr;
};

Spill SpillWrittenInBranch(bool storedBeforeBranch, bool prefixBeforeRead = false) {
    Spill spill;
    auto& b = spill.b;
    IrBlock& entry = *b.block;
    IrBlock& then = b.newBlock();
    IrBlock& merge = b.newBlock();
    entry.AddBranch(&then);
    entry.AddBranch(&merge);
    then.AddBranch(&merge);
    auto& initial = b.unknown();
    if (storedBeforeBranch) spill.stored = &b.unknown();
    b.block = &then;
    if (!storedBeforeBranch) spill.stored = &b.unknown();
    auto& written = b.emit(IrOpcode::WriteLane, IrType::U32, {spill.stored, &b.constant(5u), &initial});
    b.block = &merge;
    auto& slot = b.emit(IrOpcode::Phi, IrType::U32, {});
    slot.AddPhiOperand(&entry, &initial);
    slot.AddPhiOperand(&then, &written);
    if (prefixBeforeRead) static_cast<void>(b.unknown());
    spill.read = &b.emit(IrOpcode::ReadLane, IrType::U32, {&slot, &b.constant(5u)});
    b.keep(*spill.read);
    return spill;
}

bool expect(const char* name, bool condition) {
    if (!condition) std::fprintf(stderr, "%s\n", name);
    return condition;
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

    passed &= run("a lane value stored inside a branch reaches the read after the merge through a phi", [] {
        auto spill = SpillWrittenInBranch(false, true);
        const auto stats = ReadLaneEliminator{}.Eliminate(spill.b.program, 64u);
        if (stats.rewrittenReads != 1u || spill.read->Opcode() != IrOpcode::Identity) return false;
        const IrValue* phi = spill.read->Argument(0);
        if (!phi->IsPhi() || phi->Parent() != spill.read->Parent() || phi->ArgumentCount() != 2u) return false;
        bool phiBeforeEveryNonPhi = false;
        bool sawNonPhi = false;
        for (const IrValue* instruction : phi->Parent()->Instructions()) {
            if (!instruction->IsPhi()) sawNonPhi = true;
            if (instruction == phi) {
                phiBeforeEveryNonPhi = !sawNonPhi;
                break;
            }
        }
        if (!phiBeforeEveryNonPhi) return false;
        bool stored = false;
        bool undefined = false;
        for (std::size_t index = 0; index < phi->ArgumentCount(); index++) {
            const IrValue* operand = phi->Argument(index);
            stored = stored || (operand == spill.stored && operand->Parent() == phi->PhiBlock(index));
            undefined = undefined || (operand->Opcode() == IrOpcode::UndefU32 && operand->Parent() == &spill.b.program.EntryBlock());
        }
        return stored && undefined;
    });

    passed &= run("a lane value computed before the branch replaces the read after the merge", [] {
        auto spill = SpillWrittenInBranch(true);
        const auto stats = ReadLaneEliminator{}.Eliminate(spill.b.program, 64u);
        return stats.rewrittenReads == 1u && spill.read->Opcode() == IrOpcode::Identity && spill.read->Argument(0) == spill.stored;
    });

    passed &= run("a loop read before its same-block store uses the preceding iteration value", [] {
        Builder b;
        IrBlock& entry = *b.block;
        IrBlock& loop = b.newBlock();
        IrBlock& exit = b.newBlock();
        entry.AddBranch(&loop);
        loop.AddBranch(&loop);
        loop.AddBranch(&exit);
        auto& initial = b.unknown();
        b.block = &loop;
        auto& slot = b.emit(IrOpcode::Phi, IrType::U32, {});
        slot.AddPhiOperand(&entry, &initial);
        auto& read = b.emit(IrOpcode::ReadLane, IrType::U32, {&slot, &b.constant(5u)});
        b.keep(read);
        auto& stored = b.unknown();
        auto& written = b.emit(IrOpcode::WriteLane, IrType::U32, {&stored, &b.constant(5u), &slot});
        slot.AddPhiOperand(&loop, &written);

        const auto stats = ReadLaneEliminator{}.Eliminate(b.program, 64u);
        if (stats.rewrittenReads != 1u || read.Opcode() != IrOpcode::Identity) return false;
        const IrValue* reaching = read.Argument(0);
        if (!reaching->IsPhi() || reaching->Parent() != &loop || reaching->ArgumentCount() != 2u) return false;
        bool entryUndefined = false;
        bool backedgeStore = false;
        for (std::size_t index = 0; index < reaching->ArgumentCount(); index++) {
            const IrValue* operand = reaching->Argument(index);
            entryUndefined = entryUndefined || (reaching->PhiBlock(index) == &entry && operand->Opcode() == IrOpcode::UndefU32);
            backedgeStore = backedgeStore || (reaching->PhiBlock(index) == &loop && operand == &stored);
        }
        return entryUndefined && backedgeStore;
    });

    if (!passed) return 1;
    std::puts("read lane elimination tests passed");
    return 0;
}
