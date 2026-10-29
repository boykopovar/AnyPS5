#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/ConstantFolder.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <initializer_list>
#include <vector>

using namespace ShaderRecompiler;

namespace {

constexpr IrOpcode And = IrOpcode::BitwiseAnd32;
constexpr IrOpcode Or = IrOpcode::BitwiseOr32;
constexpr IrOpcode Xor = IrOpcode::BitwiseXor32;
constexpr IrOpcode Shift = IrOpcode::ShiftRightLogical32;

struct Builder {
    IrProgram program;
    IrBuilder ir;
    IrBlock* block = nullptr;
    std::vector<IrValue*> kept;

    Builder() : ir(program) {
        block = &program.CreateBlock();
        program.SetEntryBlock(*block);
        program.BlockOrder().push_back(block);
        ir.SetInsertionPoint(*block);
    }

    IrValue& Emit(IrOpcode opcode, std::initializer_list<IrValue*> arguments) {
        return ir.Emit(opcode, IrType::U32, arguments);
    }

    IrValue& Value() {
        return Emit(IrOpcode::LaneId, {});
    }

    IrValue& Other(IrValue& value) {
        return Emit(IrOpcode::IAdd32, {&value, &ir.Constant(1u)});
    }

    IrValue& Not(IrValue& value) {
        return Emit(IrOpcode::BitwiseNot32, {&value});
    }

    IrValue& Same(IrValue& value) {
        auto& identity = program.CreateValue(IrOpcode::Identity, IrType::U32);
        identity.AddArgument(&value);
        block->AppendInstruction(&identity);
        return identity;
    }

    IrValue& Binary(IrOpcode opcode, IrValue& lhs, IrValue& rhs) {
        return Emit(opcode, {&lhs, &rhs});
    }

    IrValue& Binary(IrOpcode opcode, IrValue& lhs, std::uint32_t rhs) {
        return Binary(opcode, lhs, ir.Constant(rhs));
    }

    IrValue& Binary(IrOpcode opcode, std::uint32_t lhs, IrValue& rhs) {
        return Binary(opcode, ir.Constant(lhs), rhs);
    }

    std::size_t Keep(IrValue& value) {
        kept.push_back(&ir.Emit(IrOpcode::ReferenceU32, IrType::Void, {&value}));
        return kept.size() - 1u;
    }

    void Fold() {
        static_cast<void>(ir.Emit(IrOpcode::Return, IrType::Void, {}));
        ConstantFolder().Fold(program);
    }

    const IrValue& Result(std::size_t index) const {
        return *kept.at(index)->Argument(0)->Resolve();
    }

    bool Folds(std::size_t index, std::uint32_t expected) const {
        const IrValue& value = Result(index);
        return value.HasImmediate() && value.Type() == IrType::U32 && value.ImmediateU32() == expected;
    }

    bool Becomes(std::size_t index, const IrValue& expected) const {
        return &Result(index) == &expected;
    }

