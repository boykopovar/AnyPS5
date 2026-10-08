#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/ConstantFolder.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include "Optimization/ShaderInfoCollector.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("packed pixel ancillary regression"); }
static IrValue& Build(IrProgram& program, IrOpcode opcode, std::uint32_t offset, std::uint32_t count) {
    program.Resources().stage = IrShaderStage::Pixel;
    program.Resources().resourceTrackingComplete = true;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& ancillary = builder.Emit(IrOpcode::GetBuiltin, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(StageInputKind::PackedAncillary)), &builder.Constant(0u)});
    auto& user = opcode == IrOpcode::BitwiseOr32 ? builder.Emit(opcode, IrType::U32, {&ancillary, &builder.Constant(offset)})
                                                 : builder.Emit(opcode, IrType::U32, {&ancillary, &builder.Constant(offset), &builder.Constant(count)});
    static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&user}));
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    return user;
}
static void Lower(IrProgram& program) {
    ConstantFolder().Fold(program);
    DeadCodeEliminator().RemoveIdentities(program);
    DeadCodeEliminator().Eliminate(program);
    const ShaderPixelInputInfo pixel {};
    ShaderInfoCollector().Collect(program, ShaderStageInputInfo {nullptr, &pixel, nullptr});
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
static void Refused(IrOpcode opcode, std::uint32_t offset, std::uint32_t count) {
    IrProgram program;
    static_cast<void>(Build(program, opcode, offset, count));
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
    Refused(IrOpcode::BitwiseOr32, 1u, 0u);
    Refused(IrOpcode::BitFieldUExtract, 2u, 4u);
    Refused(IrOpcode::BitFieldUExtract, 10u, 4u);
    Refused(IrOpcode::BitFieldUExtract, 13u, 2u);
    Refused(IrOpcode::BitFieldUExtract, 16u, 14u);
}
