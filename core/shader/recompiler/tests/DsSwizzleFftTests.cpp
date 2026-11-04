#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include "RdnaDecoder/RdnaOpcode.hpp"
#include "Recompiler.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Assertion failed: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

struct SpirvInst {
    spv::Op opcode;
    std::vector<std::uint32_t> words;
};

std::vector<SpirvInst> ParseSpirvWords(std::span<const std::uint32_t> words) {
    std::vector<SpirvInst> result;
    if (words.size() < 5u) return result;
    std::size_t offset = 5u;
    while (offset < words.size()) {
        const std::uint32_t header = words[offset];
        const std::uint16_t length = static_cast<std::uint16_t>(header >> 16u);
        const auto opcode = static_cast<spv::Op>(header & 0xFFFFu);
        if (length == 0 || offset + length > words.size()) break;
        SpirvInst inst{};
        inst.opcode = opcode;
        inst.words.assign(words.begin() + offset, words.begin() + offset + length);
        result.push_back(std::move(inst));
        offset += length;
    }
    return result;
}

std::unordered_map<std::uint32_t, std::uint32_t> ExtractConstants(const std::vector<SpirvInst>& instructions) {
    std::unordered_map<std::uint32_t, std::uint32_t> constants;
    for (const auto& inst : instructions) {
        if (inst.opcode == spv::Op::OpConstant && inst.words.size() >= 4u) {
            const std::uint32_t resultId = inst.words[2];
            const std::uint32_t value = inst.words[3];
            constants[resultId] = value;
        }
    }
    return constants;
}

void TestDecoderFftOffsets() {
    constexpr std::array<std::uint16_t, 9> offsets{
        0xE000u, 0xE001u, 0xE002u, 0xE004u, 0xE008u, 0xE010u, 0xE01Fu, 0xEA85u, 0xFFFFu
    };

    for (std::uint16_t offset : offsets) {
        const std::uint32_t word0 = (0x36u << 26u) | (0x35u << 18u) | static_cast<std::uint32_t>(offset);
        const std::uint32_t word1 = (7u << 24u) | 15u;
        const std::array<std::uint32_t, 2> code{word0, word1};

        const auto inst = ShaderRecompiler::DecodeRdnaMemoryOp(code, 0u);
        Require(inst.family == ShaderRecompiler::RdnaInstructionFamily::DS, "DS family expected");
        Require(inst.op == ShaderRecompiler::RdnaOpcode::DsSwizzleB32, "DsSwizzleB32 opcode expected");
        Require(inst.memoryOffset == static_cast<std::uint32_t>(offset), "memoryOffset mismatch");
        Require(inst.destination.kind == ShaderRecompiler::RdnaOperandKind::VectorRegister, "destination must be VGPR");
        Require(inst.destination.reg == 7u, "destination reg mismatch");
        Require(inst.source0.kind == ShaderRecompiler::RdnaOperandKind::VectorRegister, "source0 must be VGPR");
        Require(inst.source0.reg == 15u, "source0 reg mismatch");
        Require(inst.sourceCount == 1u, "sourceCount must be 1");
        Require(inst.wordCount == 2u, "wordCount must be 2");
    }
}

void TestDecoderRefusals() {
    bool caughtTruncated = false;
    try {
        const std::array<std::uint32_t, 1> code{(0x36u << 26u) | (0x35u << 18u) | 0xE001u};
        (void)ShaderRecompiler::DecodeRdnaMemoryOp(code, 0u);
    } catch (const std::exception&) {
        caughtTruncated = true;
    }
    Require(caughtTruncated, "truncated DS instruction must throw");

    bool caughtGds = false;
    try {
        const std::uint32_t word0 = (0x36u << 26u) | (0x35u << 18u) | (1u << 17u) | 0xE001u;
        const std::uint32_t word1 = (1u << 24u) | 2u;
        const std::array<std::uint32_t, 2> code{word0, word1};
        (void)ShaderRecompiler::DecodeRdnaMemoryOp(code, 0u);
    } catch (const std::exception&) {
        caughtGds = true;
    }
    Require(caughtGds, "GDS swizzle must throw");

    bool caughtInvalidOpcode = false;
    try {
        const std::array<std::uint32_t, 2> code{0x10000000u, 0u};
        (void)ShaderRecompiler::DecodeRdnaMemoryOp(code, 0u);
    } catch (const std::exception&) {
        caughtInvalidOpcode = true;
    }
    Require(caughtInvalidOpcode, "non-memory opcode prefix must throw");
}

