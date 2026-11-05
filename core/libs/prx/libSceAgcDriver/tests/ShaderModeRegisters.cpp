#include "ControlFlow/GraphBuilder.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Recompiler.hpp"
#include "Translation/InstructionTranslator.hpp"
#include "BdaAbi.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace ShaderRecompiler;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

IrProgram translate(std::span<const std::uint32_t> code, std::optional<ShaderFloatMode> mode, std::uint32_t waveSize = 32) {
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    const auto graph = GraphBuilder{}.Build(decoded);
    const ShaderComputeInputInfo compute{};
    TranslateOptions options;
    options.stage = ShaderStageKind::Compute;
    options.waveSize = waveSize;
    options.floatMode = mode;
    options.inputInfo.compute = &compute;
    return InstructionTranslator{}.Translate(decoded, graph, options);
}

void expectRegister(const IrProgram& program, std::uint32_t index, std::uint32_t expected) {
    const IrValue* value = nullptr;
    for (const auto& block : program.Blocks()) {
        for (const auto* instruction : block->Instructions()) {
            if (instruction->Opcode() != IrOpcode::SetScalarRegister) continue;
            const auto reg = instruction->Argument(0)->Register();
            if (reg.bank == RegisterBank::Scalar && reg.index == index) value = instruction->Argument(1);
        }
    }
    require(value && value->HasImmediate() && value->ImmediateU32() == expected,
            "MODE read into s" + std::to_string(index) + " did not produce " + std::to_string(expected));
}

void expectRejected(std::span<const std::uint32_t> code, std::optional<ShaderFloatMode> mode, std::string_view diagnostic, std::uint32_t pc = 0) {
    try {
        static_cast<void>(translate(code, mode));
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        require(message.find(diagnostic) != std::string::npos && message.find("at pc " + std::to_string(pc) + " ") != std::string::npos,
                "unexpected MODE diagnostic: " + message);
        return;
    }
    throw std::runtime_error("unsupported MODE operation was accepted: " + std::string(diagnostic));
}

void verifyReads() {
    constexpr std::array<std::uint32_t, 11> code{
        0xb9140001, 0xb9150801, 0xb9161001, 0xb9171801, 0xb9180041,
        0xb9190841, 0xb91a1041, 0xb91b0081, 0xb91c0881, 0xb91d00c1, 0xbf810000
    };
    constexpr std::array<std::uint32_t, 10> offsets{0, 0, 0, 0, 1, 1, 1, 2, 2, 3};
    constexpr std::array<std::uint32_t, 10> masks{1, 3, 7, 15, 1, 3, 7, 1, 3, 1};
    for (const auto waveSize : {32u, 64u}) {
        for (std::uint32_t mode = 0; mode < 256; ++mode) {
            const auto program = translate(code, ShaderFloatMode{mode, true, true, true}, waveSize);
            for (std::uint32_t index = 0; index < offsets.size(); ++index) {
                expectRegister(program, 20 + index, (mode >> offsets[index]) & masks[index]);
            }
        }
        const auto legacy = translate(code, std::nullopt, waveSize);
        for (std::uint32_t index = 0; index < offsets.size(); ++index) expectRegister(legacy, 20 + index, 0);
    }
}

std::vector<std::uint32_t> opcodeSequence(const IrProgram& program) {
    std::vector<std::uint32_t> sequence;
    for (const auto& block : program.Blocks()) {
        for (const auto* instruction : block->Instructions()) {
            if (instruction->Opcode() != IrOpcode::ControlNop) sequence.push_back(static_cast<std::uint32_t>(instruction->Opcode()));
        }
    }
    return sequence;
}

constexpr std::uint32_t endProgram = 0xbf810000;
constexpr std::uint32_t getregMode = 0xb9141801;
constexpr std::uint32_t vAddF32 = 0x06000300;
constexpr std::uint32_t vCvtF32I32 = 0x7e000b00;
constexpr std::uint32_t vCvtF32U32 = 0x7e000d00;
constexpr std::uint32_t vCvtF16F32 = 0x7e001500;
constexpr std::uint32_t vAddF16 = 0x64000300;

std::uint32_t roundMode(std::uint32_t value) {
    return 0xbfa40000u | value;
}

void requireOpcode(std::uint32_t word, RdnaOpcode expected) {
    const std::array<std::uint32_t, 3> code{word, 0u, endProgram};
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    require(!decoded.instructions.empty() && decoded.instructions.front().op == expected, "test encoding " + std::to_string(word) + " does not decode to the expected opcode");
}

