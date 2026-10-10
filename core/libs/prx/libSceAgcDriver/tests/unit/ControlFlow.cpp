#include <Testing/Test.hpp>
#include "ControlFlow/GraphBuilder.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "ControlFlow/Structurizer.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Recompiler.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <exception>
#include <fstream>
#include <initializer_list>
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
using Testing::Case;
using Testing::Require;

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

std::vector<std::uint32_t> constructBlocks(const ControlFlowGraph& graph, std::uint32_t header, std::uint32_t exclude) {
    std::vector<std::uint32_t> blocks;
    for (const auto& block : graph.blocks) {
        if (graph.Dominates(header, block.id) && (exclude == InvalidControlFlowId || !graph.Dominates(exclude, block.id))) {
            blocks.push_back(block.id);
        }
    }
    return blocks;
}

bool inEnclosingContinueConstruct(const ControlFlowGraph& graph, std::uint32_t header, std::uint32_t member) {
    return std::any_of(graph.blocks.begin(), graph.blocks.end(), [&](const BasicBlock& loop) {
        const auto continueBlock = loop.terminator.continueBlock;
        return loop.terminator.loopHeader && graph.Dominates(loop.id, header) && !graph.Dominates(continueBlock, header) && graph.Dominates(continueBlock, member);
    });
}

void verifyStructured(const std::string& prefix, const ControlFlowGraph& graph) {
    std::map<std::uint32_t, std::uint32_t> mergeOwners;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> loopExits;
    for (const auto& block : graph.blocks) {
        const auto& terminator = block.terminator;
        if (terminator.mergeBlock == InvalidControlFlowId) continue;
        Require(mergeOwners.emplace(terminator.mergeBlock, block.id).second, prefix + "block " + std::to_string(terminator.mergeBlock) + " merges two constructs");
        if (terminator.loopHeader) loopExits.emplace_back(terminator.mergeBlock, terminator.continueBlock);
    }
    for (const auto& block : graph.blocks) {
        const auto& terminator = block.terminator;
        if (terminator.loopHeader || terminator.mergeBlock == InvalidControlFlowId) continue;
        for (const auto member : constructBlocks(graph, block.id, terminator.mergeBlock)) {
            if (inEnclosingContinueConstruct(graph, block.id, member)) continue;
            for (const auto successor : graph.FindBlock(member).successors) {
                if (successor == terminator.mergeBlock || (graph.Dominates(block.id, successor) && !graph.Dominates(terminator.mergeBlock, successor))) continue;
                const bool loopExit = std::any_of(loopExits.begin(), loopExits.end(), [&](const auto& exits) { return successor == exits.first || successor == exits.second; });
                Require(loopExit, prefix + "block " + std::to_string(member) + " of the selection at block " + std::to_string(block.id) + " branches to block " + std::to_string(successor) + " outside the construct");
            }
        }
    }
    for (const auto& loop : graph.blocks) {
        if (!loop.terminator.loopHeader) continue;
        const auto continueBlock = loop.terminator.continueBlock;
        for (const auto member : constructBlocks(graph, loop.id, loop.terminator.mergeBlock)) {
            const auto& terminator = graph.FindBlock(member).terminator;
            if (terminator.loopHeader || graph.Dominates(continueBlock, member)) continue;
            Require(terminator.mergeBlock != continueBlock, prefix + "the selection at block " + std::to_string(member) + " in the loop at block " + std::to_string(loop.id) + " merges at the loop's continue block " + std::to_string(continueBlock));
        }
    }
}

