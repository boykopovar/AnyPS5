#include <codegen/x86/GatherLowering.hpp>
#include <codegen/x86/VexEncoding.hpp>
#include <array>
#include <limits>
#include <string>

namespace Codegen {

using namespace ScalarLowering;
using namespace VexEncoding;

namespace {

constexpr std::size_t kScratchCount = 6;
constexpr std::int32_t kAddressSlot = static_cast<std::int32_t>(kScratchCount) * 32;
constexpr std::int32_t kFrame = 128 + kAddressSlot + 16;
constexpr Form kExtractDword{Map0F3A, Prefix66, false, false, 0x16};
constexpr Form kExtractQword{Map0F3A, Prefix66, true, false, 0x16};
constexpr Form kInsertSingle{Map0F3A, Prefix66, false, false, 0x21};
constexpr Form kMoveQuad{Map0F, PrefixF3, false, false, 0x7E};
constexpr Form kShiftBytesRight{Map0F, Prefix66, false, false, 0x73};
constexpr Form kBlendDoubles{Map0F3A, Prefix66, false, false, 0x0D};
constexpr Form kMoveLowHigh{Map0F, 0, false, false, 0x16};
constexpr Form kExclusiveOrSingles{Map0F, 0, false, false, 0x57};
constexpr Form kMaskLoadSingles{Map0F38, Prefix66, false, false, 0x2C};
constexpr Form kMaskLoadDoubles{Map0F38, Prefix66, false, false, 0x2D};
constexpr std::array<std::uint8_t, 12> kAddressCandidates{0, 1, 2, 3, 6, 7, 8, 9, 10, 11, 14, 15};
constexpr std::array<const char*, 8> kNames{"VPGATHERDD", "VPGATHERQD", "VGATHERDPS", "VGATHERQPS", "VPGATHERDQ", "VPGATHERQQ", "VGATHERDPD", "VGATHERQPD"};

struct Registers {
    std::uint8_t Address;
    std::uint8_t IndexHigh;
    std::uint8_t MaskHigh;
    std::uint8_t Single;
    std::uint8_t Loaded;
    std::uint8_t GatheredLow;
    std::uint8_t GatheredHigh;
};

void _address(StubBodyBuilder& body, const GatherOperands& operands, const std::uint8_t address) {
    std::int64_t displacement = operands.Memory.Displacement;
    if (operands.Base && *operands.Base == Stack) displacement += kFrame;
    if (displacement > std::numeric_limits<std::int32_t>::max())
        throw CodegenException(std::string(operands.Name) + " stack displacement does not fit after spilling");
    const std::uint8_t base = operands.Base ? *operands.Base : 5;
    const std::uint8_t scale = operands.Scale == 8 ? 3 : operands.Scale == 4 ? 2 : operands.Scale == 2 ? 1 : 0;
    std::vector<std::uint8_t> bytes{static_cast<std::uint8_t>(0x48 | ((address & 8) >> 1) | ((address & 8) >> 2) | ((base & 8) >> 3)), 0x8D,
        static_cast<std::uint8_t>((operands.Base ? 0x80 : 0x00) | ((address & 7) << 3) | 4),
        static_cast<std::uint8_t>((scale << 6) | ((address & 7) << 3) | (base & 7))};
    Append32(bytes, static_cast<std::int32_t>(displacement));
    body.Raw(bytes);
}

void _maskLoad(StubBodyBuilder& body, const GatherOperands& operands, const std::uint8_t destination, const std::uint8_t mask, const std::uint8_t address) {
    std::vector<std::uint8_t> bytes = operands.Memory.Prefixes;
    const auto header = Header(operands.DataBytes == 8 ? kMaskLoadDoubles : kMaskLoadSingles, destination, mask, false, (address & 8) != 0);
    bytes.insert(bytes.end(), header.begin(), header.end());
    bytes.push_back(static_cast<std::uint8_t>(((destination & 7) << 3) | (address & 7)));
    body.Raw(bytes);
}

void _element(StubBodyBuilder& body, const GatherOperands& operands, const Registers& work, const unsigned element) {
    const unsigned dataPerHalf = 16u / operands.DataBytes;
    const unsigned indexPerHalf = 16u / operands.IndexBytes;
    const auto index = element / indexPerHalf == 0 ? operands.Index : work.IndexHigh;
    const auto indexPosition = static_cast<std::uint8_t>(element % indexPerHalf);
    const auto mask = element / dataPerHalf == 0 ? operands.Mask : work.MaskHigh;
    const auto gathered = element / dataPerHalf == 0 ? work.GatheredLow : work.GatheredHigh;
    const auto dataPosition = static_cast<std::uint8_t>(element % dataPerHalf);
    if (operands.IndexBytes == 4) {
        Register(body, kExtractDword, index, 0, work.Address, indexPosition);
        ScalarLowering::Register(body, 0x63, true, work.Address, work.Address);
    } else {
        Register(body, kExtractQword, index, 0, work.Address, indexPosition);
    }
    _address(body, operands, work.Address);
    if (operands.DataBytes == 4)
        Register(body, kInsertSingle, work.Single, work.Single, mask, static_cast<std::uint8_t>((dataPosition << 6) | 0x0E));
    else if (dataPosition == 0)
        Register(body, kMoveQuad, work.Single, 0, mask);
    else
        Register(body, kShiftBytesRight, 3, work.Single, mask, std::uint8_t{8});
    _maskLoad(body, operands, work.Loaded, work.Single, work.Address);
    if (operands.DataBytes == 4)
        Register(body, kInsertSingle, gathered, gathered, work.Loaded, static_cast<std::uint8_t>(dataPosition << 4));
    else if (dataPosition == 0)
        Register(body, kBlendDoubles, gathered, gathered, work.Loaded, std::uint8_t{1});
    else
        Register(body, kMoveLowHigh, gathered, gathered, work.Loaded);
}

}

std::optional<GatherOperands> DecodeGather(const std::uint8_t* data, const std::size_t length) {
    const auto instruction = Decode(data, length);
    if (!instruction || instruction->Map != Map0F38 || instruction->Prefix != Prefix66 || instruction->Opcode < 0x90 || instruction->Opcode > 0x93) return std::nullopt;
    if (!instruction->Memory || instruction->Memory->Rm != 4) return std::nullopt;
    const auto& memory = *instruction->Memory;
    const auto name = kNames[(instruction->Wide ? 4 : 0) + (instruction->Opcode - 0x90)];
    const auto index = static_cast<std::uint8_t>(((memory.Sib >> 3) & 7) | ((memory.RexIndexBase & 2) << 2));
    if (instruction->Reg == instruction->Vvvv || instruction->Reg == index || instruction->Vvvv == index) return std::nullopt;
    if (instruction->End != length) throw CodegenException(std::string(name) + " has trailing bytes");
    for (const auto prefix : memory.Prefixes)
        if (prefix == 0x67) throw CodegenException(std::string(name) + " with 32-bit addressing is not lowered");
    const bool noBase = memory.Mod == 0 && (memory.Sib & 7) == 5;
    std::optional<std::uint8_t> base;
    if (!noBase) base = static_cast<std::uint8_t>((memory.Sib & 7) | ((memory.RexIndexBase & 1) << 3));
    return GatherOperands{name, static_cast<std::uint8_t>(instruction->Wide ? 8 : 4), static_cast<std::uint8_t>((instruction->Opcode & 1) != 0 ? 8 : 4), instruction->Vector256,
        instruction->Reg, instruction->Vvvv, index, base, static_cast<std::uint8_t>(1u << (memory.Sib >> 6)), memory};
}

void EmitGather(StubBodyBuilder& body, const GatherOperands& operands) {
    std::array<bool, 16> used{};
    used[operands.Destination] = true;
    used[operands.Mask] = true;
    used[operands.Index] = true;
    std::array<std::uint8_t, kScratchCount> scratch{};
    std::size_t taken = 0;
    for (std::uint8_t reg = 15; taken < scratch.size(); --reg)
        if (!used[reg]) scratch[taken++] = reg;
    std::uint8_t address = 0;
    for (const auto candidate : kAddressCandidates)
        if (!operands.Base || candidate != *operands.Base) {
            address = candidate;
            break;
        }
    const Registers work{address, scratch[0], scratch[1], scratch[2], scratch[3], scratch[4], scratch[5]};
    const unsigned width = operands.Vector256 ? 32u : 16u;
    const unsigned elements = width / (operands.DataBytes > operands.IndexBytes ? operands.DataBytes : operands.IndexBytes);
    const bool wideData = elements * operands.DataBytes == 32;
    const bool wideIndex = elements * operands.IndexBytes == 32;
    StackSlot(body, 0x8D, true, Stack, -kFrame);
    for (std::size_t index = 0; index < scratch.size(); ++index)
        StackOperand(body, MoveUnaligned256Store, scratch[index], 0, static_cast<std::int32_t>(index * 32));
    StackSlot(body, 0x89, true, address, kAddressSlot);
    if (wideIndex) ExtractUpper(body, work.IndexHigh, operands.Index);
    if (wideData) ExtractUpper(body, work.MaskHigh, operands.Mask);
    Register(body, kExclusiveOrSingles, work.Single, work.Single, work.Single);
    Register(body, kExclusiveOrSingles, work.GatheredLow, work.GatheredLow, work.GatheredLow);
    Register(body, kExclusiveOrSingles, work.GatheredHigh, work.GatheredHigh, work.GatheredHigh);
    for (unsigned element = 0; element < elements; ++element) _element(body, operands, work, element);
    if (wideData) InsertUpper(body, work.GatheredLow, work.GatheredLow, work.GatheredHigh);
    const Form blend{Map0F3A, Prefix66, false, wideData, static_cast<std::uint8_t>(operands.DataBytes == 8 ? 0x4B : 0x4A)};
    Register(body, blend, operands.Destination, operands.Destination, work.GatheredLow, static_cast<std::uint8_t>(operands.Mask << 4));
    if (elements * operands.DataBytes == 8) Register(body, kMoveQuad, operands.Destination, 0, operands.Destination);
    Register(body, kExclusiveOrSingles, operands.Mask, operands.Mask, operands.Mask);
    StackSlot(body, 0x8B, true, address, kAddressSlot);
    for (std::size_t index = 0; index < scratch.size(); ++index)
        StackOperand(body, MoveUnaligned256Load, scratch[index], 0, static_cast<std::int32_t>(index * 32));
    StackSlot(body, 0x8D, true, Stack, kFrame);
}

}
