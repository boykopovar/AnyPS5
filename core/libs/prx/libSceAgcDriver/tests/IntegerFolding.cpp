#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ConstantFolder.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

using namespace ShaderRecompiler;

namespace {

struct Builder {
    IrProgram program;
    IrBlock* block = nullptr;
    std::vector<IrValue*> kept;

    Builder() {
        block = &NewBlock();
        program.SetEntryBlock(*block);
    }

    IrBlock& NewBlock() {
        auto& created = program.CreateBlock();
        program.BlockOrder().push_back(&created);
        return created;
    }

    IrValue& Constant(std::uint32_t value) {
        auto& created = program.CreateValue(IrOpcode::Void, IrType::U32);
        created.SetImmediateU32(value);
        return created;
    }

    IrValue& Emit(IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments) {
        auto& created = program.CreateValue(opcode, type);
        for (auto* argument : arguments) created.AddArgument(argument);
        block->AppendInstruction(&created);
        return created;
    }

    IrValue& Unknown() {
        return Emit(IrOpcode::LaneId, IrType::U32, {});
    }

    IrValue& Binary(IrOpcode opcode, IrValue& lhs, IrValue& rhs) {
        return Emit(opcode, IrType::U32, {&lhs, &rhs});
    }

    IrValue& Binary(IrOpcode opcode, IrValue& lhs, std::uint32_t rhs) {
        return Binary(opcode, lhs, Constant(rhs));
    }

    IrValue& Shifted(std::uint32_t amount) {
        return Binary(IrOpcode::ShiftLeftLogical32, Unknown(), amount);
    }

    std::size_t Compare(IrOpcode opcode, IrValue& value, std::uint32_t immediate) {
        auto& test = Emit(opcode, IrType::Bool, {&value, &Constant(immediate)});
        auto& user = Emit(IrOpcode::Reference, IrType::Void, {&test});
        kept.push_back(&user);
        return kept.size() - 1u;
    }

    std::size_t Equal(IrValue& value, std::uint32_t immediate) {
        return Compare(IrOpcode::IEqual32, value, immediate);
    }

    std::size_t NotEqual(IrValue& value, std::uint32_t immediate) {
        return Compare(IrOpcode::INotEqual32, value, immediate);
    }

    std::size_t Less(IrValue& value, std::uint32_t immediate) {
        return Compare(IrOpcode::ULessThan32, value, immediate);
    }

    void Fold() {
        ConstantFolder().Fold(program);
    }

    std::optional<bool> Result(std::size_t index) const {
        const IrValue* value = kept.at(index)->Argument(0)->Resolve();
        if (!value->HasImmediate()) return std::nullopt;
        return value->ImmediateBool();
    }

    bool Folds(std::size_t index, bool expected) const {
        return Result(index) == std::optional<bool>(expected);
    }

