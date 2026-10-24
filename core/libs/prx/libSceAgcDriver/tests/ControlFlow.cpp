#include "ControlFlow/GraphBuilder.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "ControlFlow/Structurizer.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Recompiler.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <map>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

using namespace ShaderRecompiler;

enum class Split {
    None,
    Clone,
    Route
};

struct Program {
    std::string_view name;
    std::string_view source;
    std::vector<std::uint32_t> code;
    Split split;
    std::size_t clonedLimit = 0;
    std::vector<std::uint32_t> reference = {};
};

using RouteState = std::map<std::uint32_t, bool>;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

class StructuralDominators {
public:
    explicit StructuralDominators(const ControlFlowGraph& graph) : dominators(graph.blocks.size()) {
        std::vector<std::vector<std::uint32_t>> predecessors(graph.blocks.size());
        for (const auto& block : graph.blocks) {
            for (const auto successor : block.successors) predecessors[successor].push_back(block.id);
            const auto& terminator = block.terminator;
            if (terminator.mergeBlock != InvalidControlFlowId) predecessors[terminator.mergeBlock].push_back(block.id);
            if (terminator.loopHeader && terminator.continueBlock != InvalidControlFlowId) predecessors[terminator.continueBlock].push_back(block.id);
        }

        std::vector<std::uint32_t> all;
        all.reserve(graph.blocks.size());
        for (const auto& block : graph.blocks) all.push_back(block.id);
        for (const auto& block : graph.blocks) dominators[block.id] = block.id == graph.entryBlock ? std::vector<std::uint32_t>{block.id} : all;

        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& block : graph.blocks) {
                if (block.id == graph.entryBlock) continue;
                auto next = predecessors[block.id].empty() ? std::vector<std::uint32_t>{} : dominators[predecessors[block.id].front()];
                for (std::size_t i = 1; i < predecessors[block.id].size(); ++i) {
                    std::vector<std::uint32_t> intersection;
                    const auto& other = dominators[predecessors[block.id][i]];
                    std::set_intersection(next.begin(), next.end(), other.begin(), other.end(), std::back_inserter(intersection));
                    next = std::move(intersection);
                }
                if (!std::binary_search(next.begin(), next.end(), block.id)) {
                    next.insert(std::lower_bound(next.begin(), next.end(), block.id), block.id);
                }
                if (next != dominators[block.id]) {
                    dominators[block.id] = std::move(next);
                    changed = true;
                }
            }
        }
    }

    bool Dominates(std::uint32_t dominator, std::uint32_t block) const {
        const auto& values = dominators.at(block);
        return std::binary_search(values.begin(), values.end(), dominator);
    }

private:
    std::vector<std::vector<std::uint32_t>> dominators;
};

std::vector<std::uint32_t> constructBlocks(const ControlFlowGraph& graph, const StructuralDominators& dominators, std::uint32_t header, std::uint32_t exclude) {
    std::vector<std::uint32_t> blocks;
    for (const auto& block : graph.blocks) {
        if (dominators.Dominates(header, block.id) && (exclude == InvalidControlFlowId || !dominators.Dominates(exclude, block.id))) {
            blocks.push_back(block.id);
        }
    }
    return blocks;
}

bool inEnclosingContinueConstruct(const ControlFlowGraph& graph, const StructuralDominators& dominators, std::uint32_t header, std::uint32_t member) {
    return std::any_of(graph.blocks.begin(), graph.blocks.end(), [&](const BasicBlock& loop) {
        const auto continueBlock = loop.terminator.continueBlock;
        return loop.terminator.loopHeader && dominators.Dominates(loop.id, header) && !dominators.Dominates(continueBlock, header) && dominators.Dominates(continueBlock, member);
    });
}

void verifyStructured(const std::string& prefix, const ControlFlowGraph& graph) {
    const StructuralDominators dominators(graph);
    std::map<std::uint32_t, std::uint32_t> mergeOwners;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> loopExits;
    for (const auto& block : graph.blocks) {
        const auto& terminator = block.terminator;
        if (terminator.mergeBlock == InvalidControlFlowId) continue;
        require(mergeOwners.emplace(terminator.mergeBlock, block.id).second, prefix + "block " + std::to_string(terminator.mergeBlock) + " merges two constructs");
        if (terminator.loopHeader) loopExits.emplace_back(terminator.mergeBlock, terminator.continueBlock);
    }
    for (const auto& block : graph.blocks) {
        const auto& terminator = block.terminator;
        if (terminator.loopHeader || terminator.mergeBlock == InvalidControlFlowId) continue;
        for (const auto member : constructBlocks(graph, dominators, block.id, terminator.mergeBlock)) {
            if (inEnclosingContinueConstruct(graph, dominators, block.id, member)) continue;
            for (const auto successor : graph.FindBlock(member).successors) {
                if (successor == terminator.mergeBlock || (dominators.Dominates(block.id, successor) && !dominators.Dominates(terminator.mergeBlock, successor))) continue;
                const bool loopExit = std::any_of(loopExits.begin(), loopExits.end(), [&](const auto& exits) { return successor == exits.first || successor == exits.second; });
                require(loopExit, prefix + "block " + std::to_string(member) + " of the selection at block " + std::to_string(block.id) + " branches to block " + std::to_string(successor) + " outside the construct");
            }
        }
    }
    for (const auto& loop : graph.blocks) {
        if (!loop.terminator.loopHeader) continue;
        const auto continueBlock = loop.terminator.continueBlock;
        for (const auto member : constructBlocks(graph, dominators, loop.id, loop.terminator.mergeBlock)) {
            const auto& terminator = graph.FindBlock(member).terminator;
            if (terminator.loopHeader || dominators.Dominates(continueBlock, member)) continue;
            require(terminator.mergeBlock != continueBlock, prefix + "the selection at block " + std::to_string(member) + " in the loop at block " + std::to_string(loop.id) + " merges at the loop's continue block " + std::to_string(continueBlock));
        }
    }
}

ControlFlowGraph makeGraph(const std::vector<std::vector<std::uint32_t>>& successors) {
    ControlFlowGraph graph;
    graph.entryBlock = 0;
    for (std::uint32_t id = 0; id < successors.size(); ++id) {
        BasicBlock block;
        block.id = id;
        block.startProgramCounter = id * 8;
        block.endProgramCounter = id * 8 + 8;
        block.instructionBegin = id * 2;
        block.instructionEnd = id * 2 + 2;
        block.successors = successors[id];
        auto& terminator = block.terminator;
        if (successors[id].empty()) {
            terminator.kind = TerminatorKind::Return;
        } else if (successors[id].size() == 1) {
            terminator.kind = TerminatorKind::Branch;
            terminator.trueBlock = successors[id][0];
        } else {
            terminator.kind = TerminatorKind::ConditionalBranch;
            terminator.condition = BranchCondition::SccNonZero;
            terminator.trueBlock = successors[id][0];
            terminator.falseBlock = successors[id][1];
        }
        graph.blocks.push_back(std::move(block));
    }
    for (const auto& block : graph.blocks) {
        for (const auto successor : block.successors) graph.blocks[successor].predecessors.push_back(block.id);
    }
    return graph;
}

void verifyLoopMergeDoesNotEnterNestedSelection() {
    auto graph = makeGraph({{1, 5}, {2, 3}, {5}, {5, 4}, {1}, {}});
    Structurizer{}.Structurize(graph);
    verifyStructured("loop merge structural dominance: ", graph);
}

std::uint32_t followEmptyBlocks(const std::string& prefix, const ControlFlowGraph& graph, std::uint32_t blockId, RouteState& routes) {
    for (std::size_t steps = 0; steps <= graph.blocks.size(); ++steps) {
        const auto& block = graph.FindBlock(blockId);
        const auto& terminator = block.terminator;
        const bool empty = block.instructionBegin == block.instructionEnd;
        require(terminator.gotoValue < 0 || empty, prefix + "block " + std::to_string(block.id) + " sets a route variable after guest instructions");
        if (terminator.gotoValue >= 0) routes[terminator.gotoVariable] = terminator.gotoValue != 0;
        if (!empty) return blockId;
        if (terminator.kind == TerminatorKind::Branch) {
            blockId = terminator.trueBlock;
        } else if (terminator.kind == TerminatorKind::ConditionalBranch && terminator.condition == BranchCondition::GotoVariable) {
            const auto route = routes.find(terminator.gotoVariable);
            require(route != routes.end(), prefix + "block " + std::to_string(block.id) + " reads route variable " + std::to_string(terminator.gotoVariable) + " before any path sets it");
            blockId = route->second ? terminator.trueBlock : terminator.falseBlock;
        } else {
            return blockId;
        }
    }
    throw std::runtime_error(prefix + "a cycle of empty blocks");
}

void verifySameExecutions(const std::string& prefix, const ControlFlowGraph& original, const ControlFlowGraph& structured) {
    std::vector<std::tuple<std::uint32_t, std::uint32_t, RouteState>> pending{{structured.blocks.front().id, original.entryBlock, {}}};
    std::vector<std::tuple<std::uint32_t, std::uint32_t, RouteState>> visited;
    RouteState none;
    while (!pending.empty()) {
        auto [structuredId, originalId, routes] = pending.back();
        pending.pop_back();
        const auto copyId = followEmptyBlocks(prefix, structured, structuredId, routes);
        const auto sourceId = followEmptyBlocks(prefix, original, originalId, none);
        auto state = std::make_tuple(copyId, sourceId, routes);
        if (std::find(visited.begin(), visited.end(), state) != visited.end()) continue;
        visited.push_back(state);
        const auto& copy = structured.FindBlock(copyId);
        const auto& source = original.FindBlock(sourceId);
        require(copy.instructionBegin == source.instructionBegin && copy.instructionEnd == source.instructionEnd, prefix + "block " + std::to_string(copy.id) + " runs other instructions than block " + std::to_string(source.id));
        require(copy.terminator.kind == source.terminator.kind && copy.terminator.condition == source.terminator.condition, prefix + "block " + std::to_string(copy.id) + " branches differently from block " + std::to_string(source.id));
        if (source.terminator.kind == TerminatorKind::Branch || source.terminator.kind == TerminatorKind::ConditionalBranch) pending.emplace_back(copy.terminator.trueBlock, source.terminator.trueBlock, routes);
        if (source.terminator.kind == TerminatorKind::ConditionalBranch) pending.emplace_back(copy.terminator.falseBlock, source.terminator.falseBlock, routes);
    }
}

