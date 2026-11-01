#include "TestProgress.hpp"
#include <relinker/analysis/UnusedNidFilter.hpp>
#include <relinker/analysis/UnusedNidFilter/StrictReachability.hpp>
#include <relinker/output/SysVDynamicSectionBuilder.hpp>
#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <relinker/analysis/UnusedNidFilter/PltCompactor.hpp>
#include <relinker/analysis/UnusedNidFilter/IRelativeRelocationIndex.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using Relinker::UnusedNidFilter::AnalyzeStrictReachability;
using Relinker::UnusedNidFilter::StrictReachabilityInput;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void requireFailure(const std::function<void()>& operation, const char* message) {
    try {
        operation();
    } catch (const Relinker::RelinkerException&) {
        return;
    }
    throw std::runtime_error(message);
}

template<typename TValue>
void write(std::vector<std::uint8_t>& bytes, std::size_t offset, TValue value) {
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) throw std::runtime_error("Test fixture write is out of bounds");
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

void deadTailAndExplicitEntry() {
    auto input = fixture();
    emit(input, 0, {0x0F, 0x0B});
    importThunk(input, 16, 0x2000);
    const auto dead = AnalyzeStrictReachability(input);
    require(dead.ImportSlots.empty(), "UD2 fallthrough retained a dead import");
    input.Entries.push_back(0x1010);
    const auto live = AnalyzeStrictReachability(input);
    require(live.ImportSlots == std::set<std::uint64_t>{0x2000}, "Explicit entry after UD2 lost its import");
}

void ud1DeadTail() {
    auto input = fixture();
    emit(input, 0, {0x0F, 0xB9, 0xC0});
    importThunk(input, 16, 0x2000);
    require(AnalyzeStrictReachability(input).ImportSlots.empty(), "UD1 fallthrough retained a dead import");
    input = fixture();
    ripOperand(input, 0, {0x0F, 0xB9, 0x05}, 0x3000);
    importThunk(input, 16, 0x2000);
    require(AnalyzeStrictReachability(input).ImportSlots.empty(), "UD1 with a RIP-relative operand retained a dead import");
}

void conditionalTailCall() {
    auto input = fixture();
    emit(input, 0, {0x75, 0x0E, 0xC3});
    importThunk(input, 16, 0x2000);
    require(AnalyzeStrictReachability(input).ImportSlots.contains(0x2000), "Conditional tail call was removed");
}

void fallthroughAfterZeroPadding() {
    auto input = fixture();
    ripOperand(input, 0, {0xE9}, 0x1000);
    std::fill(input.Text.begin() + 5, input.Text.begin() + 16, 0);
    importThunk(input, 16, 0x2000);
    input.Functions = {{0x1000, 0x1010, {}}, {0x1010, 0x1018, {}}};
    require(AnalyzeStrictReachability(input).ImportSlots.contains(0x2000), "Zero padding after a region's last jump dropped its fall-through edge");
}

void addressSizeOverrideImport() {
    auto input = fixture();
    input.Text.assign(32, 0);
    input.TextVaddr = 0x100001000ull;
    input.Entries = {input.TextVaddr};
    input.ImportSlots = {0x2000, 0x100002000ull};
    emit(input, 0, {0x67, 0xFF, 0x15, 0xF9, 0x0F, 0x00, 0x00, 0xC3});
    require(AnalyzeStrictReachability(input).ImportSlots == std::set<std::uint64_t>{0x2000}, "EIP-relative import was not resolved with 32-bit address wrapping");
    input.Text.assign(32, 0);
    emit(input, 0, {0x67, 0x48, 0x8B, 0x05, 0xF8, 0x0F, 0x00, 0x00, 0xC3});
    require(AnalyzeStrictReachability(input).ImportSlots == std::set<std::uint64_t>{0x2000}, "REX.W EIP-relative load did not resolve its import");
    input.Text.assign(32, 0);
    emit(input, 0, {0xFF, 0x15, 0xFA, 0x0F, 0x00, 0x00, 0xC3});
    require(AnalyzeStrictReachability(input).ImportSlots == std::set<std::uint64_t>{0x100002000ull}, "RIP-relative import lost its full 64-bit address");
}

void callbackAndRelocationRoots() {
    auto input = fixture();
    ripOperand(input, 0, {0x48, 0x8D, 0x3D}, 0x1020);
    emit(input, 7, {0xC3});
    importThunk(input, 32, 0x2000);
    importThunk(input, 48, 0x2008);
    input.Pointers.emplace(0x3000, 0x1030);
    require(AnalyzeStrictReachability(input).ImportSlots == input.ImportSlots, "Address-taken callback or relocation root was removed");
}

void registerImportCall() {
    auto input = fixture();
    ripOperand(input, 0, {0x48, 0x8B, 0x05}, 0x2000);
    emit(input, 7, {0xFF, 0xD0, 0xC3});
    const auto result = AnalyzeStrictReachability(input);
    require(result.ImportSlots.contains(0x2000) && result.IndirectTransfers == 1, "Imported register call was not resolved");

}

void mergedJumpTable() {
    auto input = fixture();
    emit(input, 0, {0x75, 0x07, 0xB9, 0, 0, 0, 0, 0xEB, 0x05, 0xB9, 1, 0, 0, 0});
    ripOperand(input, 14, {0x48, 0x8D, 0x05}, 0x3000);
    emit(input, 21, {0xFF, 0x24, 0xC8});
    importThunk(input, 48, 0x2000);
    importThunk(input, 64, 0x2008);
    input.Pointers = {{0x3000, 0x1030}, {0x3008, 0x1040}};
    require(AnalyzeStrictReachability(input).ImportSlots == input.ImportSlots, "Merged indexed jump table lost a target");
}

void conservativeIndirectTargets() {
    auto input = fixture();
    emit(input, 0, {0xFF, 0xE0});
    importThunk(input, 32, 0x2000);
    importThunk(input, 64, 0x2008);
    input.Pointers.emplace(0x3000, 0x1020);
    const auto result = AnalyzeStrictReachability(input);
    require(result.ImportSlots == std::set<std::uint64_t>{0x2000}, "Unknown indirect transfer lost its address-taken target");
    input.Functions = {{0x1000, 0x1040, {}}, {0x1020, 0x1050, {}}};
    requireFailure([&] { AnalyzeStrictReachability(input); }, "Overlapping unwind functions were accepted");
}