    bool Keeps(std::size_t index) const {
        return !Result(index).has_value();
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

    passed &= run("a compare against a constant that contradicts the zero bits of a left shift folds", [] {
        Builder b;
        auto& value = b.Shifted(2u);
        const auto odd = b.Equal(value, 5u);
        const auto oddNot = b.NotEqual(value, 6u);
        const auto aligned = b.Equal(value, 4u);
        const auto alignedNot = b.NotEqual(value, 8u);
        b.Fold();
        return b.Folds(odd, false) && b.Folds(oddNot, true) && b.Keeps(aligned) && b.Keeps(alignedNot);
    });

    passed &= run("a compare against an unknown value or a variable shift is kept", [] {
        Builder b;
        auto& lane = b.Unknown();
        auto& variable = b.Binary(IrOpcode::ShiftLeftLogical32, b.Unknown(), lane);
        const auto plain = b.Equal(lane, 5u);
        const auto byLane = b.Equal(variable, 1u);
        b.Fold();
        return b.Keeps(plain) && b.Keeps(byLane);
    });

    passed &= run("known bits pass through right shifts, bitfield extracts, and, or and xor", [] {
        Builder b;
        auto& high = b.Binary(IrOpcode::ShiftRightLogical32, b.Unknown(), 4u);
        auto& field = b.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&b.Shifted(4u), &b.Constant(2u), &b.Constant(4u)});
        auto& masked = b.Binary(IrOpcode::BitwiseAnd32, b.Unknown(), 0xfffffff0u);
        auto& merged = b.Binary(IrOpcode::BitwiseOr32, b.Shifted(4u), 3u);
        auto& flipped = b.Binary(IrOpcode::BitwiseXor32, merged, 1u);
        const auto highTop = b.Equal(high, 0x10000000u);
        const auto highFits = b.Equal(high, 0x0fffffffu);
        const auto fieldLow = b.Equal(field, 1u);
        const auto fieldTop = b.Equal(field, 0x10u);
        const auto fieldFits = b.Equal(field, 4u);
        const auto maskedLow = b.Equal(masked, 0x21u);
        const auto mergedLow = b.Equal(merged, 0x10u);
        const auto mergedFits = b.Equal(merged, 0x13u);
        const auto flippedLow = b.Equal(flipped, 0x13u);
        const auto flippedFits = b.NotEqual(flipped, 0x12u);
        b.Fold();
        return b.Folds(highTop, false) && b.Keeps(highFits) && b.Folds(fieldLow, false) && b.Folds(fieldTop, false) && b.Keeps(fieldFits) && b.Folds(maskedLow, false) && b.Folds(mergedLow, false) && b.Keeps(mergedFits) && b.Folds(flippedLow, false) && b.Keeps(flippedFits);
    });

    passed &= run("known bits of a sum stop at the first unknown bit and include the carries below it", [] {
        Builder b;
        auto& offset = b.Binary(IrOpcode::IAdd32, b.Shifted(2u), 1u);
        auto& carried = b.Binary(IrOpcode::IAdd32, b.Binary(IrOpcode::BitwiseOr32, b.Shifted(2u), 3u), 1u);
        auto& partial = b.Binary(IrOpcode::IAdd32, b.Shifted(2u), b.Unknown());
        const auto offsetWrong = b.Equal(offset, 6u);
        const auto offsetFits = b.Equal(offset, 5u);
        const auto carriedWrong = b.Equal(carried, 1u);
        const auto carriedFits = b.Equal(carried, 4u);
        const auto carriedHigh = b.Equal(carried, 8u);
        const auto partialAny = b.Equal(partial, 1u);
        b.Fold();
        return b.Folds(offsetWrong, false) && b.Keeps(offsetFits) && b.Folds(carriedWrong, false) && b.Keeps(carriedFits) && b.Keeps(carriedHigh) && b.Keeps(partialAny);
    });

    passed &= run("a product has the trailing zeros of both factors", [] {
        Builder b;
        auto& product = b.Binary(IrOpcode::IMul32, b.Shifted(1u), b.Shifted(2u));
        auto& single = b.Binary(IrOpcode::IMul32, b.Shifted(1u), b.Unknown());
        const auto productLow = b.Equal(product, 4u);
        const auto productFits = b.Equal(product, 8u);
        const auto singleLow = b.Equal(single, 1u);
        const auto singleFits = b.Equal(single, 2u);
        b.Fold();
        return b.Folds(productLow, false) && b.Keeps(productFits) && b.Folds(singleLow, false) && b.Keeps(singleFits);
    });

    passed &= run("an unsigned minimum keeps the common bits and the width of the smaller bound", [] {
        Builder b;
        auto& both = b.Binary(IrOpcode::UMin32, b.Shifted(2u), b.Shifted(3u));
        auto& bounded = b.Binary(IrOpcode::UMin32, b.Binary(IrOpcode::BitwiseAnd32, b.Unknown(), 0xffu), b.Shifted(4u));
        const auto bothLow = b.Equal(both, 2u);
        const auto bothFits = b.Equal(both, 4u);
        const auto boundedHigh = b.Equal(bounded, 0x100u);
        const auto boundedFits = b.Equal(bounded, 0xf0u);
        b.Fold();
        return b.Folds(bothLow, false) && b.Keeps(bothFits) && b.Folds(boundedHigh, false) && b.Keeps(boundedFits);
    });

