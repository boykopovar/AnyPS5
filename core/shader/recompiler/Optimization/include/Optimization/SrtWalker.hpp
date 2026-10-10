#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_SRTWALKER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_SRTWALKER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

using SrtMemoryReader = bool (*)(void* userData, std::uint64_t address, std::uint32_t* value);
using SrtMemoryProbe = bool (*)(void* userData, std::uint64_t address, std::uint64_t bytes);

// The guest addresses a walk dereferenced (Detail::Evaluator::EvaluateRawRead): the leaf read of
// each pure flat slot (IrResourcePlan::pureFlatSlots) as (flat offset, address), set by the
// flattened loop through `leaf`/`leafSlot` around that slot's evaluation, and every other raw read
// address.
struct SrtReadTrace {
    const IrValue* leaf = nullptr;
    std::uint32_t leafSlot = 0;
    std::vector<std::pair<std::uint32_t, std::uint64_t>> leaves;
    std::vector<std::uint64_t> otherReads;
};

struct SrtRuntime {
    std::span<const std::uint32_t> userData;
    std::uint64_t shaderBase = 0;
    SrtMemoryReader readMemory = nullptr;
    void* userContext = nullptr;
    SrtMemoryReader readSpecializationMemory = nullptr;
    SrtReadTrace* readTrace = nullptr;
    SrtMemoryProbe accessible = nullptr;
};

enum class RuntimeValueType {
    Any,
    Integer
};

class SrtWalker {
public:
    void BuildPlan(IrProgram& program) const;
    [[nodiscard]] bool ValidateRuntimeValue(const IrResourcePlan& program, const IrValue* value, RuntimeValueType type = RuntimeValueType::Any) const;
    void EvaluateUniformValues(const IrResourcePlan& program, std::span<IrValue* const> values, const SrtRuntime& runtime, std::span<std::uint32_t> results) const;
    void EvaluateDescriptorSource(const IrResourcePlan& program, std::uint32_t source, const SrtRuntime& runtime, DescriptorValue& result) const;
    void EvaluateDescriptorSources(const IrResourcePlan& program, std::span<const std::uint32_t> sources, const SrtRuntime& runtime, std::vector<DescriptorValue>& results) const;
    void EvaluateRuntimeSources(const IrResourcePlan& program, std::span<const std::uint32_t> sources, const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flat, std::span<const std::uint8_t> cleanFlatSlots, std::vector<std::uint8_t>& activeSources, std::vector<SrtReadPoison>* poison = nullptr) const;
    void Walk(const IrResourcePlan& program, const SrtRuntime& runtime, std::vector<std::uint32_t>& flat) const;

};

}

#endif
