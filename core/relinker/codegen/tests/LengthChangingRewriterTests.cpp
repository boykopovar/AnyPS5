#include <codegen/AddressMap.hpp>
#include <codegen/CodegenException.hpp>
#include <codegen/x86/LengthChangingFixups.hpp>
#include <codegen/x86/X64InstructionRewriter.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

int passed = 0;

void Require(const bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
    ++passed;
}

template<typename TOperation>
void RequireFailure(const TOperation& operation, const char* message) {
    try {
        operation();
    } catch (const Codegen::CodegenException&) {
        ++passed;
        return;
    }
    throw std::runtime_error(message);
}

template<typename TOperation>
void RequireFailure(const TOperation& operation, const char* expected, const char* message) {
    try {
        operation();
    } catch (const Codegen::CodegenException& error) {
        if (std::string(error.what()).find(expected) == std::string::npos)
            throw std::runtime_error(message);
        ++passed;
        return;
    }
    throw std::runtime_error(message);
}

template<typename TValue>
void Write(Bytes& bytes, const std::size_t at, const TValue value) {
    if (at > bytes.size() || sizeof(value) > bytes.size() - at)
        throw std::runtime_error("Fixture write is out of bounds");
    std::memcpy(bytes.data() + at, &value, sizeof(value));
}

template<typename TValue>
TValue Read(const Bytes& bytes, const std::size_t at) {
    if (at > bytes.size() || sizeof(TValue) > bytes.size() - at)
        throw std::runtime_error("Fixture read is out of bounds");
    TValue value;
    std::memcpy(&value, bytes.data() + at, sizeof(value));
    return value;
}

void WriteBranch32(Bytes& bytes, const std::size_t field, const std::int64_t target) {
    Write<std::int32_t>(bytes, field, static_cast<std::int32_t>(target - static_cast<std::int64_t>(field) - 4));
}

void WriteBranch8(Bytes& bytes, const std::size_t field, const std::int64_t target) {
    Write<std::int8_t>(bytes, field, static_cast<std::int8_t>(target - static_cast<std::int64_t>(field) - 1));
}

const Bytes kGrowReplacement = {0xC7, 0x00, 0x11, 0x22, 0x33, 0x44};
const Bytes kShrinkReplacement = {0x66, 0x90};
constexpr std::size_t kGrowCut = 0x0E;
constexpr std::size_t kShrinkCut = 0x0E;

Bytes GrowCode() {
    Bytes code(0x18, 0xCC);
    code[0x00] = 0xEB;
    WriteBranch8(code, 0x01, 0x02);
    code[0x02] = 0xE9;
    WriteBranch32(code, 0x03, 0x10);
    code[0x07] = 0x48;
    code[0x08] = 0x8B;
    code[0x09] = 0x05;
    WriteBranch32(code, 0x0A, 0x12);
    code[0x0E] = 0x66;
    code[0x0F] = 0x90;
    code[0x10] = 0xEB;
    WriteBranch8(code, 0x11, 0x00);
    code[0x12] = 0xE9;
    WriteBranch32(code, 0x13, 0x10);
    code[0x17] = 0xC3;
    return code;
}

Bytes ShrinkCode() {
    Bytes code(0x1C, 0xCC);
    code[0x00] = 0xEB;
    WriteBranch8(code, 0x01, 0x02);
    code[0x02] = 0xE9;
    WriteBranch32(code, 0x03, 0x14);
    code[0x07] = 0x48;
    code[0x08] = 0x8B;
    code[0x09] = 0x05;
    WriteBranch32(code, 0x0A, 0x16);
    code[0x0E] = 0xC7;
    code[0x0F] = 0x00;
    Write<std::uint32_t>(code, 0x10, 0x44332211);
    code[0x14] = 0xEB;
    WriteBranch8(code, 0x15, 0x00);
    code[0x16] = 0xE9;
    WriteBranch32(code, 0x17, 0x14);
    code[0x1B] = 0xC3;
    return code;
}

void CheckBranch(
    const Bytes& rewritten,
    const Codegen::AddressMap& map,
    const std::size_t oldSite,
    const std::size_t length,
    const std::size_t fieldOffset,
    const bool wide,
    const std::int64_t oldTarget,
    const char* message
) {
    const auto newSite = static_cast<std::size_t>(map.MapAddress(static_cast<std::int64_t>(oldSite)));
    const auto newField = newSite + fieldOffset;
    const std::int64_t displacement = wide
        ? static_cast<std::int64_t>(Read<std::int32_t>(rewritten, newField))
        : static_cast<std::int64_t>(Read<std::int8_t>(rewritten, newField));
    const auto newTarget = static_cast<std::int64_t>(newSite) + static_cast<std::int64_t>(length) + displacement;
    Require(newTarget == map.MapAddress(oldTarget), message);
}