void isolatedFunctionCycle() {
    auto input = fixture();
    emit(input, 0, {0xC3});
    ripOperand(input, 32, {0xE8}, 0x1030);
    emit(input, 37, {0xC3});
    ripOperand(input, 48, {0xE8}, 0x1020);
    importThunk(input, 53, 0x2000);
    input.Functions = {{0x1000, 0x1001, {}}, {0x1020, 0x1026, {}}, {0x1030, 0x103B, {}}};
    require(AnalyzeStrictReachability(input).ImportSlots.empty(), "Unrooted function cycle kept its import");
    input.Pointers.emplace(0x3000, 0x1020);
    require(AnalyzeStrictReachability(input).ImportSlots.contains(0x2000), "Rooted function cycle lost its import");
}

void relativeTableAndWholeFunction() {
    auto input = fixture();
    ripOperand(input, 0, {0x48, 0x8D, 0x05}, 0x3000);
    emit(input, 7, {0xFF, 0xE0});
    importThunk(input, 32, 0x2000);
    importThunk(input, 64, 0x2008);
    Relinker::UnusedNidFilter::StrictDataRegion data{0x3000, std::vector<std::uint8_t>(8)};
    write<std::int32_t>(data.Bytes, 0, 0x1020 - 0x3000);
    input.Data.push_back(data);
    require(AnalyzeStrictReachability(input).ImportSlots == std::set<std::uint64_t>{0x2000}, "Relative jump-table target was removed");
    input = fixture();
    emit(input, 0, {0xFF, 0xE0});
    importThunk(input, 32, 0x2000);
    input.Functions = {{0x1000, 0x1026, {}}};
    require(AnalyzeStrictReachability(input).ImportSlots.contains(0x2000), "Unknown switch target inside a live function was removed");
}

void indexedTableImports() {
    auto input = fixture();
    ripOperand(input, 0, {0x48, 0x8D, 0x05}, 0x2000);
    emit(input, 7, {0xFF, 0x14, 0xF8, 0xC3});
    input.Data.push_back({0x2000, std::vector<std::uint8_t>(16)});
    require(AnalyzeStrictReachability(input).ImportSlots == std::set<std::uint64_t>{0x2000, 0x2008}, "Import slot reached by indexing a live table base was removed");
}

void exceptionLeb128Bounds() {
    const auto check = [](const std::vector<std::uint8_t>& codeAlignment, const std::vector<std::uint8_t>& dataAlignment, bool overflowing) {
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
        write<std::uint8_t>(bytes, 0x300, 1);
        write<std::uint8_t>(bytes, 0x302, 3);
        write<std::uint64_t>(bytes, 0x304, 0x200);
        write<std::uint32_t>(bytes, 0x30C, 1);
        write<std::uint64_t>(bytes, 0x310, 0x1000);
        write<std::uint64_t>(bytes, 0x318, 0x240);
        const std::vector<Relinker::ProgramHeader> headers = {{1, 4, 0, 0, 0, bytes.size(), bytes.size(), 8}, {0x6474E550, 4, 0x300, 0x300, 0, 32, 32, 4}};
        try {
            const auto functions = Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            require(!overflowing, "Overflowing CIE LEB128 value was accepted");
            require(functions.size() == 1 && functions[0].Begin == 0x1000 && functions[0].End == 0x1020 && functions[0].ExtraTargets.empty(), "Valid CIE LEB128 changed the function range");
        } catch (const Relinker::RelinkerException& error) {
            require(overflowing && std::string(error.what()).find("overflowing LEB128 value") != std::string::npos, "CIE LEB128 was rejected for an unexpected reason");
        }
    };
    check({1}, {0x78}, false);
    for (const std::uint8_t prefix : {0x80, 0xFF}) {
        for (unsigned terminal = 0; terminal < 0x80; ++terminal) {
            std::vector<std::uint8_t> encoded(10, prefix);
            encoded.back() = static_cast<std::uint8_t>(terminal);
            check({1}, encoded, terminal != 0 && terminal != 0x7F);
            check(encoded, {0x78}, terminal != 0 && terminal != 1);
        }
    }
}

void vectorInstructionLengths() {
    const Codegen::X64InstructionDecoder decoder;
    const std::vector<std::vector<std::uint8_t>> instructions = {
        {0xC5, 0xD9, 0x73, 0xD4, 0x20},
        {0xC4, 0xE1, 0x79, 0x70, 0xC0, 0x1B},
        {0xC4, 0xE1, 0x78, 0x77},
        {0x62, 0xF1, 0x7D, 0x48, 0x72, 0xD0, 0x04},
        {0xF3, 0x0F, 0xB8, 0xC0},
        {0xCD, 0x41}
    };
    for (const auto& instruction : instructions)
        require(decoder.Decode(instruction.data(), instruction.size()) == instruction.size(), "Vector immediate, VZEROUPPER, POPCNT or INT was decoded with the wrong length");
}

