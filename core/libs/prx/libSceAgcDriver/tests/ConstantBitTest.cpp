#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/ConstantFolder.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include <array>
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

bool masksFold(std::uint32_t inner, std::uint32_t outer) {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    auto& sum = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&builder.Constant(1u), &lane});
    auto& first = builder.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&sum, &builder.Constant(inner)});
    auto& second = builder.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&first, &builder.Constant(outer)});
    static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&first}));
    auto& user = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&second});
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    ConstantFolder().Fold(program);
    const IrValue* result = user.Argument(0)->Resolve();
    if ((inner & ~outer) == 0u) return result == &first;
    return result->Opcode() == IrOpcode::BitwiseAnd32 && result->Argument(0)->Resolve() == &sum && result->Argument(1)->Resolve()->ImmediateU32() == outer;
}

bool mergedHalfMask() {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    auto& sum = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&builder.Constant(1u), &lane});
    auto& low = builder.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&sum, &builder.Constant(0xffffu)});
    auto& high = builder.Emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&lane, &builder.Constant(16u)});
    auto& merged = builder.Emit(IrOpcode::BitwiseOr32, IrType::U32, {&low, &high});
    auto& masked = builder.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&merged, &builder.Constant(0x7fffu)});
    static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&low}));
    auto& user = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&masked});
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    ConstantFolder().Fold(program);
    const IrValue* result = user.Argument(0)->Resolve();
    return result->Opcode() == IrOpcode::BitwiseAnd32 && result->Argument(0)->Resolve() == &sum;
}

bool halfExtractions() {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    auto& low = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&builder.Constant(3u), &lane});
    auto& high = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&builder.Constant(5u), &lane});
    auto& lowHalf = builder.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&low, &builder.Constant(0xffffu)});
    auto& highHalf = builder.Emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&high, &builder.Constant(16u)});
    auto& merged = builder.Emit(IrOpcode::BitwiseOr32, IrType::U32, {&lowHalf, &highHalf});
    auto& top = builder.Emit(IrOpcode::ShiftRightLogical32, IrType::U32, {&merged, &builder.Constant(16u)});
    auto& field = builder.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&merged, &builder.Constant(16u), &builder.Constant(16u)});
    auto& bottom = builder.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&merged, &builder.Constant(0u), &builder.Constant(16u)});
    auto& topUser = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&top});
    auto& fieldUser = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&field});
    auto& bottomUser = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&bottom});
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    ConstantFolder().Fold(program);
    const IrValue* topResult = topUser.Argument(0)->Resolve();
    const IrValue* fieldResult = fieldUser.Argument(0)->Resolve();
    const IrValue* bottomResult = bottomUser.Argument(0)->Resolve();
    const bool topMasked = topResult->Opcode() == IrOpcode::BitwiseAnd32 && topResult->Argument(0)->Resolve() == &high && topResult->Argument(1)->Resolve()->ImmediateU32() == 0xffffu;
    const bool fieldThrough = fieldResult->Opcode() == IrOpcode::BitFieldUExtract && fieldResult->Argument(0)->Resolve() == &high && fieldResult->Argument(1)->Resolve()->ImmediateU32() == 0u;
    const bool bottomThrough = bottomResult->Opcode() == IrOpcode::BitFieldUExtract && bottomResult->Argument(0)->Resolve() == &low;
    return topMasked && fieldThrough && bottomThrough;
}

