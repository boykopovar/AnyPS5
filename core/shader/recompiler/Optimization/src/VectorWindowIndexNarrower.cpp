#include "Optimization/VectorWindowIndexNarrower.hpp"
#include <algorithm>
#include <bit>
#include <optional>
#include <unordered_map>
#include <vector>

namespace ShaderRecompiler {

namespace {

constexpr std::size_t MaxValues = 16;

using Values = std::optional<std::vector<std::uint32_t>>;

Values finish(std::vector<std::uint32_t> values) {
    std::ranges::sort(values);
    values.erase(std::unique(values.begin(), values.end()), values.end());
    if (values.size() > MaxValues) return std::nullopt;
    return values;
}

template <typename Operation>
Values combine(const Values& lhs, const Values& rhs, Operation operation) {
    if (!lhs || !rhs || lhs->size() * rhs->size() > MaxValues * MaxValues) return std::nullopt;
    std::vector<std::uint32_t> values;
    for (const auto left : *lhs) {
        for (const auto right : *rhs) values.push_back(operation(left, right));
    }
    return finish(std::move(values));
}

Values submasks(std::uint32_t mask) {
    if (static_cast<std::size_t>(1u) << std::popcount(mask) > MaxValues) return std::nullopt;
    std::vector<std::uint32_t> values;
    for (std::uint32_t subset = mask;; subset = (subset - 1u) & mask) {
        values.push_back(subset);
        if (subset == 0u) break;
    }
    return finish(std::move(values));
}

Values possibleValues(const IrValue& value, unsigned depth = 0) {
    if (value.HasImmediate() && value.Type() == IrType::U32) return std::vector<std::uint32_t>{value.ImmediateU32()};
    if (depth >= 8u || value.Type() != IrType::U32) return std::nullopt;
    const auto arg = [&](std::size_t index) -> const IrValue& { return *value.Argument(index)->Resolve(); };
    const auto immediate = [&](std::size_t index) -> std::optional<std::uint32_t> {
        if (arg(index).HasImmediate() && arg(index).Type() == IrType::U32) return arg(index).ImmediateU32();
        return std::nullopt;
    };
    const auto operand = [&](std::size_t index) { return possibleValues(arg(index), depth + 1); };
    switch (value.Opcode()) {
        case IrOpcode::IAdd32: return combine(operand(0), operand(1), [](std::uint32_t a, std::uint32_t b) { return a + b; });
        case IrOpcode::ISub32: return combine(operand(0), operand(1), [](std::uint32_t a, std::uint32_t b) { return a - b; });
        case IrOpcode::BitwiseOr32: return combine(operand(0), operand(1), [](std::uint32_t a, std::uint32_t b) { return a | b; });
        case IrOpcode::BitwiseXor32: return combine(operand(0), operand(1), [](std::uint32_t a, std::uint32_t b) { return a ^ b; });
        case IrOpcode::UMin32: return combine(operand(0), operand(1), [](std::uint32_t a, std::uint32_t b) { return std::min(a, b); });
        case IrOpcode::UMax32: return combine(operand(0), operand(1), [](std::uint32_t a, std::uint32_t b) { return std::max(a, b); });
        case IrOpcode::BitwiseAnd32: {
            const auto lhs = operand(0);
            const auto rhs = operand(1);
            if (lhs && rhs) return combine(lhs, rhs, [](std::uint32_t a, std::uint32_t b) { return a & b; });
            if (const auto mask = immediate(1)) return submasks(*mask);
            if (const auto mask = immediate(0)) return submasks(*mask);
            return std::nullopt;
        }
        case IrOpcode::ShiftLeftLogical32:
        case IrOpcode::ShiftRightLogical32: {
            const auto amount = immediate(1);
            if (!amount || *amount >= 32u) return std::nullopt;
            const bool left = value.Opcode() == IrOpcode::ShiftLeftLogical32;
            return combine(operand(0), std::vector<std::uint32_t>{*amount}, [left](std::uint32_t a, std::uint32_t b) { return left ? a << b : a >> b; });
        }
        case IrOpcode::BitFieldUExtract: {
            const auto offset = immediate(1);
            const auto width = immediate(2);
            if (!offset || !width || *offset >= 32u || *width == 0u || *width >= 32u) return std::nullopt;
            const std::uint32_t mask = (std::uint32_t{1} << *width) - 1u;
            if (const auto source = operand(0)) return combine(source, std::vector<std::uint32_t>{*offset}, [mask](std::uint32_t a, std::uint32_t b) { return (a >> b) & mask; });
            return submasks(mask);
        }
        case IrOpcode::SelectU32: {
            const auto whenTrue = operand(1);
            const auto whenFalse = operand(2);
            if (!whenTrue || !whenFalse) return std::nullopt;
            std::vector<std::uint32_t> values(whenTrue->begin(), whenTrue->end());
            values.insert(values.end(), whenFalse->begin(), whenFalse->end());
            return finish(std::move(values));
        }
        default: return std::nullopt;
    }
}

IrValue& immediate(IrProgram& program, std::uint32_t value) {
    IrValue& constant = program.CreateValue(IrOpcode::Void, IrType::U32);
    constant.SetImmediateU32(value);
    return constant;
}

}

VectorWindowIndexNarrowingStats VectorWindowIndexNarrower::Narrow(IrProgram& program) const {
    VectorWindowIndexNarrowingStats stats;
    const std::uint32_t slots = program.Metadata().vectorWindowSize;
    if (slots == 0u) return stats;
    std::unordered_map<const IrValue*, std::vector<std::uint32_t>> narrowed;
    for (const auto& block : program.Blocks()) {
        for (const IrValue* access : block->Instructions()) {
            if (access->Opcode() != IrOpcode::LoadVectorWindow && access->Opcode() != IrOpcode::StoreVectorWindow) continue;
            const IrValue* index = access->Argument(0)->Resolve();
            if (index->HasImmediate()) continue;
            if (access->Opcode() == IrOpcode::StoreVectorWindow) return stats;
            auto values = possibleValues(*index);
            if (!values || values->empty() || values->back() >= slots) return stats;
            narrowed.emplace(access, std::move(*values));
        }
    }
    for (const auto& block : program.Blocks()) {
        auto& instructions = block->Instructions();
        for (auto position = instructions.begin(); position != instructions.end();) {
            IrValue* load = *position;
            const auto found = narrowed.find(load);
            if (found == narrowed.end()) {
                ++position;
                continue;
            }
            IrValue* index = load->Argument(0)->Resolve();
            const auto* values = &found->second;
            const auto slotLoad = [&](std::uint32_t slot) -> IrValue& {
                IrValue& loaded = program.CreateValue(IrOpcode::LoadVectorWindow, IrType::U32);
                loaded.AddArgument(&immediate(program, slot));
                block->InsertInstructionBefore(load, &loaded);
                return loaded;
            };
            IrValue* result = &slotLoad(values->front());
            for (auto slot = std::next(values->begin()); slot != values->end(); ++slot) {
                IrValue& loaded = slotLoad(*slot);
                IrValue& hit = program.CreateValue(IrOpcode::IEqual32, IrType::Bool);
                hit.AddArgument(index);
                hit.AddArgument(&immediate(program, *slot));
                block->InsertInstructionBefore(load, &hit);
                IrValue& chosen = program.CreateValue(IrOpcode::SelectU32, IrType::U32);
                chosen.AddArgument(&hit);
                chosen.AddArgument(&loaded);
                chosen.AddArgument(result);
                block->InsertInstructionBefore(load, &chosen);
                result = &chosen;
            }
            load->ReplaceAllUsesWith(result);
            load->Invalidate();
            position = instructions.erase(position);
            load->SetParent(nullptr);
            ++stats.narrowedLoads;
        }
    }
    return stats;
}

}