void verifyEncodings() {
    for (const auto& [word, opcode] : {std::pair{vAddF32, RdnaOpcode::VAddF32}, {vCvtF32I32, RdnaOpcode::VCvtF32I32}, {vCvtF32U32, RdnaOpcode::VCvtF32U32},
                                       {vCvtF16F32, RdnaOpcode::VCvtF16F32}, {vAddF16, RdnaOpcode::VAddF16}, {roundMode(8), RdnaOpcode::SRoundMode}}) {
        const std::array<std::uint32_t, 2> code{word, endProgram};
        const auto decoded = RdnaInstructionDecoder{}.Decode(code);
        require(decoded.instructions.front().op == opcode, "test encoding " + std::to_string(word) + " does not decode to the expected opcode");
    }
    requireOpcode(0xba801801, RdnaOpcode::SSetregImm32B32);
    const std::array<std::uint32_t, 2> sgpr{0xb9800001, endProgram};
    require(RdnaInstructionDecoder{}.Decode(sgpr).instructions.front().op == RdnaOpcode::SSetregB32, "s_setreg_b32 encoding does not decode");
}

void verifyDynamicWrites() {
    for (const auto instruction : {0xb9800001u, 0xb9801801u, 0xb9800041u}) {
        const ShaderFloatMode mode{0xc0};
        const std::array<std::uint32_t, 2> unused{instruction, endProgram};
        static_cast<void>(translate(unused, mode));
        expectRejected(unused, std::nullopt, "s_setreg_b32");
        const std::array<std::uint32_t, 3> read{instruction, getregMode, endProgram};
        expectRejected(read, mode, "s_getreg_b32", 4);
        const std::array<std::uint32_t, 3> used{instruction, vAddF32, endProgram};
        expectRejected(used, mode, "reads MODE rounding bits", 4);
    }
    for (const auto instruction : {0xb9801881u, 0xb980f801u, 0xb9800101u}) {
        const std::array<std::uint32_t, 2> code{instruction, endProgram};
        expectRejected(code, std::nullopt, "s_setreg_b32");
        expectRejected(code, ShaderFloatMode{0xc0}, "s_setreg_b32");
    }
}

void verifyDynamicWriteSurvivesPartialOverwrite() {
    constexpr ShaderFloatMode mode{0xc0};
    const std::array<std::uint32_t, 6> code{0xb9801801, 0xba801801, 0, getregMode, vAddF32, endProgram};
    expectRegister(translate(code, mode), 20, 0);
    const std::array<std::uint32_t, 6> partial{0xb9801801, 0xba800801, 0, getregMode, vAddF32, endProgram};
    expectRejected(partial, mode, "s_getreg_b32", 12);
    const std::array<std::uint32_t, 6> split{0xb9801801, 0xba800801, 0, 0xba800881, 0, endProgram};
    static_cast<void>(translate(split, mode));
}

void verifyImmediateWrites() {
    constexpr std::array<std::uint32_t, 4> instructions{0xba801801, 0xba800801, 0xba800881, 0xba800841};
    constexpr std::array<std::uint32_t, 4> masks{15, 3, 12, 6};
    constexpr std::array<std::uint32_t, 4> offsets{0, 0, 2, 1};
    for (std::uint32_t rounding = 0; rounding < 16; ++rounding) {
        for (std::uint32_t value = 0; value < 16; ++value) {
            for (std::uint32_t field = 0; field < instructions.size(); ++field) {
                const std::array<std::uint32_t, 4> code{instructions[field], 0xfffffff0u | value, getregMode, endProgram};
                const ShaderFloatMode mode{0xc0u | rounding};
                const std::uint32_t expected = (rounding & ~masks[field]) | ((value << offsets[field]) & masks[field]);
                expectRegister(translate(code, mode), 20, expected);
            }
        }
    }
    constexpr std::array<std::uint32_t, 3> full{0xba801801, 0, endProgram};
    static_cast<void>(translate(full, std::nullopt));
    constexpr std::array<std::uint32_t, 3> nonzero{0xba801801, 15, endProgram};
    expectRejected(nonzero, std::nullopt, "s_setreg_imm32_b32");
    constexpr std::array<std::uint32_t, 3> denorm{0xba801881, 0, endProgram};
    expectRejected(denorm, ShaderFloatMode{0xc0}, "s_setreg_imm32_b32");
    constexpr std::array<std::uint32_t, 3> otherRegister{0xba801802, 0, endProgram};
    expectRejected(otherRegister, ShaderFloatMode{0xc0}, "s_setreg_imm32_b32");
}

