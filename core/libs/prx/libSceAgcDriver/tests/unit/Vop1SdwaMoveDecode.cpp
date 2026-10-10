#include <Testing/Test.hpp>
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"

#include <array>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>

namespace {

using namespace ShaderRecompiler;
using Testing::Case;
using Testing::Require;

constexpr std::uint32_t MoveSdwa = 0x7e0602f9u;
constexpr std::uint32_t SourceVector = 4;
constexpr std::uint32_t DestinationVector = 3;

constexpr std::uint32_t Modifier(std::uint32_t destinationSelector, std::uint32_t unused, std::uint32_t sourceSelector, std::uint32_t signExtend) {
    return SourceVector | (destinationSelector << 8u) | (unused << 11u) | (sourceSelector << 16u) | (signExtend << 19u);
}

bool Matches(const RdnaInstruction& instruction, std::uint32_t destinationSelector, std::uint32_t unused, std::uint32_t sourceSelector, std::uint32_t signExtend) {
    return instruction.op == RdnaOpcode::VMovB32 && instruction.wordCount == 2u && instruction.sourceCount == 1u &&
        instruction.destination.kind == RdnaOperandKind::VectorRegister && instruction.destination.reg == DestinationVector &&
        instruction.destination.explicitSdwaDst && instruction.destination.sdwaSel == destinationSelector &&
        instruction.destination.sdwaDstUnused == unused &&
        instruction.source0.kind == RdnaOperandKind::VectorRegister && instruction.source0.reg == SourceVector &&
        instruction.source0.sdwaSel == sourceSelector && instruction.source0.sdwaSext == (signExtend != 0u) &&
        !instruction.source0.negate && !instruction.source0.absolute;
}

const Case validSelectors{"DecodeRdnaInstruction_SdwaMoveWithValidSelectors_DecodesEveryOperand", [] {
    for (std::uint32_t sourceSelector = 0; sourceSelector < 7u; ++sourceSelector) {
        for (std::uint32_t signExtend = 0; signExtend < 2u; ++signExtend) {
            for (std::uint32_t destinationSelector = 0; destinationSelector < 7u; ++destinationSelector) {
                for (std::uint32_t unused = 0; unused < 3u; ++unused) {
                    const std::array<std::uint32_t, 2> words{MoveSdwa, Modifier(destinationSelector, unused, sourceSelector, signExtend)};
                    const auto name = "dst_sel " + std::to_string(destinationSelector) + " dst_unused " + std::to_string(unused) +
                        " src0_sel " + std::to_string(sourceSelector) + " sext " + std::to_string(signExtend);
                    RdnaInstruction instruction{};
                    try {
                        instruction = DecodeRdnaInstruction(0u, words, 0u);
                    } catch (const std::exception& error) {
                        Testing::Fail(name + ": " + error.what());
                    }
                    Require(Matches(instruction, destinationSelector, unused, sourceSelector, signExtend), name + " decodes with the wrong operands");
                }
            }
        }
    }
}};

const Case reservedEncodings{"DecodeRdnaInstruction_SdwaMoveWithReservedOrInvalidModifier_ThrowsInvalidArgument", [] {
    struct Rejected {
        const char* name;
        std::uint32_t modifier;
    };
    constexpr std::array<Rejected, 6> rejected{{
        {"reserved dst_unused on a byte destination", Modifier(1u, 3u, 0u, 0u)},
        {"reserved dst_unused on a dword destination", Modifier(6u, 3u, 6u, 0u)},
        {"reserved dst_sel", Modifier(7u, 0u, 0u, 0u)},
        {"reserved src0_sel", Modifier(0u, 2u, 7u, 0u)},
        {"neg on a byte source", Modifier(1u, 2u, 0u, 0u) | (1u << 20u)},
        {"abs on a word source", Modifier(5u, 2u, 4u, 0u) | (1u << 21u)},
    }};
    for (const auto& entry : rejected) {
        const std::array<std::uint32_t, 2> words{MoveSdwa, entry.modifier};
        Testing::RequireThrows<std::invalid_argument>([&] { static_cast<void>(DecodeRdnaInstruction(0u, words, 0u)); }, std::string(entry.name) + " decodes");
    }
}};

} // namespace
