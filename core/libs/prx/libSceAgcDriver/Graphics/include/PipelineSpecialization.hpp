#ifndef CORE_LIBS_AGCDRIVER_GRAPHICS_PIPELINESPECIALIZATION_HPP
#define CORE_LIBS_AGCDRIVER_GRAPHICS_PIPELINESPECIALIZATION_HPP

#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <spirv/unified1/spirv.hpp>
#include <vector>

namespace AgcDriver::Graphics {

class PipelineSpecialization {
public:
    static void Validate(const ShaderRecompiler::RecompileResult& shader) {
        const auto& words = shader.spirv;
        Require(words.size() >= 5 && words[0] == spv::MagicNumber && words[3] != 0, "invalid specialization module header");
        std::set<std::uint32_t> unsignedTypes;
        std::map<std::uint32_t, std::uint32_t> constants;
        std::map<std::uint32_t, std::uint32_t> decorations;
        std::set<std::uint32_t> constantIds;
        std::set<std::uint32_t> supplied;
        for (const auto& constant : shader.specialization) Require(supplied.insert(constant.id).second, "duplicate pipeline specialization ID");
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            const auto op = static_cast<spv::Op>(words[cursor] & 0xffffu);
            Require(count != 0 && count <= words.size() - cursor, "truncated specialization module instruction");
            const auto validId = [&](std::uint32_t id) { Require(id != 0 && id < words[3], "specialization ID exceeds module bound"); };
            if (op == spv::OpTypeInt) {
                Require(count == 4, "malformed integer type");
                if (words[cursor + 2] == 32u && words[cursor + 3] == 0u) unsignedTypes.insert(words[cursor + 1]);
            } else if (op == spv::OpSpecConstant) {
                Require(count == 4, "specialization constant must have a 32-bit literal");
                validId(words[cursor + 2]);
                Require(constants.emplace(words[cursor + 2], words[cursor + 1]).second, "duplicate specialization result ID");
            } else if (op == spv::OpDecorate && count >= 3 && words[cursor + 2] == spv::DecorationSpecId) {
                Require(count == 4, "malformed SpecId decoration");
                validId(words[cursor + 1]);
                Require(decorations.emplace(words[cursor + 1], words[cursor + 3]).second && constantIds.insert(words[cursor + 3]).second, "duplicate SpecId decoration");
            } else if (op == spv::OpSpecConstantTrue || op == spv::OpSpecConstantFalse || op == spv::OpSpecConstantComposite || op == spv::OpSpecConstantOp) {
                throw std::runtime_error("AGC graphics: unsupported specialization constant representation");
            }
            cursor += count;
        }
        Require(constants.size() == decorations.size(), "specialization declarations and SpecId decorations disagree");
        for (const auto& [id, type] : constants) {
            Require(unsignedTypes.contains(type), "specialization constant must be uint32");
            const auto decoration = decorations.find(id);
            Require(decoration != decorations.end() && supplied.contains(decoration->second), "required pipeline specialization value is missing");
        }
    }

    explicit PipelineSpecialization(const ShaderRecompiler::RecompileResult& shader, std::uint32_t layerBase = 0u, std::uint32_t layerCount = 0xffffffffu) {
        values = shader.specialization;
        for (auto& constant : values) {
            if (constant.id == ShaderRecompiler::PipelineSpecialization::MeshLayerBase) constant.value = layerBase;
            if (constant.id == ShaderRecompiler::PipelineSpecialization::MeshLayerCount) constant.value = layerCount;
        }
        Validate(shader);
        describe(values);
    }

    PipelineSpecialization(const ShaderRecompiler::RecompileResult& shader, std::span<const ShaderRecompiler::PipelineSpecializationConstant> replaced) : values(shader.specialization) {
        Validate(shader);
        for (const auto& constant : replaced) {
            const auto found = std::ranges::find(values, constant.id, &ShaderRecompiler::PipelineSpecializationConstant::id);
            Require(found != values.end(), "a replaced pipeline specialization ID is not supplied by the shader");
            found->value = constant.value;
        }
        describe(values);
    }

    PipelineSpecialization(const PipelineSpecialization&) = delete;
    PipelineSpecialization& operator=(const PipelineSpecialization&) = delete;
    PipelineSpecialization(PipelineSpecialization&&) noexcept = default;
    PipelineSpecialization& operator=(PipelineSpecialization&&) noexcept = default;

    [[nodiscard]] const VkSpecializationInfo* Info() const { return entries.empty() ? nullptr : &info; }

private:
    void describe(std::span<const ShaderRecompiler::PipelineSpecializationConstant> constants) {
        entries.reserve(constants.size());
        for (std::size_t index = 0; index < constants.size(); ++index) {
            const auto offset = index * sizeof(ShaderRecompiler::PipelineSpecializationConstant) + offsetof(ShaderRecompiler::PipelineSpecializationConstant, value);
            Require(offset <= std::numeric_limits<std::uint32_t>::max(), "pipeline specialization offset exceeds Vulkan limits");
            entries.push_back({constants[index].id, static_cast<std::uint32_t>(offset), sizeof(std::uint32_t)});
        }
        info.mapEntryCount = static_cast<std::uint32_t>(entries.size());
        info.pMapEntries = entries.data();
        info.dataSize = constants.size() * sizeof(ShaderRecompiler::PipelineSpecializationConstant);
        info.pData = constants.data();
    }

    std::vector<ShaderRecompiler::PipelineSpecializationConstant> values;
    std::vector<VkSpecializationMapEntry> entries;
    VkSpecializationInfo info{};
};

}

#endif