bool halvesRejoin() {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    auto& word = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&builder.Constant(7u), &lane});
    auto& low = builder.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&word, &builder.Constant(0u), &builder.Constant(16u)});
    auto& high = builder.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&word, &builder.Constant(16u), &builder.Constant(16u)});
    auto& raised = builder.Emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&high, &builder.Constant(16u)});
    auto& joined = builder.Emit(IrOpcode::BitwiseOr32, IrType::U32, {&low, &raised});
    auto& shifted = builder.Emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&word, &builder.Constant(16u)});
    auto& back = builder.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&shifted, &builder.Constant(16u), &builder.Constant(16u)});
    auto& joinedUser = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&joined});
    auto& backUser = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&back});
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    ConstantFolder().Fold(program);
    const IrValue* rejoined = joinedUser.Argument(0)->Resolve();
    const IrValue* field = backUser.Argument(0)->Resolve();
    const bool fieldBack = field->Opcode() == IrOpcode::BitFieldUExtract && field->Argument(0)->Resolve() == &word && field->Argument(1)->Resolve()->ImmediateU32() == 0u;
    return rejoined == &word && fieldBack;
}

bool factorFieldKept(bool multiplied) {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    auto& sum = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&builder.Constant(1u), &lane});
    auto& masked = builder.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&sum, &builder.Constant(0xffffu)});
    auto& field = builder.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&masked, &builder.Constant(0u), &builder.Constant(24u)});
    auto& used = multiplied ? builder.IMul(field, field) : field;
    auto& user = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&used});
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    ConstantFolder().Fold(program);
    const IrValue* result = user.Argument(0)->Resolve();
    if (!multiplied) return result == &masked;
    return result->Opcode() == IrOpcode::IMul32 && result->Argument(0)->Resolve() == &field && field.Opcode() == IrOpcode::BitFieldUExtract && field.Argument(0)->Resolve() == &masked;
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

enum class QuadMask {
    Expanded,
    Intersected,
    Nested,
    SeparateHalves,
    RealOnly,
    RealPartial,
    RealDivergent,
    Partial,
    WrongHalf,
    Divergent,
};

bool quadProjection(std::uint32_t wave, QuadMask shape, bool expected, IrShaderStage stage = IrShaderStage::Pixel) {
    IrProgram program;
    program.Resources().stage = stage;
    program.SetWaveSize(wave);
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    if (shape == QuadMask::Divergent || shape == QuadMask::RealDivergent) {
        auto& taken = program.CreateBlock();
        auto& other = program.CreateBlock();
        program.BlockOrder().push_back(&taken);
        program.BlockOrder().push_back(&other);
        builder.BranchConditional(builder.ULessThan(lane, builder.Constant(16u)), taken, other);
        builder.SetInsertionPoint(other);
        static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
        builder.SetInsertionPoint(taken);
    }
    const auto ballot = [&](IrValue& condition) {
        auto& mask = builder.Emit(IrOpcode::Ballot, IrType::U32x4, {&condition});
        return std::array<IrValue*, 2>{&builder.CompositeExtract(mask, 0u), wave == 64u ? &builder.CompositeExtract(mask, 1u) : &builder.Constant(0u)};
    };
    const auto expand = [&](const std::array<IrValue*, 2>& words) {
        auto& wide = builder.ConstructU64(*words[0], *words[1]);
        auto& mask = builder.Emit(IrOpcode::WqmU64, IrType::U64, {&wide});
        return std::array<IrValue*, 2>{&builder.CompositeExtract(mask, 0u), &builder.CompositeExtract(mask, 1u)};
    };
    auto& helper = builder.Emit(IrOpcode::GetBuiltin, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(StageInputKind::HelperInvocation)), &builder.Constant(0u)});
    auto& realCondition = builder.IEqual(helper, builder.Constant(0u));
    const auto real = ballot(realCondition);
    const auto quads = expand(real);
    const auto host = expand(ballot(builder.ConstantBool(true)));
    auto words = quads;
    if (shape == QuadMask::Intersected || shape == QuadMask::Nested || shape == QuadMask::SeparateHalves || shape == QuadMask::RealOnly || shape == QuadMask::RealPartial || shape == QuadMask::RealDivergent) {
        const auto highHost = shape == QuadMask::SeparateHalves ? expand(ballot(builder.ConstantBool(true))) : host;
        const auto source = shape == QuadMask::Nested || shape == QuadMask::RealOnly || shape == QuadMask::RealPartial || shape == QuadMask::RealDivergent ? real : quads;
        words = {&builder.BitwiseAnd(*source[0], *host[0]), &builder.BitwiseAnd(*source[1], *highHost[1])};
        if (shape == QuadMask::Nested) words = expand(words);
    }
    if (shape == QuadMask::Partial || shape == QuadMask::RealPartial) {
        words = {&builder.BitwiseAnd(*words[0], builder.Constant(0x0000ffffu)), &builder.Constant(0u)};
    }
    if (shape == QuadMask::WrongHalf) words[1] = words[0];
    IrValue& word = wave == 64u ? builder.Select(builder.ULessThan(lane, builder.Constant(32u)), *words[0], *words[1]) : *words[0];
    auto& shifted = builder.ShiftRightLogical(word, builder.BitwiseAnd(lane, builder.Constant(31u)));
    auto& test = builder.INotEqual(builder.BitwiseAnd(shifted, builder.Constant(1u)), builder.Constant(0u));
    auto& user = builder.Emit(IrOpcode::Reference, IrType::Void, {&test});
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    ConstantFolder{}.Fold(program);
    const auto* result = user.Argument(0)->Resolve();
    if (shape == QuadMask::RealOnly) return result == &realCondition;
    return expected ? result->HasImmediate() && result->ImmediateBool() : result->Opcode() == IrOpcode::INotEqual32;
}

