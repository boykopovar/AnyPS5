#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

IrValue& constant(IrProgram& program, std::uint32_t value) {
    auto& created = program.CreateValue(IrOpcode::Void, IrType::U32);
    created.SetImmediateU32(value);
    return created;
}

IrValue& add(IrProgram& program, IrValue& left, IrValue& right) {
    auto& created = program.CreateValue(IrOpcode::IAdd32, IrType::U32);
    created.AddArgument(&left);
    created.AddArgument(&right);
    return created;
}

IrBlock& entry(IrProgram& program) {
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    BlockInfo info;
    info.id = 0;
    program.Metadata().blockInfo.push_back(info);
    return block;
}

IrBlock& block(IrProgram& program, std::uint32_t id) {
    auto& created = program.CreateBlock();
    program.BlockOrder().push_back(&created);
    BlockInfo info;
    info.id = id;
    program.Metadata().blockInfo.push_back(info);
    return created;
}

void branch(IrProgram& program, std::size_t from, std::initializer_list<std::size_t> targets) {
    auto& source = *program.BlockOrder()[from];
    auto& info = program.Metadata().blockInfo[from];
    for (const auto target : targets) {
        auto& destination = *program.BlockOrder()[target];
        source.AddSuccessor(&destination);
        destination.AddPredecessor(&source);
    }
    const auto id = [&](std::size_t index) { return program.Metadata().blockInfo[index].id; };
    if (targets.size() == 1) {
        info.terminator.kind = TerminatorKind::Branch;
        info.terminator.trueBlock = id(*targets.begin());
        return;
    }
    info.terminator.kind = TerminatorKind::ConditionalBranch;
    info.terminator.trueBlock = id(*targets.begin());
    info.terminator.falseBlock = id(*(targets.begin() + 1));
    auto& condition = program.CreateValue(IrOpcode::Void, IrType::Bool);
    info.condition = &condition;
}

struct Diamond {
    IrProgram program;
    IrBlock* blocks[4];
    IrValue* one;

    Diamond() {
        blocks[0] = &entry(program);
        for (std::uint32_t id = 1; id < 4; ++id) blocks[id] = &block(program, id);
        branch(program, 0, {1, 2});
        branch(program, 1, {3});
        branch(program, 2, {3});
        one = &constant(program, 1);
    }

    IrValue& define(std::size_t at) {
        auto& value = add(program, *one, *one);
        blocks[at]->AppendInstruction(&value);
        return value;
    }
};

struct Loop {
    IrProgram program;
    IrBlock* blocks[3];
    IrValue* one;

    Loop() {
        blocks[0] = &entry(program);
        for (std::uint32_t id = 1; id < 3; ++id) blocks[id] = &block(program, id);
        branch(program, 0, {1});
        branch(program, 1, {1, 2});
        one = &constant(program, 1);
    }
};

bool rejects(const char* name, const std::function<void()>& build, const std::string& expected) {
    try {
        build();
    } catch (const std::exception& error) {
        if (std::string(error.what()).find(expected) != std::string::npos) return true;
        std::fprintf(stderr, "%s: rejected with \"%s\", expected \"%s\"\n", name, error.what(), expected.c_str());
        return false;
    }
    std::fprintf(stderr, "%s: accepted, expected \"%s\"\n", name, expected.c_str());
    return false;
}

}

