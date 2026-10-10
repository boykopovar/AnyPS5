#include <Testing/Test.hpp>
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Recompiler.hpp"

#include <array>
#include <exception>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

using namespace ShaderRecompiler;

using Testing::Case;

void require(bool condition, const std::string& message, std::source_location location = std::source_location::current()) {
    Testing::Require(condition, message, location);
}

bool isVector(const RdnaOperand& operand, std::uint32_t reg) {
    return operand.kind == RdnaOperandKind::VectorRegister && operand.reg == reg;
}

RdnaProgram DecodeWaveControl() {
    const std::vector<std::uint32_t> code{
        0xbf8d0000u, 0xbf8d0001u, 0xbf911234u, 0xbe802204u,
        0xd8661234u, 0x00000001u, 0xd86a0000u, 0x00000000u, 0xd86e0000u, 0x00000002u,
        0xd8720000u, 0x00000000u, 0xd8620000u, 0x00000000u, 0xd8760000u, 0x00000003u,
        0xd8fe0004u, 0x05000001u,
        0xbf9f0000u, 0xbf9f0000u, 0xffffffffu,
    };
    auto program = RdnaInstructionDecoder{}.Decode(code);
    require(program.instructions.size() == 12u, "decoding did not stop at the first s_code_end: " + std::to_string(program.instructions.size()) + " instructions");
    return program;
}

const Case codeEnd{"Decode_ProgramPaddedWithCodeEnd_StopsAtTheFirstCodeEnd", [] {
    const auto program = DecodeWaveControl();
    require(program.instructions[11].op == RdnaOpcode::SCodeEnd, "s_code_end");
}};

const Case haltAndTrapReturn{"Decode_HaltAndTrapReturnInstructions_DecodeTheirOperands", [] {
    const auto program = DecodeWaveControl();
    const auto& decoded = program.instructions;
    require(decoded[0].op == RdnaOpcode::SSethalt && decoded[0].source0.value == 0u && decoded[0].sourceCount == 1u, "s_sethalt 0");
    require(decoded[1].op == RdnaOpcode::SSethalt && decoded[1].source0.value == 1u, "s_sethalt 1");
    require(decoded[2].op == RdnaOpcode::SSendmsghalt && decoded[2].source0.value == 0x1234u && decoded[2].sourceCount == 1u, "s_sendmsghalt 0x1234");
    require(decoded[3].op == RdnaOpcode::SRfeB64 && decoded[3].destination.kind == RdnaOperandKind::Null && decoded[3].sourceCount == 1u &&
        decoded[3].source0.kind == RdnaOperandKind::ScalarRegister && decoded[3].source0.reg == 4u, "s_rfe_b64 s[4:5]");
}};

const Case globalWaveSync{"Decode_GlobalWaveSyncInstructions_DecodeAsGdsOperations", [] {
    const auto program = DecodeWaveControl();
    const auto& decoded = program.instructions;
    const std::array<std::tuple<RdnaOpcode, std::uint32_t, std::uint32_t, const char*>, 6> gws{{
        {RdnaOpcode::DsGwsInit, 1u, 0x1234u, "ds_gws_init v1 offset:4660 gds"},
        {RdnaOpcode::DsGwsSemaV, 0u, 0u, "ds_gws_sema_v gds"},
        {RdnaOpcode::DsGwsSemaBr, 1u, 0u, "ds_gws_sema_br v2 gds"},
        {RdnaOpcode::DsGwsSemaP, 0u, 0u, "ds_gws_sema_p gds"},
        {RdnaOpcode::DsGwsSemaReleaseAll, 0u, 0u, "ds_gws_sema_release_all gds"},
        {RdnaOpcode::DsGwsBarrier, 1u, 0u, "ds_gws_barrier v3 gds"},
    }};
    const std::array<std::uint32_t, 6> dataRegisters{1u, 0u, 2u, 0u, 0u, 3u};
    for (std::size_t index = 0; index < gws.size(); ++index) {
        const auto& [opcode, sources, offset, name] = gws[index];
        const auto& instruction = decoded[4u + index];
        require(instruction.op == opcode && instruction.gds && instruction.sourceCount == sources && instruction.memoryOffset == offset &&
            instruction.destination.kind == RdnaOperandKind::None && instruction.wordCount == 2u, name);
        if (sources != 0u) require(isVector(instruction.source0, dataRegisters[index]), std::string(name) + " reads the wrong data register");
    }
}};

