#ifndef CORE_SHADER_RECOMPILER_BDAABI_HPP
#define CORE_SHADER_RECOMPILER_BDAABI_HPP

#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ShaderRecompiler::BdaAbi {

inline constexpr std::uint32_t Version = 1;
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
    std::uint32_t reserved;
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
    std::uint32_t otherFaults;
};

static_assert(std::is_standard_layout_v<Header> && std::is_trivially_copyable_v<Header> && sizeof(Header) == 16);
static_assert(std::is_standard_layout_v<Range> && std::is_trivially_copyable_v<Range> && sizeof(Range) == 32);
static_assert(offsetof(Range, deviceAddress) == 16 && offsetof(Range, permissions) == 24);
static_assert(std::is_standard_layout_v<Fault> && std::is_trivially_copyable_v<Fault> && sizeof(Fault) == 32);
static_assert(offsetof(Fault, address) == 8 && offsetof(Fault, instruction) == 24 && offsetof(Fault, otherFaults) == 28);

inline constexpr std::uint32_t WrittenPageShift = 12;
inline constexpr std::uint32_t WrittenChunkShift = 17;
inline constexpr std::uint32_t WrittenChunkPages = 1u << (WrittenChunkShift - WrittenPageShift);
inline constexpr std::uint32_t WrittenPageSlots = 2048;
inline constexpr std::uint32_t WrittenPageProbes = 8;
inline constexpr std::uint64_t WrittenAddressLimit = std::uint64_t{0xffffffffu} << WrittenChunkShift;
inline constexpr std::uint32_t WrittenOverflowWord = sizeof(Fault) / sizeof(std::uint32_t);
inline constexpr std::uint32_t WrittenCountWord = WrittenOverflowWord + 1;
inline constexpr std::uint32_t WrittenSlotsWord = WrittenCountWord + 1;
inline constexpr std::uint32_t WrittenListWord = WrittenSlotsWord + 2 * WrittenPageSlots;
inline constexpr std::uint32_t SrtGuardWord = WrittenListWord + WrittenPageSlots;
inline constexpr std::uint32_t SrtGuardStageShift = 24;
inline constexpr std::size_t FaultBufferBytes = (SrtGuardWord + 1) * sizeof(std::uint32_t);
static_assert((WrittenPageSlots & (WrittenPageSlots - 1)) == 0);
static_assert(WrittenChunkPages == 32);

inline constexpr std::uint32_t WrittenChunkKey(std::uint64_t address) {
    return static_cast<std::uint32_t>(address >> WrittenChunkShift) + 1u;
}

template<typename TMark>
bool ForEachWrittenRange(const std::uint32_t* words, TMark&& mark) {
    const auto count = words[WrittenCountWord];
    if (count > WrittenPageSlots) return false;
    for (std::uint32_t claim = 0; claim < count; ++claim) {
        const auto slot = words[WrittenListWord + claim];
        if (slot >= WrittenPageSlots || words[WrittenSlotsWord + 2 * slot] == 0) return false;
    }
    for (std::uint32_t claim = 0; claim < count; ++claim) {
        const auto slot = words[WrittenListWord + claim];
        const auto key = words[WrittenSlotsWord + 2 * slot];
        auto mask = words[WrittenSlotsWord + 2 * slot + 1];
        const auto base = static_cast<std::uint64_t>(key - 1u) << WrittenChunkShift;
        while (mask != 0) {
            const auto first = static_cast<std::uint32_t>(std::countr_zero(mask));
            const auto pages = static_cast<std::uint32_t>(std::countr_one(mask >> first));
            mark(base + (static_cast<std::uint64_t>(first) << WrittenPageShift), static_cast<std::size_t>(pages) << WrittenPageShift);
            mask = pages + first >= 32u ? 0u : mask & ~((1u << (first + pages)) - 1u);
        }
    }
    return true;
}

}

#endif
