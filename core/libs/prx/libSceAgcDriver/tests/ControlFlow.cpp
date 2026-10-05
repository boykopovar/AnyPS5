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

std::vector<std::uint32_t> constructBlocks(const ControlFlowGraph& graph, std::uint32_t header, std::uint32_t exclude) {
    std::vector<std::uint32_t> blocks;
    for (const auto& block : graph.blocks) {
        if (graph.Dominates(header, block.id) && (exclude == InvalidControlFlowId || !graph.Dominates(exclude, block.id))) {
            blocks.push_back(block.id);
        }
    }
    return blocks;
}

void verifyStructured(const std::string& prefix, const ControlFlowGraph& graph) {
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
        for (const auto member : constructBlocks(graph, block.id, terminator.mergeBlock)) {
            for (const auto successor : graph.FindBlock(member).successors) {
                if (successor == terminator.mergeBlock || (graph.Dominates(block.id, successor) && !graph.Dominates(terminator.mergeBlock, successor))) continue;
                const bool loopExit = std::any_of(loopExits.begin(), loopExits.end(), [&](const auto& exits) { return successor == exits.first || successor == exits.second; });
                require(loopExit, prefix + "block " + std::to_string(member) + " of the selection at block " + std::to_string(block.id) + " branches to block " + std::to_string(successor) + " outside the construct");
            }
        }
    }
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
    require(!result.spirv.empty(), name + ": no SPIR-V for a " + std::to_string(subgroupSize) + "-lane subgroup");
    return result.spirv.size();
}

void verifyProgram(const Program& program) {
    const std::string name(program.name);
    const auto result = verifyGraph(name, program.code);
    if (program.split == Split::None) {
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

}

int main(int argc, char** argv) {
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
    if (failures != 0) return 1;
    std::puts("Control flow structurization tests passed");
    return 0;
}
