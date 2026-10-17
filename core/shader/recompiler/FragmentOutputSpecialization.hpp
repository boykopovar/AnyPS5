#ifndef CORE_SHADER_RECOMPILER_FRAGMENTOUTPUTSPECIALIZATION_HPP
#define CORE_SHADER_RECOMPILER_FRAGMENTOUTPUTSPECIALIZATION_HPP

#include "PipelineSpecialization.hpp"
#include "Recompiler.hpp"
#include <array>
#include <initializer_list>
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
    namespace Code = ReversedBlendCode;
    std::set<std::uint32_t> packed;
    bool dualSource = false;
    std::uint32_t reversed = 0;
    for (const auto& constant : constants) {
        if (constant.id == PipelineSpecialization::DualSourceBlend) {
            if (constant.value > 1u) throw std::runtime_error("invalid prepared dual-source blend flag");
            dualSource = constant.value != 0u;
        }
        if (constant.id == PipelineSpecialization::ReversedBlend) {
            const auto known = Code::Enable | (Code::Routes << Code::ColorShift) | (Code::Routes << Code::SecondShift);
            if ((constant.value & ~known) != 0u || (constant.value != 0u && (constant.value & Code::Enable) == 0u)) throw std::runtime_error("invalid prepared reversed-order blend");
            reversed = constant.value;
        }
        if (constant.id < PipelineSpecialization::ExportPackingBase || constant.id >= PipelineSpecialization::ExportPackingBase + 8u) continue;
        if (constant.value > static_cast<std::uint32_t>(ColorExportPacking::Float11_11_10)) throw std::runtime_error("invalid prepared fragment export packing");
        if (constant.value != static_cast<std::uint32_t>(ColorExportPacking::None)) packed.insert(constant.id - PipelineSpecialization::ExportPackingBase);
    }
    if (packed.empty() && !dualSource && reversed == 0u) return words;
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
        if (!specializedIds.contains(PipelineSpecialization::ExportPackingBase + location)) throw std::runtime_error("fragment output " + std::to_string(location) + " has no color export packing path");
        if (!floatVectorPointer(outputs.at(variable))) throw std::runtime_error("packed fragment output " + std::to_string(location) + " is not a float4");
        retyped.insert(variable);
    }
    const auto secondSource = dualSource ? output(1u) : 0u;
    if (retyped.contains(secondSource)) throw std::runtime_error("the second dual-source blend color cannot be packed");
    std::uint32_t blended = 0;
    if (reversed != 0u) {
        if (dualSource) throw std::runtime_error("reversed-order blending cannot be combined with dual-source blending");
        blended = output(0u);
        if (blended == 0u || retyped.contains(blended) || !floatVectorPointer(outputs.at(blended))) throw std::runtime_error("reversed-order blending needs a float4 MRT0 output");
        for (const auto& [id, pointer] : outputs) {
            if (id != blended && locations.contains(id)) throw std::runtime_error("reversed-order blending needs MRT0 to be the only color output");
        }
    }
    if (retyped.empty() && secondSource == 0u && blended == 0u) return words;
    auto bound = words[3];
    const auto allocate = [&] {
        if (bound == std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("fragment output specialization module ID overflow");
        return bound++;
    };
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
    const auto colorType = blended == 0u ? 0u : key(outputs.at(blended))[2];
    const auto floatType = blended == 0u ? 0u : key(colorType)[1];
    const auto zero = blended == 0u ? 0u : allocate();
    const auto one = blended == 0u ? 0u : allocate();
    const auto secondColor = blended == 0u ? 0u : allocate();
    std::vector<std::uint32_t> result(words.begin(), words.begin() + 5);
    std::vector<std::vector<std::uint32_t>> variables;
    if (blended != 0u) {
        declarations.push_back({(4u << 16u) | spv::OpConstant, floatType, zero, 0u});
        declarations.push_back({(4u << 16u) | spv::OpConstant, floatType, one, 0x3f800000u});
        variables.push_back({(4u << 16u) | spv::OpVariable, outputs.at(blended), secondColor, spv::StorageClassOutput});
    }
    const auto emit = [&](spv::Op opcode, std::initializer_list<std::uint32_t> operands) {
        result.push_back((static_cast<std::uint32_t>(operands.size() + 1u) << 16u) | opcode);
        result.insert(result.end(), operands);
    };
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
        if (op == spv::OpDecorate && count == 4u && blended != 0u && operand(1u) == blended && operand(2u) == spv::DecorationLocation) {
            result.insert(result.end(), words.begin() + cursor, words.begin() + cursor + count);
            result.insert(result.end(), {(4u << 16u) | spv::OpDecorate, secondColor, spv::DecorationLocation, 0u, (4u << 16u) | spv::OpDecorate, secondColor, spv::DecorationIndex, 1u});
            cursor += count;
            continue;
        }
        if (op == spv::OpEntryPoint && secondColor != 0u) {
            result.push_back(((count + 1u) << 16u) | spv::OpEntryPoint);
            result.insert(result.end(), words.begin() + cursor + 1u, words.begin() + cursor + count);
            result.push_back(secondColor);
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
        if (op == spv::OpStore && count >= 3u && blended != 0u && operand(1u) == blended) {
            std::array<std::uint32_t, 4> source{};
            for (std::uint32_t component = 0; component < source.size(); ++component) {
                source[component] = allocate();
                emit(spv::OpCompositeExtract, {floatType, source[component], operand(2u), component});
            }
            const auto routed = [&](std::uint32_t shift) {
                std::array<std::uint32_t, 4> picked{};
                for (std::uint32_t component = 0; component < picked.size(); ++component) {
                    switch ((reversed >> (shift + Code::RouteBits * component)) & Code::RouteMask) {
                        case Code::Own: picked[component] = source[component]; break;
                        case Code::Component0: picked[component] = source[0]; break;
                        case Code::Zero: picked[component] = zero; break;
                        default: picked[component] = one; break;
                    }
                }
                const auto value = allocate();
                emit(spv::OpCompositeConstruct, {colorType, value, picked[0], picked[1], picked[2], picked[3]});
                return value;
            };
            emit(spv::OpStore, {secondColor, routed(Code::SecondShift)});
            const auto stored = ((reversed >> Code::ColorShift) & Code::Routes) == 0u ? operand(2u) : routed(Code::ColorShift);
            result.insert(result.end(), words.begin() + cursor, words.begin() + cursor + count);
            result[result.size() - count + 2u] = stored;
            cursor += count;
            continue;
        }
        const auto pointerUse = [&](auto&& matches) {
            return ((op == spv::OpLoad || op == spv::OpAccessChain || op == spv::OpInBoundsAccessChain || op == spv::OpPtrAccessChain || op == spv::OpInBoundsPtrAccessChain || op == spv::OpCopyObject) && matches(operand(3u))) ||
                ((op == spv::OpCopyMemory || op == spv::OpCopyMemorySized) && (matches(operand(1u)) || matches(operand(2u))));
        };
        if (pointerUse([&](std::uint32_t id) { return retyped.contains(id); })) throw std::runtime_error("a packed fragment output is accessed other than by whole stores");
        if (blended != 0u && pointerUse([&](std::uint32_t id) { return id == blended; })) throw std::runtime_error("a reversed-order blended fragment output is accessed other than by whole stores");
        result.insert(result.end(), words.begin() + cursor, words.begin() + cursor + count);
        cursor += count;
    }
    if (!inserted) throw std::runtime_error("fragment output specialization module has no function");
    result[3] = bound;
    return result;
}

}

#endif