const Case orderedCount{"Decode_OrderedCount_DecodesAsGdsCounter", [] {
    const auto program = DecodeWaveControl();
    const auto& ordered = program.instructions[10];
    require(ordered.op == RdnaOpcode::DsOrderedCount && ordered.gds && ordered.memoryOffset == 4u && isVector(ordered.destination, 5u) &&
        isVector(ordered.source0, 1u) && ordered.sourceCount == 1u, "ds_ordered_count v5, v1 offset:4 gds");
}};

std::string recompile(std::span<const std::uint32_t> code) {
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
    request.target.subgroupSize = 64u;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 49152;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    try {
        const auto result = Recompile(request);
        require(!result.spirv.empty(), "no SPIR-V");
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

std::vector<std::uint32_t> withStore(std::initializer_list<std::uint32_t> words) {
    std::vector<std::uint32_t> code(words);
    code.insert(code.end(), {0x7e020281u, 0xe0700000u, 0x80000100u, 0xbf810000u});
    return code;
}

const Case sethaltZero{"Recompile_SethaltZero_IsAccepted", [] {
    require(recompile(withStore({0xbf8d0000u})).empty(), "s_sethalt 0 is refused: " + recompile(withStore({0xbf8d0000u})));
}};

const Case trailingCodeEnd{"Recompile_CodeEndPaddingAfterLastBranch_IsAccepted", [] {
    const std::vector<std::uint32_t> trailingLoop{0x7e020281u, 0xbf820003u, 0xe0700000u, 0x80000100u, 0xbf810000u, 0xbf82fffcu, 0xbf9f0000u, 0xbf9f0000u, 0xffffffffu};
    require(recompile(trailingLoop).empty(), "a program padded with s_code_end after its last branch is refused: " + recompile(trailingLoop));
}};

const Case refusedWaveControl{"Recompile_WaveHaltTrapReturnOrGdsInstruction_IsRefused", [] {
    const std::vector<std::tuple<std::string_view, std::vector<std::uint32_t>, std::string_view>> refused{
        {"s_sethalt 1", withStore({0xbf8d0001u}), "s_sethalt 1 at pc 0 halts the wave"},
        {"s_sendmsghalt", withStore({0xbf910001u}), "s_sendmsghalt 1 at pc 0 halts the wave"},
        {"s_code_end", {0x7e020281u, 0xbf9f0000u}, "s_code_end at pc 4 is reached"},
        {"s_rfe_b64", withStore({0xbe802204u}), "s_rfe_b64 at pc 0 returns from a trap handler"},
        {"ds_gws_init", withStore({0xd8660000u, 0x00000001u}), "ds_gws_init at pc 0: global wave sync"},
        {"ds_gws_sema_v", withStore({0xd86a0000u, 0x00000000u}), "ds_gws_sema_v at pc 0: global wave sync"},
        {"ds_gws_sema_br", withStore({0xd86e0000u, 0x00000002u}), "ds_gws_sema_br at pc 0: global wave sync"},
        {"ds_gws_sema_p", withStore({0xd8720000u, 0x00000000u}), "ds_gws_sema_p at pc 0: global wave sync"},
        {"ds_gws_sema_release_all", withStore({0xd8620000u, 0x00000000u}), "ds_gws_sema_release_all at pc 0: global wave sync"},
        {"ds_gws_barrier", withStore({0xd8760000u, 0x00000003u}), "ds_gws_barrier at pc 0: global wave sync"},
        {"ds_ordered_count", withStore({0xd8fe0000u, 0x05000001u}), "ds_ordered_count at pc 0: GDS counters"},
    };
    for (const auto& [name, code, message] : refused) {
        const auto error = recompile(code);
        require(error.find(message) != std::string::npos, std::string(name) + ": expected a refusal with '" + std::string(message) + "', got '" + error + "'");
    }
}};

} // namespace
