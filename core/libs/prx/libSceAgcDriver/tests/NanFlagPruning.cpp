#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ConstantFolder.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include "Optimization/NanFlagPruner.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <initializer_list>
#include <vector>

using namespace ShaderRecompiler;

namespace {

constexpr std::uint8_t Low = NanCheckLowHalf;
constexpr std::uint8_t High = NanCheckHighHalf;
constexpr std::uint8_t Both = NanCheckLowHalf | NanCheckHighHalf;
constexpr std::uint8_t Word = NanCheckWord;

struct Builder {
    IrProgram program;
    IrBlock* block = nullptr;
    IrValue* chain = nullptr;

    Builder() {
        block = &program.CreateBlock();
        program.BlockOrder().push_back(block);
        program.SetEntryBlock(*block);
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

    IrValue& check(IrValue& value, std::uint8_t lanes, std::uint32_t region = 0) {
        auto& condition = emit(IrOpcode::INotEqual32, IrType::Bool, {&value, &constant(0u)});
        chain = chain == nullptr ? &condition : &emit(IrOpcode::LogicalOr, IrType::Bool, {chain, &condition});
        program.Metadata().nanSpeculationChecks.push_back({&condition, &value, lanes, region});
        return condition;
    }

    std::uint32_t prune() {
        if (chain != nullptr) (void)emit(IrOpcode::Reference, IrType::Void, {chain});
        return NanFlagPruner{}.Prune(program).removedChecks;
    }
};

bool Removed(const IrValue& condition) {
    return !condition.HasUses();
}

struct Case {
    const char* name;
    std::function<bool(Builder&)> run;
};

IrValue& Packed(Builder& b, IrValue& value) {
    return b.emit(IrOpcode::FPAdd16x2, IrType::U32, {&value, &b.unknown()});
}

std::vector<Case> Cases() {
    return {
        {"a packed product read by a checked packed sum is pruned", [](Builder& b) {
            auto& v = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& vc = b.check(v, Both);
            auto& wc = b.check(Packed(b, v), Both);
            return b.prune() == 1u && Removed(vc) && !Removed(wc);
        }},
        {"a high lane shifted out is kept", [](Builder& b) {
            auto& v = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& vc = b.check(v, Both);
            b.check(Packed(b, b.emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&v, &b.constant(16u)})), Both);
            b.prune();
            return !Removed(vc);
        }},
        {"a low lane shifted into the high lane is pruned", [](Builder& b) {
            auto& v = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& vc = b.check(v, Low);
            b.check(Packed(b, b.emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&v, &b.constant(16u)})), Both);
            b.prune();
            return Removed(vc);
        }},
        {"a low lane shifted into an unchecked lane is kept", [](Builder& b) {
            auto& v = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& vc = b.check(v, Low);
            b.check(Packed(b, b.emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&v, &b.constant(16u)})), Low);
            b.prune();
            return !Removed(vc);
        }},
        {"a low lane shifted out to the right is kept", [](Builder& b) {
            auto& v = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& vc = b.check(v, Low);
            b.check(Packed(b, b.emit(IrOpcode::ShiftRightLogical32, IrType::U32, {&v, &b.constant(16u)})), Both);
            b.prune();
            return !Removed(vc);
        }},
        {"a high lane shifted into the low lane is pruned", [](Builder& b) {
            auto& v = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& vc = b.check(v, High);
            b.check(Packed(b, b.emit(IrOpcode::ShiftRightLogical32, IrType::U32, {&v, &b.constant(16u)})), Low);
            b.prune();
            return Removed(vc);
        }},
        {"an extracted high lane does not cover the low lane", [](Builder& b) {
            auto& v = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& vc = b.check(v, Low);
            b.check(Packed(b, b.emit(IrOpcode::BitFieldUExtract, IrType::U32, {&v, &b.constant(16u), &b.constant(16u)})), Both);
            b.prune();
            return !Removed(vc);
        }},
        {"an extracted high lane covers the high lane", [](Builder& b) {
            auto& v = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& vc = b.check(v, High);
            b.check(Packed(b, b.emit(IrOpcode::BitFieldUExtract, IrType::U32, {&v, &b.constant(16u), &b.constant(16u)})), Low);
            b.prune();
            return Removed(vc);
        }},
        {"a check in another region does not cover", [](Builder& b) {
            auto& v = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& vc = b.check(v, Both, 0u);
            b.check(Packed(b, v), Both, 1u);
            b.prune();
            return !Removed(vc);
        }},
        {"an integer add does not propagate", [](Builder& b) {
            auto& v = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& vc = b.check(v, Both);
            b.check(Packed(b, b.emit(IrOpcode::IAdd32, IrType::U32, {&v, &b.constant(1u)})), Both);
            b.prune();
            return !Removed(vc);
        }},
        {"a select on an unrelated condition does not propagate", [](Builder& b) {
            auto& v = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& vc = b.check(v, Both);
            auto& condition = b.emit(IrOpcode::IEqual32, IrType::Bool, {&b.unknown(), &b.constant(0u)});
            b.check(Packed(b, b.emit(IrOpcode::SelectU32, IrType::U32, {&condition, &b.constant(0u), &v})), Both);
            b.prune();
            return !Removed(vc);
        }},
        {"a magnitude mask and a sign flip propagate, a magnitude flip does not", [](Builder& b) {
            auto& v = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& vc = b.check(v, Both);
            auto& masked = b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&v, &b.constant(0x7fff7fffu)});
            auto& flipped = b.emit(IrOpcode::BitwiseXor32, IrType::U32, {&masked, &b.constant(0x80008000u)});
            b.check(Packed(b, flipped), Both);
            auto& u = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& uc = b.check(u, Both);
            b.check(Packed(b, b.emit(IrOpcode::BitwiseXor32, IrType::U32, {&u, &b.constant(0x7fff7fffu)})), Both);
            auto& t = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& tc = b.check(t, Both);
            b.check(Packed(b, b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&t, &b.constant(0xffff0000u)})), Both);
            b.prune();
            return Removed(vc) && !Removed(uc) && !Removed(tc);
        }},
        {"an f32 product through a denormal flush is pruned, through a partial-exponent test it is kept", [](Builder& b) {
            const auto flushed = [&](IrValue& value, std::uint32_t mask) -> IrValue& {
                auto& bits = b.emit(IrOpcode::BitCastU32F32, IrType::U32, {&value});
                auto& exponent = b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&bits, &b.constant(mask)});
                auto& denormal = b.emit(IrOpcode::IEqual32, IrType::Bool, {&exponent, &b.constant(0u)});
                auto& sign = b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&bits, &b.constant(0x80000000u)});
                auto& selected = b.emit(IrOpcode::SelectU32, IrType::U32, {&denormal, &sign, &bits});
                return b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&selected});
            };
            auto& v = b.emit(IrOpcode::FPMul32, IrType::F32, {&b.unknown(), &b.unknown()});
            auto& vc = b.check(v, Word);
            b.check(b.emit(IrOpcode::FPAdd32, IrType::F32, {&flushed(v, 0x7f800000u), &b.unknown()}), Word);
            auto& u = b.emit(IrOpcode::FPMul32, IrType::F32, {&b.unknown(), &b.unknown()});
            auto& uc = b.check(u, Word);
            b.check(b.emit(IrOpcode::FPAdd32, IrType::F32, {&flushed(u, 0x7f000000u), &b.unknown()}), Word);
            b.prune();
            return Removed(vc) && !Removed(uc);
        }},
        {"lanes through f32 and a two-lane pack keep their position", [](Builder& b) {
            const auto widened = [&](IrValue& value) -> IrValue& {
                auto& low = b.emit(IrOpcode::BitFieldUExtract, IrType::U32, {&value, &b.constant(0u), &b.constant(16u)});
                auto& narrow = b.emit(IrOpcode::ConvertU16U32, IrType::U16, {&low});
                auto& half = b.emit(IrOpcode::BitCastF16U16, IrType::F16, {&narrow});
                return b.emit(IrOpcode::ConvertF32F16, IrType::F32, {&half});
            };
            auto& v = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& vc = b.check(v, Low);
            auto& sum = b.emit(IrOpcode::FPAdd32, IrType::F32, {&widened(v), &b.unknown()});
            auto& pair = b.emit(IrOpcode::CompositeConstructF32x2, IrType::F32x2, {&b.unknown(), &sum});
            auto& packed = b.emit(IrOpcode::PackHalf2x16, IrType::U32, {&pair});
            b.check(packed, Low);
            auto& u = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& uc = b.check(u, Low);
            auto& usum = b.emit(IrOpcode::FPAdd32, IrType::F32, {&widened(u), &b.unknown()});
            auto& upair = b.emit(IrOpcode::CompositeConstructF32x2, IrType::F32x2, {&b.unknown(), &usum});
            b.check(b.emit(IrOpcode::PackHalf2x16, IrType::U32, {&upair}), High);
            b.prune();
            return !Removed(vc) && Removed(uc);
        }},
        {"a lane write passes its new value but not the old one", [](Builder& b) {
            auto& v = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& vc = b.check(v, Both);
            auto& exec = b.emit(IrOpcode::IEqual32, IrType::Bool, {&b.unknown(), &b.constant(0u)});
            auto& written = b.emit(IrOpcode::SelectU32, IrType::U32, {&exec, &v, &b.unknown()});
            b.program.Metadata().nanSpeculationLaneWrites.push_back(&written);
            b.check(Packed(b, written), Both);
            auto& u = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& uc = b.check(u, Both);
            auto& kept = b.emit(IrOpcode::SelectU32, IrType::U32, {&exec, &b.unknown(), &u});
            b.program.Metadata().nanSpeculationLaneWrites.push_back(&kept);
            b.check(Packed(b, kept), Both);
            b.prune();
            return Removed(vc) && !Removed(uc) && b.program.Metadata().nanSpeculationLaneWrites.empty();
        }},
        {"a branch condition the constant fold replaces still names a live value after dead code elimination", [](Builder& b) {
            auto& compare = b.emit(IrOpcode::IEqual32, IrType::Bool, {&b.constant(1u), &b.constant(1u)});
            b.emit(IrOpcode::Reference, IrType::Void, {&compare});
            BlockInfo info;
            info.id = 0;
            info.terminator.kind = TerminatorKind::ConditionalBranch;
            info.condition = &compare;
            b.program.Metadata().blockInfo.push_back(info);
            ConstantFolder{}.Fold(b.program);
            DeadCodeEliminator{}.RemoveIdentities(b.program);
            DeadCodeEliminator{}.Eliminate(b.program);
            const IrValue* condition = b.program.Metadata().blockInfo.front().condition->Resolve();
            return condition->Opcode() != IrOpcode::Unreachable && (condition->HasImmediate() || condition->Parent() != nullptr);
        }},
        {"a NaN check through an identity is pruned", [](Builder& b) {
            auto& v = b.emit(IrOpcode::FPMul16x2, IrType::U32, {&b.unknown(), &b.unknown()});
            auto& vc = b.check(v, Both);
            auto& same = b.emit(IrOpcode::Identity, IrType::U32, {&v});
            b.check(Packed(b, same), Both);
            b.prune();
            return Removed(vc);
        }},
    };
}

}

int main() {
    try {
        std::size_t failures = 0;
        for (const auto& test : Cases()) {
            Builder builder;
            if (test.run(builder)) continue;
            std::fprintf(stderr, "nan flag pruning: %s\n", test.name);
            ++failures;
        }
        if (failures != 0u) return 1;
        std::puts("nan flag pruning tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