    passed &= run("a select keeps the bits common to both arms", [] {
        Builder b;
        auto& condition = b.Emit(IrOpcode::ULessThan32, IrType::Bool, {&b.Unknown(), &b.Constant(16u)});
        auto& choice = b.Emit(IrOpcode::SelectU32, IrType::U32, {&condition, &b.Shifted(2u), &b.Binary(IrOpcode::BitwiseOr32, b.Shifted(3u), 4u)});
        const auto choiceLow = b.Equal(choice, 2u);
        const auto choiceArm = b.Equal(choice, 4u);
        b.Fold();
        return b.Folds(choiceLow, false) && b.Keeps(choiceArm);
    });

    passed &= run("a phi keeps the bits common to its incoming values and ignores itself", [] {
        Builder b;
        IrBlock& entry = *b.block;
        IrBlock& other = b.NewBlock();
        IrBlock& join = b.NewBlock();
        entry.AddBranch(&other);
        entry.AddBranch(&join);
        other.AddBranch(&join);
        join.AddBranch(&join);
        auto& first = b.Shifted(2u);
        b.block = &other;
        auto& second = b.Binary(IrOpcode::BitwiseOr32, b.Shifted(3u), 4u);
        b.block = &join;
        auto& merged = b.program.CreateValue(IrOpcode::Phi, IrType::U32);
        join.AppendInstruction(&merged);
        merged.AddPhiOperand(&entry, &first);
        merged.AddPhiOperand(&other, &second);
        merged.AddPhiOperand(&join, &merged);
        const auto mergedLow = b.Equal(merged, 1u);
        const auto mergedFits = b.Equal(merged, 8u);
        b.Fold();
        return b.Folds(mergedLow, false) && b.Keeps(mergedFits);
    });

    passed &= run("a loop counter phi keeps every compare its values can satisfy", [] {
        Builder b;
        IrBlock& entry = *b.block;
        IrBlock& loop = b.NewBlock();
        entry.AddBranch(&loop);
        loop.AddBranch(&loop);
        b.block = &loop;
        auto& counter = b.program.CreateValue(IrOpcode::Phi, IrType::U32);
        loop.AppendInstruction(&counter);
        auto& next = b.Binary(IrOpcode::IAdd32, counter, 1u);
        counter.AddPhiOperand(&entry, &b.Constant(0u));
        counter.AddPhiOperand(&loop, &next);
        const auto counterSix = b.Equal(counter, 6u);
        const auto counterBelow = b.Less(counter, 1u);
        b.Fold();
        return b.Keeps(counterSix) && b.Keeps(counterBelow);
    });

    passed &= run("a lane read has the known bits of its source", [] {
        Builder b;
        auto& first = b.Emit(IrOpcode::ReadFirstLane, IrType::U32, {&b.Shifted(2u), &b.Emit(IrOpcode::IEqual32, IrType::Bool, {&b.Unknown(), &b.Constant(0u)})});
        auto& lane = b.Emit(IrOpcode::ReadLane, IrType::U32, {&b.Binary(IrOpcode::IAdd32, b.Shifted(2u), 3u), &b.Unknown()});
        auto& offset = b.Binary(IrOpcode::IAdd32, first, 1u);
        const auto firstLow = b.Equal(first, 3u);
        const auto offsetLow = b.NotEqual(offset, 4u);
        const auto offsetFits = b.Equal(offset, 5u);
        const auto laneLow = b.Equal(lane, 4u);
        const auto laneFits = b.Equal(lane, 7u);
        b.Fold();
        return b.Folds(firstLow, false) && b.Folds(offsetLow, true) && b.Keeps(offsetFits) && b.Folds(laneLow, false) && b.Keeps(laneFits);
    });

