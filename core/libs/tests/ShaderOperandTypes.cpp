#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaVectorOpDecoder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ShaderRecompiler;

static void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

static void CheckSelectTypes() {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    IrBuilder ir(program);
    ir.SetInsertionPoint(block);

    IrValue& cond = ir.ConstantBool(true);

    IrValue& bTrue = ir.ConstantBool(true);
    IrValue& bFalse = ir.ConstantBool(false);
    IrValue& bSel = ir.Select(cond, bTrue, bFalse);
    Require(bSel.Type() == IrType::Bool, "Select bool type mismatch");
    Require(bSel.Opcode() == IrOpcode::SelectU1, "Select bool opcode mismatch");

    IrValue& u32True = ir.Constant(10u);
    IrValue& u32False = ir.Constant(20u);
    IrValue& u32Sel = ir.Select(cond, u32True, u32False);
    Require(u32Sel.Type() == IrType::U32, "Select u32 type mismatch");
    Require(u32Sel.Opcode() == IrOpcode::SelectU32, "Select u32 opcode mismatch");

    IrValue& f32True = ir.ConstantF32(1.0f);
    IrValue& f32False = ir.ConstantF32(2.0f);
    IrValue& f32Sel = ir.Select(cond, f32True, f32False);
    Require(f32Sel.Type() == IrType::F32, "Select f32 type mismatch");
    Require(f32Sel.Opcode() == IrOpcode::SelectF32, "Select f32 opcode mismatch");

    IrValue& u8True = ir.Emit(IrOpcode::ConvertU8U32, IrType::U8, {&u32True});
    IrValue& u8False = ir.Emit(IrOpcode::ConvertU8U32, IrType::U8, {&u32False});
    IrValue& u8Sel = ir.Select(cond, u8True, u8False);
    Require(u8Sel.Type() == IrType::U8, "Select u8 type mismatch");
    Require(u8Sel.Opcode() == IrOpcode::ConvertU8U32, "Select u8 opcode mismatch");

    IrValue& u16True = ir.Emit(IrOpcode::ConvertU16U32, IrType::U16, {&u32True});
    IrValue& u16False = ir.Emit(IrOpcode::ConvertU16U32, IrType::U16, {&u32False});
    IrValue& u16Sel = ir.Select(cond, u16True, u16False);
    Require(u16Sel.Type() == IrType::U16, "Select u16 type mismatch");
    Require(u16Sel.Opcode() == IrOpcode::ConvertU16U32, "Select u16 opcode mismatch");

    IrValue& f16True = ir.Emit(IrOpcode::BitCastF16U16, IrType::F16, {&u16True});
    IrValue& f16False = ir.Emit(IrOpcode::BitCastF16U16, IrType::F16, {&u16False});
    IrValue& f16Sel = ir.Select(cond, f16True, f16False);
    Require(f16Sel.Type() == IrType::F16, "Select f16 type mismatch");
    Require(f16Sel.Opcode() == IrOpcode::BitCastF16U16, "Select f16 opcode mismatch");

    IrValue& u64True = ir.ConstructU64(u32True, u32False);
    IrValue& u64False = ir.ConstructU64(u32False, u32True);
    IrValue& u64Sel = ir.Select(cond, u64True, u64False);
    Require(u64Sel.Type() == IrType::U64, "Select u64 type mismatch");
    Require(u64Sel.Opcode() == IrOpcode::CompositeConstructU64, "Select u64 opcode mismatch");

    IrValue& u32x2True = ir.Emit(IrOpcode::CompositeConstructU32x2, IrType::U32x2, {&u32True, &u32False});
    IrValue& u32x2False = ir.Emit(IrOpcode::CompositeConstructU32x2, IrType::U32x2, {&u32False, &u32True});
    IrValue& u32x2Sel = ir.Select(cond, u32x2True, u32x2False);
    Require(u32x2Sel.Type() == IrType::U32x2, "Select u32x2 type mismatch");
    Require(u32x2Sel.Opcode() == IrOpcode::CompositeConstructU32x2, "Select u32x2 opcode mismatch");

    IrValue& u32x3True = ir.Emit(IrOpcode::CompositeConstructU32x3, IrType::U32x3, {&u32True, &u32False, &u32True});
    IrValue& u32x3False = ir.Emit(IrOpcode::CompositeConstructU32x3, IrType::U32x3, {&u32False, &u32True, &u32False});
    IrValue& u32x3Sel = ir.Select(cond, u32x3True, u32x3False);
    Require(u32x3Sel.Type() == IrType::U32x3, "Select u32x3 type mismatch");
    Require(u32x3Sel.Opcode() == IrOpcode::CompositeConstructU32x3, "Select u32x3 opcode mismatch");

    IrValue& u32x4True = ir.Emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {&u32True, &u32False, &u32True, &u32False});
    IrValue& u32x4False = ir.Emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {&u32False, &u32True, &u32False, &u32True});
    IrValue& u32x4Sel = ir.Select(cond, u32x4True, u32x4False);
    Require(u32x4Sel.Type() == IrType::U32x4, "Select u32x4 type mismatch");
    Require(u32x4Sel.Opcode() == IrOpcode::CompositeConstructU32x4, "Select u32x4 opcode mismatch");

    bool mismatchCaught = false;
    try {
        (void)ir.Select(cond, u32True, f32True);
    } catch (const std::invalid_argument&) {
        mismatchCaught = true;
    }
    Require(mismatchCaught, "Select allowed type mismatch");

    bool invalidTypeCaught = false;
    IrValue& opaqueVal = ir.Emit(IrOpcode::GetExec, IrType::Opaque, {});
    try {
        (void)ir.Select(cond, opaqueVal, opaqueVal);
    } catch (const std::invalid_argument&) {
        invalidTypeCaught = true;
    }
    Require(invalidTypeCaught, "Select allowed invalid type");
}