void exceptionLandingPads() {
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
    write<std::uint8_t>(bytes, 0x300, 1);
    write<std::uint8_t>(bytes, 0x302, 3);
    write<std::uint64_t>(bytes, 0x304, 0x200);
    write<std::uint32_t>(bytes, 0x30C, 1);
    write<std::uint64_t>(bytes, 0x310, 0x1000);
    write<std::uint64_t>(bytes, 0x318, 0x220);
    const std::vector<Relinker::ProgramHeader> headers = {{1, 4, 0, 0, 0, bytes.size(), bytes.size(), 8}, {0x6474E550, 4, 0x300, 0x300, 0, 32, 32, 4}};
    auto input = fixture();
    input.Functions = Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
    require(input.Functions.size() == 1 && input.Functions[0].ExtraTargets == std::vector<std::uint64_t>{0x1000, 0x1040}, "LSDA landing pad outside the function was not recovered");
    emit(input, 0, {0x0F, 0x0B});
    emit(input, 31, {0xC3});
    importThunk(input, 64, 0x2000);
    require(AnalyzeStrictReachability(input).ImportSlots.contains(0x2000), "Exception-only import was removed");
    const auto checkLanding = [&](std::uint64_t base, std::uint64_t offset, bool signedOffset, bool rejected) {
        auto data = bytes;
        write<std::uint8_t>(data, 0x280, 0);
        write<std::uint64_t>(data, 0x281, base);
        write<std::uint8_t>(data, 0x289, 0xFF);
        write<std::uint8_t>(data, 0x28A, signedOffset ? 12 : 4);
        write<std::uint8_t>(data, 0x28B, 25);
        write<std::uint64_t>(data, 0x28C, 0);
        write<std::uint64_t>(data, 0x294, 1);
        write<std::uint64_t>(data, 0x29C, offset);
        write<std::uint8_t>(data, 0x2A4, 0);
        try {
            const auto functions = Relinker::UnusedNidFilter::ReadExceptionFunctions(data, headers, {}, {});
            require(!rejected, "Overflowing LSDA landing pad address was accepted");
            const auto expected = offset == 0 ? std::vector<std::uint64_t>{0x1000} :
                std::vector<std::uint64_t>{0x1000, base + offset};
            require(functions.size() == 1 && functions[0].ExtraTargets == expected,
                    "Valid LSDA landing pad address changed");
        } catch (const Relinker::RelinkerException& error) {
            require(rejected && std::string(error.what()).find("LSDA landing pad address overflowed") != std::string::npos,
                    "LSDA landing pad rejected for an unexpected reason");
        }
    };
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    for (const std::uint64_t base : {std::uint64_t{0x1000}, maximum - 16}) {
        checkLanding(base, 0, false, false);
        checkLanding(base, maximum - base, false, false);
        checkLanding(base, maximum - base + 1, false, true);
        checkLanding(base, maximum, false, true);
    }
    checkLanding(0x1000, maximum, true, false);
    checkLanding(0x1000, std::uint64_t{0} - 0x1000, true, false);
    checkLanding(0x1000, std::uint64_t{0} - 0x1001, true, true);
    checkLanding(maximum - 16, 16, true, false);
    checkLanding(maximum - 16, 17, true, true);
    checkLanding(0, 0, true, false);
    checkLanding(0, maximum, false, false);
    checkLanding(std::uint64_t{1} << 63, std::uint64_t{1} << 63, true, false);
    checkLanding((std::uint64_t{1} << 63) - 1, std::uint64_t{1} << 63, true, true);
    write<std::uint8_t>(bytes, 0x282, 0xFF);
    requireFailure([&] { Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {}); }, "Malformed LSDA was accepted");
}

void exceptionDataWithoutPersonality() {
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
    write<std::uint8_t>(bytes, 0x300, 1);
    write<std::uint8_t>(bytes, 0x302, 3);
    write<std::uint64_t>(bytes, 0x304, 0x200);
    write<std::uint32_t>(bytes, 0x30C, 1);
    write<std::uint64_t>(bytes, 0x310, 0x1000);
    write<std::uint64_t>(bytes, 0x318, 0x220);
    const std::vector<Relinker::ProgramHeader> headers = {{1, 4, 0, 0, 0, bytes.size(), bytes.size(), 8}, {0x6474E550, 4, 0x300, 0x300, 0, 32, 32, 4}};
    const auto functions = Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
    require(functions.size() == 1 && functions[0].Begin == 0x1000 && functions[0].End == 0x1020 && functions[0].ExtraTargets.empty(), "LSDA without a personality routine was read as a C++ call-site table");
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
    write<std::uint32_t>(bytes, 20, 1);
    write<std::uint64_t>(bytes, 24, input.Entries.at(0));
    write<std::uint64_t>(bytes, 32, 64);
    write<std::uint16_t>(bytes, 52, 64);
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

void filterCallbackDataImports() {
    auto input = fixture();
    ripOperand(input, 0, {0x48, 0x8D, 0x3D}, 0x1020);
    emit(input, 7, {0xC3});
    ripOperand(input, 32, {0x48, 0x8B, 0x05}, 0x2000);
    emit(input, 39, {0xC3});
    ripOperand(input, 48, {0x48, 0x8B, 0x05}, 0x2008);
    emit(input, 55, {0xC3});
    const auto bytes = elfFixture(input);
    for (const std::uint32_t relocationType : {1, 6}) {
        const std::vector<Relinker::NidReference> references = {{"callbackData", {}, relocationType, 0x300, 0x2000, 0}, {"deadData", {}, relocationType, 0x318, 0x2008, 0}};
        const auto filtered = Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, bytes, input.Text, input.TextVaddr);
        require(filtered.size() == 1 && filtered[0].Nid == references[0].Nid, "Strict ELF filter lost callback data or retained unreachable data");
    }
}