void TestAddressMap() {
    using Codegen::AddressMap;
    using Codegen::AddressRange;
    using Codegen::LayoutShift;

    const AddressMap identity;
    Require(identity.IsIdentity(), "Default address map is not an identity");
    Require(identity.MapAddress(0x40) == 0x40, "Identity map moved an address");
    Require(identity.MapRange(0x10, 0x20) == AddressRange{0x10, 0x20}, "Identity map resized a range");

    const AddressMap single(std::vector<LayoutShift>{LayoutShift{0x10, 0x14, 4}});
    Require(!single.IsIdentity(), "Single shift map reported an identity");
    Require(single.TotalDelta() == 4, "Single shift map reported a wrong total delta");
    Require(single.MapAddress(0x0F) == 0x0F, "Address before the cut moved");
    Require(single.MapAddress(0x10) == 0x10, "Address at the cut begin moved");
    Require(single.MapAddress(0x14) == 0x18, "Address at the cut end did not move");
    Require(single.MapAddress(0x40) == 0x44, "Address after the cut did not move");
    Require(single.MapRange(0x10, 4) == AddressRange{0x10, 8}, "Range at the cut did not grow");
    Require(single.MapRange(0x08, 0x10) == AddressRange{0x08, 0x14}, "Range spanning the cut did not grow");
    RequireFailure([&] { (void)single.MapAddress(0x12); }, "inside a rewritten instruction", "Address inside the cut was mapped");
    RequireFailure([&] { (void)single.MapRange(0x08, -1); }, "Negative range size was mapped");

    const AddressMap multiple(std::vector<LayoutShift>{LayoutShift{0x20, 0x24, 4}, LayoutShift{0x10, 0x12, 2}});
    Require(multiple.Shifts().front().Begin == 0x10, "Layout shifts were not sorted");
    Require(multiple.TotalDelta() == 6, "Multiple shift map reported a wrong total delta");
    Require(multiple.MapAddress(0x18) == 0x1A, "Address between shifts did not move");
    Require(multiple.MapAddress(0x30) == 0x36, "Address after both shifts did not move");

    RequireFailure([&] { (void)AddressMap(std::vector<LayoutShift>{LayoutShift{0x10, 0x14, 4}, LayoutShift{0x12, 0x16, 4}}); }, "Layout shifts overlap", "Overlapping layout shifts were accepted");
    RequireFailure([&] { (void)AddressMap(std::vector<LayoutShift>{LayoutShift{-1, 0x4, 4}}); }, "Negative layout shift was accepted");
    RequireFailure([&] { (void)AddressMap(std::vector<LayoutShift>{LayoutShift{0x10, 0x10, 4}}); }, "Empty layout shift was accepted");
    RequireFailure([&] { (void)AddressMap(std::vector<LayoutShift>{LayoutShift{0x10, 0x14, 0}}); }, "Zero-delta layout shift was accepted");
}

void TestGrowRewrite() {
    const auto original = GrowCode();
    const auto result = Codegen::X64InstructionRewriter{}.Rewrite(original, {kGrowCut, kGrowReplacement});

    Require(result.Bytes.size() == original.size() + 4, "Growing rewrite did not grow the section by four bytes");
    Require(result.Mapping.Shifts().size() == 1, "Growing rewrite did not report one layout shift");
    Require(result.Mapping.Shifts().front() == Codegen::LayoutShift{kGrowCut, kGrowCut + 2, 4}, "Growing rewrite reported a wrong layout shift");
    Require(std::equal(kGrowReplacement.begin(), kGrowReplacement.end(), result.Bytes.begin() + static_cast<std::ptrdiff_t>(kGrowCut)), "Growing rewrite wrote other replacement bytes");
    Require(result.Bytes[result.Bytes.size() - 1] == 0xC3, "Growing rewrite lost the last instruction");

    CheckBranch(result.Bytes, result.Mapping, 0x00, 2, 1, false, 0x02, "Rel8 branch before the cut to before the cut moved");
    CheckBranch(result.Bytes, result.Mapping, 0x02, 5, 1, true, 0x10, "Rel32 branch before the cut to after the cut did not follow");
    CheckBranch(result.Bytes, result.Mapping, 0x07, 7, 3, true, 0x12, "RIP-relative operand before the cut to after the cut did not follow");
    CheckBranch(result.Bytes, result.Mapping, 0x10, 2, 1, false, 0x00, "Rel8 branch after the cut to before the cut did not follow");
    CheckBranch(result.Bytes, result.Mapping, 0x12, 5, 1, true, 0x10, "Rel32 branch after the cut to before the cut did not follow");
    Require(result.Bytes[static_cast<std::size_t>(result.Mapping.MapAddress(0x17))] == 0xC3, "Growing rewrite moved the return instruction wrongly");
}

