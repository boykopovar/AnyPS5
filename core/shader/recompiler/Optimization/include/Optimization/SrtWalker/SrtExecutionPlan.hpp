#ifndef CORE_SHADER_RECOMPILER_OPTIMIZATION_SRTWALKER_SRTEXECUTIONPLAN_HPP
#define CORE_SHADER_RECOMPILER_OPTIMIZATION_SRTWALKER_SRTEXECUTIONPLAN_HPP

#include "Optimization/SrtWalker.hpp"
#include <array>
#include <memory>
#include <vector>

namespace ShaderRecompiler::Detail {

struct SrtExecutionPlan {
    struct Operation {
        IrOpcode opcode = IrOpcode::Void;
        std::array<std::uint32_t, 5> arguments{};
        std::uint64_t immediate = 0;
        std::uint32_t leafSlot = ~0u;
    };
    struct Descriptor {
        std::array<std::uint32_t, 8> words{};
        std::uint32_t count = 0;
    };

    std::vector<Operation> operations;
    std::vector<Descriptor> descriptors;
    std::vector<std::uint32_t> flat;
    std::vector<std::uint8_t> active;

    bool Evaluate(const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flattened, std::vector<std::uint8_t>& activeSources) const;
};

std::shared_ptr<const SrtExecutionPlan> CompileSrtExecutionPlan(const IrResourcePlan& program);

}

#endif
