#ifndef CORE_SHADER_RECOMPILER_BDAABI_HPP
#define CORE_SHADER_RECOMPILER_BDAABI_HPP

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ShaderRecompiler::BdaAbi {

inline constexpr std::uint32_t Version = 2;
inline constexpr std::uint32_t Read = 1;
inline constexpr std::uint32_t Write = 2;

struct Header {
    std::uint32_t version;
    std::uint32_t count;
    std::uint32_t entryBytes;
    std::uint32_t reserved;
};

struct Range {
    std::uint64_t begin;
    std::uint64_t end;
    std::uint64_t deviceAddress;
    std::uint32_t permissions;
    std::uint32_t written;
};

inline bool RangesOrdered(const Range* ranges, std::size_t count) {
    for (std::size_t index = 1; index < count; ++index) {
        if (ranges[index].begin < ranges[index - 1].begin) return false;
    }
    return true;
}

enum class FaultState : std::uint32_t { Empty, Writing, Ready };
enum class FaultReason : std::uint32_t { Unmapped = 1, Permission, Overflow, InvalidTable, InvalidRectangle, LoopLimit, Unaligned, InvalidDescriptor, ImageTable, UnlinkedCall, WindowLaneSpill };

struct Fault {
    FaultState state;
    FaultReason reason;
    std::uint64_t address;
    std::uint32_t bytes;
    std::uint32_t stage;
    std::uint32_t instruction;
    std::uint32_t claim;
    std::uint32_t discard;
    std::uint32_t reserved;
};

inline constexpr std::uint32_t FaultClaimed = 1u;
inline constexpr std::uint32_t FaultClaimOtherFaults = 2u;

inline constexpr std::uint32_t OtherFaults(const Fault& fault) {
    return (fault.claim & FaultClaimOtherFaults) != 0u ? 1u : 0u;
}

static_assert(std::is_standard_layout_v<Header> && std::is_trivially_copyable_v<Header> && sizeof(Header) == 16);
static_assert(std::is_standard_layout_v<Range> && std::is_trivially_copyable_v<Range> && sizeof(Range) == 32);
static_assert(offsetof(Range, deviceAddress) == 16 && offsetof(Range, permissions) == 24);
static_assert(std::is_standard_layout_v<Fault> && std::is_trivially_copyable_v<Fault> && sizeof(Fault) == 40);
static_assert(offsetof(Fault, address) == 8 && offsetof(Fault, instruction) == 24 && offsetof(Fault, claim) == 28 && offsetof(Fault, discard) == 32);

inline constexpr std::uint32_t WrittenPageShift = 12;
inline constexpr std::uint32_t WrittenDirtyWord = sizeof(Fault) / sizeof(std::uint32_t);
inline constexpr std::uint32_t WrittenPoolWord = WrittenDirtyWord + 1;
inline constexpr std::uint32_t WrittenPoolWords = 6145;
inline constexpr std::uint32_t SrtGuardWord = WrittenPoolWord + WrittenPoolWords;
inline constexpr std::uint32_t SrtGuardStageShift = 24;
inline constexpr std::size_t FaultBufferBytes = (SrtGuardWord + 1) * sizeof(std::uint32_t);
inline constexpr std::uint32_t WrittenBlockBits = 16;
inline constexpr std::uint32_t WrittenMaxShift = 47;
static_assert(SrtGuardWord < (1u << WrittenBlockBits));

struct WrittenBlock {
    std::uint32_t word;
    std::uint32_t shift;
};

inline constexpr std::uint32_t EncodeWrittenBlock(WrittenBlock block) {
    return block.word | (block.shift << WrittenBlockBits);
}

inline constexpr WrittenBlock DecodeWrittenBlock(std::uint32_t written) {
    return {written & ((1u << WrittenBlockBits) - 1u), written >> WrittenBlockBits};
}

inline constexpr std::uint64_t WrittenGranules(std::uint64_t begin, std::uint64_t end, std::uint32_t shift) {
    return ((end - 1u) >> shift) - (begin >> shift) + 1u;
}

inline constexpr std::uint64_t WrittenBlockWords(std::uint64_t begin, std::uint64_t end, std::uint32_t shift) {
    return (WrittenGranules(begin, end, shift) + 31u) / 32u;
}

struct WrittenAssignment {
    std::uint32_t writable = 0;
    std::uint32_t words = 0;
    std::uint32_t coarse = 0;
    std::uint32_t maxShift = 0;
};

inline bool AssignWrittenBlocks(Range* ranges, std::size_t count, WrittenAssignment* assignment = nullptr) {
    const auto shiftWithin = [](const Range& range, std::uint64_t cap) {
        auto shift = WrittenPageShift;
        while (shift < WrittenMaxShift && WrittenBlockWords(range.begin, range.end, shift) > cap) ++shift;
        return shift;
    };
    const auto total = [&](std::uint64_t cap) {
        std::uint64_t words = 0;
        for (std::size_t index = 0; index < count; ++index) {
            const auto& range = ranges[index];
            if ((range.permissions & Write) == 0 || range.begin >= range.end) continue;
            words += WrittenBlockWords(range.begin, range.end, shiftWithin(range, cap));
        }
        return words;
    };
    std::uint64_t low = 1;
    std::uint64_t high = WrittenPoolWords;
    if (total(high) <= WrittenPoolWords) low = high;
    else if (total(low) > WrittenPoolWords) return false;
    while (low < high) {
        const auto middle = low + (high - low + 1) / 2;
        if (total(middle) <= WrittenPoolWords) low = middle;
        else high = middle - 1;
    }
    WrittenAssignment used{};
    auto word = WrittenPoolWord;
    for (std::size_t index = 0; index < count; ++index) {
        auto& range = ranges[index];
        range.written = 0;
        if ((range.permissions & Write) == 0 || range.begin >= range.end) continue;
        const auto shift = shiftWithin(range, low);
        range.written = EncodeWrittenBlock({word, shift});
        word += static_cast<std::uint32_t>(WrittenBlockWords(range.begin, range.end, shift));
        ++used.writable;
        if (shift > WrittenPageShift) ++used.coarse;
        used.maxShift = std::max(used.maxShift, shift);
    }
    used.words = word - WrittenPoolWord;
    if (assignment != nullptr) *assignment = used;
    return true;
}

template<typename TMark>
bool ForEachWrittenRange(const std::uint32_t* words, std::size_t wordCount, const Range& range, TMark&& mark) {
    if (range.written == 0) return true;
    const auto block = DecodeWrittenBlock(range.written);
    if (range.begin >= range.end || block.shift < WrittenPageShift || block.shift > WrittenMaxShift || block.word < WrittenPoolWord) return false;
    const auto count = WrittenBlockWords(range.begin, range.end, block.shift);
    if (block.word + count > SrtGuardWord || block.word + count > wordCount) return false;
    const auto granules = WrittenGranules(range.begin, range.end, block.shift);
    const auto first = range.begin >> block.shift;
    std::uint64_t run = 0;
    std::uint64_t length = 0;
    const auto flush = [&] {
        if (length == 0) return;
        const auto begin = std::max(range.begin, (first + run) << block.shift);
        const auto stop = first + run + length;
        const auto end = stop > (~std::uint64_t{0} >> block.shift) ? range.end : std::min(range.end, stop << block.shift);
        mark(begin, static_cast<std::size_t>(end - begin));
        length = 0;
    };
    for (std::uint64_t word = 0; word < count; ++word) {
        auto bits = words[block.word + word];
        if (word + 1 == count && granules % 32u != 0) bits &= (1u << (granules % 32u)) - 1u;
        if (bits == 0) {
            flush();
            continue;
        }
        for (std::uint32_t bit = 0; bit < 32; ++bit) {
            const auto granule = word * 32u + bit;
            if ((bits >> bit) & 1u) {
                if (length == 0) run = granule;
                ++length;
            } else {
                flush();
            }
        }
    }
    flush();
    return true;
}
}

#endif
