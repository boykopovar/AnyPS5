#include <codegen/x86/MoveMaskLowering.hpp>
#include <codegen/x86/VexEncoding.hpp>

namespace Codegen {

using namespace ScalarLowering;
using namespace VexEncoding;

namespace {

constexpr std::int32_t kSaveSize = 32;
constexpr std::int32_t kMaskSlot = kSaveSize;
constexpr std::int32_t kFrame = 128 + kSaveSize + 16;
constexpr Form kMoveMask128{Map0F, Prefix66, false, false, 0xD7};
constexpr std::uint8_t kStore = 0x89;
constexpr std::uint8_t kLoad = 0x8B;
constexpr std::uint8_t kOperandSize = 0x66;

void _storeHalf(StubBodyBuilder& body, const std::uint8_t destination, const std::uint8_t source, const std::int32_t slot) {
    Register(body, kMoveMask128, destination, 0, source);
    auto store = StackBytes(kStore, false, destination, slot);
    store.insert(store.begin(), kOperandSize);
    body.Raw(store);
}

}

std::optional<MoveMaskOperands> DecodeMoveMask(const std::uint8_t* data, const std::size_t length) {
    const auto instruction = Decode(data, length);
    if (!instruction || instruction->Map != Map0F || instruction->Prefix != Prefix66 || instruction->Opcode != 0xD7) return std::nullopt;
    if (!instruction->Vector256 || instruction->Memory || instruction->Vvvv != 0) return std::nullopt;
    if (instruction->End != length) throw CodegenException("VPMOVMSKB has trailing bytes");
    if (instruction->Reg == Stack) throw CodegenException("VPMOVMSKB into RSP is not lowered");
    return MoveMaskOperands{instruction->Reg, instruction->Rm};
}

void EmitMoveMask(StubBodyBuilder& body, const MoveMaskOperands& operands) {
    const std::uint8_t high = operands.Source == 15 ? 14 : 15;
    StackSlot(body, 0x8D, true, Stack, -kFrame);
    StackOperand(body, MoveUnaligned256Store, high, 0, 0);
    _storeHalf(body, operands.Destination, operands.Source, kMaskSlot);
    ExtractUpper(body, high, operands.Source);
    _storeHalf(body, operands.Destination, high, kMaskSlot + 2);
    StackSlot(body, kLoad, false, operands.Destination, kMaskSlot);
    StackOperand(body, MoveUnaligned256Load, high, 0, 0);
    StackSlot(body, 0x8D, true, Stack, kFrame);
}

}
