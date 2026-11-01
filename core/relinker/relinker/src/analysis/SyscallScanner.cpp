#include <relinker/analysis/SyscallScanner.hpp>
#include <codegen/IInstructionScanner.hpp>
#include <codegen/x86/DecodedInstruction.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <limits>

#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <tuple>

namespace Relinker {

static constexpr std::uint8_t SYSCALL_BYTE0 = 0x0F;
static constexpr std::uint8_t SYSCALL_BYTE1 = 0x05;
static constexpr std::uint8_t INT80_BYTE0 = 0xCD;
static constexpr std::uint8_t INT80_BYTE1 = 0x80;
static constexpr std::uint8_t SYSENTER_BYTE0 = 0x0F;
static constexpr std::uint8_t SYSENTER_BYTE1 = 0x34;
static constexpr std::uint8_t SYSRET_BYTE0 = 0x0F;
static constexpr std::uint8_t SYSRET_BYTE1 = 0x07;
static constexpr std::size_t SYSCALL_NUMBER_LOOKBACK = 8;

static constexpr std::array<std::uint8_t, 23> SDK_GENERIC_SYSCALL_STUB{
    0x48, 0x89, 0xF8, 0x48, 0x89, 0xF7, 0x48, 0x89, 0xD6, 0x48, 0x89, 0xCA,
    0x4D, 0x89, 0xC2, 0x4D, 0x89, 0xC8, 0x4C, 0x8B, 0x4C, 0x24, 0x08,
};

static bool IsSdkSyscallStub(const std::vector<std::uint8_t>& code, const std::size_t syscallOffset, const std::size_t limit) {
    constexpr std::array<std::uint8_t, 3> numberLoad{0x48, 0xC7, 0xC0};
    constexpr std::array<std::uint8_t, 3> argumentShuffle{0x49, 0x89, 0xCA};
    if (limit > code.size() || syscallOffset > limit || limit - syscallOffset < 3u)
        return false;
    if (syscallOffset >= SDK_GENERIC_SYSCALL_STUB.size()) {
        const auto begin = code.begin() + static_cast<std::ptrdiff_t>(syscallOffset - SDK_GENERIC_SYSCALL_STUB.size());
        if (std::equal(SDK_GENERIC_SYSCALL_STUB.begin(), SDK_GENERIC_SYSCALL_STUB.end(), begin)
            && code[syscallOffset] == 0x0F && code[syscallOffset + 1] == 0x05 && code[syscallOffset + 2] == 0xC3)
            return true;
    }
    if (syscallOffset < 10u)
        return false;
    const auto begin = code.begin() + static_cast<std::ptrdiff_t>(syscallOffset - 10u);
    return std::equal(numberLoad.begin(), numberLoad.end(), begin)
        && std::equal(argumentShuffle.begin(), argumentShuffle.end(), begin + 7)
        && code[syscallOffset] == 0x0F && code[syscallOffset + 1] == 0x05 && code[syscallOffset + 2] == 0xC3;
}

enum class ForbiddenInstruction {
    None,
    Invocation,
    Return,
};

static constexpr std::array<std::uint8_t, 62> SDK_MPROTECT_INLINE{
    0x55, 0x48, 0x89, 0xE5, 0x53, 0x50, 0x89, 0xD1, 0x49, 0x89, 0xF0, 0x48, 0x89, 0xFE,
    0xF6, 0xC1, 0x04, 0x75, 0x31, 0x48, 0x63, 0xD1, 0xB8, 0x4A, 0x00, 0x00, 0x00,
    0x48, 0x89, 0xF7, 0x4C, 0x89, 0xC6, 0x45, 0x31, 0xD2, 0x45, 0x31, 0xC0, 0x45, 0x31, 0xC9,
    0x0F, 0x05, 0x0F, 0x93, 0xC1, 0x48, 0x89, 0xC3, 0x48, 0xF7, 0xDB, 0x84, 0xC9,
    0x48, 0x0F, 0x45, 0xD8, 0x48, 0x85, 0xDB,
};

static constexpr std::array<std::uint8_t, 37> SDK_MMAP_INLINE{
    0x49, 0x63, 0xD6, 0x4C, 0x63, 0xD1, 0x4D, 0x63, 0xC0, 0xB8, 0xDD, 0x01, 0x00, 0x00,
    0x48, 0x89, 0xDE, 0x0F, 0x05, 0x48, 0x89, 0xC1, 0x0F, 0x93, 0xC2, 0x48, 0xF7, 0xD8,
    0x84, 0xD2, 0x48, 0x0F, 0x45, 0xC1, 0x48, 0x85, 0xC0,
};

static constexpr std::array<std::uint8_t, 43> SDK_MMAP_EXEC_INLINE{
    0x44, 0x89, 0xF0, 0x83, 0xE0, 0xFB, 0x48, 0x63, 0xD0, 0x4C, 0x63, 0xD1, 0x4D, 0x63, 0xC0,
    0xB8, 0xDD, 0x01, 0x00, 0x00, 0x48, 0x89, 0xDE, 0x0F, 0x05, 0x0F, 0x93, 0xC1,
    0x49, 0x89, 0xC7, 0x49, 0xF7, 0xDF, 0x84, 0xC9, 0x4C, 0x0F, 0x45, 0xF8, 0x4D, 0x85, 0xFF,
};

struct RecentInstructions {
    static constexpr std::size_t Capacity = 64;
    std::array<Codegen::InstructionMatch, Capacity> matches{};
    std::size_t count = 0;

