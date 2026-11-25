#include "Optimization/ReadLaneEliminator.hpp"
#include <cstdio>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace ShaderRecompiler;

namespace {

void Require(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error(what);
}

// Hand-built IR in the shape Gran Turismo 7's compute shader 0x120aed7f00 has after SSA construction and folding: an
// SGPR spilled with v_writelane_b32 to lane 29 of a vector that a loop also compacts into (v_writelane v,
// v_readlane(w, s_lane), s_count), read back with v_readlane_b32 after the loop.
struct Program {
    IrProgram program;
    IrBlock& entry;
    IrBlock& loop;
    IrBlock& exit;
    std::uint32_t builtins = 0;

    Program() : entry(program.CreateBlock()), loop(program.CreateBlock()), exit(program.CreateBlock()) {
        entry.AddBranch(&loop);
        loop.AddBranch(&loop);
        loop.AddBranch(&exit);
        for (IrBlock* block : {&entry, &loop, &exit}) program.BlockOrder().push_back(block);
        program.SetEntryBlock(entry);
        program.SetWaveSize(64u);
    }

    IrValue& Immediate(std::uint32_t value) {
        auto& constant = program.CreateValue(IrOpcode::Void, IrType::U32);
        constant.SetImmediateU32(value);
        return constant;
    }

    IrValue& Emit(IrBlock& block, IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments) {
        auto& value = program.CreateValue(opcode, type);
        for (auto* argument : arguments) value.AddArgument(argument);
        block.AppendInstruction(&value);
        return value;
    }

    // An opaque uniform U32 the pass cannot see through: a stand-in for a ReadConst or an SGPR input.
    IrValue& Scalar(IrBlock& block) {
        return Emit(block, IrOpcode::GetBuiltin, IrType::U32, {&Immediate(0), &Immediate(builtins++)});
    }

    IrValue& Phi(IrBlock& block) {
        return Emit(block, IrOpcode::Phi, IrType::U32, {});
    }

    IrValue& Spill(IrValue& value, std::uint32_t lane, IrValue& into) {
        return Emit(entry, IrOpcode::WriteLane, IrType::U32, {&value, &Immediate(lane), &into});
    }

    IrValue& Read(IrValue& vector, std::uint32_t lane) {
        return Emit(exit, IrOpcode::ReadLane, IrType::U32, {&vector, &Immediate(lane)});
    }

    std::uint32_t Count(const IrBlock& block, IrOpcode opcode) const {
        std::uint32_t count = 0;
        for (const IrValue* inst : block.Instructions()) count += inst->Opcode() == opcode ? 1u : 0u;
        return count;
    }