void TestShrinkRewrite() {
    const auto original = ShrinkCode();
    const auto result = Codegen::X64InstructionRewriter{}.Rewrite(original, {kShrinkCut, kShrinkReplacement});

    Require(result.Bytes.size() == original.size() - 4, "Shrinking rewrite did not shrink the section by four bytes");
    Require(result.Mapping.Shifts().front() == Codegen::LayoutShift{kShrinkCut, kShrinkCut + 6, -4}, "Shrinking rewrite reported a wrong layout shift");
    Require(std::equal(kShrinkReplacement.begin(), kShrinkReplacement.end(), result.Bytes.begin() + static_cast<std::ptrdiff_t>(kShrinkCut)), "Shrinking rewrite wrote other replacement bytes");

    CheckBranch(result.Bytes, result.Mapping, 0x00, 2, 1, false, 0x02, "Rel8 branch before the cut to before the cut moved on a shrink");
    CheckBranch(result.Bytes, result.Mapping, 0x02, 5, 1, true, 0x14, "Rel32 branch before the cut to after the cut did not follow on a shrink");
    CheckBranch(result.Bytes, result.Mapping, 0x07, 7, 3, true, 0x16, "RIP-relative operand before the cut to after the cut did not follow on a shrink");
    CheckBranch(result.Bytes, result.Mapping, 0x14, 2, 1, false, 0x00, "Rel8 branch after the cut to before the cut did not follow on a shrink");
    CheckBranch(result.Bytes, result.Mapping, 0x16, 5, 1, true, 0x14, "Rel32 branch after the cut to the cut end did not follow on a shrink");
}

void TestRelocations(const std::size_t originalSize, const Codegen::AddressMap& map) {
    using Codegen::CodeExtent;
    using Codegen::CodeRelocation;
    using Codegen::RelocationAddendKind;

    const CodeExtent code{0x1000, originalSize};
    std::vector<CodeRelocation> relocations{
        {0x1002, 0x1014, RelocationAddendKind::CodeAddress},
        {0x1014, 0x1018, RelocationAddendKind::SymbolValue},
        {0x5000, 0x1000, RelocationAddendKind::CodeAddress},
        {0x1000, -4, RelocationAddendKind::SymbolValue},
    };
    Codegen::FixupCodeRelocations(relocations, code, map);
    Require(relocations[0].Offset == 0x1002, "Relocation slot before the cut moved");
    Require(relocations[0].Addend == 0x1018, "Relative relocation addend into shifted code did not move");
    Require(relocations[1].Offset == 0x1018, "Relocation slot after the cut did not move");
    Require(relocations[1].Addend == 0x1018, "Symbol-relative relocation addend was treated as an address");
    Require(relocations[2].Offset == 0x5000, "Relocation slot outside the code moved");
    Require(relocations[2].Addend == 0x1000, "Relocation addend before the cut moved");
    Require(relocations[3].Offset == 0x1000, "Relocation slot at the code start moved");
    Require(relocations[3].Addend == -4, "Negative symbol-relative addend moved");

    std::vector<CodeRelocation> interiorAddend{{0x1000, 0x100F, RelocationAddendKind::CodeAddress}};
    RequireFailure([&] { Codegen::FixupCodeRelocations(interiorAddend, code, map); }, "inside a rewritten instruction", "Relocation addend inside the cut was mapped");

    std::vector<CodeRelocation> interiorSlot{{0x100F, 0, RelocationAddendKind::SymbolValue}};
    RequireFailure([&] { Codegen::FixupCodeRelocations(interiorSlot, code, map); }, "inside a rewritten instruction", "Relocation slot inside the cut was mapped");

    std::vector<Codegen::CodeSymbol> symbols{{0x1012}, {0x5000}, {0x1000}};
    Codegen::FixupCodeSymbols(symbols, code, map);
    Require(symbols[0].Value == 0x1016, "Symbol after the cut did not move");
    Require(symbols[1].Value == 0x5000, "Symbol outside the code moved");
    Require(symbols[2].Value == 0x1000, "Symbol at the code start moved");

    std::vector<Codegen::CodeSymbol> interiorSymbol{{0x100F}};
    RequireFailure([&] { Codegen::FixupCodeSymbols(interiorSymbol, code, map); }, "inside a rewritten instruction", "Symbol inside the cut was mapped");
}

