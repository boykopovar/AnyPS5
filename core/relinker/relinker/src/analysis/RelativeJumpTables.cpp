#include <unordered_map>
#include <unordered_set>
#include <relinker/analysis/RelativeJumpTables.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>
#include <set>

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
        if (relative < 0 && static_cast<std::uint64_t>(-static_cast<std::int64_t>(relative)) > table) break;
        if (relative > 0 && static_cast<std::uint64_t>(relative) > std::numeric_limits<std::int64_t>::max() - table) break;
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

bool IsInvalid64BitSingleByteOpcode(std::uint8_t opcode) {
    switch (opcode) {
    case 0x06:
    case 0x07:
    case 0x0e:
    case 0x16:
    case 0x17:
    case 0x1e:
    case 0x1f:
    case 0x27:
    case 0x2f:
    case 0x37:
    case 0x3f:
    case 0x60:
    case 0x61:
    case 0x82:
    case 0x9a:
    case 0xce:
    case 0xd4:
    case 0xd5:
    case 0xd6:
    case 0xea:
        return true;
    default:
        return false;
    }
}

bool IsForbiddenSyscallInstruction(std::span<const std::uint8_t> code, std::size_t pos, const Codegen::DecodedInstructionInfo& info) {
    const auto opPos = pos + info.OpcodeOffset;
    if (opPos + 1 >= pos + info.Length || opPos + 1 >= code.size()) return false;
    const auto b0 = code[opPos];
    const auto b1 = code[opPos + 1];
    return (b0 == 0x0f && (b1 == 0x05 || b1 == 0x34 || b1 == 0x07)) || (b0 == 0xcd && b1 == 0x80);
}

std::optional<VirtualAddress> RelativeTarget(const VirtualAddress next, const std::int64_t displacement) {
    const auto target = next + static_cast<VirtualAddress>(displacement);
    const bool wrapped = displacement < 0 ? target > next : target < next;
    if (wrapped) return std::nullopt;
    return target;
}

bool IsValid64BitInstruction(std::span<const std::uint8_t> code, std::size_t pos, const Codegen::DecodedInstructionInfo& info, VirtualAddress baseVAddr, VirtualAddress segmentVAddr, VirtualAddress segmentEnd) {
    if (info.Length == 0 || info.Length > code.size() - pos) return false;
    if (!info.IsTwoByteOpcode && IsInvalid64BitSingleByteOpcode(info.Opcode)) return false;
    if (info.HasBranchTarget && !info.HasRipRelativeDisp) {
        const auto target = RelativeTarget(baseVAddr + pos + info.Length, info.BranchDisp);
        if (!target || *target < segmentVAddr || *target >= segmentEnd) return false;
    }
    return true;
}

bool EndsControlFlow(const Codegen::DecodedInstructionInfo& info, bool isSyscall) {
    return info.FlowKind == Codegen::ControlFlowKind::Return
        || info.FlowKind == Codegen::ControlFlowKind::UnconditionalJump
        || info.FlowKind == Codegen::ControlFlowKind::IndirectJump
        || info.FlowKind == Codegen::ControlFlowKind::Trap || isSyscall;
}