void TestButterflyPatternsWave32() {
    for (std::uint32_t stride = 0u; stride < 32u; ++stride) {
        const std::uint16_t offset = static_cast<std::uint16_t>(0xE000u | stride);
        const std::uint32_t word0 = (0x36u << 26u) | (0x35u << 18u) | offset;
        const std::uint32_t word1 = (2u << 24u) | 4u;
        const std::array<std::uint32_t, 2> code{word0, word1};

        const auto decoded = ShaderRecompiler::DecodeRdnaMemoryOp(code, 0u);
        Require(decoded.op == ShaderRecompiler::RdnaOpcode::DsSwizzleB32, "decoded opcode must be DsSwizzleB32");
        Require(decoded.memoryOffset == offset, "decoded memoryOffset mismatch");

        std::array<bool, 32> hit{};
        for (std::uint32_t lane = 0u; lane < 32u; ++lane) {
            const std::uint32_t target = (lane & 0xffffffe0u) | ((lane & 31u) ^ (decoded.memoryOffset & 0x1fu));
            Require(target < 32u, "Wave32 target lane out of bounds");
            Require(!hit[target], "Wave32 target collision (not a bijection)");
            hit[target] = true;

            const std::uint32_t roundTrip = (target & 0xffffffe0u) | ((target & 31u) ^ (decoded.memoryOffset & 0x1fu));
            Require(roundTrip == lane, "Wave32 butterfly involution failed");
        }
    }

    auto TargetOf = [](std::uint32_t lane, std::uint16_t offset) -> std::uint32_t {
        const std::uint32_t word0 = (0x36u << 26u) | (0x35u << 18u) | offset;
        const std::uint32_t word1 = (2u << 24u) | 4u;
        const std::array<std::uint32_t, 2> code{word0, word1};
        const auto decoded = ShaderRecompiler::DecodeRdnaMemoryOp(code, 0u);
        return (lane & 0xffffffe0u) | ((lane & 31u) ^ (decoded.memoryOffset & 0x1fu));
    };

    Require(TargetOf(0u, 0xE000u) == 0u, "stride 0 lane 0");
    Require(TargetOf(31u, 0xE000u) == 31u, "stride 0 lane 31");

    Require(TargetOf(0u, 0xE001u) == 1u, "stride 1 lane 0");
    Require(TargetOf(1u, 0xE001u) == 0u, "stride 1 lane 1");
    Require(TargetOf(2u, 0xE001u) == 3u, "stride 1 lane 2");
    Require(TargetOf(3u, 0xE001u) == 2u, "stride 1 lane 3");
    Require(TargetOf(30u, 0xE001u) == 31u, "stride 1 lane 30");
    Require(TargetOf(31u, 0xE001u) == 30u, "stride 1 lane 31");

    Require(TargetOf(0u, 0xE002u) == 2u, "stride 2 lane 0");
    Require(TargetOf(1u, 0xE002u) == 3u, "stride 2 lane 1");
    Require(TargetOf(2u, 0xE002u) == 0u, "stride 2 lane 2");
    Require(TargetOf(3u, 0xE002u) == 1u, "stride 2 lane 3");

    Require(TargetOf(0u, 0xE004u) == 4u, "stride 4 lane 0");
    Require(TargetOf(4u, 0xE004u) == 0u, "stride 4 lane 4");
    Require(TargetOf(3u, 0xE004u) == 7u, "stride 4 lane 3");
    Require(TargetOf(7u, 0xE004u) == 3u, "stride 4 lane 7");

    Require(TargetOf(0u, 0xE008u) == 8u, "stride 8 lane 0");
    Require(TargetOf(8u, 0xE008u) == 0u, "stride 8 lane 8");
    Require(TargetOf(7u, 0xE008u) == 15u, "stride 8 lane 7");
    Require(TargetOf(15u, 0xE008u) == 7u, "stride 8 lane 15");

    Require(TargetOf(0u, 0xE010u) == 16u, "stride 16 lane 0");
    Require(TargetOf(16u, 0xE010u) == 0u, "stride 16 lane 16");
    Require(TargetOf(15u, 0xE010u) == 31u, "stride 16 lane 15");
    Require(TargetOf(31u, 0xE010u) == 15u, "stride 16 lane 31");

    Require(TargetOf(0u, 0xE01Fu) == 31u, "stride 31 lane 0");
    Require(TargetOf(31u, 0xE01Fu) == 0u, "stride 31 lane 31");
    Require(TargetOf(1u, 0xE01Fu) == 30u, "stride 31 lane 1");
    Require(TargetOf(15u, 0xE01Fu) == 16u, "stride 31 lane 15");
}