std::size_t clonedInstructions(const ControlFlowGraph& graph) {
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::size_t> copies;
    std::size_t cloned = 0;
    for (const auto& block : graph.blocks) {
        if (block.instructionBegin != block.instructionEnd && copies[{block.instructionBegin, block.instructionEnd}]++ != 0) cloned += block.instructionEnd - block.instructionBegin;
    }
    return cloned;
}

std::size_t routeVariables(const ControlFlowGraph& graph) {
    std::vector<std::uint32_t> variables;
    for (const auto& block : graph.blocks) {
        if (block.terminator.condition == BranchCondition::GotoVariable && std::find(variables.begin(), variables.end(), block.terminator.gotoVariable) == variables.end()) variables.push_back(block.terminator.gotoVariable);
    }
    return variables.size();
}

struct Structured {
    std::size_t addedBlocks;
    std::size_t clonedInstructions;
    std::size_t routeVariables;
};

Structured verifyGraph(const std::string& name, std::span<const std::uint32_t> code) {
    const std::string prefix = name + ": ";
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    const auto original = GraphBuilder{}.Build(decoded);
    auto graph = original;
    Structurizer{}.Structurize(graph);
    verifyStructured(prefix, graph);
    verifySameExecutions(prefix, original, graph);
    return {graph.blocks.size() - original.blocks.size(), clonedInstructions(graph), routeVariables(graph)};
}

std::vector<std::uint32_t> Store(std::vector<std::uint32_t> code) {
    code.insert(code.end(), {0xe0700000u, 0x80000100u, 0xbf810000u});
    return code;
}

void verifyRequest(const char* path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    const auto request = RequestSerializer{}.Deserialize(text.str());
    const auto result = verifyGraph(path, request.request.shader.code);
    std::printf("%s: structured with %zu added blocks, %zu cloned instructions, %zu route variables\n", path, result.addedBlocks, result.clonedInstructions, result.routeVariables);
}

std::size_t recompile(const std::string& name, std::span<const std::uint32_t> code, std::uint32_t subgroupSize, std::uint32_t waveSize = 64u) {
    const std::array<std::uint32_t, 4> userData{0x10000000u, 0x00100000u, 0x40u, 0x00027facu};
    const std::array<std::uint32_t, 2> capabilities{1u, 61u};
    const std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = waveSize;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010600u;
    request.target.subgroupSize = subgroupSize;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 49152;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    const auto result = Recompile(request);
    require(!result.spirv.empty(), name + ": no SPIR-V for a " + std::to_string(subgroupSize) + "-lane subgroup");
    return result.spirv.size();
}

void verifyCallLinkOverwrittenByWave64Compare() {
    const std::vector<std::uint32_t> code{0xbb080007u, 0xd4060008u, 0x0001e500u, 0xd5010001u, 0x0021e880u, 0xe0700000u, 0x80010100u, 0xbf810000u, 0xbe802008u, 0xbf9f0000u};
    static_cast<void>(recompile("a wave64 compare that overwrites a call's link pair before a v_cndmask reads it", code, 64u));
}

void verifyCallLinkMaskReadByScalarInstruction() {
    const std::array<std::uint32_t, 3> readers{0xbe8a2408u, 0xbe8a0408u, 0xbe8a0309u};
    for (const auto reader : readers) {
        const std::vector<std::uint32_t> code{0xbb080007u, 0xd4060008u, 0x0001e500u, reader, 0xbf800000u, 0xe0700000u, 0x80010100u, 0xbf810000u, 0xbe802008u, 0xbf9f0000u};
        static_cast<void>(recompile("a wave64 scalar read of a compare mask in a call's link pair", code, 64u));
        bool refused = false;
        try {
            static_cast<void>(recompile("a wave32 scalar read of a call's return address", code, 32u, 32u));
        } catch (const std::exception& error) {
            refused = std::string_view(error.what()).find("escapes the constant-offset call/return model") != std::string_view::npos;
        }
        require(refused, "a wave32 program reading a call's return address high half was not refused");
    }
}

void verifyLargeLoopExitFallback() {
    const std::vector<std::uint32_t> code{
        0x060202ffu, 0x3f800000u, 0x060202ffu, 0x3f000000u, 0x060202ffu, 0x3f001000u, 0x060202ffu, 0x3f002000u,
        0x060202ffu, 0x3f003000u, 0x060202ffu, 0x3f004000u, 0x060202ffu, 0x3f005000u, 0x060202ffu, 0x3f006000u,
        0x060202ffu, 0x3f007000u, 0x060202ffu, 0x3f008000u, 0x060202ffu, 0x3f009000u, 0x060202ffu, 0x3f00a000u,
        0x060202ffu, 0x3f00b000u, 0x060202ffu, 0x3f00c000u, 0x060202ffu, 0x3f00d000u, 0x060202ffu, 0x3f00e000u,
        0x060202ffu, 0x3f00f000u, 0x060202ffu, 0x3f010000u, 0x060202ffu, 0x3f810000u, 0x060202ffu, 0x3f000010u,
        0x060202ffu, 0x3f001010u, 0x060202ffu, 0x3f002010u, 0x060202ffu, 0x3f003010u, 0x060202ffu, 0x3f004010u,
        0x060202ffu, 0x3f005010u, 0x060202ffu, 0x3f006010u, 0x060202ffu, 0x3f007010u, 0x060202ffu, 0x3f008010u,
        0x060202ffu, 0x3f009010u, 0x060202ffu, 0x3f00a010u, 0x060202ffu, 0x3f00b010u, 0x060202ffu, 0x3f00c010u,
        0x060202ffu, 0x3f00d010u, 0x060202ffu, 0x3f00e010u, 0x060202ffu, 0x3f00f010u, 0x060202ffu, 0x3f010010u,
        0x7d880084u, 0xbf870025u, 0x060202ffu, 0x3f830000u, 0x060202ffu, 0x3f000030u, 0x060202ffu, 0x3f001030u,
        0x060202ffu, 0x3f002030u, 0x060202ffu, 0x3f003030u, 0x060202ffu, 0x3f004030u, 0x060202ffu, 0x3f005030u,
        0x060202ffu, 0x3f006030u, 0x060202ffu, 0x3f007030u, 0x060202ffu, 0x3f008030u, 0x060202ffu, 0x3f009030u,
        0x060202ffu, 0x3f00a030u, 0x060202ffu, 0x3f00b030u, 0x060202ffu, 0x3f00c030u, 0x060202ffu, 0x3f00d030u,
        0x060202ffu, 0x3f00e030u, 0x060202ffu, 0x3f00f030u, 0x060202ffu, 0x3f010030u, 0xbf820099u, 0x060202ffu,
        0x3f820000u, 0x060202ffu, 0x3f000020u, 0x060202ffu, 0x3f001020u, 0x060202ffu, 0x3f002020u, 0x060202ffu,
        0x3f003020u, 0x060202ffu, 0x3f004020u, 0x060202ffu, 0x3f005020u, 0x060202ffu, 0x3f006020u, 0x060202ffu,
        0x3f007020u, 0x060202ffu, 0x3f008020u, 0x060202ffu, 0x3f009020u, 0x060202ffu, 0x3f00a020u, 0x060202ffu,
        0x3f00b020u, 0x060202ffu, 0x3f00c020u, 0x060202ffu, 0x3f00d020u, 0x060202ffu, 0x3f00e020u, 0x060202ffu,
        0x3f00f020u, 0x060202ffu, 0x3f010020u, 0x7d880085u, 0xbf87004cu, 0x060202ffu, 0x3f850000u, 0x060202ffu,
        0x3f000050u, 0x060202ffu, 0x3f001050u, 0x060202ffu, 0x3f002050u, 0x060202ffu, 0x3f003050u, 0x060202ffu,
        0x3f004050u, 0x060202ffu, 0x3f005050u, 0x060202ffu, 0x3f006050u, 0x060202ffu, 0x3f007050u, 0x060202ffu,
        0x3f008050u, 0x060202ffu, 0x3f009050u, 0x060202ffu, 0x3f00a050u, 0x060202ffu, 0x3f00b050u, 0x060202ffu,
        0x3f00c050u, 0x060202ffu, 0x3f00d050u, 0x060202ffu, 0x3f00e050u, 0x060202ffu, 0x3f00f050u, 0x060202ffu,
        0x3f010050u, 0x7d880088u, 0xbf870001u, 0xbf82ff68u, 0x060202ffu, 0x3f870000u, 0x060202ffu, 0x3f000070u,
        0x060202ffu, 0x3f001070u, 0x060202ffu, 0x3f002070u, 0x060202ffu, 0x3f003070u, 0x060202ffu, 0x3f004070u,
        0x060202ffu, 0x3f005070u, 0x060202ffu, 0x3f006070u, 0x060202ffu, 0x3f007070u, 0x060202ffu, 0x3f008070u,
        0x060202ffu, 0x3f009070u, 0x060202ffu, 0x3f00a070u, 0x060202ffu, 0x3f00b070u, 0x060202ffu, 0x3f00c070u,
        0x060202ffu, 0x3f00d070u, 0x060202ffu, 0x3f00e070u, 0x060202ffu, 0x3f00f070u, 0x060202ffu, 0x3f010070u,
        0xbf820027u, 0x060202ffu, 0x3f840000u, 0x060202ffu, 0x3f000040u, 0x060202ffu, 0x3f001040u, 0x060202ffu,
        0x3f002040u, 0x060202ffu, 0x3f003040u, 0x060202ffu, 0x3f004040u, 0x060202ffu, 0x3f005040u, 0x060202ffu,
        0x3f006040u, 0x060202ffu, 0x3f007040u, 0x060202ffu, 0x3f008040u, 0x060202ffu, 0x3f009040u, 0x060202ffu,
        0x3f00a040u, 0x060202ffu, 0x3f00b040u, 0x060202ffu, 0x3f00c040u, 0x060202ffu, 0x3f00d040u, 0x060202ffu,
        0x3f00e040u, 0x060202ffu, 0x3f00f040u, 0x060202ffu, 0x3f010040u, 0x7d880087u, 0xbf87ff43u, 0xbf82ff8du,
        0x060202ffu, 0x3f860000u, 0x060202ffu, 0x3f000060u, 0x060202ffu, 0x3f001060u, 0x060202ffu, 0x3f002060u,
        0x060202ffu, 0x3f003060u, 0x060202ffu, 0x3f004060u, 0x060202ffu, 0x3f005060u, 0x060202ffu, 0x3f006060u,
        0x060202ffu, 0x3f007060u, 0x060202ffu, 0x3f008060u, 0x060202ffu, 0x3f009060u, 0x060202ffu, 0x3f00a060u,
        0x060202ffu, 0x3f00b060u, 0x060202ffu, 0x3f00c060u, 0x060202ffu, 0x3f00d060u, 0x060202ffu, 0x3f00e060u,
        0x060202ffu, 0x3f00f060u, 0x060202ffu, 0x3f010060u, 0xe0700000u, 0x80010100u, 0xbf810000u,
    };
    const std::string name("large loop-exit shader preserves the original guarded fallback");
    static_cast<void>(verifyGraph(name, code));
    static_cast<void>(recompile(name, code, 32u));
    static_cast<void>(recompile(name, code, 64u));
}

