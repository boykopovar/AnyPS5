#ifndef CORE_SHADER_RECOMPILER_VERTEXINPUTSPECIALIZATION_HPP
#define CORE_SHADER_RECOMPILER_VERTEXINPUTSPECIALIZATION_HPP

#include "Recompiler.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>
#include <spirv/unified1/spirv.hpp>

namespace ShaderRecompiler {

inline SharedSpirv SpecializeVertexInputTypes(const CompiledShaderArtifact& artifact, std::span<const std::uint32_t> classes) {
    if (artifact.vertexInputPatches.empty()) return artifact.spirv;
    auto words = artifact.spirv.Words();
    for (const auto& patch : artifact.vertexInputPatches) {
        if (patch.location >= classes.size() || classes[patch.location] >= patch.values.size() || patch.word >= words.size()) throw std::runtime_error("invalid prepared vertex type patch");
        words[patch.word] = patch.values[classes[patch.location]];
    }
    return SharedSpirv(std::move(words));
}

inline void PrepareVertexInputSpecialization(CompiledShaderArtifact& artifact) {
    const auto& source = artifact.spirv.Words();
    if (source.size() < 5u || source[0] != spv::MagicNumber) throw std::runtime_error("invalid vertex specialization module");
    std::map<std::uint32_t, std::uint32_t> locations;
    std::map<std::vector<std::uint32_t>, std::uint32_t> types;
    std::vector<std::vector<std::uint32_t>> declarations;
    std::vector<std::vector<std::uint32_t>> variables;
    std::map<std::uint32_t, std::uint32_t> pointers;
    std::map<std::uint32_t, std::uint32_t> loads;
    std::map<std::uint32_t, std::array<std::uint32_t, 3>> variableTypes;
    std::map<std::uint32_t, std::uint32_t> components;
    for (const auto& input : artifact.vertexInputs) {
        if (input.location >= ShaderVertexStageInfo::MaxResources || input.components == 0u || input.components > 4u || !components.emplace(input.location, input.components).second) throw std::runtime_error("invalid vertex specialization input");
    }
    auto bound = source[3];
    const auto declare = [&](std::vector<std::uint32_t> key) {
        const auto found = types.find(key);
        if (found != types.end()) return found->second;
        if (bound == std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("vertex specialization module ID overflow");
        const auto id = bound++;
        std::vector<std::uint32_t> words{(static_cast<std::uint32_t>(key.size() + 1u) << 16u) | key.front(), id};
        words.insert(words.end(), key.begin() + 1, key.end());
        types.emplace(std::move(key), id);
        declarations.push_back(std::move(words));
        return id;
    };
    for (std::size_t cursor = 5; cursor < source.size();) {
        const auto count = source[cursor] >> 16u;
        const auto op = static_cast<spv::Op>(source[cursor] & 0xffffu);
        if (count == 0u || count > source.size() - cursor) throw std::runtime_error("truncated vertex specialization instruction");
        if (op == spv::OpDecorate && count == 4u && source[cursor + 2u] == spv::DecorationLocation) locations.emplace(source[cursor + 1u], source[cursor + 3u]);
        if (op == spv::OpTypeInt || op == spv::OpTypeFloat || op == spv::OpTypeVector || op == spv::OpTypePointer) {
            std::vector<std::uint32_t> key{static_cast<std::uint32_t>(op)};
            key.insert(key.end(), source.begin() + cursor + 2u, source.begin() + cursor + count);
            types.emplace(std::move(key), source[cursor + 1u]);
        }
        cursor += count;
    }
    const std::array scalarTypes{declare({spv::OpTypeFloat, 32u}), declare({spv::OpTypeInt, 32u, 1u}), declare({spv::OpTypeInt, 32u, 0u})};
    std::array<std::uint32_t, 3> scalarPointers{};
    for (std::size_t kind = 0; kind < 3u; ++kind) scalarPointers[kind] = declare({spv::OpTypePointer, spv::StorageClassInput, scalarTypes[kind]});
    for (std::size_t cursor = 5; cursor < source.size();) {
        const auto count = source[cursor] >> 16u;
        const auto op = static_cast<spv::Op>(source[cursor] & 0xffffu);
        if (op == spv::OpVariable && count == 4u && source[cursor + 3u] == spv::StorageClassInput) {
            const auto location = locations.find(source[cursor + 2u]);
            if (location != locations.end() && components.contains(location->second)) {
                const auto id = source[cursor + 2u];
                pointers.emplace(id, location->second);
                auto& choices = variableTypes[id];
                for (std::size_t kind = 0; kind < 3u; ++kind) {
                    const auto count = components.at(location->second);
                    const auto type = count == 1u ? scalarTypes[kind] : declare({spv::OpTypeVector, scalarTypes[kind], count});
                    choices[kind] = declare({spv::OpTypePointer, spv::StorageClassInput, type});
                }
                variables.emplace_back(source.begin() + cursor, source.begin() + cursor + count);
            }
        }
        cursor += count;
    }
    if (variables.size() != artifact.vertexInputs.size()) throw std::runtime_error("vertex specialization inputs disagree with metadata");
    std::vector<std::uint32_t> result(source.begin(), source.begin() + 5);
    result[3] = bound;
    bool inserted = false;
    const auto patch = [&](std::uint32_t location, std::size_t word, const std::array<std::uint32_t, 3>& values) {
        if (word > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("vertex specialization offset overflow");
        artifact.vertexInputPatches.push_back({location, static_cast<std::uint32_t>(word), values});
    };
    artifact.vertexInputPatches.clear();
    for (std::size_t cursor = 5; cursor < source.size();) {
        const auto count = source[cursor] >> 16u;
        const auto op = static_cast<spv::Op>(source[cursor] & 0xffffu);
        if (op == spv::OpFunction && !inserted) {
            for (const auto& declaration : declarations) result.insert(result.end(), declaration.begin(), declaration.end());
            for (const auto& variable : variables) {
                patch(pointers.at(variable[2]), result.size() + 1u, variableTypes.at(variable[2]));
                result.insert(result.end(), variable.begin(), variable.end());
            }
            inserted = true;
        }
        if (op == spv::OpVariable && variableTypes.contains(source[cursor + 2u])) {
            cursor += count;
            continue;
        }
        if (op == spv::OpAccessChain && count == 5u && pointers.contains(source[cursor + 3u])) {
            const auto location = pointers.at(source[cursor + 3u]);
            pointers.emplace(source[cursor + 2u], location);
            patch(location, result.size() + 1u, scalarPointers);
        } else if (op == spv::OpLoad && count == 4u && pointers.contains(source[cursor + 3u])) {
            const auto location = pointers.at(source[cursor + 3u]);
            loads.emplace(source[cursor + 2u], location);
            patch(location, result.size() + 1u, scalarTypes);
        } else if (op == spv::OpBitcast && count == 4u && loads.contains(source[cursor + 3u])) {
            patch(loads.at(source[cursor + 3u]), result.size(), {(4u << 16u) | spv::OpBitcast, (4u << 16u) | spv::OpBitcast, (4u << 16u) | spv::OpCopyObject});
        }
        result.insert(result.end(), source.begin() + cursor, source.begin() + cursor + count);
        cursor += count;
    }
    if (!inserted) throw std::runtime_error("vertex specialization module has no function");
    artifact.spirv = std::move(result);
}

}

#endif
