#ifndef CORE_SHADER_RECOMPILER_IMAGEOVERWRITE_HPP
#define CORE_SHADER_RECOMPILER_IMAGEOVERWRITE_HPP

#include <algorithm>
#include <array>
#include <cstdint>

namespace ShaderRecompiler {

inline constexpr std::uint32_t NoOverwriteRegister = 0xffffffffu;

struct ImageOverwriteAxis {
    std::uint32_t groupAxis = 0;
    std::uint32_t tile = 0;
    std::uint32_t offsetRegister = NoOverwriteRegister;
    std::uint32_t boundRegister = NoOverwriteRegister;
    std::uint32_t boundImmediate = 0;
    std::uint32_t maxLane = 0;
    bool operator==(const ImageOverwriteAxis& other) const = default;
};

struct ImageOverwriteRange {
    std::uint32_t groupAxis = 0;
    std::uint32_t scale = 0;
    std::int32_t minLane = 0;
    std::int32_t maxLane = 0;
    std::uint32_t bits = 0;
    bool operator==(const ImageOverwriteRange& other) const = default;
};

struct ImageOverwrite {
    bool proved = false;
    std::uint32_t fullGroups = 0;
    std::array<ImageOverwriteAxis, 2> axes{};
    std::array<ImageOverwriteRange, 4> ranges{};
    bool operator==(const ImageOverwrite& other) const = default;
};

struct ImageOverwriteDispatch {
    std::array<std::uint32_t, 3> groups{};
    std::array<std::uint32_t, 3> partialThreads{};
    std::array<std::uint32_t, 16> userData{};
    std::uint32_t userCount = 0;
};

[[nodiscard]] inline bool ImageOverwriteCovers(const ImageOverwrite& proof, const ImageOverwriteDispatch& dispatch, std::uint32_t width, std::uint32_t height) {
    if (!proof.proved) return false;
    for (std::uint32_t axis = 0; axis < 3; ++axis) {
        if (dispatch.groups[axis] == 0u) return false;
        if ((proof.fullGroups & (1u << axis)) != 0u && dispatch.partialThreads[axis] != 0u) return false;
    }
    for (const auto& range : proof.ranges) {
        if (range.bits == 0u) continue;
        if (range.groupAxis >= 3u || range.minLane < 0 || dispatch.groups[range.groupAxis] == 0u) return false;
        const auto largest = std::uint64_t{range.scale} * (dispatch.groups[range.groupAxis] - 1u) + static_cast<std::uint64_t>(std::max(range.maxLane, 0));
        if (largest >= (std::uint64_t{1} << range.bits)) return false;
    }
    const std::array<std::uint32_t, 2> extent{width, height};
    for (std::size_t index = 0; index < proof.axes.size(); ++index) {
        const auto& axis = proof.axes[index];
        if (axis.groupAxis >= 3u || axis.tile == 0u) return false;
        if (axis.offsetRegister != NoOverwriteRegister && (axis.offsetRegister >= dispatch.userCount || axis.offsetRegister >= dispatch.userData.size() || dispatch.userData[axis.offsetRegister] != 0u)) return false;
        if (axis.boundRegister != NoOverwriteRegister && (axis.boundRegister >= dispatch.userCount || axis.boundRegister >= dispatch.userData.size())) return false;
        const auto groups = std::uint64_t{dispatch.groups[axis.groupAxis]};
        if (std::uint64_t{axis.tile} * groups + axis.maxLane >= (std::uint64_t{1} << 31u)) return false;
        auto covered = std::uint64_t{axis.tile} * groups;
        if (axis.boundRegister != NoOverwriteRegister) covered = std::min<std::uint64_t>(covered, dispatch.userData[axis.boundRegister]);
        if (axis.boundImmediate != 0u) covered = std::min<std::uint64_t>(covered, axis.boundImmediate);
        if (covered < extent[index]) return false;
    }
    return true;
}

}

#endif