void filterAndPltCompaction() {
    auto input = fixture();
    importThunk(input, 0, 0x2000);
    importThunk(input, 16, 0x2008);
    const auto bytes = elfFixture(input);
    const std::vector<Relinker::NidReference> references = {{"live", {}, 7, 0x300, 0x2000, 0}, {"dead", {}, 7, 0x318, 0x2008, 0}};
    const auto filtered = Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, bytes, input.Text, input.TextVaddr);
    require(filtered.size() == 1 && filtered[0].Nid == references[0].Nid, "Strict ELF filter did not remove only the dead PLT import");
    for (std::size_t index = 0; index < references.size(); ++index) {
        const auto offset = index * 16;
        emit(input, offset + 6, {0x68, 0, 0, 0, 0, 0xE9, 0, 0, 0, 0});
        write<std::uint32_t>(input.Text, offset + 7, static_cast<std::uint32_t>(index));
    }
    const std::vector<Relinker::NidReference> kept = {references[1]};
    auto compacted = Relinker::UnusedNidFilter::CompactPlt(references, kept, input.Text, input.TextVaddr, 0, 0x300);
    require(compacted.SlotCount == 1 && compacted.References[0].RelocationTableOffset == 0x300, "PLT relocation indices were not compacted");
    auto invalidThunks = input.Text;
    write<std::uint32_t>(invalidThunks, 23, 99);
    requireFailure([&] { Relinker::UnusedNidFilter::CompactPlt(references, kept, invalidThunks, input.TextVaddr, 0, 0x300); }, "Mismatched original PLT index was accepted");
    for (const auto& patch : compacted.Patches)
        std::copy(patch.Bytes.begin(), patch.Bytes.end(), input.Text.begin() + static_cast<std::ptrdiff_t>(patch.Offset));
    std::uint32_t newIndex;
    std::memcpy(&newIndex, input.Text.data() + 23, sizeof(newIndex));
    require(newIndex == 0 && input.Text[0] == 0x0F && input.Text[1] == 0x0B && input.Text[6] == 0x0F && input.Text[7] == 0x0B, "PLT thunk index or removed thunk traps are incorrect");
    Relinker::SysVDynamicSectionBuilder builder;
    const auto section = builder.BuildDynamicSection(compacted.References, {}, 0x300, compacted.SlotCount);
    require(section.RelaPltData.size() == 24 && section.DynSymData.size() == 48, "Compacted PLT retained a dead dynamic symbol");
    auto truncated = bytes;
    truncated.resize(70);
    requireFailure([&] { Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, truncated, input.Text, input.TextVaddr); }, "Truncated ELF was accepted");
    auto exceptional = bytes;
    write<std::uint16_t>(exceptional, 56, 3);
    write<std::uint32_t>(exceptional, 176, 0x6474E550);
    write<std::uint64_t>(exceptional, 184, 0x380);
    write<std::uint64_t>(exceptional, 208, 8);
    requireFailure([&] { Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, exceptional, input.Text, input.TextVaddr); }, "Exception metadata was ignored");
}

void relativeTableTargetsBounds() {
    using Relinker::UnusedNidFilter::StrictDataRegion;
    using Relinker::UnusedNidFilter::ReadRelativeTableTargets;

    std::vector<std::uint8_t> tableBytes(16);
    write<std::int32_t>(tableBytes, 0, 0x100);
    write<std::int32_t>(tableBytes, 4, -0x80);
    write<std::int32_t>(tableBytes, 8, 0x500);
    write<std::int32_t>(tableBytes, 12, 0x20);

    const std::vector<StrictDataRegion> data = {
        {0x2000, tableBytes}
    };

    const auto targets = ReadRelativeTableTargets(data, 0x2000, 0x1000, 0x2000);
    require(targets == std::vector<std::uint64_t>{0x2100, 0x1F80, 0x2500, 0x2020},
            "Valid relative table targets were not read");

    const auto bounded = ReadRelativeTableTargets(data, 0x2000, 0x1000, 0x1200);
    require(bounded == std::vector<std::uint64_t>{0x2100, 0x1F80},
            "Relative table reading did not terminate when target fell outside the text section");

    requireFailure([&] {
        ReadRelativeTableTargets(data, 0x2000, 0x1000, 0);
    }, "Zero text size was accepted by relative table reader");

    requireFailure([&] {
        ReadRelativeTableTargets(data, 0x2000, std::numeric_limits<std::uint64_t>::max() - 0x10, 0x100);
    }, "Wrapping text range was accepted by relative table reader");

    std::vector<std::uint8_t> discBytes(4);
    write<std::int32_t>(discBytes, 0, 0x8);
    const std::vector<StrictDataRegion> discData = {
        {std::numeric_limits<std::uint64_t>::max() - 0x20, discBytes}
    };
    requireFailure([&] {
        ReadRelativeTableTargets(discData, std::numeric_limits<std::uint64_t>::max() - 0x20, std::numeric_limits<std::uint64_t>::max() - 0x30, 0x100);
    }, "Wrapping text range was accepted by relative table reader");

    const auto missingBase = ReadRelativeTableTargets(data, 0x3000, 0x1000, 0x2000);
    require(missingBase.empty(), "Table base outside data regions returned targets");

    const auto unalignedEnd = ReadRelativeTableTargets(data, 0x200D, 0x1000, 0x2000);
    require(unalignedEnd.empty(), "Table base with fewer than four bytes remaining returned targets");

    std::vector<std::uint8_t> wrapBytes(8);
    write<std::int32_t>(wrapBytes, 0, 0x1080);
    write<std::int32_t>(wrapBytes, 4, -0x200);
    const std::vector<StrictDataRegion> wrapData = {
        {std::numeric_limits<std::uint64_t>::max() - 0x7F, wrapBytes}
    };
    const auto modularWrap = ReadRelativeTableTargets(wrapData, std::numeric_limits<std::uint64_t>::max() - 0x7F, 0x1000, 0x200);
    require(modularWrap == std::vector<std::uint64_t>{0x1000},
            "Modular two's-complement displacement wrapping across the address boundary was lost");
}

void pltCompactorBounds() {
    const std::vector<std::uint8_t> text(64, 0x90);
    const std::vector<Relinker::NidReference> emptyReferences;

    requireFailure([&] {
        Relinker::UnusedNidFilter::CompactPlt(emptyReferences, emptyReferences, text, std::numeric_limits<std::uint64_t>::max() - 10, 0, 0x300);
    }, "Wrapping text section virtual address range was accepted by PLT compactor");

    requireFailure([&] {
        Relinker::UnusedNidFilter::CompactPlt(emptyReferences, emptyReferences, text, 0x1000, std::numeric_limits<std::uint64_t>::max() - 10, 0x300);
    }, "Overflowing text section file byte offset was accepted by PLT compactor");
}