std::vector<std::uint32_t> branchProgram(std::span<const std::array<std::uint32_t, 2>> targets, std::span<const std::uint32_t> order) {
    constexpr auto none = InvalidControlFlowId;
    std::vector<std::uint32_t> code;
    std::vector<std::size_t> starts(targets.size());
    std::vector<std::pair<std::size_t, std::uint32_t>> patches;
    for (std::size_t at = 0; at < order.size(); ++at) {
        const auto block = order[at];
        starts[block] = code.size();
        code.insert(code.end(), {0x060202ffu, 0x3f800000u + block * 0x10000u});
        for (std::uint32_t padding = 0; padding < 17; ++padding) code.insert(code.end(), {0x060202ffu, 0x3f000000u + padding * 0x1000u + block * 0x10u});
        const auto next = at + 1 < order.size() ? order[at + 1] : none;
        const auto [first, second] = targets[block];
        if (first == none) {
            code.insert(code.end(), {0xe0700000u, 0x80010100u, 0xbf810000u});
            continue;
        }
        if (second != none) {
            code.push_back(0x7d880080u + block + 3u);
            patches.emplace_back(code.size(), first);
            code.push_back(0xbf870000u);
        }
        const auto branch = second != none ? second : first;
        if (branch != next) {
            patches.emplace_back(code.size(), branch);
            code.push_back(0xbf820000u);
        }
    }
    for (const auto& [instruction, target] : patches) {
        const auto displacement = static_cast<std::int64_t>(starts[target]) - static_cast<std::int64_t>(instruction) - 1;
        require(displacement >= -32768 && displacement <= 32767, "a generated test branch is out of range");
        code[instruction] |= static_cast<std::uint32_t>(displacement) & 0xffffu;
    }
    return code;
}

void verifyNestedExitForwarders() {
    constexpr auto none = InvalidControlFlowId;
    constexpr std::array<std::array<std::uint32_t, 2>, 21> targets{{
        {1, none}, {2, none}, {2, 3}, {2, 4}, {5, 6}, {1, 7}, {8, 9}, {10, none},
        {11, 12}, {13, 14}, {1, 15}, {16, none}, {9, none}, {17, 18}, {19, none},
        {16, none}, {none, none}, {16, none}, {15, 20}, {10, none}, {19, none},
    }};
    constexpr std::array<std::uint32_t, 21> order{0, 1, 2, 3, 4, 6, 9, 14, 19, 10, 15, 13, 18, 20, 17, 8, 12, 11, 5, 7, 16};
    const auto code = branchProgram(targets, order);
    const std::string name("nested exit forwarders keep conditional selections structured");
    static_cast<void>(verifyGraph(name, code));
    static_cast<void>(recompile(name, code, 32u));
    static_cast<void>(recompile(name, code, 64u));
}

void verifyIrreducibleLoopEntry() {
    constexpr auto none = InvalidControlFlowId;
    constexpr std::array<std::array<std::uint32_t, 2>, 4> targets{{{1, 2}, {2, 3}, {1, 3}, {none, none}}};
    constexpr std::array<std::uint32_t, 4> order{0, 1, 2, 3};
    const auto code = branchProgram(targets, order);
    const std::string name("a loop entered at either of its two blocks runs behind reach flags");
    const auto result = verifyGraph(name, code);
    require(result.clonedInstructions == 0 && result.routeVariables != 0, name + ": expected the reach flag fallback, got " + std::to_string(result.clonedInstructions) + " cloned instructions and " + std::to_string(result.routeVariables) + " route variables");
    static_cast<void>(recompile(name, code, 32u));
    static_cast<void>(recompile(name, code, 64u));
}

void verifyProgram(const Program& program) {
    const std::string name(program.name);
    const auto result = verifyGraph(name, program.code);
    if (std::getenv("APS5_FORCE_STRUCTURIZER_FALLBACK") != nullptr) {
        require(result.clonedInstructions == 0 && result.routeVariables != 0, name + ": expected the reach flag fallback, got " + std::to_string(result.clonedInstructions) + " cloned instructions and " + std::to_string(result.routeVariables) + " route variables");
    } else if (program.split == Split::None) {
        require(result.clonedInstructions == 0 && result.routeVariables == 0, name + ": expected no clone or routing, got " + std::to_string(result.clonedInstructions) + " cloned instructions and " + std::to_string(result.routeVariables) + " route variables");
    } else if (program.split == Split::Clone) {
        require(result.clonedInstructions != 0 && result.routeVariables == 0, name + ": expected a clone, got " + std::to_string(result.clonedInstructions) + " cloned instructions and " + std::to_string(result.routeVariables) + " route variables");
    } else {
        require(result.clonedInstructions <= program.clonedLimit && result.routeVariables != 0, name + ": expected routing with at most " + std::to_string(program.clonedLimit) + " cloned instructions, got " + std::to_string(result.clonedInstructions) + " cloned instructions and " + std::to_string(result.routeVariables) + " route variables");
    }
    const auto words = recompile(name, program.code, 64u);
    static_cast<void>(recompile(name, program.code, 32u));
    if (!program.reference.empty()) {
        const auto reference = recompile(name, program.reference, 64u);
        require(words * 10u <= reference * 11u, name + ": " + std::to_string(words) + " SPIR-V words, more than 10% over the " + std::to_string(reference) + " of the program without the entering branch");
    }
}

const std::vector<std::uint32_t> DwordTableGetpcFirst{
    0xbe8c1f00u, 0x800cff0cu, 0x00000058u, 0x820d800du, 0x83928202u, 0xd4c40016u, 0x02000500u, 0x8f128212u, 0xf4000386u, 0x24000000u,
    0xbf8cc07fu, 0x7d820003u, 0x808c0e0cu, 0x828d800du, 0xbefd210cu, 0x7e020281u, 0xbf820003u, 0x7e020282u, 0xbf820001u, 0x7e020283u,
    0xe0700000u, 0x80000100u, 0xbf810000u, 0x00000020u, 0x00000018u, 0x00000010u};
const std::vector<std::uint32_t> DwordTableIndexFirst{
    0x816ac102u, 0x83ea826au, 0x7e020280u, 0x8f6a826au, 0xbe8e1f00u, 0x800ebc0eu, 0x820f800fu, 0xf4000487u, 0xd4000008u, 0x8aea167eu,
    0xbf8cc07fu, 0x808e120eu, 0x828f800fu, 0xbefd210eu, 0x7e020281u, 0xbf820003u, 0x7e020282u, 0xbf820001u, 0x7e020283u, 0xe0700000u,
    0x80000100u, 0xbf810000u, 0x00000018u, 0x00000010u, 0x00000008u};
const std::vector<std::uint32_t> LongBranchVcc{
    0x7e020281u, 0xbeea1f00u, 0x806a906au, 0x826b806bu, 0xbefd216au, 0x7e020282u, 0xe0700000u, 0x80000100u, 0xbf810000u};
const std::vector<std::uint32_t> LongBranchBack{
    0xbe880380u, 0x7e020280u, 0x4a020281u, 0x80089008u, 0xbf0ac008u, 0xbf840004u, 0xbe901f00u, 0x80909410u, 0x82918011u, 0xbefd2110u,
    0xe0700000u, 0x80000100u, 0xbf810000u};

std::vector<std::uint32_t> patched(std::vector<std::uint32_t> code, std::size_t word, std::initializer_list<std::uint32_t> replacement) {
    std::copy(replacement.begin(), replacement.end(), code.begin() + static_cast<std::ptrdiff_t>(word));
    return code;
}