void TestRelativeJumpTables() {
    using Codegen::CodeExtent;
    using Codegen::RelativeJumpTableRef;

    Bytes tableCode(20, 0xCC);
    tableCode[0] = 0xEB;
    WriteBranch8(tableCode, 1, 2);
    tableCode[2] = 0x90;
    tableCode[3] = 0x66;
    tableCode[4] = 0x90;
    tableCode[5] = 0xC3;
    const auto result = Codegen::X64InstructionRewriter{}.Rewrite(tableCode, {3, kGrowReplacement});
    Require(result.Bytes.size() == tableCode.size() + 4, "Jump table fixture did not grow by four bytes");
    const CodeExtent code{0x2000, tableCode.size()};

    std::vector<std::int32_t> inside{-8, -3};
    RelativeJumpTableRef insideTable{inside, 0x2008};
    Codegen::FixupRelativeJumpTable(insideTable, code, result.Mapping);
    Require(inside[0] == -12, "Jump table entry to code before the cut did not follow the table");
    Require(inside[1] == -3, "Jump table entry to code after the cut moved twice");

    std::vector<std::int32_t> outside{0x2000 - 0x3000, 0x2005 - 0x3000};
    RelativeJumpTableRef outsideTable{outside, 0x3000};
    Codegen::FixupRelativeJumpTable(outsideTable, code, result.Mapping);
    Require(outside[0] == 0x2000 - 0x3000, "Jump table entry to code before the cut moved with a fixed table");
    Require(outside[1] == 0x2009 - 0x3000, "Jump table entry to code after the cut did not move with a fixed table");

    std::vector<std::int32_t> interior{0x2004 - 0x2008};
    RelativeJumpTableRef interiorTable{interior, 0x2008};
    RequireFailure([&] { Codegen::FixupRelativeJumpTable(interiorTable, code, result.Mapping); }, "inside a rewritten instruction", "Jump table entry inside the cut was mapped");

    std::vector<std::int32_t> tableInsideCut{-1};
    RelativeJumpTableRef tableInsideCutRef{tableInsideCut, 0x2004};
    RequireFailure([&] { Codegen::FixupRelativeJumpTable(tableInsideCutRef, code, result.Mapping); }, "inside a rewritten instruction", "Jump table base inside the cut was mapped");
}

constexpr std::uint8_t kEncodeAbsPtr = 0x00;
constexpr std::uint8_t kEncodeUleb = 0x01;
constexpr std::uint8_t kEncodeSleb = 0x09;
constexpr std::uint8_t kEncodeUdata4 = 0x03;
constexpr std::uint8_t kEncodeSdata4 = 0x0B;
constexpr std::uint8_t kEncodePcRelSdata4 = 0x1B;
constexpr std::uint8_t kEncodePcRelIndirectSdata4 = 0x9B;
constexpr std::uint8_t kEncodeFunctionRelSdata4 = 0x4B;
constexpr std::uint8_t kEncodeDataRelAbsPtr = 0x30;
constexpr std::uint8_t kEncodeOmit = 0xFF;

void AppendByte(Bytes& frame, const std::uint8_t value) {
    frame.push_back(value);
}

void AppendU32(Bytes& frame, const std::uint32_t value) {
    const auto at = frame.size();
    frame.resize(at + sizeof(value));
    Write<std::uint32_t>(frame, at, value);
}

void AppendUleb(Bytes& frame, std::uint64_t value) {
    do {
        auto byte = static_cast<std::uint8_t>(value & 0x7F);
        value >>= 7;
        if (value != 0)
            byte |= 0x80;
        AppendByte(frame, byte);
    } while (value != 0);
}

void AppendSleb(Bytes& frame, std::int64_t value) {
    bool more = true;
    while (more) {
        auto byte = static_cast<std::uint8_t>(value & 0x7F);
        value >>= 7;
        if ((value == 0 && (byte & 0x40) == 0) || (value == -1 && (byte & 0x40) != 0))
            more = false;
        else
            byte |= 0x80;
        AppendByte(frame, byte);
    }
}

std::size_t EncodingWidth(const std::uint8_t encoding) {
    switch (encoding & 0x0F) {
    case kEncodeAbsPtr:
        return 8;
    case kEncodeUdata4:
    case kEncodeSdata4:
        return 4;
    default:
        return 0;
    }
}

void AppendFormat(Bytes& frame, const std::uint8_t encoding, const std::int64_t value) {
    switch (encoding & 0x0F) {
    case kEncodeAbsPtr:
        AppendU32(frame, static_cast<std::uint32_t>(value));
        AppendU32(frame, static_cast<std::uint32_t>(static_cast<std::uint64_t>(value) >> 32));
        return;
    case kEncodeUdata4:
        AppendU32(frame, static_cast<std::uint32_t>(value));
        return;
    case kEncodeSdata4:
        AppendU32(frame, static_cast<std::uint32_t>(static_cast<std::int32_t>(value)));
        return;
    case kEncodeUleb:
        AppendUleb(frame, static_cast<std::uint64_t>(value));
        return;
    case kEncodeSleb:
        AppendSleb(frame, value);
        return;
    default:
        throw std::runtime_error("Fixture does not encode this pointer format");
    }
}

void AppendPointer(Bytes& frame, const std::uint64_t fieldAddress, const std::uint8_t encoding, const std::uint64_t address) {
    const auto application = static_cast<std::uint8_t>(encoding & 0x70);
    const auto value = application == 0x10
        ? static_cast<std::int64_t>(address - fieldAddress)
        : static_cast<std::int64_t>(address);
    AppendFormat(frame, encoding, value);
}

void PatchLength(Bytes& frame, const std::size_t start) {
    Write<std::uint32_t>(frame, start, static_cast<std::uint32_t>(frame.size() - start - 4));
}