void relativeRelocationTableBounds() {
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
    write<std::uint32_t>(bytes, 20, 1);
    write<std::uint64_t>(bytes, 32, 64);
    write<std::uint16_t>(bytes, 52, 64);
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

    auto valid = Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(bytes);
    require(valid->TargetOfSlot(0x5000) == 0x6000, "Valid relative relocation was not indexed");
    require(!valid->TargetOfSlot(0x7000).has_value(), "Out of segment relocation was indexed in valid table");

    auto emptyRelocationTable = bytes;
    write<std::uint64_t>(emptyRelocationTable, 0x108, ~std::uint64_t{0});
    write<std::uint64_t>(emptyRelocationTable, 0x118, 0);
    auto emptyIndex = Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(emptyRelocationTable);
    require(!emptyIndex->TargetOfSlot(0x5000).has_value(),
            "Empty relocation table was unexpectedly parsed");

    auto overextended = bytes;
    write<std::uint64_t>(overextended, 0x118, 48);
    requireFailure([&] { Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(overextended); }, "Relocation table extending beyond PT_LOAD was accepted");

    auto badSize = bytes;
    write<std::uint64_t>(badSize, 0x118, 25);
    requireFailure([&] { Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(badSize); }, "Unaligned relocation table size was accepted");

    auto pltOverextended = bytes;
    write<std::int64_t>(pltOverextended, 0x100, 23);
    write<std::uint64_t>(pltOverextended, 0x108, 0x10E8);
    write<std::int64_t>(pltOverextended, 0x110, 2);
    write<std::uint64_t>(pltOverextended, 0x118, 48);
    requireFailure([&] { Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(pltOverextended); }, "PLT relocation table extending beyond PT_LOAD was accepted");

    auto pltBadSize = bytes;
    write<std::int64_t>(pltBadSize, 0x100, 23);
    write<std::uint64_t>(pltBadSize, 0x108, 0x10E8);
    write<std::int64_t>(pltBadSize, 0x110, 2);
    write<std::uint64_t>(pltBadSize, 0x118, 1);
    requireFailure([&] { Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(pltBadSize); }, "Unaligned PLT relocation table size was accepted");

    auto overflowSize = bytes;
    write<std::uint64_t>(overflowSize, 0x118, (~std::uint64_t{0} / 24) * 24);
    requireFailure([&] { Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(overflowSize); }, "Relocation table size near UINT64_MAX was accepted");

    auto overflowRelaVa = bytes;
    write<std::uint64_t>(overflowRelaVa, 0x108, ~std::uint64_t{0} - 15);
    requireFailure([&] { Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(overflowRelaVa); }, "Relocation table vaddr near UINT64_MAX was accepted");

    auto overflowLoadOffset = bytes;
    write<std::uint64_t>(overflowLoadOffset, 72, ~std::uint64_t{0} - 16);
    requireFailure([&] { Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(overflowLoadOffset); }, "PT_LOAD file offset near UINT64_MAX was accepted");

    auto overflowLoadSize = bytes;
    write<std::uint64_t>(overflowLoadSize, 96, ~std::uint64_t{0} - 16);
    write<std::uint64_t>(overflowLoadSize, 0x118, 0x300);
    requireFailure([&] { Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(overflowLoadSize); }, "Relocation table beyond file with fileSize near UINT64_MAX was accepted");

    auto overflowDynamicOffset = bytes;
    write<std::uint64_t>(overflowDynamicOffset, 128, ~std::uint64_t{0} - 16);
    requireFailure([&] { Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(overflowDynamicOffset); }, "PT_DYNAMIC offset near UINT64_MAX was accepted");
}

