#include "prx/libSceAgcDriver/Graphics/include/MeshReuseGroups.hpp"

#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Require(bool condition, const std::string& message) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message.c_str());
    ++failures;
}

using AgcDriver::Graphics::BuildMeshReuseGroups;
using AgcDriver::Graphics::MeshReuseGroups;
using AgcDriver::Graphics::PackMeshGroupTable;

void RequireReconstructs(const std::vector<std::uint32_t>& indices, const MeshReuseGroups& result, std::uint32_t vertexLimit, std::uint32_t primitiveLimit, const std::string& name) {
    std::uint32_t nextTriangle = 0;
    for (std::size_t g = 0; g < result.groups.size(); ++g) {
        const auto& group = result.groups[g];
        Require(group.firstTriangle == nextTriangle, name + ": group " + std::to_string(g) + " does not start where the last one ended");
        Require(group.triangleCount != 0u && group.triangleCount <= primitiveLimit, name + ": group " + std::to_string(g) + " has " + std::to_string(group.triangleCount) + " triangles");
        Require(group.vertexCount <= vertexLimit, name + ": group " + std::to_string(g) + " has " + std::to_string(group.vertexCount) + " vertices");
        for (std::uint32_t t = group.firstTriangle; t < group.firstTriangle + group.triangleCount; ++t) {
            for (std::uint32_t k = 0; k < 3u; ++k) {
                const auto slot = result.cornerSlots[t * 3u + k];
                Require(slot < group.vertexCount, name + ": corner slot outside its group");
                if (slot >= group.vertexCount) continue;
                const auto position = result.vertexPositions[group.firstVertex + slot];
                Require(indices[position] == indices[t * 3u + k], name + ": corner of triangle " + std::to_string(t) + " reads another vertex");
            }
        }
        for (std::uint32_t a = 0; a < group.vertexCount; ++a) {
            for (std::uint32_t b = a + 1u; b < group.vertexCount; ++b) {
                Require(indices[result.vertexPositions[group.firstVertex + a]] != indices[result.vertexPositions[group.firstVertex + b]], name + ": a group lists one vertex twice");
            }
        }
        nextTriangle += group.triangleCount;
    }
    Require(nextTriangle == indices.size() / 3u, name + ": groups do not cover every triangle");
}

void SharedQuad() {
    const std::vector<std::uint32_t> indices{0, 1, 2, 2, 1, 3};
    const auto result = BuildMeshReuseGroups(indices, 12u, 4u);
    Require(result.groups.size() == 1u, "quad: expected one group");
    Require(result.groups[0].vertexCount == 4u, "quad: expected 4 distinct vertices");
    Require(result.vertexPositions == std::vector<std::uint32_t>({0u, 1u, 2u, 5u}), "quad: vertex positions");
    Require(result.cornerSlots == std::vector<std::uint8_t>({0u, 1u, 2u, 2u, 1u, 3u}), "quad: corner slots");
    RequireReconstructs(indices, result, 12u, 4u, "quad");
}

void VertexLimitSplits() {
    const std::vector<std::uint32_t> indices{0, 1, 2, 3, 4, 5, 6, 7, 8};
    const auto result = BuildMeshReuseGroups(indices, 6u, 64u);
    Require(result.groups.size() == 2u, "disjoint: expected two groups under a 6-vertex budget");
    Require(result.groups[0].triangleCount == 2u && result.groups[1].triangleCount == 1u, "disjoint: expected 2 + 1 triangles");
    RequireReconstructs(indices, result, 6u, 64u, "disjoint");
}

void PrimitiveLimitSplits() {
    std::vector<std::uint32_t> indices;
    for (std::uint32_t i = 1; i <= 10; ++i) indices.insert(indices.end(), {0u, i, i + 1u});
    const auto result = BuildMeshReuseGroups(indices, 256u, 4u);
    Require(result.groups.size() == 3u, "fan: expected 4 + 4 + 2 triangles");
    RequireReconstructs(indices, result, 256u, 4u, "fan");
}

