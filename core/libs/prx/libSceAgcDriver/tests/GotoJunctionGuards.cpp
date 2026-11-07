#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/GotoPhiRefiner.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace ShaderRecompiler;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Graph {
    IrProgram program;
    std::vector<IrBlock*> blocks;
};

void MakeBlocks(Graph& graph, std::uint32_t count) {
    for (std::uint32_t id = 0; id < count; ++id) {
        auto& block = graph.program.CreateBlock();
        graph.program.BlockOrder().push_back(&block);
        BlockInfo info;
        info.id = id;
        info.terminator.kind = TerminatorKind::Return;
        graph.program.Metadata().blockInfo.push_back(info);
        graph.blocks.push_back(&block);
    }
    graph.program.SetEntryBlock(*graph.blocks[0]);
}

void Branch(Graph& graph, std::uint32_t from, std::uint32_t to) {
    graph.blocks[from]->AddBranch(graph.blocks[to]);
    auto& terminator = graph.program.Metadata().blockInfo[from].terminator;
    terminator.kind = TerminatorKind::Branch;
    terminator.trueBlock = to;
}

void BranchIf(Graph& graph, std::uint32_t from, IrValue& condition, std::uint32_t whenTrue, std::uint32_t whenFalse) {
    graph.blocks[from]->AddBranch(graph.blocks[whenTrue]);
    graph.blocks[from]->AddBranch(graph.blocks[whenFalse]);
    auto& info = graph.program.Metadata().blockInfo[from];
    info.terminator.kind = TerminatorKind::ConditionalBranch;
    info.terminator.trueBlock = whenTrue;
    info.terminator.falseBlock = whenFalse;
    info.condition = &condition;
}

IrValue& Phi(IrBuilder& builder, IrBlock& block, IrType type, std::initializer_list<std::pair<IrBlock*, IrValue*>> operands) {
    builder.SetInsertionPoint(block);
    auto& phi = builder.Emit(IrOpcode::Phi, type, {});
    for (const auto& [predecessor, value] : operands) phi.AddPhiOperand(predecessor, value);
    return phi;
}

std::uint32_t Refine(Graph& graph) {
    ValidateProgram(graph.program, true);
    const auto replaced = GotoPhiRefiner{}.Refine(graph.program).replacedOperands;
    ValidateProgram(graph.program, true);
    return replaced;
}

void CheckJunctionGuard(bool constantGuard) {
    Graph graph;
    MakeBlocks(graph, 6u);
    IrBlock* entry = graph.blocks[0];
    IrBlock* taken = graph.blocks[1];
    IrBlock* other = graph.blocks[2];
    IrBlock* junction = graph.blocks[3];
    IrBlock* below = graph.blocks[4];
    IrBuilder builder(graph.program);
    builder.SetInsertionPoint(*entry);
    auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    auto& split = builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {&lane, &builder.Constant(16u)});
    auto& runtimeGuard = builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {&lane, &builder.Constant(8u)});
    auto& takenValue = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&lane, &builder.Constant(1u)});
    auto& otherValue = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&lane, &builder.Constant(2u)});
    auto& yes = builder.ConstantBool(true);
    auto& no = builder.ConstantBool(false);
    BranchIf(graph, 0u, split, 1u, 2u);
    Branch(graph, 1u, 3u);
    Branch(graph, 2u, 3u);
    auto& guard = Phi(builder, *junction, IrType::Bool, {{taken, &yes}, {other, constantGuard ? &no : &runtimeGuard}});
    auto& merged = Phi(builder, *junction, IrType::U32, {{taken, &takenValue}, {other, &otherValue}});
    BranchIf(graph, 3u, guard, 4u, 5u);
    builder.SetInsertionPoint(*below);
    auto& use = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&merged});
    const auto replaced = Refine(graph);
    if (constantGuard) {
        require(replaced == 1u && use.Argument(0)->Resolve() == &takenValue, "a junction Phi whose other arm's goto guard is constant false was not narrowed below the true branch");
    } else {
        require(replaced == 0u && use.Argument(0) == &merged, "a junction Phi arm whose goto guard is not a constant was dropped");
    }
}

