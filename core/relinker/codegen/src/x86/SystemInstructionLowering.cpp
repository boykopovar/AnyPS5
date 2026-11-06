#include <codegen/x86/SystemInstructionLowering.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <array>

namespace Codegen {

namespace {

using namespace Amd64OnlySubstitutionTable;

constexpr std::uint8_t kPause[] = {0xF3, 0x90};
constexpr std::uint8_t kStc[] = {0xF9};
constexpr std::uint8_t kPushFlags[] = {0x9C};
constexpr std::uint8_t kPopFlags[] = {0x9D};
constexpr std::uint8_t kPushRax[] = {0x50};
constexpr std::uint8_t kPopRax[] = {0x58};
constexpr std::uint8_t kAlignRax[] = {0x48, 0x83, 0xE0, 0xC0};
constexpr std::uint8_t kStoreZero[] = {0x48, 0xC7, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00};
constexpr std::size_t kStoreDisplacementOffset = 3;
constexpr std::size_t kCacheLineBytes = 64;
constexpr std::size_t kStoreBytes = 8;

void _append(std::vector<std::uint8_t>& out, const std::span<const std::uint8_t> bytes) {
    out.insert(out.end(), bytes.begin(), bytes.end());
}

void _nopFill(std::vector<std::uint8_t>& out, std::size_t count) {
    while (count > 0) {
        const auto& nop = kNops[std::min<std::size_t>(count, std::size(kNops)) - 1];
        _append(out, {nop.Bytes, nop.Size});
        count -= nop.Size;
    }
}

void _emitClzero(std::vector<std::uint8_t>& out, bool addressSize32) {
    _append(out, {kLeaRspBelowRedZone.Bytes, kLeaRspBelowRedZone.Size});
    _append(out, kPushFlags);
    _append(out, kPushRax);
    // A 67h prefix uses EAX; clear its high half after saving the original RAX.
    if (addressSize32) _append(out, std::array<std::uint8_t, 2>{0x89, 0xC0});
    _append(out, kAlignRax);
    for (std::size_t offset = 0; offset < kCacheLineBytes; offset += kStoreBytes) {
        std::array<std::uint8_t, sizeof(kStoreZero)> store{};
        std::copy(std::begin(kStoreZero), std::end(kStoreZero), store.begin());
        store[kStoreDisplacementOffset] = static_cast<std::uint8_t>(offset);
        _append(out, store);
    }
    _append(out, kPopRax);
    _append(out, kPopFlags);
    _append(out, {kLeaRspRestore.Bytes, kLeaRspRestore.Size});
}

void _emitRelocated(std::vector<std::uint8_t>& out, const SystemInstruction instruction) {
    switch (instruction) {
    case SystemInstruction::Monitorx:
        return;
    case SystemInstruction::Mwaitx:
        _append(out, kPause);
        return;
    case SystemInstruction::Clzero:
        _emitClzero(out, false);
        return;
    case SystemInstruction::Clzero32:
        _emitClzero(out, true);
        return;
    case SystemInstruction::Mcommit:
        _append(out, kStc);
        return;
    }
    throw CodegenException("Unknown AMD-only system instruction");
}

}

std::vector<std::uint8_t> SystemInstructionLowering::LowerInPlace(const SystemInstruction instruction, const std::size_t originalLength) const {
    if ((instruction == SystemInstruction::Clzero || instruction == SystemInstruction::Clzero32))
        throw CodegenException("CLZERO has no in-place Intel lowering");
    std::vector<std::uint8_t> sequence;
    _emitRelocated(sequence, instruction);
    if (sequence.size() > originalLength)
        throw CodegenException("Intel replacement is longer than the AMD-only instruction");
    _nopFill(sequence, originalLength - sequence.size());
    return sequence;
}

LoweredBody SystemInstructionLowering::LowerOutOfLine(const std::span<const SystemInstruction> sequence, const std::span<const std::uint8_t> trailing) const {
    std::vector<std::uint8_t> bytes;
    for (const auto instruction : sequence)
        _emitRelocated(bytes, instruction);
    const auto trailingOffset = bytes.size();
    _append(bytes, trailing);
    const auto returnBranchOffset = bytes.size();
    _append(bytes, {kJmpRel32.Bytes, kJmpRel32.Size});
    return {std::move(bytes), returnBranchOffset, trailingOffset};
}

}
