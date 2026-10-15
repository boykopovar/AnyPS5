#include "Recompiler.hpp"
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

ShaderRecompiler::MeshTargetLimits Limits(std::uint32_t vertexGranularity, std::uint32_t primitiveGranularity) {
    ShaderRecompiler::MeshTargetLimits limits{};
    limits.maxOutputMemoryBytes = 32768u;
    limits.maxOutputComponents = 128u;
    limits.outputPerVertexGranularity = vertexGranularity;
    limits.outputPerPrimitiveGranularity = primitiveGranularity;
    return limits;
}

ShaderRecompiler::MeshConfiguration Mesh(std::uint32_t vertices, std::uint32_t primitives) {
    ShaderRecompiler::MeshConfiguration mesh{};
    mesh.maxVertices = vertices;
    mesh.maxPrimitives = primitives;
    return mesh;
}

}

int main() {
    try {
        using ShaderRecompiler::MeshBarycentricOutputBudget;
        using ShaderRecompiler::MeshOutputUse;

        MeshOutputUse ccleste;
        ccleste.flatParameters = 6u;
        ccleste.perVertexParameters = 2u;
        ccleste.clipVaryings = 1u;
        const auto budget = MeshBarycentricOutputBudget(Mesh(128u, 128u), Limits(32u, 32u), ccleste);
        Require(budget.vertexLocations == 8u, "per-vertex locations: position, six flat exports and the smooth clip varying make 8");
        Require(budget.primitiveLocations == 10u, "per-primitive locations: three corners, two per-vertex parameters of three vertices and the cull flag make 10");
        Require(budget.components == 72u, "components are 4 per location: 18 locations make 72");
        Require(budget.bytes == 36864u, "bytes: (8 + 10) locations x 16 bytes x 128 = 36864, got " + std::to_string(budget.bytes));
        Require(budget.bytes > Limits(32u, 32u).maxOutputMemoryBytes, "ccleste's configuration does not fit 32768 bytes");

        Require(MeshBarycentricOutputBudget(Mesh(100u, 100u), Limits(32u, 32u), ccleste).bytes == 36864u, "100 vertices and primitives are padded to the granularity of 32, which gives 128");
        Require(MeshBarycentricOutputBudget(Mesh(128u, 128u), Limits(1u, 1u), ccleste).bytes == 36864u, "a granularity of 1 changes nothing for 128");

        MeshOutputUse distances = ccleste;
        distances.clipDistances = 5u;
        distances.cullDistances = 1u;
        distances.layer = true;
        const auto more = MeshBarycentricOutputBudget(Mesh(128u, 128u), Limits(32u, 32u), distances);
        Require(more.vertexLocations == 8u + 2u + 1u, "five clip distances take 2 locations and one cull distance 1");
        Require(more.primitiveLocations == 11u, "the layer output takes one per-primitive location");

        std::puts("mesh output budget tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
