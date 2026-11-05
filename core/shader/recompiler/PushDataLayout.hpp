#ifndef CORE_SHADER_RECOMPILER_PUSHDATALAYOUT_HPP
#define CORE_SHADER_RECOMPILER_PUSHDATALAYOUT_HPP

#include "PipelineSpecialization.hpp"
#include "Recompiler.hpp"
#include "RuntimeAbi.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

enum class PushPart : std::uint32_t { PreRasterization, Fragment, Compute };

inline PushPart PushPartOf(ShaderStage stage) {
    if (stage == ShaderStage::Fragment) return PushPart::Fragment;
    return stage == ShaderStage::Compute ? PushPart::Compute : PushPart::PreRasterization;
}

struct PushStage {
    PushPart part = PushPart::PreRasterization;
    std::uint32_t items = 0;
    std::uint32_t pushedWanted = 0;
};

struct StagePushPlacement {
    std::array<std::uint32_t, RuntimeAbi::RootItemCount> items{};
    RuntimeAbi::PushedPlacement pushed;

    [[nodiscard]] std::uint32_t Item(RuntimeAbi::RootItem item) const { return items[static_cast<std::uint32_t>(item)]; }
    bool operator==(const StagePushPlacement&) const = default;
};

constexpr std::uint64_t PushBytesMask(std::uint32_t offset, std::uint32_t bytes) {
    std::uint64_t mask = 0;
    for (auto dword = offset / 4u; dword < (offset + bytes + 3u) / 4u && dword < RuntimeAbi::PushDataDwords; ++dword) mask |= 1ull << dword;
    return mask;
}

constexpr std::uint64_t PushFeatureDwords(bool meshDraw, bool tessellationLevels, bool depthRemap) {
    std::uint64_t mask = 0;
    if (meshDraw) mask |= PushBytesMask(MeshDrawPushOffsetBytes, MeshDrawPushBytes);
    if (tessellationLevels) mask |= PushBytesMask(TessellationLevelPushOffsetBytes, TessellationLevelPushBytes);
    if (depthRemap) mask |= PushBytesMask(DepthRemapPushOffsetBytes, DepthRemapPushBytes);
    return mask;
}

inline constexpr std::uint32_t RootItemMask = (1u << RuntimeAbi::RootItemCount) - 1u;

constexpr std::uint64_t StagePushFeatures(ShaderStage stage, std::uint32_t pushItems) {
    const bool depthRemap = (pushItems & RuntimeAbi::DepthRemapPushBit) != 0u;
    std::uint64_t mask = PushFeatureDwords(stage == ShaderStage::Mesh || stage == ShaderStage::Geometry, stage == ShaderStage::TessellationControl || stage == ShaderStage::TessellationEvaluation, depthRemap);
    if (stage == ShaderStage::Compute) mask |= PushBytesMask(GpuScratchPoolAddressPushByte - 8u, GpuScratchPoolDwordsPushByte + 4u - (GpuScratchPoolAddressPushByte - 8u));
    return mask;
}

