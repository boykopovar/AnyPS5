#ifndef CORE_SHADER_RECOMPILIER_CONTROLFLOW_INCLUDE_CONTROLFLOW_GRAPHBUILDER_HPP
#define CORE_SHADER_RECOMPILIER_CONTROLFLOW_INCLUDE_CONTROLFLOW_GRAPHBUILDER_HPP

#include "ControlFlow/ControlFlowGraph.hpp"
#include "RdnaDecoder/RdnaProgram.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace ShaderRecompiler {

struct SwappcInfo {
    bool fetchCallAllowed = false;
    std::uint32_t userDataBaseRegister = 0;
    std::uint32_t userDataCount = 0;
};

struct SwappcCall {
    std::uint32_t callIndex = 0;
    std::uint32_t linkRegister = 0;
    bool fetch = false;
    std::uint32_t targetIndex = 0;
    std::uint32_t targetProgramCounter = 0;
    std::uint32_t returnIndex = 0;
    std::uint32_t returnTargetProgramCounter = 0;
};

[[nodiscard]] std::optional<std::uint32_t> UnresolvableSwappcTarget(const RdnaProgram& program, const SwappcInfo* swappc);

class GraphBuilder {
public:
    [[nodiscard]] ControlFlowGraph Build(const RdnaProgram& program, const SwappcInfo* swappc = nullptr) const;

private:
    [[nodiscard]] std::vector<BasicBlock> splitIntoBlocks(const RdnaProgram& program, const std::vector<SwappcCall>& calls) const;
    void linkBlocks(std::vector<BasicBlock>& blocks, const RdnaProgram& program, const std::vector<SwappcCall>& calls) const;
};

}

#endif
