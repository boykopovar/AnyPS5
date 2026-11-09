#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdexcept>

using namespace ShaderRecompiler;

namespace {

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

}

int main() {
    int failures = 0;
    for (std::uint32_t sourceSelector = 0; sourceSelector < 7u; ++sourceSelector) {
        for (std::uint32_t signExtend = 0; signExtend < 2u; ++signExtend) {
            for (std::uint32_t destinationSelector = 0; destinationSelector < 7u; ++destinationSelector) {
                for (std::uint32_t unused = 0; unused < 3u; ++unused) {
                    const std::array<std::uint32_t, 2> words{MoveSdwa, Modifier(destinationSelector, unused, sourceSelector, signExtend)};
                    try {
                        if (!Matches(DecodeRdnaInstruction(0u, words, 0u), destinationSelector, unused, sourceSelector, signExtend)) {
                            std::fprintf(stderr, "dst_sel %u dst_unused %u src0_sel %u sext %u decodes with the wrong operands\n", destinationSelector, unused, sourceSelector, signExtend);
                            ++failures;
                        }
                    } catch (const std::exception& error) {
                        std::fprintf(stderr, "dst_sel %u dst_unused %u src0_sel %u sext %u: %s\n", destinationSelector, unused, sourceSelector, signExtend, error.what());
                        ++failures;
                    }
                }
            }
        }
    }
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
        try {
            static_cast<void>(DecodeRdnaInstruction(0u, words, 0u));
            std::fprintf(stderr, "%s decodes\n", entry.name);
            ++failures;
        } catch (const std::invalid_argument&) {
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s: %s\n", entry.name, error.what());
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