std::size_t AppendCie(Bytes& frame, const std::uint8_t pointerEncoding, const std::uint8_t lsdaEncoding) {
    const auto start = frame.size();
    AppendU32(frame, 0);
    AppendU32(frame, 0);
    AppendByte(frame, 1);
    const bool encoded = pointerEncoding != kEncodeOmit;
    const bool lsda = encoded && lsdaEncoding != kEncodeOmit;
    if (encoded) {
        AppendByte(frame, 'z');
        AppendByte(frame, 'R');
        if (lsda)
            AppendByte(frame, 'L');
    }
    AppendByte(frame, 0);
    AppendByte(frame, 1);
    AppendByte(frame, 0x78);
    AppendByte(frame, 16);
    if (encoded) {
        AppendByte(frame, static_cast<std::uint8_t>(1 + (lsda ? 1 : 0)));
        AppendByte(frame, pointerEncoding);
        if (lsda)
            AppendByte(frame, lsdaEncoding);
    }
    PatchLength(frame, start);
    return start;
}

struct FrameFields {
    std::size_t Start = 0;
    std::size_t BeginField = 0;
    std::size_t RangeField = 0;
    std::size_t LsdaField = 0;
    bool HasLsda = false;
};

struct LsdaPointer {
    std::uint8_t Encoding;
    std::uint64_t Address;
};

FrameFields AppendFde(
    Bytes& frame,
    const std::size_t cieStart,
    const std::uint8_t pointerEncoding,
    const std::uint64_t frameAddress,
    const std::uint64_t begin,
    const std::uint64_t range,
    const std::optional<LsdaPointer>& lsda
) {
    FrameFields fields;
    fields.Start = frame.size();
    AppendU32(frame, 0);
    const auto cieField = frame.size();
    AppendU32(frame, static_cast<std::uint32_t>(cieField - cieStart));
    fields.BeginField = frame.size();
    AppendPointer(frame, frameAddress + frame.size(), pointerEncoding, begin);
    fields.RangeField = frame.size();
    AppendFormat(frame, static_cast<std::uint8_t>(pointerEncoding & 0x0F), static_cast<std::int64_t>(range));
    if (pointerEncoding != kEncodeOmit) {
        if (lsda) {
            AppendUleb(frame, EncodingWidth(lsda->Encoding));
            fields.LsdaField = frame.size();
            fields.HasLsda = true;
            AppendPointer(frame, frameAddress + frame.size(), lsda->Encoding, lsda->Address);
        } else {
            AppendUleb(frame, 0);
        }
    }
    PatchLength(frame, fields.Start);
    return fields;
}

std::int64_t ReadEncodedAddress(const Bytes& frame, const std::size_t field, const std::uint64_t frameAddress, const std::uint8_t encoding) {
    std::int64_t value = 0;
    switch (encoding & 0x0F) {
    case kEncodeAbsPtr:
        value = static_cast<std::int64_t>(Read<std::uint64_t>(frame, field));
        break;
    case kEncodeUdata4:
        value = static_cast<std::int64_t>(Read<std::uint32_t>(frame, field));
        break;
    case kEncodeSdata4:
        value = static_cast<std::int64_t>(Read<std::int32_t>(frame, field));
        break;
    default:
        throw std::runtime_error("Fixture does not read this pointer format");
    }
    if ((encoding & 0x70) == 0x10)
        value += static_cast<std::int64_t>(frameAddress + field);
    return value;
}