void sceDynlibDataOffsets() {
    auto input = fixture();
    emit(input, 0, {0xC3});
    importThunk(input, 16, 0x2000);
    auto bytes = elfFixture(input);
    bytes.resize(0xA00);
    write<std::uint16_t>(bytes, 56, 4);
    write<std::uint32_t>(bytes, 176, 2);
    write<std::uint32_t>(bytes, 180, 6);
    write<std::uint64_t>(bytes, 184, 0x300);
    write<std::uint64_t>(bytes, 192, 0x2000);
    write<std::uint64_t>(bytes, 208, 0x40);
    write<std::uint64_t>(bytes, 216, 0x40);
    write<std::uint64_t>(bytes, 224, 8);
    write<std::uint32_t>(bytes, 232, 0x61000000);
    write<std::uint32_t>(bytes, 236, 4);
    write<std::uint64_t>(bytes, 240, 0x500);
    write<std::uint64_t>(bytes, 264, 0x500);
    write<std::uint64_t>(bytes, 272, 0x500);
    write<std::uint64_t>(bytes, 280, 0x10);
    write<std::int64_t>(bytes, 0x300, 0x6100002F);
    write<std::uint64_t>(bytes, 0x308, 0x400);
    write<std::int64_t>(bytes, 0x310, 0x61000031);
    write<std::uint64_t>(bytes, 0x318, 24);
    write<std::int64_t>(bytes, 0x320, 0x61000033);
    write<std::uint64_t>(bytes, 0x328, 24);
    write<std::uint64_t>(bytes, 0x900, 0x2040);
    write<std::uint64_t>(bytes, 0x908, 8);
    write<std::uint64_t>(bytes, 0x910, 0x1010);

    std::string failures;
    try {
        const auto index = Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(bytes);
        if (index->TargetOfSlot(0x2040) != 0x1010) failures += "SCE relative relocation was not indexed; ";
    } catch (const std::exception& error) {
        failures += std::string("SCE relative relocation index: ") + error.what() + "; ";
    }
    try {
        const std::vector<Relinker::NidReference> references = {{"live", {}, 6, 0x900, 0x2000, 0}};
        const auto filtered = Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, bytes, input.Text, input.TextVaddr);
        if (filtered.size() != 1) failures += "SCE relocation target did not keep its import";
    } catch (const std::exception& error) {
        failures += std::string("SCE strict filter: ") + error.what();
    }
    require(failures.empty(), failures.c_str());

    auto noSegment = bytes;
    write<std::uint32_t>(noSegment, 232, 0x6FFFFF01);
    write<std::uint64_t>(noSegment, 0x308, 0x900);
    require(Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(noSegment)->TargetOfSlot(0x2040) == 0x1010,
            "SCE relocation without PT_SCE_DYNLIBDATA was not read at its absolute file offset");
    const std::vector<Relinker::NidReference> references = {{"live", {}, 6, 0x900, 0x2000, 0}};
    require(Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, noSegment, input.Text, input.TextVaddr).size() == 1,
            "Strict filter lost an SCE relocation without PT_SCE_DYNLIBDATA");

    auto jmprel = bytes;
    write<std::uint64_t>(jmprel, 208, 0x60);
    write<std::uint64_t>(jmprel, 216, 0x60);
    write<std::uint64_t>(jmprel, 0x318, 0);
    write<std::int64_t>(jmprel, 0x330, 0x61000029);
    write<std::uint64_t>(jmprel, 0x338, 0x430);
    write<std::int64_t>(jmprel, 0x340, 0x6100002D);
    write<std::uint64_t>(jmprel, 0x348, 24);
    write<std::uint64_t>(jmprel, 0x930, 0x2050);
    write<std::uint64_t>(jmprel, 0x938, 8);
    write<std::uint64_t>(jmprel, 0x940, 0x1010);
    require(Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(jmprel)->TargetOfSlot(0x2050) == 0x1010,
            "SCE JMPREL table was not read at its segment-relative file offset");
    write<std::uint64_t>(jmprel, 0x338, 0x4F0);
    requireFailure([&] { (void)Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(jmprel); },
                   "SCE JMPREL table past the end of the file was accepted");

    auto emptyJmprel = jmprel;
    write<std::uint64_t>(emptyJmprel, 0x338, 0x430);
    write<std::uint64_t>(emptyJmprel, 0x348, 0);
    require(!Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(emptyJmprel)->TargetOfSlot(0x2050),
            "Valid empty SCE JMPREL table was rejected or parsed");
    write<std::uint64_t>(emptyJmprel, 0x338, 0x501);
    requireFailure([&] { (void)Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(emptyJmprel); },
                   "Empty SCE JMPREL table past the file was accepted");
    write<std::uint64_t>(emptyJmprel, 0x338, std::numeric_limits<std::uint64_t>::max());
    requireFailure([&] { (void)Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(emptyJmprel); },
                   "Empty SCE JMPREL table with overflowing offset was accepted");

    auto emptyRela = bytes;
    write<std::uint64_t>(emptyRela, 0x318, 0);
    require(!Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(emptyRela)->TargetOfSlot(0x2040),
            "Valid empty SCE RELA table was rejected or parsed");
    write<std::uint64_t>(emptyRela, 0x308, 0x501);
    requireFailure([&] { (void)Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(emptyRela); },
                   "Empty SCE RELA table past the file was accepted");
    write<std::uint64_t>(emptyRela, 0x308, std::numeric_limits<std::uint64_t>::max());
    requireFailure([&] { (void)Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(emptyRela); },
                   "Empty SCE RELA table with overflowing offset was accepted");

    auto overflow = bytes;
    write<std::uint64_t>(overflow, 0x308, std::numeric_limits<std::uint64_t>::max());
    requireFailure([&] { (void)Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(overflow); },
                   "SCE relocation file offset overflow was accepted");
    requireFailure([&] { (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, overflow, input.Text, input.TextVaddr); },
                   "Strict filter accepted an overflowing SCE table offset");

    auto duplicate = bytes;
    write<std::uint16_t>(duplicate, 56, 5);
    write<std::uint32_t>(duplicate, 288, 0x61000000);
    requireFailure([&] { (void)Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(duplicate); },
                   "Duplicate PT_SCE_DYNLIBDATA segments were accepted");
    requireFailure([&] { (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, duplicate, input.Text, input.TextVaddr); },
                   "Strict filter accepted duplicate PT_SCE_DYNLIBDATA segments");

    auto sectionRelative = bytes;
    write<std::uint64_t>(sectionRelative, 208, 0x70);
    write<std::uint64_t>(sectionRelative, 216, 0x70);
    write<std::int64_t>(sectionRelative, 0x330, 0x61000039);
    write<std::uint64_t>(sectionRelative, 0x338, 0x100);
    write<std::int64_t>(sectionRelative, 0x340, 0x6100003B);
    write<std::uint64_t>(sectionRelative, 0x348, 24);
    write<std::int64_t>(sectionRelative, 0x350, 0x6100003F);
    write<std::uint64_t>(sectionRelative, 0x358, 48);
    write<std::uint8_t>(sectionRelative, 0x61C, 3);
    write<std::uint64_t>(sectionRelative, 0x908, (std::uint64_t{1} << 32) | 8);
    require(Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, sectionRelative, input.Text, input.TextVaddr).size() == 1,
            "Strict filter rejected a RELATIVE relocation against a zero-value section symbol");
    auto valuedSection = sectionRelative;
    write<std::uint64_t>(valuedSection, 0x620, 0x10);
    requireFailure([&] { (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, valuedSection, input.Text, input.TextVaddr); },
                   "Strict filter accepted a RELATIVE relocation against a section symbol with a value");
    auto untyped = sectionRelative;
    write<std::uint8_t>(untyped, 0x61C, 0);
    requireFailure([&] { (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, untyped, input.Text, input.TextVaddr); },
                   "Strict filter accepted a RELATIVE relocation against a non-section symbol");
    auto outside = sectionRelative;
    write<std::uint64_t>(outside, 0x908, (std::uint64_t{2} << 32) | 8);
    requireFailure([&] { (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, outside, input.Text, input.TextVaddr); },
                   "Strict filter accepted a RELATIVE relocation symbol outside the symbol table");
}