void verifyRoundInstruction() {
    for (std::uint32_t rounding = 0; rounding < 16; ++rounding) {
        for (std::uint32_t value = 0; value < 16; ++value) {
            for (const auto upper : {0u, 0x8000u}) {
                const std::array<std::uint32_t, 3> code{roundMode(upper | value), getregMode, endProgram};
                expectRegister(translate(code, ShaderFloatMode{0xc0u | rounding}), 20, value);
            }
        }
    }
    constexpr std::array<std::uint32_t, 2> code{0xbfa40000, endProgram};
    static_cast<void>(translate(code, std::nullopt));
    constexpr std::array<std::uint32_t, 2> nonzero{0xbfa40003, endProgram};
    expectRejected(nonzero, std::nullopt, "s_round_mode");
}

void verifyUnsupportedReads() {
    for (const auto instruction : {0xb9140002u, 0xb9140101u, 0xb91408c1u, 0xb914f801u}) {
        const std::array<std::uint32_t, 2> code{instruction, endProgram};
        expectRejected(code, ShaderFloatMode{0xc0}, "s_getreg_b32");
    }
}

void verifyBranch() {
    for (std::uint32_t rounding = 0; rounding < 16; ++rounding) {
        const ShaderFloatMode mode{0xc0u | rounding};
        std::array<std::uint32_t, 5> code{0xbf840002, 0xba801801, rounding, getregMode, endProgram};
        expectRegister(translate(code, mode), 20, rounding);
        code[2] ^= 1u;
        expectRejected(code, mode, "s_getreg_b32", 12);
    }
}

void verifyDiamond() {
    for (std::uint32_t rounding = 0; rounding < 16; ++rounding) {
        const ShaderFloatMode mode{0xc0u | rounding};
        const std::uint32_t other = rounding ^ 8u;
        const std::array<std::uint32_t, 9> same{0xbf840003, 0xba801801, other, 0xbf820002, 0xba801801, other, getregMode, endProgram, endProgram};
        expectRegister(translate(same, mode), 20, other);
        const std::array<std::uint32_t, 9> differing{0xbf840003, 0xba801801, other, 0xbf820002, 0xba801801, rounding, getregMode, endProgram, endProgram};
        expectRejected(differing, mode, "s_getreg_b32", 24);
        const std::array<std::uint32_t, 9> differingUse{0xbf840003, 0xba801801, 3, 0xbf820002, 0xba801801, 0, vCvtF32I32, endProgram, endProgram};
        if (rounding == 0u) expectRejected(differingUse, mode, "reads MODE rounding bits", 24);
    }
}

void verifyRoundingBeforeDirected32Use() {
    for (const auto instruction : {vCvtF32I32, vCvtF32U32}) {
        for (std::uint32_t rounding = 1; rounding < 4; ++rounding) {
            const std::array<std::uint32_t, 3> runtime{roundMode(rounding), instruction, endProgram};
            const std::array<std::uint32_t, 2> declared{instruction, endProgram};
            const auto converted = opcodeSequence(translate(runtime, ShaderFloatMode{0xc0}));
            require(converted == opcodeSequence(translate(declared, ShaderFloatMode{0xc0u | rounding})), "runtime rounding does not match the declared rounding");
            require(converted != opcodeSequence(translate(declared, ShaderFloatMode{0xc0})), "runtime rounding did not change the conversion");
        }
    }
}

void verifyRoundingRestoredBeforeUse() {
    for (const auto instruction : {vAddF32, vAddF16}) {
        for (std::uint32_t directed = 1; directed < 16; ++directed) {
            const std::array<std::uint32_t, 5> restored{roundMode(directed), roundMode(0), instruction, roundMode(directed), endProgram};
            const std::array<std::uint32_t, 2> plain{instruction, endProgram};
            require(opcodeSequence(translate(restored, ShaderFloatMode{0xc0})) == opcodeSequence(translate(plain, ShaderFloatMode{0xc0})), "restored rounding changed the translation");
        }
    }
}

void verifyUntouchedFieldsStayAccepted() {
    const std::array<std::uint32_t, 4> f16Only{roundMode(8), vAddF32, roundMode(0), endProgram};
    const std::array<std::uint32_t, 2> plain{vAddF32, endProgram};
    require(opcodeSequence(translate(f16Only, ShaderFloatMode{0xc0})) == opcodeSequence(translate(plain, ShaderFloatMode{0xc0})), "f16 rounding changed an f32 operation");
    const std::array<std::uint32_t, 4> f32Only{roundMode(2), vAddF16, roundMode(0), endProgram};
    const std::array<std::uint32_t, 2> plain16{vAddF16, endProgram};
    require(opcodeSequence(translate(f32Only, ShaderFloatMode{0xc0})) == opcodeSequence(translate(plain16, ShaderFloatMode{0xc0})), "f32 rounding changed an f16 operation");
}

