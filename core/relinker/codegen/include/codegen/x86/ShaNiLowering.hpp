#ifndef CODEGEN_X86_SHANILOWERING_HPP
#define CODEGEN_X86_SHANILOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Codegen {

inline constexpr std::uint8_t kShaOpSha1Nexte = 0xC8;
inline constexpr std::uint8_t kShaOpSha1Msg1 = 0xC9;
inline constexpr std::uint8_t kShaOpSha1Msg2 = 0xCA;
inline constexpr std::uint8_t kShaOpSha256Rnds2 = 0xCB;
inline constexpr std::uint8_t kShaOpSha256Msg1 = 0xCC;
inline constexpr std::uint8_t kShaOpSha256Msg2 = 0xCD;
inline constexpr std::uint8_t kShaOpSha1Rnds4 = 0xCC;

struct ShaNiOperands {
    std::uint8_t Opcode = 0;
    std::uint8_t Destination = 0;
    std::uint8_t Source = 0;
    std::uint8_t Immediate = 0;
    bool ThreeByte3A = false;
    bool MemoryForm = false;
    bool StackRelative = false;
    bool RipRelative = false;
    std::size_t Length = 0;
    std::uint8_t RexExtension = 0;
    std::int32_t Displacement = 0;
    std::vector<std::uint8_t> Address;
};

[[nodiscard]] ShaNiOperands DecodeShaNi(const std::uint8_t* data, std::size_t length);

class ShaNiLowering {
public:
    [[nodiscard]] bool CanLower(const ShaNiOperands& operands) const;
    [[nodiscard]] LoweredBody LowerOutOfLine(std::span<const ShaNiOperands> sequence, std::span<const std::uint8_t> trailing) const;
};

}

#endif
