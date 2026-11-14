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
    std::uint32_t pushedBuffers = 0;
};

static_assert(RuntimeAbi::PushedBufferLimit == PipelineSpecialization::BufferRootCount);

constexpr std::array<std::uint32_t, RuntimeAbi::PushedBufferLimit> MakeBufferRoots() {
    std::array<std::uint32_t, RuntimeAbi::PushedBufferLimit> roots{};
    roots.fill(RuntimeAbi::NoPushPosition);
    return roots;
}

struct StagePushPlacement {
    std::array<std::uint32_t, RuntimeAbi::RootItemCount> items{};
    RuntimeAbi::PushedPlacement pushed;
    std::array<std::uint32_t, RuntimeAbi::PushedBufferLimit> bufferRoots = MakeBufferRoots();
    std::uint32_t bufferOverflow = RuntimeAbi::NoPushPosition;
    std::uint32_t pushedBuffers = 0;

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

inline std::vector<StagePushPlacement> PlacePushData(std::span<const PushStage> stages, std::uint64_t occupied, std::uint32_t pushedRecords = RuntimeAbi::PushedBufferLimit) {
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
    const auto tryTake = [&](std::uint32_t width, bool aligned, bool top) {
        for (std::uint32_t step = 0; step + width <= total; ++step) {
            const auto first = top ? total - width - step : step;
            if ((aligned && first % 2u != 0u) || !isFree(first, width)) continue;
            for (auto dword = first; dword < first + width; ++dword) used |= 1ull << dword;
            return first;
        }
        return Abi::NoPushPosition;
    };
    const auto take = [&](std::uint32_t width, bool aligned, bool top) {
        const auto first = tryTake(width, aligned, top);
        if (first == Abi::NoPushPosition) throw std::runtime_error("the push data has no room for a pipeline's root");
        return first;
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
    constexpr auto optionalItems = Abi::RootItemBit(Abi::RootItem::UserData);
    constexpr std::array parts{PushPart::Fragment, PushPart::Compute, PushPart::PreRasterization};
    std::vector<std::uint32_t> overflows(stages.size(), Abi::NoPushPosition);
    std::uint32_t globals = 0;
    std::uint32_t fragmentGlobals = 0;
    for (const auto& stage : stages) {
        globals |= stage.items & globalItems;
        if (stage.part == PushPart::Fragment) fragmentGlobals |= stage.items & globalItems;
    }
    std::array<std::uint32_t, Abi::RootItemCount> shared{};
    shared.fill(Abi::NoPushPosition);
    placeRoot(fragmentGlobals, false, shared);
    placeRoot(globals & ~fragmentGlobals, false, shared);
    for (std::size_t index = 0; index < stages.size(); ++index) {
        for (std::uint32_t item = 0; item < Abi::RootItemCount; ++item) {
            if ((stages[index].items & globalItems & (1u << item)) != 0u) placements[index].items[item] = shared[item];
        }
    }
    for (const auto part : parts) {
        const bool top = part != PushPart::PreRasterization;
        for (std::size_t index = 0; index < stages.size(); ++index) {
            if (stages[index].part != part) continue;
            placeRoot(stages[index].items & ~globalItems & ~optionalItems, top, placements[index].items);
            placements[index].pushedBuffers = stages[index].pushedBuffers;
        }
    }
    const auto recordsOf = [&](std::size_t index) { return std::min({stages[index].pushedBuffers, pushedRecords, Abi::PushedBufferLimit}); };
    for (const auto part : parts) {
        for (std::size_t index = 0; index < stages.size(); ++index) {
            if (stages[index].part == part && stages[index].pushedBuffers != 0u) overflows[index] = tryTake(2u, true, part != PushPart::PreRasterization);
        }
    }
    for (const auto part : parts) {
        const bool top = part != PushPart::PreRasterization;
        const auto reserved = part == PushPart::Fragment ? PushFeatureDwords(true, true, true) & ~used : 0u;
        used |= reserved;
        for (std::size_t index = 0; index < stages.size(); ++index) {
            const auto buffers = stages[index].pushedBuffers;
            if (stages[index].part != part || buffers == 0u) continue;
            const auto records = recordsOf(index);
            bool overflowed = buffers > records;
            for (std::uint32_t buffer = 0; buffer < records; ++buffer) {
                placements[index].bufferRoots[buffer] = tryTake(Abi::PushedBufferWords, true, top);
                overflowed = overflowed || placements[index].bufferRoots[buffer] == Abi::NoPushPosition;
            }
            if (overflowed && overflows[index] == Abi::NoPushPosition) throw std::runtime_error("the push data has no room for a stage's buffer addresses");
            if (overflowed) placements[index].bufferOverflow = overflows[index];
            else if (overflows[index] != Abi::NoPushPosition) used &= ~(3ull << overflows[index]);
        }
        used &= ~reserved;
    }
    for (const auto part : parts) {
        const bool top = part != PushPart::PreRasterization;
        const auto reserved = part == PushPart::Fragment ? PushFeatureDwords(true, true, true) & ~used : 0u;
        used |= reserved;
        for (std::size_t index = 0; index < stages.size(); ++index) {
            if (stages[index].part != part || (stages[index].items & Abi::RootItemBit(Abi::RootItem::UserData)) == 0u) continue;
            placements[index].items[static_cast<std::uint32_t>(Abi::RootItem::UserData)] = tryTake(2u, true, top);
        }
        used &= ~reserved;
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

static_assert(RuntimeAbi::RootItemCount <= PipelineSpecialization::RootItemPositions);

inline StagePushPlacement StandalonePushPlacement(ShaderStage stage, std::uint32_t pushItems, std::size_t pushedWanted, std::uint32_t pushedBuffers = 0u) {
    const PushStage pushStage{PushPartOf(stage), pushItems & RootItemMask, static_cast<std::uint32_t>(pushedWanted), pushedBuffers};
    return PlacePushData(std::span(&pushStage, 1), StagePushFeatures(stage, pushItems)).front();
}

inline void AppendPushSpecialization(std::vector<PipelineSpecializationConstant>& constants, const StagePushPlacement& placement) {
    for (std::uint32_t item = 0; item < RuntimeAbi::RootItemCount; ++item) {
        const bool optional = item == static_cast<std::uint32_t>(RuntimeAbi::RootItem::UserData);
        if (optional || placement.items[item] != RuntimeAbi::NoPushPosition) constants.push_back({PipelineSpecialization::RootItemPositionBase + item, placement.items[item]});
    }
    for (std::uint32_t buffer = 0; buffer < std::min(placement.pushedBuffers, RuntimeAbi::PushedBufferLimit); ++buffer) constants.push_back({PipelineSpecialization::BufferRootPosition + buffer, placement.bufferRoots[buffer]});
    if (placement.pushedBuffers != 0u) constants.push_back({PipelineSpecialization::BufferOverflowPosition, placement.bufferOverflow});
    constants.push_back({PipelineSpecialization::PushedDataBase, placement.pushed.base});
    constants.push_back({PipelineSpecialization::PushedDataCount, placement.pushed.count});
}

}

#endif