    void Push(const Codegen::InstructionMatch& match) {
        matches[count % Capacity] = match;
        ++count;
    }

    [[nodiscard]] const Codegen::InstructionMatch& Before(std::size_t step) const {
        return matches[(count - 1 - step) % Capacity];
    }

    [[nodiscard]] bool Contains(FileByteOffset offset) const {
        for (std::size_t step = 0; step < std::min(count, Capacity); ++step)
            if (Before(step).Offset == offset) return true;
        return false;
    }
};

template<std::size_t TSize>
static bool MatchesSdkInlineStub(const std::vector<std::uint8_t>& code, std::size_t syscallOffset, std::size_t limit, const std::array<std::uint8_t, TSize>& pattern, std::size_t beforeSyscall, const RecentInstructions& recent, FileByteOffset codeOffset) {
    if (syscallOffset < beforeSyscall || syscallOffset > limit) return false;
    const auto start = syscallOffset - beforeSyscall;
    if (pattern.size() > limit - start || !std::equal(pattern.begin(), pattern.end(), code.begin() + static_cast<std::ptrdiff_t>(start))) return false;
    return recent.Contains(codeOffset + start);
}

static bool IsSdkMmanSyscallStub(const std::vector<std::uint8_t>& code, std::size_t syscallOffset, std::size_t limit, const RecentInstructions& recent, FileByteOffset codeOffset) {
    return MatchesSdkInlineStub(code, syscallOffset, limit, SDK_MPROTECT_INLINE, 42, recent, codeOffset) || MatchesSdkInlineStub(code, syscallOffset, limit, SDK_MMAP_INLINE, 17, recent, codeOffset) || MatchesSdkInlineStub(code, syscallOffset, limit, SDK_MMAP_EXEC_INLINE, 23, recent, codeOffset);
}

static ForbiddenInstruction ClassifyForbiddenInstruction(const std::vector<std::uint8_t>& codeSection, const std::size_t opcodeOffset, const std::size_t limit) {
    if (opcodeOffset >= limit || limit - opcodeOffset < 2u)
        return ForbiddenInstruction::None;

    const std::uint8_t byte0 = codeSection[opcodeOffset];
    const std::uint8_t byte1 = codeSection[opcodeOffset + 1];

    const bool isSyscall = (byte0 == SYSCALL_BYTE0 && byte1 == SYSCALL_BYTE1);
    const bool isInt80 = (byte0 == INT80_BYTE0 && byte1 == INT80_BYTE1);
    const bool isSysenter = (byte0 == SYSENTER_BYTE0 && byte1 == SYSENTER_BYTE1);
    const bool isSysret = (byte0 == SYSRET_BYTE0 && byte1 == SYSRET_BYTE1);

    if (isSyscall || isInt80 || isSysenter)
        return ForbiddenInstruction::Invocation;

    if (isSysret)
        return ForbiddenInstruction::Return;

    return ForbiddenInstruction::None;
}

static std::optional<std::uint64_t> ConstantRaxLoad(const std::uint8_t* bytes, const std::size_t length) {
    const std::size_t opcode = Codegen::DecodedInstruction{bytes, length}.OpcodeOffset();
    const bool hasRex = opcode > 0 && (bytes[opcode - 1] & 0xF0) == 0x40;
    const std::uint8_t rex = hasRex ? bytes[opcode - 1] : 0;

    if (hasRex && rex == 0x48 && length - opcode == 6 && bytes[opcode] == 0xC7 && bytes[opcode + 1] == 0xC0) {
        std::int32_t immediate = 0;
        std::memcpy(&immediate, bytes + opcode + 2, sizeof(immediate));
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(immediate));
    }

