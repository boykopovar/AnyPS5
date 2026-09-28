#include <codegen/IAmd64OnlyConverter.hpp>
#include <codegen/CodegenException.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/x86/Sse4aLowering.hpp>
#include <codegen/x86/Sse4aOperands.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <elfpatcher/general/ProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/general/SectionHeaderTableBuilder.hpp>
#include <elfpatcher/general/SegmentFilter.hpp>
#include <elfpatcher/linux/LinuxElfPatcher.hpp>
#include <io/ByteWriter.hpp>
#include <cstring>
#include <cstdint>
#ifdef __linux__
#include <sys/mman.h>
#endif
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using Bytes = std::vector<std::uint8_t>;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void requireFailure(const std::function<void()>& operation, const char* message) {
    try {
        operation();
    } catch (const Codegen::CodegenException&) {
        return;
    } catch (const Domain::RelinkerException&) {
        return;
    }
    throw std::runtime_error(message);
}

Domain::FileByteOffset failureOffset(const std::function<void()>& operation, const char* message) {
    try {
        operation();
    } catch (const Codegen::CodegenException& error) {
        return error.FailureOffset;
    }
    throw std::runtime_error(message);
}

template<typename TValue>
void write(Bytes& bytes, std::size_t offset, TValue value) {
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) throw std::runtime_error("Test fixture write is out of bounds");
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

template<typename TValue>
TValue read(const Bytes& bytes, std::size_t offset) {
    TValue value;
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) throw std::runtime_error("Test fixture read is out of bounds");
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

Bytes withReturn(Bytes body, const std::size_t returnBranchOffset) {
    body.at(returnBranchOffset) = 0xE9;
    for (std::size_t index = 1; index <= 4; ++index) body.at(returnBranchOffset + index) = 0;
    return body;
}

const Bytes kExtrqSite = {0x66, 0x0F, 0x78, 0xC3, 0x08, 0x28};
const Bytes kInsertqSelfSite = {0xF2, 0x0F, 0x78, 0xDB, 0x08, 0x08};
const Bytes kInsertqCrossSite = {0xF2, 0x0F, 0x78, 0xC8, 0x08, 0x00};
const Bytes kInsertqHighSite = {0xF2, 0x44, 0x0F, 0x78, 0xCC, 0x10, 0x10};
const Bytes kInsertqWordSite = {0xF2, 0x0F, 0x78, 0xDC, 0x10, 0x10};

