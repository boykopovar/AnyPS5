#pragma once

#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace AgcDriver::Graphics {

struct MeshReuseGroup {
    std::uint32_t firstTriangle;
    std::uint32_t triangleCount;
    std::uint32_t firstVertex;
    std::uint32_t vertexCount;
};

struct MeshReuseGroups {
    std::vector<MeshReuseGroup> groups;
    std::vector<std::uint32_t> vertexPositions;
    std::vector<std::uint8_t> cornerSlots;
};

inline MeshReuseGroups BuildMeshReuseGroups(std::span<const std::uint32_t> indices, std::uint32_t vertexLimit, std::uint32_t primitiveLimit) {
    if (vertexLimit < 3u || vertexLimit > 256u) throw std::invalid_argument("mesh reuse grouping: vertex limit must be 3 to 256");
    if (primitiveLimit == 0u) throw std::invalid_argument("mesh reuse grouping: primitive limit must be nonzero");
    MeshReuseGroups result;
    const auto triangles = static_cast<std::uint32_t>(indices.size() / 3u);
    result.cornerSlots.resize(static_cast<std::size_t>(triangles) * 3u);
    MeshReuseGroup open{0u, 0u, 0u, 0u};
    std::vector<std::uint32_t> openValues;
    openValues.reserve(vertexLimit);
    const auto slotOf = [&](std::uint32_t value) -> std::int32_t {
        for (std::size_t i = 0; i < openValues.size(); ++i) {
            if (openValues[i] == value) return static_cast<std::int32_t>(i);
        }
        return -1;
    };
    const auto close = [&] {
        if (open.triangleCount != 0u) result.groups.push_back(open);
    };
    for (std::uint32_t t = 0; t < triangles; ++t) {
        std::uint32_t fresh = 0;
        for (std::uint32_t k = 0; k < 3u; ++k) {
            const auto value = indices[t * 3u + k];
            bool seen = slotOf(value) >= 0;
            for (std::uint32_t j = 0; j < k && !seen; ++j) seen = indices[t * 3u + j] == value;
            if (!seen) ++fresh;
        }
        if (open.triangleCount != 0u && (open.triangleCount + 1u > primitiveLimit || open.vertexCount + fresh > vertexLimit)) {
            close();
            open = MeshReuseGroup{t, 0u, static_cast<std::uint32_t>(result.vertexPositions.size()), 0u};
            openValues.clear();
        }
        for (std::uint32_t k = 0; k < 3u; ++k) {
            const auto position = t * 3u + k;
            const auto value = indices[position];
            auto slot = slotOf(value);
            if (slot < 0) {
                slot = static_cast<std::int32_t>(openValues.size());
                openValues.push_back(value);
                result.vertexPositions.push_back(position);
                ++open.vertexCount;
            }
            result.cornerSlots[position] = static_cast<std::uint8_t>(slot);
        }
        ++open.triangleCount;
    }
    close();
    return result;
}

inline std::vector<std::uint32_t> PackMeshGroupTable(const MeshReuseGroups& grouped) {
    constexpr std::uint32_t headerDwords = 3u;
    constexpr std::uint32_t groupDwords = 3u;
    const auto groups = static_cast<std::uint32_t>(grouped.groups.size());
    const auto triangles = static_cast<std::uint32_t>(grouped.cornerSlots.size() / 3u);
    const std::uint32_t listBase = headerDwords + groups * groupDwords;
    const std::uint32_t slotBase = listBase + static_cast<std::uint32_t>(grouped.vertexPositions.size());
    std::vector<std::uint32_t> words(static_cast<std::size_t>(slotBase) + triangles);
    words[0] = groups;
    words[1] = listBase;
    words[2] = slotBase;
    for (std::uint32_t g = 0; g < groups; ++g) {
        const auto& group = grouped.groups[g];
        if (group.triangleCount > 0xffffu || group.vertexCount > 0xffffu) throw std::length_error("mesh group table: a group's counts do not fit 16 bits");
        words[headerDwords + g * groupDwords + 0u] = group.firstTriangle;
        words[headerDwords + g * groupDwords + 1u] = group.firstVertex;
        words[headerDwords + g * groupDwords + 2u] = group.triangleCount | (group.vertexCount << 16u);
    }
    for (std::size_t i = 0; i < grouped.vertexPositions.size(); ++i) words[listBase + i] = grouped.vertexPositions[i];
    for (std::uint32_t t = 0; t < triangles; ++t) {
        words[slotBase + t] = grouped.cornerSlots[t * 3u] | (grouped.cornerSlots[t * 3u + 1u] << 8u) | (static_cast<std::uint32_t>(grouped.cornerSlots[t * 3u + 2u]) << 16u);
    }
    return words;
}

}