std::uint32_t followEmptyBlocks(const std::string& prefix, const ControlFlowGraph& graph, std::uint32_t blockId, RouteState& routes) {
    for (std::size_t steps = 0; steps <= graph.blocks.size(); ++steps) {
        const auto& block = graph.FindBlock(blockId);
        const auto& terminator = block.terminator;
        const bool empty = block.instructionBegin == block.instructionEnd;
        Require(terminator.gotoValue < 0 || empty, prefix + "block " + std::to_string(block.id) + " sets a route variable after guest instructions");
        if (terminator.gotoValue >= 0) routes[terminator.gotoVariable] = terminator.gotoValue != 0;
        if (!empty) return blockId;
        if (terminator.kind == TerminatorKind::Branch) {
            blockId = terminator.trueBlock;
        } else if (terminator.kind == TerminatorKind::ConditionalBranch && terminator.condition == BranchCondition::GotoVariable) {
            const auto route = routes.find(terminator.gotoVariable);
            Require(route != routes.end(), prefix + "block " + std::to_string(block.id) + " reads route variable " + std::to_string(terminator.gotoVariable) + " before any path sets it");
            blockId = route->second ? terminator.trueBlock : terminator.falseBlock;
        } else {
            return blockId;
        }
    }
    Testing::Fail(prefix + "a cycle of empty blocks");
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
        Require(copy.instructionBegin == source.instructionBegin && copy.instructionEnd == source.instructionEnd, prefix + "block " + std::to_string(copy.id) + " runs other instructions than block " + std::to_string(source.id));
        Require(copy.terminator.kind == source.terminator.kind && copy.terminator.condition == source.terminator.condition, prefix + "block " + std::to_string(copy.id) + " branches differently from block " + std::to_string(source.id));
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

void verifyRequest(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    Require(file.is_open(), path + ": cannot open the request file");
    std::ostringstream text;
    text << file.rdbuf();
    const auto request = RequestSerializer{}.Deserialize(text.str());
    const auto result = verifyGraph(path, request.request.shader.code);
    std::printf("%s: structured with %zu added blocks, %zu cloned instructions, %zu route variables\n", path.c_str(), result.addedBlocks, result.clonedInstructions, result.routeVariables);
}

std::size_t recompile(const std::string& name, std::span<const std::uint32_t> code, std::uint32_t subgroupSize) {
    const std::array<std::uint32_t, 4> userData{0x10000000u, 0x00100000u, 0x40u, 0x00027facu};
    const std::array<std::uint32_t, 2> capabilities{1u, 61u};
    const std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = 64;
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
    Require(!result.spirv.empty(), name + ": no SPIR-V for a " + std::to_string(subgroupSize) + "-lane subgroup");
    return result.spirv.size();
}

void verifyProgram(const Program& program) {
    const std::string name(program.name);
    const auto result = verifyGraph(name, program.code);
    if (program.split == Split::None) {
        Require(result.clonedInstructions == 0 && result.routeVariables == 0, name + ": expected no clone or routing, got " + std::to_string(result.clonedInstructions) + " cloned instructions and " + std::to_string(result.routeVariables) + " route variables");
    } else if (program.split == Split::Clone) {
        Require(result.clonedInstructions != 0 && result.routeVariables == 0, name + ": expected a clone, got " + std::to_string(result.clonedInstructions) + " cloned instructions and " + std::to_string(result.routeVariables) + " route variables");
    } else {
        Require(result.clonedInstructions <= program.clonedLimit && result.routeVariables != 0, name + ": expected routing with at most " + std::to_string(program.clonedLimit) + " cloned instructions, got " + std::to_string(result.clonedInstructions) + " cloned instructions and " + std::to_string(result.routeVariables) + " route variables");
    }
    const auto words = recompile(name, program.code, 64u);
    static_cast<void>(recompile(name, program.code, 32u));
    if (!program.reference.empty()) {
        const auto reference = recompile(name, program.reference, 64u);
        Require(words * 10u <= reference * 11u, name + ": " + std::to_string(words) + " SPIR-V words, more than 10% over the " + std::to_string(reference) + " of the program without the entering branch");
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
    Require(graph.codeTableLoads.size() == 1u && graph.codeTableLoads.front().programCounter == loadPc && graph.codeTableLoads.front().values == values, name + ": wrong code table");
    std::vector<std::uint32_t> lowered;
    for (const auto& block : graph.blocks) {
        if (block.terminator.indirectPcSgpr != InvalidControlFlowId) lowered.insert(lowered.end(), block.terminator.indirectTargetProgramCounters.begin(), block.terminator.indirectTargetProgramCounters.end());
    }
    std::sort(lowered.begin(), lowered.end());
    Require(lowered == targets, name + ": the jump does not reach exactly the table targets");
}

void verifyLongBranch(const std::string& name, std::span<const std::uint32_t> code, std::uint32_t branchPc, std::uint32_t target) {
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    const auto graph = GraphBuilder{}.Build(decoded);
    const auto source = std::find_if(graph.blocks.begin(), graph.blocks.end(), [&](const BasicBlock& block) { return block.instructionEnd != block.instructionBegin && decoded.instructions[block.instructionEnd - 1u].programCounter == branchPc; });
    Require(source != graph.blocks.end() && source->terminator.kind == TerminatorKind::Branch && graph.FindBlock(source->terminator.trueBlock).startProgramCounter == target, name + ": the jump does not branch to " + std::to_string(target));
}

const std::vector<Program>& Programs() {
    static const std::vector<Program> programs{
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
    return programs;
}

void verifyPrograms(Split split) {
    std::size_t verified = 0;
    for (const auto& program : Programs()) {
        if (program.split != split) continue;
        ++verified;
        try {
            verifyProgram(program);
        } catch (const Testing::Failure&) {
            throw;
        } catch (const std::exception& error) {
            Testing::Fail(std::string(program.name) + ": " + error.what());
        }
    }
    Require(verified != 0, "no program of this kind was verified");
}

const Case unsplitPrograms{"Structurize_StructurablePrograms_NeedNoCloneOrRouting", [] {
    verifyPrograms(Split::None);
}};

const Case clonedPrograms{"Structurize_SharedCode_IsCloned", [] {
    verifyPrograms(Split::Clone);
}};

const Case routedPrograms{"Structurize_EnteredRegions_AreRoutedThroughVariables", [] {
    verifyPrograms(Split::Route);
}};

const Case nullSwappc{"Decode_SwappcWithNullDestination_IsSetpc", [] {
    const std::array<std::uint32_t, 2> jump{0xbefd210cu, 0xbf810000u};
    const auto swappc = RdnaInstructionDecoder{}.Decode(jump).instructions.front();
    Require(swappc.op == RdnaOpcode::SSetpcB64 && swappc.opcodeId == 0x21u && swappc.destination.kind == RdnaOperandKind::Null &&
        swappc.source0.kind == RdnaOperandKind::ScalarRegister && swappc.source0.reg == 12u, "s_swappc_b64 null, s[12:13] does not decode as s_setpc_b64 s[12:13]");
    const std::array<std::uint32_t, 3> front{0x7e020281u, 0xbefd2106u, 0xbf810000u};
    Require(DecodeRdnaFrontProgram(front).code.size() == 2u, "a front program ending in s_swappc_b64 null, s[6:7] does not end there");
}};

const Case jumpTables{"Build_DwordJumpTables_ReachExactlyTheTableTargets", [] {
    verifyJumpTable("dword jump table after its base", DwordTableGetpcFirst, 0x20u, {0x20u, 0x18u, 0x10u}, {0x3cu, 0x44u, 0x4cu});
    verifyJumpTable("dword jump table after its index", DwordTableIndexFirst, 0x1cu, {0x18u, 0x10u, 0x08u}, {0x38u, 0x40u, 0x48u});
}};

const Case longBranches{"Build_LongBranches_BranchToTheirTarget", [] {
    verifyLongBranch("long branch through vcc", LongBranchVcc, 0x10u, 0x18u);
    verifyLongBranch("backward long branch", LongBranchBack, 0x24u, 0x08u);
}};

const Case unsupportedJumps{"Build_UnsupportedDynamicJumps_AreRefused", [] {
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
        Require(error.find(message) != std::string::npos, std::string(name) + ": expected a refusal with '" + std::string(message) + "', got '" + error + "'");
    }
}};

const Case requestFiles{"Structurize_RequestFilesGivenAsArguments_KeepTheirExecutions", [] {
    const auto& paths = Testing::Arguments();
    if (paths.empty()) Testing::Skip("no request files were given as arguments");
    for (const auto& path : paths) verifyRequest(path);
}};

} // namespace
