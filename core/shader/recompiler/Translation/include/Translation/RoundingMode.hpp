#ifndef CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_ROUNDINGMODE_HPP
#define CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_ROUNDINGMODE_HPP

#include "ControlFlow/ControlFlowGraph.hpp"
#include "RdnaDecoder/RdnaOpcode.hpp"
#include "RdnaDecoder/RdnaProgram.hpp"
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace ShaderRecompiler {

inline constexpr std::uint32_t ModeRoundBits = 0xfu;
inline constexpr std::uint32_t ModeRound32Bits = 0x3u;
inline constexpr std::uint32_t ModeRound1664Bits = 0xcu;

struct RoundingState {
    std::uint8_t value = 0;
    std::uint8_t unknown = 0;

    bool operator==(const RoundingState&) const = default;
};

struct RoundingWrite {
    std::uint32_t mask = 0;
    std::uint32_t value = 0;
    bool known = false;
};

enum class RoundingUseKind {
    Directed32,
    Directed16,
    SelfChecked,
    Unmodelled
};

struct RoundingUse {
    std::uint32_t fields = 0;
    RoundingUseKind kind = RoundingUseKind::Unmodelled;
};

[[nodiscard]] std::optional<RoundingWrite> DecodeRoundingWrite(const RdnaInstruction& instruction);
[[nodiscard]] RoundingState ApplyRoundingWrite(RoundingState state, const RoundingWrite& write);
[[nodiscard]] RoundingState JoinRoundingStates(RoundingState lhs, RoundingState rhs);
[[nodiscard]] RoundingUse RoundingUseOf(RdnaOpcode opcode);
[[nodiscard]] std::unordered_map<std::uint32_t, RoundingState> ComputeEntryRounding(const ControlFlowGraph& cfg, const RdnaProgram& program, std::uint32_t initial);

}

#endif
