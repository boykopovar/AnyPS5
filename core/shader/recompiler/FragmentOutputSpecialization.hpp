#ifndef CORE_SHADER_RECOMPILER_FRAGMENTOUTPUTSPECIALIZATION_HPP
#define CORE_SHADER_RECOMPILER_FRAGMENTOUTPUTSPECIALIZATION_HPP

#include "PipelineSpecialization.hpp"
#include "Recompiler.hpp"
#include <limits>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#include <spirv/unified1/spirv.hpp>

namespace ShaderRecompiler {

inline std::vector<std::uint32_t> SpecializeFragmentOutputs(std::vector<std::uint32_t> words, std::span<const PipelineSpecializationConstant> constants, const std::set<std::uint32_t>& specializedIds) {
    std::set<std::uint32_t> packed;
    bool dualSource = false;
    for (const auto& constant : constants) {
        if (constant.id == PipelineSpecialization::DualSourceBlend) {
            if (constant.value > 1u) throw std::runtime_error("invalid prepared dual-source blend flag");
            dualSource = constant.value != 0u;
        }
        if (constant.id < PipelineSpecialization::ExportPackingBase || constant.id >= PipelineSpecialization::ExportPackingBase + 8u) continue;
        if (constant.value > static_cast<std::uint32_t>(ColorExportPacking::Unorm10_11_11)) throw std::runtime_error("invalid prepared fragment export packing");
        if (constant.value != static_cast<std::uint32_t>(ColorExportPacking::None)) packed.insert(constant.id - PipelineSpecialization::ExportPackingBase);
    }
    if (packed.empty() && !dualSource) return words;
    if (words.size() < 5u || words[0] != spv::MagicNumber) throw std::runtime_error("invalid fragment output specialization module");
    std::map<std::uint32_t, std::uint32_t> locations;
    std::set<std::uint32_t> indexed;
    std::map<std::uint32_t, std::vector<std::uint32_t>> typeKeys;
    std::map<std::vector<std::uint32_t>, std::uint32_t> types;
    std::map<std::uint32_t, std::uint32_t> outputs;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto op = static_cast<spv::Op>(words[cursor] & 0xffffu);
        if (count == 0u || count > words.size() - cursor) throw std::runtime_error("truncated fragment output specialization instruction");
        if (op == spv::OpDecorate && count == 4u && words[cursor + 2u] == spv::DecorationLocation) locations.emplace(words[cursor + 1u], words[cursor + 3u]);
        if (op == spv::OpDecorate && count == 4u && words[cursor + 2u] == spv::DecorationIndex) indexed.insert(words[cursor + 1u]);
        if ((op == spv::OpTypeInt || op == spv::OpTypeFloat || op == spv::OpTypeVector || op == spv::OpTypePointer) && count >= 3u) {
            std::vector<std::uint32_t> key{static_cast<std::uint32_t>(op)};
            key.insert(key.end(), words.begin() + cursor + 2u, words.begin() + cursor + count);
            typeKeys.emplace(words[cursor + 1u], key);
            types.emplace(std::move(key), words[cursor + 1u]);
        }
        if (op == spv::OpVariable && count >= 4u && words[cursor + 3u] == spv::StorageClassOutput) outputs.emplace(words[cursor + 2u], words[cursor + 1u]);
        cursor += count;
    }
    const auto key = [&](std::uint32_t id) {
        const auto found = typeKeys.find(id);
        return found != typeKeys.end() ? found->second : std::vector<std::uint32_t>{};
    };
    const auto floatVectorPointer = [&](std::uint32_t pointer) {
        const auto outer = key(pointer);
        if (outer.size() != 3u || outer[0] != spv::OpTypePointer || outer[1] != spv::StorageClassOutput) return false;
        const auto vector = key(outer[2]);
        return vector.size() == 3u && vector[0] == spv::OpTypeVector && vector[2] == 4u && key(vector[1]) == std::vector<std::uint32_t>{spv::OpTypeFloat, 32u};
    };
    const auto output = [&](std::uint32_t location) {
        std::uint32_t variable = 0;
        for (const auto& [id, pointer] : outputs) {
            const auto found = locations.find(id);
            if (found == locations.end() || found->second != location || indexed.contains(id)) continue;
            if (variable != 0u) throw std::runtime_error("fragment outputs share location " + std::to_string(location));
            variable = id;
        }
        return variable;
    };
    std::set<std::uint32_t> retyped;
    for (const auto location : packed) {
        const auto variable = output(location);
        if (variable == 0u) continue;
        if (!specializedIds.contains(PipelineSpecialization::ExportPackingBase + location)) throw std::runtime_error("fragment output " + std::to_string(location) + " has no 10_11_11 unorm packing path");
        if (!floatVectorPointer(outputs.at(variable))) throw std::runtime_error("packed fragment output " + std::to_string(location) + " is not a float4");
        retyped.insert(variable);
    }
    const auto secondSource = dualSource ? output(1u) : 0u;
    if (retyped.contains(secondSource)) throw std::runtime_error("the second dual-source blend color cannot be packed");
    if (retyped.empty() && secondSource == 0u) return words;
    auto bound = words[3];
    std::vector<std::vector<std::uint32_t>> declarations;
    const auto declare = [&](std::vector<std::uint32_t> declaration) {
        const auto found = types.find(declaration);
        if (found != types.end()) return found->second;
        if (bound == std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("fragment output specialization module ID overflow");
        const auto id = bound++;
        std::vector<std::uint32_t> instruction{(static_cast<std::uint32_t>(declaration.size() + 1u) << 16u) | declaration.front(), id};
        instruction.insert(instruction.end(), declaration.begin() + 1, declaration.end());
        types.emplace(std::move(declaration), id);
        declarations.push_back(std::move(instruction));
        return id;
    };
    const auto wordType = retyped.empty() ? 0u : declare({spv::OpTypeInt, 32u, 0u});
    const auto vectorType = retyped.empty() ? 0u : declare({spv::OpTypeVector, wordType, 4u});
    const auto pointerType = retyped.empty() ? 0u : declare({spv::OpTypePointer, spv::StorageClassOutput, vectorType});
    std::vector<std::uint32_t> result(words.begin(), words.begin() + 5);
    std::vector<std::vector<std::uint32_t>> variables;
    bool inserted = false;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto op = static_cast<spv::Op>(words[cursor] & 0xffffu);
        const auto operand = [&](std::size_t index) { return index < count ? words[cursor + index] : 0u; };
        if (op == spv::OpVariable && retyped.contains(operand(2u))) {
            if (count != 4u) throw std::runtime_error("a packed fragment output has an initializer");
            std::vector<std::uint32_t> variable(words.begin() + cursor, words.begin() + cursor + count);
            variable[1] = pointerType;
            variables.push_back(std::move(variable));
            cursor += count;
            continue;
        }
        if (op == spv::OpDecorate && count == 4u && secondSource != 0u && operand(1u) == secondSource && operand(2u) == spv::DecorationLocation) {
            result.insert(result.end(), {(4u << 16u) | spv::OpDecorate, secondSource, spv::DecorationLocation, 0u, (4u << 16u) | spv::OpDecorate, secondSource, spv::DecorationIndex, 1u});
            cursor += count;
            continue;
        }
        if (op == spv::OpFunction && !inserted) {
            for (const auto& declaration : declarations) result.insert(result.end(), declaration.begin(), declaration.end());
            for (const auto& variable : variables) result.insert(result.end(), variable.begin(), variable.end());
            inserted = true;
        }
        if (op == spv::OpStore && count >= 3u && retyped.contains(operand(1u))) {
            if (bound == std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("fragment output specialization module ID overflow");
            const auto bits = bound++;
            result.insert(result.end(), {(4u << 16u) | spv::OpBitcast, vectorType, bits, operand(2u)});
            result.insert(result.end(), words.begin() + cursor, words.begin() + cursor + count);
            result[result.size() - count + 2u] = bits;
            cursor += count;
            continue;
        }
        const bool pointerUse = ((op == spv::OpLoad || op == spv::OpAccessChain || op == spv::OpInBoundsAccessChain || op == spv::OpPtrAccessChain || op == spv::OpInBoundsPtrAccessChain || op == spv::OpCopyObject) && retyped.contains(operand(3u))) ||
            ((op == spv::OpCopyMemory || op == spv::OpCopyMemorySized) && (retyped.contains(operand(1u)) || retyped.contains(operand(2u))));
        if (pointerUse) throw std::runtime_error("a packed fragment output is accessed other than by whole stores");
        result.insert(result.end(), words.begin() + cursor, words.begin() + cursor + count);
        cursor += count;
    }
    if (!inserted) throw std::runtime_error("fragment output specialization module has no function");
    result[3] = bound;
    return result;
}

}

#endif