    if (hasRex && rex == 0x48 && length - opcode == 9 && bytes[opcode] == 0xB8) {
        std::uint64_t value = 0;
        std::memcpy(&value, bytes + opcode + 1, sizeof(value));
        return value;
    }

    if ((rex & 0x09) == 0 && length - opcode == 5 && bytes[opcode] == 0xB8) {
        std::uint32_t value = 0;
        std::memcpy(&value, bytes + opcode + 1, sizeof(value));
        return value;
    }

    if ((rex & 0x05) == 0 && length - opcode == 2 && (bytes[opcode] == 0x31 || bytes[opcode] == 0x33) && bytes[opcode + 1] == 0xC0
        && std::find(bytes, bytes + opcode, 0x66) == bytes + opcode)
        return 0;

    return std::nullopt;
}

static bool MovesOtherRegisterThanRax(const std::uint8_t* bytes, const std::size_t length) {
    const std::uint8_t rex = length == 3 ? bytes[0] : 0;
    if (length == 3 && (rex & 0xF0) != 0x40)
        return false;
    const std::uint8_t opcode = bytes[length - 2];
    const std::uint8_t modrm = bytes[length - 1];
    if ((modrm & 0xC0) != 0xC0)
        return false;
    const bool writesRm = opcode == 0x89 || opcode == 0x31;
    const bool writesReg = opcode == 0x8B || opcode == 0x33;
    if (!writesRm && !writesReg)
        return false;
    const unsigned written = writesRm ? (modrm & 7u) | ((rex & 1u) << 3) : ((modrm >> 3) & 7u) | ((rex & 4u) << 1);
    return written != 0;
}

static std::optional<std::uint64_t> ConstantSyscallNumber(const std::vector<std::uint8_t>& codeSection, const FileByteOffset codeSectionOffset, const RecentInstructions& recent) {
    FileByteOffset next = recent.Before(0).Offset;
    for (std::size_t step = 1; step <= SYSCALL_NUMBER_LOOKBACK && step < recent.count; ++step) {
        const auto& match = recent.Before(step);
        if (match.Offset + match.Length != next)
            return std::nullopt;
        next = match.Offset;
        const std::size_t start = static_cast<std::size_t>(match.Offset - codeSectionOffset);

        if (start >= codeSection.size() || match.Length > codeSection.size() - start)
            return std::nullopt;

        const std::uint8_t* bytes = codeSection.data() + start;

        if (const auto value = ConstantRaxLoad(bytes, match.Length))
            return value;

        const std::size_t opcode = Codegen::DecodedInstruction{bytes, match.Length}.OpcodeOffset();
        const std::size_t begin = opcode > 0 && (bytes[opcode - 1] & 0xF0) == 0x40 ? opcode - 1 : opcode;
        if (match.Length - opcode != 2 || !MovesOtherRegisterThanRax(bytes + begin, match.Length - begin))
            return std::nullopt;
    }

    return std::nullopt;
}

struct ActivePrefixes {
    bool OperandSize;
    std::uint8_t Rex;
    std::size_t Count;
};

static ActivePrefixes ActivePrefixesBefore(const std::vector<std::uint8_t>& code, const std::size_t opcode) {
    ActivePrefixes prefixes{false, 0, 0};
    if (opcode > code.size())
        return prefixes;
    std::size_t start = opcode;
    if (start > 0 && (code[start - 1] & 0xF0) == 0x40) {
        prefixes.Rex = code[start - 1];
        --start;
    }
    while (start > 0) {
        const std::uint8_t prefix = code[start - 1];
        if (prefix == 0x66)
            prefixes.OperandSize = true;
        else if (prefix != 0x67 && prefix != 0xF0 && prefix != 0xF2 && prefix != 0xF3 &&
                 prefix != 0x26 && prefix != 0x2E && prefix != 0x36 && prefix != 0x3E && prefix != 0x64 && prefix != 0x65)
            break;
        --start;
    }
    prefixes.Count = opcode - start;
    return prefixes;
}

