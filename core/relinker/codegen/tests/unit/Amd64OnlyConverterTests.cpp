#include <Testing/Test.hpp>
#include <codegen/IAmd64OnlyConverter.hpp>
#include <codegen/CodegenException.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/x86/Sse4aLowering.hpp>
#include <codegen/x86/Sse4aOperands.hpp>
#include <codegen/x86/Sha256Operands.hpp>
#include <codegen/x86/Sha1Operands.hpp>
#include <codegen/x86/ClzeroOperands.hpp>
#include <codegen/x86/ClzeroLowering.hpp>
#include <codegen/x86/ReciprocalOperands.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>
#include <codegen/x86/DecodedInstruction.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/x86/X64InstructionRewriter.hpp>
#include <codegen/IInstructionScanner.hpp>
#include <domain/Types.hpp>
#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <elfpatcher/general/ProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/general/SectionHeaderTableBuilder.hpp>
#include <elfpatcher/general/SegmentFilter.hpp>
#include <elfpatcher/linux/LinuxElfPatcher.hpp>
#include <io/ByteWriter.hpp>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#ifdef __linux__
#include <sys/mman.h>
#endif
#include <iomanip>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;
using Bytes = std::vector<std::uint8_t>;
using Lowering = Codegen::Amd64OnlyLowering;

std::string hex(std::span<const std::uint8_t> bytes) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if (index != 0) stream << ' ';
        stream << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[index]);
    }
    return stream.str();
}

