#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/ConstantFolder.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include "Optimization/ShaderInfoCollector.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("packed pixel ancillary regression"); }

constexpr std::uint32_t UnmodelledBits = 0xE000F0FFu;
constexpr std::uint32_t SelectOther = 0x5A3C9E17u;

struct Row {
    std::uint32_t sample;
    std::uint32_t layer;
    bool frontFacing;
};
constexpr std::array<Row, 6> Rows {{{0u, 0u, false}, {0u, 0u, true}, {15u, 0x1FFFu, false}, {15u, 0x1FFFu, true}, {0u, 0x1FFFu, true}, {15u, 0u, false}}};

static IrBlock& Begin(IrProgram& program) {
    program.Resources().stage = IrShaderStage::Pixel;
    program.Resources().resourceTrackingComplete = true;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    return block;
}
static IrValue& AncillaryBuiltin(IrBuilder& builder) {
    return builder.Emit(IrOpcode::GetBuiltin, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(StageInputKind::PackedAncillary)), &builder.Constant(0u)});
}
static IrValue& FrontFacingBuiltin(IrBuilder& builder) {
    return builder.Emit(IrOpcode::GetBuiltin, IrType::U1, {&builder.Constant(static_cast<std::uint32_t>(StageInputKind::FrontFacing)), &builder.Constant(0u)});
}
static void End(IrBuilder& builder, IrValue& user) {
    static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&user}));
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
}
static IrValue& Build(IrProgram& program, IrOpcode opcode, std::uint32_t offset, std::uint32_t count, bool throughSelect = false) {
    auto& block = Begin(program);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    IrValue& ancillary = AncillaryBuiltin(builder);
    IrValue* source = &ancillary;
    if (throughSelect) {
        source = &builder.Emit(IrOpcode::SelectU32, IrType::U32, {&FrontFacingBuiltin(builder), &ancillary, &builder.Constant(SelectOther)});
    }
    IrValue& user = opcode == IrOpcode::BitwiseOr32 ? builder.Emit(opcode, IrType::U32, {source, &builder.Constant(offset)})
                                                    : builder.Emit(opcode, IrType::U32, {source, &builder.Constant(offset), &builder.Constant(count)});
    End(builder, user);
    return user;
}
static void Lower(IrProgram& program) {
    ConstantFolder().Fold(program);
    DeadCodeEliminator().RemoveIdentities(program);
    DeadCodeEliminator().Eliminate(program);
    const ShaderPixelInputInfo pixel {};
    ShaderInfoCollector().Collect(program, ShaderStageInputInfo {nullptr, &pixel, nullptr});
}
static void ExtractVector(std::uint32_t offset, std::uint32_t count, StageInputKind kind, std::uint32_t fieldOffset) {
    IrProgram program;
    program.Resources().stage = IrShaderStage::Pixel;
    program.Resources().resourceTrackingComplete = true;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& ancillary = builder.Emit(IrOpcode::GetBuiltin, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(StageInputKind::PackedAncillary)), &builder.Constant(0u)});
    auto& maskedOffset = builder.BitwiseAnd(builder.Constant(offset), builder.Constant(31u));
    auto& maskedCount = builder.BitwiseAnd(builder.Constant(count), builder.Constant(31u));
    auto& available = builder.ISub(builder.Constant(32u), maskedOffset);
    auto& clampedCount = builder.Emit(IrOpcode::UMin32, IrType::U32, {&maskedCount, &available});
    auto& user = builder.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&ancillary, &maskedOffset, &clampedCount});
    static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&user}));
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    Lower(program);
    const IrValue* field = user.Argument(0)->Resolve();
    Require(field->Opcode() == IrOpcode::GetBuiltin);
    Require(static_cast<StageInputKind>(field->Argument(0)->Resolve()->ImmediateU32()) == kind);
    Require(user.Argument(1)->Resolve()->ImmediateU32() == fieldOffset);
    Require(user.Argument(2)->Resolve()->ImmediateU32() == count);
}

