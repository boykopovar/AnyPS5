#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVVERTEXLAYER_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVVERTEXLAYER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <vector>

namespace ShaderRecompiler {

struct VertexLayerOutput {
    std::uint32_t variable;
    std::uint32_t location;
    std::size_t decoration;
};

[[nodiscard]] inline std::optional<VertexLayerOutput> FindVertexLayerOutput(std::span<const std::uint32_t> words) {
    if (words.size() < 5u || words[0] != spv::MagicNumber) return std::nullopt;
    std::optional<std::size_t> decoration;
    std::map<std::uint32_t, std::uint32_t> locations;
    std::set<std::uint32_t> components;
    std::map<std::uint32_t, std::uint32_t> pointees;
    std::map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> arrays;
    std::map<std::uint32_t, std::uint32_t> columns;
    std::map<std::uint32_t, std::uint32_t> constants;
    std::set<std::uint32_t> unsignedInts;
    std::map<std::uint32_t, std::uint32_t> outputs;
    for (std::size_t i = 5; i < words.size();) {
        const auto count = words[i] >> 16u;
        const auto opcode = words[i] & 0xffffu;
        if (count == 0u || i + count > words.size()) throw std::runtime_error("Vertex layer output: malformed vertex SPIR-V");
        const auto* word = words.data() + i;
        if (opcode == spv::OpDecorate && count >= 3u) {
            if (word[2] == spv::DecorationBuiltIn && count == 4u && word[3] == spv::BuiltInViewportIndex) throw std::runtime_error("Vertex layer output: the vertex shader writes gl_ViewportIndex, which the generated stages do not forward");
            if (word[2] == spv::DecorationBuiltIn && count == 4u && word[3] == spv::BuiltInLayer) {
                if (decoration) throw std::runtime_error("Vertex layer output: more than one gl_Layer decoration");
                decoration = i;
            }
            if (word[2] == spv::DecorationLocation && count == 4u) locations[word[1]] = word[3];
            if (word[2] == spv::DecorationComponent) components.insert(word[1]);
        }
        if (opcode == spv::OpMemberDecorate && count == 5u && word[3] == spv::DecorationBuiltIn && (word[4] == spv::BuiltInLayer || word[4] == spv::BuiltInViewportIndex)) throw std::runtime_error("Vertex layer output: gl_Layer or gl_ViewportIndex is a block member");
        if (opcode == spv::OpTypePointer && count == 4u) pointees[word[1]] = word[3];
        if (opcode == spv::OpTypeArray && count == 4u) arrays[word[1]] = {word[2], word[3]};
        if (opcode == spv::OpTypeMatrix && count == 4u) columns[word[1]] = word[3];
        if (opcode == spv::OpTypeInt && count == 4u && word[2] == 32u && word[3] == 0u) unsignedInts.insert(word[1]);
        if (opcode == spv::OpConstant && count == 4u) constants[word[2]] = word[3];
        if (opcode == spv::OpVariable && count >= 4u && word[3] == spv::StorageClassOutput) outputs[word[2]] = word[1];
        i += count;
    }
    if (!decoration) return std::nullopt;
    const auto variable = words[*decoration + 1];
    const auto output = outputs.find(variable);
    if (output == outputs.end()) throw std::runtime_error("Vertex layer output: gl_Layer is not an output variable");
    const auto pointee = pointees.find(output->second);
    if (pointee == pointees.end() || !unsignedInts.contains(pointee->second)) throw std::runtime_error("Vertex layer output: gl_Layer is not a 32-bit unsigned integer");
    if (locations.contains(variable) || components.contains(variable)) throw std::runtime_error("Vertex layer output: gl_Layer has a location or component decoration");
    const auto span = [&](std::uint32_t type) {
        std::uint32_t slots = 1u;
        for (std::uint32_t depth = 0; depth < 8u; ++depth) {
            const auto array = arrays.find(type);
            if (array == arrays.end()) break;
            const auto length = constants.find(array->second.second);
            if (length == constants.end() || length->second == 0u || length->second > 32u) throw std::runtime_error("Vertex layer output: a vertex output array has no constant length of one to 32");
            slots *= length->second;
            type = array->second.first;
        }
        const auto matrix = columns.find(type);
        if (matrix != columns.end()) slots *= matrix->second;
        return slots;
    };
    std::array<bool, 32> used{};
    for (const auto& [id, pointer] : outputs) {
        const auto location = locations.find(id);
        if (location == locations.end()) continue;
        const auto type = pointees.find(pointer);
        if (type == pointees.end()) throw std::runtime_error("Vertex layer output: a vertex output has no pointer type");
        const auto slots = span(type->second);
        for (std::uint32_t slot = 0; slot < slots && location->second + slot < used.size(); ++slot) used[location->second + slot] = true;
    }
    for (std::uint32_t location = 0; location < used.size(); ++location) {
        if (!used[location]) return VertexLayerOutput{variable, location, *decoration};
    }
    throw std::runtime_error("Vertex layer output: no free location for the vertex gl_Layer");
}

[[nodiscard]] inline std::vector<std::uint32_t> RelocateVertexLayerOutput(std::span<const std::uint32_t> words) {
    const auto layer = FindVertexLayerOutput(words);
    if (!layer) return {};
    std::vector<std::uint32_t> relocated(words.begin(), words.end());
    relocated[layer->decoration + 2] = spv::DecorationLocation;
    relocated[layer->decoration + 3] = layer->location;
    return relocated;
}

}

#endif
