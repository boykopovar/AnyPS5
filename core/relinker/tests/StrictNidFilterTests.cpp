#include <relinker/analysis/UnusedNidFilter.hpp>
#include <relinker/analysis/UnusedNidFilter/StrictReachability.hpp>
#include <relinker/output/SysVDynamicSectionBuilder.hpp>
#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <relinker/analysis/UnusedNidFilter/PltCompactor.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>

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

void conditionalTailCall() {
    auto input = fixture();
    emit(input, 0, {0x75, 0x0E, 0xC3});
    importThunk(input, 16, 0x2000);
    require(AnalyzeStrictReachability(input).ImportSlots.contains(0x2000), "Conditional tail call was removed");
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

void vectorInstructionLengths() {
    const Codegen::X64InstructionDecoder decoder;
    const std::vector<std::vector<std::uint8_t>> instructions = {
        {0xC5, 0xD9, 0x73, 0xD4, 0x20},
        {0xC4, 0xE1, 0x79, 0x70, 0xC0, 0x1B},
        {0xC4, 0xE1, 0x78, 0x77},
        {0x62, 0xF1, 0x7D, 0x48, 0x72, 0xD0, 0x04}
    };
    for (const auto& instruction : instructions)
        require(decoder.Decode(instruction.data(), instruction.size()) == instruction.size(), "Vector immediate or VZEROUPPER was decoded with the wrong length");
}

void exceptionLandingPads() {
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
    require(input.Functions.size() == 1 && input.Functions[0].ExtraTargets == std::vector<std::uint64_t>{0x1040}, "LSDA landing pad outside the function was not recovered");
    emit(input, 0, {0x0F, 0x0B});
    emit(input, 31, {0xC3});
    importThunk(input, 64, 0x2000);
    require(AnalyzeStrictReachability(input).ImportSlots.contains(0x2000), "Exception-only import was removed");
    write<std::uint8_t>(bytes, 0x282, 0xFF);
    requireFailure([&] { Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {}); }, "Malformed LSDA was accepted");
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

void filterAndPltCompaction() {
    auto input = fixture();
    importThunk(input, 0, 0x2000);
    importThunk(input, 16, 0x2008);
    const auto bytes = elfFixture(input);
    const std::vector<Relinker::NidReference> references = {{"live", {}, 7, 0x300, 0x2000, 0}, {"dead", {}, 7, 0x318, 0x2008, 0}};
    const auto filtered = Relinker::MakeStrictUnusedNidFilter()->Filter(references, bytes, input.Text, input.TextVaddr);
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
    requireFailure([&] { Relinker::MakeStrictUnusedNidFilter()->Filter(references, truncated, input.Text, input.TextVaddr); }, "Truncated ELF was accepted");
    auto exceptional = bytes;
    write<std::uint16_t>(exceptional, 56, 3);
    write<std::uint32_t>(exceptional, 176, 0x6474E550);
    write<std::uint64_t>(exceptional, 184, 0x380);
    write<std::uint64_t>(exceptional, 208, 8);
    requireFailure([&] { Relinker::MakeStrictUnusedNidFilter()->Filter(references, exceptional, input.Text, input.TextVaddr); }, "Exception metadata was ignored");
}

}

int main() {
    try {
        deadTailAndExplicitEntry();
        conditionalTailCall();
        callbackAndRelocationRoots();
        registerImportCall();
        mergedJumpTable();
        conservativeIndirectTargets();
        isolatedFunctionCycle();
        relativeTableAndWholeFunction();
        vectorInstructionLengths();
        exceptionLandingPads();
        filterAndPltCompaction();
        std::cout << "Strict NID filter tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