    passed &= run("a small value set folds compares against values outside it", [] {
        Builder b;
        auto& scaled = b.Binary(IrOpcode::IMul32, b.Binary(IrOpcode::UMin32, b.Unknown(), 3u), 5u);
        auto& field = b.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&b.Unknown(), &b.Constant(8u), &b.Constant(2u)});
        auto& sum = b.Binary(IrOpcode::IAdd32, field, scaled);
        auto& single = b.Binary(IrOpcode::BitwiseAnd32, b.Binary(IrOpcode::UMin32, b.Unknown(), 0u), 7u);
        const auto scaledOutside = b.Equal(scaled, 7u);
        const auto scaledInside = b.NotEqual(scaled, 10u);
        const auto sumOutside = b.Equal(sum, 19u);
        const auto sumInside = b.Equal(sum, 18u);
        const auto singleInside = b.Equal(single, 0u);
        const auto singleOutside = b.NotEqual(single, 1u);
        b.Fold();
        return b.Folds(scaledOutside, false) && b.Keeps(scaledInside) && b.Folds(sumOutside, false) && b.Keeps(sumInside) && b.Folds(singleInside, true) && b.Folds(singleOutside, true);
    });

    passed &= run("a sum of two 64-value sets is not enumerated and keeps its compares", [] {
        Builder b;
        auto& sum = b.Binary(IrOpcode::IAdd32, b.Binary(IrOpcode::UMin32, b.Unknown(), 63u), b.Binary(IrOpcode::UMin32, b.Unknown(), 63u));
        std::vector<std::size_t> compares;
        for (std::uint32_t entry = 1u; entry <= 100u; ++entry) compares.push_back(b.Equal(sum, entry));
        b.Fold();
        bool kept = true;
        for (const auto compare : compares) kept &= b.Keeps(compare);
        return kept;
    });

    passed &= run("the value set of x & mask leaves out the known bits of x", [] {
        Builder b;
        auto& masked = b.Binary(IrOpcode::BitwiseAnd32, b.Shifted(4u), 0x3ffu);
        auto& scaled = b.Binary(IrOpcode::IMul32, masked, 3u);
        auto& fixed = b.Binary(IrOpcode::IMul32, b.Binary(IrOpcode::BitwiseAnd32, b.Binary(IrOpcode::BitwiseOr32, b.Shifted(3u), 5u), 0x1fu), 3u);
        auto& wide = b.Binary(IrOpcode::IMul32, b.Binary(IrOpcode::BitwiseAnd32, b.Shifted(2u), 0x3ffu), 3u);
        const auto scaledOutside = b.Equal(scaled, 32u);
        const auto scaledInside = b.Equal(scaled, 48u);
        const auto fixedOutside = b.Equal(fixed, 27u);
        const auto fixedInside = b.Equal(fixed, 39u);
        const auto wideUnknown = b.Equal(wide, 4u);
        b.Fold();
        return b.Folds(scaledOutside, false) && b.Keeps(scaledInside) && b.Folds(fixedOutside, false) && b.Keeps(fixedInside) && b.Keeps(wideUnknown);
    });

    passed &= run("wrapping sums and products keep compares that their values straddle", [] {
        Builder b;
        auto& wrapped = b.Binary(IrOpcode::IAdd32, b.Binary(IrOpcode::BitwiseAnd32, b.Unknown(), 1u), 0xffffffffu);
        auto& product = b.Binary(IrOpcode::IMul32, b.Binary(IrOpcode::BitwiseAnd32, b.Unknown(), 1u), 0x80000000u);
        auto& scaled = b.Binary(IrOpcode::IMul32, b.Binary(IrOpcode::BitwiseAnd32, b.Unknown(), 3u), 0x80000001u);
        const auto wrappedZero = b.Equal(wrapped, 0u);
        const auto wrappedBelow = b.Less(wrapped, 1u);
        const auto wrappedMax = b.Equal(wrapped, 0xffffffffu);
        const auto wrappedOutside = b.Equal(wrapped, 1u);
        const auto productZero = b.Equal(product, 0u);
        const auto productBelow = b.Less(product, 1u);
        const auto productTop = b.Equal(product, 0x80000000u);
        const auto productOutside = b.Equal(product, 1u);
        const auto scaledWrapped = b.Equal(scaled, 2u);
        const auto scaledBelow = b.Less(scaled, 0x80000001u);
        const auto scaledOutside = b.Equal(scaled, 0x80000002u);
        b.Fold();
        return b.Keeps(wrappedZero) && b.Keeps(wrappedBelow) && b.Keeps(wrappedMax) && b.Folds(wrappedOutside, false) && b.Keeps(productZero) && b.Keeps(productBelow) && b.Keeps(productTop) && b.Folds(productOutside, false) && b.Keeps(scaledWrapped) && b.Keeps(scaledBelow) && b.Folds(scaledOutside, false);
    });

    passed &= run("a chain of wide phi joins exhausts the analysis budget, folds nothing and never folds a satisfiable compare", [] {
        Builder b;
        std::vector<IrBlock*> predecessors;
        for (std::uint32_t edge = 0u; edge < 100u; ++edge) predecessors.push_back(&b.NewBlock());
        IrValue* previous = nullptr;
        for (std::uint32_t level = 0u; level < 5u; ++level) {
            auto& join = b.NewBlock();
            auto& phi = b.program.CreateValue(IrOpcode::Phi, IrType::U32);
            join.AppendInstruction(&phi);
            for (std::uint32_t edge = 0u; edge < 100u; ++edge) {
                IrValue* incoming = &b.Constant(3u);
                if (previous == nullptr && edge == 0u) incoming = &b.Constant(1u);
                if (previous != nullptr && edge != 99u) incoming = previous;
                phi.AddPhiOperand(predecessors[edge], incoming);
            }
            previous = &phi;
        }
        auto& masked = b.Binary(IrOpcode::BitwiseAnd32, *previous, 0xffu);
        const auto isOne = b.Equal(masked, 1u);
        const auto isThree = b.Equal(masked, 3u);
        const auto isTwo = b.Equal(masked, 2u);
        b.Fold();
        return b.Keeps(isOne) && b.Keeps(isThree) && b.Keeps(isTwo);
    });

    passed &= run("a phi charges every incoming edge to the analysis budget, so only a phi within the budget folds", [] {
        Builder b;
        const auto oddCompare = [&](std::uint32_t edges) {
            auto& join = b.NewBlock();
            auto& phi = b.program.CreateValue(IrOpcode::Phi, IrType::U32);
            join.AppendInstruction(&phi);
            for (std::uint32_t edge = 0u; edge < edges; ++edge) phi.AddPhiOperand(&b.NewBlock(), &b.Constant(2u * (edge + 1u)));
            return b.Equal(b.Binary(IrOpcode::BitwiseAnd32, phi, 1u), 1u);
        };
        const auto narrow = oddCompare(100u);
        const auto wide = oddCompare(3000u);
        b.Fold();
        return b.Folds(narrow, false) && b.Keeps(wide);
    });

    passed &= run("right shifts by 26 to 31 enumerate every value of the shifted-out field", [] {
        Builder b;
        std::vector<std::size_t> reachable;
        std::vector<std::size_t> unreachable;
        for (std::uint32_t shift = 26u; shift <= 31u; ++shift) {
            const std::uint32_t largest = (std::uint32_t{1} << (32u - shift)) - 1u;
            auto& scaled = b.Binary(IrOpcode::IMul32, b.Binary(IrOpcode::ShiftRightLogical32, b.Unknown(), shift), 3u);
            reachable.push_back(b.Equal(scaled, largest * 3u));
            reachable.push_back(b.Equal(scaled, 3u));
            reachable.push_back(b.Equal(scaled, 0u));
            unreachable.push_back(b.Equal(scaled, largest * 3u + 1u));
            unreachable.push_back(b.Equal(scaled, 4u));
        }
        auto& tooWide = b.Binary(IrOpcode::IMul32, b.Binary(IrOpcode::ShiftRightLogical32, b.Unknown(), 25u), 3u);
        const auto wide = b.Equal(tooWide, 4u);
        b.Fold();
        bool matched = b.Keeps(wide);
        for (const auto compare : reachable) matched &= b.Keeps(compare);
        for (const auto compare : unreachable) matched &= b.Folds(compare, false);
        return matched;
    });

    passed &= run("value sets propagate through select, or and xor", [] {
        Builder b;
        auto& five = b.Binary(IrOpcode::IMul32, b.Binary(IrOpcode::UMin32, b.Unknown(), 3u), 5u);
        auto& seven = b.Binary(IrOpcode::IMul32, b.Binary(IrOpcode::UMin32, b.Unknown(), 2u), 7u);
        auto& condition = b.Emit(IrOpcode::ULessThan32, IrType::Bool, {&b.Unknown(), &b.Constant(16u)});
        auto& choice = b.Emit(IrOpcode::SelectU32, IrType::U32, {&condition, &five, &seven});
        auto& merged = b.Binary(IrOpcode::BitwiseOr32, five, 16u);
        auto& flipped = b.Binary(IrOpcode::BitwiseXor32, five, 0xf0u);
        const auto choiceMissing = b.Equal(choice, 12u);
        const auto choiceMissingNot = b.NotEqual(choice, 11u);
        const auto choiceFirst = b.Equal(choice, 15u);
        const auto choiceSecond = b.Equal(choice, 14u);
        const auto choiceShared = b.Equal(choice, 0u);
        const auto mergedMissing = b.Equal(merged, 22u);
        const auto mergedPresent = b.Equal(merged, 21u);
        const auto flippedMissing = b.Equal(flipped, 246u);
        const auto flippedPresent = b.Equal(flipped, 245u);
        b.Fold();
        return b.Folds(choiceMissing, false) && b.Folds(choiceMissingNot, true) && b.Keeps(choiceFirst) && b.Keeps(choiceSecond) && b.Keeps(choiceShared) && b.Folds(mergedMissing, false) && b.Keeps(mergedPresent) && b.Folds(flippedMissing, false) && b.Keeps(flippedPresent);
    });

    passed &= run("an unsigned less-than folds from the known-bit and value-set bounds", [] {
        Builder b;
        auto& scaled = b.Binary(IrOpcode::IMul32, b.Binary(IrOpcode::BitwiseAnd32, b.Shifted(4u), 0x3ffu), 3u);
        auto& masked = b.Binary(IrOpcode::BitwiseAnd32, b.Unknown(), 0xfffu);
        auto& raised = b.Binary(IrOpcode::BitwiseOr32, b.Unknown(), 0x100u);
        const auto scaledBelow = b.Less(scaled, 3025u);
        const auto scaledEdge = b.Less(scaled, 3024u);
        const auto maskedBelow = b.Less(masked, 0x1000u);
        const auto maskedEdge = b.Less(masked, 0xfffu);
        const auto raisedAbove = b.Less(raised, 0x100u);
        const auto raisedEdge = b.Less(raised, 0x101u);
        const auto lane = b.Less(b.Unknown(), 0xffffffffu);
        b.Fold();
        return b.Folds(scaledBelow, true) && b.Keeps(scaledEdge) && b.Folds(maskedBelow, true) && b.Keeps(maskedEdge) && b.Folds(raisedAbove, false) && b.Keeps(raisedEdge) && b.Keeps(lane);
    });

    passed &= run("relative indexing compares fold for an offset of 4 * i + 1", [] {
        Builder b;
        auto& index = b.Emit(IrOpcode::ReadFirstLane, IrType::U32, {&b.Shifted(2u), &b.Emit(IrOpcode::IEqual32, IrType::Bool, {&b.Unknown(), &b.Constant(0u)})});
        auto& offset = b.Binary(IrOpcode::IAdd32, index, 1u);
        std::vector<std::size_t> hits;
        for (std::uint32_t entry = 1u; entry < 16u; ++entry) hits.push_back(b.Equal(offset, entry));
        b.Fold();
        bool matched = true;
        for (std::uint32_t entry = 1u; entry < 16u; ++entry) matched &= (entry & 3u) == 1u ? b.Keeps(hits[entry - 1u]) : b.Folds(hits[entry - 1u], false);
        return matched;
    });

    return passed ? 0 : 1;
}
