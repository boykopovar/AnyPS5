#include <codegen/IAmd64OnlyConverter.hpp>
#include <codegen/CodegenTypes.hpp>
#include <codegen/CodegenException.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <codegen/x86/DecodedInstruction.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>
#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace Codegen {

namespace {

template<typename TOperation>
auto _atFileOffset(const Domain::FileByteOffset base, const TOperation& operation) {
    try {
        return operation();
    } catch (const CodegenException& e) {
        throw CodegenException(e.what(), base + e.FailureOffset);
    }
}

constexpr std::int64_t kShortJumpSize = 2;
constexpr std::uint32_t kLoadSegment = 1;
constexpr std::uint32_t kExecutableSegment = 1;
constexpr std::uint8_t kLea = 0x8D;
constexpr std::uint64_t kIslandSearchBytes = 0x100;
constexpr std::size_t kFlagScanBytes = 0x80;
constexpr std::size_t kMaxX64InstructionLength = 15;
constexpr std::size_t kLocalWindowRadius = 512;
constexpr std::size_t kNoMatchIndex = std::numeric_limits<std::size_t>::max();

struct ByteWindow {
    std::size_t Begin;
    std::size_t End;
};

struct LeaTableCandidate {
    std::uint64_t Table;
    bool AddressSize32;
};

struct SegmentAnalysis {
    std::vector<std::uint64_t> BranchTargets;
    std::vector<InstructionMatch> LocalMatches;
    std::set<std::size_t> NonEntryWindowStarts;
};

bool _blocksFollowingInfo(const DecodedInstructionInfo& info) {
    return info.FlowKind != ControlFlowKind::Sequential && info.FlowKind != ControlFlowKind::ConditionalBranch;
}

bool _paddingInfo(const std::span<const std::uint8_t> seg, const std::size_t offset, const DecodedInstructionInfo& info) {
    if (info.IsTwoByteOpcode)
        return info.Opcode == X64OpcodeConstants::TwoByteNopModRm;
    if (info.Opcode != X64OpcodeConstants::OneByteNop)
        return info.Length == 1 && seg[offset] == X64OpcodeConstants::OneByteInt3;
    const auto prefixes = seg.subspan(offset, info.OpcodeOffset);
    return (info.RexPrefix & X64OpcodeConstants::RexBBit) == 0 &&
           std::find(prefixes.begin(), prefixes.end(), X64OpcodeConstants::PrefixRep) == prefixes.end();
}

struct Placement {
    std::vector<TrampolineSite> Sites;
    std::vector<std::size_t> Claimed;
    std::vector<std::size_t> Consumed;
    std::size_t ReplacementLength = 0;
};

class WindowPlacer {
public:
    WindowPlacer(
        const std::span<std::uint8_t> seg,
        const std::vector<InstructionMatch>& matches,
        const Domain::ProgramHeader& ph,
        const std::vector<std::uint64_t>& branchTargets,
        const std::set<std::size_t>& pending,
        const std::set<std::size_t>& claimed,
        const std::set<std::size_t>& nonEntryWindowStarts,
        const IAmd64OnlyInstructionMatcher& matcher,
        const bool keepSegmentAccesses
    ) : _seg(seg), _matches(matches), _ph(ph), _branchTargets(branchTargets), _pending(pending), _claimed(claimed), _nonEntryWindowStarts(nonEntryWindowStarts), _matcher(matcher), _keepSegmentAccesses(keepSegmentAccesses) {}

    [[nodiscard]] std::optional<Placement> Window(const std::size_t index) const {
        std::optional<Placement> fallback;
        for (const bool avoidSegment : {true, false}) {
            if (!avoidSegment && _keepSegmentAccesses)
                break;
            if (index + 1 < _matches.size() && (_pending.contains(index + 1) || _movable(index + 1, true, avoidSegment))) {
                if (auto placement = _relocate(index, index, index + 1))
                    return placement;
            }
            if (index > 0 && _movable(index - 1, false, avoidSegment)) {
                if (auto placement = _relocate(index, index - 1, index))
                    return placement;
            }
            if (!fallback && index + 1 < _matches.size() && _pending.contains(index + 1))
                fallback = _relocate(index, index, index + 1, true);
        }
        return fallback;
    }