void verifyUnmodelledDirectedRounding() {
    for (std::uint32_t directed = 1; directed < 4; ++directed) {
        const std::array<std::uint32_t, 3> f32{roundMode(directed), vAddF32, endProgram};
        expectRejected(f32, ShaderFloatMode{0xc0}, "does not model directed rounding", 4);
        const std::array<std::uint32_t, 3> f16{roundMode(directed << 2u), vAddF16, endProgram};
        expectRejected(f16, ShaderFloatMode{0xc0}, "does not model directed rounding", 4);
    }
}

void verifyDirectedF16Conversion() {
    for (std::uint32_t rounding = 1; rounding < 4; ++rounding) {
        const std::array<std::uint32_t, 3> runtime{roundMode(rounding << 2u), vCvtF16F32, endProgram};
        const std::array<std::uint32_t, 2> declared{vCvtF16F32, endProgram};
        const auto converted = opcodeSequence(translate(runtime, ShaderFloatMode{0xc0}));
        require(converted == opcodeSequence(translate(declared, ShaderFloatMode{0xc0u | (rounding << 2u)})), "runtime f16 rounding does not match the declared rounding");
        expectRejected(runtime, ShaderFloatMode{0x00}, "f16 denormals not kept", 4);
    }
}

void verifyRoundingInLoop() {
    const std::array<std::uint32_t, 6> stable{roundMode(0), vAddF32, 0xbf84fffd, roundMode(0), endProgram, endProgram};
    static_cast<void>(translate(stable, ShaderFloatMode{0xc0}));
    const std::array<std::uint32_t, 6> alternating{roundMode(0), vAddF32, 0xbf84fffd, roundMode(3), endProgram, endProgram};
    static_cast<void>(translate(alternating, ShaderFloatMode{0xc0}));
    const std::array<std::uint32_t, 6> carried{vAddF32, roundMode(3), 0xbf84fffd, getregMode, endProgram, endProgram};
    expectRejected(carried, ShaderFloatMode{0xc0}, "reads MODE rounding bits", 0);
}

void verifyRecompilePropagation() {
    constexpr std::array<std::uint32_t, 2> code{0xbfa40005, 0xbf810000};
    constexpr std::array<std::uint32_t, 2> capabilities{1, 61};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000, code, 0, {}};
    request.context.waveSize = 64;
    request.context.compute = ShaderComputeStageInfo{{1, 1, 1}, 0, {false, false, false}, false, 0};
    request.context.floatMode = ShaderFloatMode{0xc5};
    request.target.vulkanVersion = 0x00403000;
    request.target.spirvVersion = 0x00010600;
    request.target.subgroupSize = 64;
    request.target.bdaAbiVersion = BdaAbi::Version;
    request.target.supportedCapabilities = capabilities;
    request.target.maxWorkgroupSize = {1024, 1024, 64};
    request.target.maxWorkgroupInvocations = 1024;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    require(!Recompile(request).spirv.empty(), "declared MODE did not reach full recompilation");
    request.context.floatMode.reset();
    try {
        static_cast<void>(Recompile(request));
    } catch (const std::runtime_error& error) {
        require(std::string(error.what()).find("s_round_mode") != std::string::npos, "unexpected full recompilation failure");
        return;
    }
    throw std::runtime_error("full recompilation ignored a runtime MODE change");
}

}

int main() {
    struct Check {
        const char* name;
        void (*run)();
    };
    constexpr std::array<Check, 16> checks{{
        {"encodings", verifyEncodings}, {"reads", verifyReads}, {"dynamicWrites", verifyDynamicWrites},
        {"partialOverwrite", verifyDynamicWriteSurvivesPartialOverwrite}, {"immediateWrites", verifyImmediateWrites},
        {"roundInstruction", verifyRoundInstruction}, {"unsupportedReads", verifyUnsupportedReads}, {"branch", verifyBranch},
        {"diamond", verifyDiamond}, {"directed32", verifyRoundingBeforeDirected32Use}, {"restored", verifyRoundingRestoredBeforeUse},
        {"untouchedFields", verifyUntouchedFieldsStayAccepted}, {"unmodelled", verifyUnmodelledDirectedRounding},
        {"directedF16", verifyDirectedF16Conversion}, {"loop", verifyRoundingInLoop}, {"propagation", verifyRecompilePropagation},
    }};
    int failures = 0;
    for (const auto& check : checks) {
        try {
            check.run();
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s: %s\n", check.name, error.what());
            ++failures;
        }
    }
    if (failures != 0) return 1;
    std::puts("Shader MODE register tests passed");
    return 0;
}
