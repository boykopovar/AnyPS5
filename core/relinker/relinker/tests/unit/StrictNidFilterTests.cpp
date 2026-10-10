#include <Testing/Test.hpp>
#include <relinker/analysis/UnusedNidFilter.hpp>
#include <relinker/analysis/UnusedNidFilter/StrictReachability.hpp>
#include <relinker/output/SysVDynamicSectionBuilder.hpp>
#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <relinker/analysis/UnusedNidFilter/PltCompactor.hpp>
#include <relinker/analysis/UnusedNidFilter/IRelativeRelocationIndex.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <algorithm>
#include <cstring>
#include <string>

namespace {

using Relinker::UnusedNidFilter::AnalyzeStrictReachability;
using Relinker::UnusedNidFilter::StrictReachabilityInput;

template<typename TValue>
void write(std::vector<std::uint8_t>& bytes, std::size_t offset, TValue value) {
    Testing::Require(offset <= bytes.size() && sizeof(value) <= bytes.size() - offset, "Test fixture write is out of bounds");
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

StrictReachabilityInput fixture() {
    StrictReachabilityInput input;
    input.Text.assign(128, 0xCC);
    input.TextVaddr = 0x1000;
    input.Entries = {0x1000};
    input.ImportSlots = {0x2000, 0x2008};
    return input;
}

void emit(StrictReachabilityInput& input, std::size_t offset, std::initializer_list<std::uint8_t> bytes) {
    for (const auto byte : bytes) input.Text.at(offset++) = byte;
}

void ripOperand(StrictReachabilityInput& input, std::size_t offset, std::initializer_list<std::uint8_t> opcode, std::uint64_t target) {
    emit(input, offset, opcode);
    const auto next = input.TextVaddr + offset + opcode.size() + 4;
    write(input.Text, offset + opcode.size(), static_cast<std::int32_t>(target - next));
}

void importThunk(StrictReachabilityInput& input, std::size_t offset, std::uint64_t slot) {
    ripOperand(input, offset, {0xFF, 0x25}, slot);
}

StrictReachabilityInput ud2DeadTailInput() {
    auto input = fixture();
    emit(input, 0, {0x0F, 0x0B});
    importThunk(input, 16, 0x2000);
    return input;
}

StrictReachabilityInput unknownIndirectInput() {
    auto input = fixture();
    emit(input, 0, {0xFF, 0xE0});
    importThunk(input, 32, 0x2000);
    importThunk(input, 64, 0x2008);
    input.Pointers.emplace(0x3000, 0x1020);
    return input;
}

StrictReachabilityInput functionCycleInput() {
    auto input = fixture();
    emit(input, 0, {0xC3});
    ripOperand(input, 32, {0xE8}, 0x1030);
    emit(input, 37, {0xC3});
    ripOperand(input, 48, {0xE8}, 0x1020);
    importThunk(input, 53, 0x2000);
    input.Functions = {{0x1000, 0x1001, {}}, {0x1020, 0x1026, {}}, {0x1030, 0x103B, {}}};
    return input;
}

std::vector<Relinker::ProgramHeader> exceptionHeaders(std::size_t size) {
    return {{1, 4, 0, 0, 0, size, size, 8}, {0x6474E550, 4, 0x300, 0x300, 0, 32, 32, 4}};
}

void writeEhFrameHeader(std::vector<std::uint8_t>& bytes, std::uint64_t fdeOffset) {
    write<std::uint8_t>(bytes, 0x300, 1);
    write<std::uint8_t>(bytes, 0x302, 3);
    write<std::uint64_t>(bytes, 0x304, 0x200);
    write<std::uint32_t>(bytes, 0x30C, 1);
    write<std::uint64_t>(bytes, 0x310, 0x1000);
    write<std::uint64_t>(bytes, 0x318, fdeOffset);
}

void requireCieLeb128(const std::vector<std::uint8_t>& codeAlignment, const std::vector<std::uint8_t>& dataAlignment, bool overflowing,
                      const std::string& description) {
    std::vector<std::uint8_t> bytes(0x400);
    std::vector<std::uint8_t> cie{1, 0};
    cie.insert(cie.end(), codeAlignment.begin(), codeAlignment.end());
    cie.insert(cie.end(), dataAlignment.begin(), dataAlignment.end());
    cie.push_back(16);
    write<std::uint32_t>(bytes, 0x200, static_cast<std::uint32_t>(4 + cie.size()));
    std::copy(cie.begin(), cie.end(), bytes.begin() + 0x208);
    write<std::uint32_t>(bytes, 0x240, 20);
    write<std::uint32_t>(bytes, 0x244, 0x44);
    write<std::uint64_t>(bytes, 0x248, 0x1000);
    write<std::uint64_t>(bytes, 0x250, 0x20);
    writeEhFrameHeader(bytes, 0x240);
    const auto headers = exceptionHeaders(bytes.size());
    if (overflowing) {
        const auto error = Testing::RequireThrows<Relinker::RelinkerException>(
            [&] { Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {}); },
            "Overflowing CIE LEB128 value was accepted: " + description);
        Testing::Require(std::string(error.what()).find("overflowing LEB128 value") != std::string::npos,
                         "CIE LEB128 was rejected for an unexpected reason: " + description + ": " + error.what());
        return;
    }
    const auto functions = Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
    Testing::RequireEqual(functions.size(), std::size_t{1}, "Valid CIE LEB128 changed the function count: " + description);
    Testing::RequireEqual(functions[0].Begin, std::uint64_t{0x1000}, "Valid CIE LEB128 changed the function begin: " + description);
    Testing::RequireEqual(functions[0].End, std::uint64_t{0x1020}, "Valid CIE LEB128 changed the function end: " + description);
    Testing::Require(functions[0].ExtraTargets.empty(), "Valid CIE LEB128 added extra targets: " + description);
}

std::vector<std::uint8_t> overlongLeb128(std::uint8_t prefix, unsigned terminal) {
    std::vector<std::uint8_t> encoded(10, prefix);
    encoded.back() = static_cast<std::uint8_t>(terminal);
    return encoded;
}

std::string leb128Description(std::uint8_t prefix, unsigned terminal) {
    return "prefix=" + std::to_string(prefix) + " terminal=" + std::to_string(terminal);
}

std::vector<std::uint8_t> landingPadEhFrame() {
    std::vector<std::uint8_t> bytes(0x400);
    write<std::uint32_t>(bytes, 0x200, 25);
    write<std::uint8_t>(bytes, 0x208, 1);
    const std::vector<std::uint8_t> augmentation = {'z', 'P', 'L', 'R', 0, 1, 0x78, 16, 11, 0};
    std::copy(augmentation.begin(), augmentation.end(), bytes.begin() + 0x209);
    write<std::uint64_t>(bytes, 0x213, 0x1000);
    write<std::uint8_t>(bytes, 0x21B, 0);
    write<std::uint8_t>(bytes, 0x21C, 0);
    write<std::uint32_t>(bytes, 0x220, 29);
    write<std::uint32_t>(bytes, 0x224, 0x24);
    write<std::uint64_t>(bytes, 0x228, 0x1000);
    write<std::uint64_t>(bytes, 0x230, 0x20);
    write<std::uint8_t>(bytes, 0x238, 8);
    write<std::uint64_t>(bytes, 0x239, 0x280);
    const std::vector<std::uint8_t> lsda = {0xFF, 0xFF, 1, 4, 0, 1, 0x40, 0};
    std::copy(lsda.begin(), lsda.end(), bytes.begin() + 0x280);
    writeEhFrameHeader(bytes, 0x220);
    return bytes;
}

std::vector<std::uint8_t> personalityFreeEhFrame() {
    std::vector<std::uint8_t> bytes(0x400);
    write<std::uint32_t>(bytes, 0x200, 15);
    write<std::uint8_t>(bytes, 0x208, 1);
    const std::vector<std::uint8_t> augmentation = {'z', 'L', 'R', 0, 1, 0x78, 16, 2, 0, 0};
    std::copy(augmentation.begin(), augmentation.end(), bytes.begin() + 0x209);
    write<std::uint32_t>(bytes, 0x220, 29);
    write<std::uint32_t>(bytes, 0x224, 0x24);
    write<std::uint64_t>(bytes, 0x228, 0x1000);
    write<std::uint64_t>(bytes, 0x230, 0x20);
    write<std::uint8_t>(bytes, 0x238, 8);
    write<std::uint64_t>(bytes, 0x239, 0x280);
    const std::vector<std::uint8_t> lsda = {0xFF, 0xFF, 1, 3, 0, 1, 0x40, 0};
    std::copy(lsda.begin(), lsda.end(), bytes.begin() + 0x280);
    writeEhFrameHeader(bytes, 0x220);
    return bytes;
}

std::vector<std::uint8_t> elfFixture(const StrictReachabilityInput& input) {
    std::vector<std::uint8_t> bytes(0x400);
    bytes[0] = 0x7F;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 2;
    bytes[5] = 1;
    bytes[6] = 1;
    write<std::uint16_t>(bytes, 16, 3);
    write<std::uint16_t>(bytes, 18, 62);
    write<std::uint64_t>(bytes, 24, input.Entries.at(0));
    write<std::uint64_t>(bytes, 32, 64);
    write<std::uint16_t>(bytes, 54, 56);
    write<std::uint16_t>(bytes, 56, 2);
    write<std::uint32_t>(bytes, 64, 1);
    write<std::uint32_t>(bytes, 68, 5);
    write<std::uint64_t>(bytes, 72, 0x200);
    write<std::uint64_t>(bytes, 80, input.TextVaddr);
    write<std::uint64_t>(bytes, 96, input.Text.size());
    write<std::uint64_t>(bytes, 104, input.Text.size());
    write<std::uint32_t>(bytes, 120, 1);
    write<std::uint32_t>(bytes, 124, 6);
    write<std::uint64_t>(bytes, 128, 0x300);
    write<std::uint64_t>(bytes, 136, 0x2000);
    write<std::uint64_t>(bytes, 152, 0x100);
    write<std::uint64_t>(bytes, 160, 0x100);
    std::copy(input.Text.begin(), input.Text.end(), bytes.begin() + 0x200);
    return bytes;
}

void requireCallbackDataFiltered(std::uint32_t relocationType) {
    auto input = fixture();
    ripOperand(input, 0, {0x48, 0x8D, 0x3D}, 0x1020);
    emit(input, 7, {0xC3});
    ripOperand(input, 32, {0x48, 0x8B, 0x05}, 0x2000);
    emit(input, 39, {0xC3});
    ripOperand(input, 48, {0x48, 0x8B, 0x05}, 0x2008);
    emit(input, 55, {0xC3});
    const auto bytes = elfFixture(input);
    const std::vector<Relinker::NidReference> references = {{"callbackData", {}, relocationType, 0x300, 0x2000, 0}, {"deadData", {}, relocationType, 0x318, 0x2008, 0}};

    const auto filtered = Relinker::MakeStrictUnusedNidFilter()->Filter(references, bytes, input.Text, input.TextVaddr);

    Testing::RequireEqual(filtered.size(), std::size_t{1}, "Strict ELF filter did not keep exactly the callback data import");
    Testing::RequireEqual(filtered[0].Nid, references[0].Nid, "Strict ELF filter lost callback data or retained unreachable data");
}

StrictReachabilityInput pltInput() {
    auto input = fixture();
    importThunk(input, 0, 0x2000);
    importThunk(input, 16, 0x2008);
    return input;
}

std::vector<Relinker::NidReference> pltReferences() {
    return {{"live", {}, 7, 0x300, 0x2000, 0}, {"dead", {}, 7, 0x318, 0x2008, 0}};
}

StrictReachabilityInput pltThunkInput() {
    auto input = pltInput();
    for (std::size_t index = 0; index < 2; ++index) {
        const auto offset = index * 16;
        emit(input, offset + 6, {0x68, 0, 0, 0, 0, 0xE9, 0, 0, 0, 0});
        write<std::uint32_t>(input.Text, offset + 7, static_cast<std::uint32_t>(index));
    }
    return input;
}

auto compactDeadPlt(const std::vector<std::uint8_t>& text, std::uint64_t textVaddr) {
    const auto references = pltReferences();
    const std::vector<Relinker::NidReference> kept = {references[1]};
    return Relinker::UnusedNidFilter::CompactPlt(references, kept, text, textVaddr, 0, 0x300);
}

std::vector<std::uint8_t> patchedPltText() {
    auto input = pltThunkInput();
    const auto compacted = compactDeadPlt(input.Text, input.TextVaddr);
    for (const auto& patch : compacted.Patches)
        std::copy(patch.Bytes.begin(), patch.Bytes.end(), input.Text.begin() + static_cast<std::ptrdiff_t>(patch.Offset));
    return input.Text;
}

std::vector<std::uint8_t> relocationElf() {
    std::vector<std::uint8_t> bytes(0x400, 0);
    bytes[0] = 0x7F;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 2;
    bytes[5] = 1;
    bytes[6] = 1;
    write<std::uint16_t>(bytes, 16, 3);
    write<std::uint16_t>(bytes, 18, 62);
    write<std::uint64_t>(bytes, 32, 64);
    write<std::uint16_t>(bytes, 54, 56);
    write<std::uint16_t>(bytes, 56, 2);

    write<std::uint32_t>(bytes, 64, 1);
    write<std::uint32_t>(bytes, 68, 7);
    write<std::uint64_t>(bytes, 72, 0x200);
    write<std::uint64_t>(bytes, 80, 0x1000);
    write<std::uint64_t>(bytes, 96, 0x100);
    write<std::uint64_t>(bytes, 104, 0x100);

    write<std::uint32_t>(bytes, 120, 2);
    write<std::uint32_t>(bytes, 124, 6);
    write<std::uint64_t>(bytes, 128, 0x100);
    write<std::uint64_t>(bytes, 136, 0x2000);
    write<std::uint64_t>(bytes, 152, 0x80);
    write<std::uint64_t>(bytes, 160, 0x80);

    write<std::int64_t>(bytes, 0x100, 7);
    write<std::uint64_t>(bytes, 0x108, 0x10E8);
    write<std::int64_t>(bytes, 0x110, 8);
    write<std::uint64_t>(bytes, 0x118, 24);
    write<std::int64_t>(bytes, 0x120, 0);
    write<std::uint64_t>(bytes, 0x128, 0);

    write<std::uint64_t>(bytes, 0x2E8, 0x5000);
    write<std::uint64_t>(bytes, 0x2F0, 8);
    write<std::int64_t>(bytes, 0x2F8, 0x6000);

    write<std::uint64_t>(bytes, 0x300, 0x7000);
    write<std::uint64_t>(bytes, 0x308, 8);
    write<std::int64_t>(bytes, 0x310, 0x8000);
    return bytes;
}

std::vector<std::uint8_t> pltRelocationElf(std::uint64_t tableSize) {
    auto bytes = relocationElf();
    write<std::int64_t>(bytes, 0x100, 23);
    write<std::uint64_t>(bytes, 0x108, 0x10E8);
    write<std::int64_t>(bytes, 0x110, 2);
    write<std::uint64_t>(bytes, 0x118, tableSize);
    return bytes;
}

void requireRelocationIndexRejected(const std::vector<std::uint8_t>& bytes, const char* message) {
    Testing::RequireThrows<Relinker::RelinkerException>([&] { Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(bytes); }, message);
}

const Testing::Case ud2FallthroughImportIsRemoved{"StrictReachability_ImportAfterUd2_IsRemoved", [] {
    const auto input = ud2DeadTailInput();

    const auto result = AnalyzeStrictReachability(input);

    Testing::Require(result.ImportSlots.empty(), "UD2 fallthrough retained a dead import");
}};

const Testing::Case explicitEntryAfterUd2KeepsImport{"StrictReachability_ExplicitEntryAfterUd2_KeepsImport", [] {
    auto input = ud2DeadTailInput();
    input.Entries.push_back(0x1010);

    const auto result = AnalyzeStrictReachability(input);

    Testing::Require(result.ImportSlots == std::set<std::uint64_t>{0x2000}, "Explicit entry after UD2 lost its import");
}};

const Testing::Case ud1FallthroughImportIsRemoved{"StrictReachability_ImportAfterUd1_IsRemoved", [] {
    auto input = fixture();
    emit(input, 0, {0x0F, 0xB9, 0xC0});
    importThunk(input, 16, 0x2000);

    const auto result = AnalyzeStrictReachability(input);

    Testing::Require(result.ImportSlots.empty(), "UD1 fallthrough retained a dead import");
}};

const Testing::Case ud1RipOperandImportIsRemoved{"StrictReachability_ImportAfterRipRelativeUd1_IsRemoved", [] {
    auto input = fixture();
    ripOperand(input, 0, {0x0F, 0xB9, 0x05}, 0x3000);
    importThunk(input, 16, 0x2000);

    const auto result = AnalyzeStrictReachability(input);

    Testing::Require(result.ImportSlots.empty(), "UD1 with a RIP-relative operand retained a dead import");
}};

const Testing::Case conditionalTailCallIsKept{"StrictReachability_ConditionalTailCall_KeepsImport", [] {
    auto input = fixture();
    emit(input, 0, {0x75, 0x0E, 0xC3});
    importThunk(input, 16, 0x2000);

    const auto result = AnalyzeStrictReachability(input);

    Testing::Require(result.ImportSlots.contains(0x2000), "Conditional tail call was removed");
}};

const Testing::Case zeroPaddingKeepsFallthrough{"StrictReachability_ZeroPaddingAfterLastJump_KeepsFallthroughImport", [] {
    auto input = fixture();
    ripOperand(input, 0, {0xE9}, 0x1000);
    std::fill(input.Text.begin() + 5, input.Text.begin() + 16, 0);
    importThunk(input, 16, 0x2000);
    input.Functions = {{0x1000, 0x1010, {}}, {0x1010, 0x1018, {}}};

    const auto result = AnalyzeStrictReachability(input);

    Testing::Require(result.ImportSlots.contains(0x2000), "Zero padding after a region's last jump dropped its fall-through edge");
}};

const Testing::Case callbackAndRelocationRootsAreKept{"StrictReachability_AddressTakenCallbackAndRelocationRoot_KeepImports", [] {
    auto input = fixture();
    ripOperand(input, 0, {0x48, 0x8D, 0x3D}, 0x1020);
    emit(input, 7, {0xC3});
    importThunk(input, 32, 0x2000);
    importThunk(input, 48, 0x2008);
    input.Pointers.emplace(0x3000, 0x1030);

    const auto result = AnalyzeStrictReachability(input);

    Testing::Require(result.ImportSlots == input.ImportSlots, "Address-taken callback or relocation root was removed");
}};

const Testing::Case registerImportCallIsResolved{"StrictReachability_RegisterImportCall_IsResolved", [] {
    auto input = fixture();
    ripOperand(input, 0, {0x48, 0x8B, 0x05}, 0x2000);
    emit(input, 7, {0xFF, 0xD0, 0xC3});

    const auto result = AnalyzeStrictReachability(input);

    Testing::Require(result.ImportSlots.contains(0x2000), "Imported register call lost its import");
    Testing::RequireEqual(result.IndirectTransfers, std::size_t{1}, "Imported register call indirect transfer count");
}};

const Testing::Case mergedJumpTableKeepsTargets{"StrictReachability_MergedIndexedJumpTable_KeepsAllTargets", [] {
    auto input = fixture();
    emit(input, 0, {0x75, 0x07, 0xB9, 0, 0, 0, 0, 0xEB, 0x05, 0xB9, 1, 0, 0, 0});
    ripOperand(input, 14, {0x48, 0x8D, 0x05}, 0x3000);
    emit(input, 21, {0xFF, 0x24, 0xC8});
    importThunk(input, 48, 0x2000);
    importThunk(input, 64, 0x2008);
    input.Pointers = {{0x3000, 0x1030}, {0x3008, 0x1040}};

    const auto result = AnalyzeStrictReachability(input);

    Testing::Require(result.ImportSlots == input.ImportSlots, "Merged indexed jump table lost a target");
}};

const Testing::Case unknownIndirectKeepsAddressTakenTarget{"StrictReachability_UnknownIndirectTransfer_KeepsOnlyAddressTakenTarget", [] {
    const auto input = unknownIndirectInput();

    const auto result = AnalyzeStrictReachability(input);

    Testing::Require(result.ImportSlots == std::set<std::uint64_t>{0x2000}, "Unknown indirect transfer lost its address-taken target");
}};

const Testing::Case overlappingFunctionsAreRejected{"StrictReachability_OverlappingUnwindFunctions_Throws", [] {
    auto input = unknownIndirectInput();
    input.Functions = {{0x1000, 0x1040, {}}, {0x1020, 0x1050, {}}};

    Testing::RequireThrows<Relinker::RelinkerException>([&] { AnalyzeStrictReachability(input); }, "Overlapping unwind functions were accepted");
}};

const Testing::Case unrootedCycleIsRemoved{"StrictReachability_UnrootedFunctionCycle_IsRemoved", [] {
    const auto input = functionCycleInput();

    const auto result = AnalyzeStrictReachability(input);

    Testing::Require(result.ImportSlots.empty(), "Unrooted function cycle kept its import");
}};

const Testing::Case rootedCycleIsKept{"StrictReachability_RootedFunctionCycle_KeepsImport", [] {
    auto input = functionCycleInput();
    input.Pointers.emplace(0x3000, 0x1020);

    const auto result = AnalyzeStrictReachability(input);

    Testing::Require(result.ImportSlots.contains(0x2000), "Rooted function cycle lost its import");
}};

const Testing::Case relativeJumpTableKeepsTarget{"StrictReachability_RelativeJumpTable_KeepsOnlyTableTarget", [] {
    auto input = fixture();
    ripOperand(input, 0, {0x48, 0x8D, 0x05}, 0x3000);
    emit(input, 7, {0xFF, 0xE0});
    importThunk(input, 32, 0x2000);
    importThunk(input, 64, 0x2008);
    Relinker::UnusedNidFilter::StrictDataRegion data{0x3000, std::vector<std::uint8_t>(8)};
    write<std::int32_t>(data.Bytes, 0, 0x1020 - 0x3000);
    input.Data.push_back(data);

    const auto result = AnalyzeStrictReachability(input);

    Testing::Require(result.ImportSlots == std::set<std::uint64_t>{0x2000}, "Relative jump-table target was removed");
}};

const Testing::Case unknownSwitchInsideLiveFunctionIsKept{"StrictReachability_UnknownSwitchTargetInsideLiveFunction_KeepsImport", [] {
    auto input = fixture();
    emit(input, 0, {0xFF, 0xE0});
    importThunk(input, 32, 0x2000);
    input.Functions = {{0x1000, 0x1026, {}}};

    const auto result = AnalyzeStrictReachability(input);

    Testing::Require(result.ImportSlots.contains(0x2000), "Unknown switch target inside a live function was removed");
}};

const Testing::Case validCieLeb128IsRead{"EhFrameReader_ValidCieLeb128_ReadsFunctionRange", [] {
    requireCieLeb128({1}, {0x78}, false, "code=1 data=0x78");
}};

const Testing::Case overlongDataAlignmentLeb128{"EhFrameReader_OverlongDataAlignmentLeb128_RejectsOnlyOverflowingValues", [] {
    for (const std::uint8_t prefix : {0x80, 0xFF}) {
        for (unsigned terminal = 0; terminal < 0x80; ++terminal)
            requireCieLeb128({1}, overlongLeb128(prefix, terminal), terminal != 0 && terminal != 0x7F, "data " + leb128Description(prefix, terminal));
    }
}};

const Testing::Case overlongCodeAlignmentLeb128{"EhFrameReader_OverlongCodeAlignmentLeb128_RejectsOnlyOverflowingValues", [] {
    for (const std::uint8_t prefix : {0x80, 0xFF}) {
        for (unsigned terminal = 0; terminal < 0x80; ++terminal)
            requireCieLeb128(overlongLeb128(prefix, terminal), {0x78}, terminal != 0 && terminal != 1, "code " + leb128Description(prefix, terminal));
    }
}};

const Testing::Case vectorInstructionLengths{"X64InstructionDecoder_VectorPopcntAndIntInstructions_DecodeFullLength", [] {
    const Codegen::X64InstructionDecoder decoder;
    const std::vector<std::vector<std::uint8_t>> instructions = {
        {0xC5, 0xD9, 0x73, 0xD4, 0x20},
        {0xC4, 0xE1, 0x79, 0x70, 0xC0, 0x1B},
        {0xC4, 0xE1, 0x78, 0x77},
        {0x62, 0xF1, 0x7D, 0x48, 0x72, 0xD0, 0x04},
        {0xF3, 0x0F, 0xB8, 0xC0},
        {0xCD, 0x41}
    };

    for (std::size_t index = 0; index < instructions.size(); ++index) {
        const auto& instruction = instructions[index];
        Testing::RequireEqual(static_cast<std::size_t>(decoder.Decode(instruction.data(), instruction.size())), instruction.size(),
                              "Vector immediate, VZEROUPPER, POPCNT or INT length for instruction " + std::to_string(index));
    }
}};

const Testing::Case lsdaLandingPadIsRecovered{"EhFrameReader_LsdaLandingPadOutsideFunction_IsRecovered", [] {
    const auto bytes = landingPadEhFrame();

    const auto functions = Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, exceptionHeaders(bytes.size()), {}, {});

    Testing::RequireEqual(functions.size(), std::size_t{1}, "Landing pad function count");
    Testing::Require(functions[0].ExtraTargets == std::vector<std::uint64_t>{0x1000, 0x1040}, "LSDA landing pad outside the function was not recovered");
}};