static void CheckCompositeExtract() {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    IrBuilder ir(program);
    ir.SetInsertionPoint(block);

    IrValue& c0 = ir.Constant(1u);
    IrValue& c1 = ir.Constant(2u);
    IrValue& u64 = ir.ConstructU64(c0, c1);
    IrValue& ext64_0 = ir.CompositeExtract(u64, 0u);
    IrValue& ext64_1 = ir.CompositeExtract(u64, 1u);
    Require(ext64_0.Opcode() == IrOpcode::CompositeExtractU64, "CompositeExtract U64 opcode");
    Require(ext64_0.Type() == IrType::U32, "CompositeExtract U64 type");
    Require(ext64_1.Opcode() == IrOpcode::CompositeExtractU64, "CompositeExtract U64 opcode");

    IrValue& u32x2 = ir.Emit(IrOpcode::CompositeConstructU32x2, IrType::U32x2, {&c0, &c1});
    IrValue& ext2_0 = ir.CompositeExtract(u32x2, 0u);
    IrValue& ext2_1 = ir.CompositeExtract(u32x2, 1u);
    Require(ext2_0.Opcode() == IrOpcode::CompositeExtractU32x2, "CompositeExtract U32x2 opcode");
    Require(ext2_1.Opcode() == IrOpcode::CompositeExtractU32x2, "CompositeExtract U32x2 opcode");

    IrValue& u32x3 = ir.Emit(IrOpcode::CompositeConstructU32x3, IrType::U32x3, {&c0, &c1, &c0});
    IrValue& ext3_0 = ir.CompositeExtract(u32x3, 0u);
    IrValue& ext3_1 = ir.CompositeExtract(u32x3, 1u);
    IrValue& ext3_2 = ir.CompositeExtract(u32x3, 2u);
    Require(ext3_0.Opcode() == IrOpcode::CompositeExtractU32x3, "CompositeExtract U32x3 opcode");
    Require(ext3_1.Opcode() == IrOpcode::CompositeExtractU32x3, "CompositeExtract U32x3 opcode");
    Require(ext3_2.Opcode() == IrOpcode::CompositeExtractU32x3, "CompositeExtract U32x3 opcode");

    IrValue& u32x4 = ir.Emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {&c0, &c1, &c0, &c1});
    IrValue& ext4_0 = ir.CompositeExtract(u32x4, 0u);
    IrValue& ext4_1 = ir.CompositeExtract(u32x4, 1u);
    IrValue& ext4_2 = ir.CompositeExtract(u32x4, 2u);
    IrValue& ext4_3 = ir.CompositeExtract(u32x4, 3u);
    Require(ext4_0.Opcode() == IrOpcode::CompositeExtractU32x4, "CompositeExtract U32x4 opcode");
    Require(ext4_1.Opcode() == IrOpcode::CompositeExtractU32x4, "CompositeExtract U32x4 opcode");
    Require(ext4_2.Opcode() == IrOpcode::CompositeExtractU32x4, "CompositeExtract U32x4 opcode");
    Require(ext4_3.Opcode() == IrOpcode::CompositeExtractU32x4, "CompositeExtract U32x4 opcode");

    bool nonCompositeCaught = false;
    try {
        (void)ir.CompositeExtract(c0, 0u);
    } catch (const std::invalid_argument& err) {
        nonCompositeCaught = std::string(err.what()).find("unsupported composite type") != std::string::npos;
    }
    Require(nonCompositeCaught, "CompositeExtract allowed non-composite type");
}