void duplicatePointerRelocations() {
    auto input = fixture();
    emit(input, 0, {0xC3});
    importThunk(input, 16, 0x2000);
    auto bytes = elfFixture(input);
    bytes.resize(0xA00);
    write<std::uint16_t>(bytes, 56, 4);
    write<std::uint32_t>(bytes, 176, 2);
    write<std::uint32_t>(bytes, 180, 6);
    write<std::uint64_t>(bytes, 184, 0x300);
    write<std::uint64_t>(bytes, 192, 0x2000);
    write<std::uint64_t>(bytes, 208, 0x40);
    write<std::uint64_t>(bytes, 216, 0x40);
    write<std::uint64_t>(bytes, 224, 8);
    write<std::uint32_t>(bytes, 232, 0x61000000);
    write<std::uint32_t>(bytes, 236, 4);
    write<std::uint64_t>(bytes, 240, 0x500);
    write<std::uint64_t>(bytes, 264, 0x500);
    write<std::uint64_t>(bytes, 272, 0x500);
    write<std::uint64_t>(bytes, 280, 0x10);
    write<std::int64_t>(bytes, 0x300, 0x6100002F);
    write<std::uint64_t>(bytes, 0x308, 0x400);
    write<std::int64_t>(bytes, 0x310, 0x61000031);
    write<std::uint64_t>(bytes, 0x318, 48);
    write<std::int64_t>(bytes, 0x320, 0x61000033);
    write<std::uint64_t>(bytes, 0x328, 24);
    write<std::uint64_t>(bytes, 0x900, 0x2040);
    write<std::uint64_t>(bytes, 0x908, 8);
    write<std::uint64_t>(bytes, 0x910, 0x1010);
    write<std::uint64_t>(bytes, 0x918, 0x2040);
    write<std::uint64_t>(bytes, 0x920, 8);
    write<std::uint64_t>(bytes, 0x928, 0x1010);
    const std::vector<Relinker::NidReference> references = {{"live", {}, 6, 0x900, 0x2000, 0}};
    require(Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, bytes, input.Text, input.TextVaddr).size() == 1,
            "Strict filter rejected an identical duplicate pointer relocation");
    write<std::uint64_t>(bytes, 0x928, 0x1020);
    requireFailure([&] { (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, bytes, input.Text, input.TextVaddr); },
                   "Strict filter accepted a conflicting duplicate pointer relocation");
    write<std::uint64_t>(bytes, 0x928, 0x1010);
    write<std::uint64_t>(bytes, 0x918, 0x2000);
    requireFailure([&] { (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, bytes, input.Text, input.TextVaddr); },
                   "Strict filter accepted a pointer relocation overlapping an import slot");
}

void constructorArrayBounds() {
    auto input = fixture();
    emit(input, 0, {0xC3});
    auto bytes = elfFixture(input);
    bytes.resize(0xA00);
    write<std::uint16_t>(bytes, 56, 4);
    write<std::uint32_t>(bytes, 176, 2);
    write<std::uint32_t>(bytes, 180, 6);
    write<std::uint64_t>(bytes, 184, 0x300);
    write<std::uint64_t>(bytes, 192, 0x2000);
    write<std::uint64_t>(bytes, 208, 0x80);
    write<std::uint64_t>(bytes, 216, 0x80);
    write<std::uint64_t>(bytes, 224, 8);
    write<std::uint32_t>(bytes, 232, 0x61000000);
    write<std::uint32_t>(bytes, 236, 4);
    write<std::uint64_t>(bytes, 240, 0x500);
    write<std::uint64_t>(bytes, 264, 0x500);
    write<std::uint64_t>(bytes, 272, 0x500);
    write<std::uint64_t>(bytes, 280, 0x10);
    write<std::int64_t>(bytes, 0x300, 25);
    write<std::uint64_t>(bytes, 0x308, 0x2060);
    write<std::int64_t>(bytes, 0x310, 27);
    write<std::uint64_t>(bytes, 0x318, 8);
    write<std::uint64_t>(bytes, 0x360, 0x1000);
    const std::vector<Relinker::NidReference> references = {{"live", {}, 6, 0x2040, 0x2000, 0}};
    require(Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, bytes, input.Text, input.TextVaddr).empty(),
            "Strict filter retained uncalled import");

    auto outsideCode = bytes;
    write<std::uint64_t>(outsideCode, 0x360, 0x5000);
    requireFailure([&] { (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, outsideCode, input.Text, input.TextVaddr); },
                   "Strict filter accepted constructor entry outside code");

    auto missingSize = outsideCode;
    write<std::int64_t>(missingSize, 0x310, 0);
    write<std::uint64_t>(missingSize, 0x318, 0);
    require(Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, missingSize, input.Text, input.TextVaddr).empty(),
            "Strict filter inspected constructor array when size was omitted");

    auto missingArray = bytes;
    write<std::int64_t>(missingArray, 0x300, 0x6100003F);
    write<std::uint64_t>(missingArray, 0x308, 0);
    try {
        (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, missingArray, input.Text, input.TextVaddr);
        require(false, "Strict filter accepted constructor size without an array pointer");
    } catch (const Relinker::RelinkerException& error) {
        require(std::string(error.what()).find("incomplete constructor array metadata") != std::string::npos,
                "Constructor size without array was rejected for an unexpected reason");
    }

    auto invalidSize = bytes;
    write<std::uint64_t>(invalidSize, 0x318, 12);
    try {
        (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, invalidSize, input.Text, input.TextVaddr);
        require(false, "Strict filter accepted constructor size not divisible by 8");
    } catch (const Relinker::RelinkerException& error) {
        require(std::string(error.what()).find("invalid constructor array size") != std::string::npos,
                "Invalid constructor array size was rejected for an unexpected reason");
        require(error.FailureOffset == 0x2060, "Invalid constructor array size failure offset was incorrect");
    }

    auto overflow = bytes;
    write<std::int64_t>(overflow, 0x300, 0x6100002F);
    write<std::uint64_t>(overflow, 0x308, 0x400);
    write<std::int64_t>(overflow, 0x310, 0x61000031);
    write<std::uint64_t>(overflow, 0x318, 48);
    write<std::int64_t>(overflow, 0x320, 0x61000033);
    write<std::uint64_t>(overflow, 0x328, 24);
    write<std::int64_t>(overflow, 0x330, 25);
    write<std::uint64_t>(overflow, 0x338, std::numeric_limits<std::uint64_t>::max() - 7);
    write<std::int64_t>(overflow, 0x340, 27);
    write<std::uint64_t>(overflow, 0x348, 16);
    write<std::uint64_t>(overflow, 0x900, std::numeric_limits<std::uint64_t>::max() - 7);
    write<std::uint64_t>(overflow, 0x908, 8);
    write<std::uint64_t>(overflow, 0x910, 0x1000);
    write<std::uint64_t>(overflow, 0x918, 0);
    write<std::uint64_t>(overflow, 0x920, 8);
    write<std::uint64_t>(overflow, 0x928, 0x1000);
    try {
        (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, overflow, input.Text, input.TextVaddr);
        require(false, "Strict filter accepted an overflowing constructor array address");
    } catch (const Relinker::RelinkerException& error) {
        require(std::string(error.what()).find("constructor array address overflow") != std::string::npos,
                "Constructor array address overflow was rejected for an unexpected reason");
        require(error.FailureOffset == std::numeric_limits<std::uint64_t>::max() - 7,
                "Constructor array address overflow failure offset was incorrect");
    }

    auto finiOverflow = overflow;
    write<std::int64_t>(finiOverflow, 0x330, 26);
    write<std::int64_t>(finiOverflow, 0x340, 28);
    try {
        (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, finiOverflow, input.Text, input.TextVaddr);
        require(false, "Strict filter accepted an overflowing destructor array address");
    } catch (const Relinker::RelinkerException& error) {
        require(std::string(error.what()).find("constructor array address overflow") != std::string::npos,
                "Destructor array address overflow was rejected for an unexpected reason");
        require(error.FailureOffset == std::numeric_limits<std::uint64_t>::max() - 7,
                "Destructor array address overflow failure offset was incorrect");
    }

    auto preinitOverflow = overflow;
    write<std::int64_t>(preinitOverflow, 0x330, 32);
    write<std::int64_t>(preinitOverflow, 0x340, 33);
    try {
        (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, preinitOverflow, input.Text, input.TextVaddr);
        require(false, "Strict filter accepted an overflowing preinit array address");
    } catch (const Relinker::RelinkerException& error) {
        require(std::string(error.what()).find("constructor array address overflow") != std::string::npos,
                "Preinit array address overflow was rejected for an unexpected reason");
        require(error.FailureOffset == std::numeric_limits<std::uint64_t>::max() - 7,
                "Preinit array address overflow failure offset was incorrect");
    }

    auto sceOverflow = overflow;
    write<std::int64_t>(sceOverflow, 0x330, 0x60000019);
    write<std::int64_t>(sceOverflow, 0x340, 0x6000001B);
    try {
        (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, sceOverflow, input.Text, input.TextVaddr);
        require(false, "Strict filter accepted an overflowing SCE constructor array address");
    } catch (const Relinker::RelinkerException& error) {
        require(std::string(error.what()).find("constructor array address overflow") != std::string::npos,
                "SCE constructor array address overflow was rejected for an unexpected reason");
        require(error.FailureOffset == std::numeric_limits<std::uint64_t>::max() - 7,
                "SCE constructor array address overflow failure offset was incorrect");
    }
}

