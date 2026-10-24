#include <relinker/analysis/RelativeJumpTables.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <cstring>

namespace Relinker {

namespace {

constexpr VirtualAddress MaxTableGapAfterFunction = 0x10000;

struct TableLoad {
    std::size_t At;
    std::size_t Target;
};

bool IsNop(std::span<const std::uint8_t> code, std::size_t at, std::size_t length) {
    auto position = at;
    const auto end = at + length;
    while (position < end && (code[position] == 0x66 || code[position] == 0x2e || code[position] == 0x3e)) ++position;
    if (position >= end) return false;
    if (code[position] == 0x90) return position + 1 == end;
    return code[position] == 0x0f && position + 1 < end && code[position + 1] == 0x1f;
}

bool OnlyPadding(std::span<const std::uint8_t> code, std::size_t from, std::size_t to, const Codegen::X64InstructionDecoder& decoder) {
    for (auto position = from; position < to;) {
        if (code[position] == 0x00 || code[position] == 0xcc || code[position] == 0x90) {
            ++position;
            continue;
        }
        try {
            const auto length = decoder.DecodeInstruction(code.data() + position, to - position).Length;
            if (length == 0 || length > to - position || !IsNop(code, position, length)) return false;
            position += length;
        } catch (const Codegen::CodegenException&) {
            return false;
        }
    }
    return true;
}

std::size_t TableEntries(std::span<const std::uint8_t> code, std::size_t table, std::size_t end, std::size_t firstTable, const std::vector<bool>& instructionStarts) {
    std::size_t entries = 0;
    while (table + (entries + 1) * 4 <= end) {
        std::int32_t relative = 0;
        std::memcpy(&relative, code.data() + table + entries * 4, sizeof(relative));
        const auto destination = static_cast<std::int64_t>(table) + relative;
        if (destination < 0 || destination >= static_cast<std::int64_t>(firstTable) || !instructionStarts[static_cast<std::size_t>(destination)]) break;
        ++entries;
    }
    return entries;
}

std::size_t TablesEnd(std::span<const std::uint8_t> code, std::size_t firstTable, const std::vector<TableLoad>& loads, const std::vector<bool>& instructionStarts) {
    auto table = firstTable;
    while (table < code.size()) {
        auto end = code.size();
        for (const auto& load : loads)
            if (load.Target > table && load.Target < end) end = load.Target;
        const auto entries = TableEntries(code, table, end, firstTable, instructionStarts);
        if (entries == 0 || (table == firstTable && entries < 2)) return table == firstTable ? 0 : table;
        table += entries * 4;
        if (std::none_of(loads.begin(), loads.end(), [&](const auto& load) { return load.Target == table; })) return table;
    }
    return table;
}

}

std::size_t FindTrailingRelativeJumpTable(std::span<const std::uint8_t> code, VirtualAddress codeAddress, std::size_t* tableEnd, std::size_t unwindEnd) {
    const Codegen::X64InstructionDecoder decoder;
    std::vector<TableLoad> loads;
    std::vector<bool> instructionStarts(code.size() + 1, false);
    bool flowEnded = false;
    bool sawIndirectJump = false;
    std::size_t lastIndirectJump = 0;
    for (std::size_t position = 0; position < code.size();) {
        instructionStarts[position] = true;
        if (flowEnded && sawIndirectJump) {
            for (const auto& load : loads) {
                if (load.Target != position || load.At >= lastIndirectJump) continue;
                const auto end = TablesEnd(code, position, loads, instructionStarts);
                if (end == 0) continue;
                const auto aligned = std::min<std::size_t>(code.size(), static_cast<std::size_t>(((codeAddress + end + 15) & ~VirtualAddress{15}) - codeAddress));
                const auto checkedTo = std::max(aligned, std::min(code.size(), unwindEnd));
                const bool toRangeEnd = OnlyPadding(code, end, code.size(), decoder);
                if (!toRangeEnd && !OnlyPadding(code, end, checkedTo, decoder)) continue;
                if (tableEnd) *tableEnd = toRangeEnd ? code.size() : checkedTo;
                return position;
            }
        }
        Codegen::DecodedInstructionInfo info;
        try {
            info = decoder.DecodeInstruction(code.data() + position, code.size() - position);
        } catch (const Codegen::CodegenException&) {
            break;
        }
        if (info.Length == 0 || info.Length > code.size() - position) break;
        const auto next = position + info.Length;
        if (info.HasRipRelativeDisp && !info.IsTwoByteOpcode && info.OpcodeOffset < info.Length && info.Length >= 4 && code[position + info.OpcodeOffset] == 0x8d) {
            std::int32_t displacement = 0;
            std::memcpy(&displacement, code.data() + next - 4, sizeof(displacement));
            const auto target = static_cast<std::int64_t>(next) + displacement;
            if (target > static_cast<std::int64_t>(next) && target < static_cast<std::int64_t>(code.size())) loads.push_back({position, static_cast<std::size_t>(target)});
        }
        if (info.FlowKind == Codegen::ControlFlowKind::IndirectJump) {
            sawIndirectJump = true;
            lastIndirectJump = position;
        }
        if (!IsNop(code, position, info.Length)) {
            const bool directJumpOrCall = (info.Length == 5 && (code[position] == 0xe8 || code[position] == 0xe9)) || (info.Length == 2 && code[position] == 0xeb);
            flowEnded = directJumpOrCall || info.FlowKind == Codegen::ControlFlowKind::Return || info.FlowKind == Codegen::ControlFlowKind::IndirectJump || info.FlowKind == Codegen::ControlFlowKind::Trap;
        }
        position = next;
    }
    return code.size();
}

std::vector<RelativeJumpTable> FindRelativeJumpTables(std::span<const std::uint8_t> segment, VirtualAddress segmentVAddr, const std::vector<std::pair<VirtualAddress, VirtualAddress>>& functions) {
    std::vector<RelativeJumpTable> tables;
    const auto segmentEnd = segmentVAddr + segment.size();
    for (std::size_t index = 0; index < functions.size(); ++index) {
        const auto [begin, end] = functions[index];
        if (begin < segmentVAddr || begin >= segmentEnd || end <= begin) continue;
        auto inspectEnd = std::min(end, segmentEnd);
        if (index + 1 < functions.size() && functions[index + 1].first > inspectEnd && functions[index + 1].first - inspectEnd <= MaxTableGapAfterFunction)
            inspectEnd = std::min(functions[index + 1].first, segmentEnd);
        const auto code = segment.subspan(begin - segmentVAddr, inspectEnd - begin);
        std::size_t tableEnd = code.size();
        if (const auto tail = FindTrailingRelativeJumpTable(code, begin, &tableEnd, std::min(end, segmentEnd) - begin); tail != 0 && tail < code.size())
            tables.push_back({begin, begin + tail, begin + tableEnd});
    }
    return tables;
}

}
