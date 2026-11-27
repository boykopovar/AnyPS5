#include <codegen/x86/WideStoreLowering.hpp>
#include <codegen/x86/ScalarLowering.hpp>
#include <codegen/x86/VexEncoding.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <codegen/CodegenException.hpp>
#include <array>

namespace Codegen {

namespace {

using namespace X64OpcodeConstants;

constexpr std::int32_t kFrameSize = 128 + 3 * 8;
constexpr std::int32_t kSpillBytes = 3 * 8;
constexpr std::int32_t kStoreBytes = 32;
constexpr std::uint8_t kAddressRegister = 0;
constexpr std::int32_t kUpperHalfBytes = 16;

void _emitHalf(StubBodyBuilder& body, const WideStoreHalf& half) {
    if (half.RipDisplacementOffset)
        body.RipRelative(half.Bytes, *half.RipDisplacementOffset, half.RipTarget);
    else
        body.Raw(half.Bytes);
}

bool _overlapsSpill(const MemoryOperand& memory) {
    if (!ScalarLowering::UnindexedStack(memory))
        return false;
    const auto displacement = static_cast<std::int64_t>(memory.Displacement);
    return displacement < kSpillBytes - kFrameSize && displacement + kStoreBytes > -kFrameSize;
}

MemoryOperand _storeAddress(const MemoryOperand& memory, const std::uint8_t mod, const std::int32_t displacement) {
    MemoryOperand address = memory;
    address.Prefixes.clear();
    for (const auto prefix : memory.Prefixes)
        if (prefix != PrefixAddressSize)
            address.Prefixes.push_back(prefix);
    address.RexIndexBase = 0;
    address.Mod = mod;
    address.Rm = kAddressRegister;
    address.Sib = 0;
    address.Displacement = displacement;
    address.StackBase = false;
    address.RipRelative = false;
    return address;
}

void _emitMaterialized(StubBodyBuilder& body, const WideStoreMaterialized& materialized) {
    if (_overlapsSpill(materialized.Memory))
        throw CodegenException("256-bit store operand overlaps its spill frame");
    body.Raw(std::array<std::uint8_t, 8>{0x48, 0x8D, 0x64, 0x24, 0x80, 0x9C, 0x50, 0x51});
    ScalarLowering::Memory(body, 0x8D, true, materialized.Memory, kAddressRegister, "256-bit store", kFrameSize);
    VexEncoding::Memory(body, VexEncoding::ExtractHigh, materialized.Source, 0, _storeAddress(materialized.Memory, ModRmModIndirect, 0), 0, "256-bit store", std::uint8_t{0});
    VexEncoding::Memory(body, VexEncoding::ExtractHigh, materialized.Source, 0, _storeAddress(materialized.Memory, ModRmModDisp8, kUpperHalfBytes), 0, "256-bit store", std::uint8_t{1});
    body.Raw(std::array<std::uint8_t, 11>{0x59, 0x58, 0x9D, 0x48, 0x8D, 0xA4, 0x24, 0x80, 0x00, 0x00, 0x00});
}

}

void WideStoreLowering::EmitOutOfLine(StubBodyBuilder& body, const WideStoreOperands& operands) const {
    if (operands.Materialized) {
        _emitMaterialized(body, *operands.Materialized);
        return;
    }
    _emitHalf(body, operands.Low);
    _emitHalf(body, operands.High);
}

}
