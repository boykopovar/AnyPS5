#ifndef CORE_SHADER_RECOMPILIER_CONTROLFLOW_INCLUDE_CONTROLFLOW_CONTROLFLOWGRAPH_HPP
#define CORE_SHADER_RECOMPILIER_CONTROLFLOW_INCLUDE_CONTROLFLOW_CONTROLFLOWGRAPH_HPP

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace ShaderRecompiler {

inline constexpr std::uint32_t InvalidControlFlowId = std::numeric_limits<std::uint32_t>::max();

enum class BranchCondition {
    Always,
    SccZero,
    SccNonZero,
    VccZero,
    VccNonZero,
    ExecZero,
    ExecNonZero,
    ScalarInstruction,
    IndirectTarget,
    GotoVariable,
    Unknown
};

enum class TerminatorKind {
    Branch,
    ConditionalBranch,
    IndirectBranch,
    Return,
    Unsupported
};

enum class FailureKind {
    None,
    InvalidInput,
    UnsupportedInstruction,
    InvalidBranchTarget,
    MissingFallthrough,
    InvalidLabel,
    IrreducibleControlFlow,
    StructuredControlFlow
};

struct Terminator {
    TerminatorKind kind = TerminatorKind::Return;
    BranchCondition condition = BranchCondition::Always;
    std::uint32_t trueBlock = InvalidControlFlowId;
    std::uint32_t falseBlock = InvalidControlFlowId;
    std::uint32_t mergeBlock = InvalidControlFlowId;
    std::uint32_t continueBlock = InvalidControlFlowId;
    std::uint32_t indirectPcSgpr = InvalidControlFlowId;
    std::uint32_t indirectSelectorCode = InvalidControlFlowId;
    std::vector<std::uint32_t> indirectTargetProgramCounters;
    std::vector<std::uint32_t> indirectTargets;
    std::vector<std::uint32_t> indirectSelectorValues;
    std::vector<std::uint32_t> indirectSelectorTargets;
    std::uint32_t gotoVariable = InvalidControlFlowId;
    std::int32_t gotoValue = -1;
    bool loopHeader = false;
};

struct BasicBlock {
    std::uint32_t id = 0;
    std::uint32_t startProgramCounter = 0;
    std::uint32_t endProgramCounter = 0;
    std::uint32_t instructionBegin = 0;
    std::uint32_t instructionEnd = 0;
    std::uint32_t estimatedSpirvWords = 0;
    std::vector<std::uint32_t> predecessors;
    std::vector<std::uint32_t> successors;
    std::vector<std::uint32_t> dominators;
    std::vector<std::uint32_t> postDominators;
    Terminator terminator;
};

struct NaturalLoop {
    std::uint32_t headerBlock = InvalidControlFlowId;
    std::uint32_t latchBlock = InvalidControlFlowId;
    std::uint32_t mergeBlock = InvalidControlFlowId;
    std::uint32_t continueBlock = InvalidControlFlowId;
    std::vector<std::uint32_t> blocks;
    std::vector<std::uint32_t> exitBlocks;
};

struct BackEdge {
    std::uint32_t sourceBlock = InvalidControlFlowId;
    std::uint32_t targetBlock = InvalidControlFlowId;
    bool natural = false;
};

struct StronglyConnectedComponent {
    std::vector<std::uint32_t> blocks;
    std::vector<std::uint32_t> entryBlocks;
    bool irreducible = false;
};

struct ControlFlowGraph {
    struct CodeTableLoad {
        std::uint32_t programCounter = 0;
        std::vector<std::uint64_t> values;
    };
    std::vector<BasicBlock> blocks;
    std::vector<NaturalLoop> naturalLoops;
    std::vector<BackEdge> backEdges;
    std::vector<StronglyConnectedComponent> components;
    std::vector<std::uint32_t> codeTableLoadProgramCounters;
    std::vector<CodeTableLoad> codeTableLoads;
    std::uint32_t entryBlock = InvalidControlFlowId;
    bool hasFetchCall = false;
    std::uint32_t fetchCallProgramCounter = 0;
    bool irreducible = false;
    bool unsupported = false;
    FailureKind failureKind = FailureKind::None;
    std::uint32_t failureBlock = InvalidControlFlowId;
    std::string unsupportedReason;

    [[nodiscard]] const BasicBlock& FindBlock(std::uint32_t blockId) const;
    [[nodiscard]] BasicBlock& FindBlock(std::uint32_t blockId);
    [[nodiscard]] const BasicBlock& FindBlockByProgramCounter(std::uint32_t programCounter) const;
    [[nodiscard]] BasicBlock& FindBlockByProgramCounter(std::uint32_t programCounter);
    [[nodiscard]] bool Dominates(std::uint32_t dominator, std::uint32_t blockId) const;
    [[nodiscard]] bool PostDominates(std::uint32_t postDominator, std::uint32_t blockId) const;
    [[nodiscard]] std::uint32_t FindNearestCommonPostDominator(std::uint32_t firstBlock, std::uint32_t secondBlock) const;
};

[[nodiscard]] std::string BranchConditionToString(BranchCondition condition);
[[nodiscard]] std::string FailureKindToString(FailureKind kind);
[[nodiscard]] std::string GraphToString(const ControlFlowGraph& graph);

}

#endif