const Bytes kExtrqBody = {
    0x66, 0x0F, 0x38, 0x00, 0x1D, 0x07, 0x00, 0x00, 0x00, 0xE9, 0x00, 0x00, 0x00, 0x00, 0xCC, 0xCC,
    0x05, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
const Bytes kInsertqSelfBody = {
    0x66, 0x0F, 0x38, 0x00, 0x1D, 0x07, 0x00, 0x00, 0x00, 0xE9, 0x00, 0x00, 0x00, 0x00, 0xCC, 0xCC,
    0x00, 0x00, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
const Bytes kInsertqCrossBody = {
    0x66, 0x0F, 0x6C, 0xC8, 0x66, 0x0F, 0x38, 0x00, 0x0D, 0x13, 0x00, 0x00, 0x00, 0xE9, 0x00, 0x00,
    0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
    0x08, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
const Bytes kInsertqHighBody = {
    0x66, 0x44, 0x0F, 0x6C, 0xCC, 0x66, 0x44, 0x0F, 0x38, 0x00, 0x0D, 0x11, 0x00, 0x00, 0x00, 0xE9,
    0x00, 0x00, 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
    0x00, 0x01, 0x08, 0x09, 0x04, 0x05, 0x06, 0x07, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
const Bytes kInsertqWordBody = {
    0x66, 0x0F, 0x6C, 0xDC, 0x66, 0x0F, 0x38, 0x00, 0x1D, 0x13, 0x00, 0x00, 0x00, 0xE9, 0x00, 0x00,
    0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
    0x00, 0x01, 0x08, 0x09, 0x04, 0x05, 0x06, 0x07, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};

void decoderLengths() {
    const Codegen::X64InstructionDecoder decoder;
    const std::vector<Bytes> instructions = {
        kExtrqSite, kInsertqSelfSite, kInsertqCrossSite, kInsertqHighSite, kInsertqWordSite,
        {0x66, 0x0F, 0x79, 0xCA}, {0xF2, 0x0F, 0x79, 0xCA}, {0x66, 0x45, 0x0F, 0x79, 0xCA},
        {0xF3, 0x0F, 0xB8, 0xC0}, {0xCD, 0x41}, {0x0F, 0x0D, 0x08}, {0x0F, 0xC0, 0xC1}, {0x0F, 0xC3, 0x07},
        {0x66, 0x0F, 0xC4, 0xC0, 0x01}, {0xC2, 0x08, 0x00}, {0xC8, 0x10, 0x00, 0x00}, {0xF3, 0x0F, 0x2B, 0x07},
        {0xF2, 0x44, 0x0F, 0x2B, 0x4C, 0x24, 0x10}, {0x0F, 0x01, 0xFA}, {0x0F, 0xB9, 0x00},
        {0x41, 0x0F, 0xBB, 0xF7}, {0x0F, 0xBB, 0x47, 0x08}};
    Bytes padded;
    for (const auto& instruction : instructions) {
        padded = instruction;
        padded.insert(padded.end(), 8, 0x90);
        require(decoder.Decode(padded.data(), padded.size()) == instruction.size(), "AMD-only or repaired two-byte opcode was decoded with the wrong length");
    }
    requireFailure([&] { const Bytes bare = {0x0F, 0x78, 0xC3, 0x08, 0x28}; (void)decoder.Decode(bare.data(), bare.size()); }, "0F 78 without an SSE4a prefix was accepted");
}

void sse4aOperands() {
    const auto check = [](const Bytes& site, const bool insertq, const int dst, const int src, const int length, const int index) {
        const auto operands = Codegen::DecodeSse4a(site.data(), site.size());
        require(operands.Insertq == insertq && !operands.RegisterForm && operands.Destination == dst && operands.Source == src && operands.Length == length && operands.Index == index, "SSE4a operands were decoded incorrectly");
    };
    check(kExtrqSite, false, 3, 3, 8, 40);
    check(kInsertqSelfSite, true, 3, 3, 8, 8);
    check(kInsertqCrossSite, true, 1, 0, 8, 0);
    check(kInsertqHighSite, true, 9, 4, 16, 16);
    check(kInsertqWordSite, true, 3, 4, 16, 16);
    const Bytes fullField = {0xF2, 0x0F, 0x78, 0xC8, 0x00, 0x00};
    require(Codegen::DecodeSse4a(fullField.data(), fullField.size()).Length == 64, "Zero length does not mean 64");
    const Bytes registerForm = {0x66, 0x45, 0x0F, 0x79, 0xCA};
    const auto decoded = Codegen::DecodeSse4a(registerForm.data(), registerForm.size());
    require(decoded.RegisterForm && !decoded.Insertq && decoded.Destination == 9 && decoded.Source == 10, "Register form operands were decoded incorrectly");
    requireFailure([] { const Bytes bytes = {0x66, 0x0F, 0x78, 0xCB, 0x08, 0x28}; (void)Codegen::DecodeSse4a(bytes.data(), bytes.size()); }, "EXTRQ with a non-zero reg field was accepted");
    requireFailure([] { const Bytes bytes = {0xF2, 0x0F, 0x78, 0x1B, 0x08, 0x08}; (void)Codegen::DecodeSse4a(bytes.data(), bytes.size()); }, "SSE4a memory operand was accepted");
    requireFailure([] { const Bytes bytes = {0xF2, 0x0F, 0x78, 0xC8, 0x20, 0x30}; (void)Codegen::DecodeSse4a(bytes.data(), bytes.size()); }, "Field beyond bit 64 was accepted");
}

void matcherSubstitutions() {
    const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto match = [&](const Bytes& bytes) { return matcher->Match(bytes.data(), bytes.size()); };
    const auto movntss = match({0xF3, 0x0F, 0x2B, 0x07});
    require(movntss && movntss->Lowering == Codegen::Amd64OnlyLowering::InPlace && movntss->ReplacementBytes == Bytes{0xF3, 0x0F, 0x11, 0x07} && movntss->InstructionName == "MOVNTSS", "MOVNTSS was not rewritten to MOVSS");
    const auto movntsd = match({0xF2, 0x44, 0x0F, 0x2B, 0x4C, 0x24, 0x10});
    require(movntsd && movntsd->Lowering == Codegen::Amd64OnlyLowering::InPlace && movntsd->ReplacementBytes == Bytes{0xF2, 0x44, 0x0F, 0x11, 0x4C, 0x24, 0x10} && movntsd->InstructionName == "MOVNTSD", "MOVNTSD was not rewritten to MOVSD");
    requireFailure([&] { (void)match({0xF3, 0x0F, 0x2B, 0xC1}); }, "MOVNTSS with a register operand was accepted");
    const auto monitorx = match({0x0F, 0x01, 0xFA});
    require(monitorx && monitorx->Lowering == Codegen::Amd64OnlyLowering::Unsupported && monitorx->InstructionName == "MONITORX", "MONITORX was not reported as unsupported");
    const auto registerForm = match({0x66, 0x0F, 0x79, 0xCA});
    require(registerForm && registerForm->Lowering == Codegen::Amd64OnlyLowering::Trampoline && registerForm->InstructionName == "EXTRQ register form", "EXTRQ register form was not lowered through a stub");
    const auto insertqRegisterForm = match({0xF2, 0x0F, 0x79, 0xCA});
    require(insertqRegisterForm && insertqRegisterForm->Lowering == Codegen::Amd64OnlyLowering::Unsupported, "INSERTQ register form was not reported as unsupported");
    require(!match({0x66, 0x0F, 0x2B, 0x07}) && !match({0x0F, 0x2B, 0x07}) && !match({0x48, 0x8B, 0x05, 0, 0, 0, 0}), "Ordinary instruction was matched");
    const auto stub = match(kInsertqHighSite);
    require(stub && stub->Lowering == Codegen::Amd64OnlyLowering::Trampoline && stub->StubBody == kInsertqHighBody && stub->ReturnBranchOffset == 15 && stub->InstructionName == "INSERTQ", "INSERTQ was not lowered through a stub");
    const auto shiftInPlace = match({0x66, 0x0F, 0x78, 0xC3, 0x18, 0x28});
    require(shiftInPlace && shiftInPlace->Lowering == Codegen::Amd64OnlyLowering::InPlace && shiftInPlace->ReplacementBytes == Bytes{0x66, 0x0F, 0x73, 0xD3, 0x28, 0x90}, "Top-aligned EXTRQ was not lowered in place");
}

void goldenBodies() {
    const Codegen::Sse4aLowering lowering;
    const auto outOfLine = [&](const Bytes& site, const Bytes& expected, const std::size_t returnBranchOffset) {
        const auto operands = Codegen::DecodeSse4a(site.data(), site.size());
        require(!lowering.LowerInPlace(operands, site.size()).has_value(), "Demon's Souls site unexpectedly qualified for an in-place lowering");
        const auto body = lowering.LowerOutOfLine(operands);
        require(body.ReturnBranchOffset == returnBranchOffset, "Stub return branch is at the wrong offset");
        require(body.Bytes == expected, "Stub body differs from the golden encoding");
    };
    outOfLine(kExtrqSite, kExtrqBody, 9);
    outOfLine(kInsertqSelfSite, kInsertqSelfBody, 9);
    outOfLine(kInsertqCrossSite, kInsertqCrossBody, 13);
    outOfLine(kInsertqHighSite, kInsertqHighBody, 15);
    outOfLine(kInsertqWordSite, kInsertqWordBody, 13);
    const auto inPlace = [&](const Bytes& site, const Bytes& expected) {
        const auto operands = Codegen::DecodeSse4a(site.data(), site.size());
        const auto sequence = lowering.LowerInPlace(operands, site.size());
        require(sequence.has_value() && *sequence == expected, "In-place lowering differs from the golden encoding");
    };
    inPlace({0xF2, 0x0F, 0x78, 0xC8, 0x00, 0x00}, {0xF3, 0x0F, 0x7E, 0xC8, 0x66, 0x90});
    inPlace({0x66, 0x0F, 0x78, 0xC3, 0x18, 0x28}, {0x66, 0x0F, 0x73, 0xD3, 0x28, 0x90});
    inPlace({0x66, 0x0F, 0x78, 0xC3, 0x08, 0x00}, {0x66, 0x0F, 0x38, 0x32, 0xDB, 0x90});
    inPlace({0xF2, 0x0F, 0x78, 0xDB, 0x08, 0x00}, {0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00});
    inPlace({0xF2, 0x0F, 0x78, 0xC8, 0x20, 0x00}, {0x66, 0x0F, 0x3A, 0x0E, 0xC8, 0x03});
    inPlace({0xF2, 0x45, 0x0F, 0x78, 0xC8, 0x10, 0x00}, {0x66, 0x45, 0x0F, 0x3A, 0x0E, 0xC8, 0x01});
    const auto highRegisters = Codegen::DecodeSse4a(kInsertqHighSite.data(), kInsertqHighSite.size());
    const auto generic = lowering.LowerOutOfLine(Codegen::Sse4aOperands{true, false, 9, 4, 5, 3});
    require(generic.Bytes[0] == 0x48 && generic.Bytes.size() % 16 == 0 && generic.ReturnBranchOffset < generic.Bytes.size(), "Generic INSERTQ body does not start with the red-zone skip");
    (void)highRegisters;
    requireFailure([&] { (void)lowering.LowerOutOfLine(Codegen::Sse4aOperands{true, true, 1, 2, 0, 0}); }, "Register form was lowered out of line");
}

Bytes segmentFixture() {
    Bytes file(0x300, 0xCC);
    const Bytes text = {
        0xF3, 0x0F, 0xB8, 0xC0,
        0xCD, 0x41,
        0xEB, 0x07,
        0xF2, 0x44, 0x0F, 0x78, 0xCC, 0x10, 0x10,
        0xF3, 0x0F, 0x2B, 0x07,
        0xC3};
    std::copy(text.begin(), text.end(), file.begin() + 0x200);
    return file;
}

Domain::ProgramHeader segmentHeader(const std::uint64_t size) {
    return {1, 5, 0x200, 0x1000, 0, size, size, 16};
}

void converterSegment() {
    const auto converter = Codegen::MakeAmd64OnlyConverter();
    const auto file = segmentFixture();
    const auto result = converter->Convert(file, {segmentHeader(20)});
    require(result.ReplacedCount == 1 && result.Reports.size() == 2 && result.Trampolines.size() == 1, "Converter did not classify the segment's AMD-only instructions");
    const auto& site = result.Trampolines[0];
    require(site.Offset == 0x208 && site.Address == 0x1008 && site.Length == 7 && site.OriginalBytes == kInsertqHighSite && site.Body == kInsertqHighBody && site.ReturnBranchOffset == 15, "Trampoline site was recorded incorrectly");
    require(result.Reports[0].InstructionName == "INSERTQ" && result.Reports[0].Offset == 0x208 && result.Reports[0].Lowering == Codegen::Amd64OnlyLowering::Trampoline && result.Reports[0].ReplacementLength == 48, "Trampoline report is wrong");
    require(result.Reports[1].InstructionName == "MOVNTSS" && result.Reports[1].Offset == 0x20F && result.Reports[1].Lowering == Codegen::Amd64OnlyLowering::InPlace && result.Reports[1].ReplacementLength == 4, "In-place report is wrong");
    auto expected = file;
    expected[0x211] = 0x11;
    require(result.Bytes == expected, "Converter changed bytes other than the MOVNTSS opcode");
    const auto untouched = converter->Convert(Bytes(0x300, 0x90), {segmentHeader(0x100)});
    require(untouched.ReplacedCount == 0 && untouched.Trampolines.empty() && untouched.Reports.empty() && untouched.Bytes == Bytes(0x300, 0x90), "Segment without AMD-only instructions was changed");
    auto branchInside = file;
    branchInside[0x207] = 0x02;
    requireFailure([&] { (void)converter->Convert(branchInside, {segmentHeader(20)}); }, "Branch into an AMD-only instruction was accepted");
    auto monitorx = file;
    monitorx[0x20F] = 0x0F;
    monitorx[0x210] = 0x01;
    monitorx[0x211] = 0xFA;
    monitorx[0x212] = 0x90;
    requireFailure([&] { (void)converter->Convert(monitorx, {segmentHeader(20)}); }, "MONITORX was silently kept");
    auto registerForm = file;
    const Bytes extrqRegister = {0x66, 0x0F, 0x79, 0xCA};
    std::copy(extrqRegister.begin(), extrqRegister.end(), registerForm.begin() + 0x20F);
    requireFailure([&] { (void)converter->Convert(registerForm, {segmentHeader(20)}); }, "Short EXTRQ followed by a return was relocated");
    registerForm[0x213] = 0x90;
    const auto relocated = converter->Convert(registerForm, {segmentHeader(20)});
    require(relocated.Trampolines.size() == 2, "Short EXTRQ register form was not lowered through a stub");
    const auto& shortSite = relocated.Trampolines[1];
    const Bytes shortOriginal = {0x66, 0x0F, 0x79, 0xCA, 0x90};
    require(shortSite.Offset == 0x20F && shortSite.Length == 5 && shortSite.OriginalBytes == shortOriginal, "Short EXTRQ site did not absorb the following instruction");
    require(shortSite.Body[shortSite.ReturnBranchOffset - 1] == 0x90 && shortSite.Body[shortSite.ReturnBranchOffset] == 0xE9, "Absorbed instruction does not run before the return jump");
    requireFailure([&] { (void)converter->Convert(file, {segmentHeader(0x200)}); }, "Segment exceeding the file was accepted");
}

void converterFailureOffsets() {
    const auto converter = Codegen::MakeAmd64OnlyConverter();
    const auto file = segmentFixture();
    auto undecodable = file;
    const Bytes bareSse4a = {0x0F, 0x78, 0xC0, 0x00};
    std::copy(bareSse4a.begin(), bareSse4a.end(), undecodable.begin() + 0x20F);
    require(failureOffset([&] { (void)converter->Convert(undecodable, {segmentHeader(20)}); }, "Undecodable instruction was accepted") == 0x20F, "Decoder failure does not carry the file offset");
    auto memoryForm = file;
    memoryForm[0x20C] = 0x08;
    require(failureOffset([&] { (void)converter->Convert(memoryForm, {segmentHeader(20)}); }, "INSERTQ memory form was accepted") == 0x208, "SSE4a operand failure does not carry the file offset");
    auto movntsRegister = file;
    movntsRegister[0x212] = 0xC1;
    require(failureOffset([&] { (void)converter->Convert(movntsRegister, {segmentHeader(20)}); }, "MOVNTSS register form was accepted") == 0x20F, "MOVNTSS failure does not carry the file offset");
    auto monitorx = file;
    const Bytes monitorxBytes = {0x0F, 0x01, 0xFA, 0x90};
    std::copy(monitorxBytes.begin(), monitorxBytes.end(), monitorx.begin() + 0x20F);
    require(failureOffset([&] { (void)converter->Convert(monitorx, {segmentHeader(20)}); }, "MONITORX was accepted") == 0x20F, "Unsupported instruction failure does not carry the file offset");
}

Bytes elfFixture(const Bytes& text) {
    Bytes bytes(0x400);
    bytes[0] = 0x7F;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 2;
    bytes[5] = 1;
    bytes[6] = 1;
    write<std::uint16_t>(bytes, 16, 3);
    write<std::uint16_t>(bytes, 18, 62);
    write<std::uint64_t>(bytes, 24, 0x1000);
    write<std::uint64_t>(bytes, 32, 64);
    write<std::uint16_t>(bytes, 54, 56);
    write<std::uint16_t>(bytes, 56, 6);
    write<std::uint32_t>(bytes, 64, 1);
    write<std::uint32_t>(bytes, 68, 5);
    write<std::uint64_t>(bytes, 72, 0x200);
    write<std::uint64_t>(bytes, 80, 0x1000);
    write<std::uint64_t>(bytes, 96, 0x100);
    write<std::uint64_t>(bytes, 104, 0x100);
    write<std::uint64_t>(bytes, 112, 0x1000);
    write<std::uint32_t>(bytes, 120, 1);
    write<std::uint32_t>(bytes, 124, 6);
    write<std::uint64_t>(bytes, 128, 0x300);
    write<std::uint64_t>(bytes, 136, 0x2000);
    write<std::uint64_t>(bytes, 152, 0x100);
    write<std::uint64_t>(bytes, 160, 0x100);
    write<std::uint64_t>(bytes, 168, 0x1000);
    std::fill(bytes.begin() + 0x200, bytes.begin() + 0x300, 0xCC);
    std::copy(text.begin(), text.end(), bytes.begin() + 0x200);
    return bytes;
}

std::vector<Domain::ProgramHeader> elfHeaders() {
    return {{1, 5, 0x200, 0x1000, 0, 0x100, 0x100, 0x1000}, {1, 6, 0x300, 0x2000, 0, 0x100, 0x100, 0x1000}};
}

void linuxPlacement() {
    const auto source = elfFixture({0xEB, 0x06, 0xF2, 0x0F, 0x78, 0xDB, 0x08, 0x08, 0xC3});
    const auto headers = elfHeaders();
    const auto converted = Codegen::MakeAmd64OnlyConverter()->Convert(source, {headers[0]});
    require(converted.Trampolines.size() == 1 && converted.Bytes == source, "Linux fixture conversion produced unexpected results");
    const auto byteWriter = std::make_shared<Io::ByteWriter>();
    Elfpatcher::Linux::LinuxElfPatcher patcher(
        std::make_shared<Elfpatcher::EntryStubBuilder>(),
        std::make_shared<Elfpatcher::ProgramHeaderLayoutBuilder>(std::make_shared<Elfpatcher::SegmentFilter>(), byteWriter),
        std::make_shared<Elfpatcher::SectionHeaderTableBuilder>(byteWriter),
        byteWriter);
    const auto output = patcher.Patch(converted.Bytes, headers, {}, 0, "$ORIGIN/libs", true, false, converted.Trampolines);
    require(output[0x202] == 0xE9 && output[0x207] == 0x90, "Linux site was not replaced by a jump");
    const auto target = 0x1002 + 5 + static_cast<std::int64_t>(read<std::int32_t>(output, 0x203));
    require(target % 16 == 0 && target > 0x2100, "Linux stub is misaligned or inside the original image");
    const auto phNum = read<std::uint16_t>(output, 56);
    std::uint64_t bodyOffset = 0;
    bool found = false;
    for (std::uint16_t index = 0; index < phNum; ++index) {
        const auto header = 64 + index * 56;
        if (read<std::uint32_t>(output, header) != 1) continue;
        const auto vaddr = read<std::uint64_t>(output, header + 16);
        const auto memSize = read<std::uint64_t>(output, header + 40);
        if (static_cast<std::uint64_t>(target) < vaddr || static_cast<std::uint64_t>(target) >= vaddr + memSize) continue;
        require((read<std::uint32_t>(output, header + 4) & 1) != 0, "Linux stub segment is not executable");
        bodyOffset = read<std::uint64_t>(output, header + 8) + (static_cast<std::uint64_t>(target) - vaddr);
        found = true;
    }
    require(found, "Linux stub is not inside a PT_LOAD segment");
    auto expectedBody = kInsertqSelfBody;
    write<std::int32_t>(expectedBody, 10, static_cast<std::int32_t>(0x1008 - (target + 9 + 5)));
    const Bytes actualBody(output.begin() + static_cast<std::ptrdiff_t>(bodyOffset), output.begin() + static_cast<std::ptrdiff_t>(bodyOffset + expectedBody.size()));
    require(actualBody == expectedBody, "Linux stub body or return branch is wrong");
    auto altered = converted.Bytes;
    altered[0x205] = 0xDC;
    requireFailure([&] { (void)patcher.Patch(altered, headers, {}, 0, "$ORIGIN/libs", true, false, converted.Trampolines); }, "Changed Linux site bytes were accepted");
}

}

#if defined(__linux__) && defined(__x86_64__)
std::uint64_t extrqReference(std::uint64_t value, std::uint64_t control) {
    const auto length = static_cast<unsigned>(control & 0x3f);
    const auto index = static_cast<unsigned>((control >> 8) & 0x3f);
    const auto shifted = value >> index;
    return length == 0 ? shifted : shifted & ((std::uint64_t{1} << length) - 1);
}

std::uint64_t runExtrqStub(const Bytes& site, std::uint64_t destination, std::uint64_t control) {
    const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto match = matcher->Match(site.data(), site.size());
    require(match && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline, "EXTRQ register form stub was not produced");
    auto body = match->StubBody;
    const auto ret = body.size();
    body.push_back(0xC3);
    const auto displacement = static_cast<std::int32_t>(ret - (match->ReturnBranchOffset + 5));
    std::memcpy(body.data() + match->ReturnBranchOffset + 1, &displacement, sizeof(displacement));
    void* code = mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    require(code != MAP_FAILED, "cannot map executable memory for the stub");
    std::memcpy(code, body.data(), body.size());
    alignas(16) std::uint64_t destinationIn[2] = {destination, 0x1122334455667788ull};
    alignas(16) std::uint64_t controlIn[2] = {control, 0};
    alignas(16) std::uint64_t out[2] = {};
    alignas(16) std::uint64_t scratchIn[2] = {0x0123456789abcdefull, 0xfedcba9876543210ull};
    alignas(16) std::uint64_t scratchOut[2] = {};
    const bool same = site[3] == 0xD2;
    asm volatile(
        "movdqu (%[scratch]), %%xmm0\n\t"
        "movdqu (%[dst]), %%xmm2\n\t"
        "movdqu (%[ctl]), %%xmm5\n\t"
        "sub $128, %%rsp\n\t"
        "call *%[code]\n\t"
        "add $128, %%rsp\n\t"
        "movdqu %%xmm2, (%[out])\n\t"
        "movdqu %%xmm0, (%[scratchOut])\n\t"
        :
        : [scratch] "r"(scratchIn), [dst] "r"(same ? controlIn : destinationIn), [ctl] "r"(controlIn), [code] "r"(code), [out] "r"(out), [scratchOut] "r"(scratchOut)
        : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "memory", "cc");
    munmap(code, 4096);
    require(scratchOut[0] == scratchIn[0] && scratchOut[1] == scratchIn[1], "EXTRQ stub clobbered a scratch register");
    return out[0];
}

void registerFormExecution() {
    const Bytes distinct = {0x66, 0x0F, 0x79, 0xD5};
    const Bytes same = {0x66, 0x0F, 0x79, 0xD2};
    const std::uint64_t value = 0x9e3779b97f4a7c15ull;
    for (const auto [length, index] : {std::pair{8u, 4u}, {0u, 0u}, {40u, 20u}, {63u, 1u}, {1u, 63u}, {16u, 48u}}) {
        const auto control = static_cast<std::uint64_t>(length) | (static_cast<std::uint64_t>(index) << 8) | 0xffffc000ull;
        require(runExtrqStub(distinct, value, control) == extrqReference(value, control), "EXTRQ register form stub computed the wrong field");
        require(runExtrqStub(same, control, control) == extrqReference(control, control), "EXTRQ register form stub with equal operands computed the wrong field");
    }
}
#else
void registerFormExecution() {}
#endif

int main() {
    try {
        decoderLengths();
        sse4aOperands();
        matcherSubstitutions();
        goldenBodies();
        registerFormExecution();
        converterSegment();
        converterFailureOffsets();
        linuxPlacement();
        std::cout << "AMD64-only converter tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