const Testing::Case exceptionOnlyImportIsKept{"StrictReachability_ExceptionOnlyImport_IsKept", [] {
    const auto bytes = landingPadEhFrame();
    auto input = fixture();
    input.Functions = Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, exceptionHeaders(bytes.size()), {}, {});
    emit(input, 0, {0x0F, 0x0B});
    emit(input, 31, {0xC3});
    importThunk(input, 64, 0x2000);

    const auto result = AnalyzeStrictReachability(input);

    Testing::Require(result.ImportSlots.contains(0x2000), "Exception-only import was removed");
}};

const Testing::Case malformedLsdaIsRejected{"EhFrameReader_MalformedLsda_Throws", [] {
    auto bytes = landingPadEhFrame();
    write<std::uint8_t>(bytes, 0x282, 0xFF);
    const auto headers = exceptionHeaders(bytes.size());

    Testing::RequireThrows<Relinker::RelinkerException>([&] { Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {}); }, "Malformed LSDA was accepted");
}};

const Testing::Case lsdaWithoutPersonalityIsIgnored{"EhFrameReader_LsdaWithoutPersonality_IsNotReadAsCallSiteTable", [] {
    const auto bytes = personalityFreeEhFrame();

    const auto functions = Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, exceptionHeaders(bytes.size()), {}, {});

    Testing::RequireEqual(functions.size(), std::size_t{1}, "Personality-free function count");
    Testing::RequireEqual(functions[0].Begin, std::uint64_t{0x1000}, "Personality-free function begin");
    Testing::RequireEqual(functions[0].End, std::uint64_t{0x1020}, "Personality-free function end");
    Testing::Require(functions[0].ExtraTargets.empty(), "LSDA without a personality routine was read as a C++ call-site table");
}};

