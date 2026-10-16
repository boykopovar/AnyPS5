#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/ConstantFolder.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>

using namespace ShaderRecompiler;

namespace {

bool foldsTo(std::uint32_t word, bool expected, bool maskedShift) {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    auto& shift = maskedShift ? builder.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&lane, &builder.Constant(31u)}) : lane;
    auto& shifted = builder.Emit(IrOpcode::ShiftRightLogical32, IrType::U32, {&builder.Constant(word), &shift});
    auto& bit = builder.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&shifted, &builder.Constant(1u)});
    auto& test = builder.Emit(IrOpcode::INotEqual32, IrType::Bool, {&bit, &builder.Constant(0u)});
    auto& user = builder.Emit(IrOpcode::Reference, IrType::Void, {&test});
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    ConstantFolder().Fold(program);
    const IrValue* result = user.Argument(0)->Resolve();
    if (!maskedShift || (word != 0u && word != 0xffffffffu)) return result->Opcode() == IrOpcode::INotEqual32;
    return result->HasImmediate() && result->ImmediateBool() == expected;
}

bool reversesTo(std::uint32_t word, std::uint32_t expected) {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& reversed = builder.Emit(IrOpcode::BitReverse32, IrType::U32, {&builder.Constant(word)});
    auto& test = builder.Emit(IrOpcode::IEqual32, IrType::Bool, {&reversed, &builder.Constant(expected)});
    auto& user = builder.Emit(IrOpcode::Reference, IrType::Void, {&test});
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    ConstantFolder().Fold(program);
    const IrValue* result = user.Argument(0)->Resolve();
    return result->HasImmediate() && result->ImmediateBool();
}

bool vertexProjection(std::uint32_t low, std::uint32_t hostSubgroup, std::uint32_t version, bool expected, bool directLane = false, IrShaderStage stage = IrShaderStage::Vertex, bool ballotWords = false) {
    IrProgram program;
    program.Resources().stage = stage;
    program.SetWaveSize(64u);
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    auto& half = builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {&lane, &builder.Constant(32u)});
    IrValue* lowWord = &builder.Constant(low);
    IrValue* highWord = &builder.Constant(0u);
    if (ballotWords) {
        auto& entry = builder.Emit(IrOpcode::Ballot, IrType::U32x4, {&builder.ConstantBool(true)});
        lowWord = &builder.Emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&entry, &builder.Constant(0u)});
        highWord = &builder.Emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&entry, &builder.Constant(1u)});
    }
    auto& word = builder.Emit(IrOpcode::SelectU32, IrType::U32, {&half, lowWord, highWord});
    auto& amount = builder.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&lane, &builder.Constant(31u)});
    auto& shifted = builder.Emit(IrOpcode::ShiftRightLogical32, IrType::U32, {&word, &amount});
    auto& bit = builder.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&shifted, &builder.Constant(1u)});
    auto& test = builder.Emit(IrOpcode::INotEqual32, IrType::Bool, {&bit, &builder.Constant(0u)});
    auto& user = builder.Emit(IrOpcode::Reference, IrType::Void, {&test});
    if (directLane) static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&lane}));
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    ConstantFolder folder;
    const bool folded = folder.FoldVertexLaneBounds(program, hostSubgroup, version);
    const bool shouldFold = !ballotWords && stage == IrShaderStage::Vertex && hostSubgroup == 32u && version >= 0x00010000u && version < 0x00010600u;
    if (folded != shouldFold) return false;
    if (!shouldFold && (word.Opcode() != IrOpcode::SelectU32 || word.Argument(0)->Resolve() != &half || half.Opcode() != IrOpcode::ULessThan32)) return false;
    if (shouldFold && (!word.Resolve()->HasImmediate() || word.Resolve()->ImmediateU32() != low)) return false;
    folder.Fold(program);
    DeadCodeEliminator{}.RemoveIdentities(program);
    DeadCodeEliminator{}.Eliminate(program);
    const auto* result = user.Argument(0)->Resolve();
    if (expected) return result->HasImmediate() && result->ImmediateBool();
    bool retainedLane = false;
    for (const auto* instruction : block.Instructions()) retainedLane = retainedLane || instruction->Opcode() == IrOpcode::LaneId;
    return retainedLane;
}

bool run(const char* name, bool passed) {
    if (!passed) std::fprintf(stderr, "%s\n", name);
    return passed;
}

}

int main() {
    try {
        bool passed = true;
        passed &= run("an all-zero word must fold the lane bit test to false", foldsTo(0u, false, true));
        passed &= run("an all-one word must fold the lane bit test to true", foldsTo(0xffffffffu, true, true));
        passed &= run("a mixed word must keep the lane bit test", foldsTo(0x0000ffffu, false, true));
        passed &= run("an unmasked lane number must keep the lane bit test", foldsTo(0xffffffffu, true, false));
        passed &= run("a fixed 32-lane vertex subgroup removes a constant lower-half export guard", vertexProjection(0xffffffffu, 32u, 0x00010300u, true));
        passed &= run("a single set bit remains dependent on the vertex lane", vertexProjection(1u, 32u, 0x00010300u, false));
        passed &= run("a partial mask remains dependent on the vertex lane", vertexProjection(0x0000ffffu, 32u, 0x00010300u, false));
        passed &= run("a real vertex lane output stays observable after folding its guard", vertexProjection(0xffffffffu, 32u, 0x00010300u, false, true));
        passed &= run("a lane word selected from ballot words keeps its lower-half test", vertexProjection(0xffffffffu, 32u, 0x00010300u, false, false, IrShaderStage::Vertex, true));
        passed &= run("a 64-lane subgroup keeps a lower-half export guard", vertexProjection(0xffffffffu, 64u, 0x00010300u, false));
        passed &= run("an unknown subgroup keeps a lower-half export guard", vertexProjection(0xffffffffu, 0u, 0x00010300u, false));
        passed &= run("a single-lane request does not borrow the physical 32-lane proof", vertexProjection(0xffffffffu, 1u, 0x00010300u, false));
        passed &= run("SPIR-V 1.6 does not borrow a fixed subgroup-size proof", vertexProjection(0xffffffffu, 32u, 0x00010600u, false));
        passed &= run("an unknown module version keeps the lane guard", vertexProjection(0xffffffffu, 32u, 0u, false));
        passed &= run("the lane-bound optimization does not change compute shaders", vertexProjection(0xffffffffu, 32u, 0x00010300u, false, false, IrShaderStage::Compute));
        passed &= run("the lane-bound optimization does not change pixel shaders", vertexProjection(0xffffffffu, 32u, 0x00010300u, false, false, IrShaderStage::Pixel));
        passed &= run("a reversed zero must fold to zero", reversesTo(0x00000000u, 0x00000000u));
        passed &= run("a reversed single bit must fold to the opposite end", reversesTo(0x00000001u, 0x80000000u));
        passed &= run("a reversed high bit must fold to bit zero", reversesTo(0x80000000u, 0x00000001u));
        passed &= run("a reversed mixed word must fold to its mirror image", reversesTo(0x12345678u, 0x1e6a2c48u));
        passed &= run("a reversed descriptor dword must fold to its mirror image", reversesTo(0xdeadbeefu, 0xf77db57bu));
        return passed ? 0 : 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