void negativeRelativeRelocationTarget() {
    auto input = fixture();
    emit(input, 0, {0xC3});
    importThunk(input, 16, 0x2000);
    auto bytes = elfFixture(input);
    bytes.resize(0xA00);
    write<std::uint16_t>(bytes, 56, 4);
    write<std::uint32_t>(bytes, 176, 2);
    write<std::uint32_t>(bytes, 180, 6);
    write<std::uint64_t>(bytes, 184, 0x300);
    write<std::uint64_t>(bytes, 192, 0x2000);
    write<std::uint64_t>(bytes, 208, 0x40);
    write<std::uint64_t>(bytes, 216, 0x40);
    write<std::uint64_t>(bytes, 224, 8);
    write<std::uint32_t>(bytes, 232, 0x61000000);
    write<std::uint32_t>(bytes, 236, 4);
    write<std::uint64_t>(bytes, 240, 0x500);
    write<std::uint64_t>(bytes, 264, 0x500);
    write<std::uint64_t>(bytes, 272, 0x500);
    write<std::uint64_t>(bytes, 280, 0x10);
    write<std::int64_t>(bytes, 0x300, 0x6100002F);
    write<std::uint64_t>(bytes, 0x308, 0x400);
    write<std::int64_t>(bytes, 0x310, 0x61000031);
    write<std::uint64_t>(bytes, 0x318, 24);
    write<std::int64_t>(bytes, 0x320, 0x61000033);
    write<std::uint64_t>(bytes, 0x328, 24);
    write<std::uint64_t>(bytes, 0x900, 0x2040);
    write<std::uint64_t>(bytes, 0x908, 8);
    write<std::uint64_t>(bytes, 0x910, static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1);
    const std::vector<Relinker::NidReference> references = {{"live", {}, 6, 0x900, 0x2000, 0}};
    try {
        (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, bytes, input.Text, input.TextVaddr);
        require(false, "Strict filter accepted a negative relative relocation target address");
    } catch (const Relinker::RelinkerException& error) {
        require(std::string(error.what()).find("relative relocation target address is negative") != std::string::npos,
                "Unexpected error message for negative relative relocation target");
        require(error.FailureOffset == 0x2040, "Unexpected failure offset for negative relative relocation target");
    }
    write<std::uint64_t>(bytes, 0x910, std::numeric_limits<std::uint64_t>::max());
    requireFailure([&] { (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, bytes, input.Text, input.TextVaddr); },
                   "Strict filter accepted UINT64_MAX as relative relocation target address");
    write<std::uint64_t>(bytes, 0x910, static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()));
    (void)Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, bytes, input.Text, input.TextVaddr);
    write<std::uint64_t>(bytes, 0x910, 0x1010);
    require(Relinker::MakeStrictUnusedNidFilter(testProgress)->Filter(references, bytes, input.Text, input.TextVaddr).size() == 1,
            "Strict filter rejected a valid relative relocation target address");
}

}

int main() {
    try {
        deadTailAndExplicitEntry();
        ud1DeadTail();
        conditionalTailCall();
        fallthroughAfterZeroPadding();
        addressSizeOverrideImport();
        callbackAndRelocationRoots();
        registerImportCall();
        mergedJumpTable();
        conservativeIndirectTargets();
        isolatedFunctionCycle();
        relativeTableAndWholeFunction();
        indexedTableImports();
        exceptionLeb128Bounds();
        vectorInstructionLengths();
        exceptionLandingPads();
        exceptionDataWithoutPersonality();
        filterCallbackDataImports();
        filterAndPltCompaction();
        relativeTableTargetsBounds();
        pltCompactorBounds();
        relativeRelocationTableBounds();
        sceDynlibDataOffsets();
        duplicatePointerRelocations();
        constructorArrayBounds();
        negativeRelativeRelocationTarget();
        std::cout << "Strict NID filter tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