const Testing::Case absoluteCallbackDataIsFiltered{"StrictNidFilter_AbsoluteDataRelocation_KeepsOnlyCallbackData", [] {
    requireCallbackDataFiltered(1);
}};

const Testing::Case globDatCallbackDataIsFiltered{"StrictNidFilter_GlobDatRelocation_KeepsOnlyCallbackData", [] {
    requireCallbackDataFiltered(6);
}};

const Testing::Case deadPltImportIsRemoved{"StrictNidFilter_DeadPltImport_IsRemoved", [] {
    const auto input = pltInput();
    const auto bytes = elfFixture(input);
    const auto references = pltReferences();

    const auto filtered = Relinker::MakeStrictUnusedNidFilter()->Filter(references, bytes, input.Text, input.TextVaddr);

    Testing::RequireEqual(filtered.size(), std::size_t{1}, "Strict ELF filter kept the wrong number of PLT imports");
    Testing::RequireEqual(filtered[0].Nid, references[0].Nid, "Strict ELF filter did not remove only the dead PLT import");
}};

const Testing::Case pltRelocationIndicesAreCompacted{"PltCompactor_KeptReference_GetsCompactedSlotAndRelocationOffset", [] {
    const auto input = pltThunkInput();

    const auto compacted = compactDeadPlt(input.Text, input.TextVaddr);

    Testing::RequireEqual(compacted.SlotCount, decltype(compacted.SlotCount){1}, "Compacted PLT slot count");
    Testing::RequireEqual(compacted.References[0].RelocationTableOffset, decltype(compacted.References[0].RelocationTableOffset){0x300},
                          "PLT relocation indices were not compacted");
}};