static void CheckOperandAccess() {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    IrBuilder ir(program);
    ir.SetInsertionPoint(block);

    RdnaOperand vOp;
    vOp.kind = RdnaOperandKind::VectorRegister;
    vOp.reg = 4u;

    IrValue* rU32 = context.ReadOperand(vOp, IrType::U32);
    Require(rU32 != nullptr, "readOperand U32 null");
    Require(rU32->Type() == IrType::U32, "readOperand U32 type mismatch");
    Require(rU32->Opcode() == IrOpcode::GetVectorRegister, "readOperand U32 opcode mismatch");
    Require(rU32->Argument(0)->Register().index == 4u, "readOperand U32 reg index");

    IrValue* rU64 = context.ReadOperand(vOp, IrType::U64);
    Require(rU64 != nullptr, "readOperand U64 null");
    Require(rU64->Type() == IrType::U64, "readOperand U64 type mismatch");
    Require(rU64->Opcode() == IrOpcode::CompositeConstructU64, "readOperand U64 opcode mismatch");
    Require(rU64->Argument(0)->Opcode() == IrOpcode::GetVectorRegister, "readOperand U64 low opcode");
    Require(rU64->Argument(0)->Argument(0)->Register().index == 4u, "readOperand U64 low index");
    Require(rU64->Argument(1)->Opcode() == IrOpcode::GetVectorRegister, "readOperand U64 high opcode");
    Require(rU64->Argument(1)->Argument(0)->Register().index == 5u, "readOperand U64 high index");

    IrValue* rU32x2 = context.ReadOperand(vOp, IrType::U32x2);
    Require(rU32x2 != nullptr, "readOperand U32x2 null");
    Require(rU32x2->Type() == IrType::U32x2, "readOperand U32x2 type mismatch");
    Require(rU32x2->Opcode() == IrOpcode::CompositeConstructU32x2, "readOperand U32x2 opcode mismatch");
    Require(rU32x2->Argument(0)->Argument(0)->Register().index == 4u, "readOperand U32x2 index 0");
    Require(rU32x2->Argument(1)->Argument(0)->Register().index == 5u, "readOperand U32x2 index 1");

    IrValue* rU32x3 = context.ReadOperand(vOp, IrType::U32x3);
    Require(rU32x3 != nullptr, "readOperand U32x3 null");
    Require(rU32x3->Type() == IrType::U32x3, "readOperand U32x3 type mismatch");
    Require(rU32x3->Opcode() == IrOpcode::CompositeConstructU32x3, "readOperand U32x3 opcode mismatch");
    Require(rU32x3->Argument(0)->Argument(0)->Register().index == 4u, "readOperand U32x3 index 0");
    Require(rU32x3->Argument(1)->Argument(0)->Register().index == 5u, "readOperand U32x3 index 1");
    Require(rU32x3->Argument(2)->Argument(0)->Register().index == 6u, "readOperand U32x3 index 2");

    IrValue* rU32x4 = context.ReadOperand(vOp, IrType::U32x4);
    Require(rU32x4 != nullptr, "readOperand U32x4 null");
    Require(rU32x4->Type() == IrType::U32x4, "readOperand U32x4 type mismatch");
    Require(rU32x4->Opcode() == IrOpcode::CompositeConstructU32x4, "readOperand U32x4 opcode mismatch");
    Require(rU32x4->Argument(0)->Argument(0)->Register().index == 4u, "readOperand U32x4 index 0");
    Require(rU32x4->Argument(1)->Argument(0)->Register().index == 5u, "readOperand U32x4 index 1");
    Require(rU32x4->Argument(2)->Argument(0)->Register().index == 6u, "readOperand U32x4 index 2");
    Require(rU32x4->Argument(3)->Argument(0)->Register().index == 7u, "readOperand U32x4 index 3");

    bool invalidReadCaught = false;
    try {
        (void)context.ReadOperand(vOp, IrType::Label);
    } catch (const std::runtime_error&) {
        invalidReadCaught = true;
    }
    Require(invalidReadCaught, "readOperand allowed invalid type");

    RdnaOperand dstOp;
    dstOp.kind = RdnaOperandKind::VectorRegister;
    dstOp.reg = 10u;

    auto collectVectorWrites = [](const IrBlock& blk, std::size_t skipCount) {
        std::vector<IrValue*> writes;
        std::size_t idx = 0;
        for (auto* val : blk.Instructions()) {
            if (idx++ >= skipCount && val->Opcode() == IrOpcode::SetVectorRegister) {
                writes.push_back(val);
            }
        }
        return writes;
    };

    const std::size_t beforeVoid = block.Instructions().size();
    IrValue& voidVal = ir.Emit(IrOpcode::Return, IrType::Void, {});
    context.WriteOperand(dstOp, &voidVal);
    auto voidWrites = collectVectorWrites(block, beforeVoid + 1u);
    Require(voidWrites.empty(), "writeOperand void emitted unexpected register write");

    const std::size_t beforeU32 = block.Instructions().size();
    IrValue& u32Val = ir.Constant(42u);
    context.WriteOperand(dstOp, &u32Val);
    auto u32Writes = collectVectorWrites(block, beforeU32);
    Require(u32Writes.size() == 1u, "writeOperand U32 write count");
    Require(u32Writes[0]->Argument(0)->Register().index == 10u, "writeOperand U32 reg index");

    const std::size_t beforeU64 = block.Instructions().size();
    IrValue& u64Val = ir.ConstructU64(u32Val, u32Val);
    context.WriteOperand(dstOp, &u64Val);
    auto u64Writes = collectVectorWrites(block, beforeU64);
    Require(u64Writes.size() == 2u, "writeOperand U64 write count");
    Require(u64Writes[0]->Argument(0)->Register().index == 10u, "writeOperand U64 reg 0 index");
    Require(u64Writes[1]->Argument(0)->Register().index == 11u, "writeOperand U64 reg 1 index");

    const std::size_t beforeU32x2 = block.Instructions().size();
    IrValue& u32x2Val = ir.Emit(IrOpcode::CompositeConstructU32x2, IrType::U32x2, {&u32Val, &u32Val});
    context.WriteOperand(dstOp, &u32x2Val);
    auto u32x2Writes = collectVectorWrites(block, beforeU32x2);
    Require(u32x2Writes.size() == 2u, "writeOperand U32x2 write count");
    Require(u32x2Writes[0]->Argument(0)->Register().index == 10u, "writeOperand U32x2 reg 0 index");
    Require(u32x2Writes[1]->Argument(0)->Register().index == 11u, "writeOperand U32x2 reg 1 index");

    const std::size_t beforeU32x3 = block.Instructions().size();
    IrValue& u32x3Val = ir.Emit(IrOpcode::CompositeConstructU32x3, IrType::U32x3, {&u32Val, &u32Val, &u32Val});
    context.WriteOperand(dstOp, &u32x3Val);
    auto u32x3Writes = collectVectorWrites(block, beforeU32x3);
    Require(u32x3Writes.size() == 3u, "writeOperand U32x3 write count");
    Require(u32x3Writes[0]->Argument(0)->Register().index == 10u, "writeOperand U32x3 reg 0 index");
    Require(u32x3Writes[1]->Argument(0)->Register().index == 11u, "writeOperand U32x3 reg 1 index");
    Require(u32x3Writes[2]->Argument(0)->Register().index == 12u, "writeOperand U32x3 reg 2 index");

    const std::size_t beforeU32x4 = block.Instructions().size();
    IrValue& u32x4Val = ir.Emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {&u32Val, &u32Val, &u32Val, &u32Val});
    context.WriteOperand(dstOp, &u32x4Val);
    auto u32x4Writes = collectVectorWrites(block, beforeU32x4);
    Require(u32x4Writes.size() == 4u, "writeOperand U32x4 write count");
    Require(u32x4Writes[0]->Argument(0)->Register().index == 10u, "writeOperand U32x4 reg 0 index");
    Require(u32x4Writes[1]->Argument(0)->Register().index == 11u, "writeOperand U32x4 reg 1 index");
    Require(u32x4Writes[2]->Argument(0)->Register().index == 12u, "writeOperand U32x4 reg 2 index");
    Require(u32x4Writes[3]->Argument(0)->Register().index == 13u, "writeOperand U32x4 reg 3 index");

    bool invalidWriteCaught = false;
    IrValue& labelVal = program.CreateValue(IrOpcode::Return, IrType::Label);
    try {
        context.WriteOperand(dstOp, &labelVal);
    } catch (const std::runtime_error&) {
        invalidWriteCaught = true;
    }
    Require(invalidWriteCaught, "writeOperand allowed invalid type");
}

static void CheckVop3Translation(std::uint32_t encoding, RdnaOpcode opcode, IrOpcode expected) {
    const std::array<std::uint32_t, 2> code{(encoding << 16u) | 1u, 1u | (2u << 9u) | (3u << 18u)};
    const RdnaInstruction instruction = DecodeRdnaVop3(0u, code, 0u);
    Require(instruction.op == opcode, "VOP3 opcode mismatch");
    Require(instruction.family == RdnaInstructionFamily::VOP3, "VOP3 family mismatch");

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);

    bool found = false;
    for (auto* value : block.Instructions()) {
        found = found || value->Opcode() == expected;
    }
    Require(found, "Expected translated opcode not found");
}

int main() {
    CheckSelectTypes();
    CheckCompositeExtract();
    CheckOperandAccess();
    CheckVop3Translation(0x164u, RdnaOpcode::VAddF64, IrOpcode::FPAdd64);
    CheckVop3Translation(0x165u, RdnaOpcode::VMulF64, IrOpcode::FPMul64);
    CheckVop3Translation(0x14cu, RdnaOpcode::VFmaF64, IrOpcode::FPFma64);
    std::puts("shader operand types tests passed");
    return 0;
}