void TestButterflyPatternsWave64() {
    for (std::uint32_t stride = 0u; stride < 32u; ++stride) {
        const std::uint16_t offset = static_cast<std::uint16_t>(0xE000u | stride);
        const std::uint32_t word0 = (0x36u << 26u) | (0x35u << 18u) | offset;
        const std::uint32_t word1 = (2u << 24u) | 4u;
        const std::array<std::uint32_t, 2> code{word0, word1};

        const auto decoded = ShaderRecompiler::DecodeRdnaMemoryOp(code, 0u);
        Require(decoded.op == ShaderRecompiler::RdnaOpcode::DsSwizzleB32, "decoded opcode must be DsSwizzleB32");

        std::array<bool, 64> hit{};
        for (std::uint32_t lane = 0u; lane < 64u; ++lane) {
            const std::uint32_t target = (lane & 0xffffffe0u) | ((lane & 31u) ^ (decoded.memoryOffset & 0x1fu));
            Require(target < 64u, "Wave64 target lane out of bounds");
            Require(!hit[target], "Wave64 target collision");
            hit[target] = true;

            if (lane < 32u) {
                Require(target < 32u, "Wave64 lower half must not cross into upper half");
            } else {
                Require(target >= 32u, "Wave64 upper half must not cross into lower half");
            }

            const std::uint32_t roundTrip = (target & 0xffffffe0u) | ((target & 31u) ^ (decoded.memoryOffset & 0x1fu));
            Require(roundTrip == lane, "Wave64 butterfly involution failed");
        }
    }

    auto TargetOf = [](std::uint32_t lane, std::uint16_t offset) -> std::uint32_t {
        const std::uint32_t word0 = (0x36u << 26u) | (0x35u << 18u) | offset;
        const std::uint32_t word1 = (2u << 24u) | 4u;
        const std::array<std::uint32_t, 2> code{word0, word1};
        const auto decoded = ShaderRecompiler::DecodeRdnaMemoryOp(code, 0u);
        return (lane & 0xffffffe0u) | ((lane & 31u) ^ (decoded.memoryOffset & 0x1fu));
    };

    Require(TargetOf(32u, 0xE000u) == 32u, "wave64 stride 0 lane 32");
    Require(TargetOf(63u, 0xE000u) == 63u, "wave64 stride 0 lane 63");

    Require(TargetOf(32u, 0xE001u) == 33u, "wave64 stride 1 lane 32");
    Require(TargetOf(33u, 0xE001u) == 32u, "wave64 stride 1 lane 33");
    Require(TargetOf(62u, 0xE001u) == 63u, "wave64 stride 1 lane 62");
    Require(TargetOf(63u, 0xE001u) == 62u, "wave64 stride 1 lane 63");

    Require(TargetOf(32u, 0xE002u) == 34u, "wave64 stride 2 lane 32");
    Require(TargetOf(34u, 0xE002u) == 32u, "wave64 stride 2 lane 34");

    Require(TargetOf(32u, 0xE004u) == 36u, "wave64 stride 4 lane 32");
    Require(TargetOf(36u, 0xE004u) == 32u, "wave64 stride 4 lane 36");

    Require(TargetOf(32u, 0xE008u) == 40u, "wave64 stride 8 lane 32");
    Require(TargetOf(40u, 0xE008u) == 32u, "wave64 stride 8 lane 40");

    Require(TargetOf(32u, 0xE010u) == 48u, "wave64 stride 16 lane 32");
    Require(TargetOf(48u, 0xE010u) == 32u, "wave64 stride 16 lane 48");
    Require(TargetOf(47u, 0xE010u) == 63u, "wave64 stride 16 lane 47");
    Require(TargetOf(63u, 0xE010u) == 47u, "wave64 stride 16 lane 63");

    Require(TargetOf(32u, 0xE01Fu) == 63u, "wave64 stride 31 lane 32");
    Require(TargetOf(63u, 0xE01Fu) == 32u, "wave64 stride 31 lane 63");
    Require(TargetOf(33u, 0xE01Fu) == 62u, "wave64 stride 31 lane 33");
    Require(TargetOf(47u, 0xE01Fu) == 48u, "wave64 stride 31 lane 47");
}