const Testing::Case mismatchedPltIndexIsRejected{"PltCompactor_MismatchedOriginalPltIndex_Throws", [] {
    auto input = pltThunkInput();
    write<std::uint32_t>(input.Text, 23, 99);

    Testing::RequireThrows<Relinker::RelinkerException>([&] { compactDeadPlt(input.Text, input.TextVaddr); }, "Mismatched original PLT index was accepted");
}};

const Testing::Case pltPatchesRenumberAndTrap{"PltCompactor_Patches_RenumberKeptThunkAndTrapRemovedThunk", [] {
    const auto text = patchedPltText();

    std::uint32_t newIndex;
    std::memcpy(&newIndex, text.data() + 23, sizeof(newIndex));
    Testing::RequireEqual(newIndex, std::uint32_t{0}, "PLT thunk index was not renumbered");
    Testing::RequireEqual(text[0], std::uint8_t{0x0F}, "Removed thunk jump trap byte 0");
    Testing::RequireEqual(text[1], std::uint8_t{0x0B}, "Removed thunk jump trap byte 1");
    Testing::RequireEqual(text[6], std::uint8_t{0x0F}, "Removed thunk push trap byte 0");
    Testing::RequireEqual(text[7], std::uint8_t{0x0B}, "Removed thunk push trap byte 1");
}};