static std::optional<std::uint64_t> ConstantRaxSetEndingAt(const std::vector<std::uint8_t>& code, const std::size_t end) {
    if (end > code.size())
        return std::nullopt;
    if (end >= 10 && code[end - 10] == 0x48 && code[end - 9] == 0xB8) {
        std::uint64_t value = 0;
        std::memcpy(&value, code.data() + end - 8, sizeof(value));
        return value;
    }
    std::uint32_t immediate = 0;
    if (end >= 7 && code[end - 7] == 0x48 && code[end - 6] == 0xC7 && code[end - 5] == 0xC0) {
        std::memcpy(&immediate, code.data() + end - 4, sizeof(immediate));
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(immediate)));
    }
    if (end >= 5 && code[end - 5] == 0xB8) {
        const ActivePrefixes prefixes = ActivePrefixesBefore(code, end - 5);
        if (!prefixes.OperandSize && (prefixes.Rex & 0x09) == 0) {
            std::memcpy(&immediate, code.data() + end - 4, sizeof(immediate));
            return immediate;
        }
    }
    if (end >= 2 && (code[end - 2] == 0x31 || code[end - 2] == 0x33) && code[end - 1] == 0xC0) {
        const ActivePrefixes prefixes = ActivePrefixesBefore(code, end - 2);
        const bool extendedOperand = (prefixes.Rex & 0x05) != 0;
        if (!prefixes.OperandSize && !extendedOperand)
            return 0;
    }
    return std::nullopt;
}

struct LowRaxWrite {
    std::size_t Length;
    std::uint64_t Mask;
    std::uint64_t Value;
};

static std::optional<LowRaxWrite> ImmediateLowRaxWriteEndingAt(const std::vector<std::uint8_t>& code, const std::size_t end) {
    if (end > code.size())
        return std::nullopt;
    if (end >= 2 && code[end - 2] == 0xB0) {
        const ActivePrefixes prefixes = ActivePrefixesBefore(code, end - 2);
        if ((prefixes.Rex & 0x01) == 0)
            return LowRaxWrite{prefixes.Count + 2, 0xFF, code[end - 1]};
        return LowRaxWrite{prefixes.Count + 2, 0, 0};
    }
    if (end >= 4 && code[end - 4] == 0x66 && code[end - 3] == 0xB8) {
        const std::size_t length = end >= 5 && (code[end - 5] & 0xF0) == 0x40 ? 5 : 4;
        return LowRaxWrite{length, 0xFFFF, static_cast<std::uint64_t>(code[end - 2]) | static_cast<std::uint64_t>(code[end - 1]) << 8};
    }
    return std::nullopt;
}

static std::optional<std::uint64_t> ConstantSyscallNumberInBytes(const std::vector<std::uint8_t>& code, const std::size_t syscall) {
    if (syscall > code.size())
        return std::nullopt;
    using Step = std::tuple<std::size_t, std::uint64_t, std::uint64_t>;
    std::vector<Step> pending{{syscall, 0, 0}};
    std::set<Step> seen(pending.begin(), pending.end());
    const auto visit = [&](const Step& step) {
        if (seen.insert(step).second)
            pending.push_back(step);
    };
    while (!pending.empty()) {
        const auto [end, mask, low] = pending.back();
        pending.pop_back();
        if (const auto value = ConstantRaxSetEndingAt(code, end))
            return (*value & ~mask) | low;
        if (const auto write = ImmediateLowRaxWriteEndingAt(code, end))
            visit({end - write->Length, mask | write->Mask, low | (write->Value & ~mask)});
        for (const std::size_t length : {std::size_t{2}, std::size_t{3}})
            if (end >= length && MovesOtherRegisterThanRax(code.data() + end - length, length))
                visit({end - length, mask, low});
    }
    return std::nullopt;
}

static bool EndsControlFlow(const Codegen::ControlFlowKind kind) {
    using enum Codegen::ControlFlowKind;
    return kind == Return || kind == Trap || kind == UnconditionalJump || kind == IndirectJump;
}

static bool ContainsOffset(const std::vector<CodeRange>& ranges, const std::size_t offset) {
    const auto after = std::upper_bound(ranges.begin(), ranges.end(), offset, [](const std::size_t value, const CodeRange& range) { return value < range.Begin; });
    return after != ranges.begin() && offset < std::prev(after)->End;
}

static std::size_t NextRangeBegin(const std::vector<CodeRange>& ranges, const std::size_t offset, const std::size_t limit) {
    const auto after = std::upper_bound(ranges.begin(), ranges.end(), offset, [](const std::size_t value, const CodeRange& range) { return value < range.Begin; });
    if (after == ranges.end() || after->Begin >= limit)
        return limit;
    return static_cast<std::size_t>(after->Begin);
}

