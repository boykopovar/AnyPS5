#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/ConstantFolder.hpp"
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
        return passed ? 0 : 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