class GapAnalysis {
    enum class Trailing { Invalid, Padding, Terminator, NonTerminator };

public:
    GapAnalysis(std::span<const std::uint8_t> code, VirtualAddress baseVAddr, VirtualAddress segmentVAddr, VirtualAddress segmentEnd, const Codegen::X64InstructionDecoder& decoder)
        : code(code), padding(code.size() + 1), trailing(code.size() + 1), syscalls(code.size() + 1) {
        padding.back() = true;
        trailing.back() = Trailing::Padding;
        for (std::size_t end = code.size(); end != 0;) {
            const auto pos = --end;
            const bool bytePadding = code[pos] == 0x00 || code[pos] == 0xcc || code[pos] == 0x90;
            if (bytePadding && padding[pos + 1]) {
                padding[pos] = true;
                trailing[pos] = Trailing::Padding;
                continue;
            }
            Codegen::DecodedInstructionInfo info;
            try {
                info = decoder.DecodeInstruction(code.data() + pos, code.size() - pos);
            } catch (const Codegen::CodegenException&) {
                continue;
            }
            if (info.Length == 0 || info.Length > code.size() - pos) continue;
            const bool nop = IsNop(code, pos, info.Length);
            if (nop && padding[pos + info.Length]) {
                padding[pos] = true;
                trailing[pos] = Trailing::Padding;
                continue;
            }
            if (!IsValid64BitInstruction(code, pos, info, baseVAddr, segmentVAddr, segmentEnd)) continue;
            const auto next = pos + info.Length;
            const bool isSyscall = IsForbiddenSyscallInstruction(code, pos, info);
            syscalls[pos] = isSyscall || syscalls[next];
            if (EndsControlFlow(info, isSyscall)) {
                trailing[pos] = isSyscall || trailing[next] == Trailing::Padding ? Trailing::Terminator : Trailing::NonTerminator;
                continue;
            }
            if (trailing[next] != Trailing::Padding) {
                trailing[pos] = trailing[next];
                continue;
            }
            trailing[pos] = nop || code[pos] == 0xcc || code[pos] == 0x00 ? Trailing::Padding : Trailing::NonTerminator;
        }
    }

    bool OnlyPadding(std::size_t from) const { return padding[from]; }

    bool DecodesAsTrailingCodeBlock(std::size_t from, bool* outHasSyscall = nullptr) const {
        if (outHasSyscall != nullptr) *outHasSyscall = false;
        if (from >= code.size() || code[from] == 0x00 || code[from] == 0xcc || code[from] == 0x90) return false;
        if (outHasSyscall != nullptr) *outHasSyscall = syscalls[from];
        return trailing[from] == Trailing::Terminator;
    }

private:
    std::span<const std::uint8_t> code;
    std::vector<bool> padding;
    std::vector<Trailing> trailing;
    std::vector<bool> syscalls;
};

std::size_t FindEmbeddedDataTableEnd(std::span<const std::uint8_t> code, VirtualAddress tableVAddr, VirtualAddress segmentVAddr, VirtualAddress segmentEnd, const Codegen::X64InstructionDecoder& decoder) {
    const GapAnalysis analysis(code, tableVAddr, segmentVAddr, segmentEnd, decoder);
    std::optional<std::size_t> firstInvalidPos;
    for (std::size_t pos = 0; pos < code.size();) {
        if (analysis.OnlyPadding(pos)) break;
        Codegen::DecodedInstructionInfo info;
        try {
            info = decoder.DecodeInstruction(code.data() + pos, code.size() - pos);
        } catch (const Codegen::CodegenException&) {
            firstInvalidPos = pos;
            break;
        }
        if (!IsValid64BitInstruction(code, pos, info, tableVAddr, segmentVAddr, segmentEnd)) {
            firstInvalidPos = pos;
            break;
        }
        if (IsForbiddenSyscallInstruction(code, pos, info) || info.FlowKind != Codegen::ControlFlowKind::Sequential) return 0;
        pos += info.Length;
    }
    if (!firstInvalidPos) return 0;
    const bool duplicatedEightByteRow = code.size() >= 16 && std::memcmp(code.data(), code.data() + 8, 8) == 0;
    if (*firstInvalidPos != 0 && (!duplicatedEightByteRow || *firstInvalidPos > 4)) return 0;

    std::optional<std::size_t> firstDecodeFailure;
    for (std::size_t pos = 0; pos < code.size();) {
        if (analysis.OnlyPadding(pos)) break;
        try {
            const auto info = decoder.DecodeInstruction(code.data() + pos, code.size() - pos);
            if (info.Length == 0 || info.Length > code.size() - pos) {
                firstDecodeFailure = pos;
                break;
            }
            if (IsForbiddenSyscallInstruction(code, pos, info)) return 0;
            pos += info.Length;
        } catch (const Codegen::CodegenException&) {
            firstDecodeFailure = pos;
            break;
        }
    }
    if (!firstDecodeFailure) return 0;

    auto tableEnd = code.size();
    for (std::size_t pos = *firstDecodeFailure + 1; pos < code.size();) {
        if (code[pos] != 0xcc && code[pos] != 0x90 && code[pos] != 0x00) {
            ++pos;
            continue;
        }
        const auto padStart = pos;
        bool hasZeroPad = false;
        while (pos < code.size() && (code[pos] == 0xcc || code[pos] == 0x90 || code[pos] == 0x00)) {
            if (code[pos] == 0x00) hasZeroPad = true;
            ++pos;
        }
        if (pos < code.size() && analysis.DecodesAsTrailingCodeBlock(pos)) {
            tableEnd = hasZeroPad ? pos : padStart;
            break;
        }
    }
    for (std::size_t pos = *firstDecodeFailure + 1; pos < tableEnd; ++pos) {
        bool hasSyscall = false;
        if (analysis.DecodesAsTrailingCodeBlock(pos, &hasSyscall) && hasSyscall)
            return pos;
    }
    return tableEnd;
}

