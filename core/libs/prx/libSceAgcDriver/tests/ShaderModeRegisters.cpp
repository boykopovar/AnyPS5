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

void verifyDynamicWrites() {
    for (const auto instruction : {0xb9800001u, 0xb9801801u, 0xb9800041u, 0xb9801881u, 0xb980f801u}) {
        const std::array<std::uint32_t, 2> code{instruction, 0xbf810000};
        expectRejected(code, std::nullopt, "s_setreg_b32");
        expectRejected(code, ShaderFloatMode{0xc0}, "s_setreg_b32");
    }
}

void verifyImmediateWrites() {
    constexpr std::array<std::uint32_t, 4> instructions{0xba801801, 0xba800801, 0xba800881, 0xba800841};
    constexpr std::array<std::uint32_t, 4> masks{15, 3, 12, 6};
    constexpr std::array<std::uint32_t, 4> offsets{0, 0, 2, 1};
    for (std::uint32_t rounding = 0; rounding < 16; ++rounding) {
        for (std::uint32_t value = 0; value < 16; ++value) {
            for (std::uint32_t field = 0; field < instructions.size(); ++field) {
                const std::array<std::uint32_t, 4> code{instructions[field], 0xfffffff0u | value, 0xb9141801, 0xbf810000};
                const ShaderFloatMode mode{0xc0u | rounding};
                if (((value << offsets[field]) & masks[field]) != (rounding & masks[field])) {
                    expectRejected(code, mode, "s_setreg_imm32_b32");
                } else {
                    expectRegister(translate(code, mode), 20, rounding);
                }
            }
        }
    }
    constexpr std::array<std::uint32_t, 3> full{0xba801801, 0, 0xbf810000};
    static_cast<void>(translate(full, std::nullopt));
    constexpr std::array<std::uint32_t, 3> nonzero{0xba801801, 15, 0xbf810000};
    constexpr std::array<std::uint32_t, 3> denorm{0xba801881, 0, 0xbf810000};
    expectRejected(nonzero, ShaderFloatMode{0xc0}, "s_setreg_imm32_b32");
    expectRejected(denorm, ShaderFloatMode{0xc0}, "s_setreg_imm32_b32");
}

void verifyRoundInstruction() {
    for (std::uint32_t rounding = 0; rounding < 16; ++rounding) {
        for (std::uint32_t value = 0; value < 16; ++value) {
            for (const auto upper : {0u, 0x8000u}) {
                const std::array<std::uint32_t, 3> code{0xbfa40000u | upper | value, 0xb9141801, 0xbf810000};
                const ShaderFloatMode mode{0xc0u | rounding};
                if (value != rounding) expectRejected(code, mode, "s_round_mode");
                else expectRegister(translate(code, mode), 20, rounding);
            }
        }
    }
    constexpr std::array<std::uint32_t, 2> code{0xbfa40000, 0xbf810000};
    static_cast<void>(translate(code, std::nullopt));
    constexpr std::array<std::uint32_t, 2> nonzero{0xbfa40003, 0xbf810000};
    expectRejected(nonzero, ShaderFloatMode{0xc0}, "s_round_mode");
}

void verifyUnsupportedReads() {
    for (const auto instruction : {0xb9140002u, 0xb9140101u, 0xb91408c1u, 0xb914f801u}) {
        const std::array<std::uint32_t, 2> code{instruction, 0xbf810000};
        expectRejected(code, ShaderFloatMode{0xc0}, "s_getreg_b32");
    }
}

void verifyBranch() {
    for (std::uint32_t rounding = 0; rounding < 16; ++rounding) {
        const ShaderFloatMode mode{0xc0u | rounding};
        std::array<std::uint32_t, 5> code{0xbf840002, 0xba801801, rounding, 0xb9141801, 0xbf810000};
        expectRegister(translate(code, mode), 20, rounding);
        code[2] ^= 1u;
        expectRejected(code, mode, "s_setreg_imm32_b32", 4);
    }
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
    int failures = 0;
    for (const auto check : {verifyReads, verifyDynamicWrites, verifyImmediateWrites, verifyRoundInstruction, verifyUnsupportedReads, verifyBranch, verifyRecompilePropagation}) {
        try {
            check();
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s\n", error.what());
            ++failures;
        }
    }
    if (failures != 0) return 1;
    std::puts("Shader MODE register tests passed");
    return 0;
}
