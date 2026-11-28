#ifndef CORE_SHADER_RECOMPILER_DEVICEIMAGETABLEABI_HPP
#define CORE_SHADER_RECOMPILER_DEVICEIMAGETABLEABI_HPP

#include <array>
#include <cstdint>
#include <span>

namespace ShaderRecompiler::DeviceImageTableAbi {

inline constexpr std::uint32_t Version = 2;

inline constexpr std::uint32_t DescriptorWords = 8;
inline constexpr std::uint32_t InterfaceWord = 8;
inline constexpr std::uint32_t SamplerClassWord = 10;
inline constexpr std::uint32_t SamplerWord = 11;
inline constexpr std::uint32_t SamplerWords = 4;
inline constexpr std::uint32_t KeyWords = 15;
inline constexpr std::uint32_t StateWord = 15;
inline constexpr std::uint32_t SlotWord = 16;
inline constexpr std::uint32_t ModeWord = 17;
inline constexpr std::uint32_t FaultWord = 18;
inline constexpr std::uint32_t SamplerSlotWord = 19;
inline constexpr std::uint32_t BucketWords = 20;
inline constexpr std::uint32_t BucketBytes = BucketWords * 4u;
inline constexpr std::uint32_t EmptyState = 0;
inline constexpr std::uint32_t LiveState = 1;
inline constexpr std::uint32_t ProbeSteps = 16;

inline constexpr std::uint32_t HeaderVersion = 0;
inline constexpr std::uint32_t HeaderTableLow = 1;
inline constexpr std::uint32_t HeaderTableHigh = 2;
inline constexpr std::uint32_t HeaderMask = 3;
inline constexpr std::uint32_t HeaderRingLow = 4;
inline constexpr std::uint32_t HeaderRingHigh = 5;
inline constexpr std::uint32_t HeaderRingCapacity = 6;
inline constexpr std::uint32_t HeaderSeenLow = 7;
inline constexpr std::uint32_t HeaderSeenHigh = 8;
inline constexpr std::uint32_t HeaderSeenSlots = 9;
inline constexpr std::uint32_t HeaderWords = 16;
inline constexpr std::uint32_t SeenProbeSteps = 8;
inline constexpr std::uint32_t SeenTagSeed = 0x811c9dc5u;
inline constexpr std::uint32_t SeenTagPrime = 0x01000193u;

inline constexpr std::uint32_t RingCountWord = 0;
inline constexpr std::uint32_t RingRecordsWord = 4;
inline constexpr std::uint32_t RecordWords = 16;
inline constexpr std::uint32_t RecordPcWord = 15;

inline constexpr std::uint32_t FaultFlag = 0x40000000u;

inline constexpr std::uint32_t SamplerFilterMinmax = 1u << 0u;
inline constexpr std::uint32_t SamplerOpaqueBlackBorder = 1u << 1u;
inline constexpr std::uint32_t SamplerNonSeamlessCube = 1u << 2u;
inline constexpr std::uint32_t SamplerFloatBorder = 1u << 3u;
inline constexpr std::uint32_t SamplerIntegerBorder = 1u << 4u;
inline constexpr std::uint32_t SamplerAnisoOverride = 1u << 5u;
inline constexpr std::uint32_t SamplerReduction = 1u << 6u;
inline constexpr std::uint32_t SamplerExactAtBase = 1u << 7u;
inline constexpr std::uint32_t SamplerUnnormalized = 1u << 15u;
inline constexpr std::uint32_t ModeCompareShift = 16u;
inline constexpr std::uint32_t RuntimeSampler = 1u << 31u;

inline constexpr std::uint32_t HashSeed = 0x9747b28cu;
inline constexpr std::uint32_t HashC1 = 0xcc9e2d51u;
inline constexpr std::uint32_t HashC2 = 0x1b873593u;
inline constexpr std::uint32_t HashRound = 0xe6546b64u;
inline constexpr std::uint32_t HashMix1 = 0x85ebca6bu;
inline constexpr std::uint32_t HashMix2 = 0xc2b2ae35u;

using Key = std::array<std::uint32_t, KeyWords>;

inline constexpr std::uint32_t NoMapSampler = 0xffffffffu;

struct Expectation {
    Key key{};
    std::uint32_t pc = 0;
    std::uint32_t image = 0;
    std::uint32_t mapSampler = NoMapSampler;
    bool candidate = false;

    bool operator==(const Expectation& other) const = default;
};

inline void NameInterfaces(std::span<Expectation> images, std::span<const std::uint64_t> interfaces) {
    for (auto& image : images) {
        const auto id = interfaces[image.image];
        image.key[InterfaceWord] = static_cast<std::uint32_t>(id);
        image.key[InterfaceWord + 1u] = static_cast<std::uint32_t>(id >> 32u);
    }
}

constexpr std::uint32_t RotateLeft(std::uint32_t value, std::uint32_t bits) {
    return (value << bits) | (value >> (32u - bits));
}

constexpr std::uint32_t HashWord(std::uint32_t hash, std::uint32_t word) {
    const auto mixed = RotateLeft(word * HashC1, 15u) * HashC2;
    return RotateLeft(hash ^ mixed, 13u) * 5u + HashRound;
}

constexpr std::uint32_t HashFinish(std::uint32_t hash) {
    hash ^= KeyWords * 4u;
    hash ^= hash >> 16u;
    hash *= HashMix1;
    hash ^= hash >> 13u;
    hash *= HashMix2;
    return hash ^ (hash >> 16u);
}

constexpr std::uint32_t Hash(const Key& key) {
    auto hash = HashSeed;
    for (const auto word : key) hash = HashWord(hash, word);
    return HashFinish(hash);
}

constexpr std::uint32_t SeenTag(const Key& key) {
    auto tag = SeenTagSeed;
    for (const auto word : key) tag = (tag ^ word) * SeenTagPrime;
    return tag | 1u;
}

constexpr bool NullDescriptor(std::uint32_t word0, std::uint32_t word1) {
    return word0 == 0u && (word1 & 0xffu) == 0u;
}

}

#endif
