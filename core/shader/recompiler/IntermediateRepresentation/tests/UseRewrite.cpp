#include "IntermediateRepresentation/IrBlock.hpp"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <random>
#include <stdexcept>
#include <vector>

using namespace ShaderRecompiler;

namespace IrRewriteAllocation {
extern std::ptrdiff_t failureAfter;
extern std::size_t allocations;
extern bool countAllocations;
}
using namespace IrRewriteAllocation;

void Require(bool condition) {
    if (!condition) throw std::runtime_error("IR use rewrite assertion failed");
}

struct Graph {
    IrBlock block{1};
    std::vector<std::unique_ptr<IrValue>> values;

    explicit Graph(std::size_t count) {
        for (std::size_t index = 0; index < count; ++index) {
            auto value = std::make_unique<IrValue>(IrOpcode::IAdd32, IrType::U32, static_cast<std::uint32_t>(index));
            value->SetFlags(static_cast<std::uint64_t>(index * 17));
            block.AppendInstruction(value.get());
            values.push_back(std::move(value));
        }
    }

    IrValue* Value(std::size_t index) const { return values.at(index).get(); }
};

void ReferenceReplace(IrValue* source, IrValue* replacement) {
    if (source == replacement) return;
    while (!source->OperandUses().empty()) {
        const IrUse use = source->OperandUses().back();
        use.user->ReplaceArgument(use.operand, replacement);
    }
}

void EqualGraphs(const Graph& first, const Graph& second) {
    Require(first.values.size() == second.values.size());
    for (std::size_t index = 0; index < first.values.size(); ++index) {
        const auto* left = first.Value(index);
        const auto* right = second.Value(index);
        Require(left->Opcode() == right->Opcode() && left->Type() == right->Type());
        Require(left->Id() == right->Id() && left->Flags<std::uint64_t>() == right->Flags<std::uint64_t>());
        Require((left->Parent() == nullptr) == (right->Parent() == nullptr));
        if (left->Parent()) Require(left->Parent()->Id() == right->Parent()->Id());
        Require(left->ArgumentCount() == right->ArgumentCount());
        for (std::size_t operand = 0; operand < left->ArgumentCount(); ++operand) {
            const auto* a = left->Argument(operand);
            const auto* b = right->Argument(operand);
            Require((a == nullptr) == (b == nullptr));
            if (a) Require(a->Id() == b->Id());
        }
        Require(left->Uses().size() == right->Uses().size());
        for (std::size_t use = 0; use < left->Uses().size(); ++use) {
            Require(left->Uses()[use]->Id() == right->Uses()[use]->Id());
        }
        Require(left->OperandUses().size() == right->OperandUses().size());
        for (std::size_t use = 0; use < left->OperandUses().size(); ++use) {
            const auto& a = left->OperandUses()[use];
            const auto& b = right->OperandUses()[use];
            Require(a.user->Id() == b.user->Id() && a.operand == b.operand);
        }
    }
}

void CheckSimpleReplacement() {
    Graph graph(5);
    auto* source = graph.Value(0);
    auto* target = graph.Value(1);
    auto* first = graph.Value(2);
    auto* second = graph.Value(3);
    auto* existing = graph.Value(4);
    existing->AddArgument(target);
    first->AddArgument(source);
    second->AddArgument(source);
    first->AddArgument(source);
    first->AddArgument(target);
    source->ReplaceAllUsesWith(target);
    Require(!source->HasUses() && source->OperandUses().empty());
    Require(first->Argument(0) == target && first->Argument(1) == target && first->Argument(2) == target);
    Require(second->Argument(0) == target);
    const IrUseList expected{{existing, 0}, {first, 2}, {first, 1}, {second, 0}, {first, 0}};
    Require(target->OperandUses() == expected);
    Require(std::ranges::equal(target->Uses(), std::vector<IrValue*>({existing, first, first, second, first})));
    first->ReplaceArgument(1, source);
    Require(source->UseCount() == 1);
    source->ReplaceAllUsesWith(nullptr);
    Require(first->Argument(1) == nullptr && target->UseCount() == 4);
    Require(source->Opcode() == IrOpcode::IAdd32 && source->Parent() == &graph.block);
}

void CheckTailArgumentRemoval() {
    Graph graph(4);
    auto* source = graph.Value(0);
    auto* first = graph.Value(1);
    auto* last = graph.Value(2);
    first->AddArgument(source);
    last->AddArgument(source);
    last->AddArgument(source);
    last->ReplaceArgument(0, nullptr);
    Require(source->OperandUses() == IrUseList({{first, 0}, {last, 1}}));
    last->ReplaceArgument(1, graph.Value(3));
    Require(source->OperandUses() == IrUseList({{first, 0}}));
    Require(graph.Value(3)->OperandUses() == IrUseList({{last, 1}}));
    first->Invalidate();
    Require(!source->HasUses() && first->Argument(0) == nullptr);
}

void CheckSelfAndEmpty() {
    Graph graph(3);
    graph.Value(2)->AddArgument(graph.Value(0));
    graph.Value(0)->ReplaceAllUsesWith(graph.Value(0));
    Require(graph.Value(0)->UseCount() == 1 && graph.Value(2)->Argument(0) == graph.Value(0));
    graph.Value(1)->ReplaceAllUsesWith(graph.Value(0));
    Require(graph.Value(0)->UseCount() == 1);
    graph.Value(1)->ReplaceAllUsesWith(nullptr);
    Require(!graph.Value(1)->HasUses());
}