constexpr std::size_t MaxCalleeSteps = 4096;
constexpr std::size_t MaxCalleeDepth = 8;

struct BranchEdge {
    VirtualAddress Target;
    VirtualAddress Source;
};

using CalleeReturnCache = std::unordered_map<VirtualAddress, bool>;

std::optional<VirtualAddress> DirectBranchTarget(VirtualAddress address, const Codegen::DecodedInstructionInfo& info, VirtualAddress rangeBegin, std::size_t rangeSize) {
    if (!info.HasBranchTarget || info.HasRipRelativeDisp || info.TruncatesBranchTarget || address < rangeBegin) return std::nullopt;
    const auto next = static_cast<std::uint64_t>(address - rangeBegin) + info.Length;
    if (next > rangeSize) return std::nullopt;
    if (info.BranchDisp < 0) {
        const auto back = static_cast<std::uint64_t>(-(info.BranchDisp + 1)) + 1;
        if (back > next) return std::nullopt;
        return rangeBegin + (next - back);
    }
    const auto forward = static_cast<std::uint64_t>(info.BranchDisp);
    if (forward >= rangeSize - next) return std::nullopt;
    return rangeBegin + next + forward;
}

bool CalleeMayReturn(std::span<const std::uint8_t> segment, VirtualAddress segmentVAddr, VirtualAddress callee, const Codegen::X64InstructionDecoder& decoder, CalleeReturnCache& cache, std::unordered_set<VirtualAddress>& active, std::size_t depth) {
    if (callee < segmentVAddr || callee - segmentVAddr >= segment.size()) return false;
    if (const auto found = cache.find(callee); found != cache.end()) return found->second;
    if (depth >= MaxCalleeDepth) return true;
    if (!active.insert(callee).second) return false;
    bool mayReturn = false;
    bool exhausted = false;
    std::size_t steps = 0;
    std::unordered_set<std::size_t> visited;
    std::vector<std::size_t> pending{static_cast<std::size_t>(callee - segmentVAddr)};
    while (!pending.empty() && !mayReturn && !exhausted) {
        auto pos = pending.back();
        pending.pop_back();
        while (pos < segment.size() && visited.insert(pos).second) {
            if (++steps > MaxCalleeSteps) {
                exhausted = true;
                break;
            }
            Codegen::DecodedInstructionInfo info;
            try {
                info = decoder.DecodeInstruction(segment.data() + pos, segment.size() - pos);
            } catch (const Codegen::CodegenException&) {
                break;
            }
            if (info.Length == 0 || info.Length > segment.size() - pos) break;
            if (info.FlowKind == Codegen::ControlFlowKind::Return) {
                mayReturn = true;
                break;
            }
            if (info.FlowKind == Codegen::ControlFlowKind::Trap || info.FlowKind == Codegen::ControlFlowKind::IndirectJump) break;
            const auto target = DirectBranchTarget(segmentVAddr + pos, info, segmentVAddr, segment.size());
            if (info.FlowKind == Codegen::ControlFlowKind::UnconditionalJump) {
                if (target) pending.push_back(static_cast<std::size_t>(*target - segmentVAddr));
                break;
            }
            if (info.FlowKind == Codegen::ControlFlowKind::ConditionalBranch && target) pending.push_back(static_cast<std::size_t>(*target - segmentVAddr));
            if (info.FlowKind == Codegen::ControlFlowKind::Call && target && !CalleeMayReturn(segment, segmentVAddr, *target, decoder, cache, active, depth + 1)) break;
            pos += info.Length;
        }
    }
    active.erase(callee);
    const bool result = mayReturn || exhausted;
    if (result || active.empty()) cache.emplace(callee, result);
    return result;
}