void CheckNestedJunctionAfterJunction(bool nestedValue) {
    Graph graph;
    MakeBlocks(graph, 7u);
    IrBlock* entry = graph.blocks[0];
    IrBlock* junction = graph.blocks[1];
    IrBlock* left = graph.blocks[2];
    IrBlock* right = graph.blocks[3];
    IrBlock* nested = graph.blocks[4];
    IrBlock* below = graph.blocks[5];
    IrBlock* latch = graph.blocks[6];
    IrBuilder builder(graph.program);
    builder.SetInsertionPoint(*entry);
    auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    auto& split = builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {&lane, &builder.Constant(16u)});
    auto& entryValue = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&lane, &builder.Constant(1u)});
    auto& initial = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&lane, &builder.Constant(2u)});
    auto& yes = builder.ConstantBool(true);
    auto& no = builder.ConstantBool(false);
    Branch(graph, 0u, 1u);
    auto& guard = Phi(builder, *junction, IrType::Bool, {{entry, &no}});
    auto& merged = Phi(builder, *junction, IrType::U32, {{entry, &initial}});
    BranchIf(graph, 1u, split, 2u, 3u);
    builder.SetInsertionPoint(*left);
    auto& leftValue = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&lane, &builder.Constant(3u)});
    Branch(graph, 2u, 4u);
    builder.SetInsertionPoint(*right);
    auto& rightValue = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&lane, &builder.Constant(4u)});
    Branch(graph, 3u, 4u);
    auto& nestedPhi = Phi(builder, *nested, IrType::U32, {{left, &leftValue}, {right, &rightValue}});
    BranchIf(graph, 4u, guard, 5u, 6u);
    builder.SetInsertionPoint(*below);
    auto& use = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&merged});
    Branch(graph, 6u, 1u);
    guard.AddPhiOperand(latch, &yes);
    merged.AddPhiOperand(latch, nestedValue ? &nestedPhi : &entryValue);
    const auto replaced = Refine(graph);
    if (nestedValue) {
        require(replaced == 0u && use.Argument(0) == &merged, "a junction Phi was narrowed to a nested junction value that is recomputed between the junction and the use");
    } else {
        require(replaced == 1u && use.Argument(0)->Resolve() == &entryValue, "a junction Phi whose latch arm is an entry value was not narrowed below the true branch");
    }
}

void CheckJunctionRerunBeforeUse() {
    Graph graph;
    MakeBlocks(graph, 6u);
    IrBlock* entry = graph.blocks[0];
    IrBlock* junction = graph.blocks[1];
    IrBlock* latch = graph.blocks[3];
    IrBlock* after = graph.blocks[5];
    IrBuilder builder(graph.program);
    builder.SetInsertionPoint(*entry);
    auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    auto& entryValue = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&lane, &builder.Constant(1u)});
    auto& latchValue = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&lane, &builder.Constant(2u)});
    auto& leave = builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {&lane, &builder.Constant(16u)});
    auto& yes = builder.ConstantBool(true);
    auto& no = builder.ConstantBool(false);
    Branch(graph, 0u, 1u);
    auto& guard = Phi(builder, *junction, IrType::Bool, {{entry, &yes}});
    auto& merged = Phi(builder, *junction, IrType::U32, {{entry, &entryValue}});
    BranchIf(graph, 1u, guard, 2u, 4u);
    BranchIf(graph, 2u, leave, 5u, 3u);
    Branch(graph, 3u, 1u);
    Branch(graph, 4u, 5u);
    guard.AddPhiOperand(latch, &no);
    merged.AddPhiOperand(latch, &latchValue);
    builder.SetInsertionPoint(*after);
    auto& use = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&merged});
    const auto replaced = Refine(graph);
    require(replaced == 0u && use.Argument(0) == &merged, "a junction Phi was narrowed at a use the junction can reach again without the goto branch");
}