void TestEhFrameFixups(const std::size_t originalSize, const Codegen::AddressMap& map) {
    constexpr std::uint64_t kFrameAddress = 0x8000;
    const Codegen::CodeExtent code{0x1000, originalSize};
    Bytes frame;
    const auto cie = AppendCie(frame, kEncodePcRelSdata4, kEncodeOmit);
    const auto before = AppendFde(frame, cie, kEncodePcRelSdata4, kFrameAddress, 0x1000, 0x08, std::nullopt);
    const auto spanning = AppendFde(frame, cie, kEncodePcRelSdata4, kFrameAddress, 0x1008, 0x08, std::nullopt);
    const auto after = AppendFde(frame, cie, kEncodePcRelSdata4, kFrameAddress, 0x1010, 0x07, std::nullopt);
    const auto ending = AppendFde(frame, cie, kEncodePcRelSdata4, kFrameAddress, 0x1000, 0x0E, std::nullopt);
    const auto frameSize = frame.size();

    Codegen::FixupEhFrame(frame, kFrameAddress, code, map);

    Require(frame.size() == frameSize, ".eh_frame fixup changed a record size");
    Require(ReadEncodedAddress(frame, before.BeginField, kFrameAddress, kEncodePcRelSdata4) == 0x1000, "FDE before the cut moved its pc_begin");
    Require(Read<std::uint32_t>(frame, before.RangeField) == 0x08, "FDE before the cut changed its pc_range");
    Require(ReadEncodedAddress(frame, spanning.BeginField, kFrameAddress, kEncodePcRelSdata4) == 0x1008, "FDE spanning the cut moved its pc_begin");
    Require(Read<std::uint32_t>(frame, spanning.RangeField) == 0x0C, "FDE spanning the cut did not grow its pc_range");
    Require(ReadEncodedAddress(frame, after.BeginField, kFrameAddress, kEncodePcRelSdata4) == 0x1014, "FDE after the cut did not move its pc_begin");
    Require(Read<std::uint32_t>(frame, after.RangeField) == 0x07, "FDE after the cut changed its pc_range");
    Require(ReadEncodedAddress(frame, ending.BeginField, kFrameAddress, kEncodePcRelSdata4) == 0x1000, "FDE ending at the cut moved its pc_begin");
    Require(Read<std::uint32_t>(frame, ending.RangeField) == 0x0E, "FDE ending at the cut changed its pc_range");

    Bytes lsdaFrame;
    const auto lsdaCie = AppendCie(lsdaFrame, kEncodePcRelSdata4, kEncodePcRelSdata4);
    const auto lsdaFde = AppendFde(lsdaFrame, lsdaCie, kEncodePcRelSdata4, kFrameAddress, 0x1010, 0x04, LsdaPointer{kEncodePcRelSdata4, 0x1014});
    const auto outsideFde = AppendFde(lsdaFrame, lsdaCie, kEncodePcRelSdata4, kFrameAddress, 0x1000, 0x04, LsdaPointer{kEncodePcRelSdata4, 0x9000});
    Codegen::FixupEhFrame(lsdaFrame, kFrameAddress, code, map);
    Require(ReadEncodedAddress(lsdaFrame, lsdaFde.BeginField, kFrameAddress, kEncodePcRelSdata4) == 0x1014, "FDE with an LSDA did not move its pc_begin");
    Require(ReadEncodedAddress(lsdaFrame, lsdaFde.LsdaField, kFrameAddress, kEncodePcRelSdata4) == 0x1018, "LSDA pointer into shifted code did not move");
    Require(ReadEncodedAddress(lsdaFrame, outsideFde.LsdaField, kFrameAddress, kEncodePcRelSdata4) == 0x9000, "LSDA pointer outside the code moved");

    Bytes slebFrame;
    const auto slebCie = AppendCie(slebFrame, kEncodeSleb, kEncodeOmit);
    (void)AppendFde(slebFrame, slebCie, kEncodeSleb, kFrameAddress, 0x1010, 0x04, std::nullopt);
    RequireFailure([&] { Codegen::FixupEhFrame(slebFrame, kFrameAddress, code, map); }, "variable-length", "Variable-length FDE start was re-encoded");

    Bytes indirectFrame;
    const auto indirectCie = AppendCie(indirectFrame, kEncodePcRelIndirectSdata4, kEncodeOmit);
    (void)AppendFde(indirectFrame, indirectCie, kEncodePcRelIndirectSdata4, kFrameAddress, 0x1010, 0x04, std::nullopt);
    RequireFailure([&] { Codegen::FixupEhFrame(indirectFrame, kFrameAddress, code, map); }, "Indirect", "Indirect FDE start was adjusted");

    Bytes dataRelFrame;
    const auto dataRelCie = AppendCie(dataRelFrame, kEncodeDataRelAbsPtr, kEncodeOmit);
    (void)AppendFde(dataRelFrame, dataRelCie, kEncodeAbsPtr, kFrameAddress, 0x1010, 0x04, std::nullopt);
    RequireFailure([&] { Codegen::FixupEhFrame(dataRelFrame, kFrameAddress, code, map); }, "Unsupported .eh_frame pointer base", "Data-relative FDE start was adjusted");

    Bytes overrunFrame;
    const auto overrunCie = AppendCie(overrunFrame, kEncodePcRelSdata4, kEncodeOmit);
    (void)AppendFde(overrunFrame, overrunCie, kEncodePcRelSdata4, kFrameAddress, 0x1010, 0x10, std::nullopt);
    RequireFailure([&] { Codegen::FixupEhFrame(overrunFrame, kFrameAddress, code, map); }, "extends past the rewritten code", "FDE past the rewritten code was adjusted");

    Bytes defaultFrame;
    const auto defaultCie = AppendCie(defaultFrame, kEncodeOmit, kEncodeOmit);
    const auto defaultFde = AppendFde(defaultFrame, defaultCie, kEncodeAbsPtr, kFrameAddress, 0x1010, 0x04, std::nullopt);
    Codegen::FixupEhFrame(defaultFrame, kFrameAddress, code, map);
    Require(ReadEncodedAddress(defaultFrame, defaultFde.BeginField, kFrameAddress, kEncodeAbsPtr) == 0x1014, "FDE without an R augmentation did not default to absolute pointers");
}

