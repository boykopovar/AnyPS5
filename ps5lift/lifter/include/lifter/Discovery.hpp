#ifndef PS5LIFT_DISCOVERY_HPP
#define PS5LIFT_DISCOVERY_HPP

#include <lifter/GuestImage.hpp>

#include <Zydis/Zydis.h>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Ps5Lift {

struct Instruction {
    std::uint64_t Address;
    ZydisDecodedInstruction Info;
    std::array<ZydisDecodedOperand, ZYDIS_MAX_OPERAND_COUNT> Operands;

    std::uint64_t Next() const;
};

struct InstructionSpan {
    std::uint64_t Address;
    std::uint8_t Length;

    std::uint64_t Next() const;
};

struct FunctionFlow {
    std::uint64_t Entry = 0;
    std::set<std::uint64_t> Leaders;
    std::map<std::uint64_t, std::vector<std::uint64_t>> JumpTables;
    std::set<std::uint64_t> UnguardedJumpTables;
    std::set<std::uint64_t> ClosedJumpTables;
    std::set<std::uint64_t> ResumePoints;
};

using RegisterSet = std::uint32_t;
constexpr RegisterSet EveryRegister = ~RegisterSet{0};
constexpr unsigned FirstVectorBit = 16;

struct RegisterEffects {
    RegisterSet Named = 0;
    RegisterSet Written = 0;

    bool operator==(const RegisterEffects&) const = default;
};

struct GuestFunction : FunctionFlow {
    std::string Name;
    std::optional<std::string> Import;
    std::vector<InstructionSpan> Code;
    std::vector<std::uint64_t> Exits;
    RegisterEffects Effects;
};

struct DecodedFunction : FunctionFlow {
    std::map<std::uint64_t, Instruction> Instructions;
};

class Decoder {
public:
    Decoder();
    Instruction Decode(const GuestImage& image, std::uint64_t address) const;
    std::optional<Instruction> TryDecode(std::span<const std::uint8_t> code, std::uint64_t address) const;

private:
    ZydisDecoder _decoder;
};

bool IsConditionalJump(ZydisMnemonic mnemonic);
RegisterEffects InstructionEffects(const Instruction& instruction);
std::optional<std::uint64_t> DirectTarget(const Instruction& instruction);
std::optional<std::uint64_t> ImportTarget(const GuestImage& image, const Instruction& instruction);
std::string BridgeSymbol(const std::string& import);
std::string Nid(std::string_view name);
std::optional<std::string> JumpBufferFunction(const std::string& import);

std::map<std::uint64_t, GuestFunction> DiscoverFunctions(const GuestImage& image);
DecodedFunction Decode(const GuestImage& image, const GuestFunction& function);

}

#endif
