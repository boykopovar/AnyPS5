#include <codegen/x86/WidenBroadcastLowering.hpp>
#include <codegen/x86/VexEncoding.hpp>
#include <array>
#include <string>

namespace Codegen {

using namespace ScalarLowering;
using namespace VexEncoding;

namespace {

constexpr std::int32_t kFrame = 128 + 3 * 32;
constexpr Form kLoad128{Map0F, PrefixF3, false, false, 0x6F};
constexpr Form kLoad64{Map0F, PrefixF3, false, false, 0x7E};
constexpr Form kLoad32{Map0F, Prefix66, false, false, 0x6E};
constexpr Form kShiftBytesRight{Map0F, Prefix66, false, false, 0x73};
constexpr Form kShuffleSingles{Map0F, 0, false, false, 0xC6};
constexpr Form kDuplicateLow{Map0F, PrefixF2, false, false, 0x12};
constexpr Form kExclusiveOr{Map0F, Prefix66, false, false, 0xEF};
constexpr Form kInsertByte{Map0F3A, Prefix66, false, false, 0x20};
constexpr Form kInsertWord{Map0F, Prefix66, false, false, 0xC4};
constexpr Form kShuffleBytes{Map0F38, Prefix66, false, false, 0x00};
constexpr Form kShuffleLowWords{Map0F, PrefixF2, false, false, 0x70};
constexpr Form kUnpackLowQwords{Map0F, Prefix66, false, false, 0x6C};
constexpr std::uint8_t kShiftRightBytesExtension = 3;

constexpr std::array<std::uint8_t, 6> kWidenSourceBytes{16, 8, 4, 16, 8, 16};
constexpr std::array<const char*, 6> kSignNames{"VPMOVSXBW", "VPMOVSXBD", "VPMOVSXBQ", "VPMOVSXWD", "VPMOVSXWQ", "VPMOVSXDQ"};
constexpr std::array<const char*, 6> kZeroNames{"VPMOVZXBW", "VPMOVZXBD", "VPMOVZXBQ", "VPMOVZXWD", "VPMOVZXWQ", "VPMOVZXDQ"};

void _finish(StubBodyBuilder& body, const WidenBroadcastOperands& operands, const std::uint8_t low) {
    if (operands.Vector256) InsertUpper(body, operands.Destination, low, low);
}

void _widen(StubBodyBuilder& body, const WidenBroadcastOperands& operands, const std::uint8_t source, const std::uint8_t high) {
    const Form widen{Map0F38, Prefix66, false, false, operands.Opcode};
    Register(body, kShiftBytesRight, kShiftRightBytesExtension, high, source, static_cast<std::uint8_t>(operands.SourceBytes / 2));
    Register(body, widen, high, 0, high);
    Register(body, widen, operands.Destination, 0, source);
    InsertUpper(body, operands.Destination, operands.Destination, high);
}

}

std::optional<WidenBroadcastOperands> DecodeWidenBroadcast(const std::uint8_t* data, const std::size_t length) {
    const auto instruction = Decode(data, length);
    if (!instruction || instruction->Map != Map0F38 || instruction->Prefix != Prefix66 || instruction->Wide) return std::nullopt;
    const auto opcode = instruction->Opcode;
    const bool memory = instruction->Memory.has_value();
    WidenBroadcastOperands operands{nullptr, WidenBroadcastKind::Widen, opcode, instruction->Vector256, instruction->Reg, instruction->Rm, 0, instruction->Memory};
    if ((opcode >= 0x20 && opcode <= 0x25) || (opcode >= 0x30 && opcode <= 0x35)) {
        if (!instruction->Vector256) return std::nullopt;
        operands.Name = (opcode & 0x10) != 0 ? kZeroNames[opcode & 7] : kSignNames[opcode & 7];
        operands.SourceBytes = kWidenSourceBytes[opcode & 7];
    } else if ((opcode == 0x18 || opcode == 0x58) && !memory) {
        operands.Name = opcode == 0x18 ? "VBROADCASTSS" : "VPBROADCASTD";
        operands.Kind = WidenBroadcastKind::BroadcastDword;
    } else if (((opcode == 0x19 && instruction->Vector256) || opcode == 0x59) && !memory) {
        operands.Name = opcode == 0x19 ? "VBROADCASTSD" : "VPBROADCASTQ";
        operands.Kind = WidenBroadcastKind::BroadcastQword;
    } else if (opcode == 0x78) {
        operands.Name = "VPBROADCASTB";
        operands.Kind = WidenBroadcastKind::BroadcastByte;
    } else if (opcode == 0x79) {
        operands.Name = "VPBROADCASTW";
        operands.Kind = WidenBroadcastKind::BroadcastWord;
    } else {
        return std::nullopt;
    }
    if (instruction->End != length) throw CodegenException(std::string(operands.Name) + " has trailing bytes");
    return operands;
}

void EmitWidenBroadcast(StubBodyBuilder& body, const WidenBroadcastOperands& operands) {
    std::array<bool, 16> used{};
    used[operands.Destination] = true;
    if (!operands.Memory) used[operands.Source] = true;
    std::array<std::uint8_t, 3> scratch{};
    std::size_t taken = 0;
    for (std::uint8_t reg = 15; taken < scratch.size(); --reg)
        if (!used[reg]) scratch[taken++] = reg;
    const auto loaded = scratch[0];
    const auto work = scratch[1];
    const auto zero = scratch[2];
    StackSlot(body, 0x8D, true, Stack, -kFrame);
    for (std::size_t index = 0; index < scratch.size(); ++index)
        StackOperand(body, MoveUnaligned256Store, scratch[index], 0, static_cast<std::int32_t>(index * 32));
    auto source = operands.Source;
    if (operands.Memory) {
        source = loaded;
        switch (operands.Kind) {
        case WidenBroadcastKind::Widen:
            Memory(body, operands.SourceBytes == 16 ? kLoad128 : operands.SourceBytes == 8 ? kLoad64 : kLoad32, loaded, 0, *operands.Memory, kFrame, operands.Name);
            break;
        case WidenBroadcastKind::BroadcastByte:
            Register(body, kExclusiveOr, loaded, loaded, loaded);
            Memory(body, kInsertByte, loaded, loaded, *operands.Memory, kFrame, operands.Name, std::uint8_t{0});
            break;
        case WidenBroadcastKind::BroadcastWord:
            Register(body, kExclusiveOr, loaded, loaded, loaded);
            Memory(body, kInsertWord, loaded, loaded, *operands.Memory, kFrame, operands.Name, std::uint8_t{0});
            break;
        default:
            break;
        }
    }
    const auto low = operands.Vector256 ? work : operands.Destination;
    switch (operands.Kind) {
    case WidenBroadcastKind::Widen:
        _widen(body, operands, source, work);
        break;
    case WidenBroadcastKind::BroadcastDword:
        Register(body, kShuffleSingles, low, source, source, std::uint8_t{0});
        _finish(body, operands, low);
        break;
    case WidenBroadcastKind::BroadcastQword:
        Register(body, kDuplicateLow, low, 0, source);
        _finish(body, operands, low);
        break;
    case WidenBroadcastKind::BroadcastByte:
        Register(body, kExclusiveOr, zero, zero, zero);
        Register(body, kShuffleBytes, low, source, zero);
        _finish(body, operands, low);
        break;
    case WidenBroadcastKind::BroadcastWord:
        Register(body, kShuffleLowWords, zero, 0, source, std::uint8_t{0});
        Register(body, kUnpackLowQwords, low, zero, zero);
        _finish(body, operands, low);
        break;
    }
    for (std::size_t index = 0; index < scratch.size(); ++index)
        StackOperand(body, MoveUnaligned256Load, scratch[index], 0, static_cast<std::int32_t>(index * 32));
    StackSlot(body, 0x8D, true, Stack, kFrame);
}

}