bool DirectCallEndsFlow(VirtualAddress address, const Codegen::DecodedInstructionInfo& info, std::span<const std::uint8_t> segment, VirtualAddress segmentVAddr, const Codegen::X64InstructionDecoder& decoder, CalleeReturnCache& cache) {
    const auto callee = DirectBranchTarget(address, info, segmentVAddr, segment.size());
    std::unordered_set<VirtualAddress> active;
    return !callee || !CalleeMayReturn(segment, segmentVAddr, *callee, decoder, cache, active, 0);
}

bool ReachesRange(std::span<const std::uint8_t> code, VirtualAddress codeAddress, std::size_t from, std::size_t to, const std::vector<std::size_t>& intraTargets, std::span<const BranchEdge> edges, const std::vector<bool>* decoded, std::span<const std::uint8_t> segment, VirtualAddress segmentVAddr, const Codegen::X64InstructionDecoder& decoder, CalleeReturnCache& cache) {
    const auto rangeBegin = codeAddress + from;
    const auto rangeEnd = codeAddress + to;
    if (decoded != nullptr && std::any_of(decoded->begin() + static_cast<std::ptrdiff_t>(rangeBegin - segmentVAddr), decoded->begin() + static_cast<std::ptrdiff_t>(rangeEnd - segmentVAddr), [](bool value) { return value; }))
        return true;
    std::vector<std::size_t> pending(intraTargets);
    pending.push_back(0);
    for (const auto& edge : edges)
        if (edge.Source < rangeBegin || edge.Source >= rangeEnd) pending.push_back(static_cast<std::size_t>(edge.Target - codeAddress));
    std::vector<bool> visited(code.size(), false);
    while (!pending.empty()) {
        auto pos = pending.back();
        pending.pop_back();
        while (pos < code.size() && !visited[pos]) {
            if (pos >= from && pos < to) return true;
            visited[pos] = true;
            Codegen::DecodedInstructionInfo info;
            try {
                info = decoder.DecodeInstruction(code.data() + pos, code.size() - pos);
            } catch (const Codegen::CodegenException&) {
                break;
            }
            if (info.Length == 0 || info.Length > code.size() - pos) break;
            if (pos + info.Length > from && pos < to) return true;
            if (info.FlowKind == Codegen::ControlFlowKind::Return || info.FlowKind == Codegen::ControlFlowKind::IndirectJump || info.FlowKind == Codegen::ControlFlowKind::Trap) break;
            if (const auto target = DirectBranchTarget(codeAddress + pos, info, codeAddress, code.size())) pending.push_back(static_cast<std::size_t>(*target - codeAddress));
            if (info.FlowKind == Codegen::ControlFlowKind::UnconditionalJump) break;
            if (info.FlowKind == Codegen::ControlFlowKind::Call && info.HasBranchTarget && !info.HasRipRelativeDisp && !info.TruncatesBranchTarget
                && DirectCallEndsFlow(codeAddress + pos, info, segment, segmentVAddr, decoder, cache)) break;
            pos += info.Length;
        }
    }
    return false;
}