    [[nodiscard]] std::optional<Placement> Island(const std::size_t index) {
        const auto store = _address(index);
        for (std::size_t distance = 1;; ++distance) {
            bool inReach = false;
            for (const bool before : {true, false}) {
                if (before ? distance > index : index + distance >= _matches.size())
                    continue;
                const auto start = before ? index - distance : index + distance;
                if (_address(start) + kIslandSearchBytes < store || _address(start) > store + kIslandSearchBytes)
                    continue;
                inReach = true;
                if (auto placement = _islandAt(index, start))
                    return placement;
            }
            if (!inReach)
                return std::nullopt;
        }
    }

private:
    std::span<std::uint8_t> _seg;
    const std::vector<InstructionMatch>& _matches;
    const Domain::ProgramHeader& _ph;
    const std::vector<std::uint64_t>& _branchTargets;
    const std::set<std::size_t>& _pending;
    const std::set<std::size_t>& _claimed;
    const std::set<std::size_t>& _nonEntryWindowStarts;
    const IAmd64OnlyInstructionMatcher& _matcher;
    const bool _keepSegmentAccesses;
    X64InstructionDecoder _decoder;

    [[nodiscard]] std::uint64_t _address(const std::size_t index) const { return _ph.MappedAddress + _matches[index].Offset; }
    [[nodiscard]] std::uint64_t _end(const std::size_t index) const { return _address(index) + _matches[index].Length; }
    [[nodiscard]] static std::int64_t _distance(const std::uint64_t to, const std::uint64_t from) { return static_cast<std::int64_t>(to - from); }
    [[nodiscard]] std::span<const std::uint8_t> _bytes(const std::size_t index) const { return {_seg.data() + _matches[index].Offset, _matches[index].Length}; }
    [[nodiscard]] DecodedInstructionInfo _info(const std::size_t index) const { return _decoder.DecodeInstruction(_seg.data() + _matches[index].Offset, _matches[index].Length); }

    [[nodiscard]] bool _entered(const std::uint64_t start, const std::uint64_t end) const {
        const auto hit = std::upper_bound(_branchTargets.begin(), _branchTargets.end(), start);
        return hit != _branchTargets.end() && *hit < end;
    }

    [[nodiscard]] bool _int3(const std::size_t index) const {
        const auto bytes = _bytes(index);
        return bytes.size() == 1 && bytes[0] == X64OpcodeConstants::OneByteInt3;
    }

    [[nodiscard]] bool _padding(const std::size_t index) const {
        return _paddingInfo(_seg, static_cast<std::size_t>(_matches[index].Offset), _info(index));
    }

    [[nodiscard]] bool _mayBeEntered(const std::size_t index) const {
        for (auto previous = index; previous > 0; --previous) {
            if (_end(previous - 1) != _address(previous))
                return !_nonEntryWindowStarts.contains(previous);
            if (_blocksFollowing(previous - 1))
                return true;
            if (!_padding(previous - 1))
                return false;
        }
        return !_nonEntryWindowStarts.contains(0);
    }

    [[nodiscard]] bool _enteredWithin(const std::size_t first, const std::size_t last) const {
        if (_entered(_address(first), _end(last)))
            return true;
        for (auto index = first + 1; index <= last; ++index) {
            if (!_int3(index) && _mayBeEntered(index))
                return true;
        }
        return false;
    }

    [[nodiscard]] bool _blocksFollowing(const std::size_t index) const {
        const auto kind = _info(index).FlowKind;
        return kind != ControlFlowKind::Sequential && kind != ControlFlowKind::ConditionalBranch;
    }

    [[nodiscard]] bool _movable(const std::size_t index, const bool last, const bool avoidSegment = false) const {
        return !_claimed.contains(index) && !_pending.contains(index) && (last || !_blocksFollowing(index)) && !((avoidSegment || _keepSegmentAccesses) && _info(index).SegmentPrefix != 0);
    }

