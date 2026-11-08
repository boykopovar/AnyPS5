#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/NanPayloadEliminator.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <initializer_list>

using namespace ShaderRecompiler;

namespace {

struct Builder {
    IrProgram program;
    IrBlock* block = nullptr;

    Builder() {
        auto& created = program.CreateBlock();
        program.BlockOrder().push_back(&created);
        program.SetEntryBlock(created);
        block = &created;
    }

    IrValue& constant(std::uint32_t value) {
        auto& created = program.CreateValue(IrOpcode::Void, IrType::U32);
        created.SetImmediateU32(value);
        return created;
    }

    IrValue& emit(IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments) {
        auto& created = program.CreateValue(opcode, type);
        for (auto* argument : arguments) created.AddArgument(argument);
        block->AppendInstruction(&created);
        return created;
    }

    IrValue& unknown() {
        return emit(IrOpcode::LaneId, IrType::U32, {});
    }

    IrValue& f32(IrValue& bits) {
        return emit(IrOpcode::BitCastF32U32, IrType::F32, {&bits});
    }

    IrValue& bits(IrValue& value) {
        return emit(IrOpcode::BitCastU32F32, IrType::U32, {&value});
    }

    IrValue& isNan(IrValue& value) {
        return emit(IrOpcode::UGreaterThan32, IrType::Bool, {&emit(IrOpcode::BitwiseAnd32, IrType::U32, {&value, &constant(0x7fffffffu)}), &constant(0x7f800000u)});
    }

    IrValue& select(IrValue& condition, IrValue& taken, IrValue& otherwise) {
        return emit(IrOpcode::SelectU32, IrType::U32, {&condition, &taken, &otherwise});
    }

    IrValue& nanRule(IrOpcode opcode, IrValue& lhs, IrValue& rhs) {
        auto& result = bits(emit(opcode, IrType::F32, {&f32(lhs), &f32(rhs)}));
        auto& nan = select(isNan(rhs), rhs, select(isNan(lhs), lhs, constant(0xffc00000u)));
        return select(isNan(result), nan, result);
    }

    IrValue& saturate(IrValue& value) {
        return bits(emit(IrOpcode::FPSaturate32, IrType::F32, {&f32(value)}));
    }

    IrValue& flush(IrValue& value) {
        auto& exponent = emit(IrOpcode::BitwiseAnd32, IrType::U32, {&value, &constant(0x7f800000u)});
        auto& denormal = emit(IrOpcode::IEqual32, IrType::Bool, {&exponent, &constant(0u)});
        auto& sign = emit(IrOpcode::BitwiseAnd32, IrType::U32, {&value, &constant(0x80000000u)});
        return select(denormal, sign, value);
    }

    void keep(IrValue& value) {
        static_cast<void>(emit(IrOpcode::ReferenceU32, IrType::Void, {&value}));
    }

    NanPayloadEliminationStats eliminate() {
        return NanPayloadEliminator{}.Eliminate(program);
    }
};

bool expect(const char* name, bool condition) {
    if (!condition) std::fprintf(stderr, "%s\n", name);
    return condition;
}

bool run(const char* name, const std::function<bool()>& test) {
    try {
        return expect(name, test());
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: %s\n", name, error.what());
        return false;
    }
}

}

int main() {
    bool passed = true;

    passed &= run("an f32 NaN result that is only clamped drops its payload selects", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        b.keep(b.saturate(sum));
        const auto stats = b.eliminate();
        return stats.removedSelects >= 1u && !sum.HasUses();
    });

    passed &= run("an f32 NaN result kept as raw bits keeps every select", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        b.keep(sum);
        return b.eliminate().removedSelects == 0u && sum.HasUses();
    });

    passed &= run("integer work on the result bits keeps the selects", [] {
        Builder b;
        auto& product = b.nanRule(IrOpcode::FPMul32, b.unknown(), b.unknown());
        b.keep(b.emit(IrOpcode::ShiftRightLogical32, IrType::U32, {&product, &b.constant(16u)}));
        return b.eliminate().removedSelects == 0u && product.HasUses();
    });

    passed &= run("a sign flip passes the payload on to a raw sink", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        b.keep(b.emit(IrOpcode::BitwiseXor32, IrType::U32, {&sum, &b.constant(0x80000000u)}));
        return b.eliminate().removedSelects == 0u && sum.HasUses();
    });

    passed &= run("exponent and ordered tests of the result only see NaN-ness", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        auto& infinite = b.emit(IrOpcode::IEqual32, IrType::Bool, {&b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&sum, &b.constant(0x7f800000u)}), &b.constant(0x7f800000u)});
        auto& below = b.emit(IrOpcode::FPOrdLessThan32, IrType::Bool, {&b.f32(sum), &b.f32(b.constant(0x3f000000u))});
        b.keep(b.select(b.emit(IrOpcode::LogicalOr, IrType::Bool, {&infinite, &below}), b.constant(1u), b.constant(0u)));
        return b.eliminate().removedSelects >= 1u && !sum.HasUses();
    });

    passed &= run("a flush to a signed zero passes a NaN through unchanged", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        b.keep(b.saturate(b.flush(sum)));
        return b.eliminate().removedSelects >= 1u && !sum.HasUses();
    });

    passed &= run("bit work on the non-NaN arm of a NaN guard is never taken for a NaN", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        auto& halved = b.emit(IrOpcode::ISub32, IrType::U32, {&sum, &b.constant(0x00800000u)});
        b.keep(b.saturate(b.select(b.isNan(sum), sum, halved)));
        return b.eliminate().removedSelects >= 1u && !sum.HasUses();
    });

    passed &= run("bit work outside a NaN guard reaches the clamp and keeps the selects", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        auto& halved = b.emit(IrOpcode::ISub32, IrType::U32, {&sum, &b.constant(0x00800000u)});
        b.keep(b.saturate(halved));
        return b.eliminate().removedSelects == 0u && sum.HasUses();
    });

    passed &= run("a NaN result feeding a later NaN rule is removed only when that rule's result is unobserved", [] {
        Builder b;
        auto& first = b.nanRule(IrOpcode::FPMul32, b.unknown(), b.unknown());
        auto& second = b.nanRule(IrOpcode::FPAdd32, first, b.unknown());
        auto& third = b.nanRule(IrOpcode::FPAdd32, first, b.unknown());
        b.keep(b.saturate(second));
        b.keep(third);
        b.eliminate();
        return first.HasUses() && third.HasUses() && !second.HasUses();
    });

    return passed ? 0 : 1;
}
