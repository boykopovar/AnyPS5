#ifndef CORE_SHADER_RECOMPILIER_CONTROLFLOW_INCLUDE_CONTROLFLOW_GRAPHBUILDER_HPP
#define CORE_SHADER_RECOMPILIER_CONTROLFLOW_INCLUDE_CONTROLFLOW_GRAPHBUILDER_HPP

#include "ControlFlow/ControlFlowGraph.hpp"
#include "RdnaDecoder/RdnaProgram.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ShaderRecompiler {

struct UserDataTail {
    std::uint32_t programCounter;
    std::uint32_t userDataRegister;
    std::optional<std::uint32_t> constantHigh;
};

[[nodiscard]] std::optional<UserDataTail> FindUserDataTail(const RdnaProgram& program, std::uint32_t userDataBase, std::uint32_t userDataCount);
[[nodiscard]] std::vector<std::uint32_t> LinkUserDataTail(const RdnaProgram& program, const UserDataTail& tail, const RdnaProgram& continuation);

struct SwappcInfo {
    bool fetchCallAllowed = false;
    std::uint32_t userDataBaseRegister = 0;
    std::uint32_t userDataCount = 0;
    std::uint32_t waveSize = 0;
    std::span<const std::uint32_t> linkedCallFallbacks;
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

struct UserDataCall {
    std::uint32_t programCounter = 0;
    std::uint32_t linkRegister = 0;
    std::uint32_t userDataRegister = 0;
};

struct TableCall {
    std::uint32_t programCounter = 0;
    std::uint32_t linkRegister = 0;
    std::uint32_t targetRegister = 0;
    std::uint32_t userDataRegister = 0;
    std::uint32_t descriptorOffset = 0;
    std::uint32_t entryOffset = 0;
    std::uint32_t entryStride = 0;
    std::optional<std::uint64_t> constantPointer;
};

struct TableCallee {
    std::uint64_t target = 0;
    std::vector<std::uint32_t> code;
};

[[nodiscard]] std::optional<std::uint32_t> UnresolvableSwappcTarget(const RdnaProgram& program, const SwappcInfo* swappc);
[[nodiscard]] std::vector<UserDataCall> FindUserDataCalls(const RdnaProgram& program, const SwappcInfo& swappc);
[[nodiscard]] std::optional<std::uint32_t> UserDataCalleeLength(std::span<const std::uint32_t> callee, std::uint32_t linkRegister);
[[nodiscard]] std::vector<std::uint32_t> LinkUserDataCalls(const RdnaProgram& program, std::span<const UserDataCall> calls, std::span<const std::vector<std::uint32_t>> callees);
[[nodiscard]] std::vector<TableCall> FindTableCalls(const RdnaProgram& program, const SwappcInfo& swappc);
struct LinkedCalls {
    std::vector<std::uint32_t> code;
    std::vector<std::uint32_t> fallbacks;
};

[[nodiscard]] LinkedCalls LinkCalls(const RdnaProgram& program, std::span<const UserDataCall> userDataCalls, std::span<const std::vector<std::uint32_t>> userDataCallees,
    std::span<const TableCall> tableCalls, std::span<const std::vector<TableCallee>> tableCallees);

class GraphBuilder {
public:
    [[nodiscard]] ControlFlowGraph Build(const RdnaProgram& program, const SwappcInfo* swappc = nullptr) const;

private:
    [[nodiscard]] std::vector<BasicBlock> splitIntoBlocks(const RdnaProgram& program, const std::vector<SwappcCall>& calls) const;
    void linkBlocks(std::vector<BasicBlock>& blocks, const RdnaProgram& program, const std::vector<SwappcCall>& calls) const;
};

}

#endif