static void QueueInstructionTargets(std::vector<std::size_t>& pending, const std::vector<bool>& visited, const std::vector<CodeRange>& unwind, const Codegen::DecodedInstructionInfo& info, const std::vector<std::uint8_t>& code, const std::size_t pos, const std::size_t limit) {
    if (info.HasBranchTarget && !info.HasRipRelativeDisp) {
        const auto target = static_cast<std::int64_t>(pos + info.Length) + info.BranchDisp;
        if (target >= 0 && target < static_cast<std::int64_t>(limit)) {
            const auto targetPos = static_cast<std::size_t>(target);
            if (!ContainsOffset(unwind, targetPos) && !visited[targetPos])
                pending.push_back(targetPos);
        }
    }
    if (!info.IsTwoByteOpcode && info.Opcode == 0x8D && info.HasRipRelativeDisp &&
        info.RipRelativeDispOffset + sizeof(std::int32_t) <= info.Length &&
        pos + info.RipRelativeDispOffset + sizeof(std::int32_t) <= code.size()) {
        std::int32_t disp = 0;
        std::memcpy(&disp, code.data() + pos + info.RipRelativeDispOffset, sizeof(disp));
        const auto target = static_cast<std::int64_t>(pos + info.Length) + disp;
        if (target >= 0 && target < static_cast<std::int64_t>(limit)) {
            const auto targetPos = static_cast<std::size_t>(target);
            if (!ContainsOffset(unwind, targetPos) && !visited[targetPos])
                pending.push_back(targetPos);
        }
    }
}

static std::vector<CodeRange> DiscoverRoutines(const std::vector<std::uint8_t>& code, const std::size_t limit, const std::vector<CodeRange>& unwindRanges) {
    if (unwindRanges.empty())
        return {};
    const auto unwind = Codegen::MergeCodeRanges(unwindRanges);
    std::vector<CodeRange> routines = unwind;
    const Codegen::X64InstructionDecoder decoder;
    std::vector<std::size_t> pending;
    std::vector<bool> visited(limit, false);
    for (const auto& range : unwind) {
        if (range.Begin >= limit)
            continue;
        const auto end = static_cast<std::size_t>(std::min(range.End, static_cast<ByteCount>(limit)));
        auto pos = static_cast<std::size_t>(range.Begin);
        while (pos < end) {
            Codegen::DecodedInstructionInfo info;
            try {
                info = decoder.DecodeInstruction(code.data() + pos, end - pos);
            } catch (const Codegen::CodegenException&) {
                break;
            }
            if (info.Length == 0 || info.Length > end - pos)
                break;
            QueueInstructionTargets(pending, visited, unwind, info, code, pos, limit);
            for (std::size_t b = 0; b < info.Length; ++b)
                visited[pos + b] = true;
            pos += info.Length;
        }
    }

    std::set<std::size_t> explored;
    while (!pending.empty()) {
        const auto root = pending.back();
        pending.pop_back();
        if (root >= limit || ContainsOffset(unwind, root) || !explored.insert(root).second)
            continue;
        auto pos = root;
        while (pos < limit && !ContainsOffset(unwind, pos)) {
            if (visited[pos] && pos != root)
                break;
            const auto fence = NextRangeBegin(unwind, pos, limit);
            Codegen::DecodedInstructionInfo info;
            try {
                info = decoder.DecodeInstruction(code.data() + pos, fence - pos);
            } catch (const Codegen::CodegenException&) {
                break;
            }
            if (info.Length == 0 || info.Length > fence - pos)
                break;
            QueueInstructionTargets(pending, visited, unwind, info, code, pos, limit);
            for (std::size_t b = 0; b < info.Length; ++b)
                visited[pos + b] = true;
            pos += info.Length;
            if (EndsControlFlow(info.FlowKind))
                break;
        }
        if (pos > root)
            routines.push_back({root, pos});
    }
    return Codegen::MergeCodeRanges(std::move(routines));
}

class SyscallScanner : public ISyscallScanner {
public:
    SyscallScanner(Domain::IProgress& progress, const bool trapSdkStubs) : progress(progress), trapSdkStubs(trapSdkStubs) {}

    std::vector<std::size_t> ScanCodeSectionForSyscalls(const std::vector<std::uint8_t>& codeSection, FileByteOffset codeSectionOffset, FileByteOffset codeSectionSize, std::string_view input, const std::vector<CodeRange>& unwindRanges) override;

private:
    Domain::IProgress& progress;
    bool trapSdkStubs;
};