    [[nodiscard]] TrampolineSite _site(const std::uint64_t start, const std::size_t length, LoweredBody body) const {
        const auto offset = static_cast<std::size_t>(start - _ph.MappedAddress);
        return {
            static_cast<Domain::FileByteOffset>(_ph.Offset + offset),
            static_cast<Domain::VirtualAddress>(start),
            length,
            std::vector<std::uint8_t>(_seg.begin() + static_cast<std::ptrdiff_t>(offset), _seg.begin() + static_cast<std::ptrdiff_t>(offset + length)),
            std::move(body.Bytes),
            body.ReturnBranchOffset,
            std::move(body.Relocations)
        };
    }

    [[nodiscard]] std::optional<Placement> _relocate(const std::size_t index, const std::size_t first, const std::size_t last, const bool allowInPlace = false) const {
        if (_enteredWithin(first, last))
            return std::nullopt;
        StubBodyBuilder body;
        Placement placement;
        try {
            for (auto k = first; k <= last; ++k) {
                if (_pending.contains(k)) {
                    _matcher.EmitOutOfLine(body, _bytes(k), allowInPlace);
                    if (k != index)
                        placement.Consumed.push_back(k);
                } else {
                    body.MoveControlFlow(_bytes(k), _info(k));
                }
                placement.Claimed.push_back(k);
            }
        } catch (const CodegenException&) {
            return std::nullopt;
        }
        placement.Sites.push_back(_site(_address(first), static_cast<std::size_t>(_end(last) - _address(first)), body.Finish()));
        placement.ReplacementLength = placement.Sites.back().Body.size();
        return placement;
    }

    [[nodiscard]] std::optional<Placement> _islandAt(const std::size_t index, const std::size_t start) {
        using Amd64OnlySubstitutionTable::kJmpRel32;
        std::size_t head = 0;
        for (auto last = start; last < _matches.size(); ++last) {
            if (!_movable(last, true) || _enteredWithin(start, last))
                return std::nullopt;
            if (head == 0 && _end(last) - _address(start) >= kJmpRel32.Size)
                head = static_cast<std::size_t>(_end(last) - _address(start));
            const auto island = _address(start) + head;
            const auto reach = _distance(island, _address(index) + kShortJumpSize);
            if (head != 0 && (reach < std::numeric_limits<std::int8_t>::min() || reach > std::numeric_limits<std::int8_t>::max()))
                return std::nullopt;
            if (head != 0 && _end(last) >= island + kJmpRel32.Size) {
                try {
                    return _buildIsland(index, start, last, head);
                } catch (const CodegenException&) {
                    return std::nullopt;
                }
            }
            if (_blocksFollowing(last))
                return std::nullopt;
        }
        return std::nullopt;
    }

    [[nodiscard]] Placement _buildIsland(const std::size_t index, const std::size_t start, const std::size_t last, const std::size_t head) {
        using Amd64OnlySubstitutionTable::kJmpRel32;
        const auto windowStart = _address(start);
        const auto windowEnd = _end(last);
        const auto island = windowStart + head;
        const auto store = _address(index);
        Placement placement;

        StubBodyBuilder moved;
        for (auto k = start; k <= last; ++k) {
            moved.MoveControlFlow(_bytes(k), _info(k));
            placement.Claimed.push_back(k);
        }
        if (!_blocksFollowing(last))
            moved.JumpTo(static_cast<std::int64_t>(windowEnd - windowStart));
        placement.Sites.push_back(_site(windowStart, head, moved.Finish()));

        StubBodyBuilder lowered;
        lowered.Seek(_distance(store, island));
        _matcher.EmitOutOfLine(lowered, _bytes(index));
        lowered.JumpTo(_distance(_end(index), island));
        placement.Sites.push_back(_site(island, kJmpRel32.Size, lowered.Finish()));
        placement.ReplacementLength = placement.Sites.back().Body.size();
        placement.Claimed.push_back(index);

        const auto offset = _matches[index].Offset;
        std::fill_n(_seg.begin() + static_cast<std::ptrdiff_t>(offset), _matches[index].Length, Amd64OnlySubstitutionTable::kTrapFill);
        _seg[offset] = X64OpcodeConstants::OneByteJmpRel8;
        _seg[offset + 1] = static_cast<std::uint8_t>(static_cast<std::int8_t>(_distance(island, store + kShortJumpSize)));
        return placement;
    }
};

class Amd64OnlyConverter : public IAmd64OnlyConverter {
public:
    Amd64OnlyConverter(const Amd64OnlyTarget target, std::function<void(std::uint64_t, std::uint64_t)> progress, const bool keepSegmentAccesses) : _matcher(MakeAmd64OnlyInstructionMatcher(target)), progress(std::move(progress)), _keepSegmentAccesses(keepSegmentAccesses) {}