std::string slice(const Bytes& bytes, const std::size_t begin, const std::size_t end) {
    Require(begin <= end && end <= bytes.size(), "Slice of " + hex(bytes) + " is out of bounds");
    return hex(std::span<const std::uint8_t>(bytes).subspan(begin, end - begin));
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

template<typename TOperation>
Domain::FileByteOffset failureOffset(const TOperation& operation, const std::string& message) {
    return RequireThrows<Codegen::CodegenException>(operation, message).FailureOffset;
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

const Bytes kClzeroBody = {
    0x48, 0x8D, 0xA4, 0x24, 0x70, 0xFF, 0xFF, 0xFF, 0xF3, 0x0F, 0x7F, 0x04, 0x24, 0x51, 0x66, 0x48,
    0x0F, 0x6E, 0xC0, 0x66, 0x0F, 0xDB, 0x05, 0x35, 0x00, 0x00, 0x00, 0x66, 0x48, 0x0F, 0x7E, 0xC1,
    0x66, 0x0F, 0xEF, 0xC0, 0x66, 0x0F, 0xE7, 0x01, 0x66, 0x0F, 0xE7, 0x41, 0x10, 0x66, 0x0F, 0xE7,
    0x41, 0x20, 0x66, 0x0F, 0xE7, 0x41, 0x30, 0x59, 0xF3, 0x0F, 0x6F, 0x04, 0x24, 0x48, 0x8D, 0xA4,
    0x24, 0x90, 0x00, 0x00, 0x00, 0xE9, 0x00, 0x00, 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
    0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const std::vector<Bytes> kRipRelativeVectorLoads = {
    {0xC5, 0xF9, 0x6F, 0x05, 0x10, 0x00, 0x00, 0x00},
    {0xC4, 0xE2, 0x79, 0x00, 0x05, 0x10, 0x00, 0x00, 0x00},
    {0x62, 0xF1, 0xFD, 0x08, 0x6F, 0x05, 0x10, 0x00, 0x00, 0x00},
    {0x66, 0x0F, 0x38, 0x00, 0x05, 0x10, 0x00, 0x00, 0x00},
    {0x66, 0x0F, 0x3A, 0x0F, 0x05, 0x10, 0x00, 0x00, 0x00, 0x08}};

const std::vector<Bytes> kRegisterVectorOperations = {
    {0xC5, 0xF9, 0x6F, 0xC1}, {0x66, 0x0F, 0x38, 0x00, 0xC1}, {0xC5, 0xF8, 0x77}, {0xC4, 0xE2, 0x79, 0x00, 0x00}};

Bytes csPrefixed(const std::size_t count, const Bytes& instruction) {
    Bytes bytes(count + instruction.size(), 0x2E);
    std::copy(instruction.begin(), instruction.end(), bytes.begin() + static_cast<std::ptrdiff_t>(count));
    return bytes;
}

Bytes padded(Bytes instruction) {
    instruction.insert(instruction.end(), 8, 0x90);
    return instruction;
}

Codegen::DecodedInstructionInfo describeInstruction(const Bytes& bytes) {
    return Codegen::X64InstructionDecoder{}.DecodeInstruction(bytes.data(), bytes.size());
}

std::optional<Codegen::Amd64OnlyMatch> match(const Bytes& bytes) {
    return Codegen::MakeAmd64OnlyInstructionMatcher()->Match(bytes.data(), bytes.size());
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

Bytes fileWithText(const Bytes& text) {
    Bytes file(0x300, 0xCC);
    std::copy(text.begin(), text.end(), file.begin() + 0x200);
    return file;
}

Domain::ProgramHeader segmentHeader(const std::uint64_t size) {
    return {1, 5, 0x200, 0x1000, 0, size, size, 16};
}

Codegen::ConvertResult convert(const Bytes& file, const std::uint64_t segmentSize) {
    return Codegen::MakeAmd64OnlyConverter()->Convert(file, {segmentHeader(segmentSize)});
}

Codegen::ConvertResult convertText(const Bytes& text) {
    return convert(fileWithText(text), text.size());
}

std::pair<Bytes, std::uint64_t> extrqFollowedBy(const Bytes& following) {
    Bytes text = {0x66, 0x0F, 0x79, 0xCA};
    text.insert(text.end(), following.begin(), following.end());
    text.push_back(0xC3);
    return {fileWithText(text), text.size()};
}

const Bytes kSha256Text = {
    0x0F, 0x38, 0xCB, 0xCA,
    0x0F, 0x38, 0xCC, 0xD3,
    0x66, 0x0F, 0xFE, 0xC1,
    0x0F, 0x38, 0xCD, 0xE5,
    0x90,
    0xC3};

const Bytes kSha1Text = {
    0x0F, 0x3A, 0xCC, 0xCA, 0x00,
    0x0F, 0x38, 0xC8, 0xD3,
    0x0F, 0x38, 0xC9, 0xE5,
    0x66, 0x0F, 0xFE, 0xC1,
    0x0F, 0x38, 0xCA, 0xE5,
    0x90,
    0xC3};

const Bytes kClzeroText = {
    0x0F, 0x01, 0xFC,
    0x48, 0x83, 0xC0, 0x40,
    0x0F, 0x01, 0xFC,
    0x0F, 0x01, 0xFC,
    0x90,
    0xC3};

const Bytes kReciprocalText = {
    0xC5, 0xF8, 0x52, 0xD5,
    0xC5, 0xE8, 0x59, 0xD1,
    0xC5, 0xF8, 0x53, 0xC1,
    0xC3};

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

Elfpatcher::Linux::LinuxElfPatcher linuxPatcher() {
    const auto byteWriter = std::make_shared<Io::ByteWriter>();
    return Elfpatcher::Linux::LinuxElfPatcher(
        std::make_shared<Elfpatcher::EntryStubBuilder>(),
        std::make_shared<Elfpatcher::ProgramHeaderLayoutBuilder>(std::make_shared<Elfpatcher::SegmentFilter>(), byteWriter),
        std::make_shared<Elfpatcher::SectionHeaderTableBuilder>(byteWriter),
        byteWriter);
}

std::uint64_t executableFileOffset(const Bytes& output, const std::uint64_t address) {
    const auto phNum = read<std::uint16_t>(output, 56);
    for (std::uint16_t index = 0; index < phNum; ++index) {
        const auto header = 64 + index * 56;
        if (read<std::uint32_t>(output, header) != 1) continue;
        const auto vaddr = read<std::uint64_t>(output, header + 16);
        const auto memSize = read<std::uint64_t>(output, header + 40);
        if (address < vaddr || address >= vaddr + memSize) continue;
        Require((read<std::uint32_t>(output, header + 4) & 1) != 0, "Linux stub segment is not executable");
        return read<std::uint64_t>(output, header + 8) + (address - vaddr);
    }
    Testing::Fail("Linux stub is not inside a PT_LOAD segment");
}

struct LinuxSse4aPlacement {
    Codegen::ConvertResult Converted;
    Bytes Output;
};

LinuxSse4aPlacement placeLinuxSse4aSite() {
    const auto source = elfFixture({0xEB, 0x06, 0xF2, 0x0F, 0x78, 0xDB, 0x08, 0x08, 0xC3});
    const auto headers = elfHeaders();
    auto converted = Codegen::MakeAmd64OnlyConverter()->Convert(source, {headers[0]});
    RequireEqual(converted.Trampolines.size(), std::size_t{1}, "Linux fixture trampoline count");
    Require(converted.Bytes == source, "Linux fixture conversion changed the source bytes");
    auto output = linuxPatcher().Patch(converted.Bytes, headers, {}, 0, "$ORIGIN/libs", true, false, converted.Trampolines);
    return {std::move(converted), std::move(output)};
}

std::int64_t linuxSse4aStubAddress(const Bytes& output) {
    return 0x1002 + 5 + static_cast<std::int64_t>(read<std::int32_t>(output, 0x203));
}

const Case decoderAmd64OnlyLengths{"Decoder_Amd64OnlyAndRepairedOpcodes_DecodeToInstructionLength", [] {
    const Codegen::X64InstructionDecoder decoder;
    const std::vector<Bytes> instructions = {
        kExtrqSite, kInsertqSelfSite, kInsertqCrossSite, kInsertqHighSite, kInsertqWordSite,
        {0x66, 0x0F, 0x79, 0xCA}, {0xF2, 0x0F, 0x79, 0xCA}, {0x66, 0x45, 0x0F, 0x79, 0xCA},
        {0xF3, 0x0F, 0xB8, 0xC0}, {0xCD, 0x41}, {0x0F, 0x0D, 0x08}, {0x0F, 0xC0, 0xC1}, {0x0F, 0xC3, 0x07},
        {0x66, 0x0F, 0xC4, 0xC0, 0x01}, {0xC2, 0x08, 0x00}, {0xC8, 0x10, 0x00, 0x00}, {0xF3, 0x0F, 0x2B, 0x07},
        {0xF2, 0x44, 0x0F, 0x2B, 0x4C, 0x24, 0x10}, {0x0F, 0x01, 0xFA}, {0x0F, 0xB9, 0x00},
        {0x41, 0x0F, 0xBB, 0xF7}, {0x0F, 0xBB, 0x47, 0x08},
        {0xA0, 1, 2, 3, 4, 5, 6, 7, 8}, {0x48, 0xA1, 1, 2, 3, 4, 5, 6, 7, 8}, {0xA2, 1, 2, 3, 4, 5, 6, 7, 8},
        {0x64, 0x48, 0xA3, 1, 2, 3, 4, 5, 6, 7, 8}, {0x67, 0xA1, 1, 2, 3, 4}, {0x48, 0x67, 0xA3, 1, 2, 3, 4},
        {0x0F, 0x38, 0xCB, 0xCA}, {0x45, 0x0F, 0x38, 0xCC, 0xE1}, {0x0F, 0x38, 0xCD, 0x08}, {0x0F, 0x38, 0xCB, 0x0D, 0x10, 0x00, 0x00, 0x00},
        {0x62, 0xF1, 0x75, 0x48, 0xF6, 0xC2}, {0x62, 0xF1, 0x7D, 0x48, 0xF6, 0x05, 0x10, 0x00, 0x00, 0x00}, {0x62, 0xF1, 0x6D, 0x48, 0xF6, 0x4C, 0x24, 0x01},
        {0x0F, 0x3A, 0xCC, 0xD5, 0x03}, {0x45, 0x0F, 0x3A, 0xCC, 0xE1, 0x00}, {0x0F, 0x3A, 0xCC, 0x08, 0x01}, {0x0F, 0x38, 0xC8, 0xCA}, {0x0F, 0x38, 0xC9, 0xCA}, {0x0F, 0x38, 0xCA, 0x08},
        {0x48, 0x66, 0xB8, 0x34, 0x12}, {0x66, 0x48, 0xB8, 1, 2, 3, 4, 5, 6, 7, 8}, {0x41, 0x48, 0xB8, 1, 2, 3, 4, 5, 6, 7, 8},
        {0x66, 0x48, 0x81, 0xC0, 1, 2, 3, 4}, {0x66, 0x48, 0x05, 1, 2, 3, 4}, {0x66, 0x48, 0xC7, 0xC0, 1, 2, 3, 4},
        {0x66, 0x48, 0x69, 0xC0, 1, 2, 3, 4}, {0x66, 0x48, 0x68, 1, 2, 3, 4}, {0x66, 0x48, 0xA9, 1, 2, 3, 4},
        {0x66, 0x48, 0xF7, 0xC0, 1, 2, 3, 4}, {0x66, 0x81, 0xC0, 0x34, 0x12}, {0x48, 0x66, 0x81, 0xC0, 0x34, 0x12},
        {0x66, 0x41, 0x81, 0xC0, 0x34, 0x12}, {0x66, 0xF7, 0xC0, 0x34, 0x12},
        {0x48, 0x64, 0x8B, 0x00}, {0x48, 0xF3, 0x0F, 0x2B, 0x00}, {0x48, 0x67, 0x0F, 0x01, 0xFC}, {0x48, 0x48, 0x0F, 0x01, 0xFC}};
    for (const auto& instruction : instructions) {
        const auto bytes = padded(instruction);
        RequireEqual(decoder.Decode(bytes.data(), bytes.size()), instruction.size(), "Decoded length of " + hex(instruction));
    }
}};

const Case decoderBareSse4a{"Decoder_Sse4aOpcodeWithoutPrefix_RejectsInstruction", [] {
    const Bytes bare = {0x0F, 0x78, 0xC3, 0x08, 0x28};
    RequireThrows<Codegen::CodegenException>([&] { (void)Codegen::X64InstructionDecoder{}.Decode(bare.data(), bare.size()); }, "0F 78 without an SSE4a prefix");
}};

const Case decoderRexBeforeLegacy{"Decoder_RexBeforeLegacyPrefix_DiscardsRex", [] {
    const auto stray = describeInstruction({0x48, 0x64, 0x8B, 0x00, 0x90});
    RequireEqual(stray.Length, std::size_t{4}, "Length");
    RequireEqual(stray.OpcodeOffset, std::size_t{2}, "Opcode offset");
    RequireEqual(stray.RexPrefix, std::uint8_t{0}, "REX prefix");
    RequireEqual(stray.SegmentPrefix, std::uint8_t{0x64}, "Segment prefix");
}};

const Case decoderLastRex{"Decoder_TwoRexPrefixes_KeepsRexBeforeOpcode", [] {
    const auto last = describeInstruction({0x41, 0x48, 0x8B, 0x00, 0x90});
    RequireEqual(last.Length, std::size_t{4}, "Length");
    RequireEqual(last.OpcodeOffset, std::size_t{2}, "Opcode offset");
    RequireEqual(last.RexPrefix, std::uint8_t{0x48}, "REX prefix");
}};

const Case decoderVectorRipLoads{"Decoder_VexEvexAndThreeByteRipOperands_ReportDisplacement", [] {
    for (const auto& instruction : kRipRelativeVectorLoads) {
        const auto info = describeInstruction(instruction);
        RequireEqual(info.Length, instruction.size(), "Length of " + hex(instruction));
        Require(info.HasRipRelativeDisp, "RIP-relative operand not reported for " + hex(instruction));
        RequireEqual(read<std::int32_t>(instruction, info.RipRelativeDispOffset), 0x10, "Displacement of " + hex(instruction));
    }
}};

const Case decoderVectorRegisterOperations{"Decoder_VectorRegisterOperations_ReportNoRipOperand", [] {
    for (const auto& instruction : kRegisterVectorOperations)
        Require(!describeInstruction(instruction).HasRipRelativeDisp, "RIP-relative operand reported for " + hex(instruction));
}};

const Case decoderUd1{"Decoder_Ud1WithRipOperand_DecodesAsTrap", [] {
    const Bytes ud1 = {0x0F, 0xB9, 0x05, 0x10, 0x00, 0x00, 0x00};
    const auto trap = describeInstruction(ud1);
    RequireEqual(trap.Length, ud1.size(), "Length");
    RequireEqual(trap.FlowKind, Codegen::ControlFlowKind::Trap, "Flow kind");
    Require(trap.HasRipRelativeDisp, "UD1 RIP-relative operand was not reported");
    RequireEqual(read<std::int32_t>(ud1, trap.RipRelativeDispOffset), 0x10, "Displacement");
}};

const Case decoderTwoByteLengths{"Decoder_TwoByteOpcodesWithoutOperands_DecodeToInstructionLength", [] {
    const Codegen::X64InstructionDecoder decoder;
    const std::vector<Bytes> instructions = {
        {0x0F, 0xA8}, {0x0F, 0xA9}, {0x0F, 0xAA}, {0x66, 0x0F, 0xA8}, {0x41, 0x0F, 0xA9},
        {0x0F, 0xAB, 0xC8}, {0x0F, 0xAB, 0x05, 0x10, 0x00, 0x00, 0x00}, {0x48, 0x0F, 0xAB, 0x44, 0x24, 0x08},
        {0x0F, 0x20, 0xC0}, {0x0F, 0x20, 0x05}, {0x0F, 0x22, 0x04}, {0x0F, 0x21, 0x45}, {0x0F, 0x23, 0x85},
        {0x66, 0x0F, 0x38, 0x20, 0x05, 0x10, 0x00, 0x00, 0x00}, {0x66, 0x0F, 0x38, 0x23, 0x44, 0x24, 0x08}};
    for (const auto& instruction : instructions) {
        const auto bytes = padded(instruction);
        RequireEqual(decoder.Decode(bytes.data(), bytes.size()), instruction.size(), "Decoded length of " + hex(instruction));
        RequireEqual(decoder.DecodeInstruction(bytes.data(), bytes.size()).Length, instruction.size(), "Described length of " + hex(instruction));
    }
}};

const Case decoderControlRegister{"Decoder_MovFromControlRegister_ReportsNoRipOperand", [] {
    const auto control = describeInstruction({0x0F, 0x20, 0x05, 0x90, 0x90, 0x90, 0x90});
    RequireEqual(control.Length, std::size_t{3}, "Length");
    Require(control.HasModRm, "MOV from a control register has no ModRM");
    RequireEqual(control.ModRmByte, std::uint8_t{0x05}, "ModRM byte");
    Require(!control.HasRipRelativeDisp, "MOV from a control register was reported as RIP-relative");
}};

const Case decoderDebugRegister{"Decoder_MovToDebugRegister_ReportsNoRipOperand", [] {
    Require(!describeInstruction({0x0F, 0x23, 0x05, 0x90, 0x90, 0x90, 0x90}).HasRipRelativeDisp, "MOV to a debug register was reported as RIP-relative");
}};

const Case decoderPmovsxbw{"Decoder_PmovsxbwRipOperand_ReportsDisplacement", [] {
    const Bytes extend = {0x66, 0x0F, 0x38, 0x20, 0x05, 0x10, 0x00, 0x00, 0x00};
    const auto extended = describeInstruction(extend);
    Require(extended.HasRipRelativeDisp, "PMOVSXBW lost its RIP-relative operand");
    RequireEqual(read<std::int32_t>(extend, extended.RipRelativeDispOffset), 0x10, "PMOVSXBW displacement");
}};

const Case decoderBts{"Decoder_BtsRipOperand_ReportsDisplacement", [] {
    const Bytes bitTest = {0x0F, 0xAB, 0x05, 0x10, 0x00, 0x00, 0x00};
    const auto bts = describeInstruction(bitTest);
    Require(bts.HasRipRelativeDisp, "BTS lost its RIP-relative operand");
    RequireEqual(read<std::int32_t>(bitTest, bts.RipRelativeDispOffset), 0x10, "BTS displacement");
}};

const Case sse4aImmediateForms{"Sse4aOperands_ImmediateForms_DecodeFields", [] {
    struct Expected {
        Bytes Site;
        bool Insertq;
        int Destination;
        int Source;
        int Length;
        int Index;
    };
    const std::vector<Expected> cases = {
        {kExtrqSite, false, 3, 3, 8, 40},
        {kInsertqSelfSite, true, 3, 3, 8, 8},
        {kInsertqCrossSite, true, 1, 0, 8, 0},
        {kInsertqHighSite, true, 9, 4, 16, 16},
        {kInsertqWordSite, true, 3, 4, 16, 16}};
    for (const auto& item : cases) {
        const auto operands = Codegen::DecodeSse4a(item.Site.data(), item.Site.size());
        const auto site = hex(item.Site);
        RequireEqual(operands.Insertq, item.Insertq, "Insertq of " + site);
        Require(!operands.RegisterForm, "Register form reported for " + site);
        RequireEqual(static_cast<int>(operands.Destination), item.Destination, "Destination of " + site);
        RequireEqual(static_cast<int>(operands.Source), item.Source, "Source of " + site);
        RequireEqual(static_cast<int>(operands.Length), item.Length, "Length of " + site);
        RequireEqual(static_cast<int>(operands.Index), item.Index, "Index of " + site);
    }
}};

const Case sse4aZeroLength{"Sse4aOperands_ZeroLength_MeansSixtyFourBits", [] {
    const Bytes fullField = {0xF2, 0x0F, 0x78, 0xC8, 0x00, 0x00};
    RequireEqual(static_cast<int>(Codegen::DecodeSse4a(fullField.data(), fullField.size()).Length), 64, "Field length");
}};

const Case sse4aRegisterForm{"Sse4aOperands_RegisterForm_DecodesRegisters", [] {
    const Bytes registerForm = {0x66, 0x45, 0x0F, 0x79, 0xCA};
    const auto decoded = Codegen::DecodeSse4a(registerForm.data(), registerForm.size());
    Require(decoded.RegisterForm, "Register form was not reported");
    Require(!decoded.Insertq, "EXTRQ register form was decoded as INSERTQ");
    RequireEqual(static_cast<int>(decoded.Destination), 9, "Destination");
    RequireEqual(static_cast<int>(decoded.Source), 10, "Source");
}};

const Case sse4aStrayRex{"Sse4aOperands_RexBeforeLegacyPrefix_IgnoresRex", [] {
    const Bytes strayRex = {0x41, 0xF2, 0x0F, 0x79, 0xCA};
    const auto ignored = Codegen::DecodeSse4a(strayRex.data(), strayRex.size());
    Require(ignored.RegisterForm, "Register form was not reported");
    Require(ignored.Insertq, "INSERTQ register form was decoded as EXTRQ");
    RequireEqual(static_cast<int>(ignored.Destination), 1, "Destination");
    RequireEqual(static_cast<int>(ignored.Source), 2, "Source");
}};

const Case sse4aInvalidSites{"Sse4aOperands_InvalidImmediateForms_RejectsInstruction", [] {
    const std::vector<std::pair<Bytes, const char*>> cases = {
        {{0x66, 0x0F, 0x78, 0xCB, 0x08, 0x28}, "EXTRQ with a non-zero reg field"},
        {{0xF2, 0x0F, 0x78, 0x1B, 0x08, 0x08}, "SSE4a memory operand"},
        {{0xF2, 0x0F, 0x78, 0xC8, 0x20, 0x30}, "Field beyond bit 64"}};
    for (const auto& [bytes, name] : cases)
        RequireThrows<Codegen::CodegenException>([&] { (void)Codegen::DecodeSse4a(bytes.data(), bytes.size()); }, name);
}};

const Case sha256RegisterForms{"Sha256Operands_RegisterForms_DecodeOperationAndRegisters", [] {
    struct Expected {
        Bytes Site;
        Codegen::Sha256Operation Operation;
        int Destination;
        int Source;
    };
    const std::vector<Expected> cases = {
        {{0x0F, 0x38, 0xCB, 0xCA}, Codegen::Sha256Operation::Rnds2, 1, 2},
        {{0x45, 0x0F, 0x38, 0xCC, 0xE1}, Codegen::Sha256Operation::Msg1, 12, 9},
        {{0x44, 0x0F, 0x38, 0xCD, 0xC0}, Codegen::Sha256Operation::Msg2, 8, 0}};
    for (const auto& item : cases) {
        const auto operands = Codegen::DecodeSha256(item.Site.data(), item.Site.size());
        RequireEqual(operands.Operation, item.Operation, "Operation of " + hex(item.Site));
        RequireEqual(static_cast<int>(operands.Destination), item.Destination, "Destination of " + hex(item.Site));
        RequireEqual(static_cast<int>(operands.Source), item.Source, "Source of " + hex(item.Site));
    }
}};

const Case sha256RexPosition{"Sha256Operands_RexBeforeLegacyPrefix_IgnoresRex", [] {
    const Bytes ignored = {0x41, 0x2E, 0x0F, 0x38, 0xCC, 0xCA};
    const Bytes applied = {0x2E, 0x41, 0x0F, 0x38, 0xCC, 0xCA};
    const auto stray = Codegen::DecodeSha256(ignored.data(), ignored.size());
    const auto kept = Codegen::DecodeSha256(applied.data(), applied.size());
    RequireEqual(stray.Operation, Codegen::Sha256Operation::Msg1, "Operation with a stray REX");
    RequireEqual(static_cast<int>(stray.Destination), 1, "Destination with a stray REX");
    RequireEqual(static_cast<int>(stray.Source), 2, "Source with a stray REX");
    RequireEqual(kept.Operation, Codegen::Sha256Operation::Msg1, "Operation with REX before the opcode");
    RequireEqual(static_cast<int>(kept.Destination), 1, "Destination with REX before the opcode");
    RequireEqual(static_cast<int>(kept.Source), 10, "Source with REX before the opcode");
}};

const Case sha256StackMemory{"Sha256Operands_PrefixedStackMemory_DecodesOperand", [] {
    const Bytes memory = {0x65, 0x67, 0x44, 0x0F, 0x38, 0xCD, 0x54, 0x8C, 0xF0};
    const auto decoded = Codegen::DecodeSha256(memory.data(), memory.size());
    RequireEqual(decoded.Operation, Codegen::Sha256Operation::Msg2, "Operation");
    RequireEqual(static_cast<int>(decoded.Destination), 10, "Destination");
    Require(decoded.Memory.has_value(), "Memory operand was not decoded");
    RequireEqual(hex(decoded.Memory->Prefixes), hex(Bytes{0x65, 0x67}), "Prefixes");
    RequireEqual(static_cast<int>(decoded.Memory->Mod), 1, "Mod");
    RequireEqual(static_cast<int>(decoded.Memory->Rm), 4, "Rm");
    RequireEqual(static_cast<int>(decoded.Memory->Sib), 0x8C, "SIB");
    RequireEqual(decoded.Memory->Displacement, -16, "Displacement");
    Require(decoded.Memory->StackBase, "RSP base was not reported as a stack base");
}};

const Case sha256R12Base{"Sha256Operands_R12Base_IsNotStackBase", [] {
    const Bytes r12Base = {0x41, 0x0F, 0x38, 0xCC, 0x14, 0x24};
    const auto decoded = Codegen::DecodeSha256(r12Base.data(), r12Base.size());
    Require(decoded.Memory.has_value(), "Memory operand was not decoded");
    Require(!decoded.Memory->StackBase, "R12 base was decoded as RSP");
}};

const Case sha256AbsoluteSib{"Sha256Operands_SibWithoutBase_DecodesAbsoluteDisplacement", [] {
    const Bytes absolute = {0x0F, 0x38, 0xCC, 0x14, 0x25, 0x78, 0x56, 0x34, 0x12};
    const auto noBase = Codegen::DecodeSha256(absolute.data(), absolute.size());
    Require(noBase.Memory.has_value(), "Memory operand was not decoded");
    Require(!noBase.Memory->StackBase, "SIB without a base was decoded as a stack base");
    Require(!noBase.Memory->RipRelative, "SIB without a base was decoded as RIP-relative");
    RequireEqual(noBase.Memory->Displacement, 0x12345678, "Displacement");
}};

const Case sha256RipRelative{"Sha256Operands_RipRelative_DecodesDisplacementAndLength", [] {
    const Bytes ripRelative = {0x0F, 0x38, 0xCC, 0x15, 0x78, 0x56, 0x34, 0x12};
    const auto rip = Codegen::DecodeSha256(ripRelative.data(), ripRelative.size());
    RequireEqual(static_cast<int>(rip.Destination), 2, "Destination");
    Require(rip.Memory.has_value(), "Memory operand was not decoded");
    Require(rip.Memory->RipRelative, "Operand was not decoded as RIP-relative");
    RequireEqual(rip.Memory->Displacement, 0x12345678, "Displacement");
    RequireEqual(rip.Memory->EncodedSize, std::size_t{5}, "Encoded size");
    RequireEqual(rip.Memory->InstructionLength, std::size_t{8}, "Instruction length");
}};

const Case sha256InvalidSites{"Sha256Operands_InvalidEncodings_RejectsInstruction", [] {
    const std::vector<std::pair<Bytes, const char*>> cases = {
        {{0x0F, 0x38, 0xCC, 0x15, 0, 0, 0, 0, 0x90}, "SHA-256 RIP-relative operand that does not end the instruction"},
        {{0x0F, 0x38, 0xCC, 0x15, 0, 0, 0}, "Truncated SHA-256 RIP-relative operand"},
        {{0x0F, 0x38, 0xCC, 0x54, 0x24}, "Truncated SHA-256 memory operand"},
        {{0x66, 0x0F, 0x38, 0xCB, 0xCA}, "Prefixed 0F 38 CB"},
        {{0x0F, 0x38, 0xC9, 0xCA}, "SHA-1 opcode"}};
    for (const auto& [bytes, name] : cases)
        RequireThrows<Codegen::CodegenException>([&] { (void)Codegen::DecodeSha256(bytes.data(), bytes.size()); }, name);
}};

const Case sha256IsShaNi{"DecodedInstruction_Sha256Rnds2_IsShaNi", [] {
    const Bytes rounds = {0x0F, 0x38, 0xCB, 0xCA};
    Require(Codegen::DecodedInstruction{rounds.data(), rounds.size()}.IsShaNi(), "SHA256RNDS2 is not recognised as SHA-NI");
}};

const Case sha1RegisterForms{"Sha1Operands_RegisterForms_DecodeOperationRegistersAndFunction", [] {
    struct Expected {
        Bytes Site;
        Codegen::Sha1Operation Operation;
        int Destination;
        int Source;
        int Function;
    };
    const std::vector<Expected> cases = {
        {{0x0F, 0x3A, 0xCC, 0xCA, 0x02}, Codegen::Sha1Operation::Rnds4, 1, 2, 2},
        {{0x45, 0x0F, 0x3A, 0xCC, 0xE1, 0xFF}, Codegen::Sha1Operation::Rnds4, 12, 9, 3},
        {{0x44, 0x0F, 0x38, 0xC8, 0xC0}, Codegen::Sha1Operation::Nexte, 8, 0, 0},
        {{0x0F, 0x38, 0xC9, 0xD5}, Codegen::Sha1Operation::Msg1, 2, 5, 0},
        {{0x2E, 0x41, 0x0F, 0x38, 0xCA, 0xCA}, Codegen::Sha1Operation::Msg2, 1, 10, 0}};
    for (const auto& item : cases) {
        const auto operands = Codegen::DecodeSha1(item.Site.data(), item.Site.size());
        const auto site = hex(item.Site);
        RequireEqual(operands.Operation, item.Operation, "Operation of " + site);
        RequireEqual(static_cast<int>(operands.Destination), item.Destination, "Destination of " + site);
        RequireEqual(static_cast<int>(operands.Source), item.Source, "Source of " + site);
        RequireEqual(static_cast<int>(operands.Function), item.Function, "Function of " + site);
    }
}};

const Case sha1StackMemory{"Sha1Operands_PrefixedStackMemory_DecodesOperand", [] {
    const Bytes memory = {0x65, 0x67, 0x44, 0x0F, 0x3A, 0xCC, 0x54, 0x8C, 0xF0, 0xFE};
    const auto decoded = Codegen::DecodeSha1(memory.data(), memory.size());
    RequireEqual(decoded.Operation, Codegen::Sha1Operation::Rnds4, "Operation");
    RequireEqual(static_cast<int>(decoded.Destination), 10, "Destination");
    RequireEqual(static_cast<int>(decoded.Function), 2, "Function");
    Require(decoded.Memory.has_value(), "Memory operand was not decoded");
    RequireEqual(hex(decoded.Memory->Prefixes), hex(Bytes{0x65, 0x67}), "Prefixes");
    RequireEqual(static_cast<int>(decoded.Memory->Mod), 1, "Mod");
    RequireEqual(static_cast<int>(decoded.Memory->Rm), 4, "Rm");
    RequireEqual(static_cast<int>(decoded.Memory->Sib), 0x8C, "SIB");
    RequireEqual(decoded.Memory->Displacement, -16, "Displacement");
    Require(decoded.Memory->StackBase, "RSP base was not reported as a stack base");
}};

const Case sha1Rnds4MemoryImmediate{"Sha1Operands_Rnds4MemoryForms_ReadImmediateAfterOperand", [] {
    struct Expected {
        Bytes Site;
        int Destination;
        std::int32_t Displacement;
        int Function;
    };
    const std::vector<Expected> cases = {
        {{0x0F, 0x3A, 0xCC, 0x08, 0x03}, 1, 0, 3},
        {{0x0F, 0x3A, 0xCC, 0x50, 0x02, 0x01}, 2, 2, 1},
        {{0x0F, 0x3A, 0xCC, 0x91, 0x78, 0x56, 0x34, 0x12, 0x01}, 2, 0x12345678, 1},
        {{0x41, 0x0F, 0x3A, 0xCC, 0x14, 0x24, 0x02}, 2, 0, 2},
        {{0x0F, 0x3A, 0xCC, 0x14, 0x25, 0x78, 0x56, 0x34, 0x12, 0x03}, 2, 0x12345678, 3}};
    for (const auto& item : cases) {
        const auto operands = Codegen::DecodeSha1(item.Site.data(), item.Site.size());
        const auto site = hex(item.Site);
        RequireEqual(operands.Operation, Codegen::Sha1Operation::Rnds4, "Operation of " + site);
        RequireEqual(static_cast<int>(operands.Destination), item.Destination, "Destination of " + site);
        Require(operands.Memory.has_value(), "Memory operand was not decoded for " + site);
        Require(!operands.Memory->StackBase, "Stack base reported for " + site);
        RequireEqual(operands.Memory->Displacement, item.Displacement, "Displacement of " + site);
        RequireEqual(static_cast<int>(operands.Function), item.Function, "Function of " + site);
    }
}};

const Case sha1Msg1Memory{"Sha1Operands_Msg1Memory_DecodesOperand", [] {
    const Bytes message = {0x0F, 0x38, 0xC9, 0x08};
    const auto decoded = Codegen::DecodeSha1(message.data(), message.size());
    RequireEqual(decoded.Operation, Codegen::Sha1Operation::Msg1, "Operation");
    RequireEqual(static_cast<int>(decoded.Destination), 1, "Destination");
    Require(decoded.Memory.has_value(), "Memory operand was not decoded");
    RequireEqual(static_cast<int>(decoded.Memory->Mod), 0, "Mod");
    RequireEqual(static_cast<int>(decoded.Memory->Rm), 0, "Rm");
}};

const Case sha1Msg1RipRelative{"Sha1Operands_Msg1RipRelative_DecodesDisplacementAndLength", [] {
    const Bytes ripMessage = {0x0F, 0x38, 0xC9, 0x15, 0x10, 0x00, 0x00, 0x00};
    const auto decoded = Codegen::DecodeSha1(ripMessage.data(), ripMessage.size());
    RequireEqual(decoded.Operation, Codegen::Sha1Operation::Msg1, "Operation");
    RequireEqual(static_cast<int>(decoded.Destination), 2, "Destination");
    Require(decoded.Memory.has_value() && decoded.Memory->RipRelative, "Operand was not decoded as RIP-relative");
    RequireEqual(decoded.Memory->Displacement, 0x10, "Displacement");
    RequireEqual(decoded.Memory->InstructionLength, std::size_t{8}, "Instruction length");
}};

const Case sha1Rnds4RipRelative{"Sha1Operands_Rnds4RipRelative_CountsImmediateInLength", [] {
    const Bytes ripRounds = {0x0F, 0x3A, 0xCC, 0x15, 0xF0, 0xFF, 0xFF, 0xFF, 0x03};
    const auto decoded = Codegen::DecodeSha1(ripRounds.data(), ripRounds.size());
    RequireEqual(decoded.Operation, Codegen::Sha1Operation::Rnds4, "Operation");
    RequireEqual(static_cast<int>(decoded.Function), 3, "Function");
    Require(decoded.Memory.has_value() && decoded.Memory->RipRelative, "Operand was not decoded as RIP-relative");
    RequireEqual(decoded.Memory->Displacement, -16, "Displacement");
    RequireEqual(decoded.Memory->InstructionLength, std::size_t{9}, "Instruction length");
}};

const Case sha1InvalidSites{"Sha1Operands_InvalidEncodings_RejectsInstruction", [] {
    const std::vector<std::pair<Bytes, const char*>> cases = {
        {{0x0F, 0x38, 0xC9, 0x15, 0, 0, 0, 0, 0x90}, "SHA-1 RIP-relative operand that does not end the instruction"},
        {{0x0F, 0x3A, 0xCC, 0x15, 0, 0, 0, 0, 0x00, 0x90}, "SHA1RNDS4 RIP-relative operand that does not end the instruction"},
        {{0x0F, 0x38, 0xC9, 0x54, 0x24}, "Truncated SHA-1 memory operand"},
        {{0x0F, 0x3A, 0xCC, 0xCA}, "SHA1RNDS4 without its immediate"},
        {{0x0F, 0x3A, 0xCC, 0x50, 0x02}, "SHA1RNDS4 memory form without its immediate"},
        {{0x66, 0x0F, 0x38, 0xC8, 0xCA}, "Prefixed 0F 38 C8"},
        {{0x0F, 0x38, 0xCC, 0xCA}, "SHA-256 opcode"}};
    for (const auto& [bytes, name] : cases)
        RequireThrows<Codegen::CodegenException>([&] { (void)Codegen::DecodeSha1(bytes.data(), bytes.size()); }, name);
}};

const Case sha1PrefixedRounds{"DecodedInstruction_OperandSizePrefixedSha1Rnds4_IsNotSha1", [] {
    const Bytes prefixedRounds = {0x66, 0x0F, 0x3A, 0xCC, 0xCA, 0x00};
    Require(!Codegen::DecodedInstruction{prefixedRounds.data(), prefixedRounds.size()}.IsSha1(), "66-prefixed 0F 3A CC was recognised as SHA-1");
}};

Codegen::ClzeroOperands decodeClzero(const Bytes& bytes) {
    return Codegen::DecodeClzero(bytes.data(), bytes.size());
}

const Case clzeroAddressSize{"ClzeroOperands_AddressSizePrefix_SelectsThirtyTwoBitAddress", [] {
    Require(!decodeClzero({0x0F, 0x01, 0xFC}).AddressSize32, "Plain CLZERO was decoded with a 32-bit address");
    Require(decodeClzero({0x67, 0x0F, 0x01, 0xFC}).AddressSize32, "67h CLZERO was decoded with a 64-bit address");
}};

const Case clzeroDsAndRex{"ClzeroOperands_DsOverrideAndRex_DecodesSixtyFourBitAddress", [] {
    Require(!decodeClzero({0x3E, 0x48, 0x0F, 0x01, 0xFC}).AddressSize32, "CLZERO with a DS override and REX was not decoded");
}};

const Case clzeroInvalidPrefixes{"ClzeroOperands_LockOrFsGsPrefix_RejectsInstruction", [] {
    const std::vector<std::pair<Bytes, const char*>> cases = {
        {{0xF0, 0x0F, 0x01, 0xFC}, "LOCK CLZERO"},
        {{0x64, 0x0F, 0x01, 0xFC}, "FS-relative CLZERO"},
        {{0x65, 0x0F, 0x01, 0xFC}, "GS-relative CLZERO"}};
    for (const auto& [bytes, name] : cases)
        RequireThrows<Codegen::CodegenException>([&] { (void)decodeClzero(bytes); }, name);
}};

const Case clzeroOverlong{"ClzeroOperands_LongerThanFifteenBytes_RejectsInstruction", [] {
    const auto overlong = csPrefixed(13, {0x0F, 0x01, 0xFC});
    RequireThrows<Codegen::CodegenException>([&] { (void)decodeClzero(overlong); }, "CLZERO longer than 15 bytes");
}};

std::optional<Codegen::ReciprocalOperands> decodeReciprocal(const Bytes& bytes) {
    return Codegen::DecodeVexReciprocal(bytes.data(), bytes.size());
}

const Case reciprocalVex2Rsqrt{"ReciprocalOperands_Vex2Rsqrtps_DecodesOperands", [] {
    const auto vex2 = decodeReciprocal({0xC5, 0xF8, 0x52, 0xD5});
    Require(vex2.has_value(), "VEX2 VRSQRTPS was not decoded");
    RequireEqual(vex2->Operation, Codegen::ReciprocalOperation::ReciprocalSquareRoot, "Operation");
    RequireEqual(static_cast<int>(vex2->Destination), 2, "Destination");
    RequireEqual(static_cast<int>(vex2->Source), 5, "Source");
}};

const Case reciprocalVex2HighRcp{"ReciprocalOperands_Vex2RcppsHighDestination_DecodesOperands", [] {
    const auto vex2High = decodeReciprocal({0xC5, 0x78, 0x53, 0xC1});
    Require(vex2High.has_value(), "VEX2 VRCPPS with a high destination was not decoded");
    RequireEqual(vex2High->Operation, Codegen::ReciprocalOperation::Reciprocal, "Operation");
    RequireEqual(static_cast<int>(vex2High->Destination), 8, "Destination");
    RequireEqual(static_cast<int>(vex2High->Source), 1, "Source");
}};

const Case reciprocalVex3High{"ReciprocalOperands_Vex3HighRegisters_DecodesOperands", [] {
    const auto vex3 = decodeReciprocal({0xC4, 0x41, 0x78, 0x52, 0xC9});
    Require(vex3.has_value(), "VEX3 VRSQRTPS with high registers was not decoded");
    RequireEqual(static_cast<int>(vex3->Destination), 9, "Destination");
    RequireEqual(static_cast<int>(vex3->Source), 9, "Source");
}};

const Case reciprocalNative{"ReciprocalOperands_NonPacked128RegisterForms_StayNative", [] {
    const std::vector<std::pair<Bytes, const char*>> cases = {
        {{0xC5, 0xFC, 0x52, 0xD5}, "256-bit VRSQRTPS"},
        {{0xC5, 0xFA, 0x52, 0xD5}, "VRSQRTSS"},
        {{0xC5, 0xF8, 0x52, 0x10}, "Memory form"},
        {{0xC5, 0xF8, 0x51, 0xD5}, "VSQRTPS"},
        {{0x0F, 0x52, 0xD5}, "Legacy RSQRTPS"}};
    for (const auto& [bytes, name] : cases)
        Require(!decodeReciprocal(bytes).has_value(), std::string(name) + " was decoded as a lowered reciprocal");
}};

void requireInPlace(const std::optional<Codegen::Amd64OnlyMatch>& matched, const Bytes& replacement, const std::string& name) {
    Require(matched.has_value(), name + " was not matched");
    RequireEqual(matched->Lowering, Lowering::InPlace, name + " lowering");
    RequireEqual(hex(matched->ReplacementBytes), hex(replacement), name + " replacement");
    RequireEqual(matched->InstructionName, name, "Instruction name");
}

void requireTrampoline(const std::optional<Codegen::Amd64OnlyMatch>& matched, const std::string& name, const std::string& site) {
    Require(matched.has_value(), name + " was not matched: " + site);
    RequireEqual(matched->Lowering, Lowering::Trampoline, name + " lowering of " + site);
    RequireEqual(matched->InstructionName, name, "Instruction name of " + site);
}

const Case matcherMovntss{"Matcher_MovntssMemory_RewritesToMovss", [] {
    requireInPlace(match({0xF3, 0x0F, 0x2B, 0x07}), {0xF3, 0x0F, 0x11, 0x07}, "MOVNTSS");
}};

const Case matcherMovntsd{"Matcher_MovntsdMemory_RewritesToMovsd", [] {
    requireInPlace(match({0xF2, 0x44, 0x0F, 0x2B, 0x4C, 0x24, 0x10}), {0xF2, 0x44, 0x0F, 0x11, 0x4C, 0x24, 0x10}, "MOVNTSD");
}};

const Case matcherMovntssRegister{"Matcher_MovntssRegister_RejectsInstruction", [] {
    RequireThrows<Codegen::CodegenException>([] { (void)match({0xF3, 0x0F, 0x2B, 0xC1}); }, "MOVNTSS with a register operand");
}};

const Case matcherMonitorx{"Matcher_Monitorx_ReplacedByNop", [] {
    requireInPlace(match({0x0F, 0x01, 0xFA}), {0x0F, 0x1F, 0x00}, "MONITORX");
}};

const Case matcherMwaitx{"Matcher_Mwaitx_ReplacedByPause", [] {
    requireInPlace(match({0x0F, 0x01, 0xFB}), {0xF3, 0x90, 0x90}, "MWAITX");
}};

const Case matcherPrefixedMonitorWait{"Matcher_PrefixedMonitorxAndMwaitx_PaddedToSiteLength", [] {
    const auto longMonitorx = csPrefixed(5, {0x0F, 0x01, 0xFA});
    const auto longestMwaitx = csPrefixed(12, {0x0F, 0x01, 0xFB});
    const std::vector<std::pair<Bytes, Bytes>> cases = {
        {{0x67, 0x0F, 0x01, 0xFA}, {0x0F, 0x1F, 0x40, 0x00}},
        {{0x2E, 0x41, 0x0F, 0x01, 0xFB}, {0xF3, 0x90, 0x0F, 0x1F, 0x00}},
        {longMonitorx, {0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x90}},
        {longestMwaitx, {0xF3, 0x90, 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00}}};
    for (const auto& [site, replacement] : cases) {
        const auto matched = match(site);
        Require(matched.has_value(), "Prefixed site was not matched: " + hex(site));
        RequireEqual(hex(matched->ReplacementBytes), hex(replacement), "Replacement of " + hex(site));
    }
}};

const Case matcherLockedMwaitx{"Matcher_LockedMwaitx_ReportsUnsupported", [] {
    const auto lockedMwaitx = match({0xF0, 0x0F, 0x01, 0xFB});
    Require(lockedMwaitx.has_value(), "LOCK MWAITX was not matched");
    RequireEqual(lockedMwaitx->Lowering, Lowering::Unsupported, "LOCK MWAITX lowering");
}};

const Case matcherOverlongMonitorx{"Matcher_MonitorxLongerThanFifteenBytes_ReportsUnsupported", [] {
    const auto overlongMonitorx = csPrefixed(13, {0x0F, 0x01, 0xFA});
    const auto overlong = match(overlongMonitorx);
    Require(overlong.has_value(), "Overlong MONITORX was not matched");
    RequireEqual(overlong->Lowering, Lowering::Unsupported, "Overlong MONITORX lowering");
}};

const Case matcherClzero{"Matcher_Clzero_LowersThroughStub", [] {
    requireTrampoline(match({0x0F, 0x01, 0xFC}), "CLZERO", "0F 01 FC");
}};

const Case matcherPrefixedClzero{"Matcher_ClzeroWithMandatoryPrefix_ReportsUnsupported", [] {
    for (const std::uint8_t prefix : {std::uint8_t{0x66}, std::uint8_t{0xF2}, std::uint8_t{0xF3}}) {
        const Bytes site = {prefix, 0x0F, 0x01, 0xFC};
        const auto prefixedClzero = match(site);
        Require(prefixedClzero.has_value(), "Prefixed CLZERO was not matched: " + hex(site));
        RequireEqual(prefixedClzero->Lowering, Lowering::Unsupported, "Lowering of " + hex(site));
    }
}};

const Case matcherRdpru{"Matcher_Rdpru_ReportsUnsupported", [] {
    const auto rdpru = match({0x0F, 0x01, 0xFD});
    Require(rdpru.has_value(), "RDPRU was not matched");
    RequireEqual(rdpru->Lowering, Lowering::Unsupported, "RDPRU lowering");
    RequireEqual(rdpru->InstructionName, std::string("RDPRU"), "Instruction name");
}};

const Case matcherSse4aRegisterForms{"Matcher_Sse4aRegisterForms_LowerThroughStub", [] {
    requireTrampoline(match({0x66, 0x0F, 0x79, 0xCA}), "EXTRQ register form", "66 0F 79 CA");
    requireTrampoline(match({0xF2, 0x0F, 0x79, 0xCA}), "INSERTQ register form", "F2 0F 79 CA");
}};

const Case matcherOrdinary{"Matcher_OrdinaryInstructions_AreNotMatched", [] {
    for (const Bytes& site : {Bytes{0x66, 0x0F, 0x2B, 0x07}, Bytes{0x0F, 0x2B, 0x07}, Bytes{0x48, 0x8B, 0x05, 0, 0, 0, 0}})
        Require(!match(site).has_value(), "Ordinary instruction was matched: " + hex(site));
}};

const Case matcherSha256{"Matcher_Sha256Instructions_LowerThroughStub", [] {
    for (const auto& [bytes, name] : {std::pair{Bytes{0x0F, 0x38, 0xCB, 0xCA}, "SHA256RNDS2"}, {Bytes{0x0F, 0x38, 0xCC, 0xCA}, "SHA256MSG1"}, {Bytes{0x45, 0x0F, 0x38, 0xCD, 0xE1}, "SHA256MSG2"}})
        requireTrampoline(match(bytes), name, hex(bytes));
}};

const Case matcherSha1Registers{"Matcher_Sha1RegisterForms_LowerThroughStub", [] {
    for (const auto& [bytes, name] : {std::pair{Bytes{0x0F, 0x3A, 0xCC, 0xCA, 0x01}, "SHA1RNDS4"}, {Bytes{0x0F, 0x38, 0xC8, 0xCA}, "SHA1NEXTE"}, {Bytes{0x0F, 0x38, 0xC9, 0xCA}, "SHA1MSG1"}, {Bytes{0x45, 0x0F, 0x38, 0xCA, 0xE1}, "SHA1MSG2"}})
        requireTrampoline(match(bytes), name, hex(bytes));
}};

const Case matcherSha1Memory{"Matcher_Sha1MemoryForms_LowerThroughStub", [] {
    for (const auto& [bytes, name] : {std::pair{Bytes{0x0F, 0x3A, 0xCC, 0x08, 0x01}, "SHA1RNDS4"}, {Bytes{0x0F, 0x38, 0xC8, 0x48, 0x10}, "SHA1NEXTE"}, {Bytes{0x0F, 0x38, 0xC9, 0x4C, 0x24, 0x18}, "SHA1MSG1"}, {Bytes{0x45, 0x0F, 0x38, 0xCA, 0x21}, "SHA1MSG2"}})
        requireTrampoline(match(bytes), name, hex(bytes));
}};

const Case matcherSha1StackOperand{"Matcher_Sha1StackOperand_LoadsPastSpilledScratch", [] {
    const auto stackMessage = match({0x0F, 0x38, 0xC9, 0x4C, 0x24, 0x18});
    Require(stackMessage.has_value(), "SHA1MSG1 stack form was not matched");
    RequireEqual(slice(stackMessage->StubBody, 13, 22), hex(Bytes{0xF3, 0x0F, 0x6F, 0x84, 0x24, 0xA8, 0x00, 0x00, 0x00}), "Stack operand load");
}};

const Case matcherInsertqHigh{"Matcher_InsertqHighRegisters_UsesGoldenStub", [] {
    const auto stub = match(kInsertqHighSite);
    requireTrampoline(stub, "INSERTQ", hex(kInsertqHighSite));
    RequireEqual(hex(stub->StubBody), hex(kInsertqHighBody), "Stub body");
    RequireEqual(stub->ReturnBranchOffset, std::size_t{15}, "Return branch offset");
}};

const Case matcherTopAlignedExtrq{"Matcher_TopAlignedExtrq_LowersThroughStub", [] {
    requireTrampoline(match({0x66, 0x0F, 0x78, 0xC3, 0x18, 0x28}), "EXTRQ", "66 0F 78 C3 18 28");
}};

const Case loweringDemonsSoulsSites{"Sse4aLowering_DemonsSoulsSites_MatchGoldenStubs", [] {
    const Codegen::Sse4aLowering lowering;
    const std::vector<std::tuple<Bytes, Bytes, std::size_t>> cases = {
        {kExtrqSite, kExtrqBody, 9},
        {kInsertqSelfSite, kInsertqSelfBody, 9},
        {kInsertqCrossSite, kInsertqCrossBody, 13},
        {kInsertqHighSite, kInsertqHighBody, 15},
        {kInsertqWordSite, kInsertqWordBody, 13}};
    for (const auto& [site, expected, returnBranchOffset] : cases) {
        const auto operands = Codegen::DecodeSse4a(site.data(), site.size());
        Require(!lowering.LowerInPlace(operands, site.size()).has_value(), "Site unexpectedly qualified for an in-place lowering: " + hex(site));
        const auto body = lowering.LowerOutOfLine(operands);
        RequireEqual(body.ReturnBranchOffset, returnBranchOffset, "Return branch offset of " + hex(site));
        RequireEqual(hex(body.Bytes), hex(expected), "Stub body of " + hex(site));
    }
}};

const Case loweringInPlace{"Sse4aLowering_SitesZeroingHighQword_LowerInPlace", [] {
    const Codegen::Sse4aLowering lowering;
    const std::vector<std::pair<Bytes, Bytes>> cases = {
        {{0xF2, 0x0F, 0x78, 0xC8, 0x00, 0x00}, {0xF3, 0x0F, 0x7E, 0xC8, 0x66, 0x90}},
        {{0xF2, 0x0F, 0x78, 0xDB, 0x08, 0x00}, {0xF3, 0x0F, 0x7E, 0xDB, 0x66, 0x90}},
        {{0x66, 0x0F, 0x78, 0xC3, 0x00, 0x00}, {0xF3, 0x0F, 0x7E, 0xDB, 0x66, 0x90}}};
    for (const auto& [site, expected] : cases) {
        const auto sequence = lowering.LowerInPlace(Codegen::DecodeSse4a(site.data(), site.size()), site.size());
        Require(sequence.has_value(), "Site was not lowered in place: " + hex(site));
        RequireEqual(hex(*sequence), hex(expected), "In-place lowering of " + hex(site));
    }
}};

const Case loweringNotInPlace{"Sse4aLowering_SitesNotZeroingHighQword_AreNotLoweredInPlace", [] {
    const Codegen::Sse4aLowering lowering;
    for (const auto& site : {Bytes{0x66, 0x0F, 0x78, 0xC3, 0x18, 0x28}, Bytes{0x66, 0x0F, 0x78, 0xC3, 0x08, 0x00}, Bytes{0xF2, 0x0F, 0x78, 0xC8, 0x20, 0x00}, Bytes{0xF2, 0x45, 0x0F, 0x78, 0xC8, 0x10, 0x00}}) {
        const auto operands = Codegen::DecodeSse4a(site.data(), site.size());
        Require(!lowering.LowerInPlace(operands, site.size()).has_value(), "SSE4a site was lowered in place without zeroing dst[127:64]: " + hex(site));
    }
}};

const Case loweringClzero{"ClzeroLowering_PlainSite_MatchesGoldenStub", [] {
    const Bytes clzeroSite = {0x0F, 0x01, 0xFC};
    const auto clzero = Codegen::ClzeroLowering{}.LowerOutOfLine(Codegen::DecodeClzero(clzeroSite.data(), clzeroSite.size()));
    RequireEqual(hex(clzero.Bytes), hex(kClzeroBody), "CLZERO stub body");
    RequireEqual(clzero.ReturnBranchOffset, std::size_t{69}, "Return branch offset");
}};

void requireRedZoneSkip(const Codegen::LoweredBody& body, const std::string& name) {
    Require(!body.Bytes.empty(), name + " body is empty");
    RequireEqual(static_cast<int>(body.Bytes[0]), 0x48, name + " first byte");
    RequireEqual(body.Bytes.size() % 16, std::size_t{0}, name + " size modulo 16");
    Require(body.ReturnBranchOffset < body.Bytes.size(), name + " return branch is outside the body");
}

const Case loweringGenericInsertq{"Sse4aLowering_GenericInsertq_StartsWithRedZoneSkip", [] {
    requireRedZoneSkip(Codegen::Sse4aLowering{}.LowerOutOfLine(Codegen::Sse4aOperands{true, false, 9, 4, 5, 3}), "Generic INSERTQ");
}};

const Case loweringInsertqRegisterForm{"Sse4aLowering_InsertqRegisterForm_StartsWithRedZoneSkip", [] {
    requireRedZoneSkip(Codegen::Sse4aLowering{}.LowerOutOfLine(Codegen::Sse4aOperands{true, true, 1, 2, 0, 0}), "INSERTQ register form");
}};

void requireRelocation(const Codegen::Amd64OnlyMatch& matched, const std::size_t displacementOffset, const std::size_t instructionEnd, const std::int64_t siteTarget, const std::string& name) {
    RequireEqual(matched.Relocations.size(), std::size_t{1}, name + " relocation count");
    RequireEqual(matched.Relocations[0].DisplacementOffset, displacementOffset, name + " displacement offset");
    RequireEqual(matched.Relocations[0].InstructionEnd, instructionEnd, name + " instruction end");
    RequireEqual(matched.Relocations[0].SiteTarget, siteTarget, name + " site target");
}

const Case ripSha256Reload{"Matcher_Sha256RipOperand_ReloadsThroughRelocatedMovdqu", [] {
    const auto sha256 = match({0x0F, 0x38, 0xCC, 0x15, 0x78, 0x56, 0x34, 0x12});
    Require(sha256.has_value(), "SHA256MSG1 RIP-relative form was not matched");
    RequireEqual(slice(sha256->StubBody, 13, 21), hex(Bytes{0xF3, 0x0F, 0x6F, 0x0D, 0x00, 0x00, 0x00, 0x00}), "RIP-relative reload");
    requireRelocation(*sha256, 17, 21, 8 + 0x12345678, "SHA-256 RIP-relative operand");
}};

const Case ripPrefixedReload{"Matcher_PrefixedRipOperand_KeepsSegmentAndAddressSizePrefixes", [] {
    const auto segmentOverride = match({0x65, 0x67, 0x0F, 0x38, 0xCC, 0x15, 0x78, 0x56, 0x34, 0x12});
    Require(segmentOverride.has_value(), "Prefixed SHA256MSG1 was not matched");
    RequireEqual(slice(segmentOverride->StubBody, 13, 23), hex(Bytes{0x65, 0x67, 0xF3, 0x0F, 0x6F, 0x0D, 0x00, 0x00, 0x00, 0x00}), "Prefixed reload");
    requireRelocation(*segmentOverride, 19, 23, 10 + 0x12345678, "Prefixed RIP-relative operand");
}};

const Case ripSha1Rounds{"Matcher_Sha1Rnds4RipOperand_CountsImmediateInRelocation", [] {
    const auto sha1 = match({0x0F, 0x3A, 0xCC, 0x15, 0xF0, 0xFF, 0xFF, 0xFF, 0x03});
    Require(sha1.has_value(), "SHA1RNDS4 RIP-relative form was not matched");
    RequireEqual(slice(sha1->StubBody, 13, 21), hex(Bytes{0xF3, 0x0F, 0x6F, 0x05, 0x00, 0x00, 0x00, 0x00}), "RIP-relative reload");
    requireRelocation(*sha1, 17, 21, 9 - 16, "SHA1RNDS4 RIP-relative operand");
}};

const Case ripMovedLea{"Matcher_ExtrqWithRipLeaFollower_CopiesLeaWithRelocation", [] {
    const Bytes extrq = {0x66, 0x0F, 0x79, 0xCA};
    const Bytes followers = {0x90, 0x48, 0x8D, 0x05, 0x10, 0x00, 0x00, 0x00};
    const auto moved = Codegen::MakeAmd64OnlyInstructionMatcher()->Match(extrq.data(), extrq.size(), followers);
    Require(moved.has_value(), "EXTRQ with followers was not matched");
    Require(moved->ReturnBranchOffset >= followers.size(), "EXTRQ with followers was not lowered through a stub");
    const auto start = moved->ReturnBranchOffset - followers.size();
    RequireEqual(slice(moved->StubBody, start, moved->ReturnBranchOffset), hex(Bytes{0x90, 0x48, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00}), "Moved followers");
    requireRelocation(*moved, start + 4, start + 8, 4 + 8 + 0x10, "Moved RIP-relative LEA");
}};

const Case ripSequence{"Matcher_RipOperandInSequence_IsRelativeToSiteStart", [] {
    const Bytes extrq = {0x66, 0x0F, 0x79, 0xCA};
    const Bytes message = {0x0F, 0x38, 0xCC, 0x15, 0x78, 0x56, 0x34, 0x12};
    const std::vector<std::span<const std::uint8_t>> sequence = {extrq, message};
    const auto pair = Codegen::MakeAmd64OnlyInstructionMatcher()->MatchSequence(sequence, {});
    Require(pair.has_value(), "Sequence was not matched");
    RequireEqual(pair->Relocations.size(), std::size_t{1}, "Relocation count");
    RequireEqual(pair->Relocations[0].SiteTarget, std::int64_t{4 + 8 + 0x12345678}, "Site target");
}};

const std::vector<Codegen::StubRelocation> kStubRelocations = {{2, 6, 0x1040}};

const Case relocationForward{"StubRelocations_StubBelowSite_ResolvesAgainstSiteAddress", [] {
    Bytes body(8, 0xCC);
    Codegen::ApplyStubRelocations(body, kStubRelocations, 0x401000, 0x400F00, 0);
    RequireEqual(read<std::int32_t>(body, 2), 0x401000 + 0x1040 - (0x400F00 + 6), "Displacement");
    RequireEqual(static_cast<int>(body[0]), 0xCC, "Byte before the displacement");
    RequireEqual(static_cast<int>(body[6]), 0xCC, "Byte after the displacement");
}};

const Case relocationBackward{"StubRelocations_StubAboveSite_ResolvesBackwards", [] {
    Bytes body(8, 0xCC);
    Codegen::ApplyStubRelocations(body, kStubRelocations, 0x1000, 0x7FFF0000, 0);
    RequireEqual(read<std::int32_t>(body, 2), 0x1000 + 0x1040 - (0x7FFF0000 + 6), "Displacement");
}};

const Case relocationInvalid{"StubRelocations_UnencodableRelocation_RejectsStub", [] {
    const std::vector<std::tuple<std::vector<Codegen::StubRelocation>, std::uint64_t, std::uint64_t, const char*>> cases = {
        {kStubRelocations, 0x1000, 0x1000 + 0x90000000ull, "RIP-relative operand beyond rel32 range"},
        {{{6, 10, 0}}, 0x1000, 0x2000, "Stub relocation outside the body"},
        {{{4, 6, 0}}, 0x1000, 0x2000, "Stub relocation narrower than a displacement"}};
    for (const auto& [relocations, site, stub, name] : cases) {
        Bytes body(8, 0xCC);
        RequireThrows<Codegen::CodegenException>([&] { Codegen::ApplyStubRelocations(body, relocations, site, stub, 0); }, name);
    }
}};

const Case segmentClassifies{"Converter_MixedSegment_ClassifiesAmd64OnlyInstructions", [] {
    const auto result = convert(segmentFixture(), 20);
    RequireEqual(result.ReplacedCount, std::size_t{1}, "Replaced count");
    RequireEqual(result.Reports.size(), std::size_t{2}, "Report count");
    RequireEqual(result.Trampolines.size(), std::size_t{1}, "Trampoline count");
}};

const Case segmentTrampolineSite{"Converter_MixedSegment_RecordsTrampolineSite", [] {
    const auto result = convert(segmentFixture(), 20);
    Require(!result.Trampolines.empty(), "No trampoline was recorded");
    const auto& site = result.Trampolines[0];
    RequireEqual(site.Offset, Domain::FileByteOffset{0x208}, "Offset");
    RequireEqual(site.Address, Domain::VirtualAddress{0x1008}, "Address");
    RequireEqual(site.Length, std::size_t{7}, "Length");
    RequireEqual(hex(site.OriginalBytes), hex(kInsertqHighSite), "Original bytes");
    RequireEqual(hex(site.Body), hex(kInsertqHighBody), "Body");
    RequireEqual(site.ReturnBranchOffset, std::size_t{15}, "Return branch offset");
}};

void requireReport(const Codegen::Amd64OnlySubstitutionReport& report, const std::string& name, const Domain::FileByteOffset offset, const Lowering lowering, const std::size_t replacementLength) {
    RequireEqual(report.InstructionName, name, "Report instruction name");
    RequireEqual(report.Offset, offset, name + " report offset");
    RequireEqual(report.Lowering, lowering, name + " report lowering");
    RequireEqual(report.ReplacementLength, replacementLength, name + " report replacement length");
}

const Case segmentReports{"Converter_MixedSegment_ReportsTrampolineAndInPlaceSites", [] {
    const auto result = convert(segmentFixture(), 20);
    RequireEqual(result.Reports.size(), std::size_t{2}, "Report count");
    requireReport(result.Reports[0], "INSERTQ", 0x208, Lowering::Trampoline, 48);
    requireReport(result.Reports[1], "MOVNTSS", 0x20F, Lowering::InPlace, 4);
}};

const Case segmentBytes{"Converter_MixedSegment_RewritesOnlyMovntssOpcode", [] {
    const auto file = segmentFixture();
    auto expected = file;
    expected[0x211] = 0x11;
    Require(convert(file, 20).Bytes == expected, "Converter changed bytes other than the MOVNTSS opcode");
}};

const Case segmentUntouched{"Converter_SegmentWithoutAmd64Only_IsUnchanged", [] {
    const auto untouched = convert(Bytes(0x300, 0x90), 0x100);
    RequireEqual(untouched.ReplacedCount, std::size_t{0}, "Replaced count");
    Require(untouched.Trampolines.empty(), "Trampolines were recorded");
    Require(untouched.Reports.empty(), "Reports were recorded");
    Require(untouched.Bytes == Bytes(0x300, 0x90), "Bytes were changed");
}};

const Case segmentBranchInside{"Converter_BranchIntoAmd64OnlyInstruction_RejectsSegment", [] {
    auto branchInside = segmentFixture();
    branchInside[0x207] = 0x02;
    RequireThrows<Codegen::CodegenException>([&] { (void)convert(branchInside, 20); }, "Branch into an AMD-only instruction");
}};

const Case segmentRdpru{"Converter_Rdpru_RejectsSegment", [] {
    auto rdpru = segmentFixture();
    rdpru[0x20F] = 0x0F;
    rdpru[0x210] = 0x01;
    rdpru[0x211] = 0xFD;
    rdpru[0x212] = 0x90;
    RequireThrows<Codegen::CodegenException>([&] { (void)convert(rdpru, 20); }, "RDPRU was silently kept");
}};

Bytes shortExtrqFixture() {
    auto registerForm = segmentFixture();
    const Bytes extrqRegister = {0x66, 0x0F, 0x79, 0xCA};
    std::copy(extrqRegister.begin(), extrqRegister.end(), registerForm.begin() + 0x20F);
    return registerForm;
}

const Case segmentShortExtrqBeforeReturn{"Converter_ShortExtrqBeforeReturn_RejectsSegment", [] {
    const auto registerForm = shortExtrqFixture();
    RequireThrows<Codegen::CodegenException>([&] { (void)convert(registerForm, 20); }, "Short EXTRQ followed by a return was relocated");
}};

const Case segmentShortExtrqAbsorbs{"Converter_ShortExtrqRegisterForm_AbsorbsFollowingInstruction", [] {
    auto registerForm = shortExtrqFixture();
    registerForm[0x213] = 0x90;
    const auto relocated = convert(registerForm, 20);
    RequireEqual(relocated.Trampolines.size(), std::size_t{2}, "Trampoline count");
    const auto& shortSite = relocated.Trampolines[1];
    RequireEqual(shortSite.Offset, Domain::FileByteOffset{0x20F}, "Offset");
    RequireEqual(shortSite.Length, std::size_t{5}, "Length");
    RequireEqual(hex(shortSite.OriginalBytes), hex(Bytes{0x66, 0x0F, 0x79, 0xCA, 0x90}), "Original bytes");
    RequireEqual(static_cast<int>(shortSite.Body[shortSite.ReturnBranchOffset - 1]), 0x90, "Absorbed instruction before the return jump");
    RequireEqual(static_cast<int>(shortSite.Body[shortSite.ReturnBranchOffset]), 0xE9, "Return jump opcode");
}};

const Case segmentBeyondFile{"Converter_SegmentBeyondFile_RejectsSegment", [] {
    const auto file = segmentFixture();
    RequireThrows<Codegen::CodegenException>([&] { (void)convert(file, 0x200); }, "Segment exceeding the file");
}};

const Case followerRipVectorLoad{"Converter_ExtrqFollowedByRipVectorLoad_MovesLoadWithRelocation", [] {
    for (const auto& following : kRipRelativeVectorLoads) {
        const auto [file, size] = extrqFollowedBy(following);
        const auto result = convert(file, size);
        const auto name = hex(following);
        RequireEqual(result.Trampolines.size(), std::size_t{1}, "Trampoline count with " + name);
        const auto& site = result.Trampolines[0];
        RequireEqual(site.Length, 4 + following.size(), "Site length with " + name);
        const auto info = describeInstruction(following);
        const auto start = site.ReturnBranchOffset - following.size();
        RequireEqual(site.Relocations.size(), std::size_t{1}, "Relocation count with " + name);
        RequireEqual(site.Relocations[0].DisplacementOffset, start + info.RipRelativeDispOffset, "Displacement offset with " + name);
        RequireEqual(site.Relocations[0].InstructionEnd, site.ReturnBranchOffset, "Instruction end with " + name);
        RequireEqual(site.Relocations[0].SiteTarget, static_cast<std::int64_t>(4 + following.size()) + 0x10, "Site target with " + name);
    }
}};

const Case followerRipBranch{"Converter_ExtrqFollowedByRipIndirectBranch_FailsAtSiteOffset", [] {
    for (const Bytes& following : {Bytes{0xFF, 0x15, 0x10, 0x00, 0x00, 0x00}, Bytes{0xFF, 0x25, 0x10, 0x00, 0x00, 0x00}}) {
        const auto [file, size] = extrqFollowedBy(following);
        const auto offset = failureOffset([&] { (void)convert(file, size); }, "RIP-relative indirect branch moved into an EXTRQ stub: " + hex(following));
        RequireEqual(offset, Domain::FileByteOffset{0x204}, "Failure offset with " + hex(following));
    }
}};

const Case followerRegisterVector{"Converter_ExtrqFollowedByRegisterVectorOperation_MovesOperation", [] {
    for (const auto& following : kRegisterVectorOperations) {
        const auto [file, size] = extrqFollowedBy(following);
        const auto result = convert(file, size);
        RequireEqual(result.Trampolines.size(), std::size_t{1}, "Trampoline count with " + hex(following));
        RequireEqual(result.Trampolines[0].Length, 4 + following.size(), "Site length with " + hex(following));
    }
}};

const Case sha256Sites{"Converter_Sha256Sites_LowerThroughStubsWithoutChangingBytes", [] {
    const auto file = fileWithText(kSha256Text);
    const auto result = convert(file, kSha256Text.size());
    RequireEqual(result.Trampolines.size(), std::size_t{2}, "Trampoline count");
    RequireEqual(result.Reports.size(), std::size_t{2}, "Report count");
    Require(result.Bytes == file, "Converted bytes changed");
}};

const Case sha256RoundsAbsorbs{"Converter_Sha256Rnds2_AbsorbsFollowingSha256Msg1", [] {
    const auto result = convertText(kSha256Text);
    RequireEqual(result.Trampolines.size(), std::size_t{2}, "Trampoline count");
    RequireEqual(result.Trampolines[0].Offset, Domain::FileByteOffset{0x200}, "Offset");
    RequireEqual(result.Trampolines[0].Length, std::size_t{8}, "Length");
    RequireEqual(result.Reports[0].InstructionName, std::string("SHA256RNDS2"), "Report name");
}};

const Case sha256MessageAbsorbs{"Converter_Sha256Msg2_AbsorbsFollowingInstruction", [] {
    const auto result = convertText(kSha256Text);
    RequireEqual(result.Trampolines.size(), std::size_t{2}, "Trampoline count");
    const auto& message = result.Trampolines[1];
    RequireEqual(message.Offset, Domain::FileByteOffset{0x20C}, "Offset");
    RequireEqual(message.Length, std::size_t{5}, "Length");
    RequireEqual(result.Reports[1].InstructionName, std::string("SHA256MSG2"), "Report name");
    RequireEqual(static_cast<int>(message.Body[message.ReturnBranchOffset - 1]), 0x90, "Absorbed instruction before the return jump");
    RequireEqual(static_cast<int>(message.Body[message.ReturnBranchOffset]), 0xE9, "Return jump opcode");
}};

const Case sha256BeforeReturn{"Converter_ShortSha256BeforeReturn_RejectsSegment", [] {
    auto beforeReturn = fileWithText(kSha256Text);
    beforeReturn[0x210] = 0xC3;
    RequireThrows<Codegen::CodegenException>([&] { (void)convert(beforeReturn, kSha256Text.size()); }, "Short SHA-256 instruction followed by a return was relocated");
}};

const Case sha256MemoryForm{"Converter_Sha256MemoryForm_LowersThroughStub", [] {
    auto memoryForm = fileWithText(kSha256Text);
    memoryForm[0x20F] = 0x28;
    const auto memoryResult = convert(memoryForm, kSha256Text.size());
    RequireEqual(memoryResult.Trampolines.size(), std::size_t{2}, "Trampoline count");
    RequireEqual(memoryResult.Reports[1].InstructionName, std::string("SHA256MSG2"), "Report name");
}};

const Case sha256RipForm{"Converter_Sha256RipForm_RecordsRelocation", [] {
    auto ripRelative = fileWithText(kSha256Text);
    const Bytes ripMessage = {0x0F, 0x38, 0xCD, 0x2D, 0x40, 0x00, 0x00, 0x00, 0xC3};
    std::copy(ripMessage.begin(), ripMessage.end(), ripRelative.begin() + 0x20C);
    const auto ripResult = convert(ripRelative, kSha256Text.size() + 3);
    RequireEqual(ripResult.Trampolines.size(), std::size_t{2}, "Trampoline count");
    Require(ripResult.Bytes == ripRelative, "Converted bytes changed");
    RequireEqual(ripResult.Reports[1].InstructionName, std::string("SHA256MSG2"), "Report name");
    const auto& ripSite = ripResult.Trampolines[1];
    RequireEqual(ripSite.Offset, Domain::FileByteOffset{0x20C}, "Offset");
    RequireEqual(ripSite.Length, std::size_t{8}, "Length");
    RequireEqual(ripSite.Relocations.size(), std::size_t{1}, "Relocation count");
    RequireEqual(ripSite.Relocations[0].SiteTarget, std::int64_t{8 + 0x40}, "Site target");
    Require(ripSite.Relocations[0].InstructionEnd <= ripSite.ReturnBranchOffset, "Relocation ends after the return branch");
}};

const Case sha1Sites{"Converter_Sha1Sites_LowerThroughStubsWithoutChangingBytes", [] {
    const auto file = fileWithText(kSha1Text);
    const auto result = convert(file, kSha1Text.size());
    RequireEqual(result.Trampolines.size(), std::size_t{3}, "Trampoline count");
    RequireEqual(result.Reports.size(), std::size_t{3}, "Report count");
    Require(result.Bytes == file, "Converted bytes changed");
}};

const Case sha1RoundsSite{"Converter_Sha1Rnds4_LowersAsOneSite", [] {
    const auto result = convertText(kSha1Text);
    RequireEqual(result.Trampolines.size(), std::size_t{3}, "Trampoline count");
    RequireEqual(result.Trampolines[0].Offset, Domain::FileByteOffset{0x200}, "Offset");
    RequireEqual(result.Trampolines[0].Length, std::size_t{5}, "Length");
    RequireEqual(result.Reports[0].InstructionName, std::string("SHA1RNDS4"), "Report name");
}};

const Case sha1NextAbsorbs{"Converter_Sha1Nexte_AbsorbsFollowingSha1Msg1", [] {
    const auto result = convertText(kSha1Text);
    RequireEqual(result.Trampolines.size(), std::size_t{3}, "Trampoline count");
    RequireEqual(result.Trampolines[1].Offset, Domain::FileByteOffset{0x205}, "Offset");
    RequireEqual(result.Trampolines[1].Length, std::size_t{8}, "Length");
    RequireEqual(result.Reports[1].InstructionName, std::string("SHA1NEXTE"), "Report name");
}};

const Case sha1MessageAbsorbs{"Converter_Sha1Msg2_AbsorbsFollowingInstruction", [] {
    const auto result = convertText(kSha1Text);
    RequireEqual(result.Trampolines.size(), std::size_t{3}, "Trampoline count");
    const auto& message = result.Trampolines[2];
    RequireEqual(message.Offset, Domain::FileByteOffset{0x211}, "Offset");
    RequireEqual(message.Length, std::size_t{5}, "Length");
    RequireEqual(result.Reports[2].InstructionName, std::string("SHA1MSG2"), "Report name");
    RequireEqual(static_cast<int>(message.Body[message.ReturnBranchOffset - 1]), 0x90, "Absorbed instruction before the return jump");
    RequireEqual(static_cast<int>(message.Body[message.ReturnBranchOffset]), 0xE9, "Return jump opcode");
}};

const Case sha1BeforeReturn{"Converter_ShortSha1BeforeReturn_RejectsSegment", [] {
    auto beforeReturn = fileWithText(kSha1Text);
    beforeReturn[0x215] = 0xC3;
    RequireThrows<Codegen::CodegenException>([&] { (void)convert(beforeReturn, kSha1Text.size()); }, "Short SHA-1 instruction followed by a return was relocated");
}};

const Case sha1MemoryForm{"Converter_Sha1MemoryForm_LowersThroughStub", [] {
    auto memoryForm = fileWithText(kSha1Text);
    memoryForm[0x214] = 0x28;
    const auto memoryResult = convert(memoryForm, kSha1Text.size());
    RequireEqual(memoryResult.Trampolines.size(), std::size_t{3}, "Trampoline count");
    RequireEqual(memoryResult.Reports[2].InstructionName, std::string("SHA1MSG2"), "Report name");
}};

const Case sha1RipForm{"Converter_Sha1RipForm_RecordsRelocation", [] {
    auto ripRelative = fileWithText(kSha1Text);
    const Bytes ripMessage = {0x0F, 0x38, 0xCA, 0x2D, 0xF0, 0xFF, 0xFF, 0xFF, 0xC3};
    std::copy(ripMessage.begin(), ripMessage.end(), ripRelative.begin() + 0x211);
    const auto ripResult = convert(ripRelative, kSha1Text.size() + 3);
    RequireEqual(ripResult.Trampolines.size(), std::size_t{3}, "Trampoline count");
    Require(ripResult.Bytes == ripRelative, "Converted bytes changed");
    RequireEqual(ripResult.Reports[2].InstructionName, std::string("SHA1MSG2"), "Report name");
    const auto& ripSite = ripResult.Trampolines[2];
    RequireEqual(ripSite.Offset, Domain::FileByteOffset{0x211}, "Offset");
    RequireEqual(ripSite.Length, std::size_t{8}, "Length");
    RequireEqual(ripSite.Relocations.size(), std::size_t{1}, "Relocation count");
    RequireEqual(ripSite.Relocations[0].SiteTarget, std::int64_t{8 - 16}, "Site target");
    Require(ripSite.Relocations[0].InstructionEnd <= ripSite.ReturnBranchOffset, "Relocation ends after the return branch");
}};

const Bytes kMonitorWaitText = {0x0F, 0x01, 0xFA, 0x0F, 0x01, 0xFB, 0x2E, 0x0F, 0x01, 0xFB, 0xC3};

const Case monitorWaitReports{"Converter_MonitorxAndMwaitx_ReplacedInPlace", [] {
    const auto result = convertText(kMonitorWaitText);
    RequireEqual(result.ReplacedCount, std::size_t{3}, "Replaced count");
    Require(result.Trampolines.empty(), "Trampolines were recorded");
    RequireEqual(result.Reports.size(), std::size_t{3}, "Report count");
    RequireEqual(result.Reports[0].InstructionName, std::string("MONITORX"), "First report name");
    RequireEqual(result.Reports[1].InstructionName, std::string("MWAITX"), "Second report name");
    requireReport(result.Reports[2], "MWAITX", 0x206, Lowering::InPlace, 4);
}};

const Case monitorWaitBytes{"Converter_MonitorxAndMwaitx_WriteNopAndPauseBytes", [] {
    const auto file = fileWithText(kMonitorWaitText);
    auto expected = file;
    const Bytes replaced = {0x0F, 0x1F, 0x00, 0xF3, 0x90, 0x90, 0xF3, 0x90, 0x66, 0x90, 0xC3};
    std::copy(replaced.begin(), replaced.end(), expected.begin() + 0x200);
    const auto result = convert(file, kMonitorWaitText.size());
    RequireEqual(slice(result.Bytes, 0x200, 0x200 + replaced.size()), hex(replaced), "Replaced text");
    Require(result.Bytes == expected, "Bytes outside the text changed");
}};

const Case clzeroSites{"Converter_ClzeroSites_LowerThroughStubsWithoutChangingBytes", [] {
    const auto file = fileWithText(kClzeroText);
    const auto result = convert(file, kClzeroText.size());
    RequireEqual(result.Trampolines.size(), std::size_t{2}, "Trampoline count");
    RequireEqual(result.Reports.size(), std::size_t{2}, "Report count");
    Require(result.Bytes == file, "Converted bytes changed");
}};

const Case clzeroAbsorbs{"Converter_Clzero_AbsorbsFollowingInstruction", [] {
    const auto result = convertText(kClzeroText);
    RequireEqual(result.Trampolines.size(), std::size_t{2}, "Trampoline count");
    const auto& advance = result.Trampolines[0];
    RequireEqual(advance.Offset, Domain::FileByteOffset{0x200}, "Offset");
    RequireEqual(advance.Length, std::size_t{7}, "Length");
    RequireEqual(result.Reports[0].InstructionName, std::string("CLZERO"), "Report name");
    const Bytes add = {0x48, 0x83, 0xC0, 0x40};
    RequireEqual(slice(advance.Body, advance.ReturnBranchOffset - add.size(), advance.ReturnBranchOffset), hex(add), "Absorbed instruction before the return jump");
}};

const Case clzeroConsecutive{"Converter_ConsecutiveClzero_ShareOneStub", [] {
    const auto result = convertText(kClzeroText);
    RequireEqual(result.Trampolines.size(), std::size_t{2}, "Trampoline count");
    const auto& pair = result.Trampolines[1];
    RequireEqual(pair.Offset, Domain::FileByteOffset{0x207}, "Offset");
    RequireEqual(pair.Length, std::size_t{6}, "Length");
    RequireEqual(result.Reports[1].InstructionName, std::string("CLZERO"), "Report name");
}};

const Case clzeroBeforeReturn{"Converter_ShortClzeroBeforeReturn_RejectsSegment", [] {
    auto beforeReturn = fileWithText(kClzeroText);
    beforeReturn[0x203] = 0xC3;
    RequireThrows<Codegen::CodegenException>([&] { (void)convert(beforeReturn, kClzeroText.size()); }, "Short CLZERO followed by a return was relocated");
}};

const Case clzeroFsRelative{"Converter_FsRelativeClzero_FailsAtSiteOffset", [] {
    auto segmentRelative = fileWithText(kClzeroText);
    const Bytes fsClzero = {0x64, 0x0F, 0x01, 0xFC};
    std::copy(fsClzero.begin(), fsClzero.end(), segmentRelative.begin() + 0x20A);
    const auto offset = failureOffset([&] { (void)convert(segmentRelative, kClzeroText.size()); }, "FS-relative CLZERO was accepted");
    RequireEqual(offset, Domain::FileByteOffset{0x20A}, "Failure offset");
}};

const Case reciprocalAbsorbs{"Converter_Vrsqrtps_AbsorbsFollowingVmulps", [] {
    const auto file = fileWithText(kReciprocalText);
    const auto result = convert(file, kReciprocalText.size());
    RequireEqual(result.Trampolines.size(), std::size_t{1}, "Trampoline count");
    Require(result.Bytes == file, "Converted bytes changed");
    const auto& site = result.Trampolines[0];
    RequireEqual(site.Offset, Domain::FileByteOffset{0x200}, "Offset");
    RequireEqual(site.Length, std::size_t{8}, "Length");
    RequireEqual(result.Reports[0].InstructionName, std::string("VRSQRTPS"), "Report name");
    RequireEqual(static_cast<int>(site.Body[site.ReturnBranchOffset - 4]), 0xC5, "First byte of the absorbed VMULPS");
    RequireEqual(static_cast<int>(site.Body[site.ReturnBranchOffset - 1]), 0xD1, "Last byte of the absorbed VMULPS");
}};

const Case reciprocalKept{"Converter_VrcppsBeforeReturn_IsKeptNative", [] {
    const auto result = convertText(kReciprocalText);
    RequireEqual(result.KeptCount, std::size_t{1}, "Kept count");
    RequireEqual(result.Reports.size(), std::size_t{2}, "Report count");
    RequireEqual(result.Reports[1].InstructionName, std::string("VRCPPS"), "Report name");
    RequireEqual(result.Reports[1].Lowering, Lowering::Kept, "Lowering");
    RequireEqual(result.Reports[1].Offset, Domain::FileByteOffset{0x208}, "Offset");
}};

const Case reciprocalBranchTo{"Converter_BranchToReciprocal_StillLowersThroughStub", [] {
    const auto branched = convertText({0xEB, 0x00, 0xC5, 0xF8, 0x52, 0xD5, 0xC5, 0xE8, 0x59, 0xD1, 0xC3});
    RequireEqual(branched.Trampolines.size(), std::size_t{1}, "Trampoline count");
    RequireEqual(branched.KeptCount, std::size_t{0}, "Kept count");
}};

const Case reciprocalBranchInside{"Converter_BranchIntoAbsorbedInstruction_KeepsReciprocalNative", [] {
    const auto file = fileWithText({0xEB, 0x04, 0xC5, 0xF8, 0x52, 0xD5, 0xC5, 0xE8, 0x59, 0xD1, 0xC3});
    const auto inside = convert(file, 11);
    Require(inside.Trampolines.empty(), "A stub was recorded");
    RequireEqual(inside.KeptCount, std::size_t{1}, "Kept count");
    Require(inside.Bytes == file, "Converted bytes changed");
}};

const Case reciprocalRipFollower{"Converter_VrsqrtpsFollowedByRipLoad_RelocatesLoad", [] {
    const Bytes load = {0xC5, 0xF8, 0x52, 0xD5, 0x8B, 0x05, 0x10, 0x00, 0x00, 0x00, 0xC3};
    const auto file = fileWithText(load);
    const auto moved = convert(file, load.size());
    RequireEqual(moved.Trampolines.size(), std::size_t{1}, "Trampoline count");
    RequireEqual(moved.KeptCount, std::size_t{0}, "Kept count");
    Require(moved.Bytes == file, "Converted bytes changed");
    const auto& movedSite = moved.Trampolines[0];
    RequireEqual(movedSite.Length, std::size_t{10}, "Length");
    RequireEqual(movedSite.Relocations.size(), std::size_t{1}, "Relocation count");
    RequireEqual(movedSite.Relocations[0].DisplacementOffset, movedSite.ReturnBranchOffset - 4, "Displacement offset");
    RequireEqual(movedSite.Relocations[0].InstructionEnd, movedSite.ReturnBranchOffset, "Instruction end");
    RequireEqual(movedSite.Relocations[0].SiteTarget, std::int64_t{10 + 0x10}, "Site target");
}};

const Case strayRexRejected{"Converter_ClzeroWithFsOrLockAfterStrayRex_FailsAtSiteOffset", [] {
    for (const Bytes& text : {Bytes{0x48, 0x64, 0x0F, 0x01, 0xFC, 0x90, 0x90, 0xC3}, Bytes{0x48, 0xF0, 0x0F, 0x01, 0xFC, 0x90, 0x90, 0xC3}}) {
        const auto offset = failureOffset([&] { (void)convertText(text); }, "CLZERO with a FS or LOCK prefix after a stray REX: " + hex(text));
        RequireEqual(offset, Domain::FileByteOffset{0x200}, "Failure offset of " + hex(text));
    }
}};

const Case strayRexAddressSize{"Converter_AddressSizeClzeroAfterStrayRex_LowersAsOneInstruction", [] {
    const auto addressSize32 = convertText({0x48, 0x67, 0x0F, 0x01, 0xFC, 0xC3});
    RequireEqual(addressSize32.Trampolines.size(), std::size_t{1}, "Trampoline count");
    RequireEqual(addressSize32.Trampolines[0].Length, std::size_t{5}, "Length");
    RequireEqual(addressSize32.Reports[0].InstructionName, std::string("CLZERO"), "Report name");
}};

const Case strayRexMovntss{"Converter_MovntssAfterStrayRex_RewritesOpcode", [] {
    const Bytes text = {0x48, 0xF3, 0x0F, 0x2B, 0x00, 0xC3};
    auto expected = fileWithText(text);
    expected[0x203] = 0x11;
    const auto movnts = convertText(text);
    RequireEqual(movnts.ReplacedCount, std::size_t{1}, "Replaced count");
    RequireEqual(movnts.Reports[0].InstructionName, std::string("MOVNTSS"), "Report name");
    Require(movnts.Bytes == expected, "MOVNTSS after a stray REX was not rewritten");
}};

const Case strayRexFollower{"Converter_FollowerWithStrayRex_AbsorbedWithPrefixes", [] {
    for (const Bytes& following : {Bytes{0x48, 0x64, 0x8B, 0x00}, Bytes{0x48, 0x67, 0x8B, 0x00}, Bytes{0x48, 0xF0, 0x01, 0x00}}) {
        Bytes text = {0x0F, 0x01, 0xFC};
        text.insert(text.end(), following.begin(), following.end());
        text.push_back(0xC3);
        const auto result = convertText(text);
        RequireEqual(result.Trampolines.size(), std::size_t{1}, "Trampoline count with " + hex(following));
        RequireEqual(result.Trampolines[0].Length, std::size_t{7}, "Length with " + hex(following));
        const auto& body = result.Trampolines[0].Body;
        const auto returnBranch = result.Trampolines[0].ReturnBranchOffset;
        RequireEqual(slice(body, returnBranch - following.size(), returnBranch), hex(following), "Absorbed instruction");
    }
}};

const Case rewriterStrayRex{"Rewriter_BranchWithStrayRex_AdjustedForLengthChange", [] {
    const Bytes code = {0x48, 0x2E, 0xE9, 0x01, 0x00, 0x00, 0x00, 0x90, 0xC3};
    const auto rewritten = Codegen::X64InstructionRewriter{}.Rewrite(code, {7, {0x66, 0x90}});
    RequireEqual(hex(rewritten.Bytes), hex(Bytes{0x48, 0x2E, 0xE9, 0x02, 0x00, 0x00, 0x00, 0x66, 0x90, 0xC3}), "Rewritten code");
}};

const Case rewriterReferenceSites{"Rewriter_ReferencesAcrossLengthChange_AdjustOnlyDisplacements", [] {
    const std::vector<std::tuple<Bytes, std::uint64_t, Bytes>> cases = {
        {{0x90, 0x48, 0xB8, 0x05, 0xF0, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xC3}, 0,
         {0x66, 0x90, 0x48, 0xB8, 0x05, 0xF0, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xC3}},
        {{0xC5, 0xF9, 0x6F, 0x05, 0x01, 0x00, 0x00, 0x00, 0x90, 0xC3}, 8,
         {0xC5, 0xF9, 0x6F, 0x05, 0x02, 0x00, 0x00, 0x00, 0x66, 0x90, 0xC3}},
        {{0x66, 0x0F, 0x38, 0x00, 0x05, 0x01, 0x00, 0x00, 0x00, 0x90, 0xC3}, 9,
         {0x66, 0x0F, 0x38, 0x00, 0x05, 0x02, 0x00, 0x00, 0x00, 0x66, 0x90, 0xC3}},
        {{0xE2, 0x01, 0x90, 0xC3}, 2, {0xE2, 0x02, 0x66, 0x90, 0xC3}}};
    for (const auto& [code, offset, expected] : cases)
        RequireEqual(hex(Codegen::X64InstructionRewriter{}.Rewrite(code, {offset, {0x66, 0x90}}).Bytes), hex(expected), "Rewrite of " + hex(code));
}};

const Case failureUndecodable{"Converter_UndecodableInstruction_FailsAtInstructionOffset", [] {
    auto undecodable = segmentFixture();
    const Bytes bareSse4a = {0x0F, 0x78, 0xC0, 0x00};
    std::copy(bareSse4a.begin(), bareSse4a.end(), undecodable.begin() + 0x20F);
    RequireEqual(failureOffset([&] { (void)convert(undecodable, 20); }, "Undecodable instruction was accepted"), Domain::FileByteOffset{0x20F}, "Failure offset");
}};

const Case failureInsertqMemory{"Converter_InsertqMemoryForm_FailsAtSiteOffset", [] {
    auto memoryForm = segmentFixture();
    memoryForm[0x20C] = 0x08;
    RequireEqual(failureOffset([&] { (void)convert(memoryForm, 20); }, "INSERTQ memory form was accepted"), Domain::FileByteOffset{0x208}, "Failure offset");
}};

const Case failureMovntssRegister{"Converter_MovntssRegisterForm_FailsAtSiteOffset", [] {
    auto movntsRegister = segmentFixture();
    movntsRegister[0x212] = 0xC1;
    RequireEqual(failureOffset([&] { (void)convert(movntsRegister, 20); }, "MOVNTSS register form was accepted"), Domain::FileByteOffset{0x20F}, "Failure offset");
}};

const Case failureUnsupported{"Converter_UnsupportedInstruction_FailsAtSiteOffset", [] {
    auto rdpru = segmentFixture();
    const Bytes rdpruBytes = {0x0F, 0x01, 0xFD, 0x90};
    std::copy(rdpruBytes.begin(), rdpruBytes.end(), rdpru.begin() + 0x20F);
    RequireEqual(failureOffset([&] { (void)convert(rdpru, 20); }, "RDPRU was accepted"), Domain::FileByteOffset{0x20F}, "Failure offset");
}};

const Case linuxSiteJump{"LinuxElfPatcher_Sse4aSite_ReplacedByJump", [] {
    const auto placement = placeLinuxSse4aSite();
    RequireEqual(static_cast<int>(placement.Output[0x202]), 0xE9, "Jump opcode");
    RequireEqual(static_cast<int>(placement.Output[0x207]), 0x90, "Padding after the jump");
}};

const Case linuxStubPlacement{"LinuxElfPatcher_Sse4aSite_PlacesAlignedStubOutsideImage", [] {
    const auto target = linuxSse4aStubAddress(placeLinuxSse4aSite().Output);
    RequireEqual(target % 16, std::int64_t{0}, "Stub alignment");
    Require(target > 0x2100, "Linux stub is inside the original image");
}};

const Case linuxStubBody{"LinuxElfPatcher_Sse4aSite_StubReturnsPastSite", [] {
    const auto placement = placeLinuxSse4aSite();
    const auto target = linuxSse4aStubAddress(placement.Output);
    const auto bodyOffset = executableFileOffset(placement.Output, static_cast<std::uint64_t>(target));
    auto expectedBody = kInsertqSelfBody;
    write<std::int32_t>(expectedBody, 10, static_cast<std::int32_t>(0x1008 - (target + 9 + 5)));
    RequireEqual(slice(placement.Output, bodyOffset, bodyOffset + expectedBody.size()), hex(expectedBody), "Linux stub body");
}};

const Case linuxChangedSite{"LinuxElfPatcher_ChangedSiteBytes_RejectsPatch", [] {
    const auto source = elfFixture({0xEB, 0x06, 0xF2, 0x0F, 0x78, 0xDB, 0x08, 0x08, 0xC3});
    const auto headers = elfHeaders();
    const auto converted = Codegen::MakeAmd64OnlyConverter()->Convert(source, {headers[0]});
    auto altered = converted.Bytes;
    altered[0x205] = 0xDC;
    auto patcher = linuxPatcher();
    RequireThrows<Domain::RelinkerException>([&] { (void)patcher.Patch(altered, headers, {}, 0, "$ORIGIN/libs", true, false, converted.Trampolines); }, "Changed Linux site bytes were accepted");
}};

const Case linuxRipRelative{"LinuxElfPatcher_RipRelativeSite_RelocatesOperandToOriginalAddress", [] {
    Bytes text = {0x0F, 0x38, 0xCC, 0x15, 0x00, 0x00, 0x00, 0x00, 0xC3};
    write<std::int32_t>(text, 4, 0x2040 - 0x1008);
    const auto source = elfFixture(text);
    const auto headers = elfHeaders();
    const auto converted = Codegen::MakeAmd64OnlyConverter()->Convert(source, {headers[0]});
    RequireEqual(converted.Trampolines.size(), std::size_t{1}, "Trampoline count");
    RequireEqual(converted.Trampolines[0].Relocations.size(), std::size_t{1}, "Relocation count");
    auto patcher = linuxPatcher();
    const auto output = patcher.Patch(converted.Bytes, headers, {}, 0, "$ORIGIN/libs", true, false, converted.Trampolines);
    RequireEqual(static_cast<int>(output[0x200]), 0xE9, "Jump opcode");
    const auto stub = 0x1000 + 5 + static_cast<std::int64_t>(read<std::int32_t>(output, 0x201));
    const auto& relocation = converted.Trampolines[0].Relocations[0];
    const auto bodyOffset = executableFileOffset(output, static_cast<std::uint64_t>(stub));
    const auto operand = stub + static_cast<std::int64_t>(relocation.InstructionEnd) + read<std::int32_t>(output, bodyOffset + relocation.DisplacementOffset);
    RequireEqual(operand, std::int64_t{0x2040}, "Relocated operand address");
}};

const Case scannerZeroTail{"InstructionScanner_OddZeroPaddingAtTail_EndsScan", [] {
    const Bytes code{0xC3, 0x00, 0x00, 0x00};
    RequireEqual(Codegen::MakeInstructionScanner()->ScanCodeSection(code, 0, code.size()).size(), std::size_t{2}, "Scanned instruction count");
}};

const Case scannerTruncatedTail{"InstructionScanner_TruncatedNonZeroTail_RejectsSection", [] {
    const Bytes truncated{0xC3, 0x0F};
    RequireThrows<Codegen::CodegenException>([&] { (void)Codegen::MakeInstructionScanner()->ScanCodeSection(truncated, 0, truncated.size()); }, "Truncated non-zero tail");
}};

#if defined(__linux__) && defined(__x86_64__)
std::uint64_t extrqReference(std::uint64_t value, std::uint64_t control) {
    const auto length = static_cast<unsigned>(control & 0x3f);
    const auto index = static_cast<unsigned>((control >> 8) & 0x3f);
    const auto shifted = value >> index;
    return length == 0 ? shifted : shifted & ((std::uint64_t{1} << length) - 1);
}

std::uint64_t insertqReference(std::uint64_t destination, std::uint64_t value, std::uint64_t control) {
    const auto length = static_cast<unsigned>(control & 0x3f);
    const auto index = static_cast<unsigned>((control >> 8) & 0x3f);
    const auto mask = length == 0 ? ~std::uint64_t{0} : ((std::uint64_t{1} << length) - 1);
    return (destination & ~(mask << index)) | ((value & mask) << index);
}

constexpr std::uint64_t kStubScratch[2] = {0x0123456789abcdefull, 0xfedcba9876543210ull};

std::uint32_t rotr(const std::uint32_t value, const unsigned count) {
    return (value >> count) | (value << (32 - count));
}

std::array<std::uint64_t, 2> sha256Reference(const std::uint8_t opcode, const std::uint64_t (&first)[2], const std::uint64_t (&second)[2], const std::uint64_t (&keys)[2]) {
    std::uint32_t a[4];
    std::uint32_t b[4];
    std::uint32_t k[4];
    std::uint32_t r[4];
    std::memcpy(a, first, sizeof(a));
    std::memcpy(b, second, sizeof(b));
    std::memcpy(k, keys, sizeof(k));
    const auto sigma0 = [](const std::uint32_t w) { return rotr(w, 7) ^ rotr(w, 18) ^ (w >> 3); };
    const auto sigma1 = [](const std::uint32_t w) { return rotr(w, 17) ^ rotr(w, 19) ^ (w >> 10); };
    if (opcode == 0xCC) {
        for (int lane = 0; lane < 3; ++lane) r[lane] = a[lane] + sigma0(a[lane + 1]);
        r[3] = a[3] + sigma0(b[0]);
    } else if (opcode == 0xCD) {
        r[0] = a[0] + sigma1(b[2]);
        r[1] = a[1] + sigma1(b[3]);
        r[2] = a[2] + sigma1(r[0]);
        r[3] = a[3] + sigma1(r[1]);
    } else {
        std::uint32_t sa = b[3], sb = b[2], sc = a[3], sd = a[2], se = b[1], sf = b[0], sg = a[1], sh = a[0];
        for (int round = 0; round < 2; ++round) {
            const auto t1 = sh + (rotr(se, 6) ^ rotr(se, 11) ^ rotr(se, 25)) + ((se & sf) ^ (~se & sg)) + k[round];
            const auto t2 = (rotr(sa, 2) ^ rotr(sa, 13) ^ rotr(sa, 22)) + ((sa & sb) ^ (sa & sc) ^ (sb & sc));
            sh = sg; sg = sf; sf = se; se = sd + t1; sd = sc; sc = sb; sb = sa; sa = t1 + t2;
        }
        r[0] = sf;
        r[1] = se;
        r[2] = sb;
        r[3] = sa;
    }
    std::array<std::uint64_t, 2> result{};
    std::memcpy(result.data(), r, sizeof(r));
    return result;
}

std::array<std::uint64_t, 2> runRegisterFormStub(const Bytes& site, const std::uint64_t (&destination)[2], const std::uint64_t (&source)[2]) {
    const auto matched = match(site);
    Require(matched.has_value() && matched->Lowering != Lowering::Unsupported, "Register form stub was not produced for " + hex(site));
    auto body = matched->Lowering == Lowering::InPlace ? matched->ReplacementBytes : matched->StubBody;
    const auto ret = body.size();
    body.push_back(0xC3);
    if (matched->Lowering == Lowering::Trampoline) {
        const auto displacement = static_cast<std::int32_t>(ret - (matched->ReturnBranchOffset + 5));
        std::memcpy(body.data() + matched->ReturnBranchOffset + 1, &displacement, sizeof(displacement));
    }
    void* code = mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(code != MAP_FAILED, "cannot map executable memory for the stub");
    std::memcpy(code, body.data(), body.size());
    alignas(16) std::uint64_t destinationIn[2] = {destination[0], destination[1]};
    alignas(16) std::uint64_t sourceIn[2] = {source[0], source[1]};
    alignas(16) std::uint64_t out[2] = {};
    alignas(16) std::uint64_t scratchIn[2] = {kStubScratch[0], kStubScratch[1]};
    alignas(16) std::uint64_t scratchOut[2] = {};
    alignas(16) std::uint64_t spareIn[2] = {kStubScratch[1], kStubScratch[0]};
    alignas(16) std::uint64_t spareOut[2] = {};
    asm volatile(
        "movdqu (%[scratch]), %%xmm0\n\t"
        "movdqu (%[spare]), %%xmm1\n\t"
        "movdqu (%[dst]), %%xmm2\n\t"
        "movdqu (%[ctl]), %%xmm5\n\t"
        "mov %[ctl], %%rcx\n\t"
        "mov %[ctl], %%r12\n\t"
        "mov $16, %%eax\n\t"
        "sub $128, %%rsp\n\t"
        "movdqu %%xmm5, 0x10(%%rsp)\n\t"
        "call *%[code]\n\t"
        "add $128, %%rsp\n\t"
        "movdqu %%xmm2, (%[out])\n\t"
        "movdqu %%xmm0, (%[scratchOut])\n\t"
        "movdqu %%xmm1, (%[spareOut])\n\t"
        :
        : [scratch] "r"(scratchIn), [spare] "r"(spareIn), [dst] "r"(destinationIn), [ctl] "r"(sourceIn), [code] "r"(code), [out] "r"(out), [scratchOut] "r"(scratchOut), [spareOut] "r"(spareOut)
        : "rax", "rcx", "r12", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "memory", "cc");
    munmap(code, 4096);
    Require(scratchOut[0] == scratchIn[0] && scratchOut[1] == scratchIn[1], "Register form stub clobbered a scratch register: " + hex(site));
    Require(spareOut[0] == spareIn[0] && spareOut[1] == spareIn[1], "Stub clobbered xmm1: " + hex(site));
    return {out[0], out[1]};
}

std::uint64_t packLanes(const float (&lanes)[4], const std::size_t first) {
    std::uint64_t packed = 0;
    std::memcpy(&packed, lanes + first, sizeof(packed));
    return packed;
}

bool sameLane(const float expected, const float actual) {
    if (std::isnan(expected))
        return std::isnan(actual);
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    std::memcpy(&a, &expected, sizeof(a));
    std::memcpy(&b, &actual, sizeof(b));
    return a == b;
}

struct StubRun {
    std::uint64_t Rax;
    std::uint64_t Rcx;
    std::uint64_t Flags;
    std::uint64_t Xmm[16][2];
    std::uint64_t SiteAddress;
};

StubRun runStubBody(const Codegen::Amd64OnlyMatch& matched, const std::uint64_t rax, const std::uint64_t (&xmmIn)[16][2], const Bytes& data = {}) {
    RequireEqual(matched.Lowering, Lowering::Trampoline, "Stub lowering");
    auto body = matched.StubBody;
    const auto ret = body.size();
    body.push_back(0xC3);
    const auto displacement = static_cast<std::int32_t>(ret - (matched.ReturnBranchOffset + 5));
    std::memcpy(body.data() + matched.ReturnBranchOffset + 1, &displacement, sizeof(displacement));
    Require(body.size() <= 4096 && data.size() <= 4096, "Stub or its data does not fit in a page");
    auto* code = static_cast<std::uint8_t*>(mmap(nullptr, 3 * 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(code != MAP_FAILED, "cannot map executable memory for the stub");
    StubRun run{};
    run.SiteAddress = reinterpret_cast<std::uint64_t>(code) + 4096;
    Codegen::ApplyStubRelocations(std::span<std::uint8_t>(body).first(matched.ReturnBranchOffset), matched.Relocations, run.SiteAddress, reinterpret_cast<std::uint64_t>(code), 0);
    std::memcpy(code, body.data(), body.size());
    std::copy(data.begin(), data.end(), code + 2 * 4096);
    asm volatile(
        "movdqu 0x00(%[xmmIn]), %%xmm0\n\t"
        "movdqu 0x10(%[xmmIn]), %%xmm1\n\t"
        "movdqu 0x20(%[xmmIn]), %%xmm2\n\t"
        "movdqu 0x30(%[xmmIn]), %%xmm3\n\t"
        "movdqu 0x40(%[xmmIn]), %%xmm4\n\t"
        "movdqu 0x50(%[xmmIn]), %%xmm5\n\t"
        "movdqu 0x60(%[xmmIn]), %%xmm6\n\t"
        "movdqu 0x70(%[xmmIn]), %%xmm7\n\t"
        "movdqu 0x80(%[xmmIn]), %%xmm8\n\t"
        "movdqu 0x90(%[xmmIn]), %%xmm9\n\t"
        "movdqu 0xa0(%[xmmIn]), %%xmm10\n\t"
        "movdqu 0xb0(%[xmmIn]), %%xmm11\n\t"
        "movdqu 0xc0(%[xmmIn]), %%xmm12\n\t"
        "movdqu 0xd0(%[xmmIn]), %%xmm13\n\t"
        "movdqu 0xe0(%[xmmIn]), %%xmm14\n\t"
        "movdqu 0xf0(%[xmmIn]), %%xmm15\n\t"
        "mov %[raxIn], %%rax\n\t"
        "movabs $0x1122334455667788, %%rcx\n\t"
        "sub $128, %%rsp\n\t"
        "pushq $0x8D7\n\t"
        "popfq\n\t"
        "call *%[code]\n\t"
        "pushfq\n\t"
        "popq %[flags]\n\t"
        "add $128, %%rsp\n\t"
        "mov %%rax, %[raxOut]\n\t"
        "mov %%rcx, %[rcxOut]\n\t"
        "movdqu %%xmm0, 0x00(%[xmmOut])\n\t"
        "movdqu %%xmm1, 0x10(%[xmmOut])\n\t"
        "movdqu %%xmm2, 0x20(%[xmmOut])\n\t"
        "movdqu %%xmm3, 0x30(%[xmmOut])\n\t"
        "movdqu %%xmm4, 0x40(%[xmmOut])\n\t"
        "movdqu %%xmm5, 0x50(%[xmmOut])\n\t"
        "movdqu %%xmm6, 0x60(%[xmmOut])\n\t"
        "movdqu %%xmm7, 0x70(%[xmmOut])\n\t"
        "movdqu %%xmm8, 0x80(%[xmmOut])\n\t"
        "movdqu %%xmm9, 0x90(%[xmmOut])\n\t"
        "movdqu %%xmm10, 0xa0(%[xmmOut])\n\t"
        "movdqu %%xmm11, 0xb0(%[xmmOut])\n\t"
        "movdqu %%xmm12, 0xc0(%[xmmOut])\n\t"
        "movdqu %%xmm13, 0xd0(%[xmmOut])\n\t"
        "movdqu %%xmm14, 0xe0(%[xmmOut])\n\t"
        "movdqu %%xmm15, 0xf0(%[xmmOut])\n\t"
        : [flags] "=&r"(run.Flags), [raxOut] "=&r"(run.Rax), [rcxOut] "=&r"(run.Rcx)
        : [xmmIn] "r"(xmmIn), [xmmOut] "r"(run.Xmm), [raxIn] "r"(rax), [code] "r"(code)
        : "rax", "rcx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15", "memory", "cc");
    munmap(code, 3 * 4096);
    return run;
}

std::uint32_t rotl(const std::uint32_t value, const unsigned count) {
    return (value << count) | (value >> (32 - count));
}

std::array<std::uint32_t, 4> sha1Reference(const Codegen::Sha1Operands& operands, const std::uint32_t (&x)[4], const std::uint32_t (&y)[4]) {
    switch (operands.Operation) {
    case Codegen::Sha1Operation::Nexte:
        return {y[0], y[1], y[2], y[3] + rotl(x[3], 30)};
    case Codegen::Sha1Operation::Msg1:
        return {x[0] ^ y[2], x[1] ^ y[3], x[2] ^ x[0], x[3] ^ x[1]};
    case Codegen::Sha1Operation::Msg2: {
        const auto w16 = rotl(x[3] ^ y[2], 1);
        return {rotl(x[0] ^ w16, 1), rotl(x[1] ^ y[0], 1), rotl(x[2] ^ y[1], 1), w16};
    }
    case Codegen::Sha1Operation::Rnds4:
        break;
    }
    constexpr std::uint32_t keys[4] = {0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6};
    std::uint32_t a = x[3], b = x[2], c = x[1], d = x[0], e = 0;
    for (int round = 0; round < 4; ++round) {
        std::uint32_t f = b ^ c ^ d;
        if (operands.Function == 0) f = (b & c) ^ (~b & d);
        if (operands.Function == 2) f = (b & c) ^ (b & d) ^ (c & d);
        const auto next = f + rotl(a, 5) + y[3 - round] + e + keys[operands.Function];
        e = d; d = c; c = rotl(b, 30); b = a; a = next;
    }
    return {d, c, b, a};
}

struct RipExecutionFixture {
    std::uint64_t XmmIn[16][2];
    std::uint64_t Words[2] = {0x510e527f9b05688cull, 0x1f83d9ab5be0cd19ull};
    Bytes Data = Bytes(0x40, 0xA5);
    std::uint64_t Rax = 0x5A5A5A5A5A5A5A5Aull;
};

RipExecutionFixture ripExecutionFixture() {
    RipExecutionFixture fixture{};
    for (unsigned reg = 0; reg < 16; ++reg) {
        fixture.XmmIn[reg][0] = 0x9e3779b97f4a7c15ull * (reg + 1);
        fixture.XmmIn[reg][1] = 0xc2b2ae3d27d4eb4full * (reg + 3);
    }
    std::memcpy(fixture.Data.data() + 0x20, fixture.Words, sizeof(fixture.Words));
    return fixture;
}

void requireUnchangedExcept(const RipExecutionFixture& fixture, const StubRun& run, const std::uint32_t changed, const std::string& name) {
    for (unsigned reg = 0; reg < 16; ++reg)
        if ((changed & (1u << reg)) == 0)
            Require(run.Xmm[reg][0] == fixture.XmmIn[reg][0] && run.Xmm[reg][1] == fixture.XmmIn[reg][1], name + " clobbered xmm" + std::to_string(reg));
    RequireEqual(run.Rcx, 0x1122334455667788ull, name + " rcx");
    RequireEqual(run.Flags & 0x8D5, std::uint64_t{0x8D7 & 0x8D5}, name + " status flags");
}

const Case executionReciprocal{"Execution_ReciprocalStubs_AreCorrectlyRounded", [] {
    const float inputs[][4] = {
        {1.0f, 4.0f, 0.25f, 2.0f},
        {0.0f, -0.0f, INFINITY, -1.0f},
        {1.00000012f, 0.99999994f, 3.0e-38f, 1.0e30f}};
    for (const auto& lanes : inputs) {
        const std::uint64_t source[2] = {packLanes(lanes, 0), packLanes(lanes, 2)};
        const std::uint64_t destination[2] = {0x1111111111111111ull, 0x2222222222222222ull};
        for (const bool squareRoot : {true, false}) {
            const std::uint8_t opcode = squareRoot ? 0x52 : 0x53;
            const auto distinctOut = runRegisterFormStub({0xC5, 0xF8, opcode, 0xD5}, destination, source);
            const auto sameOut = runRegisterFormStub({0xC5, 0xF8, opcode, 0xD2}, source, source);
            float distinctLanes[4];
            float sameLanes[4];
            std::memcpy(distinctLanes, distinctOut.data(), sizeof(distinctLanes));
            std::memcpy(sameLanes, sameOut.data(), sizeof(sameLanes));
            for (std::size_t lane = 0; lane < 4; ++lane) {
                const float expected = squareRoot ? 1.0f / std::sqrt(lanes[lane]) : 1.0f / lanes[lane];
                const auto input = std::string(squareRoot ? "VRSQRTPS" : "VRCPPS") + " of " + std::to_string(lanes[lane]);
                Require(sameLane(expected, distinctLanes[lane]), input + " is not correctly rounded");
                Require(sameLane(expected, sameLanes[lane]), input + " with equal operands is not correctly rounded");
            }
        }
    }
}};

const Case executionRsqrtOne{"Execution_RsqrtOfOne_IsExactlyOne", [] {
    const float one[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    const std::uint64_t ones[2] = {packLanes(one, 0), packLanes(one, 2)};
    const auto unit = runRegisterFormStub({0xC5, 0xF8, 0x52, 0xD5}, ones, ones);
    RequireEqual(unit[0], ones[0], "Low lanes");
    RequireEqual(unit[1], ones[1], "High lanes");
}};

const Case executionRegisterForm{"Execution_Sse4aRegisterFormStubs_ComputeField", [] {
    const Bytes extrqDistinct = {0x66, 0x0F, 0x79, 0xD5};
    const Bytes extrqSame = {0x66, 0x0F, 0x79, 0xD2};
    const Bytes insertqDistinct = {0xF2, 0x0F, 0x79, 0xD5};
    const Bytes insertqSame = {0xF2, 0x0F, 0x79, 0xD2};
    const std::uint64_t value = 0x9e3779b97f4a7c15ull;
    const std::uint64_t destination = 0x0f1e2d3c4b5a6978ull;
    for (const auto [length, index] : {std::pair{8u, 4u}, {0u, 0u}, {40u, 20u}, {63u, 1u}, {1u, 63u}, {16u, 48u}, {1u, 0u}, {32u, 32u}}) {
        const auto field = " length " + std::to_string(length) + " index " + std::to_string(index);
        const auto control = static_cast<std::uint64_t>(length) | (static_cast<std::uint64_t>(index) << 8) | 0xffffc000ull;
        Require(runRegisterFormStub(extrqDistinct, {value, 0x1122334455667788ull}, {control, 0}) == std::array<std::uint64_t, 2>{extrqReference(value, control), 0}, "EXTRQ register form" + field);
        Require(runRegisterFormStub(extrqSame, {control, 0x1122334455667788ull}, {control, 0x1122334455667788ull}) == std::array<std::uint64_t, 2>{extrqReference(control, control), 0}, "EXTRQ register form with equal operands" + field);
        const auto insertqControl = control | 0xC0ull;
        RequireEqual(runRegisterFormStub(insertqDistinct, {destination, 0x1122334455667788ull}, {value, insertqControl})[0], insertqReference(destination, value, insertqControl), "INSERTQ register form" + field);
        RequireEqual(runRegisterFormStub(insertqSame, {value, insertqControl}, {value, insertqControl})[0], insertqReference(value, value, insertqControl), "INSERTQ register form with equal operands" + field);
    }
}};

const Case executionImmediateForm{"Execution_Sse4aImmediateFormStubs_ComputeField", [] {
    const std::uint64_t value = 0x9e3779b97f4a7c15ull;
    const std::uint64_t destination = 0x0f1e2d3c4b5a6978ull;
    const std::uint64_t high = 0x1122334455667788ull;
    for (const auto [length, index] : {std::pair{0u, 0u}, {24u, 40u}, {8u, 0u}, {16u, 0u}, {32u, 0u}, {8u, 40u}, {5u, 3u}, {63u, 1u}, {1u, 63u}}) {
        const auto field = " length " + std::to_string(length) + " index " + std::to_string(index);
        const auto control = static_cast<std::uint64_t>(length) | (static_cast<std::uint64_t>(index) << 8);
        const auto length8 = static_cast<std::uint8_t>(length);
        const auto index8 = static_cast<std::uint8_t>(index);
        Require(runRegisterFormStub({0x66, 0x0F, 0x78, 0xC2, length8, index8}, {value, high}, {0, 0}) == std::array<std::uint64_t, 2>{extrqReference(value, control), 0}, "EXTRQ immediate form" + field);
        Require(runRegisterFormStub({0xF2, 0x0F, 0x78, 0xD5, length8, index8}, {destination, high}, {value, high}) == std::array<std::uint64_t, 2>{insertqReference(destination, value, control), 0}, "INSERTQ immediate form" + field);
        Require(runRegisterFormStub({0xF2, 0x0F, 0x78, 0xD2, length8, index8}, {value, high}, {value, high}) == std::array<std::uint64_t, 2>{insertqReference(value, value, control), 0}, "INSERTQ immediate form with equal operands" + field);
    }
}};

const Case executionSha256{"Execution_Sha256Stubs_MatchReference", [] {
    const std::uint64_t state[2] = {0x6a09e667bb67ae85ull, 0x3c6ef372a54ff53aull};
    const std::uint64_t words[2] = {0x510e527f9b05688cull, 0x1f83d9ab5be0cd19ull};
    for (const std::uint8_t opcode : {std::uint8_t{0xCB}, std::uint8_t{0xCC}, std::uint8_t{0xCD}}) {
        const Bytes distinct = {0x0F, 0x38, opcode, 0xD5};
        const Bytes same = {0x0F, 0x38, opcode, 0xD2};
        Require(runRegisterFormStub(distinct, state, words) == sha256Reference(opcode, state, words, kStubScratch), "SHA-256 stub " + hex(distinct));
        Require(runRegisterFormStub(same, state, state) == sha256Reference(opcode, state, state, kStubScratch), "SHA-256 stub with equal operands " + hex(same));
        for (const auto& memory : {Bytes{0x0F, 0x38, opcode, 0x11}, Bytes{0x0F, 0x38, opcode, 0x54, 0x24, 0x18}, Bytes{0x0F, 0x38, opcode, 0x54, 0x04, 0x08}, Bytes{0x2E, 0x41, 0x0F, 0x38, opcode, 0x14, 0x24}, Bytes{0x0F, 0x38, opcode, 0x91, 0x00, 0x00, 0x00, 0x00}})
            Require(runRegisterFormStub(memory, state, words) == sha256Reference(opcode, state, words, kStubScratch), "SHA-256 memory form stub " + hex(memory));
    }
}};

const Case executionClzero{"Execution_ClzeroStubs_ClearExactlyAddressedLine", [] {
    const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher();
    const Bytes plain = {0x0F, 0x01, 0xFC};
    const Bytes addressSize32 = {0x67, 0x0F, 0x01, 0xFC};
    const Bytes add = {0x48, 0x83, 0xC0, 0x40};
    const std::vector<std::span<const std::uint8_t>> pair = {plain, plain};
    struct ClzeroCase {
        std::optional<Codegen::Amd64OnlyMatch> Match;
        bool Low;
        std::uint64_t Junk;
        std::uint64_t Advance;
        const char* Name;
    };
    const std::vector<ClzeroCase> cases = {
        {matcher->Match(plain.data(), plain.size()), false, 0, 0, "CLZERO stub"},
        {matcher->Match(addressSize32.data(), addressSize32.size()), true, 0x5A5A5A5A00000000ull, 0, "67h CLZERO stub"},
        {matcher->Match(plain.data(), plain.size(), add), false, 0, 64, "CLZERO stub with a trailing add"},
        {matcher->MatchSequence(pair, {}), false, 0, 0, "CLZERO sequence stub"}};
    std::uint64_t xmmIn[16][2];
    for (unsigned reg = 0; reg < 16; ++reg) {
        xmmIn[reg][0] = 0x0101010101010101ull * (reg + 1);
        xmmIn[reg][1] = ~xmmIn[reg][0];
    }
    for (const auto& item : cases) {
        const std::string name = item.Name;
        Require(item.Match.has_value(), name + " was not produced");
        auto* buffer = static_cast<std::uint8_t*>(mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | (item.Low ? MAP_32BIT : 0), -1, 0));
        Require(buffer != MAP_FAILED, "cannot map the CLZERO buffer");
        for (const std::size_t offset : {std::size_t{0x80}, std::size_t{0xAD}, std::size_t{0xFF}}) {
            std::memset(buffer, 0xA5, 4096);
            const auto address = reinterpret_cast<std::uint64_t>(buffer + offset);
            const auto run = runStubBody(*item.Match, address | item.Junk, xmmIn);
            const auto line = offset & ~std::size_t{63};
            const auto at = name + " at offset " + std::to_string(offset);
            for (std::size_t index = 0; index < 4096; ++index)
                RequireEqual(static_cast<int>(buffer[index]), index >= line && index < line + 64 ? 0x00 : 0xA5, at + " byte " + std::to_string(index));
            RequireEqual(run.Rax, (address | item.Junk) + item.Advance, at + " rax");
            RequireEqual(run.Rcx, 0x1122334455667788ull, at + " rcx");
            for (unsigned reg = 0; reg < 16; ++reg)
                Require(run.Xmm[reg][0] == xmmIn[reg][0] && run.Xmm[reg][1] == xmmIn[reg][1], at + " clobbered xmm" + std::to_string(reg));
            if (item.Advance == 0)
                RequireEqual(run.Flags & 0x8D5, std::uint64_t{0x8D7 & 0x8D5}, at + " status flags");
        }
        munmap(buffer, 4096);
    }
}};

const Case executionSha1{"Execution_Sha1Stubs_MatchReferenceAndPreserveState", [] {
    const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher();
    std::uint64_t seed = 0x9e3779b97f4a7c15ull;
    const auto random = [&] { seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; return seed; };
    const std::vector<Bytes> sites = {
        {0x0F, 0x3A, 0xCC, 0xD5, 0x00}, {0x0F, 0x3A, 0xCC, 0xD5, 0x01}, {0x0F, 0x3A, 0xCC, 0xD5, 0x02}, {0x0F, 0x3A, 0xCC, 0xD5, 0xFF},
        {0x0F, 0x3A, 0xCC, 0xD2, 0x02}, {0x45, 0x0F, 0x3A, 0xCC, 0xCE, 0x00}, {0x41, 0x0F, 0x3A, 0xCC, 0xC0, 0x01},
        {0x0F, 0x38, 0xC8, 0xD5}, {0x0F, 0x38, 0xC8, 0xD2}, {0x44, 0x0F, 0x38, 0xC8, 0xC7},
        {0x0F, 0x38, 0xC9, 0xD5}, {0x0F, 0x38, 0xC9, 0xD2}, {0x41, 0x0F, 0x38, 0xC9, 0xC7},
        {0x0F, 0x38, 0xCA, 0xD5}, {0x0F, 0x38, 0xCA, 0xD2}, {0x45, 0x0F, 0x38, 0xCA, 0xFF},
        {0x0F, 0x3A, 0xCC, 0x10, 0x02}, {0x44, 0x0F, 0x3A, 0xCC, 0x40, 0x10, 0x01}, {0x0F, 0x3A, 0xCC, 0x80, 0x10, 0x00, 0x00, 0x00, 0x00}, {0x0F, 0x3A, 0xCC, 0x44, 0x20, 0x10, 0xFF},
        {0x0F, 0x38, 0xC8, 0x00}, {0x0F, 0x38, 0xC9, 0x48, 0x10}, {0x44, 0x0F, 0x38, 0xCA, 0xB8, 0x10, 0x00, 0x00, 0x00}};
    for (const auto& site : sites) {
        const auto name = hex(site);
        const auto operands = Codegen::DecodeSha1(site.data(), site.size());
        const auto matched = matcher->Match(site.data(), site.size());
        Require(matched.has_value(), "SHA-1 stub was not produced for " + name);
        for (int sample = 0; sample < 64; ++sample) {
            std::uint64_t xmmIn[16][2];
            for (auto& reg : xmmIn) {
                reg[0] = random();
                reg[1] = random();
            }
            alignas(16) const std::uint64_t memory[4] = {random(), random(), random(), random()};
            const auto rax = operands.Memory ? reinterpret_cast<std::uint64_t>(memory) : 0x5A5A5A5A5A5A5A5Aull;
            std::uint32_t x[4];
            std::uint32_t y[4];
            std::memcpy(x, xmmIn[operands.Destination], sizeof(x));
            if (operands.Memory)
                std::memcpy(y, reinterpret_cast<const std::uint8_t*>(memory) + operands.Memory->Displacement, sizeof(y));
            else
                std::memcpy(y, xmmIn[operands.Source], sizeof(y));
            const auto expected = sha1Reference(operands, x, y);
            const auto run = runStubBody(*matched, rax, xmmIn);
            for (unsigned reg = 0; reg < 16; ++reg) {
                if (reg == operands.Destination)
                    Require(std::memcmp(run.Xmm[reg], expected.data(), 16) == 0, "SHA-1 stub computed the wrong result for " + name);
                else
                    Require(run.Xmm[reg][0] == xmmIn[reg][0] && run.Xmm[reg][1] == xmmIn[reg][1], "SHA-1 stub clobbered xmm" + std::to_string(reg) + " for " + name);
            }
            RequireEqual(run.Rax, rax, "rax after " + name);
            RequireEqual(run.Rcx, 0x1122334455667788ull, "rcx after " + name);
            RequireEqual(run.Flags & 0x8D5, std::uint64_t{0x8D7 & 0x8D5}, "Status flags after " + name);
        }
    }
}};

const Case executionRipSha256{"Execution_Sha256RipStubs_ReadOriginalOperand", [] {
    const auto fixture = ripExecutionFixture();
    for (const std::uint8_t opcode : {std::uint8_t{0xCB}, std::uint8_t{0xCC}, std::uint8_t{0xCD}}) {
        Bytes site = {0x0F, 0x38, opcode, 0x15, 0x00, 0x00, 0x00, 0x00};
        write<std::int32_t>(site, 4, 4096 + 0x20 - 8);
        const auto name = "SHA-256 RIP-relative stub " + hex(site);
        const auto matched = match(site);
        Require(matched.has_value(), name + " was not produced");
        RequireEqual(matched->Relocations.size(), std::size_t{1}, name + " relocation count");
        const auto run = runStubBody(*matched, fixture.Rax, fixture.XmmIn, fixture.Data);
        Require(std::array<std::uint64_t, 2>{run.Xmm[2][0], run.Xmm[2][1]} == sha256Reference(opcode, fixture.XmmIn[2], fixture.Words, fixture.XmmIn[0]), name + " did not read the original operand");
        RequireEqual(run.Rax, fixture.Rax, name + " rax");
        requireUnchangedExcept(fixture, run, 1u << 2, name);
    }
}};

const Case executionRipSha1{"Execution_Sha1Rnds4RipStub_ReadsOriginalOperand", [] {
    const auto fixture = ripExecutionFixture();
    Bytes rounds = {0x0F, 0x3A, 0xCC, 0x15, 0x00, 0x00, 0x00, 0x00, 0x02};
    write<std::int32_t>(rounds, 4, 4096 + 0x20 - 9);
    const auto roundsMatch = match(rounds);
    Require(roundsMatch.has_value(), "SHA1RNDS4 RIP-relative stub was not produced");
    RequireEqual(roundsMatch->Relocations.size(), std::size_t{1}, "Relocation count");
    std::uint32_t x[4];
    std::uint32_t y[4];
    std::memcpy(x, fixture.XmmIn[2], sizeof(x));
    std::memcpy(y, fixture.Words, sizeof(y));
    const auto expected = sha1Reference(Codegen::DecodeSha1(rounds.data(), rounds.size()), x, y);
    const auto roundsRun = runStubBody(*roundsMatch, fixture.Rax, fixture.XmmIn, fixture.Data);
    Require(std::memcmp(roundsRun.Xmm[2], expected.data(), 16) == 0, "SHA1RNDS4 RIP-relative stub did not read the original operand");
    requireUnchangedExcept(fixture, roundsRun, 1u << 2, "SHA1RNDS4 RIP-relative stub");
}};

const Case executionRipFollowers{"Execution_MovedRipFollowers_UseOriginalAddresses", [] {
    const auto fixture = ripExecutionFixture();
    const Bytes extrq = {0x66, 0x0F, 0x79, 0xCA};
    Bytes followers = {0xF3, 0x0F, 0x6F, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00};
    write<std::int32_t>(followers, 4, 4096 + 0x20 - 12);
    write<std::int32_t>(followers, 11, 4096 + 0x30 - 19);
    const auto moved = Codegen::MakeAmd64OnlyInstructionMatcher()->Match(extrq.data(), extrq.size(), followers);
    Require(moved.has_value(), "EXTRQ with RIP-relative followers was not lowered");
    RequireEqual(moved->Relocations.size(), std::size_t{2}, "Relocation count");
    const auto movedRun = runStubBody(*moved, fixture.Rax, fixture.XmmIn, fixture.Data);
    RequireEqual(movedRun.Xmm[3][0], fixture.Words[0], "Moved MOVDQU low qword");
    RequireEqual(movedRun.Xmm[3][1], fixture.Words[1], "Moved MOVDQU high qword");
    RequireEqual(movedRun.Rax, movedRun.SiteAddress + 4096 + 0x30, "Moved LEA address");
    requireUnchangedExcept(fixture, movedRun, (1u << 1) | (1u << 3), "Stub with moved RIP-relative instructions");
}};
#else
void skipNativeExecution() {
    Testing::Skip("executing lowered stubs needs Linux x86-64 executable mappings");
}

const Case executionReciprocal{"Execution_ReciprocalStubs_AreCorrectlyRounded", skipNativeExecution};
const Case executionRsqrtOne{"Execution_RsqrtOfOne_IsExactlyOne", skipNativeExecution};
const Case executionRegisterForm{"Execution_Sse4aRegisterFormStubs_ComputeField", skipNativeExecution};
const Case executionImmediateForm{"Execution_Sse4aImmediateFormStubs_ComputeField", skipNativeExecution};
const Case executionSha256{"Execution_Sha256Stubs_MatchReference", skipNativeExecution};
const Case executionClzero{"Execution_ClzeroStubs_ClearExactlyAddressedLine", skipNativeExecution};
const Case executionSha1{"Execution_Sha1Stubs_MatchReferenceAndPreserveState", skipNativeExecution};
const Case executionRipSha256{"Execution_Sha256RipStubs_ReadOriginalOperand", skipNativeExecution};
const Case executionRipSha1{"Execution_Sha1Rnds4RipStub_ReadsOriginalOperand", skipNativeExecution};
const Case executionRipFollowers{"Execution_MovedRipFollowers_UseOriginalAddresses", skipNativeExecution};
#endif

} // namespace