std::vector<BranchEdge> CollectBranchEdges(std::span<const std::uint8_t> segment, VirtualAddress segmentVAddr, const std::vector<std::pair<VirtualAddress, VirtualAddress>>& functions, const std::vector<VirtualAddress>& scanLimits, const std::vector<bool>& decoded, const Codegen::X64InstructionDecoder& decoder) {
    std::vector<BranchEdge> edges;
    const auto addEdge = [&](std::size_t pos, std::size_t limit) {
        Codegen::DecodedInstructionInfo info;
        try {
            info = decoder.DecodeInstruction(segment.data() + pos, limit - pos);
        } catch (const Codegen::CodegenException&) {
            return std::size_t{0};
        }
        if (info.Length == 0 || info.Length > limit - pos) return std::size_t{0};
        if (const auto target = DirectBranchTarget(segmentVAddr + pos, info, segmentVAddr, segment.size())) edges.push_back({*target, segmentVAddr + pos});
        return info.Length;
    };
    const auto segmentEnd = segmentVAddr + segment.size();
    for (std::size_t index = 0; index < functions.size(); ++index) {
        const auto [begin, end] = functions[index];
        if (begin < segmentVAddr || begin >= segmentEnd || end <= begin) continue;
        const auto scanEnd = std::min(std::min(end, segmentEnd), scanLimits[index]);
        if (scanEnd <= begin) continue;
        const auto limit = static_cast<std::size_t>(scanEnd - segmentVAddr);
        for (auto pos = static_cast<std::size_t>(begin - segmentVAddr); pos < limit;) {
            const auto length = addEdge(pos, limit);
            if (length == 0) break;
            pos += length;
        }
    }
    for (std::size_t pos = 0; pos < decoded.size(); ++pos)
        if (decoded[pos]) addEdge(pos, segment.size());
    std::sort(edges.begin(), edges.end(), [](const BranchEdge& left, const BranchEdge& right) { return left.Target < right.Target; });
    return edges;
}

VirtualAddress FunctionInspectEnd(VirtualAddress segmentEnd, const std::vector<std::pair<VirtualAddress, VirtualAddress>>& functions, std::size_t index, VirtualAddress end) {
    auto inspectEnd = std::min(end, segmentEnd);
    if (index + 1 < functions.size() && functions[index + 1].first > inspectEnd && functions[index + 1].first - inspectEnd <= MaxTableGapAfterFunction)
        inspectEnd = std::min(functions[index + 1].first, segmentEnd);
    return inspectEnd;
}

std::size_t FindTrailingRelativeJumpTableImpl(std::span<const std::uint8_t> code, VirtualAddress codeAddress, std::size_t* tableEnd, std::size_t unwindEnd, const Codegen::X64InstructionDecoder& decoder, std::span<const BranchEdge> edges, const std::vector<bool>* decoded, std::span<const std::uint8_t> segment, VirtualAddress segmentVAddr, CalleeReturnCache& calleeCache) {
    std::vector<TableLoad> loads;
    std::unordered_map<std::size_t, std::vector<std::size_t>> loadsAt;
    std::vector<bool> instructionStarts(code.size() + 1, false);
    std::vector<std::size_t> intraTargets;
    bool flowEnded = false;
    bool sawIndirectJump = false;
    std::size_t lastIndirectJump = 0;
    for (std::size_t position = 0; position < code.size();) {
        instructionStarts[position] = true;
        if (flowEnded && sawIndirectJump) {
            if (const auto found = loadsAt.find(position); found != loadsAt.end()) {
                for (const auto at : found->second) {
                    if (at >= lastIndirectJump) continue;
                    const auto end = TablesEnd(code, position, loads, instructionStarts);
                    if (end == 0) continue;
                    const auto aligned = std::min<std::size_t>(code.size(), static_cast<std::size_t>(((codeAddress + end + 15) & ~VirtualAddress{15}) - codeAddress));
                    const auto checkedTo = std::max(aligned, std::min(code.size(), unwindEnd));
                    const bool toRangeEnd = OnlyPadding(code, end, code.size(), decoder);
                    if (!toRangeEnd && !OnlyPadding(code, end, checkedTo, decoder)) continue;
                    const auto maskEnd = toRangeEnd ? code.size() : checkedTo;
                    if (ReachesRange(code, codeAddress, position, maskEnd, intraTargets, edges, decoded, segment, segmentVAddr, decoder, calleeCache)) continue;
                    if (tableEnd) *tableEnd = maskEnd;
                    return position;
                }
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
            if (displacement > 0 && static_cast<std::uint64_t>(displacement) <= code.size() - next) {
                const auto target = next + static_cast<std::size_t>(displacement);
                if (target < code.size()) {
                    loads.push_back({position, target});
                    loadsAt[target].push_back(position);
                }
            }
        }
        if (info.FlowKind == Codegen::ControlFlowKind::IndirectJump) {
            sawIndirectJump = true;
            lastIndirectJump = position;
        }
        if (const auto target = DirectBranchTarget(codeAddress + position, info, codeAddress, code.size()))
            intraTargets.push_back(static_cast<std::size_t>(*target - codeAddress));
        if (!IsNop(code, position, info.Length)) {
            const bool directJump = (info.Length == 5 && code[position] == 0xe9) || (info.Length == 2 && code[position] == 0xeb);
            const bool endingCall = info.Length == 5 && code[position] == 0xe8 && DirectCallEndsFlow(codeAddress + position, info, segment, segmentVAddr, decoder, calleeCache);
            flowEnded = directJump || endingCall || info.FlowKind == Codegen::ControlFlowKind::Return || info.FlowKind == Codegen::ControlFlowKind::IndirectJump || info.FlowKind == Codegen::ControlFlowKind::Trap;
        }
        position = next;
    }
    return code.size();
}

}

