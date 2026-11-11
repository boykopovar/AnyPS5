#include "Optimization/NanFlagPruner.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ShaderRecompiler {
namespace {

constexpr std::uint8_t Low = NanCheckLowHalf;
constexpr std::uint8_t High = NanCheckHighHalf;
constexpr std::uint8_t Word = NanCheckWord;
constexpr std::size_t SearchLimit = 256u;

bool Immediate(const IrValue& value, std::uint32_t& result) {
    const IrValue* resolved = value.Resolve();
    if (!resolved->HasImmediate()) return false;
    result = resolved->ImmediateU32();
    return true;
}

std::uint8_t KeptByMask(std::uint32_t mask) {
    std::uint8_t lanes = 0;
    if ((mask & 0x7fffu) == 0x7fffu) lanes |= Low;
    if (((mask >> 16u) & 0x7fffu) == 0x7fffu) lanes |= High;
    if ((mask & 0x7fffffffu) == 0x7fffffffu) lanes |= Word;
    return lanes;
}

std::uint8_t KeptBySignFlip(std::uint32_t flip) {
    std::uint8_t lanes = 0;
    if ((flip & 0x7fffu) == 0u) lanes |= Low;
    if (((flip >> 16u) & 0x7fffu) == 0u) lanes |= High;
    if ((flip & 0x7fffffffu) == 0u) lanes |= Word;
    return lanes;
}

std::uint8_t ZeroExponentTest(const IrValue& condition, const IrValue& value) {
    const IrValue* test = condition.Resolve();
    if (test->Opcode() != IrOpcode::IEqual32 || test->ArgumentCount() != 2u) return 0u;
    for (std::size_t side = 0; side < 2u; ++side) {
        std::uint32_t zero = 0;
        std::uint32_t mask = 0;
        const IrValue* masked = test->Argument(side)->Resolve();
        if (!Immediate(*test->Argument(1u - side), zero) || zero != 0u || masked->Opcode() != IrOpcode::BitwiseAnd32 || masked->ArgumentCount() != 2u) continue;
        for (std::size_t operand = 0; operand < 2u; ++operand) {
            if (masked->Argument(operand)->Resolve() != &value || !Immediate(*masked->Argument(1u - operand), mask)) continue;
            std::uint8_t lanes = 0;
            if ((mask & 0x7c00u) == 0x7c00u) lanes |= Low;
            if (((mask >> 16u) & 0x7c00u) == 0x7c00u) lanes |= High;
            if ((mask & 0x7f800000u) == 0x7f800000u) lanes |= Word;
            return lanes;
        }
    }
    return 0u;
}

std::uint8_t Propagate(const IrValue& user, std::size_t argument, std::uint8_t lane) {
    std::uint32_t immediate = 0;
    switch (user.Opcode()) {
        case IrOpcode::SelectU32:
        case IrOpcode::SelectF32:
            return argument == 2u && user.ArgumentCount() == 3u ? lane & ZeroExponentTest(*user.Argument(0u), *user.Argument(2u)->Resolve()) : 0u;
        case IrOpcode::FPAdd16x2:
        case IrOpcode::FPMul16x2:
        case IrOpcode::FPFma16x2:
            return lane & (Low | High);
        case IrOpcode::FPAdd32:
        case IrOpcode::FPSub32:
        case IrOpcode::FPMul32:
        case IrOpcode::FPFma32:
        case IrOpcode::FPMad32:
        case IrOpcode::BitCastU32F32:
        case IrOpcode::BitCastF32U32:
            return lane & Word;
        case IrOpcode::BitCastF16U16:
        case IrOpcode::BitCastU16F16:
        case IrOpcode::ConvertU32U16:
        case IrOpcode::ConvertU16U32:
            return lane & Low;
        case IrOpcode::ConvertF32F16:
            return (lane & Low) != 0u ? Word : 0u;
        case IrOpcode::ConvertF16F32:
            return (lane & Word) != 0u ? Low : 0u;
        case IrOpcode::BitwiseOr32:
            return lane;
        case IrOpcode::PackHalf2x16:
            return lane & (Low | High);
        case IrOpcode::CompositeConstructF32x2:
            return (lane & Word) == 0u || argument > 1u ? 0u : argument == 0u ? Low : High;

        case IrOpcode::BitwiseAnd32:
            return user.ArgumentCount() == 2u && Immediate(*user.Argument(1u - argument), immediate) ? lane & KeptByMask(immediate) : 0u;
        case IrOpcode::BitwiseXor32:
            return user.ArgumentCount() == 2u && Immediate(*user.Argument(1u - argument), immediate) ? lane & KeptBySignFlip(immediate) : 0u;
        case IrOpcode::ShiftLeftLogical32:
            return argument == 0u && Immediate(*user.Argument(1u), immediate) && immediate == 16u && (lane & Low) != 0u ? High : 0u;
        case IrOpcode::ShiftRightLogical32:
            return argument == 0u && Immediate(*user.Argument(1u), immediate) && immediate == 16u && (lane & High) != 0u ? Low : 0u;
        case IrOpcode::BitFieldUExtract: {
            std::uint32_t offset = 0;
            std::uint32_t count = 0;
            if (argument != 0u || user.ArgumentCount() != 3u || !Immediate(*user.Argument(1u), offset) || !Immediate(*user.Argument(2u), count) || count != 16u) return 0u;
            if (offset == 0u) return lane & Low;
            if (offset == 16u && (lane & High) != 0u) return Low;
            return 0u;
        }
        default:
            return 0u;
    }
}

bool Reaches(IrValue& start, std::uint8_t lane, const std::unordered_map<const IrValue*, std::uint8_t>& kept, const std::unordered_set<const IrValue*>& laneWrites) {
    std::vector<std::pair<IrValue*, std::uint8_t>> pending{{&start, lane}};
    std::unordered_map<const IrValue*, std::uint8_t> seen;
    std::size_t steps = 0;
    while (!pending.empty() && steps++ < SearchLimit) {
        const auto [value, lanes] = pending.back();
        pending.pop_back();
        auto& visited = seen[value];
        if ((visited & lanes) == lanes) continue;
        visited |= lanes;
        if (const auto found = kept.find(value); found != kept.end() && (found->second & lanes) != 0u) return true;
        for (IrValue* user : value->Uses()) {
            if (user == nullptr || user->Parent() == nullptr) continue;
            if (user->IsIdentity()) {
                pending.emplace_back(user, lanes);
                continue;
            }
            for (std::size_t argument = 0; argument < user->ArgumentCount(); ++argument) {
                if (user->Argument(argument) != value) continue;
                const bool laneWrite = argument == 1u && user->ArgumentCount() == 3u && laneWrites.contains(user);
                const std::uint8_t next = laneWrite ? lanes : Propagate(*user, argument, lanes);
                if (next != 0u) pending.emplace_back(user, next);
            }
        }
    }
    return false;
}

}

