#ifndef CORE_SHADER_RECOMPILIER_CONTROLFLOW_INCLUDE_CONTROLFLOW_STRUCTURIZER_HPP
#define CORE_SHADER_RECOMPILIER_CONTROLFLOW_INCLUDE_CONTROLFLOW_STRUCTURIZER_HPP

#include "ControlFlow/ControlFlowGraph.hpp"

namespace ShaderRecompiler {

class Structurizer {
public:
    void Structurize(ControlFlowGraph& graph) const;

private:
    void structurize(ControlFlowGraph& graph, bool privatizeReturns) const;
    void privatizeSharedReturns(ControlFlowGraph& graph) const;
    void computeDominatorTree(ControlFlowGraph& graph) const;
    void detectNaturalLoops(ControlFlowGraph& graph) const;
    void computePostDominators(ControlFlowGraph& graph) const;
    void computeBackEdges(ControlFlowGraph& graph) const;
    void computeStronglyConnectedComponents(ControlFlowGraph& graph) const;
    void recomputeAnalyses(ControlFlowGraph& graph) const;
    void canonicalizeNaturalLoops(ControlFlowGraph& graph) const;
    void splitSharedMergeBlocks(ControlFlowGraph& graph) const;
    void isolateSemanticLoopHeaders(ControlFlowGraph& graph) const;
    void orderByDominance(ControlFlowGraph& graph) const;
    void clearStructuredTerminators(ControlFlowGraph& graph) const;
    void verifyReducibility(const ControlFlowGraph& graph) const;
};

}

#endif