std::size_t FindTrailingRelativeJumpTable(std::span<const std::uint8_t> code, VirtualAddress codeAddress, std::size_t* tableEnd, std::size_t unwindEnd) {
    if (code.size() > std::numeric_limits<VirtualAddress>::max() - codeAddress) {
        throw RelinkerException("Text section range exceeds the address space");
    }
    const Codegen::X64InstructionDecoder decoder;
    CalleeReturnCache calleeCache;
    return FindTrailingRelativeJumpTableImpl(code, codeAddress, tableEnd, unwindEnd, decoder, {}, nullptr, code, codeAddress, calleeCache);
}

std::vector<RelativeJumpTable> FindRelativeJumpTables(std::span<const std::uint8_t> segment, VirtualAddress segmentVAddr, const std::vector<std::pair<VirtualAddress, VirtualAddress>>& functions) {
    if (segment.size() > std::numeric_limits<VirtualAddress>::max() - segmentVAddr) {
        throw RelinkerException("Text section range exceeds the address space");
    }
    std::vector<RelativeJumpTable> tables;
    const auto segmentEnd = segmentVAddr + segment.size();
    const Codegen::X64InstructionDecoder decoder;
    std::set<VirtualAddress> branchTargets;
    for (const auto& [funcBegin, funcEnd] : functions) {
        if (funcBegin < segmentVAddr || funcBegin >= segmentEnd || funcEnd <= funcBegin) continue;
        const auto inspectFuncEnd = std::min(funcEnd, segmentEnd);
        const auto funcCode = segment.subspan(funcBegin - segmentVAddr, inspectFuncEnd - funcBegin);
        for (std::size_t pos = 0; pos < funcCode.size();) {
            Codegen::DecodedInstructionInfo info;
            try {
                info = decoder.DecodeInstruction(funcCode.data() + pos, funcCode.size() - pos);
            } catch (const Codegen::CodegenException&) {
                break;
            }
            if (info.Length == 0 || info.Length > funcCode.size() - pos) break;
            if (info.HasBranchTarget && !info.HasRipRelativeDisp) {
                const auto target = RelativeTarget(funcBegin + pos + info.Length, info.BranchDisp);
                if (target && *target >= segmentVAddr && *target < segmentEnd)
                    branchTargets.insert(*target);
            }
            pos += info.Length;
        }
    }
    auto knownFunctions = functions;
    std::erase_if(knownFunctions, [](const auto& range) { return range.first >= range.second; });
    std::sort(knownFunctions.begin(), knownFunctions.end());
    std::vector<std::pair<VirtualAddress, VirtualAddress>> mergedFunctions;
    for (const auto& range : knownFunctions) {
        if (!mergedFunctions.empty() && range.first <= mergedFunctions.back().second)
            mergedFunctions.back().second = std::max(mergedFunctions.back().second, range.second);
        else
            mergedFunctions.emplace_back(range);
    }
    std::vector<VirtualAddress> pendingTargets(branchTargets.begin(), branchTargets.end());
    std::set<VirtualAddress> exploredGaps;
    std::vector<bool> decoded(segment.size(), false);
    while (!pendingTargets.empty()) {
        const auto current = pendingTargets.back();
        pendingTargets.pop_back();
        if (current < segmentVAddr || current >= segmentEnd) continue;
        const auto after = std::upper_bound(mergedFunctions.begin(), mergedFunctions.end(), current,
            [](VirtualAddress value, const auto& range) { return value < range.first; });
        const bool inKnownFunction = after != mergedFunctions.begin() && current < std::prev(after)->second;
        if (inKnownFunction || !exploredGaps.insert(current).second) continue;
        auto pos = static_cast<std::size_t>(current - segmentVAddr);
        while (pos < segment.size() && !decoded[pos]) {
            decoded[pos] = true;
            Codegen::DecodedInstructionInfo info;
            try {
                info = decoder.DecodeInstruction(segment.data() + pos, segment.size() - pos);
            } catch (const Codegen::CodegenException&) {
                break;
            }
            if (info.Length == 0 || info.Length > segment.size() - pos) break;
            if (info.HasBranchTarget && !info.HasRipRelativeDisp) {
                const auto target = RelativeTarget(segmentVAddr + pos + info.Length, info.BranchDisp);
                if (target && *target >= segmentVAddr && *target < segmentEnd) {
                    if (branchTargets.insert(*target).second) pendingTargets.push_back(*target);
                }
            }
            if (info.FlowKind == Codegen::ControlFlowKind::Return || info.FlowKind == Codegen::ControlFlowKind::IndirectJump || info.FlowKind == Codegen::ControlFlowKind::Trap) break;
            pos += info.Length;
        }
    }
    CalleeReturnCache calleeCache;
    std::vector<VirtualAddress> scanLimits(functions.size(), segmentEnd);
    std::vector<std::optional<RelativeJumpTable>> trailingTables(functions.size());
    for (std::size_t index = 0; index < functions.size(); ++index) {
        const auto [begin, end] = functions[index];
        if (begin < segmentVAddr || begin >= segmentEnd || end <= begin) continue;
        const auto inspectEnd = FunctionInspectEnd(segmentEnd, functions, index, end);
        const auto code = segment.subspan(begin - segmentVAddr, inspectEnd - begin);
        std::size_t tableEnd = code.size();
        if (const auto tail = FindTrailingRelativeJumpTableImpl(code, begin, &tableEnd, std::min(end, segmentEnd) - begin, decoder, {}, &decoded, segment, segmentVAddr, calleeCache); tail != 0 && tail < code.size()) {
            trailingTables[index] = RelativeJumpTable{begin, begin + tail, begin + tableEnd};
            scanLimits[index] = begin + tail;
        }
    }
    for (;;) {
        const auto edges = CollectBranchEdges(segment, segmentVAddr, functions, scanLimits, decoded, decoder);
        bool changed = false;
        for (std::size_t index = 0; index < functions.size(); ++index) {
            if (!trailingTables[index]) continue;
            const auto [begin, end] = functions[index];
            const auto inspectEnd = FunctionInspectEnd(segmentEnd, functions, index, end);
            const auto byTarget = [](const BranchEdge& edge, VirtualAddress address) { return edge.Target < address; };
            const auto edgesBegin = std::lower_bound(edges.begin(), edges.end(), begin, byTarget);
            const std::span<const BranchEdge> codeEdges(edgesBegin, std::lower_bound(edgesBegin, edges.end(), inspectEnd, byTarget));
            const bool hasExternalEdge = std::any_of(codeEdges.begin(), codeEdges.end(), [&](const BranchEdge& edge) {
                return edge.Source < begin || edge.Source >= scanLimits[index];
            });
            if (!hasExternalEdge) continue;
            const auto code = segment.subspan(begin - segmentVAddr, inspectEnd - begin);
            std::size_t tableEnd = code.size();
            const auto tail = FindTrailingRelativeJumpTableImpl(code, begin, &tableEnd, std::min(end, segmentEnd) - begin, decoder, codeEdges, &decoded, segment, segmentVAddr, calleeCache);
            if (tail != 0 && tail < code.size()) {
                const RelativeJumpTable updated{begin, begin + tail, begin + tableEnd};
                if (updated.Begin != trailingTables[index]->Begin || updated.End != trailingTables[index]->End) {
                    trailingTables[index] = updated;
                    scanLimits[index] = updated.Begin;
                    changed = true;
                }
            } else {
                trailingTables[index].reset();
                scanLimits[index] = segmentEnd;
                changed = true;
            }
        }
        if (!changed) break;
    }
    for (std::size_t index = 0; index < functions.size(); ++index) {
        const auto [begin, end] = functions[index];
        if (begin < segmentVAddr || begin >= segmentEnd || end <= begin) continue;
        if (trailingTables[index]) {
            tables.push_back(*trailingTables[index]);
            continue;
        }
        const auto inspectEnd = FunctionInspectEnd(segmentEnd, functions, index, end);
        if (inspectEnd <= end) continue;
        const auto funcCode = segment.subspan(begin - segmentVAddr, end - begin);
        std::optional<VirtualAddress> candidateTable;
        bool sawIndirectJump = false;
        for (std::size_t pos = 0; pos < funcCode.size();) {
            Codegen::DecodedInstructionInfo info;
            try {
                info = decoder.DecodeInstruction(funcCode.data() + pos, funcCode.size() - pos);
            } catch (const Codegen::CodegenException&) {
                break;
            }
            if (info.Length == 0 || info.Length > funcCode.size() - pos) break;
            if (info.FlowKind == Codegen::ControlFlowKind::IndirectJump) sawIndirectJump = true;
            const auto next = pos + info.Length;
            if (info.HasRipRelativeDisp && !info.IsTwoByteOpcode && info.OpcodeOffset < info.Length && info.Length >= 4 && funcCode[pos + info.OpcodeOffset] == 0x8d) {
                std::int32_t displacement = 0;
                std::memcpy(&displacement, funcCode.data() + next - 4, sizeof(displacement));
                const auto target = RelativeTarget(begin + next, displacement);
                if (target && *target >= end && *target < inspectEnd)
                    candidateTable = candidateTable ? std::min(*candidateTable, *target) : *target;
            }
            pos = next;
        }
        if (sawIndirectJump || !candidateTable) continue;
        const auto tableVAddr = *candidateTable;
        const auto paddingCode = segment.subspan(end - segmentVAddr, tableVAddr - end);
        if (!OnlyPadding(paddingCode, 0, paddingCode.size(), decoder)) continue;
        const auto it = branchTargets.lower_bound(tableVAddr);
        if (it != branchTargets.end() && *it < inspectEnd) continue;
        const auto tableCode = segment.subspan(tableVAddr - segmentVAddr, inspectEnd - tableVAddr);
        const auto tableSize = FindEmbeddedDataTableEnd(tableCode, tableVAddr, segmentVAddr, segmentEnd, decoder);
        if (tableSize == 0) continue;
        const auto tableOffset = static_cast<std::ptrdiff_t>(tableVAddr - segmentVAddr);
        if (std::any_of(decoded.begin() + tableOffset, decoded.begin() + tableOffset + static_cast<std::ptrdiff_t>(tableSize), [](bool value) { return value; })) continue;
        tables.push_back({begin, tableVAddr, tableVAddr + tableSize});
    }
    return tables;
}

}