    std::uint32_t Guards() const {
        return Count(entry, IrOpcode::ReadLaneGuard) + Count(loop, IrOpcode::ReadLaneGuard) + Count(exit, IrOpcode::ReadLaneGuard);
    }
};

// The use resolves to `scalar`, or to a loop phi that joins `scalar` with itself (what the phi path leaves for the folder).
bool FoldsTo(const IrValue& use, const IrValue* scalar) {
    const IrValue* value = use.Argument(0)->Resolve();
    if (value == scalar) return true;
    if (!value->IsPhi()) return false;
    for (std::size_t index = 0; index < value->ArgumentCount(); ++index) {
        const IrValue* operand = value->Argument(index)->Resolve();
        if (operand != scalar && operand != value) return false;
    }
    return true;
}

bool StillReads(const IrValue& use) {
    return use.Argument(0)->Resolve()->Opcode() == IrOpcode::ReadLane;
}

struct Compaction {
    bool spill = true;             // the SGPR spill to lane 29 before the loop
    bool countIsCounter = true;    // the lane is a loop-carried phi stepped by 1; otherwise another SGPR
    bool writesReadLane = true;    // the value written is v_readlane(w, s); otherwise plain arithmetic
    std::uint32_t unrolled = 2;    // writes per loop body
};

struct Built {
    IrValue* spill = nullptr;
    IrValue* vector = nullptr;   // the loop phi the read sees
    IrValue* read = nullptr;
    IrValue* use = nullptr;      // IAdd32(read, 1): where the fold shows
};

Built BuildCompaction(Program& p, const Compaction& options) {
    Built built;
    built.spill = &p.Scalar(p.entry);
    IrValue& w = p.Scalar(p.entry);
    IrValue& s = p.Scalar(p.entry);
    IrValue* initial = options.spill ? &p.Spill(*built.spill, 29u, p.Immediate(0)) : &p.Immediate(0);

    IrValue& vectorPhi = p.Phi(p.loop);
    IrValue& countPhi = p.Phi(p.loop);
    IrValue* vector = &vectorPhi;
    IrValue* count = options.countIsCounter ? &countPhi : &p.Scalar(p.loop);
    for (std::uint32_t step = 0; step < options.unrolled; ++step) {
        IrValue& written = options.writesReadLane ? p.Emit(p.loop, IrOpcode::ReadLane, IrType::U32, {&w, &s}) : p.Emit(p.loop, IrOpcode::IAdd32, IrType::U32, {&w, &p.Immediate(step)});
        vector = &p.Emit(p.loop, IrOpcode::WriteLane, IrType::U32, {&written, count, vector});
        count = &p.Emit(p.loop, IrOpcode::IAdd32, IrType::U32, {count, &p.Immediate(1)});
    }
    vectorPhi.AddPhiOperand(&p.entry, initial);
    vectorPhi.AddPhiOperand(&p.loop, vector);
    countPhi.AddPhiOperand(&p.entry, &p.Immediate(0));
    countPhi.AddPhiOperand(&p.loop, count);
    built.vector = &vectorPhi;

    built.read = &p.Read(vectorPhi, 29u);
    built.use = &p.Emit(p.exit, IrOpcode::IAdd32, IrType::U32, {built.read, &p.Immediate(1)});
    return built;
}

ReadLaneEliminationStats Eliminate(Program& p) {
    constexpr ReadLaneEliminator eliminator;
    return eliminator.Eliminate(p.program, 64u);
}

// A guard sits right before the folded read: a fresh read of the same lane of the same vector, compared with the scalar.
void RequireGuard(const Program& p, const Built& built, std::uint32_t lane, const IrValue* scalar) {
    const IrValue* previous = nullptr;
    const IrValue* guard = nullptr;
    for (const IrValue* inst : p.exit.Instructions()) {
        if (inst == built.read) { guard = previous; break; }
        previous = inst;
    }
    Require(guard != nullptr && guard->Opcode() == IrOpcode::ReadLaneGuard, "the folded read is not preceded by a ReadLaneGuard");
    const IrValue* actual = guard->Argument(0)->Resolve();
    Require(actual->Opcode() == IrOpcode::ReadLane && actual->Argument(0)->Resolve() == built.vector && actual->Argument(1)->Resolve()->ImmediateU32() == lane, "the guard does not read the same lane of the same vector");
    Require(guard->Argument(1)->Resolve() == scalar, "the guard does not compare against the folded scalar");
    Require(guard->Argument(2)->Resolve()->ImmediateU32() == lane, "the guard does not name the lane");
}

// The GT7 shape. Before this change the walk failed at the first write with a non-immediate lane, so the read stayed.
void SpillAcrossCompactionFolds() {
    Program p;
    const auto built = BuildCompaction(p, {});
    const auto stats = Eliminate(p);
    Require(stats.rewrittenReads == 1u, "the spilled lane read across a lane-compaction loop was not rewritten");
    Require(stats.compactionFolds == 1u, "the fold through compaction writes was not counted");
    Require(built.use->Argument(0)->Resolve() == built.spill, "the read was not replaced with the spilled scalar");
    Require(p.program.Info().usesFaultBuffer, "a guarded fold did not request the fault buffer");
    Require(p.Guards() == 1u, "the fold was not guarded exactly once");
    RequireGuard(p, built, 29u, built.spill);
}

// A second read of the same lane folds too, with a guard of its own.
void SecondReadIsGuardedToo() {
    Program p;
    const auto built = BuildCompaction(p, {});
    IrValue& again = p.Read(*built.vector, 29u);
    IrValue& use = p.Emit(p.exit, IrOpcode::IAdd32, IrType::U32, {&again, &p.Immediate(2)});
    const auto stats = Eliminate(p);
    Require(stats.rewrittenReads == 2u && stats.compactionFolds == 2u, "both reads of the spilled lane were not rewritten");
    Require(use.Argument(0)->Resolve() == built.spill, "the second read was not replaced with the spilled scalar");
    Require(p.Guards() == 2u, "the second read did not get its own guard");
}

// A read of another spilled lane folds to that lane's scalar.
void OtherLaneFoldsToItsOwnScalar() {
    Program p;
    const auto built = BuildCompaction(p, {});
    IrValue& other = p.Scalar(p.entry);
    // Spill lane 28 on top of the entry value: rewrite the loop phi's entry operand.
    IrValue& spilled = p.Emit(p.entry, IrOpcode::WriteLane, IrType::U32, {&other, &p.Immediate(28), built.vector->Argument(0)});
    built.vector->ReplaceArgument(0, &spilled);
    IrValue& read = p.Read(*built.vector, 28u);
    IrValue& use = p.Emit(p.exit, IrOpcode::IAdd32, IrType::U32, {&read, &p.Immediate(3)});
    const auto stats = Eliminate(p);
    Require(stats.rewrittenReads == 2u && stats.compactionFolds == 2u, "the reads of lanes 28 and 29 were not both rewritten");
    Require(use.Argument(0)->Resolve() == &other && built.use->Argument(0)->Resolve() == built.spill, "a read was replaced with the wrong lane's scalar");
    Require(p.Guards() == 2u, "the two folds were not guarded once each");
}

// Without a compaction loop the plain phi path folds the spill, as before, with no guard.
void SpillWithoutCompactionFoldsUnguarded() {
    Program p;
    const auto built = BuildCompaction(p, {.unrolled = 0});
    const auto stats = Eliminate(p);
    Require(stats.rewrittenReads == 1u && FoldsTo(*built.use, built.spill), "a spill without compaction no longer folds");
    Require(stats.compactionFolds == 0u && p.Guards() == 0u && !p.program.Info().usesFaultBuffer, "a spill without compaction was guarded");
}

// Anything else written at a non-immediate lane still stops the walk.
void OtherVariableLaneWritesStayUnknown() {
    {
        Program p;
        const auto built = BuildCompaction(p, {.writesReadLane = false});
        const auto stats = Eliminate(p);
        Require(stats.rewrittenReads == 0u && StillReads(*built.use), "a variable-lane write of a value that is not a v_readlane was stepped over");
        Require(p.Guards() == 0u && !p.program.Info().usesFaultBuffer, "a refused fold left a guard behind");
    }
    {
        Program p;
        const auto built = BuildCompaction(p, {.countIsCounter = false});
        const auto stats = Eliminate(p);
        Require(stats.rewrittenReads == 0u && StillReads(*built.use), "a v_readlane written at a lane that is not a loop count was stepped over");
        Require(p.Guards() == 0u, "a refused fold left a guard behind");
    }
}

// Compaction into a vector that was never spilled to at the read's lane: nothing to fold, nothing to guard.
void CompactionWithoutSpillStays() {
    Program p;
    const auto built = BuildCompaction(p, {.spill = false});
    const auto stats = Eliminate(p);
    Require(stats.rewrittenReads == 0u && StillReads(*built.use), "a lane nothing spilled to was folded");
    Require(p.Guards() == 0u && !p.program.Info().usesFaultBuffer, "a read that did not fold was guarded");
}

// A spill behind a select web of `nodes` nodes, read in the exit block; optionally an unrelated compaction loop elsewhere.
Built BuildWideWeb(Program& p, std::uint32_t nodes, bool unrelatedCompaction) {
    Built built;
    built.spill = &p.Scalar(p.entry);
    IrValue& spilled = p.Spill(*built.spill, 29u, p.Immediate(0));
    auto& condition = p.program.CreateValue(IrOpcode::Void, IrType::Bool);
    condition.SetImmediateBool(true);
    // A heap-shaped tree of selects over the spilled vector: `nodes` distinct nodes, about log2(nodes) deep.
    std::vector<IrValue*> tree(nodes + 1, nullptr);
    for (std::uint32_t index = nodes; index >= 1u; --index) {
        IrValue* left = 2u * index <= nodes ? tree[2u * index] : &spilled;
        IrValue* right = 2u * index + 1u <= nodes ? tree[2u * index + 1u] : &spilled;
        tree[index] = &p.Emit(p.entry, IrOpcode::SelectU32, IrType::U32, {&condition, left, right});
    }
    if (unrelatedCompaction) {
        IrValue& w = p.Scalar(p.entry);
        IrValue& s = p.Scalar(p.entry);
        IrValue& otherPhi = p.Phi(p.loop);
        IrValue& countPhi = p.Phi(p.loop);
        IrValue& written = p.Emit(p.loop, IrOpcode::ReadLane, IrType::U32, {&w, &s});
        IrValue& write = p.Emit(p.loop, IrOpcode::WriteLane, IrType::U32, {&written, &countPhi, &otherPhi});
        IrValue& count = p.Emit(p.loop, IrOpcode::IAdd32, IrType::U32, {&countPhi, &p.Immediate(1)});
        otherPhi.AddPhiOperand(&p.entry, &p.Immediate(0));
        otherPhi.AddPhiOperand(&p.loop, &write);
        countPhi.AddPhiOperand(&p.entry, &p.Immediate(0));
        countPhi.AddPhiOperand(&p.loop, &count);
    }
    built.vector = tree[1];
    built.read = &p.Read(*tree[1], 29u);
    built.use = &p.Emit(p.exit, IrOpcode::IAdd32, IrType::U32, {built.read, &p.Immediate(1)});
    return built;
}

// The visit budget stays at 4,096 nodes unless the program has a lane-compaction write.
void VisitBudgetGrowsOnlyWithCompaction() {
    {
        Program p;
        const auto built = BuildWideWeb(p, 4200u, false);
        const auto stats = Eliminate(p);
        Require(stats.rewrittenReads == 0u && StillReads(*built.use), "a web above 4,096 nodes folded in a program without lane compaction");
    }
    {
        Program p;
        const auto built = BuildWideWeb(p, 4000u, false);
        const auto stats = Eliminate(p);
        Require(stats.rewrittenReads == 1u && built.use->Argument(0)->Resolve() == built.spill, "a web below 4,096 nodes no longer folds");
        Require(p.Guards() == 0u, "a fold that stepped over no compaction write was guarded");
    }
    {
        Program p;
        const auto built = BuildWideWeb(p, 4200u, true);
        const auto stats = Eliminate(p);
        Require(stats.rewrittenReads == 1u && built.use->Argument(0)->Resolve() == built.spill, "a web above 4,096 nodes did not fold in a program with lane compaction");
        Require(stats.compactionFolds == 0u && p.Guards() == 0u && !p.program.Info().usesFaultBuffer, "a compaction write outside the read's web was guarded");
    }
}

}

int main() {
    const std::pair<const char*, void (*)()> cases[] = {
        {"a spill read across a lane-compaction loop folds and is guarded", SpillAcrossCompactionFolds},
        {"a second read of the same lane is guarded too", SecondReadIsGuardedToo},
        {"a read of another spilled lane folds to its own scalar", OtherLaneFoldsToItsOwnScalar},
        {"a spill without compaction folds without a guard", SpillWithoutCompactionFoldsUnguarded},
        {"other variable-lane writes stay unknown", OtherVariableLaneWritesStayUnknown},
        {"compaction without a spill is left alone", CompactionWithoutSpillStays},
        {"the visit budget grows only with lane compaction", VisitBudgetGrowsOnlyWithCompaction},
    };
    int failed = 0;
    for (const auto& [name, run] : cases) {
        try {
            run();
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s: %s\n", name, error.what());
            failed = 1;
        }
    }
    return failed;
}