int main() {
    bool passed = true;
    try {
        IrProgram program;
        auto& block = entry(program);
        auto& one = constant(program, 1);
        auto& sum = add(program, one, one);
        block.AppendInstruction(&sum);
        block.AppendInstruction(&add(program, sum, one));
        ValidateProgram(program, true);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "valid program: %s\n", error.what());
        passed = false;
    }

    passed &= rejects("use before definition", [] {
        IrProgram program;
        auto& block = entry(program);
        auto& one = constant(program, 1);
        auto& sum = add(program, one, one);
        block.AppendInstruction(&add(program, sum, one));
        block.AppendInstruction(&sum);
        ValidateProgram(program, true);
    }, "uses a same-block definition before it");

    passed &= rejects("duplicated instruction", [] {
        IrProgram program;
        auto& block = entry(program);
        auto& one = constant(program, 1);
        auto& sum = add(program, one, one);
        block.AppendInstruction(&sum);
        block.Instructions().push_back(&sum);
        ValidateProgram(program, true);
    }, "instruction is duplicated");

    passed &= rejects("foreign definition with a local id", [] {
        IrProgram other;
        auto& otherBlock = entry(other);
        auto& otherOne = constant(other, 1);
        auto& foreign = add(other, otherOne, otherOne);
        otherBlock.AppendInstruction(&foreign);

        IrProgram program;
        auto& block = entry(program);
        auto& one = constant(program, 1);
        auto& sum = add(program, one, one);
        block.AppendInstruction(&sum);
        if (sum.Id() != foreign.Id()) throw std::runtime_error("test setup: the ids differ");
        block.AppendInstruction(&add(program, foreign, one));
        ValidateProgram(program, true);
    }, "foreign definition");

    const auto collidingIds = [](bool useFirst) {
        IrProgram other;
        entry(other);
        auto& otherOne = constant(other, 1);
        auto& foreign = add(other, otherOne, otherOne);

        IrProgram program;
        auto& block = entry(program);
        auto& one = constant(program, 1);
        auto& sum = add(program, one, one);
        if (sum.Id() != foreign.Id()) throw std::runtime_error("test setup: the ids differ");
        auto& use = add(program, foreign, one);
        block.AppendInstruction(&sum);
        if (useFirst) block.AppendInstruction(&use);
        block.AppendInstruction(&foreign);
        if (!useFirst) block.AppendInstruction(&use);
        ValidateProgram(program, true);
    };
    try {
        collidingIds(false);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "colliding ids in order: %s\n", error.what());
        passed = false;
    }
    passed &= rejects("colliding ids out of order", [&] { collidingIds(true); }, "uses a same-block definition before it");

    const auto accepts = [&](const char* name, const std::function<void()>& build) {
        try {
            build();
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s: %s\n", name, error.what());
            passed = false;
        }
    };
    accepts("diamond use dominated by the entry", [] {
        Diamond diamond;
        auto& value = diamond.define(0);
        diamond.blocks[3]->AppendInstruction(&add(diamond.program, value, *diamond.one));
        ValidateProgram(diamond.program, true);
    });
    passed &= rejects("diamond use of one arm's definition at the merge", [] {
        Diamond diamond;
        auto& value = diamond.define(1);
        diamond.blocks[3]->AppendInstruction(&add(diamond.program, value, *diamond.one));
        ValidateProgram(diamond.program, true);
    }, "definition does not dominate its use");
    passed &= rejects("diamond use of one arm's definition in the other arm", [] {
        Diamond diamond;
        auto& value = diamond.define(1);
        diamond.blocks[2]->AppendInstruction(&add(diamond.program, value, *diamond.one));
        ValidateProgram(diamond.program, true);
    }, "definition does not dominate its use");
    accepts("diamond Phi of both arms", [] {
        Diamond diamond;
        auto& left = diamond.define(1);
        auto& right = diamond.define(2);
        auto& phi = diamond.program.CreateValue(IrOpcode::Phi, IrType::U32);
        phi.AddPhiOperand(diamond.blocks[1], &left);
        phi.AddPhiOperand(diamond.blocks[2], &right);
        diamond.blocks[3]->AppendInstruction(&phi);
        ValidateProgram(diamond.program, true);
    });
    passed &= rejects("diamond Phi edge from an arm that the definition does not dominate", [] {
        Diamond diamond;
        auto& left = diamond.define(1);
        auto& right = diamond.define(2);
        auto& phi = diamond.program.CreateValue(IrOpcode::Phi, IrType::U32);
        phi.AddPhiOperand(diamond.blocks[1], &right);
        phi.AddPhiOperand(diamond.blocks[2], &left);
        diamond.blocks[3]->AppendInstruction(&phi);
        ValidateProgram(diamond.program, true);
    }, "Phi incoming definition does not dominate its edge");
    passed &= rejects("diamond branch condition defined in an arm", [] {
        Diamond diamond;
        auto& condition = diamond.program.CreateValue(IrOpcode::IEqual32, IrType::Bool);
        condition.AddArgument(diamond.one);
        condition.AddArgument(diamond.one);
        diamond.blocks[1]->AppendInstruction(&condition);
        diamond.program.Metadata().blockInfo[0].condition = &condition;
        ValidateProgram(diamond.program, true);
    }, "branch condition definition does not dominate its use");
    accepts("loop body definition used after the loop", [] {
        Loop loop;
        auto& value = add(loop.program, *loop.one, *loop.one);
        loop.blocks[1]->AppendInstruction(&value);
        loop.blocks[2]->AppendInstruction(&add(loop.program, value, *loop.one));
        ValidateProgram(loop.program, true);
    });
    passed &= rejects("loop exit definition used in the loop body", [] {
        Loop loop;
        auto& value = add(loop.program, *loop.one, *loop.one);
        loop.blocks[2]->AppendInstruction(&value);
        loop.blocks[1]->AppendInstruction(&add(loop.program, value, *loop.one));
        ValidateProgram(loop.program, true);
    }, "definition does not dominate its use");

    return passed ? 0 : 1;
}