bool run(const char* name, bool passed) {
    if (!passed) std::fprintf(stderr, "%s\n", name);
    return passed;
}

}

int main() {
    try {
        bool passed = true;
        for (const auto wave : {32u, 64u}) {
            passed &= run("a whole-quad entry mask covers pixel helpers", quadProjection(wave, QuadMask::Expanded, true));
            passed &= run("intersecting whole-quad entry masks preserves pixel helpers", quadProjection(wave, QuadMask::Intersected, true));
            passed &= run("a nested whole-quad entry mask covers pixel helpers", quadProjection(wave, QuadMask::Nested, true));
            passed &= run("independent low and high whole-quad masks cover pixel helpers", quadProjection(wave, QuadMask::SeparateHalves, true));
            passed &= run("an unexpanded entry mask must reuse the original helper test", quadProjection(wave, QuadMask::RealOnly, false));
            passed &= run("a partial whole-quad mask must keep the lane bit test", quadProjection(wave, QuadMask::Partial, false));
            passed &= run("a partial real-lane mask must keep the lane bit test", quadProjection(wave, QuadMask::RealPartial, false));
            passed &= run("a divergent real-lane ballot must keep the lane bit test", quadProjection(wave, QuadMask::RealDivergent, false));
            passed &= run("a divergent whole-quad ballot must keep the lane bit test", quadProjection(wave, QuadMask::Divergent, false));
            passed &= run("whole-quad entry proofs do not apply to other stages", quadProjection(wave, QuadMask::Intersected, false, IrShaderStage::Vertex));
        }
        passed &= run("a low word cannot prove the high-half helper bits", quadProjection(64u, QuadMask::WrongHalf, false));
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
        passed &= run("a repeated mask folds into the first one", masksFold(0xffffu, 0xffffu));
        passed &= run("a narrower mask reads through the wider one", masksFold(0xffffu, 0x7fffu));
        passed &= run("a wider mask after a narrower one is dropped", masksFold(0x00ffu, 0xffffu));
        passed &= run("a mask under a merge of the halves reads through it", mergedHalfMask());
        passed &= run("half extractions read the half that was merged in", halfExtractions());
        passed &= run("the two halves of one word rejoin into the word", halvesRejoin());
        passed &= run("a 24-bit field read of a value masked to 16 bits folds into the value", factorFieldKept(false));
        passed &= run("a 24-bit field read that a multiply uses is kept, even for a factor masked to 16 bits", factorFieldKept(true));
        return passed ? 0 : 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
