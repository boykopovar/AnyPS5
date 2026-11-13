#ifndef CORE_SHADER_RECOMPILER_BINDLESSIMAGEABI_HPP
#define CORE_SHADER_RECOMPILER_BINDLESSIMAGEABI_HPP

#include "DeviceImageTableAbi.hpp"
#include <array>
#include <compare>
#include <cstdint>
#include <span>
#include <vector>

namespace ShaderRecompiler::BindlessImageAbi {

namespace Table = DeviceImageTableAbi;

inline constexpr std::uint32_t HeapDescriptorSet = 3u;
inline constexpr std::uint32_t SamplerBinding = 0u;
inline constexpr std::uint32_t SampledImageBinding = 1u;
inline constexpr std::uint32_t StorageImageBinding = 2u;
inline constexpr std::uint32_t SampledImageSlots = 1u << 18u;
inline constexpr std::uint32_t StorageImageSlots = 1u << 16u;
inline constexpr std::uint32_t NullSlot = 0u;

inline constexpr std::uint32_t SlotBits = 20u;
inline constexpr std::uint32_t SlotMask = (1u << SlotBits) - 1u;
inline constexpr std::uint32_t GenerationBits = 32u - SlotBits;
inline constexpr std::uint32_t GenerationMask = (1u << GenerationBits) - 1u;
static_assert(SampledImageSlots <= SlotMask + 1u && StorageImageSlots <= SlotMask + 1u);

constexpr std::uint32_t PackSlot(std::uint32_t slot, std::uint32_t generation) {
    return (slot & SlotMask) | ((generation & GenerationMask) << SlotBits);
}

constexpr std::uint32_t SlotIndex(std::uint32_t packed) {
    return packed & SlotMask;
}

constexpr std::uint32_t SlotGeneration(std::uint32_t packed) {
    return packed >> SlotBits;
}

inline constexpr std::uint32_t FeedbackWord5 = 5u;
inline constexpr std::uint32_t FeedbackMask5 = (0xfffu << 8u) | (1u << 25u);
inline constexpr std::uint32_t FeedbackWord6 = 6u;
inline constexpr std::uint32_t FeedbackMask6 = 0xffu;

constexpr std::uint32_t MaskedDescriptorWord(std::uint32_t index, std::uint32_t word) {
    if (index == FeedbackWord5) return word & ~FeedbackMask5;
    if (index == FeedbackWord6) return word & ~FeedbackMask6;
    return word;
}

constexpr Table::Key MakeKey(std::span<const std::uint32_t, Table::DescriptorWords> descriptor, std::uint64_t interfaceId, std::uint32_t samplerClass, std::span<const std::uint32_t, Table::SamplerWords> sampler) {
    Table::Key key{};
    for (std::uint32_t word = 0; word < Table::DescriptorWords; ++word) key[word] = MaskedDescriptorWord(word, descriptor[word]);
    key[Table::InterfaceWord] = static_cast<std::uint32_t>(interfaceId);
    key[Table::InterfaceWord + 1u] = static_cast<std::uint32_t>(interfaceId >> 32u);
    key[Table::SamplerClassWord] = samplerClass;
    for (std::uint32_t word = 0; word < Table::SamplerWords; ++word) key[Table::SamplerWord + word] = sampler[word];
    return key;
}

constexpr Table::Key MakeKey(std::span<const std::uint32_t, Table::DescriptorWords> descriptor, std::uint64_t interfaceId, std::uint32_t samplerClass) {
    const std::array<std::uint32_t, Table::SamplerWords> none{};
    return MakeKey(descriptor, interfaceId, samplerClass, none);
}

inline constexpr std::uint32_t HeaderBinding = 3u;
inline constexpr std::uint32_t NoSamplerClass = 0u;
inline constexpr std::uint32_t DirectSamplerClass = 1u << 30u;
inline constexpr std::uint32_t TableSamplerClass = Table::RuntimeSampler;

struct TableUse {
    std::uint64_t interfaceId = 0;
    std::uint32_t samplerClass = 0;
    std::array<std::uint32_t, Table::SamplerWords> sampler{};

    bool operator==(const TableUse& other) const = default;
    auto operator<=>(const TableUse& other) const = default;
};

struct TableRegistration {
    std::uint64_t base = 0;
    std::uint32_t records = 0;
    std::uint32_t stride = Table::DescriptorWords * 4u;
    std::vector<TableUse> uses;
    std::uint32_t dwords = Table::DescriptorWords;

    bool operator==(const TableRegistration& other) const = default;
    auto operator<=>(const TableRegistration& other) const = default;
};

}

#endif
