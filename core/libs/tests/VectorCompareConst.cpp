#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaVectorOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("vector compare const regression"); }
static void Check(std::uint32_t encoding, RdnaOpcode opcode, bool exec) {
    const std::array<std::uint32_t, 1> code{(encoding << 17u) | (1u << 9u) | 100u};
    const RdnaInstruction instruction = DecodeRdnaVopc(0u, code, 0u);
    Require(instruction.op == opcode);
    Require(instruction.family == RdnaInstructionFamily::VOPC);
    Require(IsVectorAluOpcode(instruction.op));
    Require(instruction.destination.kind == (exec ? RdnaOperandKind::ExecLo : RdnaOperandKind::VccLo));
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);
}

static void CheckUnorderedInfinity(RdnaOpcode opcode, std::uint32_t constant, bool constantIsLhs) {
    RdnaInstruction instruction{};
    instruction.op = opcode;
    instruction.family = RdnaInstructionFamily::VOPC;
    instruction.sourceCount = 2u;
    instruction.destination.kind = RdnaOperandKind::VccLo;
    RdnaOperand& variable = constantIsLhs ? instruction.source1 : instruction.source0;
    variable.kind = RdnaOperandKind::VectorRegister;
    variable.reg = 1u;
    RdnaOperand& infinity = constantIsLhs ? instruction.source0 : instruction.source1;
    infinity.kind = RdnaOperandKind::FloatInlineConstant;
    infinity.value = constant;
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256u);
    context.TranslateInstruction(instruction);
    const IrValue* vccValue = nullptr;
    bool comparedFloats = false;
    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::SetVcc) vccValue = value->Argument(0u);
        comparedFloats = comparedFloats || value->Opcode() == IrOpcode::FPUnordLessThanEqual32 ||
            value->Opcode() == IrOpcode::FPUnordGreaterThanEqual32;
    }
    Require(vccValue != nullptr && !comparedFloats);
    Require(vccValue->Opcode() == IrOpcode::LogicalAnd);
    const IrValue* result = vccValue->Argument(1u)->Resolve();
    Require(result->HasImmediate() && result->ImmediateU32() == 1u);
}

int main() {
    Check(0xa0u, RdnaOpcode::VCmpFI64, false);
    Check(0xa7u, RdnaOpcode::VCmpTI64, false);
    Check(0xe0u, RdnaOpcode::VCmpFU64, false);
    Check(0xe7u, RdnaOpcode::VCmpTU64, false);
    Check(0xc8u, RdnaOpcode::VCmpFF16, false);
    Check(0xefu, RdnaOpcode::VCmpTruF16, false);
    Check(0xcfu, RdnaOpcode::VCmpOF16, false);
    Check(0xe8u, RdnaOpcode::VCmpUF16, false);
    Check(0x10u, RdnaOpcode::VCmpxFF32, true);
    Check(0x17u, RdnaOpcode::VCmpxOF32, true);
    Check(0x18u, RdnaOpcode::VCmpxUF32, true);
    Check(0x1fu, RdnaOpcode::VCmpxTruF32, true);
    Check(0x90u, RdnaOpcode::VCmpxFI32, true);
    Check(0x97u, RdnaOpcode::VCmpxTI32, true);
    Check(0xd0u, RdnaOpcode::VCmpxFU32, true);
    Check(0xd7u, RdnaOpcode::VCmpxTU32, true);
    Check(0xb0u, RdnaOpcode::VCmpxFI64, true);
    Check(0xb7u, RdnaOpcode::VCmpxTI64, true);
    Check(0xf0u, RdnaOpcode::VCmpxFU64, true);
    Check(0xf7u, RdnaOpcode::VCmpxTU64, true);
    Check(0xd8u, RdnaOpcode::VCmpxFF16, true);
    Check(0xffu, RdnaOpcode::VCmpxTruF16, true);
    Check(0xdfu, RdnaOpcode::VCmpxOF16, true);
    Check(0xf8u, RdnaOpcode::VCmpxUF16, true);
    CheckUnorderedInfinity(RdnaOpcode::VCmpNgtF32, 0x7f800000u, false);
    CheckUnorderedInfinity(RdnaOpcode::VCmpNltF32, 0xff800000u, false);
    CheckUnorderedInfinity(RdnaOpcode::VCmpNltF32, 0x7f800000u, true);
    CheckUnorderedInfinity(RdnaOpcode::VCmpNgtF32, 0xff800000u, true);
}