const Testing::Case compactedDynamicSectionDropsDeadSymbol{"SysVDynamicSectionBuilder_CompactedPlt_OmitsDeadSymbol", [] {
    const auto input = pltThunkInput();
    const auto compacted = compactDeadPlt(input.Text, input.TextVaddr);
    Relinker::SysVDynamicSectionBuilder builder;

    const auto section = builder.BuildDynamicSection(compacted.References, {}, 0x300, compacted.SlotCount);

    Testing::RequireEqual(section.RelaPltData.size(), std::size_t{24}, "Compacted PLT relocation table size");
    Testing::RequireEqual(section.DynSymData.size(), std::size_t{48}, "Compacted PLT retained a dead dynamic symbol");
}};

const Testing::Case truncatedElfIsRejected{"StrictNidFilter_TruncatedElf_Throws", [] {
    const auto input = pltInput();
    auto truncated = elfFixture(input);
    truncated.resize(70);
    const auto references = pltReferences();
    const auto text = patchedPltText();

    Testing::RequireThrows<Relinker::RelinkerException>(
        [&] { Relinker::MakeStrictUnusedNidFilter()->Filter(references, truncated, text, input.TextVaddr); }, "Truncated ELF was accepted");
}};

const Testing::Case exceptionMetadataIsNotIgnored{"StrictNidFilter_MalformedExceptionMetadata_Throws", [] {
    const auto input = pltInput();
    auto exceptional = elfFixture(input);
    write<std::uint16_t>(exceptional, 56, 3);
    write<std::uint32_t>(exceptional, 176, 0x6474E550);
    write<std::uint64_t>(exceptional, 184, 0x380);
    write<std::uint64_t>(exceptional, 208, 8);
    const auto references = pltReferences();
    const auto text = patchedPltText();

    Testing::RequireThrows<Relinker::RelinkerException>(
        [&] { Relinker::MakeStrictUnusedNidFilter()->Filter(references, exceptional, text, input.TextVaddr); }, "Exception metadata was ignored");
}};

