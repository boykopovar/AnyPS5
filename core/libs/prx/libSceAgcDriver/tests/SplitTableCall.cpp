#include "BdaAbi.hpp"
#include "ControlFlow/GraphBuilder.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Recompiler.hpp"
#include "SpirvBackend/SpirvAnalysis.hpp"
#include "SplitStateAbi.hpp"
#include <array>
#include <cstdio>
#include <exception>
#include <span>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include <spirv-tools/libspirv.hpp>
#endif

namespace {

using namespace ShaderRecompiler;

void Check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<std::uint32_t> WaterfallCaller() {
    std::vector<std::uint32_t> code{0xbeaa047eu, 0x7e600281u, 0x7e280500u, 0xbea8047eu, 0x7da40014u, 0x7e600280u};
    code.insert(code.end(), {0x8f148414u, 0xf4080406u, 0xfa000270u, 0xbf8cc07fu, 0xf4280208u, 0x28000000u, 0xbf8cc07fu, 0xbe8e0308u, 0xbe8f0309u, 0xbe8e210eu});
    code.insert(code.end(), {0xbefe0428u, 0x7daa6080u});
    const auto offset = 2 - static_cast<std::int32_t>(code.size() + 1u);
    code.push_back(0xbf890000u | (static_cast<std::uint32_t>(offset) & 0xffffu));
    code.insert(code.end(), {0xbefe042au, 0xbf810000u});
    return code;
}

void CheckSplitModule(const RecompileResult& result, std::string_view name) {
    const auto& words = result.spirv.Words();
    Check(words.size() >= 5 && words[0] == spv::MagicNumber, std::string(name) + ": missing SPIR-V module");
    Check(result.splitStateLanes == 64u, std::string(name) + ": does not report its split state buffer");
    bool splitSet = false;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        const auto opcode = words[offset] & spv::OpCodeMask;
        Check(count != 0 && count <= words.size() - offset, std::string(name) + ": malformed SPIR-V instruction");
        if (opcode == spv::OpDecorate && count == 4 && words[offset + 2] == spv::DecorationDescriptorSet && words[offset + 3] == SplitStateDescriptorSet) splitSet = true;
        offset += count;
    }
    Check(splitSet, std::string(name) + ": has no split state descriptor set");
#if ANYPS5_ENABLE_SPIRV_TOOLS
    std::string diagnostics;
    spvtools::SpirvTools validator(SPV_ENV_VULKAN_1_1);
    validator.SetMessageConsumer([&](spv_message_level_t, const char*, const spv_position_t&, const char* message) {
        if (message != nullptr) diagnostics += std::string(message) + '\n';
    });
    Check(validator.Validate(words.data(), words.size()), std::string(name) + ": emitted invalid SPIR-V:\n" + diagnostics);
#endif
}

SpirvTarget SplitTarget() {
    static constexpr std::array<std::uint32_t, 4> capabilities{spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityGroupNonUniformBallot};
    static constexpr std::array<std::string_view, 3> extensions{"SPV_KHR_8bit_storage", "SPV_KHR_float_controls", "SPV_KHR_physical_storage_buffer"};
    return {0x00401000u, 0x00010300u, 32, BdaAbi::Version, capabilities, extensions, false, {1024, 1024, 64}, 1024, 32768, {}, {}};
}

RecompileResult RecompileSplit(const SplitTableCallProgram& part, std::span<const std::uint32_t> userData, std::uint64_t address) {
    const std::span<const std::uint32_t> words(part.code);
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, address, words, 0, {}};
    request.target = SplitTarget();
    request.layout = {0, 0, 0, 128};
    request.context.waveSize = 32;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {true, false, false}, false, 1u};
    request.context.splitState = part.plan;
    request.useCache = false;
    return Recompile(request);
}

void CheckSplitPrograms() {
    const auto caller = WaterfallCaller();
    const auto program = RdnaInstructionDecoder{}.Decode(caller);
    const SwappcInfo swappc{false, 0u, 16u};
    const auto calls = FindTableCalls(program, swappc);
    Check(calls.size() == 1u, "the waterfall caller holds no table call");
    const std::vector<TableCallee> callees{{0x1000u, {0xbf068000u, 0x4a020285u, 0xbe80200eu}}, {0x2000u, {0x4a0202c0u, 0xbf068001u, 0xbe80200eu}}};
    std::string refusal;
    const auto split = SplitTableCall(program, calls[0], callees, 32u, refusal);
    Check(split.has_value(), "the waterfall caller cannot split: " + refusal);
    const auto programs = BuildSplitTableCallPrograms(program, calls[0], callees, *split, 4u);
    const std::array<std::uint32_t, 16> userData{};
    CheckSplitModule(RecompileSplit(programs.entry, userData, 0x710000u), "entry");
    for (std::size_t callee = 0; callee < programs.callees.size(); ++callee) CheckSplitModule(RecompileSplit(programs.callees[callee], userData, 0x720000u + callee * 0x1000u), "callee " + std::to_string(callee));
    CheckSplitModule(RecompileSplit(programs.resume, userData, 0x730000u), "resume");
}

struct Interpolation {
    std::string_view name;
    std::vector<std::uint32_t> code;
};

void CheckInterpolationAfterCallReadsM0() {
    const std::vector<TableCallee> callees{{0x1000u, {0xbefc0380u, 0xbe80200eu}}};
    const std::vector<Interpolation> interpolations{
        {"v_interp_mov_f32 v1, p20, attr0.x", {0xc8060001u}},
        {"v_interp_mov_f32_e64 v1, p20, attr0.x", {0xd6020001u, 0x02000200u}},
        {"v_interp_p1ll_f16 v1, v0, attr0.x", {0xd7420001u, 0x00020000u}},
    };
    for (const auto& interpolation : interpolations) {
        auto caller = WaterfallCaller();
        caller.insert(caller.end() - 1, interpolation.code.begin(), interpolation.code.end());
        const auto program = RdnaInstructionDecoder{}.Decode(caller);
        const auto calls = FindTableCalls(program, SwappcInfo{false, 0u, 16u});
        Check(calls.size() == 1u, std::string(interpolation.name) + ": the waterfall caller holds no table call");
        std::string refusal;
        const auto split = SplitTableCall(program, calls[0], callees, 32u, refusal);
        Check(!split.has_value(), std::string(interpolation.name) + " after the table call reads the m0 a callee writes, but the caller split");
        Check(refusal.find("a callee writes m0") != std::string::npos, std::string(interpolation.name) + ": refused for another reason: " + refusal);
    }
}

}

int main() {
    try {
        CheckSplitPrograms();
        CheckInterpolationAfterCallReadsM0();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "split table call: %s\n", error.what());
        return 1;
    }
    std::puts("split table call tests passed");
    return 0;
}