void TestUnwindEdgeCases(const std::size_t originalSize, const Codegen::AddressMap& map) {
    constexpr std::uint64_t kFrameAddress = 0x8000;
    const Codegen::CodeExtent code{0x1000, originalSize};

    Bytes stableFrame;
    const auto stableCie = AppendCie(stableFrame, kEncodePcRelSdata4, kEncodeOmit);
    const auto stableFde = AppendFde(stableFrame, stableCie, kEncodePcRelSdata4, kFrameAddress, 0x1000, 0x08, std::nullopt);
    AppendByte(stableFrame, 0x44);
    AppendByte(stableFrame, 0x44);
    PatchLength(stableFrame, stableFde.Start);
    Codegen::FixupEhFrame(stableFrame, kFrameAddress, code, map);
    Require(Read<std::uint32_t>(stableFrame, stableFde.RangeField) == 0x08, "CFI rows before the cut changed their pc_range");

    Bytes crossingFrame;
    const auto crossingCie = AppendCie(crossingFrame, kEncodePcRelSdata4, kEncodeOmit);
    const auto crossingFde = AppendFde(crossingFrame, crossingCie, kEncodePcRelSdata4, kFrameAddress, 0x1008, 0x08, std::nullopt);
    AppendByte(crossingFrame, 0x48);
    PatchLength(crossingFrame, crossingFde.Start);
    RequireFailure([&] { Codegen::FixupEhFrame(crossingFrame, kFrameAddress, code, map); }, "CFI location crosses", "CFI row spanning the cut was accepted");

    Bytes indirectLsdaFrame;
    const auto indirectLsdaCie = AppendCie(indirectLsdaFrame, kEncodePcRelSdata4, kEncodePcRelIndirectSdata4);
    (void)AppendFde(indirectLsdaFrame, indirectLsdaCie, kEncodePcRelSdata4, kFrameAddress, 0x1010, 0x04, LsdaPointer{kEncodePcRelIndirectSdata4, 0x9000});
    RequireFailure([&] { Codegen::FixupEhFrame(indirectLsdaFrame, kFrameAddress, code, map); }, "Indirect", "Indirect LSDA outside the code was skipped");

    Bytes functionRelFrame;
    const auto functionRelCie = AppendCie(functionRelFrame, kEncodePcRelSdata4, kEncodeFunctionRelSdata4);
    const auto functionRelFde = AppendFde(functionRelFrame, functionRelCie, kEncodePcRelSdata4, kFrameAddress, 0x1010, 0x04,
        LsdaPointer{kEncodeFunctionRelSdata4, static_cast<std::uint64_t>(static_cast<std::int64_t>(0x1004 - 0x1010))});
    Codegen::FixupEhFrame(functionRelFrame, kFrameAddress, code, map);
    Require(Read<std::int32_t>(functionRelFrame, functionRelFde.LsdaField) == 0x1004 - 0x1014, "Function-relative LSDA kept the old function base");

    Bytes outsideFunctionRelFrame;
    const auto outsideFunctionRelCie = AppendCie(outsideFunctionRelFrame, kEncodePcRelSdata4, kEncodeFunctionRelSdata4);
    const auto outsideFunctionRelFde = AppendFde(outsideFunctionRelFrame, outsideFunctionRelCie, kEncodePcRelSdata4, kFrameAddress, 0x1010, 0x04,
        LsdaPointer{kEncodeFunctionRelSdata4, static_cast<std::uint64_t>(static_cast<std::int64_t>(0x9000 - 0x1010))});
    Codegen::FixupEhFrame(outsideFunctionRelFrame, kFrameAddress, code, map);
    Require(Read<std::int32_t>(outsideFunctionRelFrame, outsideFunctionRelFde.LsdaField) == 0x9000 - 0x1014, "Function-relative LSDA outside the code kept the old function base");
}

void TestIdentityPass(const Bytes& growCode, const Codegen::AddressMap& growMap, const Bytes& frame, const std::uint64_t frameAddress) {
    using Codegen::CodeExtent;
    using Codegen::CodeRelocation;
    using Codegen::CodeSymbol;
    using Codegen::LengthChangingFixups;
    using Codegen::RelativeJumpTableRef;
    using Codegen::RelocationAddendKind;

    const auto unchanged = Codegen::X64InstructionRewriter{}.Rewrite(growCode, {kGrowCut, {0x66, 0x90}});
    Require(unchanged.Bytes == growCode, "Same-length rewrite changed the section bytes");
    Require(unchanged.Mapping.IsIdentity(), "Same-length rewrite reported a layout shift");

    const auto replaced = Codegen::X64InstructionRewriter{}.Rewrite(growCode, {kGrowCut, {0x90, 0x90}});
    Require(replaced.Bytes.size() == growCode.size(), "Same-length replacement changed the section size");
    Require(replaced.Mapping.IsIdentity(), "Same-length replacement reported a layout shift");

    std::vector<CodeRelocation> relocations{{0x1014, 0x1014, RelocationAddendKind::CodeAddress}};
    std::vector<CodeSymbol> symbols{{0x1014}};
    std::vector<std::int32_t> entries{-8};
    std::vector<RelativeJumpTableRef> tables{{entries, 0x2008}};
    Bytes untouchedFrame = frame;
    LengthChangingFixups fixups{
        CodeExtent{0x1000, growCode.size()},
        std::span<CodeRelocation>(relocations),
        std::span<CodeSymbol>(symbols),
        std::span<const RelativeJumpTableRef>(tables),
        std::span<std::uint8_t>(untouchedFrame),
        frameAddress
    };
    Codegen::ApplyLengthChangingFixups(fixups, Codegen::AddressMap{});

    Require(relocations[0].Offset == 0x1014 && relocations[0].Addend == 0x1014, "Identity pass moved a relocation");
    Require(symbols[0].Value == 0x1014, "Identity pass moved a symbol");
    Require(entries[0] == -8, "Identity pass moved a jump table entry");
    Require(untouchedFrame == frame, "Identity pass changed .eh_frame");
    Require(growMap.TotalDelta() == 4, "Growing rewrite map is not usable by later fixups");
}