const Testing::Case validRelativeRelocationIsIndexed{"RelativeRelocationIndex_ValidRelocation_IsIndexed", [] {
    const auto bytes = relocationElf();

    const auto index = Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(bytes);

    const auto target = index->TargetOfSlot(0x5000);
    Testing::Require(target.has_value(), "Valid relative relocation was not indexed");
    Testing::RequireEqual(*target, std::uint64_t{0x6000}, "Valid relative relocation target");
}};

const Testing::Case outOfSegmentRelocationIsIgnored{"RelativeRelocationIndex_RelocationOutsideTable_IsNotIndexed", [] {
    const auto bytes = relocationElf();

    const auto index = Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(bytes);

    Testing::Require(!index->TargetOfSlot(0x7000).has_value(), "Out of segment relocation was indexed in valid table");
}};

const Testing::Case emptyRelocationTableIsEmpty{"RelativeRelocationIndex_EmptyTable_IndexesNothing", [] {
    auto bytes = relocationElf();
    write<std::uint64_t>(bytes, 0x108, ~std::uint64_t{0});
    write<std::uint64_t>(bytes, 0x118, 0);

    const auto index = Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(bytes);

    Testing::Require(!index->TargetOfSlot(0x5000).has_value(), "Empty relocation table was unexpectedly parsed");
}};