inline std::vector<StagePushPlacement> PlacePushData(std::span<const PushStage> stages, std::uint64_t occupied) {
    namespace Abi = RuntimeAbi;
    constexpr std::uint32_t total = Abi::PushDataDwords;
    std::vector<StagePushPlacement> placements(stages.size());
    for (auto& placement : placements) placement.items.fill(Abi::NoPushPosition);
    auto used = occupied;
    const auto isFree = [&](std::uint32_t first, std::uint32_t width) {
        for (auto dword = first; dword < first + width; ++dword) {
            if (dword >= total || (used >> dword & 1ull) != 0u) return false;
        }
        return true;
    };
    const auto take = [&](std::uint32_t width, bool aligned, bool top) {
        for (std::uint32_t step = 0; step + width <= total; ++step) {
            const auto first = top ? total - width - step : step;
            if ((aligned && first % 2u != 0u) || !isFree(first, width)) continue;
            for (auto dword = first; dword < first + width; ++dword) used |= 1ull << dword;
            return first;
        }
        throw std::runtime_error("the push data has no room for a pipeline's root");
    };
    const auto placeRoot = [&](std::uint32_t items, bool top, std::array<std::uint32_t, Abi::RootItemCount>& positions) {
        for (const bool addresses : {true, false}) {
            for (std::uint32_t index = 0; index < Abi::RootItemCount; ++index) {
                const auto item = static_cast<Abi::RootItem>(index);
                if ((items & Abi::RootItemBit(item)) == 0u || Abi::RootAddressItem(item) != addresses) continue;
                positions[index] = take(addresses ? 2u : 1u, addresses, top);
            }
        }
    };
    constexpr auto globalItems = Abi::RootItemBit(Abi::RootItem::ShaderData) - 1u;
    for (const auto part : {PushPart::Fragment, PushPart::Compute, PushPart::PreRasterization}) {
        const bool top = part != PushPart::PreRasterization;
        std::uint32_t globals = 0;
        for (const auto& stage : stages) {
            if (stage.part == part) globals |= stage.items & globalItems;
        }
        std::array<std::uint32_t, Abi::RootItemCount> shared{};
        shared.fill(Abi::NoPushPosition);
        placeRoot(globals, top, shared);
        for (std::size_t index = 0; index < stages.size(); ++index) {
            if (stages[index].part != part) continue;
            for (std::uint32_t item = 0; item < Abi::RootItemCount; ++item) {
                if ((stages[index].items & globalItems & (1u << item)) != 0u) placements[index].items[item] = shared[item];
            }
            placeRoot(stages[index].items & ~globalItems, top, placements[index].items);
        }
    }
    std::vector<std::pair<std::uint32_t, std::uint32_t>> runs;
    for (std::uint32_t dword = 0; dword < total;) {
        if ((used >> dword & 1ull) != 0u) {
            ++dword;
            continue;
        }
        auto end = dword;
        while (end < total && (used >> end & 1ull) == 0u) ++end;
        runs.emplace_back(dword, end);
        dword = end;
    }
    std::uint32_t room = 0;
    std::uint32_t largest = 0;
    for (const auto& [first, end] : runs) {
        room += end - first;
        largest = std::max(largest, end - first);
    }
    std::vector<std::uint32_t> wanted(stages.size(), 0u);
    std::uint32_t sum = 0;
    for (std::size_t index = 0; index < stages.size(); ++index) {
        wanted[index] = std::min({stages[index].pushedWanted, Abi::PushedDataWantedLimit, largest});
        sum += wanted[index];
    }
    auto counts = wanted;
    if (sum > room) {
        const auto sharedAt = [&](std::uint32_t level) {
            std::uint32_t shared = 0;
            for (const auto want : wanted) shared += std::min(want, level);
            return shared;
        };
        std::uint32_t level = 0;
        while (sharedAt(level + 1u) <= room) ++level;
        auto left = room - sharedAt(level);
        for (std::size_t index = 0; index < counts.size(); ++index) {
            counts[index] = std::min(wanted[index], level);
            if (left != 0u && wanted[index] > level) {
                ++counts[index];
                --left;
            }
        }
    }
    const auto claim = [&](std::size_t index, bool top) {
        auto count = counts[index];
        if (count == 0u) return;
        auto chosen = runs.end();
        if (top) {
            for (auto run = runs.rbegin(); run != runs.rend(); ++run) {
                if (run->second - run->first >= count) {
                    chosen = std::prev(run.base());
                    break;
                }
            }
        } else {
            chosen = std::find_if(runs.begin(), runs.end(), [&](const auto& run) { return run.second - run.first >= count; });
        }
        if (chosen == runs.end()) {
            chosen = std::max_element(runs.begin(), runs.end(), [](const auto& left, const auto& right) { return left.second - left.first < right.second - right.first; });
            if (chosen == runs.end()) return;
            count = chosen->second - chosen->first;
        }
        if (top) {
            placements[index].pushed = {chosen->second - count, count};
            chosen->second -= count;
        } else {
            placements[index].pushed = {chosen->first, count};
            chosen->first += count;
        }
    };
    for (std::size_t index = 0; index < stages.size(); ++index) {
        if (stages[index].part == PushPart::Fragment) claim(index, true);
    }
    for (std::size_t index = 0; index < stages.size(); ++index) {
        if (stages[index].part != PushPart::Fragment) claim(index, false);
    }
    return placements;
}

inline StagePushPlacement StandalonePushPlacement(ShaderStage stage, std::uint32_t pushItems, std::size_t pushedWanted) {
    const PushStage pushStage{PushPartOf(stage), pushItems & RootItemMask, static_cast<std::uint32_t>(pushedWanted)};
    return PlacePushData(std::span(&pushStage, 1), StagePushFeatures(stage, pushItems)).front();
}

inline void AppendPushSpecialization(std::vector<PipelineSpecializationConstant>& constants, const StagePushPlacement& placement) {
    for (std::uint32_t item = 0; item < RuntimeAbi::RootItemCount; ++item) {
        if (placement.items[item] != RuntimeAbi::NoPushPosition) constants.push_back({PipelineSpecialization::RootItemPositionBase + item, placement.items[item]});
    }
    constants.push_back({PipelineSpecialization::PushedDataBase, placement.pushed.base});
    constants.push_back({PipelineSpecialization::PushedDataCount, placement.pushed.count});
}

}

#endif