std::string refusal(std::span<const std::uint32_t> code) {
    try {
        static_cast<void>(GraphBuilder{}.Build(RdnaInstructionDecoder{}.Decode(code)));
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

void verifyJumpTable(const std::string& name, std::span<const std::uint32_t> code, std::uint32_t loadPc, const std::vector<std::uint64_t>& values, const std::vector<std::uint32_t>& targets) {
    const auto graph = GraphBuilder{}.Build(RdnaInstructionDecoder{}.Decode(code));
    require(graph.codeTableLoads.size() == 1u && graph.codeTableLoads.front().programCounter == loadPc && graph.codeTableLoads.front().values == values, name + ": wrong code table");
    std::vector<std::uint32_t> lowered;
    for (const auto& block : graph.blocks) {
        if (block.terminator.indirectPcSgpr != InvalidControlFlowId) lowered.insert(lowered.end(), block.terminator.indirectTargetProgramCounters.begin(), block.terminator.indirectTargetProgramCounters.end());
    }
    std::sort(lowered.begin(), lowered.end());
    require(lowered == targets, name + ": the jump does not reach exactly the table targets");
}

void verifyLongBranch(const std::string& name, std::span<const std::uint32_t> code, std::uint32_t branchPc, std::uint32_t target) {
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    const auto graph = GraphBuilder{}.Build(decoded);
    const auto source = std::find_if(graph.blocks.begin(), graph.blocks.end(), [&](const BasicBlock& block) { return block.instructionEnd != block.instructionBegin && decoded.instructions[block.instructionEnd - 1u].programCounter == branchPc; });
    require(source != graph.blocks.end() && source->terminator.kind == TerminatorKind::Branch && graph.FindBlock(source->terminator.trueBlock).startProgramCounter == target, name + ": the jump does not branch to " + std::to_string(target));
}

void verifyScheduledJumpTable() {
    const std::vector<std::uint32_t> code{
        0x83888208u, 0x7e020280u, 0x8f088308u, 0x7e040280u, 0xbe8c1f00u, 0x800cff0cu, 0x00000048u,
        0x820d800du, 0xf4040386u, 0x10000000u, 0xbf8cc07fu, 0xbe901f00u, 0x80100e10u, 0x82110f11u,
        0xbe802010u, 0x7e020281u, 0xbf820003u, 0x7e020282u, 0xbf820001u, 0x7e020283u, 0xe0700000u,
        0x80000100u, 0xbf810000u, 0x0000000cu, 0u, 0x00000014u, 0u, 0x0000001cu, 0u,
    };
    verifyJumpTable("scheduled two-dword table", code, 0x20u, {0xcu, 0x14u, 0x1cu}, {0x3cu, 0x44u, 0x4cu});
    static_cast<void>(recompile("scheduled two-dword table", code, 32u));
    static_cast<void>(recompile("scheduled two-dword table", code, 64u));
    auto vcc = patched(code, 0, {0x83ea826au});
    vcc = patched(std::move(vcc), 2, {0x8f6a836au});
    vcc = patched(std::move(vcc), 9, {0xd4000000u});
    verifyJumpTable("scheduled two-dword table through vcc", vcc, 0x20u, {0xcu, 0x14u, 0x1cu}, {0x3cu, 0x44u, 0x4cu});
    static_cast<void>(recompile("scheduled two-dword table through vcc", vcc, 32u));
    static_cast<void>(recompile("scheduled two-dword table through vcc", vcc, 64u));
    const std::vector<std::vector<std::uint32_t>> refused{
        patched(code, 1, {0xbe880380u}),
        patched(code, 3, {0xbe880380u}),
        patched(code, 1, {0xbf820001u}),
        patched(code, 3, {0xbf850000u}),
    };
    for (const auto& rejected : refused) require(refusal(rejected).find("unsupported dynamic s_setpc_b64") != std::string::npos, "a rewritten or conditional two-dword table index was accepted");
}

void verifyNullSwappc() {
    const std::array<std::uint32_t, 2> jump{0xbefd210cu, 0xbf810000u};
    const auto swappc = RdnaInstructionDecoder{}.Decode(jump).instructions.front();
    require(swappc.op == RdnaOpcode::SSetpcB64 && swappc.opcodeId == 0x21u && swappc.destination.kind == RdnaOperandKind::Null &&
        swappc.source0.kind == RdnaOperandKind::ScalarRegister && swappc.source0.reg == 12u, "s_swappc_b64 null, s[12:13] does not decode as s_setpc_b64 s[12:13]");
    const std::array<std::uint32_t, 3> front{0x7e020281u, 0xbefd2106u, 0xbf810000u};
    require(DecodeRdnaFrontProgram(front).code.size() == 2u, "a front program ending in s_swappc_b64 null, s[6:7] does not end there");
    verifyJumpTable("dword jump table after its base", DwordTableGetpcFirst, 0x20u, {0x20u, 0x18u, 0x10u}, {0x3cu, 0x44u, 0x4cu});
    verifyJumpTable("dword jump table after its index", DwordTableIndexFirst, 0x1cu, {0x18u, 0x10u, 0x08u}, {0x38u, 0x40u, 0x48u});
    verifyLongBranch("long branch through vcc", LongBranchVcc, 0x10u, 0x18u);
    verifyLongBranch("backward long branch", LongBranchBack, 0x24u, 0x08u);
    const std::array<std::uint32_t, 3> call{0xbe8c1f00u, 0xbe8e210cu, 0xbf810000u};
    const std::vector<std::tuple<std::string_view, std::vector<std::uint32_t>, std::string_view>> refused{
        {"s_swappc_b64 with a return address", {call.begin(), call.end()}, "unclosable scalar call/return pairing"},
        {"index rewritten after the bound", patched(DwordTableGetpcFirst, 5, {0xbe9203ffu, 0x00000040u}), "unsupported dynamic s_setpc_b64"},
        {"index scaled for two-dword entries", patched(DwordTableGetpcFirst, 7, {0x8f128312u}), "unsupported dynamic s_setpc_b64"},
        {"entry rewritten after the load", patched(DwordTableGetpcFirst, 11, {0xbe8e0380u}), "unsupported dynamic s_setpc_b64"},
        {"base rewritten after the load", patched(DwordTableGetpcFirst, 11, {0xbe8d0380u}), "unsupported dynamic s_setpc_b64"},
        {"borrow other than zero", patched(DwordTableGetpcFirst, 13, {0x828d810du}), "unsupported dynamic s_setpc_b64"},
        {"long branch with a register high half", patched(LongBranchVcc, 3, {0x826b046bu}), "unsupported dynamic s_setpc_b64"},
        {"branch into a long branch", patched(LongBranchVcc, 0, {0xbf850001u}), "does not start in its block"},
    };
    for (const auto& [name, code, message] : refused) {
        const auto error = refusal(code);
        require(error.find(message) != std::string::npos, std::string(name) + ": expected a refusal with '" + std::string(message) + "', got '" + error + "'");
    }
}

}

void VerifyUserDataTail() {
    const RdnaInstructionDecoder decoder;
    const std::vector<std::uint32_t> code{0x7e000280u, 0xbe802008u, 0xbf9f0000u};
    const auto decoded = decoder.Decode(code);
    const auto tail = FindUserDataTail(decoded, 8u, 2u);
    require(tail && tail->programCounter == 4u && tail->userDataRegister == 8u, "unchanged user-data tail was not recognized");
    require(!FindUserDataTail(decoded, 9u, 2u) && !FindUserDataTail(decoded, 8u, 1u), "tail outside user data was accepted");
    const std::vector<std::vector<std::uint32_t>> refused{
        {0xbe880380u, 0xbe802008u, 0xbf9f0000u},
        {0xbe890300u, 0xbe802008u, 0xbf9f0000u},
        {0xbe880480u, 0xbe802008u, 0xbf9f0000u},
        {0xf4080200u, 0xfa000000u, 0xbe802008u, 0xbf9f0000u},
        {0xbf820000u, 0xbe802008u, 0xbf9f0000u},
        {0xbe802009u, 0xbf9f0000u},
        {0xbe802008u, 0xbf810000u},
        {0xbe802008u, 0xbf800000u, 0xbf9f0000u},
    };
    for (const auto& rejected : refused) require(!FindUserDataTail(decoder.Decode(rejected), 8u, 4u), "a modified, branching or malformed tail was accepted");
    const std::vector<std::uint32_t> continuation{0xbf820001u, 0x7e000281u, 0xbf810000u};
    const auto linked = LinkUserDataTail(decoded, *tail, decoder.Decode(continuation));
    require(linked == std::vector<std::uint32_t>({0x7e000280u, 0xbf820000u, 0xbf820001u, 0x7e000281u, 0xbf810000u}), "linking did not preserve relative branches and register operations");
    static_cast<void>(GraphBuilder{}.Build(decoder.Decode(linked)));
    const std::vector<std::uint32_t> readsPc{0xbe8c1f00u, 0xbf810000u};
    bool refusedPc = false;
    try {
        static_cast<void>(LinkUserDataTail(decoded, *tail, decoder.Decode(readsPc)));
    } catch (const std::invalid_argument& error) {
        refusedPc = std::string_view(error.what()).find("reads its program counter") != std::string_view::npos;
    }
    require(refusedPc, "a continuation reading its relocated PC was accepted");
}

void VerifyConstantHighTail() {
    const RdnaInstructionDecoder decoder;
    const std::vector<std::vector<std::uint32_t>> accepted{
        {0xbe850382u, 0xbe802004u, 0xbf9f0000u},
        {0xbe8503ffu, 0x12345678u, 0xbe802004u, 0xbf9f0000u},
        {0xbe850380u, 0xbe802004u, 0xbf9f0000u},
        {0xbe850300u, 0xbe850382u, 0xbe802004u, 0xbf9f0000u},
    };
    const std::array<std::uint32_t, 4> values{2u, 0x12345678u, 0u, 2u};
    const std::vector<std::uint32_t> end{0xbf810000u};
    for (std::size_t index = 0; index < accepted.size(); ++index) {
        const auto program = decoder.Decode(accepted[index]);
        const auto tail = FindUserDataTail(program, 0u, 5u);
        require(tail && tail->userDataRegister == 4u && tail->constantHigh == values[index], "constant high half was not resolved without a sixth user SGPR");
        const auto linked = LinkUserDataTail(program, *tail, decoder.Decode(end));
        require(std::equal(accepted[index].begin(), accepted[index].end() - 2u, linked.begin()), "constant high tail changed preceding scalar operations");
        auto mismatched = *tail;
        mismatched.constantHigh = values[index] ^ 1u;
        bool rejected = false;
        try {
            static_cast<void>(LinkUserDataTail(program, mismatched, decoder.Decode(end)));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, "linker accepted a stale high-half proof");
    }
    const std::vector<std::vector<std::uint32_t>> refused{
        {0xbe850382u, 0xbe850300u, 0xbe802004u, 0xbf9f0000u},
        {0xbe850382u, 0x80058105u, 0xbe802004u, 0xbf9f0000u},
        {0xbe850382u, 0xbe840380u, 0xbe802004u, 0xbf9f0000u},
        {0xbe850382u, 0xf4000140u, 0xfa000000u, 0xbe802004u, 0xbf9f0000u},
        {0xbf840001u, 0xbe850382u, 0xbe802004u, 0xbf9f0000u},
    };
    for (const auto& code : refused) require(!FindUserDataTail(decoder.Decode(code), 0u, 6u), "unknown, overwritten or conditional tail address was accepted");
    require(!FindUserDataTail(decoder.Decode(accepted.front()), 0u, 4u), "tail low half outside user data was accepted");
}

int main(int argc, char** argv) {
    try {
        verifyLoopMergeDoesNotEnterNestedSelection();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    if (argc > 1) {
        int failures = 0;
        for (int i = 1; i < argc; ++i) {
            try {
                verifyRequest(argv[i]);
            } catch (const std::exception& error) {
                std::fprintf(stderr, "%s: %s\n", argv[i], error.what());
                ++failures;
            }
        }
        return failures == 0 ? 0 : 1;
    }
    const std::vector<Program> programs{
        {"nested selections share a loop continuation", "Synthetic loop with two nested selections and a shared continue; the inner selection does not dominate the continuation.",
         {0x060202ffu, 0x3f800000u, 0x060202ffu, 0x3f810000u, 0x7d880084u, 0xbf870001u, 0xbf820018u, 0x060202ffu, 0x3f820000u, 0x7d880085u, 0xbf87000cu, 0x060202ffu, 0x3f850000u, 0x7d880088u, 0xbf870003u, 0x060202ffu, 0x3f870000u, 0xbf82fff0u, 0x060202ffu, 0x3f860000u, 0x7d880089u, 0xbf87fff9u, 0xbf820008u, 0x060202ffu, 0x3f830000u, 0x7d880086u, 0xbf870001u, 0xbf820003u, 0x060202ffu, 0x3f840000u, 0xbf82fff0u, 0x060202ffu, 0x3f880000u, 0xe0700000u, 0x80000100u, 0xbf810000u}, Split::None},
        {"reduced shared exit junction from the public CFG fuzz report", "Public reduced word stream from #2762; synthetic regression, not title data.",
         {0x060202f2u, 0x060202f4u, 0x7d880084u, 0xbf870001u, 0xbf820011u, 0x060202ffu, 0x3f820000u, 0x7d880085u, 0xbf870005u,
          0x060202ffu, 0x3f850000u, 0x7d880088u, 0xbf870009u, 0xbf82fff3u, 0x060202ffu, 0x3f840000u, 0x7d880087u, 0xbf870001u,
          0xbf820003u, 0x060202ffu, 0x3f860000u, 0xbf82ffebu, 0x060202f6u, 0xe0700000u, 0x80010100u, 0xbf810000u}, Split::None},
        {"both selection arms leave through a shared loop exit tail", R"(
  v_mov_b32 v1, 0
  s_mov_b32 s8, 0
loop:
  s_cmp_lt_u32 s8, 4
  s_cbranch_scc0 done
  s_cmp_eq_u32 s8, s2
  s_cbranch_scc1 right
  v_cmp_gt_u32 vcc, 8, v0
  s_cbranch_vccz tail
  v_add_nc_u32 v1, 1, v1
  s_branch latch
right:
  v_cmp_gt_u32 vcc, 16, v0
  s_cbranch_vccz tail
  v_add_nc_u32 v1, 2, v1
latch:
  s_add_u32 s8, s8, 1
  s_branch loop
tail:
  v_add_nc_u32 v1, 3, v1
done:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         Store({0x7e020280u, 0xbe880380u, 0xbf0a8408u, 0xbf84000cu, 0xbf060208u, 0xbf850004u, 0x7d880088u, 0xbf860007u, 0x4a020281u,
                0xbf820003u, 0x7d880090u, 0xbf860003u, 0x4a020282u, 0x80088108u, 0xbf82fff3u, 0x4a020283u}), Split::None},
        {"shared tail", R"(
  v_cmp_gt_u32 vcc, 16, v0
  s_cbranch_vccz outer_else
  v_cmp_gt_u32 vcc, 8, v0
  s_cbranch_vccnz tail
  v_add_nc_u32 v1, 1, v0
  s_branch done
outer_else:
  v_add_nc_u32 v1, 2, v0
tail:
  v_add_nc_u32 v1, 3, v1
done:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         Store({0x7d880090u, 0xbf860004u, 0x7d880088u, 0xbf870003u, 0x4a020081u, 0xbf820002u, 0x4a020082u, 0x4a020283u}), Split::Clone},
        {"shared tail with a selection", R"(
  v_cmp_gt_u32 vcc, 16, v0
  s_cbranch_vccz outer_else
  v_cmp_gt_u32 vcc, 8, v0
  s_cbranch_vccnz tail
  v_add_nc_u32 v1, 1, v0
  s_branch done
outer_else:
  v_add_nc_u32 v1, 2, v0
tail:
  v_cmp_gt_u32 vcc, 4, v0
  s_cbranch_vccz tail_join
  v_add_nc_u32 v1, 5, v1
tail_join:
  v_add_nc_u32 v1, 3, v1
done:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         Store({0x7d880090u, 0xbf860004u, 0x7d880088u, 0xbf870003u, 0x4a020081u, 0xbf820005u, 0x4a020082u, 0x7d880084u, 0xbf860001u, 0x4a020285u, 0x4a020283u}), Split::Clone},
        {"memory access in a single-block loop latch", R"(
  s_mov_b32 s8, 0
  v_mov_b32 v1, 0
loop:
  buffer_store_dword v1, off, s[0:3], 0
  s_add_u32 s8, s8, 1
  s_cmp_lt_u32 s8, 4
  s_cbranch_scc1 loop
  s_endpgm)",
         {0xbe880380u, 0x7e020280u, 0xe0700000u, 0x80000100u, 0x80088108u, 0xbf0a8408u, 0xbf85fffbu, 0xbf810000u}, Split::None},
        {"shared tail in a loop", R"(
  s_mov_b32 s8, 0
  v_mov_b32 v1, 0
loop:
  v_cmp_gt_u32 vcc, s8, v0
  s_cbranch_vccz outer_else
  v_cmp_gt_u32 vcc, 8, v0
  s_cbranch_vccnz tail
  v_add_nc_u32 v1, 1, v1
  s_branch latch
outer_else:
  v_add_nc_u32 v1, 2, v1
tail:
  v_add_nc_u32 v1, 3, v1
latch:
  s_add_u32 s8, s8, 16
  s_cmp_lt_u32 s8, 64
  s_cbranch_scc1 loop
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         Store({0xbe880380u, 0x7e020280u, 0x7d880008u, 0xbf860004u, 0x7d880088u, 0xbf870003u, 0x4a020281u, 0xbf820002u, 0x4a020282u, 0x4a020283u, 0x80089008u, 0xbf0ac008u, 0xbf85fff5u}), Split::Clone},
        {"loop selection arms that leave the iteration rejoin before a join the enclosing selection shares", R"(
  v_mov_b32 v1, 0
  s_mov_b32 s8, 0
loop:
  s_cmp_eq_u32 s8, s2
  s_cbranch_scc1 join
  v_cmp_gt_u32 vcc, 8, v0
  s_cbranch_vccz arm
  v_add_nc_u32 v1, 1, v1
  v_cmp_gt_u32 vcc, 16, v0
  s_cbranch_vccz away
arm:
  v_add_nc_u32 v1, 2, v1
  v_cmp_gt_u32 vcc, 4, v0
  s_cbranch_vccz aside
join:
  v_add_nc_u32 v1, 3, v1
  s_cmp_lt_u32 s8, 4
  s_cbranch_scc0 done
  s_branch latch
away:
  v_add_nc_u32 v1, 4, v1
  s_branch latch
aside:
  v_add_nc_u32 v1, 5, v1
latch:
  s_add_u32 s8, s8, 1
  s_branch loop
done:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         Store({0x7e020280u, 0xbe880380u, 0xbf060208u, 0xbf850008u, 0x7d880088u, 0xbf860003u, 0x4a020281u, 0x7d880090u, 0xbf860007u, 0x4a020282u, 0x7d880084u,
                0xbf860006u, 0x4a020283u, 0xbf0a8408u, 0xbf840006u, 0xbf820003u, 0x4a020284u, 0xbf820001u, 0x4a020285u, 0x80088108u, 0xbf82ffedu}), Split::None},
        {"continue beside the inner merge of a nested selection in a loop", R"(
  s_mov_b32 s8, 0
  v_mov_b32 v1, 0
loop:
  v_cmp_gt_u32 vcc, s8, v0
  s_cbranch_vccz join
  v_cmp_gt_u32 vcc, 8, v0
  s_cbranch_vccnz inner
  v_cmp_gt_u32 vcc, 4, v0
  s_cbranch_vccnz latch
inner:
  v_add_nc_u32 v1, 1, v1
join:
  v_add_nc_u32 v1, 2, v1
  s_cmp_ge_u32 s8, 64
  s_cbranch_scc1 done
latch:
  s_add_u32 s8, s8, 16
  s_branch loop
done:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         Store({0xbe880380u, 0x7e020280u, 0x7d880008u, 0xbf860005u, 0x7d880088u, 0xbf870002u, 0x7d880084u, 0xbf870004u, 0x4a020281u, 0x4a020282u, 0xbf09c008u, 0xbf850002u, 0x80089008u, 0xbf82fff4u}), Split::None},
        {"nested selections in a loop whose second latch continues from inside them", R"(
  s_mov_b32 s8, 0
  v_mov_b32 v1, 0
  s_branch loop
again:
  v_add_nc_u32 v1, 2, v1
  s_add_u32 s8, s8, 16
loop:
  s_cmp_eq_u32 s8, s2
  s_cbranch_scc1 latch
  v_cmp_gt_u32 vcc, 8, v0
  s_cbranch_vccz latch
  v_cmp_gt_u32 vcc, 4, v0
  s_cbranch_vccnz again
latch:
  v_add_nc_u32 v1, 1, v1
  s_add_u32 s8, s8, 16
  s_cmp_lt_u32 s8, 64
  s_cbranch_scc1 loop
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         Store({0xbe880380u, 0x7e020280u, 0xbf820002u, 0x4a020282u, 0x80089008u, 0xbf060208u, 0xbf850004u, 0x7d880088u, 0xbf860002u, 0x7d880084u, 0xbf87fff8u, 0x4a020281u, 0x80089008u, 0xbf0ac008u, 0xbf85fff6u}), Split::None},
        {"an inner loop that ends the program or continues its outer loop", R"(
  v_mov_b32 v1, 0
  s_mov_b32 s8, 0
outer:
  v_add_nc_u32 v1, 2, v1
inner:
  v_cmp_gt_u32 vcc, 8, v0
  s_cbranch_vccz done
  v_add_nc_u32 v1, 1, v1
  s_add_u32 s8, s8, 16
  s_cmp_lt_u32 s8, 64
  s_cbranch_scc1 inner
  s_mov_b32 s8, 0
  s_branch outer
done:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         Store({0x7e020280u, 0xbe880380u, 0x4a020282u, 0x7d880088u, 0xbf860006u, 0x4a020281u, 0x80089008u, 0xbf0ac008u, 0xbf85fffau, 0xbe880380u, 0xbf82fff7u}), Split::Route, 0},
        {"an inner loop whose body leaves both loops", R"(
  v_mov_b32 v1, 0
  s_mov_b32 s8, 0
outer:
  s_cmp_lt_u32 s8, 64
  s_cbranch_scc0 done
inner:
  v_cmp_gt_u32 vcc, 8, v0
  s_cbranch_vccz latch
  v_add_nc_u32 v1, 1, v1
  s_add_u32 s8, s8, 16
  s_cmp_eq_u32 s8, s2
  s_cbranch_scc1 done
  s_branch inner
latch:
  v_add_nc_u32 v1, 2, v1
  s_add_u32 s8, s8, 16
  s_branch outer
done:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         Store({0x7e020280u, 0xbe880380u, 0xbf0ac008u, 0xbf84000au, 0x7d880088u, 0xbf860005u, 0x4a020281u, 0x80089008u, 0xbf060208u, 0xbf850004u,
                0xbf82fff9u, 0x4a020282u, 0x80089008u, 0xbf82fff4u}), Split::Route, 0},
        {"loop exit to the end of the program beside a kill exit", R"(
  v_mov_b32 v1, 0
  s_mov_b64 s[20:21], exec
  v_cmp_gt_u32 vcc, 4, v0
  s_andn2_b64 s[20:21], s[20:21], vcc
  s_cbranch_scc0 kill
  s_mov_b32 s8, 0
loop:
  v_add_nc_u32 v1, 1, v1
  v_cmp_eq_u32 vcc, s8, v0
  s_andn2_b64 s[20:21], s[20:21], vcc
  s_cbranch_scc0 kill
  s_add_u32 s8, s8, 1
  s_cmp_lt_u32 s8, 4
  s_cbranch_scc1 loop
  s_mov_b64 exec, s[20:21]
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm
kill:
  s_mov_b64 exec, 0
  s_endpgm)",
         {0x7e020280u, 0xbe94047eu, 0x7d880084u, 0x8a946a14u, 0xbf84000cu, 0xbe880380u, 0x4a020281u, 0x7d840008u, 0x8a946a14u, 0xbf840007u,
          0x80088108u, 0xbf0a8408u, 0xbf85fff9u, 0xbefe0414u, 0xe0700000u, 0x80000100u, 0xbf810000u, 0xbefe0480u, 0xbf810000u},
         Split::Route, 0},
        {"inner loop exit to a return after the outer loop", R"(
  v_mov_b32 v1, 0
  s_mov_b32 s8, 0
outer:
  s_mov_b32 s9, 0
inner:
  v_add_nc_u32 v1, 1, v1
  s_cmp_eq_u32 s9, s2
  s_cbranch_scc1 early_exit
  s_add_u32 s9, s9, 1
  s_cmp_lt_u32 s9, 4
  s_cbranch_scc1 inner
  s_add_u32 s8, s8, 1
  s_cmp_lt_u32 s8, 4
  s_cbranch_scc1 outer
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm
early_exit:
  v_add_nc_u32 v1, 2, v1
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         {0x7e020280u, 0xbe880380u, 0xbe890380u, 0x4a020281u, 0xbf060209u, 0xbf850009u, 0x80098109u, 0xbf0a8409u, 0xbf85fffau, 0x80088108u,
          0xbf0a8408u, 0xbf85fff6u, 0xe0700000u, 0x80000100u, 0xbf810000u, 0x4a020282u, 0xe0700000u, 0x80000100u, 0xbf810000u},
         Split::Route, 0},
        {"loop exit tail beside the continue block", R"(
  v_mov_b32 v1, 0
  s_mov_b32 s8, 0
loop:
  v_add_nc_u32 v1, 1, v1
  s_cmp_eq_u32 s8, s2
  s_cbranch_scc0 latch
  v_add_nc_u32 v1, 2, v1
  s_branch done
latch:
  s_add_u32 s8, s8, 1
  s_cmp_lt_u32 s8, 4
  s_cbranch_scc1 loop
done:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         Store({0x7e020280u, 0xbe880380u, 0x4a020281u, 0xbf060208u, 0xbf840002u, 0x4a020282u, 0xbf820003u, 0x80088108u, 0xbf0a8408u, 0xbf85fff8u}), Split::None},
        {"shared early exit", R"(
  v_mov_b32 v1, 0
  v_cmp_gt_u32 vcc, 16, v0
  s_and_saveexec_b64 s[8:9], vcc
  s_cbranch_execz join
  v_cmp_gt_u32 vcc, 4, v0
  s_cbranch_vccz early_exit
  v_add_nc_u32 v1, 1, v0
join:
  s_mov_b64 exec, s[8:9]
  v_cmp_gt_u32 vcc, 32, v0
  s_cbranch_vccz early_exit
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm
early_exit:
  s_mov_b64 exec, 0
  s_endpgm)",
         {0x7e020280u, 0x7d880090u, 0xbe88246au, 0xbf880003u, 0x7d880084u, 0xbf860007u, 0x4a020081u, 0xbefe0408u, 0x7d8800a0u, 0xbf860003u, 0xe0700000u, 0x80000100u, 0xbf810000u, 0xbefe0480u, 0xbf810000u}, Split::Clone},
        {"short-circuit condition", R"(
  v_cmp_gt_u32 vcc, 16, v0
  s_cbranch_vccz body
  v_cmp_gt_u32 vcc, 8, v0
  s_cbranch_vccnz done
body:
  v_add_nc_u32 v1, 1, v0
  v_cmp_gt_u32 vcc, 4, v0
  s_cbranch_vccz done
  v_add_nc_u32 v1, 2, v1
done:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         Store({0x7d880090u, 0xbf860002u, 0x7d880088u, 0xbf870004u, 0x4a020081u, 0x7d880084u, 0xbf860001u, 0x4a020282u}), Split::Clone},
        {"entered region with an expensive body", R"(
  v_mov_b32 v1, 0
  v_cmp_gt_u32 vcc, 16, v0
  s_and_saveexec_b64 s[8:9], vcc
  s_cbranch_execz body
  v_add_nc_u32 v1, 1, v0
  v_cmp_gt_u32 vcc, 8, v1
  s_cbranch_vccnz done
body:
  s_mov_b32 s10, 0
loop:
  buffer_load_dword v2, off, s[0:3], 0
  v_mul_f32 v3, v2, v1
  v_add_f32 v3, v3, v2
  v_xor_b32 v1, v3, v1
  v_readfirstlane_b32 s11, v1
  v_add_nc_u32 v1, s11, v1
  buffer_load_dword v2, off, s[0:3], 0
  v_mul_f32 v3, v2, v1
  v_add_f32 v3, v3, v2
  v_xor_b32 v1, v3, v1
  v_readfirstlane_b32 s11, v1
  v_add_nc_u32 v1, s11, v1
  s_add_u32 s10, s10, 1
  s_cmp_lt_u32 s10, 4
  s_cbranch_scc1 loop
  s_mov_b64 exec, s[8:9]
  buffer_store_dword v1, off, s[0:3], 0
done:
  s_endpgm)",
         {0x7e020280u, 0x7d880090u, 0xbe88246au, 0xbf880003u, 0x4a020081u, 0x7d880288u, 0xbf870015u, 0xbe8a0380u, 0xe0300000u, 0x80000200u,
          0x10060302u, 0x06060503u, 0x3a020303u, 0x7e160501u, 0x4a02020bu, 0xe0300000u, 0x80000200u, 0x10060302u, 0x06060503u, 0x3a020303u,
          0x7e160501u, 0x4a02020bu, 0x800a810au, 0xbf0a840au, 0xbf85ffefu, 0xbefe0408u, 0xe0700000u, 0x80000100u, 0xbf810000u},
         Split::Route, 0,
         {0x7e020280u, 0x7d880090u, 0xbe88246au, 0xbf800000u, 0x4a020081u, 0x7d880288u, 0xbf870015u, 0xbe8a0380u, 0xe0300000u, 0x80000200u,
          0x10060302u, 0x06060503u, 0x3a020303u, 0x7e160501u, 0x4a02020bu, 0xe0300000u, 0x80000200u, 0x10060302u, 0x06060503u, 0x3a020303u,
          0x7e160501u, 0x4a02020bu, 0x800a810au, 0xbf0a840au, 0xbf85ffefu, 0xbefe0408u, 0xe0700000u, 0x80000100u, 0xbf810000u}},
        {"return tail past an entered region", R"(
  v_mov_b32 v1, 0
  v_cmp_gt_u32 vcc, 16, v0
  s_and_saveexec_b64 s[8:9], vcc
  s_cbranch_execz body
  v_cmp_gt_u32 vcc, 8, v0
  s_cbranch_vccnz done
  v_add_nc_u32 v1, 1, v0
  s_mov_b64 exec, s[8:9]
body:
  s_mov_b32 s10, 0
loop:
  buffer_load_dword v2, off, s[0:3], 0
  v_mul_f32 v3, v2, v1
  v_add_f32 v3, v3, v2
  v_xor_b32 v1, v3, v1
  v_readfirstlane_b32 s11, v1
  v_add_nc_u32 v1, s11, v1
  buffer_load_dword v2, off, s[0:3], 0
  v_mul_f32 v3, v2, v1
  v_add_f32 v3, v3, v2
  v_xor_b32 v1, v3, v1
  v_readfirstlane_b32 s11, v1
  v_add_nc_u32 v1, s11, v1
  s_add_u32 s10, s10, 1
  s_cmp_lt_u32 s10, 4
  s_cbranch_scc1 loop
  buffer_store_dword v1, off, s[0:3], 0
done:
  s_endpgm)",
         {0x7e020280u, 0x7d880090u, 0xbe88246au, 0xbf880004u, 0x7d880088u, 0xbf870016u, 0x4a020081u, 0xbefe0408u, 0xbe8a0380u, 0xe0300000u,
          0x80000200u, 0x10060302u, 0x06060503u, 0x3a020303u, 0x7e160501u, 0x4a02020bu, 0xe0300000u, 0x80000200u, 0x10060302u, 0x06060503u,
          0x3a020303u, 0x7e160501u, 0x4a02020bu, 0x800a810au, 0xbf0a840au, 0xbf85ffefu, 0xe0700000u, 0x80000100u, 0xbf810000u},
         Split::Clone},
        {"entered region in a loop", R"(
  s_mov_b32 s12, 0
  v_mov_b32 v1, 0
outer:
  v_cmp_gt_u32 vcc, s12, v0
  s_cbranch_vccz body
  v_cmp_gt_u32 vcc, 8, v0
  s_cbranch_vccnz join
  v_add_nc_u32 v1, 1, v1
body:
  buffer_load_dword v2, off, s[0:3], 0
  v_mul_f32 v3, v2, v1
  v_add_f32 v3, v3, v2
  v_xor_b32 v1, v3, v1
  v_readfirstlane_b32 s11, v1
  v_add_nc_u32 v1, s11, v1
  buffer_load_dword v2, off, s[0:3], 0
  v_mul_f32 v3, v2, v1
  v_add_f32 v3, v3, v2
  v_xor_b32 v1, v3, v1
  v_readfirstlane_b32 s11, v1
  v_add_nc_u32 v1, s11, v1
join:
  v_add_nc_u32 v1, 2, v1
  s_add_u32 s12, s12, 16
  s_cmp_lt_u32 s12, 64
  s_cbranch_scc0 exit_loop
  s_branch outer
exit_loop:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         {0xbe8c0380u, 0x7e020280u, 0x7d88000cu, 0xbf860003u, 0x7d880088u, 0xbf87000fu, 0x4a020281u, 0xe0300000u, 0x80000200u, 0x10060302u,
          0x06060503u, 0x3a020303u, 0x7e160501u, 0x4a02020bu, 0xe0300000u, 0x80000200u, 0x10060302u, 0x06060503u, 0x3a020303u, 0x7e160501u,
          0x4a02020bu, 0x4a020282u, 0x800c900cu, 0xbf0ac00cu, 0xbf840001u, 0xbf82ffe8u, 0xe0700000u, 0x80000100u, 0xbf810000u},
         Split::Route},
        {"nested entered regions", R"(
  v_mov_b32 v1, 0
  v_cmp_gt_u32 vcc, 16, v0
  s_and_saveexec_b64 s[8:9], vcc
  s_cbranch_execz outer_body
  v_add_nc_u32 v1, 1, v0
  v_cmp_gt_u32 vcc, 8, v1
  s_cbranch_vccnz done
outer_body:
  buffer_load_dword v2, off, s[0:3], 0
  v_mul_f32 v3, v2, v1
  v_add_f32 v3, v3, v2
  v_xor_b32 v1, v3, v1
  v_readfirstlane_b32 s11, v1
  v_add_nc_u32 v1, s11, v1
  s_cmp_eq_u32 s11, 64
  s_cbranch_scc0 inner_body
  v_add_nc_u32 v1, 2, v1
  v_cmp_gt_u32 vcc, 4, v1
  s_cbranch_vccz done
inner_body:
  s_mov_b32 s10, 0
inner_loop:
  buffer_load_dword v2, off, s[0:3], 0
  v_mul_f32 v3, v2, v1
  v_add_f32 v3, v3, v2
  v_xor_b32 v1, v3, v1
  v_readfirstlane_b32 s11, v1
  v_add_nc_u32 v1, s11, v1
  s_add_u32 s10, s10, 1
  s_cmp_lt_u32 s10, 4
  s_cbranch_scc1 inner_loop
  s_mov_b64 exec, s[8:9]
  buffer_store_dword v1, off, s[0:3], 0
done:
  s_endpgm)",
         {0x7e020280u, 0x7d880090u, 0xbe88246au, 0xbf880003u, 0x4a020081u, 0x7d880288u, 0xbf87001au, 0xe0300000u, 0x80000200u, 0x10060302u,
          0x06060503u, 0x3a020303u, 0x7e160501u, 0x4a02020bu, 0xbf06c00bu, 0xbf840003u, 0x4a020282u, 0x7d880284u, 0xbf86000eu, 0xbe8a0380u,
          0xe0300000u, 0x80000200u, 0x10060302u, 0x06060503u, 0x3a020303u, 0x7e160501u, 0x4a02020bu, 0x800a810au, 0xbf0a840au, 0xbf85fff6u,
          0xbefe0408u, 0xe0700000u, 0x80000100u, 0xbf810000u},
         Split::Route},
        {"shared body after an overwritten condition", R"(
  v_mov_b32 v1, 0
  s_cmp_eq_u32 s2, 64
  s_cbranch_scc0 shared
  v_add_nc_u32 v1, 1, v0
  s_cmp_eq_u32 s3, 0
  s_cbranch_scc1 done
shared:
  s_cbranch_scc1 store
  buffer_load_dword v2, off, s[0:3], 0
  v_mul_f32 v3, v2, v1
  v_add_f32 v3, v3, v2
  v_xor_b32 v1, v3, v1
  v_readfirstlane_b32 s11, v1
  v_add_nc_u32 v1, s11, v1
  buffer_load_dword v2, off, s[0:3], 0
  v_mul_f32 v3, v2, v1
  v_add_f32 v3, v3, v2
  v_xor_b32 v1, v3, v1
  v_readfirstlane_b32 s11, v1
  v_add_nc_u32 v1, s11, v1
store:
  buffer_store_dword v1, off, s[0:3], 0
done:
  s_endpgm)",
         {0x7e020280u, 0xbf06c002u, 0xbf840003u, 0x4a020081u, 0xbf068003u, 0xbf850011u, 0xbf85000eu, 0xe0300000u, 0x80000200u, 0x10060302u,
          0x06060503u, 0x3a020303u, 0x7e160501u, 0x4a02020bu, 0xe0300000u, 0x80000200u, 0x10060302u, 0x06060503u, 0x3a020303u, 0x7e160501u,
          0x4a02020bu, 0xe0700000u, 0x80000100u, 0xbf810000u},
         Split::Route},
        {"entered region beside early returns", R"(
  v_mov_b32 v1, 0
  v_cmp_gt_u32 vcc, 16, v0
  s_cbranch_vccz body
  v_add_nc_u32 v1, 1, v0
  v_cmp_gt_u32 vcc, 8, v1
  s_cbranch_vccnz tail
  v_readfirstlane_b32 s11, v1
  s_cmp_eq_u32 s11, 12
  s_cbranch_scc0 done
  s_cmp_eq_u32 s11, 38
  s_cbranch_scc1 done
body:
  buffer_load_dword v2, off, s[0:3], 0
  v_mul_f32 v3, v2, v1
  v_add_f32 v3, v3, v2
  v_xor_b32 v1, v3, v1
  v_readfirstlane_b32 s11, v1
  v_add_nc_u32 v1, s11, v1
  buffer_load_dword v2, off, s[0:3], 0
  v_mul_f32 v3, v2, v1
  v_add_f32 v3, v3, v2
  v_xor_b32 v1, v3, v1
  v_readfirstlane_b32 s11, v1
  v_add_nc_u32 v1, s11, v1
tail:
  v_add_nc_u32 v1, 1, v1
  v_xor_b32 v1, v3, v1
  v_add_nc_u32 v1, 2, v1
  v_xor_b32 v1, v3, v1
  v_add_nc_u32 v1, 3, v1
  v_xor_b32 v1, v3, v1
  v_add_nc_u32 v1, 4, v1
  v_xor_b32 v1, v3, v1
  v_add_nc_u32 v1, 5, v1
  v_xor_b32 v1, v3, v1
  v_add_nc_u32 v1, 6, v1
  v_xor_b32 v1, v3, v1
  v_add_nc_u32 v1, 7, v1
  v_xor_b32 v1, v3, v1
  v_add_nc_u32 v1, 8, v1
  v_xor_b32 v1, v3, v1
  v_add_nc_u32 v1, 9, v1
  v_xor_b32 v1, v3, v1
  v_add_nc_u32 v1, 10, v1
  v_xor_b32 v1, v3, v1
done:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         {0x7e020280u, 0x7d880090u, 0xbf860008u, 0x4a020081u, 0x7d880288u, 0xbf870013u, 0x7e160501u, 0xbf068c0bu, 0xbf840024u, 0xbf06a60bu,
          0xbf850022u, 0xe0300000u, 0x80000200u, 0x10060302u, 0x06060503u, 0x3a020303u, 0x7e160501u, 0x4a02020bu, 0xe0300000u, 0x80000200u,
          0x10060302u, 0x06060503u, 0x3a020303u, 0x7e160501u, 0x4a02020bu, 0x4a020281u, 0x3a020303u, 0x4a020282u, 0x3a020303u, 0x4a020283u,
          0x3a020303u, 0x4a020284u, 0x3a020303u, 0x4a020285u, 0x3a020303u, 0x4a020286u, 0x3a020303u, 0x4a020287u, 0x3a020303u, 0x4a020288u,
          0x3a020303u, 0x4a020289u, 0x3a020303u, 0x4a02028au, 0x3a020303u, 0xe0700000u, 0x80000100u, 0xbf810000u},
         Split::Route, 4},
        {"selection arm that only returns", R"(
  s_cbranch_execz inner
  s_cbranch_scc1 done
inner:
  v_cmp_gt_u32 vcc, 3, v1
  s_cbranch_vccnz second
  v_mul_f32 v3, v2, v1
  s_cbranch_vccz join
second:
  s_cbranch_vccnz done
join:
  v_add_nc_u32 v1, 2, v1
done:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         {0xbf880001u, 0xbf850006u, 0x7d880283u, 0xbf870002u, 0x10060302u, 0xbf860001u, 0xbf870001u, 0x4a020282u, 0xe0700000u, 0x80000100u,
          0xbf810000u},
         Split::Route, 9},
        {"shared early exit beside an expensive region", R"(
  v_mov_b32 v1, 0
  v_cmp_gt_u32 vcc, 16, v0
  s_and_saveexec_b64 s[8:9], vcc
  s_cbranch_execz join
  v_cmp_gt_u32 vcc, 4, v0
  s_cbranch_vccz early_exit
  buffer_load_dword v2, off, s[0:3], 0
  v_mul_f32 v3, v2, v1
  v_add_f32 v3, v3, v2
  v_xor_b32 v1, v3, v1
  v_readfirstlane_b32 s11, v1
  v_add_nc_u32 v1, s11, v1
  buffer_load_dword v2, off, s[0:3], 0
  v_mul_f32 v3, v2, v1
  v_add_f32 v3, v3, v2
  v_xor_b32 v1, v3, v1
join:
  s_mov_b64 exec, s[8:9]
  v_cmp_gt_u32 vcc, 32, v0
  s_cbranch_vccz early_exit
  buffer_load_dword v2, off, s[0:3], 0
  v_mul_f32 v3, v2, v1
  v_add_f32 v3, v3, v2
  v_xor_b32 v1, v3, v1
  v_readfirstlane_b32 s11, v1
  v_add_nc_u32 v1, s11, v1
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm
early_exit:
  s_mov_b64 exec, 0
  s_endpgm)",
         {0x7e020280u, 0x7d880090u, 0xbe88246au, 0xbf88000eu, 0x7d880084u, 0xbf860019u, 0xe0300000u, 0x80000200u, 0x10060302u, 0x06060503u,
          0x3a020303u, 0x7e160501u, 0x4a02020bu, 0xe0300000u, 0x80000200u, 0x10060302u, 0x06060503u, 0x3a020303u, 0xbefe0408u, 0x7d8800a0u,
          0xbf86000au, 0xe0300000u, 0x80000200u, 0x10060302u, 0x06060503u, 0x3a020303u, 0x7e160501u, 0x4a02020bu, 0xe0700000u, 0x80000100u,
          0xbf810000u, 0xbefe0480u, 0xbf810000u},
         Split::Clone},
        {"early exit arm beside a branch to the parent's merge", R"(
  v_mov_b32 v1, 0
  s_cmp_eq_u32 s2, 64
  s_cbranch_scc1 join
  v_add_nc_u32 v1, 1, v0
  s_cmp_eq_u32 s3, 0
  s_cbranch_scc1 early_exit
join:
  v_cmp_gt_u32 vcc, 8, v0
  s_cbranch_vccnz store
  v_add_nc_u32 v1, 2, v1
store:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm
early_exit:
  s_mov_b64 exec, 0
  s_endpgm)",
         {0x7e020280u, 0xbf06c002u, 0xbf850003u, 0x4a020081u, 0xbf068003u, 0xbf850006u, 0x7d880088u, 0xbf870001u, 0x4a020282u, 0xe0700000u,
          0x80000100u, 0xbf810000u, 0xbefe0480u, 0xbf810000u},
         Split::None},
        {"early exit arm beside a parent arm that reaches the same merge", R"(
  v_mov_b32 v1, 0
  s_cmp_eq_u32 s2, 64
  s_cbranch_scc1 other
  v_add_nc_u32 v1, 1, v0
  s_cmp_eq_u32 s3, 0
  s_cbranch_scc1 early_exit
join:
  v_cmp_gt_u32 vcc, 8, v0
  s_cbranch_vccnz store
  v_add_nc_u32 v1, 2, v1
store:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm
other:
  v_add_nc_u32 v1, 3, v0
  s_branch join
early_exit:
  s_mov_b64 exec, 0
  s_endpgm)",
         {0x7e020280u, 0xbf06c002u, 0xbf850009u, 0x4a020081u, 0xbf068003u, 0xbf850008u, 0x7d880088u, 0xbf870001u, 0x4a020282u, 0xe0700000u,
          0x80000100u, 0xbf810000u, 0x4a020083u, 0xbf82fff8u, 0xbefe0480u, 0xbf810000u},
         Split::None},
        {"return block entered from the return block after it", R"(
  v_mov_b32 v1, 0
  s_cmp_eq_u32 s2, 64
  s_cbranch_scc0 later
  v_add_nc_u32 v1, 1, v0
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm
middle:
  v_add_nc_u32 v1, 2, v0
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm
later:
  s_cmp_eq_u32 s3, 0
  s_cbranch_scc1 middle
  v_add_nc_u32 v1, 3, v0
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         {0x7e020280u, 0xbf06c002u, 0xbf840008u, 0x4a020081u, 0xe0700000u, 0x80000100u, 0xbf810000u, 0x4a020082u, 0xe0700000u, 0x80000100u,
          0xbf810000u, 0xbf068003u, 0xbf85fffau, 0x4a020083u, 0xe0700000u, 0x80000100u, 0xbf810000u},
         Split::None},
        {"loop body between an early return and the loop test", R"(
  s_mov_b32 s8, 0
  v_mov_b32 v1, 0
  s_cmp_eq_u32 s2, 64
  s_cbranch_scc0 test
  v_add_nc_u32 v1, 1, v0
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm
body:
  v_add_nc_u32 v1, 2, v1
  s_add_u32 s8, s8, 16
test:
  s_cmp_lt_u32 s8, 64
  s_cbranch_scc1 body
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         {0xbe880380u, 0x7e020280u, 0xbf06c002u, 0xbf840006u, 0x4a020081u, 0xe0700000u, 0x80000100u, 0xbf810000u, 0x4a020282u, 0x80089008u,
          0xbf0ac008u, 0xbf85fffcu, 0xe0700000u, 0x80000100u, 0xbf810000u},
         Split::None},
        {"dword jump table after its base", R"(
  s_getpc_b64 s[12:13]
base:
  s_add_u32 s12, s12, table - base
  s_addc_u32 s13, s13, 0
  s_min_u32 s18, s2, 2
  v_cmp_gt_u32 s[22:23], v0, s2
  s_lshl_b32 s18, s18, 2
  s_load_dword s14, s[12:13], s18
  s_waitcnt lgkmcnt(0)
  v_cmp_lt_u32 vcc, s3, v0
  s_sub_u32 s12, s12, s14
  s_subb_u32 s13, s13, 0
  s_swappc_b64 null, s[12:13]
case0:
  v_mov_b32 v1, 1
  s_branch done
case1:
  v_mov_b32 v1, 2
  s_branch done
case2:
  v_mov_b32 v1, 3
done:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm
table:
  .long table - case0, table - case1, table - case2)",
         DwordTableGetpcFirst, Split::None},
        {"dword jump table after its index", R"(
  s_add_i32 vcc_lo, s2, -1
  s_min_u32 vcc_lo, vcc_lo, 2
  v_mov_b32 v1, 0
  s_lshl_b32 vcc_lo, vcc_lo, 2
  s_getpc_b64 s[14:15]
base:
  s_add_u32 s14, s14, table - base - 8
  s_addc_u32 s15, s15, 0
  s_load_dword s18, s[14:15], vcc_lo offset:8
  s_andn2_b64 vcc, exec, s[22:23]
  s_waitcnt lgkmcnt(0)
  s_sub_u32 s14, s14, s18
  s_subb_u32 s15, s15, 0
  s_swappc_b64 null, s[14:15]
case0:
  v_mov_b32 v1, 1
  s_branch done
case1:
  v_mov_b32 v1, 2
  s_branch done
case2:
  v_mov_b32 v1, 3
done:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm
table:
  .long table - 8 - case0, table - 8 - case1, table - 8 - case2)",
         DwordTableIndexFirst, Split::None},
        {"long branch through vcc", R"(
  v_mov_b32 v1, 1
  s_getpc_b64 vcc
base:
  s_add_u32 vcc_lo, vcc_lo, target - base
  s_addc_u32 vcc_hi, vcc_hi, 0
  s_swappc_b64 null, vcc
  v_mov_b32 v1, 2
target:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         LongBranchVcc, Split::None},
        {"backward long branch", R"(
  s_mov_b32 s8, 0
  v_mov_b32 v1, 0
loop:
  v_add_nc_u32 v1, 1, v1
  s_add_u32 s8, s8, 16
  s_cmp_lt_u32 s8, 64
  s_cbranch_scc0 done
  s_getpc_b64 s[16:17]
base:
  s_sub_u32 s16, s16, base - loop
  s_subb_u32 s17, s17, 0
  s_swappc_b64 null, s[16:17]
done:
  buffer_store_dword v1, off, s[0:3], 0
  s_endpgm)",
         LongBranchBack, Split::None},
    };
    int failures = 0;
    for (const auto& program : programs) {
        try {
            verifyProgram(program);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s\n%.*s\n", error.what(), static_cast<int>(program.source.size()), program.source.data());
            ++failures;
        }
    }
    try {
        verifyNullSwappc();
        verifyScheduledJumpTable();
        VerifyUserDataTail();
        VerifyConstantHighTail();
        verifyCallLinkOverwrittenByWave64Compare();
        verifyCallLinkMaskReadByScalarInstruction();
        verifyLargeLoopExitFallback();
        verifyNestedExitForwarders();
        verifyIrreducibleLoopEntry();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        ++failures;
    }
    if (failures != 0) return 1;
    std::puts("Control flow structurization tests passed");
    return 0;
}