void GridReuseBeatsFixedGrouping() {
    std::vector<std::uint32_t> indices;
    constexpr std::uint32_t side = 17;
    for (std::uint32_t y = 0; y + 1 < side; ++y) {
        for (std::uint32_t x = 0; x + 1 < side; ++x) {
            const auto a = y * side + x, b = a + 1u, c = a + side, d = c + 1u;
            indices.insert(indices.end(), {a, b, c, c, b, d});
        }
    }
    const auto result = BuildMeshReuseGroups(indices, 14u, 14u);
    const auto triangles = indices.size() / 3u;
    Require(result.groups.size() * 4u < triangles, "grid: unique-vertex grouping should beat 4 triangles per group");
    RequireReconstructs(indices, result, 14u, 14u, "grid");
}

void RandomMeshesReconstruct() {
    std::mt19937 random(12345u);
    for (std::uint32_t round = 0; round < 200; ++round) {
        const std::uint32_t vertexCount = 3u + random() % 300u;
        const std::uint32_t triangles = 1u + random() % 400u;
        const std::uint32_t vertexLimit = 3u + random() % 254u;
        const std::uint32_t primitiveLimit = 1u + random() % 256u;
        std::vector<std::uint32_t> indices;
        for (std::uint32_t t = 0; t < triangles * 3u; ++t) indices.push_back(random() % vertexCount);
        RequireReconstructs(indices, BuildMeshReuseGroups(indices, vertexLimit, primitiveLimit), vertexLimit, primitiveLimit, "random round " + std::to_string(round));
    }
}

void PartialTriangleIgnored() {
    const std::vector<std::uint32_t> indices{0, 1, 2, 3, 4};
    const auto result = BuildMeshReuseGroups(indices, 12u, 4u);
    Require(result.groups.size() == 1u && result.groups[0].triangleCount == 1u, "partial: only the whole triangle is grouped");
    Require(result.cornerSlots.size() == 3u, "partial: corner slots cover whole triangles only");
}

void PacksTableLayout() {
    const std::vector<std::uint32_t> indices{0, 1, 2, 2, 1, 3};
    const auto words = PackMeshGroupTable(BuildMeshReuseGroups(indices, 12u, 4u));
    const std::vector<std::uint32_t> expected{1u, 6u, 10u, 0u, 0u, 2u | (4u << 16u), 0u, 1u, 2u, 5u, 0x020100u, 0x030102u};
    Require(words == expected, "table: quad layout");
    const std::vector<std::uint32_t> disjoint{0, 1, 2, 3, 4, 5};
    const auto split = PackMeshGroupTable(BuildMeshReuseGroups(disjoint, 3u, 64u));
    Require(split.size() == 3u + 2u * 3u + 6u + 2u && split[0] == 2u && split[1] == 9u && split[2] == 15u, "table: two-group header");
    Require(split[3] == 0u && split[4] == 0u && split[5] == (1u | (3u << 16u)) && split[6] == 1u && split[7] == 3u && split[8] == (1u | (3u << 16u)), "table: two-group entries");
    Require(split[16] == 0x020100u, "table: the second group's slots restart at 0");
}

void RejectsBadLimits() {
    const std::vector<std::uint32_t> indices{0, 1, 2};
    const auto throws = [&](std::uint32_t vertexLimit, std::uint32_t primitiveLimit) {
        try {
            static_cast<void>(BuildMeshReuseGroups(indices, vertexLimit, primitiveLimit));
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    Require(throws(2u, 4u), "a vertex limit below one triangle is refused");
    Require(throws(257u, 4u), "a vertex limit beyond a byte slot is refused");
    Require(throws(12u, 0u), "a zero primitive limit is refused");
}

}

int main() {
    SharedQuad();
    VertexLimitSplits();
    PrimitiveLimitSplits();
    GridReuseBeatsFixedGrouping();
    RandomMeshesReconstruct();
    PartialTriangleIgnored();
    PacksTableLayout();
    RejectsBadLimits();
    if (failures != 0) {
        std::fprintf(stderr, "%d failure(s)\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("mesh reuse grouping: all checks passed\n");
    return EXIT_SUCCESS;
}