const Testing::Case overextendedTableIsRejected{"RelativeRelocationIndex_TableBeyondLoadSegment_Throws", [] {
    auto bytes = relocationElf();
    write<std::uint64_t>(bytes, 0x118, 48);

    requireRelocationIndexRejected(bytes, "Relocation table extending beyond PT_LOAD was accepted");
}};

const Testing::Case unalignedTableIsRejected{"RelativeRelocationIndex_UnalignedTableSize_Throws", [] {
    auto bytes = relocationElf();
    write<std::uint64_t>(bytes, 0x118, 25);

    requireRelocationIndexRejected(bytes, "Unaligned relocation table size was accepted");
}};

const Testing::Case overextendedPltTableIsRejected{"RelativeRelocationIndex_PltTableBeyondLoadSegment_Throws", [] {
    const auto bytes = pltRelocationElf(48);

    requireRelocationIndexRejected(bytes, "PLT relocation table extending beyond PT_LOAD was accepted");
}};

const Testing::Case unalignedPltTableIsRejected{"RelativeRelocationIndex_UnalignedPltTableSize_Throws", [] {
    const auto bytes = pltRelocationElf(1);

    requireRelocationIndexRejected(bytes, "Unaligned PLT relocation table size was accepted");
}};

const Testing::Case overflowingTableSizeIsRejected{"RelativeRelocationIndex_TableSizeNearMaximum_Throws", [] {
    auto bytes = relocationElf();
    write<std::uint64_t>(bytes, 0x118, (~std::uint64_t{0} / 24) * 24);

    requireRelocationIndexRejected(bytes, "Relocation table size near UINT64_MAX was accepted");
}};

