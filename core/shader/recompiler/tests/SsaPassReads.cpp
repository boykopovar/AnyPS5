#include "Optimization/SsaBuilder/SsaPass.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;
using namespace ShaderRecompiler::Detail;

namespace {

void Require(bool condition, const char* what) {
    if (!condition) throw std::runtime_error(what);
}

IrValue& Constant(IrProgram& program, Variable variable, std::uint32_t number) {
    auto& value = program.CreateValue(IrOpcode::Void, VariableType(variable));
    if (VariableType(variable) == IrType::Bool) {
        value.SetImmediateBool(number != 0u);
    } else {
        value.SetImmediateU32(number);
    }
    return value;
}

std::size_t InstructionCount(const IrBlock& block) {
    return block.Instructions().size();
}

void CheckDiamondAndLoop(Variable variable) {
    IrProgram program;
    Pass pass(program);
    auto& entry = program.CreateBlock();
    auto& left = program.CreateBlock();
    auto& right = program.CreateBlock();
    auto& merge = program.CreateBlock();
    left.AddPredecessor(&entry);
    right.AddPredecessor(&entry);
    merge.AddPredecessor(&left);
    merge.AddPredecessor(&right);
    auto& first = Constant(program, variable, 0u);
    auto& second = Constant(program, variable, 1u);
    pass.Write(variable, &entry, &first);
    pass.Seal(&entry);
    pass.Seal(&left);
    pass.Seal(&right);
    Require(pass.Read(variable, &entry) == &first, "a read in the defining block does not return the definition");
    Require(pass.Read(variable, &left)->Resolve() == &first, "a single-predecessor read does not reach the definition");
    Require(pass.Read(variable, &right)->Resolve() == &first, "a second single-predecessor read does not reach the definition");
    pass.Write(variable, &right, &second);
    Require(pass.Read(variable, &right) == &second, "a read after a replacement write does not return the new value");
    pass.Seal(&merge);
    auto* phi = pass.Read(variable, &merge)->Resolve();
    Require(phi->IsPhi() && phi->ArgumentCount() == 2u, "a merge of two different values is not a two-operand phi");
    Require(phi->PhiBlock(0) == &left && phi->Argument(0)->Resolve() == &first, "the phi's first operand is not the left value");
    Require(phi->PhiBlock(1) == &right && phi->Argument(1)->Resolve() == &second, "the phi's second operand is not the right value");
    const auto mergeInstructions = InstructionCount(merge);
    const auto leftInstructions = InstructionCount(left);
    for (unsigned read = 0; read < 64u; ++read) Require(pass.Read(variable, &merge)->Resolve() == phi, "a repeated merge read returns another value");
    for (unsigned read = 0; read < 64u; ++read) Require(pass.Read(variable, &left)->Resolve() == &first, "a repeated predecessor read returns another value");
    Require(InstructionCount(merge) == mergeInstructions && InstructionCount(left) == leftInstructions, "a repeated read inserted instructions");
    pass.Write(variable, &merge, &second);
    Require(pass.Read(variable, &merge) == &second, "a merge read after a replacement write does not return the new value");
    auto& loop = program.CreateBlock();
    loop.AddPredecessor(&entry);
    loop.AddPredecessor(&loop);
    auto* pending = pass.Read(variable, &loop);
    Require(pending->IsPhi(), "a read in an unsealed loop header is not a pending phi");
    Require(pass.Read(variable, &loop) == pending, "a repeated read in an unsealed loop header returns another value");
    pass.Seal(&loop);
    Require(pass.Read(variable, &loop)->Resolve() == &first, "a loop that does not redefine the value does not read the entry value");
    Require(pending->Resolve() == &first, "the pending loop phi does not resolve to the entry value");
}

void CheckLoopCarriedValue(Variable variable) {
    IrProgram program;
    Pass pass(program);
    auto& entry = program.CreateBlock();
    auto& header = program.CreateBlock();
    auto& body = program.CreateBlock();
    header.AddPredecessor(&entry);
    header.AddPredecessor(&body);
    body.AddPredecessor(&header);
    auto& first = Constant(program, variable, 0u);
    auto& second = Constant(program, variable, 1u);
    pass.Write(variable, &entry, &first);
    pass.Seal(&entry);
    auto* pending = pass.Read(variable, &header);
    pass.Seal(&body);
    Require(pass.Read(variable, &body) == pending, "a body read does not reach the pending header phi");
    pass.Write(variable, &body, &second);
    pass.Seal(&header);
    auto* phi = pass.Read(variable, &header)->Resolve();
    Require(phi->IsPhi() && phi->ArgumentCount() == 2u, "a loop-carried value is not a two-operand header phi");
    Require(phi->Argument(0)->Resolve() == &first && phi->Argument(1)->Resolve() == &second, "the header phi's operands are not the entry and body values");
}

void CheckUndefined(Variable variable) {
    IrProgram program;
    Pass pass(program);
    auto& entry = program.CreateBlock();
    auto& next = program.CreateBlock();
    next.AddPredecessor(&entry);
    pass.Seal(&entry);
    pass.Seal(&next);
    auto* value = pass.Read(variable, &next)->Resolve();
    Require(value->HasImmediate(), "a read without a definition is not an initial constant");
    Require(VariableType(variable) == IrType::Bool ? !value->ImmediateBool() : value->ImmediateU32() == 0u, "the initial constant is not zero");
    Require(pass.Read(variable, &entry)->Resolve() == value, "the entry block does not keep the initial constant");
}

}

int main() {
    const std::array<Variable, 13> variables{ScalarReg{5}, ThreadBitScalarReg{ScalarReg{6}}, ScalarMaskTag{ScalarReg{7}}, VectorReg{8}, GotoVariable{9}, SccTag{}, ExecTag{}, ExecLoTag{}, ExecHiTag{}, VccTag{}, VccLoTag{}, VccHiTag{}, M0Tag{}};
    try {
        for (const auto& variable : variables) {
            CheckDiamondAndLoop(variable);
            CheckLoopCarriedValue(variable);
            CheckUndefined(variable);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SSA pass read test failed: %s\n", error.what());
        return 1;
    }
    return 0;
}