NanFlagPruneStats NanFlagPruner::Prune(IrProgram& program) const {
    NanFlagPruneStats stats;
    auto& checks = program.Metadata().nanSpeculationChecks;
    if (checks.empty()) {
        program.Metadata().nanSpeculationLaneWrites.clear();
        return stats;
    }
    IrBuilder builder(program);
    const auto& writes = program.Metadata().nanSpeculationLaneWrites;
    const std::unordered_set<const IrValue*> laneWrites(writes.begin(), writes.end());
    std::unordered_map<const IrValue*, std::uint8_t> kept;
    std::uint32_t region = checks.back().region;
    for (auto check = checks.rbegin(); check != checks.rend(); ++check) {
        if (check->region != region) {
            kept.clear();
            region = check->region;
        }
        IrValue* value = check->value->Resolve();
        IrValue* condition = check->condition;
        if (condition == nullptr || condition->Parent() == nullptr || !condition->HasUses()) continue;
        bool covered = true;
        for (const std::uint8_t lane : {Low, High, Word}) {
            if ((check->lanes & lane) != 0u && !Reaches(*value, lane, kept, laneWrites)) {
                covered = false;
                break;
            }
        }
        if (covered) {
            condition->ReplaceAllUsesWith(&builder.ConstantBool(false));
            ++stats.removedChecks;
            continue;
        }
        kept[value] |= check->lanes;
    }
    checks.clear();
    program.Metadata().nanSpeculationLaneWrites.clear();
    return stats;
}

}