std::vector<std::size_t> SyscallScanner::ScanCodeSectionForSyscalls(const std::vector<std::uint8_t>& codeSection, const FileByteOffset codeSectionOffset, const FileByteOffset codeSectionSize, std::string_view input, const std::vector<CodeRange>& unwindRanges) {
    if (codeSectionSize > codeSection.size()) throw RelinkerException("Code section exceeds the input buffer", codeSectionOffset);
    if (codeSectionSize > std::numeric_limits<FileByteOffset>::max() - codeSectionOffset) throw RelinkerException("Code section offset overflow", codeSectionOffset);
    const auto limit = static_cast<std::size_t>(codeSectionSize);

    const std::string source(input);
    const auto functions = DiscoverRoutines(codeSection, limit, unwindRanges);
    const auto scanner = Codegen::MakeInstructionScanner([&](auto completed, auto total) { progress.Report({Domain::ProgressStage::Decode, source, completed, total}); }, functions);
    RecentInstructions recent;
    std::vector<std::size_t> trapped;
    const auto insideFunction = [&](const std::size_t offset) { return ContainsOffset(functions, offset); };
    std::size_t skipped = 0;
    std::optional<FileByteOffset> firstSkipped;
    scanner->VisitCodeSection(codeSection, codeSectionOffset, codeSectionSize, [&](const Codegen::InstructionMatch& match) {
        recent.Push(match);
        if (match.Offset < codeSectionOffset) throw RelinkerException("Instruction match precedes code section offset", match.Offset);
        const std::size_t start = static_cast<std::size_t>(match.Offset - codeSectionOffset);
        if (start > limit || match.Length > limit - start) throw RelinkerException("Instruction match exceeds code section", match.Offset);
        const auto opcodeOffset = Codegen::DecodedInstruction{codeSection.data() + start, match.Length}.OpcodeOffset();
        const std::size_t opcode = start + opcodeOffset;
        const std::size_t spanLimit = std::min(limit, start + static_cast<std::size_t>(match.Length));

        const auto kind = ClassifyForbiddenInstruction(codeSection, opcode, spanLimit);

        if (kind == ForbiddenInstruction::None)
            return;

        if (trapSdkStubs && kind == ForbiddenInstruction::Invocation &&
            (IsSdkSyscallStub(codeSection, opcode, limit) || IsSdkMmanSyscallStub(codeSection, opcode, limit, recent, codeSectionOffset))) {
            trapped.push_back(opcode);
            std::ostringstream message;
            message << "Trapping SDK syscall instruction at code offset 0x" << std::hex << match.Offset << '\n';
            progress.Message(message.str());
            return;
        }

        std::optional<std::uint64_t> number;
        if (kind == ForbiddenInstruction::Invocation)
            number = ConstantSyscallNumber(codeSection, codeSectionOffset, recent);
        if (!unwindRanges.empty() && !number && kind == ForbiddenInstruction::Invocation)
            number = ConstantSyscallNumberInBytes(codeSection, opcode);
        if (!unwindRanges.empty() && !number && !insideFunction(start)) {
            ++skipped;
            if (!firstSkipped) firstSkipped = match.Offset;
            return;
        }

        std::ostringstream msg;
        msg << "Forbidden syscall instruction at code offset 0x" << std::hex << match.Offset;

        if (number)
            msg << ": syscall number " << std::dec << *number;

        RelinkerException error(msg.str(), match.Offset);
        error.InputPath = source;
        throw error;
    });
    if (skipped != 0) {
        std::ostringstream message;
        message << "Skipped " << std::dec << skipped << " syscall byte pattern(s) without a syscall number outside the unwind functions, the first at code offset 0x" << std::hex << *firstSkipped << '\n';
        progress.Message(message.str());
    }
    progress.Report({Domain::ProgressStage::Syscalls, source, limit, limit});
    return trapped;
}

std::unique_ptr<ISyscallScanner> MakeSyscallScanner(Domain::IProgress& progress, const bool trapSdkStubs) {
    return std::make_unique<SyscallScanner>(progress, trapSdkStubs);
}

class NullSyscallScanner : public ISyscallScanner {
public:
    std::vector<std::size_t> ScanCodeSectionForSyscalls(const std::vector<std::uint8_t>&, FileByteOffset, FileByteOffset, std::string_view, const std::vector<CodeRange>&) override { return {}; }
};

std::unique_ptr<ISyscallScanner> MakeNullSyscallScanner() {
    return std::make_unique<NullSyscallScanner>();
}

}
