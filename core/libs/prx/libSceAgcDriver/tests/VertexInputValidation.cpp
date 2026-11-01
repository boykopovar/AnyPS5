#include "Translation/ShaderInputInfoBuilder.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <optional>
#include <set>
#include <string>
#include <vector>

using namespace ShaderRecompiler;

namespace {

struct BufferInput {
    std::uint64_t address;
    std::uint32_t stride;
    std::uint32_t records;
    std::uint32_t fetchIndex;
};

ShaderVertexStageInfo vertexInputs(const std::vector<BufferInput>& buffers, bool embedded = false) {
    ShaderVertexStageInfo vertex{};
    for (const auto& buffer : buffers) {
        const auto index = vertex.resourcesNum++;
        vertex.resources[index].fields = {static_cast<std::uint32_t>(buffer.address), static_cast<std::uint32_t>(buffer.address >> 32u) | buffer.stride << 16u, buffer.records, 0u};
        vertex.resourcesDst[index] = {static_cast<std::int32_t>(index * 4u), 4, static_cast<std::int32_t>(index), buffer.fetchIndex};
    }
    vertex.fetchEmbedded = embedded;
    return vertex;
}

template<typename TRun>
std::string errorOf(TRun&& run) {
    try {
        run();
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

}

int main() {
    std::vector<std::optional<ShaderVertexStageInfo>> vertexCases{std::nullopt};
    auto overfull = vertexInputs({{0x10000u, 16u, 100u, 0u}});
    overfull.resourcesNum = ShaderVertexStageInfo::MaxResources + 1u;
    vertexCases.push_back(overfull);
    vertexCases.push_back(vertexInputs({{0x10000u, 16u, 100u, 0u}, {0x10004u, 16u, 99u, 0u}}));
    vertexCases.push_back(vertexInputs({{0x10000u, 16u, 100u, 0u}, {0x10004u, 16u, 99u, 0u}}, true));
    vertexCases.push_back(vertexInputs({{0x10000u, 16u, 100u, 0u}, {0x10004u, 16u, 100u, 0u}, {0x20000u, 8u, 50u, 0u}, {0x10008u, 16u, 100u, 1u}}));
    std::vector<BufferInput> attributes;
    std::vector<BufferInput> separate;
    for (std::uint32_t index = 0; index < ShaderVertexStageInfo::MaxResources; ++index) {
        attributes.push_back({0x30000u + index * 4u, 128u, 10u, 0u});
        separate.push_back({0x40000u + index * 0x1000u, 4u + index * 4u, 10u, 0u});
    }
    vertexCases.push_back(vertexInputs(attributes));
    vertexCases.push_back(vertexInputs(separate));

    const MeshConfiguration mesh{3u, 64u, 64u, 64u, 64u, 64u, 0u, 0u};
    auto noThreads = mesh;
    noThreads.threadsPerGroup = 0u;
    auto badProvoking = mesh;
    badProvoking.provokingVertex = 3u;
    std::vector<std::optional<MeshConfiguration>> meshCases{std::nullopt, mesh, noThreads, badProvoking};
    struct MeshExpectation {
        std::uint32_t outputPrimitiveSize;
        std::uint32_t provokingVertex;
        bool accepted;
    };
    constexpr std::array meshExpectations{MeshExpectation{0u, 0u, false}, MeshExpectation{1u, 0u, true}, MeshExpectation{1u, 1u, false}, MeshExpectation{2u, 0u, true}, MeshExpectation{2u, 1u, true}, MeshExpectation{2u, 2u, false}, MeshExpectation{3u, 2u, true}, MeshExpectation{3u, 3u, false}, MeshExpectation{4u, 0u, false}, MeshExpectation{4u, 3u, false}};
    const auto meshWith = [&](const MeshExpectation& expectation) {
        auto varied = mesh;
        varied.outputPrimitiveSize = expectation.outputPrimitiveSize;
        varied.provokingVertex = expectation.provokingVertex;
        return varied;
    };
    for (const auto& expectation : meshExpectations) meshCases.push_back(meshWith(expectation));

    const TessellationConfiguration tessellation{3u, 3u, 1u, 2u, 2u, 16u};
    std::vector<std::optional<TessellationConfiguration>> tessellationCases{std::nullopt, tessellation};
    const auto vary = [&](auto change) {
        auto varied = tessellation;
        change(varied);
        tessellationCases.push_back(varied);
    };
    vary([](TessellationConfiguration& config) { config.inputControlPoints = 0u; });
    vary([](TessellationConfiguration& config) { config.outputControlPoints = 33u; });
    vary([](TessellationConfiguration& config) { config.domain = 3u; });
    vary([](TessellationConfiguration& config) { config.partitioning = 1u; });
    vary([](TessellationConfiguration& config) { config.outputTopology = 1u; });
    vary([](TessellationConfiguration& config) { config.patchesPerGroup = 0u; });
    vary([](TessellationConfiguration& config) { config.patchesPerGroup = 257u; });

    constexpr std::array stages{ShaderStageKind::Vertex, ShaderStageKind::Local, ShaderStageKind::TessellationControl, ShaderStageKind::TessellationEvaluation, ShaderStageKind::Mesh};
    std::size_t mismatches = 0;
    std::size_t accepted = 0;
    std::set<std::string> errors;
    for (const auto stage : stages) {
        for (std::size_t vertexCase = 0; vertexCase < vertexCases.size(); ++vertexCase) {
            for (std::size_t meshCase = 0; meshCase < meshCases.size(); ++meshCase) {
                for (std::size_t tessellationCase = 0; tessellationCase < tessellationCases.size(); ++tessellationCase) {
                    GuestContext context{};
                    context.waveSize = 64u;
                    context.vertex = vertexCases[vertexCase];
                    const auto* meshConfig = meshCases[meshCase] ? &*meshCases[meshCase] : nullptr;
                    const auto* tessellationConfig = tessellationCases[tessellationCase] ? &*tessellationCases[tessellationCase] : nullptr;
                    const auto built = errorOf([&] { static_cast<void>(BuildShaderStageInputInfo(stage, context, 64u, meshConfig, tessellationConfig)); });
                    const auto validated = errorOf([&] { ValidateVertexStageInputs(stage, context, meshConfig, tessellationConfig); });
                    if (built != validated) {
                        ++mismatches;
                        std::fprintf(stderr, "stage %d, vertex case %zu, mesh case %zu, tessellation case %zu: the build says \"%s\", the validation \"%s\"\n", static_cast<int>(stage), vertexCase, meshCase, tessellationCase, built.c_str(), validated.c_str());
                    }
                    if (built.empty()) ++accepted;
                    else errors.insert(built);
                }
            }
        }
    }
    const std::array expected{"GuestContext.vertex is not set", "invalid vertex resource count", "disagree on record count", "tessellation configuration is missing", "invalid tessellation control-point counts", "unsupported tessellation configuration", "invalid tessellation patches per group", "has no mesh configuration", "invalid mesh configuration"};
    for (const auto* message : expected) {
        const bool seen = std::any_of(errors.begin(), errors.end(), [&](const std::string& error) { return error.find(message) != std::string::npos; });
        if (!seen) {
            ++mismatches;
            std::fprintf(stderr, "no case raised \"%s\"\n", message);
        }
    }
    GuestContext meshContext{};
    meshContext.waveSize = 64u;
    meshContext.vertex = vertexInputs({{0x10000u, 16u, 100u, 0u}});
    for (const auto& expectation : meshExpectations) {
        const auto varied = meshWith(expectation);
        const bool built = errorOf([&] { static_cast<void>(BuildShaderStageInputInfo(ShaderStageKind::Mesh, meshContext, 64u, &varied, nullptr)); }).empty();
        const bool validated = errorOf([&] { ValidateVertexStageInputs(ShaderStageKind::Mesh, meshContext, &varied, nullptr); }).empty();
        if (built != expectation.accepted || validated != expectation.accepted) {
            ++mismatches;
            std::fprintf(stderr, "mesh output primitive size %u, provoking vertex %u: expected %s, the build %s, the validation %s\n", expectation.outputPrimitiveSize, expectation.provokingVertex, expectation.accepted ? "accepted" : "rejected", built ? "accepts" : "rejects", validated ? "accepts" : "rejects");
        }
    }
    if (accepted == 0) {
        ++mismatches;
        std::fprintf(stderr, "no case was accepted\n");
    }
    if (mismatches != 0) return 1;
    std::printf("vertex input validation matches the input info build in %zu cases (%zu accepted)\n", stages.size() * vertexCases.size() * meshCases.size() * tessellationCases.size(), accepted);
    return 0;
}