void TestShaderRecompilerSpirvLowering(std::uint32_t waveSize, std::uint16_t offset) {
    const std::uint32_t word0 = (0x36u << 26u) | (0x35u << 18u) | static_cast<std::uint32_t>(offset);
    const std::uint32_t word1 = (1u << 24u) | 0u;
    const std::uint32_t writeWord0 = (0x36u << 26u) | (0x0Du << 18u);
    const std::uint32_t writeWord1 = (1u << 8u) | 0u;
    const std::uint32_t endWord = 0xBF810000u;
    const std::vector<std::uint32_t> code{word0, word1, writeWord0, writeWord1, endWord};

    ShaderRecompiler::RecompileRequest request{};
    request.shader.stage = ShaderRecompiler::ShaderStage::Compute;
    request.shader.code = code;
    request.shader.codeAddress = 0x1000ull;
    request.context.waveSize = waveSize;
    request.context.compute = ShaderRecompiler::ShaderComputeStageInfo{{waveSize, 1u, 1u}, 1024u, {false, false, false}, false, 1u, {0u, 0u, 0u}, 0u};

    const auto result = ShaderRecompiler::Recompile(request);
    Require(!result.spirv.empty(), "compiled SPIR-V must not be empty");
    Require(result.spirv.size() >= 5u, "SPIR-V header truncated");
    Require(result.spirv.Words()[0] == spv::MagicNumber, "SPIR-V magic number mismatch");

    const auto instructions = ParseSpirvWords(result.spirv.Words());
    Require(!instructions.empty(), "SPIR-V instructions must not be empty");
    const auto constants = ExtractConstants(instructions);

    bool foundXor = false;
    bool foundShuffle = false;
    for (const auto& inst : instructions) {
        if (inst.opcode == spv::Op::OpBitwiseXor && inst.words.size() >= 5u) {
            const std::uint32_t constOperandId = inst.words[4];
            const auto it = constants.find(constOperandId);
            if (it != constants.end() && it->second == (offset & 0x1fu)) {
                foundXor = true;
            }
        }
        if (inst.opcode == spv::Op::OpGroupNonUniformShuffle && inst.words.size() >= 6u) {
            const std::uint32_t scopeId = inst.words[3];
            const auto it = constants.find(scopeId);
            if (it != constants.end() && it->second == static_cast<std::uint32_t>(spv::Scope::ScopeSubgroup)) {
                foundShuffle = true;
            }
        }
    }

    Require(foundXor, "SPIR-V OpBitwiseXor with stride constant not found");
    Require(foundShuffle, "SPIR-V OpGroupNonUniformShuffle with ScopeSubgroup not found");
}

