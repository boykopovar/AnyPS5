#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVCLIPDISTANCES_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVCLIPDISTANCES_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>

namespace ShaderRecompiler {

[[nodiscard]] inline std::uint32_t VertexClipDistanceCount(std::span<const std::uint32_t> words) {
    std::set<std::uint32_t> builtins;
    std::map<std::uint32_t, std::uint32_t> pointees;
    std::map<std::uint32_t, std::uint32_t> arrays;
    std::map<std::uint32_t, std::uint32_t> constants;
    std::map<std::uint32_t, std::uint32_t> outputs;
    for (std::size_t i = 5; i < words.size();) {
        const auto count = words[i] >> 16u;
        const auto opcode = words[i] & 0xffffu;
        if (count == 0u || i + count > words.size()) throw std::runtime_error("Clip distances: malformed vertex SPIR-V");
        const auto* word = words.data() + i;
        if (opcode == spv::OpDecorate && count == 4u && word[2] == spv::DecorationBuiltIn && word[3] == spv::BuiltInClipDistance) builtins.insert(word[1]);
        if (opcode == spv::OpTypePointer && count == 4u) pointees[word[1]] = word[3];
        if (opcode == spv::OpTypeArray && count == 4u) arrays[word[1]] = word[3];
        if (opcode == spv::OpConstant && count == 4u) constants[word[2]] = word[3];
        if (opcode == spv::OpVariable && count >= 4u && word[3] == spv::StorageClassOutput) outputs[word[2]] = word[1];
        i += count;
    }
    for (const auto variable : builtins) {
        const auto output = outputs.find(variable);
        if (output == outputs.end()) continue;
        const auto pointee = pointees.find(output->second);
        const auto array = pointee == pointees.end() ? arrays.end() : arrays.find(pointee->second);
        const auto length = array == arrays.end() ? constants.end() : constants.find(array->second);
        if (length == constants.end() || length->second == 0u || length->second > 8u) throw std::runtime_error("Clip distances: the vertex gl_ClipDistance output is not an array of one to eight elements");
        return length->second;
    }
    return 0u;
}

}

#endif