void TestAggregateFixups(const Bytes& growCode, const std::size_t grownSize, const Codegen::AddressMap& growMap) {
    using Codegen::CodeExtent;
    using Codegen::CodeRelocation;
    using Codegen::CodeSymbol;
    using Codegen::LengthChangingFixups;
    using Codegen::RelativeJumpTableRef;
    using Codegen::RelocationAddendKind;

    constexpr std::uint64_t kFrameAddress = 0x8000;
    Bytes frame;
    const auto cie = AppendCie(frame, kEncodePcRelSdata4, kEncodeOmit);
    const auto fde = AppendFde(frame, cie, kEncodePcRelSdata4, kFrameAddress, 0x1010, 0x04, std::nullopt);

    std::vector<CodeRelocation> relocations{{0x1010, 0x1012, RelocationAddendKind::CodeAddress}};
    std::vector<CodeSymbol> symbols{{0x1010}};
    std::vector<std::int32_t> entries{0x1012 - 0x2008};
    std::vector<RelativeJumpTableRef> tables{{entries, 0x2008}};
    LengthChangingFixups fixups{
        CodeExtent{0x1000, growCode.size()},
        std::span<CodeRelocation>(relocations),
        std::span<CodeSymbol>(symbols),
        std::span<const RelativeJumpTableRef>(tables),
        std::span<std::uint8_t>(frame),
        kFrameAddress
    };

    Codegen::ApplyLengthChangingFixups(fixups, growMap);

    Require(relocations[0].Offset == 0x1014, "Aggregate pass did not move the relocation slot");
    Require(relocations[0].Addend == 0x1016, "Aggregate pass did not move the relocation addend");
    Require(symbols[0].Value == 0x1014, "Aggregate pass did not move the symbol");
    Require(entries[0] == 0x1016 - 0x2008, "Aggregate pass did not move the jump table entry");
    Require(ReadEncodedAddress(frame, fde.BeginField, kFrameAddress, kEncodePcRelSdata4) == 0x1014, "Aggregate pass did not move the FDE start");
    Require(growCode.size() + 4 == grownSize, "Aggregate fixture size mismatch");
}

void TestTruncatedBranchRejection() {
    const Bytes truncated = {0x66, 0xEB, 0x01, 0x90, 0xC3};
    RequireFailure(
        [&] { (void)Codegen::X64InstructionRewriter{}.Rewrite(truncated, {3, {0x66, 0x90}}); },
        "truncated relative branch moves",
        "Length-changing rewrite accepted a surviving truncated relative branch that moves");

    const auto kept = Codegen::X64InstructionRewriter{}.Rewrite(Bytes{0x66, 0xEB, 0x00, 0x90, 0x90, 0xC3}, {4, {0x66, 0x90}});
    Require(kept.Bytes == Bytes({0x66, 0xEB, 0x00, 0x90, 0x66, 0x90, 0xC3}), "Truncated relative branch that keeps its distance was not accepted");
}

}

int main() {
    try {
        TestAddressMap();
        TestGrowRewrite();
        TestShrinkRewrite();

        const auto growCode = GrowCode();
        const auto grown = Codegen::X64InstructionRewriter{}.Rewrite(growCode, {kGrowCut, kGrowReplacement});
        const auto grownSize = grown.Bytes.size();
        TestRelocations(growCode.size(), grown.Mapping);
        TestEhFrameFixups(growCode.size(), grown.Mapping);
        TestUnwindEdgeCases(growCode.size(), grown.Mapping);
        TestRelativeJumpTables();

        constexpr std::uint64_t kFrameAddress = 0x8000;
        Bytes frame;
        const auto cie = AppendCie(frame, kEncodePcRelSdata4, kEncodeOmit);
        (void)AppendFde(frame, cie, kEncodePcRelSdata4, kFrameAddress, 0x1000, 0x08, std::nullopt);
        TestIdentityPass(growCode, grown.Mapping, frame, kFrameAddress);
        TestAggregateFixups(growCode, grownSize, grown.Mapping);
        TestTruncatedBranchRejection();

        std::cout << passed << " length-changing rewriter checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}