void TestDifferentialRotateVsFft() {
    const std::uint16_t rotateOffset = 0xC000u | (3u << 5u);
    const std::uint32_t rotateWord0 = (0x36u << 26u) | (0x35u << 18u) | rotateOffset;
    const std::uint32_t rotateWord1 = (1u << 24u) | 0u;
    const std::uint32_t writeWord0 = (0x36u << 26u) | (0x0Du << 18u);
    const std::uint32_t writeWord1 = (1u << 8u) | 0u;
    const std::uint32_t endWord = 0xBF810000u;
    const std::vector<std::uint32_t> rotateCode{rotateWord0, rotateWord1, writeWord0, writeWord1, endWord};

    ShaderRecompiler::RecompileRequest rotateReq{};
    rotateReq.shader.stage = ShaderRecompiler::ShaderStage::Compute;
    rotateReq.shader.code = rotateCode;
    rotateReq.shader.codeAddress = 0x2000ull;
    rotateReq.context.waveSize = 32u;
    rotateReq.context.compute = ShaderRecompiler::ShaderComputeStageInfo{{32u, 1u, 1u}, 1024u, {false, false, false}, false, 1u, {0u, 0u, 0u}, 0u};

    const auto rotateResult = ShaderRecompiler::Recompile(rotateReq);
    const auto rotateInstructions = ParseSpirvWords(rotateResult.spirv.Words());

    bool rotateHasXor = false;
    bool rotateHasAdd = false;
    for (const auto& inst : rotateInstructions) {
        if (inst.opcode == spv::Op::OpBitwiseXor) rotateHasXor = true;
        if (inst.opcode == spv::Op::OpIAdd) rotateHasAdd = true;
    }
    Require(!rotateHasXor, "Rotate mode must not emit OpBitwiseXor");
    Require(rotateHasAdd, "Rotate mode must emit OpIAdd for rotatedSum");

    const std::uint16_t fftOffset = 0xE003u;
    const std::uint32_t fftWord0 = (0x36u << 26u) | (0x35u << 18u) | fftOffset;
    const std::uint32_t fftWord1 = (1u << 24u) | 0u;
    const std::vector<std::uint32_t> fftCode{fftWord0, fftWord1, writeWord0, writeWord1, endWord};

    ShaderRecompiler::RecompileRequest fftReq{};
    fftReq.shader.stage = ShaderRecompiler::ShaderStage::Compute;
    fftReq.shader.code = fftCode;
    fftReq.shader.codeAddress = 0x3000ull;
    fftReq.context.waveSize = 32u;
    fftReq.context.compute = ShaderRecompiler::ShaderComputeStageInfo{{32u, 1u, 1u}, 1024u, {false, false, false}, false, 1u, {0u, 0u, 0u}, 0u};

    const auto fftResult = ShaderRecompiler::Recompile(fftReq);
    const auto fftInstructions = ParseSpirvWords(fftResult.spirv.Words());
    const auto fftConstants = ExtractConstants(fftInstructions);

    bool fftHasXor = false;
    for (const auto& inst : fftInstructions) {
        if (inst.opcode == spv::Op::OpBitwiseXor && inst.words.size() >= 5u) {
            const auto it = fftConstants.find(inst.words[4]);
            if (it != fftConstants.end() && it->second == 3u) {
                fftHasXor = true;
            }
        }
    }
    Require(fftHasXor, "FFT mode must emit OpBitwiseXor with stride 3");
}

void TestShaderRecompilerEndToEnd() {
    constexpr std::array<std::uint16_t, 8> testOffsets{
        0xE000u, 0xE001u, 0xE002u, 0xE004u, 0xE008u, 0xE010u, 0xE01Fu, 0xFFFFu
    };
    for (std::uint16_t offset : testOffsets) {
        TestShaderRecompilerSpirvLowering(32u, offset);
        TestShaderRecompilerSpirvLowering(64u, offset);
    }
}

}

int main() {
    TestDecoderFftOffsets();
    TestDecoderRefusals();
    TestButterflyPatternsWave32();
    TestButterflyPatternsWave64();
    TestShaderRecompilerEndToEnd();
    TestDifferentialRotateVsFft();
    std::puts("ds_swizzle_fft_tests passed successfully");
    return EXIT_SUCCESS;
}