void CheckLoopHeaderInnerJunction() {
    Graph graph;
    MakeBlocks(graph, 8u);
    IrBlock* entry = graph.blocks[0];
    IrBlock* header = graph.blocks[1];
    IrBlock* body = graph.blocks[2];
    IrBlock* latch = graph.blocks[3];
    IrBlock* exit = graph.blocks[4];
    IrBlock* junction = graph.blocks[5];
    IrBlock* below = graph.blocks[6];
    IrBuilder builder(graph.program);
    builder.SetInsertionPoint(*entry);
    auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    auto& initial = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&lane, &builder.Constant(2u)});
    auto& yes = builder.ConstantBool(true);
    auto& no = builder.ConstantBool(false);
    Branch(graph, 0u, 1u);
    auto& innerGuard = Phi(builder, *header, IrType::Bool, {{entry, &no}});
    auto& innerPhi = Phi(builder, *header, IrType::U32, {{entry, &initial}});
    Branch(graph, 1u, 2u);
    builder.SetInsertionPoint(*body);
    auto& counter = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&innerPhi, &builder.Constant(1u)});
    auto& leave = builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {&counter, &builder.Constant(16u)});
    BranchIf(graph, 2u, leave, 4u, 3u);
    Branch(graph, 3u, 1u);
    innerGuard.AddPhiOperand(latch, &yes);
    innerPhi.AddPhiOperand(latch, &counter);
    Branch(graph, 4u, 5u);
    auto& guard = Phi(builder, *junction, IrType::Bool, {{exit, &innerGuard}});
    auto& merged = Phi(builder, *junction, IrType::U32, {{exit, &innerPhi}});
    BranchIf(graph, 5u, guard, 6u, 7u);
    builder.SetInsertionPoint(*below);
    auto& use = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&merged});
    const auto replaced = Refine(graph);
    require(replaced == 0u && use.Argument(0) == &merged, "a junction Phi was narrowed to a loop value redefined after the inner junction it came through");
}

void CheckDeepJunctionChain() {
    constexpr std::uint32_t Stages = 48u;
    Graph graph;
    MakeBlocks(graph, 1u + Stages * 5u);
    IrBuilder builder(graph.program);
    builder.SetInsertionPoint(*graph.blocks[0]);
    auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    auto& split = builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {&lane, &builder.Constant(16u)});
    auto& base = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&lane, &builder.Constant(1u)});
    auto& other = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&lane, &builder.Constant(2u)});
    auto& yes = builder.ConstantBool(true);
    auto& no = builder.ConstantBool(false);
    IrValue* value = &base;
    std::uint32_t from = 0u;
    for (std::uint32_t stage = 0; stage < Stages; ++stage) {
        const std::uint32_t taken = 1u + stage * 5u;
        const std::uint32_t skipped = taken + 1u;
        const std::uint32_t junction = taken + 2u;
        const std::uint32_t below = taken + 3u;
        BranchIf(graph, from, split, taken, skipped);
        Branch(graph, taken, junction);
        Branch(graph, skipped, junction);
        auto& guard = Phi(builder, *graph.blocks[junction], IrType::Bool, {{graph.blocks[taken], &yes}, {graph.blocks[skipped], &no}});
        value = &Phi(builder, *graph.blocks[junction], IrType::U32, {{graph.blocks[taken], value}, {graph.blocks[skipped], &other}});
        BranchIf(graph, junction, guard, below, taken + 4u);
        from = below;
    }
    builder.SetInsertionPoint(*graph.blocks[from]);
    auto& use = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {value});
    const auto replaced = Refine(graph);
    require(replaced != 0u && use.Argument(0)->Resolve() == &base, "a junction Phi below 48 chained junctions was not narrowed to the value that entered the first one");
}

}

int main() {
    const std::vector<std::pair<const char*, void (*)()>> checks{
        {"constant guard", [] { CheckJunctionGuard(true); }},
        {"non-constant guard", [] { CheckJunctionGuard(false); }},
        {"entry value across the loop", [] { CheckNestedJunctionAfterJunction(false); }},
        {"nested junction value after the junction", [] { CheckNestedJunctionAfterJunction(true); }},
        {"junction rerun before the use", CheckJunctionRerunBeforeUse},
        {"loop header inner junction", CheckLoopHeaderInnerJunction},
        {"deep junction chain", CheckDeepJunctionChain},
    };
    int failures = 0;
    for (const auto& [name, check] : checks) {
        try {
            check();
        } catch (const std::exception& error) {
            std::fprintf(stderr, "goto junction guards: %s: %s\n", name, error.what());
            ++failures;
        }
    }
    if (failures != 0) return 1;
    std::puts("Goto junction guard tests passed");
    return 0;
}