    bool Keeps(std::size_t index, IrOpcode opcode) const {
        return Result(index).Opcode() == opcode;
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

    passed &= run("and and or of a value with itself give the value and xor gives zero", [] {
        Builder b;
        auto& value = b.Value();
        const auto both = b.Keep(b.Binary(And, value, value));
        const auto either = b.Keep(b.Binary(Or, value, value));
        const auto differ = b.Keep(b.Binary(Xor, value, value));
        b.Fold();
        return b.Becomes(both, value) && b.Becomes(either, value) && b.Folds(differ, 0u);
    });

    passed &= run("a value and its complement give zero for and and all ones for or and xor in either order", [] {
        Builder b;
        auto& value = b.Value();
        auto& inverse = b.Not(value);
        const auto andLeft = b.Keep(b.Binary(And, value, inverse));
        const auto andRight = b.Keep(b.Binary(And, inverse, value));
        const auto orLeft = b.Keep(b.Binary(Or, value, inverse));
        const auto orRight = b.Keep(b.Binary(Or, inverse, value));
        const auto xorLeft = b.Keep(b.Binary(Xor, value, inverse));
        const auto xorRight = b.Keep(b.Binary(Xor, inverse, value));
        b.Fold();
        return b.Folds(andLeft, 0u) && b.Folds(andRight, 0u) && b.Folds(orLeft, 0xffffffffu) && b.Folds(orRight, 0xffffffffu) && b.Folds(xorLeft, 0xffffffffu) && b.Folds(xorRight, 0xffffffffu);
    });

    passed &= run("and with zero and or with all ones absorb the other operand in either order", [] {
        Builder b;
        auto& value = b.Value();
        const auto andLeft = b.Keep(b.Binary(And, value, 0u));
        const auto andRight = b.Keep(b.Binary(And, 0u, value));
        const auto orLeft = b.Keep(b.Binary(Or, value, 0xffffffffu));
        const auto orRight = b.Keep(b.Binary(Or, 0xffffffffu, value));
        b.Fold();
        return b.Folds(andLeft, 0u) && b.Folds(andRight, 0u) && b.Folds(orLeft, 0xffffffffu) && b.Folds(orRight, 0xffffffffu);
    });

    passed &= run("operands match through identities", [] {
        Builder b;
        auto& value = b.Value();
        auto& inverse = b.Same(b.Not(b.Same(value)));
        const auto both = b.Keep(b.Binary(And, value, b.Same(value)));
        const auto differ = b.Keep(b.Binary(Xor, b.Same(value), value));
        const auto complement = b.Keep(b.Binary(Or, inverse, value));
        b.Fold();
        return b.Becomes(both, value) && b.Folds(differ, 0u) && b.Folds(complement, 0xffffffffu);
    });

    passed &= run("different values, the complement of another value and xor with all ones are kept", [] {
        Builder b;
        auto& value = b.Value();
        auto& other = b.Other(value);
        auto& inverse = b.Not(other);
        const auto both = b.Keep(b.Binary(And, value, other));
        const auto either = b.Keep(b.Binary(Or, value, other));
        const auto differ = b.Keep(b.Binary(Xor, value, other));
        const auto andInverse = b.Keep(b.Binary(And, value, inverse));
        const auto orInverse = b.Keep(b.Binary(Or, inverse, value));
        const auto xorInverse = b.Keep(b.Binary(Xor, value, inverse));
        const auto flipped = b.Keep(b.Binary(Xor, value, 0xffffffffu));
        b.Fold();
        return b.Keeps(both, And) && b.Keeps(either, Or) && b.Keeps(differ, Xor) && b.Keeps(andInverse, And) && b.Keeps(orInverse, Or) && b.Keeps(xorInverse, Xor) && b.Keeps(flipped, Xor);
    });

    passed &= run("bit 0 of all ones shifted right by an amount masked to 31 is one in either order", [] {
        Builder b;
        auto& lane = b.Value();
        auto& maskRight = b.Binary(And, lane, 31u);
        auto& maskLeft = b.Binary(And, 31u, lane);
        const auto bitRight = b.Keep(b.Binary(And, b.Binary(Shift, 0xffffffffu, maskRight), 1u));
        const auto bitLeft = b.Keep(b.Binary(And, 1u, b.Binary(Shift, 0xffffffffu, maskLeft)));
        b.Fold();
        return b.Folds(bitRight, 1u) && b.Folds(bitLeft, 1u);
    });

    passed &= run("bit 0 of an unmasked or wider shift, of another word, and bit 1 are kept", [] {
        Builder b;
        auto& lane = b.Value();
        auto& masked = b.Binary(And, lane, 31u);
        const auto unmasked = b.Keep(b.Binary(And, b.Binary(Shift, 0xffffffffu, lane), 1u));
        const auto wider = b.Keep(b.Binary(And, b.Binary(Shift, 0xffffffffu, b.Binary(And, lane, 63u)), 1u));
        const auto word = b.Keep(b.Binary(And, b.Binary(Shift, 0x7fffffffu, masked), 1u));
        const auto second = b.Keep(b.Binary(And, b.Binary(Shift, 0xffffffffu, masked), 2u));
        b.Fold();
        return b.Keeps(unmasked, And) && b.Keeps(wider, And) && b.Keeps(word, And) && b.Keeps(second, And);
    });

    passed &= run("zero shifted right by any amount is zero and another word is kept", [] {
        Builder b;
        auto& lane = b.Value();
        const auto zero = b.Keep(b.Binary(Shift, 0u, lane));
        const auto word = b.Keep(b.Binary(Shift, b.Other(lane), lane));
        b.Fold();
        return b.Folds(zero, 0u) && b.Keeps(word, Shift);
    });

    return passed ? 0 : 1;
}