void CheckPhiAndIdentity() {
    Graph graph(4);
    auto* source = graph.Value(0);
    auto* target = graph.Value(1);
    auto* phi = graph.Value(2);
    phi->ReplaceOpcode(IrOpcode::Phi);
    phi->AddPhiOperand(&graph.block, source);
    phi->AddPhiOperand(&graph.block, source);
    graph.Value(3)->ReplaceOpcode(IrOpcode::Identity);
    graph.Value(3)->AddArgument(source);
    source->ReplaceUsesWith(target, false);
    Require(source->Opcode() == IrOpcode::Unreachable);
    Require(phi->Argument(0) == target && phi->Argument(1) == target);
    Require(phi->PhiBlockCount() == 2 && phi->PhiBlock(0) == &graph.block && phi->PhiBlock(1) == &graph.block);
    Require(graph.Value(3)->Resolve() == target);
    Require(target->UseCount() == 3);
}

void CheckAllocationFailure() {
    for (std::ptrdiff_t failure = 0; failure < 1; ++failure) {
        Graph graph(12);
        for (std::size_t index = 2; index < 12; ++index) {
            graph.Value(index)->AddArgument(graph.Value(0));
        }
        bool caught = false;
        failureAfter = failure;
        try {
            graph.Value(0)->ReplaceAllUsesWith(graph.Value(1));
        } catch (const std::bad_alloc&) {
            caught = true;
        } catch (...) {
            failureAfter = -1;
            throw;
        }
        failureAfter = -1;
        Require(caught && graph.Value(0)->UseCount() == 10);
        Require(graph.Value(0)->OperandUses().size() == 10);
        Require(!graph.Value(1)->HasUses() && graph.Value(1)->OperandUses().empty());
        for (std::size_t index = 2; index < 12; ++index) {
            Require(graph.Value(index)->Argument(0) == graph.Value(0));
        }
        graph.Value(0)->ReplaceAllUsesWith(graph.Value(1));
        Require(graph.Value(1)->UseCount() == 10);
    }
}

void CheckGeometricGrowth() {
    constexpr std::size_t count = 4096;
    Graph graph(count + 2);
    std::vector<std::unique_ptr<IrValue>> sources;
    for (std::size_t index = 2; index < count + 2; ++index) {
        sources.push_back(std::make_unique<IrValue>(IrOpcode::UndefU32, IrType::U32, index + count + 2));
        graph.Value(index)->AddArgument(sources.back().get());
    }
    allocations = 0;
    countAllocations = true;
    for (auto& source : sources) source->ReplaceAllUsesWith(graph.Value(1));
    countAllocations = false;
    Require(allocations <= std::bit_width(count) + 1);
    Require(graph.Value(1)->UseCount() == count);
    for (std::size_t index = 2; index < count + 2; ++index) Require(graph.Value(index)->Argument(0) == graph.Value(1));
}

void CheckLargeFanOut() {
    constexpr std::size_t count = 100000;
    Graph graph(count + 2);
    for (std::size_t index = 2; index < count + 2; ++index) {
        graph.Value(index)->AddArgument(graph.Value(0));
        graph.Value(index)->AddArgument(graph.Value(0));
    }
    graph.Value(0)->ReplaceAllUsesWith(graph.Value(1));
    Require(graph.Value(1)->UseCount() == count * 2 && !graph.Value(0)->HasUses());
    for (std::size_t index = 2; index < count + 2; ++index) {
        Require(graph.Value(index)->Argument(0) == graph.Value(1) && graph.Value(index)->Argument(1) == graph.Value(1));
    }
}

void CheckDifferentialPrograms() {
    std::mt19937 random(0x4f23a771);
    constexpr std::size_t count = 32;
    for (std::size_t trial = 0; trial < 2000; ++trial) {
        Graph actual(count), reference(count);
        for (std::size_t index = 0; index < count; ++index) {
            const auto operands = random() % 5;
            for (std::size_t operand = 0; operand < operands; ++operand) {
                const auto argument = random() % (count + 1);
                actual.Value(index)->AddArgument(argument == count ? nullptr : actual.Value(argument));
                reference.Value(index)->AddArgument(argument == count ? nullptr : reference.Value(argument));
            }
        }
        for (std::size_t step = 0; step < 24; ++step) {
            const auto source = random() % count;
            const auto replacement = random() % (count + 1);
            if (random() % 4 == 0 && actual.Value(source)->ArgumentCount() > 0) {
                const auto operand = random() % actual.Value(source)->ArgumentCount();
                actual.Value(source)->ReplaceArgument(operand, replacement == count ? nullptr : actual.Value(replacement));
                reference.Value(source)->ReplaceArgument(operand, replacement == count ? nullptr : reference.Value(replacement));
            } else {
                actual.Value(source)->ReplaceAllUsesWith(replacement == count ? nullptr : actual.Value(replacement));
                ReferenceReplace(reference.Value(source), replacement == count ? nullptr : reference.Value(replacement));
            }
            EqualGraphs(actual, reference);
        }
    }
}

int main() {
    const char* current = "setup";
    try {
        const std::pair<const char*, void (*)()> cases[] = {
            {"ordered duplicate operands", CheckSimpleReplacement},
            {"tail argument removal", CheckTailArgumentRemoval},
            {"self and empty replacement", CheckSelfAndEmpty},
            {"phi and identity rewriting", CheckPhiAndIdentity},
            {"allocation failure integrity", CheckAllocationFailure},
            {"geometric use-list growth", CheckGeometricGrowth},
            {"large fan-out", CheckLargeFanOut},
            {"differential use graphs", CheckDifferentialPrograms}
        };
        for (const auto& [name, test] : cases) {
            current = name;
            test();
        }
        std::puts("8 IR rewrite suites passed; 2000 differential use graphs and 48000 mutation comparisons");
        return 0;
    } catch (const std::exception& error) {
        failureAfter = -1;
        countAllocations = false;
        std::fprintf(stderr, "%s: %s\n", current, error.what());
        return 1;
    }
}
