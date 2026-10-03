#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
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

    return passed ? 0 : 1;
}