    [[nodiscard]] ConvertResult Convert(
        std::vector<std::uint8_t> fileBytes,
        const std::vector<Domain::ProgramHeader>& segments
    ) const override;

private:
    struct Pending {
        std::size_t Index;
        InstructionMatch Instruction;
        Amd64OnlyMatch Substitution;
    };

    std::unique_ptr<IAmd64OnlyInstructionMatcher> _matcher;
    std::function<void(std::uint64_t, std::uint64_t)> progress;
    bool _keepSegmentAccesses = false;

    void _convertSegment(
        std::vector<std::uint8_t>& fileBytes,
        const Domain::ProgramHeader& ph,
        const std::vector<Domain::ProgramHeader>& segments,
        ConvertResult& result,
        std::uint64_t completed,
        std::uint64_t total
    ) const;

    [[nodiscard]] static SegmentAnalysis _analyzeTrampolineSegment(
        std::span<const std::uint8_t> seg,
        std::size_t decodedLimit,
        const Domain::ProgramHeader& ph,
        const std::vector<std::uint8_t>& fileBytes,
        const std::vector<Domain::ProgramHeader>& segments,
        const std::vector<ByteWindow>& windows
    );
};

SegmentAnalysis Amd64OnlyConverter::_analyzeTrampolineSegment(
    const std::span<const std::uint8_t> seg,
    const std::size_t decodedLimit,
    const Domain::ProgramHeader& ph,
    const std::vector<std::uint8_t>& fileBytes,
    const std::vector<Domain::ProgramHeader>& segments,
    const std::vector<ByteWindow>& windows
) {
    SegmentAnalysis analysis;
    std::vector<std::uint64_t> instrStarts((seg.size() + 63) / 64, 0);
    const auto setStart = [&](const std::size_t offset) {
        instrStarts[offset >> 6] |= (std::uint64_t{1} << (offset & 63));
    };
    const auto hasStart = [&](const std::size_t offset) {
        return (instrStarts[offset >> 6] & (std::uint64_t{1} << (offset & 63))) != 0;
    };

    const X64InstructionDecoder decoder;
    std::vector<std::uint64_t> rawBranchTargets;
    std::vector<LeaTableCandidate> leaTables;
    const bool collectWindows = !windows.empty();
    std::size_t winIdx = 0;
    bool mayBeEntered = true;

    for (std::size_t off = 0; off < decodedLimit;) {
        const auto info = decoder.DecodeInstruction(seg.data() + off, seg.size() - off);
        setStart(off);

        if (collectWindows) {
            while (winIdx < windows.size() && windows[winIdx].End <= off)
                ++winIdx;
            if (winIdx < windows.size() && off >= windows[winIdx].Begin) {
                if (analysis.LocalMatches.empty() || analysis.LocalMatches.back().Offset + analysis.LocalMatches.back().Length != off) {
                    if (!mayBeEntered)
                        analysis.NonEntryWindowStarts.insert(analysis.LocalMatches.size());
                }
                analysis.LocalMatches.push_back({off, info.Length});
            }
            if (_blocksFollowingInfo(info))
                mayBeEntered = true;
            else if (!_paddingInfo(seg, off, info))
                mayBeEntered = false;
        }

        const auto next = ph.MappedAddress + off + info.Length;
        if (info.HasBranchTarget && !info.HasRipRelativeDisp) {
            const auto displacement = static_cast<std::uint64_t>(info.BranchDisp);
            const auto target = next + displacement;
            const auto wrapped = info.BranchDisp < 0 ? target > next : target < next;
            if (!wrapped)
                rawBranchTargets.push_back(target);
        } else if (info.HasRipRelativeDisp && info.Opcode == kLea) {
            std::int32_t displacement;
            std::memcpy(&displacement, seg.data() + off + info.RipRelativeDispOffset, sizeof(displacement));
            const auto prefixes = seg.subspan(off, info.OpcodeOffset);
            const bool tableAddress32 = info.AddressSize32 || ((info.RexPrefix & X64OpcodeConstants::RexWBit) == 0 &&
                std::find(prefixes.begin(), prefixes.end(), X64OpcodeConstants::PrefixOperandSize) == prefixes.end());
            bool overflow = false;
            if (!tableAddress32) {
                if (displacement < 0 && next < static_cast<std::uint64_t>(-static_cast<std::int64_t>(displacement)))
                    overflow = true;
                if (displacement > 0 && std::numeric_limits<std::uint64_t>::max() - next < static_cast<std::uint64_t>(displacement))
                    overflow = true;
            }
            if (!overflow) {
                auto table = next + static_cast<std::uint64_t>(static_cast<std::int64_t>(displacement));
                if (tableAddress32)
                    table = static_cast<std::uint32_t>(table);
                leaTables.push_back({table, info.AddressSize32});
            }
        }
        off += info.Length;
    }

    const auto startsInstruction = [&](const std::uint64_t address) {
        if (address < ph.MappedAddress)
            return false;
        const auto offset = address - ph.MappedAddress;
        return offset < seg.size() && hasStart(static_cast<std::size_t>(offset));
    };
    const auto pastPrefixes = [&](const std::uint64_t address) {
        if (address < ph.MappedAddress)
            return false;
        const auto offset = address - ph.MappedAddress;
        if (offset >= seg.size() || hasStart(static_cast<std::size_t>(offset)))
            return false;
        const auto offSize = static_cast<std::size_t>(offset);
        const auto back = std::min<std::size_t>(offSize, kMaxX64InstructionLength - 1);
        for (std::size_t delta = 1; delta <= back; ++delta) {
            const auto prev = offSize - delta;
            if (!hasStart(prev))
                continue;
            const auto prevLen = decoder.Decode(seg.data() + prev, seg.size() - prev);
            return delta < prevLen && delta > DecodedInstruction{seg.data() + prev, prevLen}.OpcodeOffset();
        }
        return false;
    };

    analysis.BranchTargets.reserve(rawBranchTargets.size());
    for (const auto target : rawBranchTargets) {
        if (!pastPrefixes(target))
            analysis.BranchTargets.push_back(target);
    }

    for (const auto& candidate : leaTables) {
        for (const auto& data : segments) {
            if (data.Type != kLoadSegment || candidate.Table < data.MappedAddress || candidate.Table - data.MappedAddress >= data.FileSize)
                continue;
            if (data.Offset > fileBytes.size() || data.FileSize > fileBytes.size() - data.Offset)
                throw CodegenException("Segment exceeds the file", data.Offset);
            for (auto entry = candidate.Table; entry - data.MappedAddress + sizeof(std::int32_t) <= data.FileSize; entry += sizeof(std::int32_t)) {
                std::int32_t relative;
                std::memcpy(&relative, fileBytes.data() + data.Offset + (entry - data.MappedAddress), sizeof(relative));
                if (!candidate.AddressSize32) {
                    if (relative < 0 && candidate.Table < static_cast<std::uint64_t>(-static_cast<std::int64_t>(relative)))
                        break;
                    if (relative > 0 && std::numeric_limits<std::uint64_t>::max() - candidate.Table < static_cast<std::uint64_t>(relative))
                        break;
                }
                const auto target = candidate.AddressSize32
                    ? static_cast<std::uint64_t>(static_cast<std::uint32_t>(candidate.Table + static_cast<std::uint64_t>(static_cast<std::int64_t>(relative))))
                    : candidate.Table + static_cast<std::uint64_t>(static_cast<std::int64_t>(relative));
                if (!startsInstruction(target))
                    break;
                analysis.BranchTargets.push_back(target);
            }
        }
    }

    std::sort(analysis.BranchTargets.begin(), analysis.BranchTargets.end());
    analysis.BranchTargets.erase(std::unique(analysis.BranchTargets.begin(), analysis.BranchTargets.end()), analysis.BranchTargets.end());
    return analysis;
}

void Amd64OnlyConverter::_convertSegment(
    std::vector<std::uint8_t>& fileBytes,
    const Domain::ProgramHeader& ph,
    const std::vector<Domain::ProgramHeader>& segments,
    ConvertResult& result,
    std::uint64_t completed,
    std::uint64_t total
) const {
    if (ph.Offset > fileBytes.size() || ph.FileSize > fileBytes.size() - ph.Offset)
        throw CodegenException("Code segment exceeds the file", ph.Offset);
    const auto segOffset = static_cast<std::size_t>(ph.Offset);
    const auto segSize = static_cast<std::size_t>(ph.FileSize);
    const std::span<std::uint8_t> seg(fileBytes.data() + segOffset, segSize);

    const X64InstructionDecoder decoder;
    std::vector<Pending> pending;
    std::size_t decodedLimit = 0;
    std::size_t lastReport = 0;
    if (progress) progress(completed, total);
    while (decodedLimit < segSize) {
        const std::uint8_t* cursor = seg.data() + decodedLimit;
        const std::size_t available = segSize - decodedLimit;
        std::size_t length = 0;
        try {
            length = decoder.Decode(cursor, available);
        } catch (const CodegenException& e) {
            if (std::all_of(cursor, cursor + available, [](const std::uint8_t b) { return b == 0; }))
                break;
            throw CodegenException(std::string("Cannot decode instruction: ") + e.what(), ph.Offset + decodedLimit);
        }
        const auto next = decodedLimit + length;
        const std::span<const std::uint8_t> following(seg.data() + next, next < segSize ? std::min<std::size_t>(segSize - next, kFlagScanBytes) : 0);
        auto substitution = _atFileOffset(ph.Offset + decodedLimit, [&] { return _matcher->Match(cursor, length, {}, following); });
        if (substitution.has_value())
            pending.push_back({kNoMatchIndex, InstructionMatch{decodedLimit, length}, std::move(*substitution)});
        decodedLimit = next;
        if (progress && decodedLimit - lastReport >= 65536) {
            progress(completed + std::uint64_t{2} * decodedLimit, total);
            lastReport = decodedLimit;
        }
    }
    if (progress) progress(completed + std::uint64_t{2} * segSize, total);
    if (pending.empty()) {
        if (progress) progress(completed + std::uint64_t{3} * segSize, total);
        return;
    }

    const bool needsBranchTargets = std::any_of(pending.begin(), pending.end(), [](const Pending& item) {
        return item.Substitution.Lowering == Amd64OnlyLowering::Trampoline;
    });

    std::vector<ByteWindow> windows;
    if (needsBranchTargets) {
        for (const auto& item : pending) {
            if (item.Substitution.Lowering != Amd64OnlyLowering::Trampoline || item.Instruction.Length >= Amd64OnlySubstitutionTable::kJmpRel32.Size)
                continue;
            const auto off = static_cast<std::size_t>(item.Instruction.Offset);
            const auto begin = off > kLocalWindowRadius ? off - kLocalWindowRadius : 0;
            const auto end = std::min<std::size_t>(decodedLimit, off + kLocalWindowRadius);
            if (!windows.empty() && begin <= windows.back().End)
                windows.back().End = std::max(windows.back().End, end);
            else
                windows.push_back({begin, end});
        }
    }

    const auto analysis = needsBranchTargets
        ? _analyzeTrampolineSegment(seg, decodedLimit, ph, fileBytes, segments, windows)
        : SegmentAnalysis{};
    const auto& branchTargets = analysis.BranchTargets;
    const auto& matches = analysis.LocalMatches;

    std::set<std::size_t> pendingIndices;
    if (!matches.empty()) {
        std::size_t cursor = 0;
        for (auto& item : pending) {
            while (cursor < matches.size() && matches[cursor].Offset < item.Instruction.Offset)
                ++cursor;
            if (cursor < matches.size() && matches[cursor].Offset == item.Instruction.Offset) {
                item.Index = cursor;
                pendingIndices.insert(cursor);
            }
        }
    }
    std::set<std::size_t> claimed;
    std::set<std::size_t> consumed;

    std::size_t processed = 0;
    for (const auto& item : pending) {
        if (progress && processed != 0 && processed % 256 == 0) progress(completed + std::uint64_t{2} * segSize + item.Instruction.Offset, total);
        ++processed;
        if (item.Index != kNoMatchIndex && consumed.contains(item.Index))
            continue;
        const auto& match = item.Instruction;
        const auto& substitution = item.Substitution;
        const auto fileOffset = static_cast<Domain::FileByteOffset>(ph.Offset + match.Offset);
        const auto address = static_cast<Domain::VirtualAddress>(ph.MappedAddress + match.Offset);
        std::size_t replacementLength = 0;

        switch (substitution.Lowering) {
        case Amd64OnlyLowering::InPlace: {
            if (substitution.ReplacementBytes.size() != match.Length)
                throw CodegenException("Intel substitution changes the instruction length", fileOffset);
            std::copy(substitution.ReplacementBytes.begin(), substitution.ReplacementBytes.end(), seg.begin() + static_cast<std::ptrdiff_t>(match.Offset));
            replacementLength = substitution.ReplacementBytes.size();
            ++result.ReplacedCount;
            break;
        }
        case Amd64OnlyLowering::Trampoline: {
            if (substitution.RelocatableWindow && match.Length < Amd64OnlySubstitutionTable::kJmpRel32.Size) {
                WindowPlacer placer(seg, matches, ph, branchTargets, pendingIndices, claimed, analysis.NonEntryWindowStarts, *_matcher, _keepSegmentAccesses);
                auto placement = placer.Window(item.Index);
                if (!placement)
                    placement = placer.Island(item.Index);
                if (!placement)
                    throw CodegenException("No room for a jump to the stub of " + substitution.InstructionName + " within its neighbours or a nearby island", fileOffset);
                for (auto& site : placement->Sites)
                    result.Trampolines.push_back(std::move(site));
                claimed.insert(placement->Claimed.begin(), placement->Claimed.end());
                consumed.insert(placement->Consumed.begin(), placement->Consumed.end());
                replacementLength = placement->ReplacementLength;
                break;
            }
            std::vector<std::size_t> taken;
            try {
                std::size_t siteLength = match.Length;
                auto stub = substitution;
                if (siteLength < Amd64OnlySubstitutionTable::kJmpRel32.Size) {
                    std::vector<std::span<const std::uint8_t>> sequence{std::span<const std::uint8_t>(seg.data() + match.Offset, match.Length)};
                    std::size_t trailingBytes = 0;
                    for (auto next = item.Index + 1; siteLength < Amd64OnlySubstitutionTable::kJmpRel32.Size; ++next) {
                        if (next >= matches.size() || matches[next].Offset != match.Offset + siteLength)
                            throw CodegenException("AMD-only instruction too short for a jump and not followed by an instruction", fileOffset);
                        if (claimed.contains(next))
                            throw CodegenException("AMD-only instruction too short for a jump is followed by an instruction another stub already moved", ph.Offset + matches[next].Offset);
                        const auto& following = matches[next];
                        const std::span<const std::uint8_t> bytes(seg.data() + following.Offset, following.Length);
                        const auto info = decoder.DecodeInstruction(bytes.data(), bytes.size());
                        const bool amdOnly = _matcher->Match(bytes.data(), bytes.size()).has_value();
                        if (amdOnly && trailingBytes == 0) {
                            sequence.push_back(bytes);
                            taken.push_back(next);
                        } else if (amdOnly || info.FlowKind != ControlFlowKind::Sequential || info.HasBranchTarget || (_keepSegmentAccesses && info.SegmentPrefix != 0)) {
                            throw CodegenException("AMD-only instruction too short for a jump is followed by an instruction that cannot move", ph.Offset + following.Offset);
                        } else {
                            trailingBytes += following.Length;
                        }
                        siteLength += following.Length;
                    }
                    const std::span<const std::uint8_t> trailing(seg.data() + match.Offset + siteLength - trailingBytes, trailingBytes);
                    auto relocated = _atFileOffset(fileOffset, [&] { return _matcher->MatchSequence(sequence, trailing); });
                    if (!relocated.has_value() || relocated->Lowering != Amd64OnlyLowering::Trampoline)
                        throw CodegenException("AMD-only instruction sequence has no out-of-line lowering", fileOffset);
                    stub = std::move(*relocated);
                }
                const auto hit = std::upper_bound(branchTargets.begin(), branchTargets.end(), address);
                if (hit != branchTargets.end() && *hit < address + siteLength)
                    throw CodegenException("Branch enters an AMD-only instruction", ph.Offset + (*hit - ph.MappedAddress));
                const auto begin = seg.begin() + static_cast<std::ptrdiff_t>(match.Offset);
                result.Trampolines.push_back({
                    fileOffset,
                    address,
                    siteLength,
                    std::vector<std::uint8_t>(begin, begin + static_cast<std::ptrdiff_t>(siteLength)),
                    stub.StubBody,
                    stub.ReturnBranchOffset,
                    stub.Relocations
                });
                replacementLength = stub.StubBody.size();
                consumed.insert(taken.begin(), taken.end());
                if (item.Index != kNoMatchIndex) {
                    for (auto k = item.Index; k < matches.size() && matches[k].Offset < match.Offset + siteLength; ++k)
                        claimed.insert(k);
                }
            } catch (const CodegenException&) {
                if (!substitution.Optional)
                    throw;
                ++result.KeptCount;
                result.Reports.push_back({substitution.InstructionName, fileOffset, match.Length, 0, Amd64OnlyLowering::Kept});
                continue;
            }
            break;
        }
        case Amd64OnlyLowering::Kept:
        case Amd64OnlyLowering::Unsupported:
            throw CodegenException("AMD-only instruction without Intel lowering: " + substitution.InstructionName, fileOffset);
        }

        result.Reports.push_back({substitution.InstructionName, fileOffset, match.Length, replacementLength, substitution.Lowering});
    }

    if (progress) progress(completed + std::uint64_t{3} * segSize, total);
}

ConvertResult Amd64OnlyConverter::Convert(
    std::vector<std::uint8_t> fileBytes,
    const std::vector<Domain::ProgramHeader>& segments
) const {
    ConvertResult result{};
    std::uint64_t total = 0;
    for (const auto& ph : segments) {
        if (ph.Type != kLoadSegment || (ph.Flags & kExecutableSegment) == 0) continue;
        if (ph.FileSize > (std::numeric_limits<std::uint64_t>::max() - total) / 3) throw CodegenException("Instruction conversion progress size overflow");
        total += 3 * ph.FileSize;
    }
    if (progress) progress(0, total);
    std::uint64_t completed = 0;
    for (const auto& ph : segments) {
        if (ph.Type != kLoadSegment || (ph.Flags & kExecutableSegment) == 0) continue;
        _convertSegment(fileBytes, ph, segments, result, completed, total);
        completed += 3 * ph.FileSize;
    }
    if (progress) progress(total, total);
    result.Bytes = std::move(fileBytes);
    return result;
}

}

std::unique_ptr<IAmd64OnlyConverter> MakeAmd64OnlyConverter(const Amd64OnlyTarget target, std::function<void(std::uint64_t, std::uint64_t)> progress, const bool keepSegmentAccesses) {
    return std::make_unique<Amd64OnlyConverter>(target, std::move(progress), keepSegmentAccesses);
}

}
