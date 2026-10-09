#ifndef CORE_SHADER_RECOMPILIER_CONTROLFLOW_INCLUDE_CONTROLFLOW_GRAPHBUILDER_HPP
#define CORE_SHADER_RECOMPILIER_CONTROLFLOW_INCLUDE_CONTROLFLOW_GRAPHBUILDER_HPP

#include "ControlFlow/ControlFlowGraph.hpp"
#include "RdnaDecoder/RdnaProgram.hpp"
#include "Recompiler.hpp"
#include <cstdint>
#include <vector>

namespace ShaderRecompiler {

struct SwappcInfo {
    bool fetchCallAllowed = false;
    std::uint32_t userDataBaseRegister = 0;
    std::uint32_t userDataCount = 0;
    std::span<const CapturedShaderCall> capturedCalls{};
};

struct SwappcCall {
    std::uint32_t callIndex = 0;
    std::uint32_t linkRegister = 0;
    bool fetch = false;
    std::uint32_t targetIndex = 0;
    std::uint32_t targetProgramCounter = 0;
    std::uint32_t returnIndex = 0;
    std::uint32_t returnTargetProgramCounter = 0;
    bool captured = false;
};

struct UserDataCall {
    std::uint32_t callIndex;
    std::uint32_t userDataIndex;
};

[[nodiscard]] std::vector<UserDataCall> AnalyzeUserDataCalls(const RdnaProgram& program, const SwappcInfo& swappc);
[[nodiscard]] bool WritesScalarRegister(const RdnaInstruction& instruction, std::uint32_t index);

class GraphBuilder {
public:
    [[nodiscard]] ControlFlowGraph Build(const RdnaProgram& program, const SwappcInfo* swappc = nullptr) const;

private:
    [[nodiscard]] std::vector<BasicBlock> splitIntoBlocks(const RdnaProgram& program, const std::vector<SwappcCall>& calls) const;
    void linkBlocks(std::vector<BasicBlock>& blocks, const RdnaProgram& program, const std::vector<SwappcCall>& calls) const;
};

}

#endif
