#include <Testing/Test.hpp>
#include "IntermediateRepresentation/IrProgram.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

using namespace ShaderRecompiler;
using Testing::Case;
using Testing::Require;

IrValue& Constant(IrProgram& program, std::uint32_t value) {
    auto& created = program.CreateValue(IrOpcode::Void, IrType::U32);
    created.SetImmediateU32(value);
    return created;
}

IrValue& Add(IrProgram& program, IrValue& left, IrValue& right) {
    auto& created = program.CreateValue(IrOpcode::IAdd32, IrType::U32);
    created.AddArgument(&left);
    created.AddArgument(&right);
    return created;
}

IrBlock& Entry(IrProgram& program) {
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    BlockInfo info;
    info.id = 0;
    program.Metadata().blockInfo.push_back(info);
    return block;
}

template<typename TBuild>
void RequireRejection(const char* name, const TBuild& build, const std::string& expected) {
    const auto text = Testing::RequireThrowsMessage<std::exception>(build, std::string(name) + ": accepted, expected \"" + expected + "\"");
    Require(text.find(expected) != std::string::npos,
            std::string(name) + ": rejected with \"" + text + "\", expected \"" + expected + "\"");
}

void ValidateCollidingIds(bool useFirst) {
    IrProgram other;
    Entry(other);
    auto& otherOne = Constant(other, 1);
    auto& foreign = Add(other, otherOne, otherOne);

    IrProgram program;
    auto& block = Entry(program);
    auto& one = Constant(program, 1);
    auto& sum = Add(program, one, one);
    Require(sum.Id() == foreign.Id(), "test setup: the ids differ");
    auto& use = Add(program, foreign, one);
    block.AppendInstruction(&sum);
    if (useFirst) block.AppendInstruction(&use);
    block.AppendInstruction(&foreign);
    if (!useFirst) block.AppendInstruction(&use);
    ValidateProgram(program, true);
}

const Case validProgram{"ValidateProgram_DefinitionsBeforeUses_IsAccepted", [] {
    IrProgram program;
    auto& block = Entry(program);
    auto& one = Constant(program, 1);
    auto& sum = Add(program, one, one);
    block.AppendInstruction(&sum);
    block.AppendInstruction(&Add(program, sum, one));
    ValidateProgram(program, true);
}};

const Case useBeforeDefinition{"ValidateProgram_UseBeforeSameBlockDefinition_IsRejected", [] {
    RequireRejection("use before definition", [] {
        IrProgram program;
        auto& block = Entry(program);
        auto& one = Constant(program, 1);
        auto& sum = Add(program, one, one);
        block.AppendInstruction(&Add(program, sum, one));
        block.AppendInstruction(&sum);
        ValidateProgram(program, true);
    }, "uses a same-block definition before it");
}};

const Case duplicatedInstruction{"ValidateProgram_DuplicatedInstruction_IsRejected", [] {
    RequireRejection("duplicated instruction", [] {
        IrProgram program;
        auto& block = Entry(program);
        auto& one = Constant(program, 1);
        auto& sum = Add(program, one, one);
        block.AppendInstruction(&sum);
        block.Instructions().push_back(&sum);
        ValidateProgram(program, true);
    }, "instruction is duplicated");
}};

const Case foreignDefinition{"ValidateProgram_ForeignDefinitionWithLocalId_IsRejected", [] {
    RequireRejection("foreign definition with a local id", [] {
        IrProgram other;
        auto& otherBlock = Entry(other);
        auto& otherOne = Constant(other, 1);
        auto& foreign = Add(other, otherOne, otherOne);
        otherBlock.AppendInstruction(&foreign);

        IrProgram program;
        auto& block = Entry(program);
        auto& one = Constant(program, 1);
        auto& sum = Add(program, one, one);
        block.AppendInstruction(&sum);
        if (sum.Id() != foreign.Id()) throw std::logic_error("test setup: the ids differ");
        block.AppendInstruction(&Add(program, foreign, one));
        ValidateProgram(program, true);
    }, "foreign definition");
}};

const Case collidingIdsInOrder{"ValidateProgram_CollidingIdsInDefinitionOrder_IsAccepted", [] {
    ValidateCollidingIds(false);
}};

const Case collidingIdsOutOfOrder{"ValidateProgram_CollidingIdsOutOfOrder_IsRejected", [] {
    RequireRejection("colliding ids out of order", [] { ValidateCollidingIds(true); }, "uses a same-block definition before it");
}};

} // namespace