static void ExtractVectorMaskedCopy(std::uint32_t offset, std::uint32_t count, StageInputKind kind, std::uint32_t fieldOffset) {
    IrProgram program;
    program.Resources().stage = IrShaderStage::Pixel;
    program.Resources().resourceTrackingComplete = true;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& ancillary = builder.Emit(IrOpcode::GetBuiltin, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(StageInputKind::PackedAncillary)), &builder.Constant(0u)});
    auto& maskedOffset = builder.BitwiseAnd(builder.Constant(offset), builder.Constant(31u));
    auto& maskedCount = builder.BitwiseAnd(builder.Constant(count), builder.Constant(31u));
    auto& available = builder.ISub(builder.Constant(32u), maskedOffset);
    auto& clampedCount = builder.Emit(IrOpcode::UMin32, IrType::U32, {&maskedCount, &available});
    auto& user = builder.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&ancillary, &maskedOffset, &clampedCount});
    static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&user}));
    auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    auto& active = builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {&lane, &builder.Constant(16u)});
    auto& copy = builder.Emit(IrOpcode::SelectU32, IrType::U32, {&active, &ancillary, &builder.Constant(0u)});
    static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&copy}));
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    ConstantFolder().Fold(program);
    const IrValue* field = user.Argument(0)->Resolve();
    Require(field->Opcode() == IrOpcode::GetBuiltin);
    Require(static_cast<StageInputKind>(field->Argument(0)->Resolve()->ImmediateU32()) == kind);
    Require(user.Argument(1)->Resolve()->ImmediateU32() == fieldOffset);
    Require(user.Argument(2)->Resolve()->ImmediateU32() == count);
}
static void Extract(IrOpcode opcode, std::uint32_t offset, std::uint32_t count, StageInputKind kind, std::uint32_t fieldOffset) {
    IrProgram program;
    auto& user = Build(program, opcode, offset, count);
    Lower(program);
    const IrValue* field = user.Argument(0)->Resolve();
    Require(field->Opcode() == IrOpcode::GetBuiltin);
    Require(static_cast<StageInputKind>(field->Argument(0)->Resolve()->ImmediateU32()) == kind);
    Require(user.Argument(1)->Resolve()->ImmediateU32() == fieldOffset);
    Require(user.Argument(2)->Resolve()->ImmediateU32() == count);
}
static void Refused(IrOpcode opcode, std::uint32_t offset, std::uint32_t count, bool throughSelect = false) {
    IrProgram program;
    static_cast<void>(Build(program, opcode, offset, count, throughSelect));
    try {
        Lower(program);
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find("unsupported live use") != std::string::npos);
        return;
    }
    Require(false);
}
static std::uint32_t BitField(std::uint32_t word, std::uint32_t offset, std::uint32_t count, bool isSigned) {
    const std::uint32_t mask = count == 32u ? ~0u : (1u << count) - 1u;
    std::uint32_t bits = (word >> offset) & mask;
    if (isSigned && ((bits >> (count - 1u)) & 1u) != 0u) {
        bits |= ~mask;
    }
    return bits;
}
static std::uint32_t Evaluate(const IrValue* value, const Row& row) {
    value = value->Resolve();
    if (value->HasImmediate()) {
        return value->ImmediateU32();
    }
    switch (value->Opcode()) {
        case IrOpcode::GetBuiltin:
            switch (static_cast<StageInputKind>(Evaluate(value->Argument(0), row))) {
                case StageInputKind::SampleId: return row.sample;
                case StageInputKind::Layer: return row.layer;
                case StageInputKind::FrontFacing: return row.frontFacing ? 1u : 0u;
                default: break;
            }
            break;
        case IrOpcode::ShiftLeftLogical32: return Evaluate(value->Argument(0), row) << Evaluate(value->Argument(1), row);
        case IrOpcode::BitwiseOr32: return Evaluate(value->Argument(0), row) | Evaluate(value->Argument(1), row);
        case IrOpcode::BitFieldUExtract:
        case IrOpcode::BitFieldSExtract:
            return BitField(Evaluate(value->Argument(0), row), Evaluate(value->Argument(1), row), Evaluate(value->Argument(2), row), value->Opcode() == IrOpcode::BitFieldSExtract);
        case IrOpcode::SelectU32: return Evaluate(value->Argument(0), row) != 0u ? Evaluate(value->Argument(1), row) : Evaluate(value->Argument(2), row);
        case IrOpcode::Phi: return Evaluate(value->Argument(row.frontFacing ? 0u : 1u), row);
        default: break;
    }
    throw std::runtime_error("packed pixel ancillary evaluation reached an unsupported value");
}
static std::uint32_t PackedWord(const Row& row) {
    return UnmodelledBits | (row.sample << 8u) | (row.layer << 16u);
}
static void Values(IrOpcode opcode, std::uint32_t offset, std::uint32_t count, bool throughSelect) {
    IrProgram program;
    auto& user = Build(program, opcode, offset, count, throughSelect);
    Lower(program);
    for (const Row& row : Rows) {
        const std::uint32_t word = throughSelect && !row.frontFacing ? SelectOther : PackedWord(row);
        Require(Evaluate(&user, row) == BitField(word, offset, count, opcode == IrOpcode::BitFieldSExtract));
    }
}
static void LateFold() {
    IrProgram program;
    auto& block = Begin(program);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    IrValue& ancillary = AncillaryBuiltin(builder);
    IrValue& offset = builder.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&builder.Constant(16u), &builder.Constant(31u)});
    IrValue& count = builder.Emit(IrOpcode::UMin32, IrType::U32, {&builder.Constant(11u), &builder.Constant(13u)});
    IrValue& user = builder.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&ancillary, &offset, &count});
    End(builder, user);
    Lower(program);
    const IrValue* field = user.Argument(0)->Resolve();
    Require(field->Opcode() == IrOpcode::GetBuiltin);
    Require(static_cast<StageInputKind>(field->Argument(0)->Resolve()->ImmediateU32()) == StageInputKind::Layer);
    Require(user.Argument(1)->Resolve()->ImmediateU32() == 0u);
    Require(user.Argument(2)->Resolve()->ImmediateU32() == 11u);
    for (const Row& row : Rows) {
        Require(Evaluate(&user, row) == BitField(PackedWord(row), 16u, 11u, false));
    }
}
static IrValue& Merged(IrProgram& program, IrOpcode opcode, std::uint32_t offset, std::uint32_t count, bool loop) {
    auto& entry = Begin(program);
    auto& carry = program.CreateBlock();
    auto& merge = program.CreateBlock();
    program.BlockOrder().push_back(&carry);
    program.BlockOrder().push_back(&merge);
    IrBuilder builder(program);
    builder.SetInsertionPoint(entry);
    IrValue& ancillary = AncillaryBuiltin(builder);
    IrValue& frontFacing = FrontFacingBuiltin(builder);
    IrValue& phi = program.CreateValue(IrOpcode::Phi, IrType::U32);
    IrValue* other = &builder.Constant(SelectOther);
    if (loop) {
        entry.AddBranch(&carry);
        carry.AddBranch(&carry);
        carry.AddBranch(&merge);
        carry.AppendInstruction(&phi);
        builder.SetInsertionPoint(carry);
        other = &builder.Emit(IrOpcode::SelectU32, IrType::U32, {&frontFacing, &phi, &builder.Constant(SelectOther)});
        phi.AddPhiOperand(&entry, &ancillary);
        phi.AddPhiOperand(&carry, other);
    } else {
        entry.AddBranch(&carry);
        entry.AddBranch(&merge);
        carry.AddBranch(&merge);
        merge.AppendInstruction(&phi);
        phi.AddPhiOperand(&entry, &ancillary);
        phi.AddPhiOperand(&carry, other);
    }
    builder.SetInsertionPoint(merge);
    IrValue& user = opcode == IrOpcode::BitwiseOr32 ? builder.Emit(opcode, IrType::U32, {&phi, &builder.Constant(offset)})
                                                    : builder.Emit(opcode, IrType::U32, {&phi, &builder.Constant(offset), &builder.Constant(count)});
    End(builder, user);
    return user;
}
static void MergedValues(IrOpcode opcode, std::uint32_t offset, std::uint32_t count, bool loop) {
    IrProgram program;
    auto& user = Merged(program, opcode, offset, count, loop);
    Lower(program);
    for (const Row& row : Rows) {
        const std::uint32_t word = row.frontFacing ? PackedWord(row) : SelectOther;
        Require(Evaluate(&user, row) == BitField(word, offset, count, opcode == IrOpcode::BitFieldSExtract));
    }
}
static void MergedRefused(IrOpcode opcode, std::uint32_t offset, std::uint32_t count, bool loop) {
    IrProgram program;
    static_cast<void>(Merged(program, opcode, offset, count, loop));
    try {
        Lower(program);
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find("unsupported live use") != std::string::npos);
        return;
    }
    Require(false);
}
int main() {
    Extract(IrOpcode::BitFieldUExtract, 8u, 4u, StageInputKind::SampleId, 0u);
    Extract(IrOpcode::BitFieldUExtract, 9u, 2u, StageInputKind::SampleId, 1u);
    Extract(IrOpcode::BitFieldUExtract, 16u, 13u, StageInputKind::Layer, 0u);
    Extract(IrOpcode::BitFieldSExtract, 20u, 9u, StageInputKind::Layer, 4u);
    Values(IrOpcode::BitFieldUExtract, 8u, 4u, false);
    Values(IrOpcode::BitFieldSExtract, 8u, 4u, false);
    Values(IrOpcode::BitFieldUExtract, 9u, 2u, false);
    Values(IrOpcode::BitFieldUExtract, 16u, 13u, false);
    Values(IrOpcode::BitFieldSExtract, 20u, 9u, false);
    Values(IrOpcode::BitFieldUExtract, 28u, 1u, false);
    LateFold();
    Values(IrOpcode::BitFieldUExtract, 8u, 4u, true);
    Values(IrOpcode::BitFieldUExtract, 16u, 13u, true);
    Values(IrOpcode::BitFieldSExtract, 20u, 9u, true);
    ExtractVector(16u, 11u, StageInputKind::Layer, 0u);
    ExtractVectorMaskedCopy(16u, 11u, StageInputKind::Layer, 0u);
    ExtractVectorMaskedCopy(8u, 4u, StageInputKind::SampleId, 0u);
    ExtractVector(8u, 4u, StageInputKind::SampleId, 0u);
    Refused(IrOpcode::BitwiseOr32, 1u, 0u);
    Refused(IrOpcode::BitwiseOr32, 1u, 0u, true);
    Refused(IrOpcode::BitFieldUExtract, 2u, 4u);
    Refused(IrOpcode::BitFieldUExtract, 10u, 4u);
    Refused(IrOpcode::BitFieldUExtract, 13u, 2u);
    Refused(IrOpcode::BitFieldUExtract, 16u, 14u);
    Refused(IrOpcode::BitFieldUExtract, 0u, 2u);
    Refused(IrOpcode::BitFieldUExtract, 7u, 1u);
    Refused(IrOpcode::BitFieldUExtract, 12u, 4u);
    Refused(IrOpcode::BitFieldUExtract, 29u, 1u);
    Refused(IrOpcode::BitFieldUExtract, 8u, 0u);
    Refused(IrOpcode::BitFieldUExtract, 0u, 2u, true);
    Refused(IrOpcode::BitFieldSExtract, 12u, 4u, true);
    MergedValues(IrOpcode::BitFieldUExtract, 8u, 4u, false);
    MergedValues(IrOpcode::BitFieldUExtract, 16u, 13u, false);
    MergedValues(IrOpcode::BitFieldSExtract, 20u, 9u, false);
    MergedValues(IrOpcode::BitFieldUExtract, 16u, 11u, true);
    MergedRefused(IrOpcode::BitwiseOr32, 1u, 0u, false);
    MergedRefused(IrOpcode::BitFieldUExtract, 12u, 4u, false);
    MergedRefused(IrOpcode::BitFieldUExtract, 0u, 2u, true);
}