const Testing::Case overflowingTableAddressIsRejected{"RelativeRelocationIndex_TableAddressNearMaximum_Throws", [] {
    auto bytes = relocationElf();
    write<std::uint64_t>(bytes, 0x108, ~std::uint64_t{0} - 15);

    requireRelocationIndexRejected(bytes, "Relocation table vaddr near UINT64_MAX was accepted");
}};

const Testing::Case overflowingLoadOffsetIsRejected{"RelativeRelocationIndex_LoadOffsetNearMaximum_Throws", [] {
    auto bytes = relocationElf();
    write<std::uint64_t>(bytes, 72, ~std::uint64_t{0} - 16);

    requireRelocationIndexRejected(bytes, "PT_LOAD file offset near UINT64_MAX was accepted");
}};

const Testing::Case overflowingLoadSizeIsRejected{"RelativeRelocationIndex_TableBeyondFileWithLoadSizeNearMaximum_Throws", [] {
    auto bytes = relocationElf();
    write<std::uint64_t>(bytes, 96, ~std::uint64_t{0} - 16);
    write<std::uint64_t>(bytes, 0x118, 0x300);

    requireRelocationIndexRejected(bytes, "Relocation table beyond file with fileSize near UINT64_MAX was accepted");
}};

const Testing::Case overflowingDynamicOffsetIsRejected{"RelativeRelocationIndex_DynamicOffsetNearMaximum_Throws", [] {
    auto bytes = relocationElf();
    write<std::uint64_t>(bytes, 128, ~std::uint64_t{0} - 16);

    requireRelocationIndexRejected(bytes, "PT_DYNAMIC offset near UINT64_MAX was accepted");
}};

} // namespace
