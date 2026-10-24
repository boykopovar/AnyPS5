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
#include <codegen/x86/WideStoreOperands.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>
#include <codegen/x86/DecodedInstruction.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/x86/X64InstructionRewriter.hpp>
#include <codegen/IInstructionScanner.hpp>
#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <elfpatcher/general/ProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/general/SectionHeaderTableBuilder.hpp>
#include <elfpatcher/general/SegmentFilter.hpp>
#include <elfpatcher/linux/LinuxElfPatcher.hpp>
#include <io/ByteWriter.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#if defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#endif
#include <functional>
#include <optional>
#include <span>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

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

const Bytes kClzeroBody = {
    0x48, 0x8D, 0xA4, 0x24, 0x70, 0xFF, 0xFF, 0xFF, 0xF3, 0x0F, 0x7F, 0x04, 0x24, 0x51, 0x66, 0x48,
    0x0F, 0x6E, 0xC0, 0x66, 0x0F, 0xDB, 0x05, 0x35, 0x00, 0x00, 0x00, 0x66, 0x48, 0x0F, 0x7E, 0xC1,
    0x66, 0x0F, 0xEF, 0xC0, 0x66, 0x0F, 0xE7, 0x01, 0x66, 0x0F, 0xE7, 0x41, 0x10, 0x66, 0x0F, 0xE7,
    0x41, 0x20, 0x66, 0x0F, 0xE7, 0x41, 0x30, 0x59, 0xF3, 0x0F, 0x6F, 0x04, 0x24, 0x48, 0x8D, 0xA4,
    0x24, 0x90, 0x00, 0x00, 0x00, 0xE9, 0x00, 0x00, 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
    0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void decoderLengths() {
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
        {0x48, 0x64, 0x8B, 0x00}, {0x48, 0xF3, 0x0F, 0x2B, 0x00}, {0x48, 0x67, 0x0F, 0x01, 0xFC}, {0x48, 0x48, 0x0F, 0x01, 0xFC},
        {0x8F, 0xE8, 0x40, 0xA3, 0xC1, 0x70}, {0x8F, 0xE8, 0xF8, 0xA3, 0x4A, 0x10, 0x60}, {0x8F, 0xE8, 0xC0, 0xA3, 0xBA, 0x70, 0xFF, 0xFF, 0xFF, 0x30},
        {0x8F, 0xE9, 0x78, 0x90, 0xC1}, {0x8F, 0xEA, 0x78, 0x10, 0xC0, 1, 2, 3, 4},
        {0x8F, 0xC0}, {0x41, 0x8F, 0x07}, {0x8F, 0x05, 1, 2, 3, 4}};
    Bytes padded;
    for (const auto& instruction : instructions) {
        padded = instruction;
        padded.insert(padded.end(), 8, 0x90);
        require(decoder.Decode(padded.data(), padded.size()) == instruction.size(), "AMD-only or repaired two-byte opcode was decoded with the wrong length");
    }
    requireFailure([&] { const Bytes bare = {0x0F, 0x78, 0xC3, 0x08, 0x28}; (void)decoder.Decode(bare.data(), bare.size()); }, "0F 78 without an SSE4a prefix was accepted");
    const Bytes strayRex = {0x48, 0x64, 0x8B, 0x00, 0x90};
    const auto stray = decoder.DecodeInstruction(strayRex.data(), strayRex.size());
    require(stray.Length == 4 && stray.OpcodeOffset == 2 && stray.RexPrefix == 0 && stray.SegmentPrefix == 0x64, "A REX before a legacy prefix was kept");
    const Bytes lastRex = {0x41, 0x48, 0x8B, 0x00, 0x90};
    const auto last = decoder.DecodeInstruction(lastRex.data(), lastRex.size());
    require(last.Length == 4 && last.OpcodeOffset == 2 && last.RexPrefix == 0x48, "The REX before the opcode was not the one kept");
}

const std::vector<Bytes> kRipRelativeVectorLoads = {
    {0xC5, 0xF9, 0x6F, 0x05, 0x10, 0x00, 0x00, 0x00},
    {0xC4, 0xE2, 0x79, 0x00, 0x05, 0x10, 0x00, 0x00, 0x00},
    {0x62, 0xF1, 0xFD, 0x08, 0x6F, 0x05, 0x10, 0x00, 0x00, 0x00},
    {0x66, 0x0F, 0x38, 0x00, 0x05, 0x10, 0x00, 0x00, 0x00},
    {0x66, 0x0F, 0x3A, 0x0F, 0x05, 0x10, 0x00, 0x00, 0x00, 0x08},
    {0x8F, 0xE8, 0x78, 0xA3, 0x05, 0x10, 0x00, 0x00, 0x00, 0x30}};
const std::vector<Bytes> kRegisterVectorOperations = {
    {0xC5, 0xF9, 0x6F, 0xC1}, {0x66, 0x0F, 0x38, 0x00, 0xC1}, {0xC5, 0xF8, 0x77}, {0xC4, 0xE2, 0x79, 0x00, 0x00}};

void decoderRipRelative() {
    const Codegen::X64InstructionDecoder decoder;
    for (const auto& instruction : kRipRelativeVectorLoads) {
        const auto info = decoder.DecodeInstruction(instruction.data(), instruction.size());
        require(info.Length == instruction.size() && info.HasRipRelativeDisp && read<std::int32_t>(instruction, info.RipRelativeDispOffset) == 0x10, "VEX, EVEX, XOP or three-byte RIP-relative operand was not reported");
    }
    for (const auto& instruction : kRegisterVectorOperations)
        require(!decoder.DecodeInstruction(instruction.data(), instruction.size()).HasRipRelativeDisp, "Vector instruction without a RIP-relative operand was reported as RIP-relative");
    const Bytes ud1 = {0x0F, 0xB9, 0x05, 0x10, 0x00, 0x00, 0x00};
    const auto trap = decoder.DecodeInstruction(ud1.data(), ud1.size());
    require(trap.Length == ud1.size() && trap.FlowKind == Codegen::ControlFlowKind::Trap && trap.HasRipRelativeDisp && read<std::int32_t>(ud1, trap.RipRelativeDispOffset) == 0x10, "UD1 was not decoded as a trap with its RIP-relative operand");
}

void decoderTwoByteOpcodeLengths() {
    const Codegen::X64InstructionDecoder decoder;
    const std::vector<Bytes> instructions = {
        {0x0F, 0xA8}, {0x0F, 0xA9}, {0x0F, 0xAA}, {0x66, 0x0F, 0xA8}, {0x41, 0x0F, 0xA9},
        {0x0F, 0xAB, 0xC8}, {0x0F, 0xAB, 0x05, 0x10, 0x00, 0x00, 0x00}, {0x48, 0x0F, 0xAB, 0x44, 0x24, 0x08},
        {0x0F, 0x20, 0xC0}, {0x0F, 0x20, 0x05}, {0x0F, 0x22, 0x04}, {0x0F, 0x21, 0x45}, {0x0F, 0x23, 0x85},
        {0x66, 0x0F, 0x38, 0x20, 0x05, 0x10, 0x00, 0x00, 0x00}, {0x66, 0x0F, 0x38, 0x23, 0x44, 0x24, 0x08}};
    Bytes padded;
    for (const auto& instruction : instructions) {
        padded = instruction;
        padded.insert(padded.end(), 8, 0x90);
        require(decoder.Decode(padded.data(), padded.size()) == instruction.size(), "Two-byte opcode was decoded with operand bytes it does not have");
        require(decoder.DecodeInstruction(padded.data(), padded.size()).Length == instruction.size(), "Two-byte opcode was described with operand bytes it does not have");
    }
    const Bytes controlRegister = {0x0F, 0x20, 0x05, 0x90, 0x90, 0x90, 0x90};
    const auto control = decoder.DecodeInstruction(controlRegister.data(), controlRegister.size());
    require(control.Length == 3 && control.HasModRm && control.ModRmByte == 0x05 && !control.HasRipRelativeDisp, "MOV from a control register was reported as RIP-relative");
    const Bytes debugRegister = {0x0F, 0x23, 0x05, 0x90, 0x90, 0x90, 0x90};
    require(!decoder.DecodeInstruction(debugRegister.data(), debugRegister.size()).HasRipRelativeDisp, "MOV to a debug register was reported as RIP-relative");
    const Bytes extend = {0x66, 0x0F, 0x38, 0x20, 0x05, 0x10, 0x00, 0x00, 0x00};
    const auto extended = decoder.DecodeInstruction(extend.data(), extend.size());
    require(extended.HasRipRelativeDisp && read<std::int32_t>(extend, extended.RipRelativeDispOffset) == 0x10, "PMOVSXBW lost its RIP-relative operand");
    const Bytes bitTest = {0x0F, 0xAB, 0x05, 0x10, 0x00, 0x00, 0x00};
    const auto bts = decoder.DecodeInstruction(bitTest.data(), bitTest.size());
    require(bts.HasRipRelativeDisp && read<std::int32_t>(bitTest, bts.RipRelativeDispOffset) == 0x10, "BTS lost its RIP-relative operand");
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
    const Bytes strayRex = {0x41, 0xF2, 0x0F, 0x79, 0xCA};
    const auto ignored = Codegen::DecodeSse4a(strayRex.data(), strayRex.size());
    require(ignored.RegisterForm && ignored.Insertq && ignored.Destination == 1 && ignored.Source == 2, "A REX before a legacy prefix was applied");
    requireFailure([] { const Bytes bytes = {0x66, 0x0F, 0x78, 0xCB, 0x08, 0x28}; (void)Codegen::DecodeSse4a(bytes.data(), bytes.size()); }, "EXTRQ with a non-zero reg field was accepted");
    requireFailure([] { const Bytes bytes = {0xF2, 0x0F, 0x78, 0x1B, 0x08, 0x08}; (void)Codegen::DecodeSse4a(bytes.data(), bytes.size()); }, "SSE4a memory operand was accepted");
    requireFailure([] { const Bytes bytes = {0xF2, 0x0F, 0x78, 0xC8, 0x20, 0x30}; (void)Codegen::DecodeSse4a(bytes.data(), bytes.size()); }, "Field beyond bit 64 was accepted");
}

void sha256Operands() {
    const auto check = [](const Bytes& site, const Codegen::Sha256Operation operation, const int dst, const int src) {
        const auto operands = Codegen::DecodeSha256(site.data(), site.size());
        require(operands.Operation == operation && operands.Destination == dst && operands.Source == src, "SHA-256 operands were decoded incorrectly");
    };
    check({0x0F, 0x38, 0xCB, 0xCA}, Codegen::Sha256Operation::Rnds2, 1, 2);
    check({0x45, 0x0F, 0x38, 0xCC, 0xE1}, Codegen::Sha256Operation::Msg1, 12, 9);
    check({0x44, 0x0F, 0x38, 0xCD, 0xC0}, Codegen::Sha256Operation::Msg2, 8, 0);
    const Bytes memory = {0x65, 0x67, 0x44, 0x0F, 0x38, 0xCD, 0x54, 0x8C, 0xF0};
    const auto decoded = Codegen::DecodeSha256(memory.data(), memory.size());
    require(decoded.Operation == Codegen::Sha256Operation::Msg2 && decoded.Destination == 10 && decoded.Memory && decoded.Memory->Prefixes == Bytes{0x65, 0x67} && decoded.Memory->Mod == 1 && decoded.Memory->Rm == 4 && decoded.Memory->Sib == 0x8C && decoded.Memory->Displacement == -16 && decoded.Memory->StackBase, "SHA-256 memory operand was decoded incorrectly");
    const Bytes r12Base = {0x41, 0x0F, 0x38, 0xCC, 0x14, 0x24};
    require(!Codegen::DecodeSha256(r12Base.data(), r12Base.size()).Memory->StackBase, "R12 base was decoded as RSP");
    const Bytes absolute = {0x0F, 0x38, 0xCC, 0x14, 0x25, 0x78, 0x56, 0x34, 0x12};
    const auto noBase = Codegen::DecodeSha256(absolute.data(), absolute.size());
    require(!noBase.Memory->StackBase && noBase.Memory->Displacement == 0x12345678, "SIB without a base was decoded incorrectly");
    const Bytes ripRelative = {0x0F, 0x38, 0xCC, 0x15, 0x78, 0x56, 0x34, 0x12};
    const auto rip = Codegen::DecodeSha256(ripRelative.data(), ripRelative.size());
    require(rip.Destination == 2 && rip.Memory && rip.Memory->RipRelative && rip.Memory->Displacement == 0x12345678 && rip.Memory->EncodedSize == 5 && rip.Memory->InstructionLength == 8 && !noBase.Memory->RipRelative, "SHA-256 RIP-relative operand was decoded incorrectly");
    requireFailure([] { const Bytes bytes = {0x0F, 0x38, 0xCC, 0x15, 0, 0, 0, 0, 0x90}; (void)Codegen::DecodeSha256(bytes.data(), bytes.size()); }, "SHA-256 RIP-relative operand that does not end the instruction was accepted");
    requireFailure([] { const Bytes bytes = {0x0F, 0x38, 0xCC, 0x15, 0, 0, 0}; (void)Codegen::DecodeSha256(bytes.data(), bytes.size()); }, "Truncated SHA-256 RIP-relative operand was accepted");
    requireFailure([] { const Bytes bytes = {0x0F, 0x38, 0xCC, 0x54, 0x24}; (void)Codegen::DecodeSha256(bytes.data(), bytes.size()); }, "Truncated SHA-256 memory operand was accepted");
    requireFailure([] { const Bytes bytes = {0x66, 0x0F, 0x38, 0xCB, 0xCA}; (void)Codegen::DecodeSha256(bytes.data(), bytes.size()); }, "Prefixed 0F 38 CB was decoded as SHA-256");
    requireFailure([] { const Bytes bytes = {0x0F, 0x38, 0xC9, 0xCA}; (void)Codegen::DecodeSha256(bytes.data(), bytes.size()); }, "SHA-1 was decoded as SHA-256");
    check({0x41, 0x2E, 0x0F, 0x38, 0xCC, 0xCA}, Codegen::Sha256Operation::Msg1, 1, 2);
    check({0x2E, 0x41, 0x0F, 0x38, 0xCC, 0xCA}, Codegen::Sha256Operation::Msg1, 1, 10);
    const Bytes rounds = {0x0F, 0x38, 0xCB, 0xCA};
    require(Codegen::DecodedInstruction{rounds.data(), rounds.size()}.IsShaNi(), "SHA256RNDS2 is not recognised as SHA-NI");
}

void sha1Operands() {
    const auto check = [](const Bytes& site, const Codegen::Sha1Operation operation, const int dst, const int src, const int function) {
        const auto operands = Codegen::DecodeSha1(site.data(), site.size());
        require(operands.Operation == operation && operands.Destination == dst && operands.Source == src && operands.Function == function, "SHA-1 operands were decoded incorrectly");
    };
    check({0x0F, 0x3A, 0xCC, 0xCA, 0x02}, Codegen::Sha1Operation::Rnds4, 1, 2, 2);
    check({0x45, 0x0F, 0x3A, 0xCC, 0xE1, 0xFF}, Codegen::Sha1Operation::Rnds4, 12, 9, 3);
    check({0x44, 0x0F, 0x38, 0xC8, 0xC0}, Codegen::Sha1Operation::Nexte, 8, 0, 0);
    check({0x0F, 0x38, 0xC9, 0xD5}, Codegen::Sha1Operation::Msg1, 2, 5, 0);
    check({0x2E, 0x41, 0x0F, 0x38, 0xCA, 0xCA}, Codegen::Sha1Operation::Msg2, 1, 10, 0);
    const Bytes memory = {0x65, 0x67, 0x44, 0x0F, 0x3A, 0xCC, 0x54, 0x8C, 0xF0, 0xFE};
    const auto decoded = Codegen::DecodeSha1(memory.data(), memory.size());
    require(decoded.Operation == Codegen::Sha1Operation::Rnds4 && decoded.Destination == 10 && decoded.Function == 2 && decoded.Memory && decoded.Memory->Prefixes == Bytes{0x65, 0x67} && decoded.Memory->Mod == 1 && decoded.Memory->Rm == 4 && decoded.Memory->Sib == 0x8C && decoded.Memory->Displacement == -16 && decoded.Memory->StackBase, "SHA-1 memory operand was decoded incorrectly");
    const auto memoryFunction = [](const Bytes& site, const int dst, const std::int32_t displacement, const int function) {
        const auto operands = Codegen::DecodeSha1(site.data(), site.size());
        require(operands.Operation == Codegen::Sha1Operation::Rnds4 && operands.Destination == dst && operands.Memory && !operands.Memory->StackBase && operands.Memory->Displacement == displacement && operands.Function == function, "SHA1RNDS4 immediate was not read after its memory operand");
    };
    memoryFunction({0x0F, 0x3A, 0xCC, 0x08, 0x03}, 1, 0, 3);
    memoryFunction({0x0F, 0x3A, 0xCC, 0x50, 0x02, 0x01}, 2, 2, 1);
    memoryFunction({0x0F, 0x3A, 0xCC, 0x91, 0x78, 0x56, 0x34, 0x12, 0x01}, 2, 0x12345678, 1);
    memoryFunction({0x41, 0x0F, 0x3A, 0xCC, 0x14, 0x24, 0x02}, 2, 0, 2);
    memoryFunction({0x0F, 0x3A, 0xCC, 0x14, 0x25, 0x78, 0x56, 0x34, 0x12, 0x03}, 2, 0x12345678, 3);
    const Bytes message = {0x0F, 0x38, 0xC9, 0x08};
    const auto decodedMessage = Codegen::DecodeSha1(message.data(), message.size());
    require(decodedMessage.Operation == Codegen::Sha1Operation::Msg1 && decodedMessage.Destination == 1 && decodedMessage.Memory && decodedMessage.Memory->Mod == 0 && decodedMessage.Memory->Rm == 0, "SHA1MSG1 memory operand was decoded incorrectly");
    const Bytes ripMessage = {0x0F, 0x38, 0xC9, 0x15, 0x10, 0x00, 0x00, 0x00};
    const auto ripDecoded = Codegen::DecodeSha1(ripMessage.data(), ripMessage.size());
    require(ripDecoded.Operation == Codegen::Sha1Operation::Msg1 && ripDecoded.Destination == 2 && ripDecoded.Memory && ripDecoded.Memory->RipRelative && ripDecoded.Memory->Displacement == 0x10 && ripDecoded.Memory->InstructionLength == 8, "SHA-1 RIP-relative operand was decoded incorrectly");
    const Bytes ripRounds = {0x0F, 0x3A, 0xCC, 0x15, 0xF0, 0xFF, 0xFF, 0xFF, 0x03};
    const auto ripRoundsDecoded = Codegen::DecodeSha1(ripRounds.data(), ripRounds.size());
    require(ripRoundsDecoded.Operation == Codegen::Sha1Operation::Rnds4 && ripRoundsDecoded.Function == 3 && ripRoundsDecoded.Memory && ripRoundsDecoded.Memory->RipRelative && ripRoundsDecoded.Memory->Displacement == -16 && ripRoundsDecoded.Memory->InstructionLength == 9, "SHA1RNDS4 RIP-relative operand was decoded incorrectly");
    requireFailure([] { const Bytes bytes = {0x0F, 0x38, 0xC9, 0x15, 0, 0, 0, 0, 0x90}; (void)Codegen::DecodeSha1(bytes.data(), bytes.size()); }, "SHA-1 RIP-relative operand that does not end the instruction was accepted");
    requireFailure([] { const Bytes bytes = {0x0F, 0x3A, 0xCC, 0x15, 0, 0, 0, 0, 0x00, 0x90}; (void)Codegen::DecodeSha1(bytes.data(), bytes.size()); }, "SHA1RNDS4 RIP-relative operand that does not end the instruction was accepted");
    requireFailure([] { const Bytes bytes = {0x0F, 0x38, 0xC9, 0x54, 0x24}; (void)Codegen::DecodeSha1(bytes.data(), bytes.size()); }, "Truncated SHA-1 memory operand was accepted");
    requireFailure([] { const Bytes bytes = {0x0F, 0x3A, 0xCC, 0xCA}; (void)Codegen::DecodeSha1(bytes.data(), bytes.size()); }, "SHA1RNDS4 without its immediate was accepted");
    requireFailure([] { const Bytes bytes = {0x0F, 0x3A, 0xCC, 0x50, 0x02}; (void)Codegen::DecodeSha1(bytes.data(), bytes.size()); }, "SHA1RNDS4 memory form without its immediate was accepted");
    requireFailure([] { const Bytes bytes = {0x66, 0x0F, 0x38, 0xC8, 0xCA}; (void)Codegen::DecodeSha1(bytes.data(), bytes.size()); }, "Prefixed 0F 38 C8 was decoded as SHA-1");
    requireFailure([] { const Bytes bytes = {0x0F, 0x38, 0xCC, 0xCA}; (void)Codegen::DecodeSha1(bytes.data(), bytes.size()); }, "SHA-256 was decoded as SHA-1");
    const Bytes prefixedRounds = {0x66, 0x0F, 0x3A, 0xCC, 0xCA, 0x00};
    require(!Codegen::DecodedInstruction{prefixedRounds.data(), prefixedRounds.size()}.IsSha1(), "66-prefixed 0F 3A CC was recognised as SHA-1");
}

void clzeroOperands() {
    const auto decode = [](const Bytes& bytes) { return Codegen::DecodeClzero(bytes.data(), bytes.size()); };
    require(!decode({0x0F, 0x01, 0xFC}).AddressSize32 && decode({0x67, 0x0F, 0x01, 0xFC}).AddressSize32, "CLZERO address size was decoded incorrectly");
    require(!decode({0x3E, 0x48, 0x0F, 0x01, 0xFC}).AddressSize32, "CLZERO with a DS override and REX was not decoded");
    requireFailure([&] { (void)decode({0xF0, 0x0F, 0x01, 0xFC}); }, "LOCK CLZERO was accepted");
    requireFailure([&] { (void)decode({0x64, 0x0F, 0x01, 0xFC}); }, "FS-relative CLZERO was accepted");
    requireFailure([&] { (void)decode({0x65, 0x0F, 0x01, 0xFC}); }, "GS-relative CLZERO was accepted");
    Bytes overlong(13, 0x2E);
    overlong.insert(overlong.end(), {0x0F, 0x01, 0xFC});
    requireFailure([&] { (void)decode(overlong); }, "CLZERO longer than 15 bytes was accepted");
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
    require(monitorx && monitorx->Lowering == Codegen::Amd64OnlyLowering::InPlace && monitorx->ReplacementBytes == Bytes{0x0F, 0x1F, 0x00} && monitorx->InstructionName == "MONITORX", "MONITORX was not replaced by a NOP");
    const auto mwaitx = match({0x0F, 0x01, 0xFB});
    require(mwaitx && mwaitx->Lowering == Codegen::Amd64OnlyLowering::InPlace && mwaitx->ReplacementBytes == Bytes{0xF3, 0x90, 0x90} && mwaitx->InstructionName == "MWAITX", "MWAITX was not replaced by PAUSE");
    const auto prefixedMonitorx = match({0x67, 0x0F, 0x01, 0xFA});
    require(prefixedMonitorx && prefixedMonitorx->ReplacementBytes == Bytes{0x0F, 0x1F, 0x40, 0x00}, "Prefixed MONITORX was not padded to its length");
    const auto prefixedMwaitx = match({0x2E, 0x41, 0x0F, 0x01, 0xFB});
    require(prefixedMwaitx && prefixedMwaitx->ReplacementBytes == Bytes{0xF3, 0x90, 0x0F, 0x1F, 0x00}, "Prefixed MWAITX was not padded to its length");
    Bytes longMonitorx(5, 0x2E);
    longMonitorx.insert(longMonitorx.end(), {0x0F, 0x01, 0xFA});
    const auto paddedMonitorx = match(longMonitorx);
    require(paddedMonitorx && paddedMonitorx->ReplacementBytes == Bytes{0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x90}, "8-byte MONITORX was not padded with two NOPs");
    Bytes longestMwaitx(12, 0x2E);
    longestMwaitx.insert(longestMwaitx.end(), {0x0F, 0x01, 0xFB});
    const auto paddedMwaitx = match(longestMwaitx);
    require(paddedMwaitx && paddedMwaitx->ReplacementBytes == Bytes{0xF3, 0x90, 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00}, "15-byte MWAITX was not padded to its length");
    const auto lockedMwaitx = match({0xF0, 0x0F, 0x01, 0xFB});
    require(lockedMwaitx && lockedMwaitx->Lowering == Codegen::Amd64OnlyLowering::Unsupported, "LOCK MWAITX was replaced instead of failing");
    Bytes overlongMonitorx(13, 0x2E);
    overlongMonitorx.insert(overlongMonitorx.end(), {0x0F, 0x01, 0xFA});
    const auto overlong = match(overlongMonitorx);
    require(overlong && overlong->Lowering == Codegen::Amd64OnlyLowering::Unsupported, "MONITORX longer than 15 bytes was replaced instead of failing");
    const auto clzero = match({0x0F, 0x01, 0xFC});
    require(clzero && clzero->Lowering == Codegen::Amd64OnlyLowering::Trampoline && clzero->InstructionName == "CLZERO", "CLZERO was not lowered through a stub");
    for (const std::uint8_t prefix : {std::uint8_t{0x66}, std::uint8_t{0xF2}, std::uint8_t{0xF3}}) {
        const auto prefixedClzero = match({prefix, 0x0F, 0x01, 0xFC});
        require(prefixedClzero && prefixedClzero->Lowering == Codegen::Amd64OnlyLowering::Unsupported, "CLZERO with a 66, F2 or F3 prefix was not reported as unsupported");
    }
    const auto rdpru = match({0x0F, 0x01, 0xFD});
    require(rdpru && rdpru->Lowering == Codegen::Amd64OnlyLowering::Trampoline && rdpru->InstructionName == "RDPRU" && rdpru->StubBody.size() == 40 && rdpru->ReturnBranchOffset == 35 && rdpru->Relocations.empty(), "RDPRU was not lowered through a runtime-checked stub");
    require(Bytes(rdpru->StubBody.begin(), rdpru->StubBody.begin() + 35) == Bytes{0x48, 0x8D, 0xA4, 0x24, 0x70, 0xFF, 0xFF, 0xFF, 0x9C, 0x85, 0xC9, 0x74, 0x0B, 0x83, 0xF9, 0x01, 0x74, 0x06, 0x31, 0xC0, 0x31, 0xD2, 0xEB, 0x02, 0x0F, 0x31, 0x9D, 0x48, 0x8D, 0xA4, 0x24, 0x90, 0x00, 0x00, 0x00}, "RDPRU stub does not frame the red zone or dispatch ECX to TSC or zero");
    const auto mcommit = match({0xF3, 0x0F, 0x01, 0xFA});
    require(mcommit && mcommit->Lowering == Codegen::Amd64OnlyLowering::InPlace && mcommit->InstructionName == "MCOMMIT" && mcommit->ReplacementBytes == Bytes{0xF9, 0x0F, 0x1F, 0x00}, "MCOMMIT was not replaced by STC plus NOP");
    const auto prefixedMcommit = match({0x2E, 0xF3, 0x0F, 0x01, 0xFA});
    require(prefixedMcommit && prefixedMcommit->Lowering == Codegen::Amd64OnlyLowering::InPlace && prefixedMcommit->ReplacementBytes == Bytes{0xF9, 0x0F, 0x1F, 0x40, 0x00}, "Prefixed MCOMMIT was not padded to its length");
    const auto lockedMcommit = match({0xF0, 0xF3, 0x0F, 0x01, 0xFA});
    require(lockedMcommit && lockedMcommit->Lowering == Codegen::Amd64OnlyLowering::Unsupported, "LOCK MCOMMIT was replaced instead of failing");
    const auto lockedRdpru = match({0xF0, 0x0F, 0x01, 0xFD});
    require(lockedRdpru && lockedRdpru->Lowering == Codegen::Amd64OnlyLowering::Unsupported, "LOCK RDPRU was replaced instead of failing");
    const auto registerForm = match({0x66, 0x0F, 0x79, 0xCA});
    require(registerForm && registerForm->Lowering == Codegen::Amd64OnlyLowering::Trampoline && registerForm->InstructionName == "EXTRQ register form", "EXTRQ register form was not lowered through a stub");
    const auto insertqRegisterForm = match({0xF2, 0x0F, 0x79, 0xCA});
    require(insertqRegisterForm && insertqRegisterForm->Lowering == Codegen::Amd64OnlyLowering::Trampoline && insertqRegisterForm->InstructionName == "INSERTQ register form", "INSERTQ register form was not lowered through a stub");
    require(!match({0x66, 0x0F, 0x2B, 0x07}) && !match({0x0F, 0x2B, 0x07}) && !match({0x48, 0x8B, 0x05, 0, 0, 0, 0}), "Ordinary instruction was matched");
    for (const auto& [bytes, name] : {std::pair{Bytes{0x0F, 0x38, 0xCB, 0xCA}, "SHA256RNDS2"}, {Bytes{0x0F, 0x38, 0xCC, 0xCA}, "SHA256MSG1"}, {Bytes{0x45, 0x0F, 0x38, 0xCD, 0xE1}, "SHA256MSG2"}}) {
        const auto sha256 = match(bytes);
        require(sha256 && sha256->Lowering == Codegen::Amd64OnlyLowering::Trampoline && sha256->InstructionName == name, "SHA-256 instruction was not lowered through a stub");
    }
    for (const auto& [bytes, name] : {std::pair{Bytes{0x0F, 0x3A, 0xCC, 0xCA, 0x01}, "SHA1RNDS4"}, {Bytes{0x0F, 0x38, 0xC8, 0xCA}, "SHA1NEXTE"}, {Bytes{0x0F, 0x38, 0xC9, 0xCA}, "SHA1MSG1"}, {Bytes{0x45, 0x0F, 0x38, 0xCA, 0xE1}, "SHA1MSG2"}}) {
        const auto sha1 = match(bytes);
        require(sha1 && sha1->Lowering == Codegen::Amd64OnlyLowering::Trampoline && sha1->InstructionName == name, "SHA-1 instruction was not lowered through a stub");
    }
    for (const auto& [bytes, name] : {std::pair{Bytes{0x0F, 0x3A, 0xCC, 0x08, 0x01}, "SHA1RNDS4"}, {Bytes{0x0F, 0x38, 0xC8, 0x48, 0x10}, "SHA1NEXTE"}, {Bytes{0x0F, 0x38, 0xC9, 0x4C, 0x24, 0x18}, "SHA1MSG1"}, {Bytes{0x45, 0x0F, 0x38, 0xCA, 0x21}, "SHA1MSG2"}}) {
        const auto sha1 = match(bytes);
        require(sha1 && sha1->Lowering == Codegen::Amd64OnlyLowering::Trampoline && sha1->InstructionName == name, "SHA-1 memory form was not lowered through a stub");
    }
    const auto stackMessage = match({0x0F, 0x38, 0xC9, 0x4C, 0x24, 0x18});
    require(stackMessage && stackMessage->StubBody.size() > 22 && Bytes(stackMessage->StubBody.begin() + 13, stackMessage->StubBody.begin() + 22) == Bytes{0xF3, 0x0F, 0x6F, 0x84, 0x24, 0xA8, 0x00, 0x00, 0x00}, "SHA-1 stack operand was not loaded past the spilled scratch register");
    const auto stub = match(kInsertqHighSite);
    require(stub && stub->Lowering == Codegen::Amd64OnlyLowering::Trampoline && stub->StubBody == kInsertqHighBody && stub->ReturnBranchOffset == 15 && stub->InstructionName == "INSERTQ", "INSERTQ was not lowered through a stub");
    const auto topAligned = match({0x66, 0x0F, 0x78, 0xC3, 0x18, 0x28});
    require(topAligned && topAligned->Lowering == Codegen::Amd64OnlyLowering::Trampoline && topAligned->InstructionName == "EXTRQ", "Top-aligned EXTRQ was not lowered through a stub");
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
    inPlace({0xF2, 0x0F, 0x78, 0xDB, 0x08, 0x00}, {0xF3, 0x0F, 0x7E, 0xDB, 0x66, 0x90});
    inPlace({0x66, 0x0F, 0x78, 0xC3, 0x00, 0x00}, {0xF3, 0x0F, 0x7E, 0xDB, 0x66, 0x90});
    for (const auto& site : {Bytes{0x66, 0x0F, 0x78, 0xC3, 0x18, 0x28}, Bytes{0x66, 0x0F, 0x78, 0xC3, 0x08, 0x00}, Bytes{0xF2, 0x0F, 0x78, 0xC8, 0x20, 0x00}, Bytes{0xF2, 0x45, 0x0F, 0x78, 0xC8, 0x10, 0x00}}) {
        const auto operands = Codegen::DecodeSse4a(site.data(), site.size());
        require(!lowering.LowerInPlace(operands, site.size()).has_value(), "SSE4a site was lowered in place without zeroing dst[127:64]");
    }
    const Bytes clzeroSite = {0x0F, 0x01, 0xFC};
    const auto clzero = Codegen::ClzeroLowering{}.LowerOutOfLine(Codegen::DecodeClzero(clzeroSite.data(), clzeroSite.size()));
    require(clzero.Bytes == kClzeroBody && clzero.ReturnBranchOffset == 69, "CLZERO stub differs from the golden encoding");
    const auto highRegisters = Codegen::DecodeSse4a(kInsertqHighSite.data(), kInsertqHighSite.size());
    const auto generic = lowering.LowerOutOfLine(Codegen::Sse4aOperands{true, false, 9, 4, 5, 3});
    require(generic.Bytes[0] == 0x48 && generic.Bytes.size() % 16 == 0 && generic.ReturnBranchOffset < generic.Bytes.size(), "Generic INSERTQ body does not start with the red-zone skip");
    (void)highRegisters;
    const auto insertqRegisterForm = lowering.LowerOutOfLine(Codegen::Sse4aOperands{true, true, 1, 2, 0, 0});
    require(insertqRegisterForm.Bytes[0] == 0x48 && insertqRegisterForm.Bytes.size() % 16 == 0 && insertqRegisterForm.ReturnBranchOffset < insertqRegisterForm.Bytes.size(), "INSERTQ register form body does not start with the red-zone skip");
}

void ripRelativeBodies() {
    const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto relocated = [](const Codegen::Amd64OnlyMatch& match, const std::size_t displacementOffset, const std::size_t instructionEnd, const std::int64_t siteTarget) {
        return match.Relocations.size() == 1 && match.Relocations[0].DisplacementOffset == displacementOffset && match.Relocations[0].InstructionEnd == instructionEnd && match.Relocations[0].SiteTarget == siteTarget;
    };
    const Bytes message = {0x0F, 0x38, 0xCC, 0x15, 0x78, 0x56, 0x34, 0x12};
    const auto sha256 = matcher->Match(message.data(), message.size());
    require(sha256 && sha256->StubBody.size() > 21 && Bytes(sha256->StubBody.begin() + 13, sha256->StubBody.begin() + 21) == Bytes{0xF3, 0x0F, 0x6F, 0x0D, 0x00, 0x00, 0x00, 0x00}, "SHA-256 RIP-relative operand was not reloaded through a RIP-relative MOVDQU");
    require(relocated(*sha256, 17, 21, 8 + 0x12345678), "SHA-256 RIP-relative operand has the wrong relocation");
    const Bytes prefixed = {0x65, 0x67, 0x0F, 0x38, 0xCC, 0x15, 0x78, 0x56, 0x34, 0x12};
    const auto segmentOverride = matcher->Match(prefixed.data(), prefixed.size());
    require(segmentOverride && segmentOverride->StubBody.size() > 23 && Bytes(segmentOverride->StubBody.begin() + 13, segmentOverride->StubBody.begin() + 23) == Bytes{0x65, 0x67, 0xF3, 0x0F, 0x6F, 0x0D, 0x00, 0x00, 0x00, 0x00}, "RIP-relative reload lost the segment or address-size prefix");
    require(relocated(*segmentOverride, 19, 23, 10 + 0x12345678), "Prefixed RIP-relative operand has the wrong relocation");
    const Bytes rounds = {0x0F, 0x3A, 0xCC, 0x15, 0xF0, 0xFF, 0xFF, 0xFF, 0x03};
    const auto sha1 = matcher->Match(rounds.data(), rounds.size());
    require(sha1 && sha1->StubBody.size() > 21 && Bytes(sha1->StubBody.begin() + 13, sha1->StubBody.begin() + 21) == Bytes{0xF3, 0x0F, 0x6F, 0x05, 0x00, 0x00, 0x00, 0x00}, "SHA1RNDS4 RIP-relative operand was not reloaded through a RIP-relative MOVDQU");
    require(relocated(*sha1, 17, 21, 9 - 16), "SHA1RNDS4 RIP-relative operand does not count its immediate");
    const Bytes extrq = {0x66, 0x0F, 0x79, 0xCA};
    const Bytes followers = {0x90, 0x48, 0x8D, 0x05, 0x10, 0x00, 0x00, 0x00};
    const auto moved = matcher->Match(extrq.data(), extrq.size(), followers);
    require(moved && moved->ReturnBranchOffset >= followers.size(), "EXTRQ with followers was not lowered through a stub");
    const auto start = moved->ReturnBranchOffset - followers.size();
    require(Bytes(moved->StubBody.begin() + static_cast<std::ptrdiff_t>(start), moved->StubBody.begin() + static_cast<std::ptrdiff_t>(moved->ReturnBranchOffset)) == Bytes{0x90, 0x48, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00}, "Moved RIP-relative LEA was not copied with a cleared displacement");
    require(relocated(*moved, start + 4, start + 8, 4 + 8 + 0x10), "Moved RIP-relative LEA has the wrong relocation");
    const std::vector<std::span<const std::uint8_t>> sequence = {extrq, message};
    const auto pair = matcher->MatchSequence(sequence, {});
    require(pair && pair->Relocations.size() == 1 && pair->Relocations[0].SiteTarget == 4 + 8 + 0x12345678, "RIP-relative operand in a sequence is not relative to the site start");
}

void stubRelocations() {
    Bytes body(8, 0xCC);
    const std::vector<Codegen::StubRelocation> relocations = {{2, 6, 0x1040}};
    Codegen::ApplyStubRelocations(body, relocations, 0x401000, 0x400F00, 0);
    require(read<std::int32_t>(body, 2) == 0x401000 + 0x1040 - (0x400F00 + 6) && body[0] == 0xCC && body[6] == 0xCC, "Stub relocation was not resolved against the site address");
    Codegen::ApplyStubRelocations(body, relocations, 0x1000, 0x7FFF0000, 0);
    require(read<std::int32_t>(body, 2) == 0x1000 + 0x1040 - (0x7FFF0000 + 6), "Stub placed above its site was not relocated backwards");
    requireFailure([&] { Codegen::ApplyStubRelocations(body, relocations, 0x1000, 0x1000 + 0x90000000ull, 0); }, "RIP-relative operand beyond rel32 range was accepted");
    const std::vector<Codegen::StubRelocation> outside = {{6, 10, 0}};
    requireFailure([&] { Codegen::ApplyStubRelocations(body, outside, 0x1000, 0x2000, 0); }, "Stub relocation outside the body was accepted");
    const std::vector<Codegen::StubRelocation> narrow = {{4, 6, 0}};
    requireFailure([&] { Codegen::ApplyStubRelocations(body, narrow, 0x1000, 0x2000, 0); }, "Stub relocation narrower than a displacement was accepted");
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
    auto lockedRdpru = file;
    lockedRdpru[0x20F] = 0xF0;
    lockedRdpru[0x210] = 0x0F;
    lockedRdpru[0x211] = 0x01;
    lockedRdpru[0x212] = 0xFD;
    requireFailure([&] { (void)converter->Convert(lockedRdpru, {segmentHeader(20)}); }, "LOCK RDPRU was accepted");
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

void converterRipRelativeFollower() {
    const auto converter = Codegen::MakeAmd64OnlyConverter();
    const auto withFollower = [](const Bytes& following) {
        Bytes file(0x300, 0xCC);
        Bytes text = {0x66, 0x0F, 0x79, 0xCA};
        text.insert(text.end(), following.begin(), following.end());
        text.push_back(0xC3);
        std::copy(text.begin(), text.end(), file.begin() + 0x200);
        return std::pair{file, segmentHeader(text.size())};
    };
    for (const auto& following : kRipRelativeVectorLoads) {
        const auto [file, header] = withFollower(following);
        const auto result = converter->Convert(file, {header});
        require(result.Trampolines.size() == 1 && result.Trampolines[0].Length == 4 + following.size(), "RIP-relative vector load was not moved into the EXTRQ stub");
        const auto& site = result.Trampolines[0];
        const auto info = Codegen::X64InstructionDecoder{}.DecodeInstruction(following.data(), following.size());
        const auto start = site.ReturnBranchOffset - following.size();
        require(site.Relocations.size() == 1 && site.Relocations[0].DisplacementOffset == start + info.RipRelativeDispOffset && site.Relocations[0].InstructionEnd == site.ReturnBranchOffset && site.Relocations[0].SiteTarget == static_cast<std::int64_t>(4 + following.size()) + 0x10, "Moved RIP-relative vector load has the wrong relocation");
    }
    for (const Bytes& following : {Bytes{0xFF, 0x15, 0x10, 0x00, 0x00, 0x00}, Bytes{0xFF, 0x25, 0x10, 0x00, 0x00, 0x00}}) {
        const auto [file, header] = withFollower(following);
        require(failureOffset([&] { (void)converter->Convert(file, {header}); }, "RIP-relative indirect branch was moved into an EXTRQ stub") == 0x204, "RIP-relative branch failure does not carry its file offset");
    }
    for (const auto& following : kRegisterVectorOperations) {
        const auto [file, header] = withFollower(following);
        const auto result = converter->Convert(file, {header});
        require(result.Trampolines.size() == 1 && result.Trampolines[0].Length == 4 + following.size(), "Vector instruction without a RIP-relative operand was not moved into the EXTRQ stub");
    }
}

void converterSha256() {
    const auto converter = Codegen::MakeAmd64OnlyConverter();
    Bytes file(0x300, 0xCC);
    const Bytes text = {
        0x0F, 0x38, 0xCB, 0xCA,
        0x0F, 0x38, 0xCC, 0xD3,
        0x66, 0x0F, 0xFE, 0xC1,
        0x0F, 0x38, 0xCD, 0xE5,
        0x90,
        0xC3};
    std::copy(text.begin(), text.end(), file.begin() + 0x200);
    const auto result = converter->Convert(file, {segmentHeader(text.size())});
    require(result.Trampolines.size() == 2 && result.Reports.size() == 2 && result.Bytes == file, "SHA-256 sites were not lowered through stubs");
    const auto& rounds = result.Trampolines[0];
    require(rounds.Offset == 0x200 && rounds.Length == 8 && result.Reports[0].InstructionName == "SHA256RNDS2", "SHA256RNDS2 did not absorb the following SHA256MSG1");
    const auto& message = result.Trampolines[1];
    require(message.Offset == 0x20C && message.Length == 5 && result.Reports[1].InstructionName == "SHA256MSG2", "SHA256MSG2 did not absorb the following instruction");
    require(message.Body[message.ReturnBranchOffset - 1] == 0x90 && message.Body[message.ReturnBranchOffset] == 0xE9, "Absorbed instruction does not run before the return jump");
    auto beforeReturn = file;
    beforeReturn[0x210] = 0xC3;
    requireFailure([&] { (void)converter->Convert(beforeReturn, {segmentHeader(text.size())}); }, "Short SHA-256 instruction followed by a return was relocated");
    auto memoryForm = file;
    memoryForm[0x20F] = 0x28;
    const auto memoryResult = converter->Convert(memoryForm, {segmentHeader(text.size())});
    require(memoryResult.Trampolines.size() == 2 && memoryResult.Reports[1].InstructionName == "SHA256MSG2", "SHA-256 memory form was not lowered through a stub");
    auto ripRelative = file;
    const Bytes ripMessage = {0x0F, 0x38, 0xCD, 0x2D, 0x40, 0x00, 0x00, 0x00, 0xC3};
    std::copy(ripMessage.begin(), ripMessage.end(), ripRelative.begin() + 0x20C);
    const auto ripResult = converter->Convert(ripRelative, {segmentHeader(text.size() + 3)});
    require(ripResult.Trampolines.size() == 2 && ripResult.Bytes == ripRelative && ripResult.Reports[1].InstructionName == "SHA256MSG2", "SHA-256 RIP-relative form was not lowered through a stub");
    const auto& ripSite = ripResult.Trampolines[1];
    require(ripSite.Offset == 0x20C && ripSite.Length == 8 && ripSite.Relocations.size() == 1 && ripSite.Relocations[0].SiteTarget == 8 + 0x40 && ripSite.Relocations[0].InstructionEnd <= ripSite.ReturnBranchOffset, "SHA-256 RIP-relative site was recorded without its relocation");
}

void converterSha1() {
    const auto converter = Codegen::MakeAmd64OnlyConverter();
    Bytes file(0x300, 0xCC);
    const Bytes text = {
        0x0F, 0x3A, 0xCC, 0xCA, 0x00,
        0x0F, 0x38, 0xC8, 0xD3,
        0x0F, 0x38, 0xC9, 0xE5,
        0x66, 0x0F, 0xFE, 0xC1,
        0x0F, 0x38, 0xCA, 0xE5,
        0x90,
        0xC3};
    std::copy(text.begin(), text.end(), file.begin() + 0x200);
    const auto result = converter->Convert(file, {segmentHeader(text.size())});
    require(result.Trampolines.size() == 3 && result.Reports.size() == 3 && result.Bytes == file, "SHA-1 sites were not lowered through stubs");
    require(result.Trampolines[0].Offset == 0x200 && result.Trampolines[0].Length == 5 && result.Reports[0].InstructionName == "SHA1RNDS4", "SHA1RNDS4 was not lowered as one site");
    require(result.Trampolines[1].Offset == 0x205 && result.Trampolines[1].Length == 8 && result.Reports[1].InstructionName == "SHA1NEXTE", "SHA1NEXTE did not absorb the following SHA1MSG1");
    const auto& message = result.Trampolines[2];
    require(message.Offset == 0x211 && message.Length == 5 && result.Reports[2].InstructionName == "SHA1MSG2", "SHA1MSG2 did not absorb the following instruction");
    require(message.Body[message.ReturnBranchOffset - 1] == 0x90 && message.Body[message.ReturnBranchOffset] == 0xE9, "Absorbed instruction does not run before the return jump");
    auto beforeReturn = file;
    beforeReturn[0x215] = 0xC3;
    requireFailure([&] { (void)converter->Convert(beforeReturn, {segmentHeader(text.size())}); }, "Short SHA-1 instruction followed by a return was relocated");
    auto memoryForm = file;
    memoryForm[0x214] = 0x28;
    const auto memoryResult = converter->Convert(memoryForm, {segmentHeader(text.size())});
    require(memoryResult.Trampolines.size() == 3 && memoryResult.Reports[2].InstructionName == "SHA1MSG2", "SHA-1 memory form was not lowered through a stub");
    auto ripRelative = file;
    const Bytes ripMessage = {0x0F, 0x38, 0xCA, 0x2D, 0xF0, 0xFF, 0xFF, 0xFF, 0xC3};
    std::copy(ripMessage.begin(), ripMessage.end(), ripRelative.begin() + 0x211);
    const auto ripResult = converter->Convert(ripRelative, {segmentHeader(text.size() + 3)});
    require(ripResult.Trampolines.size() == 3 && ripResult.Bytes == ripRelative && ripResult.Reports[2].InstructionName == "SHA1MSG2", "SHA-1 RIP-relative form was not lowered through a stub");
    const auto& ripSite = ripResult.Trampolines[2];
    require(ripSite.Offset == 0x211 && ripSite.Length == 8 && ripSite.Relocations.size() == 1 && ripSite.Relocations[0].SiteTarget == 8 - 16 && ripSite.Relocations[0].InstructionEnd <= ripSite.ReturnBranchOffset, "SHA-1 RIP-relative site was recorded without its relocation");
}

void converterMonitorWait() {
    const auto converter = Codegen::MakeAmd64OnlyConverter();
    Bytes file(0x300, 0xCC);
    const Bytes text = {0x0F, 0x01, 0xFA, 0x0F, 0x01, 0xFB, 0x2E, 0x0F, 0x01, 0xFB, 0xF3, 0x0F, 0x01, 0xFA, 0xC3};
    std::copy(text.begin(), text.end(), file.begin() + 0x200);
    const auto result = converter->Convert(file, {segmentHeader(text.size())});
    require(result.ReplacedCount == 4 && result.Trampolines.empty() && result.Reports.size() == 4, "MONITORX/MWAITX/MCOMMIT were not replaced in place");
    require(result.Reports[0].InstructionName == "MONITORX" && result.Reports[1].InstructionName == "MWAITX" && result.Reports[2].Offset == 0x206 && result.Reports[2].ReplacementLength == 4 && result.Reports[2].Lowering == Codegen::Amd64OnlyLowering::InPlace, "MONITORX/MWAITX reports are wrong");
    require(result.Reports[3].InstructionName == "MCOMMIT" && result.Reports[3].Offset == 0x20A && result.Reports[3].ReplacementLength == 4 && result.Reports[3].Lowering == Codegen::Amd64OnlyLowering::InPlace, "MCOMMIT report is wrong");
    auto expected = file;
    const Bytes replaced = {0x0F, 0x1F, 0x00, 0xF3, 0x90, 0x90, 0xF3, 0x90, 0x66, 0x90, 0xF9, 0x0F, 0x1F, 0x00, 0xC3};
    std::copy(replaced.begin(), replaced.end(), expected.begin() + 0x200);
    require(result.Bytes == expected, "MONITORX/MWAITX/MCOMMIT were replaced with the wrong bytes");
}

void converterInPlaceFollower() {
    const auto converter = Codegen::MakeAmd64OnlyConverter();
    const auto convert = [&](const Bytes& following) {
        Bytes file(0x300, 0xCC);
        Bytes text = {0x66, 0x0F, 0x79, 0xCA};
        text.insert(text.end(), following.begin(), following.end());
        text.push_back(0xC3);
        std::copy(text.begin(), text.end(), file.begin() + 0x200);
        return converter->Convert(file, {segmentHeader(text.size())});
    };
    const auto tail = [](const Codegen::TrampolineSite& site, const std::size_t length) {
        return Bytes(site.Body.begin() + static_cast<std::ptrdiff_t>(site.ReturnBranchOffset - length), site.Body.begin() + static_cast<std::ptrdiff_t>(site.ReturnBranchOffset));
    };
    const auto store = convert({0xF3, 0x0F, 0x2B, 0x07});
    require(store.Trampolines.size() == 1 && store.Trampolines[0].Length == 8 && tail(store.Trampolines[0], 4) == Bytes{0xF3, 0x0F, 0x11, 0x07}, "MOVNTSS after a short EXTRQ was not moved into its stub as MOVSS");
    const auto ripStore = convert({0xF2, 0x0F, 0x2B, 0x05, 0x40, 0x00, 0x00, 0x00});
    require(ripStore.Trampolines.size() == 1 && ripStore.Trampolines[0].Length == 12, "RIP-relative MOVNTSD after a short EXTRQ was not moved into its stub");
    const auto& ripSite = ripStore.Trampolines[0];
    require(tail(ripSite, 8) == Bytes{0xF2, 0x0F, 0x11, 0x05, 0x00, 0x00, 0x00, 0x00} && ripSite.Relocations.size() == 1 && ripSite.Relocations[0].SiteTarget == 12 + 0x40 && ripSite.Relocations[0].InstructionEnd == ripSite.ReturnBranchOffset, "Moved MOVNTSD has the wrong bytes or relocation");
    const auto monitor = convert({0x0F, 0x01, 0xFA});
    require(monitor.Trampolines.size() == 1 && monitor.Trampolines[0].Length == 7 && tail(monitor.Trampolines[0], 3) == Bytes{0x0F, 0x1F, 0x00}, "MONITORX after a short EXTRQ was not moved into its stub as a NOP");
}

void converterClzero() {
    const auto converter = Codegen::MakeAmd64OnlyConverter();
    Bytes file(0x300, 0xCC);
    const Bytes text = {
        0x0F, 0x01, 0xFC,
        0x48, 0x83, 0xC0, 0x40,
        0x0F, 0x01, 0xFC,
        0x0F, 0x01, 0xFC,
        0x90,
        0xC3};
    std::copy(text.begin(), text.end(), file.begin() + 0x200);
    const auto result = converter->Convert(file, {segmentHeader(text.size())});
    require(result.Trampolines.size() == 2 && result.Reports.size() == 2 && result.Bytes == file, "CLZERO sites were not lowered through stubs");
    const auto& advance = result.Trampolines[0];
    require(advance.Offset == 0x200 && advance.Length == 7 && result.Reports[0].InstructionName == "CLZERO", "CLZERO did not absorb the following instruction");
    const Bytes add = {0x48, 0x83, 0xC0, 0x40};
    require(Bytes(advance.Body.begin() + static_cast<std::ptrdiff_t>(advance.ReturnBranchOffset - add.size()), advance.Body.begin() + static_cast<std::ptrdiff_t>(advance.ReturnBranchOffset)) == add, "Absorbed instruction does not run before the return jump");
    const auto& pair = result.Trampolines[1];
    require(pair.Offset == 0x207 && pair.Length == 6 && result.Reports[1].InstructionName == "CLZERO", "Consecutive CLZERO sites were not lowered into one stub");
    auto beforeReturn = file;
    beforeReturn[0x203] = 0xC3;
    requireFailure([&] { (void)converter->Convert(beforeReturn, {segmentHeader(text.size())}); }, "Short CLZERO followed by a return was relocated");
    auto segmentRelative = file;
    const Bytes fsClzero = {0x64, 0x0F, 0x01, 0xFC};
    std::copy(fsClzero.begin(), fsClzero.end(), segmentRelative.begin() + 0x20A);
    require(failureOffset([&] { (void)converter->Convert(segmentRelative, {segmentHeader(text.size())}); }, "FS-relative CLZERO was accepted") == 0x20A, "CLZERO operand failure does not carry the file offset");
}

void rosettaSubstitutions() {
    using Codegen::Amd64OnlyLowering;
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto match = [](const auto& matcher, const Bytes& bytes) { return matcher->Match(bytes.data(), bytes.size()); };
    for (const auto& bytes : {Bytes{0x0F, 0xC7, 0xF8}, Bytes{0xF3, 0x0F, 0xC7, 0xF8}, Bytes{0x66, 0x0F, 0xAE, 0x30}, Bytes{0x0F, 0x01, 0xF9}})
        require(!match(intel, bytes).has_value(), "RDSEED, RDPID, CLWB or RDTSCP was converted for an Intel host");
    const auto rdseed = match(rosetta, {0x48, 0x0F, 0xC7, 0xF9});
    require(rdseed && rdseed->Lowering == Amd64OnlyLowering::InPlace && rdseed->ReplacementBytes == Bytes{0x48, 0x0F, 0xC7, 0xF1} && rdseed->InstructionName == "RDSEED", "RDSEED was not rewritten to RDRAND");
    const auto rdseed16 = match(rosetta, {0x66, 0x41, 0x0F, 0xC7, 0xFA});
    require(rdseed16 && rdseed16->ReplacementBytes == Bytes{0x66, 0x41, 0x0F, 0xC7, 0xF2}, "16-bit RDSEED was not rewritten to RDRAND");
    require(!match(rosetta, {0x0F, 0xC7, 0x38}).has_value(), "VMPTRST (the memory form of 0F C7 /7) was taken for RDSEED");
    const auto clwb = match(rosetta, {0x66, 0x0F, 0xAE, 0x74, 0x24, 0x08});
    require(clwb && clwb->Lowering == Amd64OnlyLowering::InPlace && clwb->ReplacementBytes == Bytes{0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00} && clwb->InstructionName == "CLWB", "CLWB was not replaced by a NOP");
    require(!match(rosetta, {0x66, 0x0F, 0xAE, 0xF1}).has_value(), "TPAUSE (the register form of 66 0F AE /6) was taken for CLWB");
    require(!match(rosetta, {0x66, 0x0F, 0xAE, 0x38}).has_value(), "CLFLUSHOPT was taken for CLWB");
    const auto rdpid = match(rosetta, {0xF3, 0x41, 0x0F, 0xC7, 0xFB});
    require(rdpid && rdpid->Lowering == Amd64OnlyLowering::Trampoline && rdpid->Optional && rdpid->InstructionName == "RDPID", "RDPID was not lowered through an optional stub");
    require(rdpid->ReturnBranchOffset == 6 && Bytes(rdpid->StubBody.begin(), rdpid->StubBody.begin() + 7) == Bytes{0x41, 0xBB, 0, 0, 0, 0, 0xE9}, "RDPID r11 stub does not load 0 before returning");
    const auto rdtscp = match(rosetta, {0x0F, 0x01, 0xF9});
    require(rdtscp && rdtscp->Lowering == Amd64OnlyLowering::Trampoline && rdtscp->Optional && rdtscp->InstructionName == "RDTSCP", "RDTSCP was not lowered through an optional stub");
    require(rdtscp->ReturnBranchOffset == 8 && Bytes(rdtscp->StubBody.begin(), rdtscp->StubBody.begin() + 9) == Bytes{0x0F, 0x01, 0xF9, 0xB9, 0, 0, 0, 0, 0xE9}, "RDTSCP stub does not load 0 into ECX after reading the counter");
    require(!match(rosetta, {0x0F, 0x01, 0xF8}).has_value(), "SWAPGS (0F 01 F8) was taken for RDTSCP");
    require(!match(rosetta, {0x66, 0x0F, 0x01, 0xF9}).has_value(), "RDTSCP with an operand size prefix was lowered");
}

void wideStoreSplit() {
    struct Case {
        Bytes Original;
        Bytes Low;
        Bytes High;
        const char* Name;
    };
    const std::vector<Case> cases = {
        {{0xC5, 0xFC, 0x11, 0x07}, {0xC5, 0xF8, 0x11, 0x07}, {0xC4, 0xE3, 0x7D, 0x19, 0x47, 0x10, 0x01}, "vmovups (%rdi)"},
        {{0xC4, 0x41, 0x7E, 0x7F, 0x4C, 0x87, 0x70}, {0xC4, 0x41, 0x7A, 0x7F, 0x4C, 0x87, 0x70}, {0xC4, 0x43, 0x7D, 0x19, 0x8C, 0x87, 0x80, 0x00, 0x00, 0x00, 0x01}, "vmovdqu 0x70(%r15,%rax,4)"},
        {{0xC5, 0xFC, 0x11, 0x4C, 0x24, 0xF8}, {0xC5, 0xF8, 0x11, 0x4C, 0x24, 0xF8}, {0xC4, 0xE3, 0x7D, 0x19, 0x4C, 0x24, 0x08, 0x01}, "vmovups -8(%rsp)"},
        {{0xC5, 0xFE, 0x7F, 0x93, 0x78, 0x56, 0x34, 0x12}, {0xC5, 0xFA, 0x7F, 0x93, 0x78, 0x56, 0x34, 0x12}, {0xC4, 0xE3, 0x7D, 0x19, 0x93, 0x88, 0x56, 0x34, 0x12, 0x01}, "vmovdqu 0x12345678(%rbx)"},
        {{0x64, 0xC5, 0xFC, 0x11, 0x04, 0x25, 0x40, 0x00, 0x00, 0x00}, {0x64, 0xC5, 0xF8, 0x11, 0x04, 0x25, 0x40, 0x00, 0x00, 0x00}, {0x64, 0xC4, 0xE3, 0x7D, 0x19, 0x04, 0x25, 0x50, 0x00, 0x00, 0x00, 0x01}, "vmovups %fs:0x40"},
        {{0xC4, 0x41, 0x7C, 0x11, 0x65, 0x00}, {0xC4, 0x41, 0x78, 0x11, 0x65, 0x00}, {0xC4, 0x43, 0x7D, 0x19, 0x65, 0x10, 0x01}, "vmovups (%r13)"},
        {{0xC4, 0xC1, 0x7C, 0x11, 0x47, 0x68}, {0xC4, 0xC1, 0x78, 0x11, 0x47, 0x68}, {0xC4, 0xC3, 0x7D, 0x19, 0x47, 0x78, 0x01}, "vmovups 0x68(%r15)"}};
    for (const auto& item : cases) {
        const auto split = Codegen::DecodeVexWideStore(item.Original.data(), item.Original.size());
        require(split && split->Low.Bytes == item.Low && split->High.Bytes == item.High && !split->Low.RipDisplacementOffset && !split->High.RipDisplacementOffset, (std::string("256-bit store was not split into its 128-bit halves: ") + item.Name).c_str());
    }
    const Bytes rip = {0xC5, 0xFD, 0x11, 0x1D, 0x00, 0x01, 0x00, 0x00};
    const auto ripSplit = Codegen::DecodeVexWideStore(rip.data(), rip.size());
    require(ripSplit && ripSplit->Low.Bytes == Bytes{0xC5, 0xF9, 0x11, 0x1D, 0x00, 0x01, 0x00, 0x00} && ripSplit->High.Bytes == Bytes{0xC4, 0xE3, 0x7D, 0x19, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x01}, "RIP-relative 256-bit store was not split");
    require(ripSplit->Low.RipDisplacementOffset == 4 && ripSplit->Low.RipTarget == 8 + 0x100 && ripSplit->High.RipDisplacementOffset == 5 && ripSplit->High.RipTarget == 8 + 0x110, "RIP-relative halves do not address the original operand and the one 16 bytes above it");
    for (const auto& other : {Bytes{0xC5, 0xF8, 0x11, 0x07}, Bytes{0xC5, 0xFC, 0x11, 0xC1}, Bytes{0xC5, 0xFC, 0x10, 0x07}, Bytes{0xC4, 0xE2, 0x75, 0x2E, 0x07}, Bytes{0xC5, 0xF4, 0x11, 0x07}, Bytes{0xC4, 0xE3, 0x7D, 0x19, 0x07, 0x01}, Bytes{0xC5, 0xFC, 0x11, 0x04}, Bytes{0xC5, 0xFC, 0x11, 0x47}})
        require(!Codegen::DecodeVexWideStore(other.data(), other.size()).has_value(), "An instruction other than a plain 256-bit store was split");
    for (const auto& aligned : {Bytes{0xC5, 0xFC, 0x29, 0x4C, 0x24, 0xF8}, Bytes{0xC5, 0xFD, 0xE7, 0x93, 0x78, 0x56, 0x34, 0x12}, Bytes{0xC5, 0xFD, 0x7F, 0x1F}, Bytes{0xC5, 0xFC, 0x2B, 0x27}})
        require(!Codegen::DecodeVexWideStore(aligned.data(), aligned.size()).has_value(), "An aligned 256-bit store, which cannot cross a page, was split");
    const Bytes farRip = {0xC5, 0xFC, 0x11, 0x05, 0xF8, 0xFF, 0xFF, 0x7F};
    const auto farRipSplit = Codegen::DecodeVexWideStore(farRip.data(), farRip.size());
    require(farRipSplit && farRipSplit->High.RipTarget == std::int64_t{8} + 0x7FFFFFF8 + 0x10, "A RIP-relative 256-bit store near the end of its displacement range was not split");
    const Bytes farBase = {0xC5, 0xFC, 0x11, 0x80, 0xF8, 0xFF, 0xFF, 0x7F};
    requireFailure([&] { (void)Codegen::DecodeVexWideStore(farBase.data(), farBase.size()); }, "A 256-bit store whose upper half has no 32-bit displacement was kept");
    requireFailure([] { Codegen::StubBodyBuilder body; body.RipRelative(Bytes{0x90}, 0, 0); }, "A RIP-relative displacement outside its instruction was relocated");
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const Bytes store = {0xC4, 0xC1, 0x7C, 0x11, 0x47, 0x68};
    const auto lowered = rosetta->Match(store.data(), store.size());
    require(lowered && lowered->Lowering == Codegen::Amd64OnlyLowering::Trampoline && !lowered->Optional && lowered->RelocatableWindow && lowered->InstructionName == "256-bit store", "A 256-bit store was not lowered through a required stub that may move its neighbours for Rosetta");
    Bytes expectedBody = cases.back().Low;
    expectedBody.insert(expectedBody.end(), cases.back().High.begin(), cases.back().High.end());
    require(Bytes(lowered->StubBody.begin(), lowered->StubBody.begin() + static_cast<std::ptrdiff_t>(lowered->ReturnBranchOffset)) == expectedBody, "The 256-bit store stub is not the two halves");
    const std::vector<std::span<const std::uint8_t>> pair = {std::span<const std::uint8_t>(cases[0].Original), std::span<const std::uint8_t>(rip)};
    const auto sequence = rosetta->MatchSequence(pair, {});
    require(sequence && sequence->Relocations.size() == 2 && sequence->Relocations[0].SiteTarget == 4 + 8 + 0x100 && sequence->Relocations[1].SiteTarget == 4 + 8 + 0x110, "A RIP-relative 256-bit store after another is not relocated from its place in the site");
    require(!Codegen::MakeAmd64OnlyInstructionMatcher()->Match(store.data(), store.size()).has_value(), "A 256-bit store was split for an Intel host");
    const std::vector<std::span<const std::uint8_t>> single = {std::span<const std::uint8_t>(store)};
    require(!Codegen::MakeAmd64OnlyInstructionMatcher()->MatchSequence(single, {}).has_value(), "A 256-bit store was split in a sequence for an Intel host");
}

void converterWideStoreWindows() {
    const auto converter = Codegen::MakeAmd64OnlyConverter(Codegen::Amd64OnlyTarget::Rosetta);
    const Bytes store = {0xC5, 0xFC, 0x11, 0x07};
    Bytes halves = {0xC5, 0xF8, 0x11, 0x07, 0xC4, 0xE3, 0x7D, 0x19, 0x47, 0x10, 0x01};
    const auto convert = [&](const Bytes& text) {
        Bytes file(0x300 + text.size(), 0xCC);
        std::copy(text.begin(), text.end(), file.begin() + 0x200);
        return converter->Convert(file, {segmentHeader(text.size())});
    };
    const auto body = [](const Codegen::TrampolineSite& site) {
        return Bytes(site.Body.begin(), site.Body.begin() + static_cast<std::ptrdiff_t>(site.ReturnBranchOffset));
    };
    const auto withStore = [&](Bytes before, const Bytes& after) {
        before.insert(before.end(), store.begin(), store.end());
        before.insert(before.end(), after.begin(), after.end());
        return before;
    };

    const auto returned = convert(withStore({}, {0xC3}));
    Bytes expected = halves;
    expected.push_back(0xC3);
    require(returned.Trampolines.size() == 1 && returned.KeptCount == 0 && returned.Reports.size() == 1 && returned.Reports[0].InstructionName == "256-bit store", "A 256-bit store followed by a return was not moved");
    require(returned.Trampolines[0].Offset == 0x200 && returned.Trampolines[0].Length == 5 && body(returned.Trampolines[0]) == expected, "A 256-bit store followed by a return did not end its stub with the return");

    const auto called = convert(withStore({}, {0xE8, 0x10, 0x00, 0x00, 0x00}));
    require(called.Trampolines.size() == 1 && called.Trampolines[0].Offset == 0x200 && called.Trampolines[0].Length == 9, "A 256-bit store followed by a call did not take the call");
    const auto& call = called.Trampolines[0];
    expected = halves;
    expected.insert(expected.end(), {0x48, 0x8D, 0x64, 0x24, 0xF8, 0x50, 0x48, 0x8D, 0x05, 0, 0, 0, 0, 0x48, 0x89, 0x44, 0x24, 0x08, 0x58, 0xE9, 0, 0, 0, 0});
    require(body(call) == expected, "A moved call does not push its original return address and jump");
    require(call.Relocations.size() == 2 && call.Relocations[0].DisplacementOffset == halves.size() + 9 && call.Relocations[0].SiteTarget == 9 && call.Relocations[1].DisplacementOffset == halves.size() + 20 && call.Relocations[1].SiteTarget == 9 + 0x10, "A moved call returns to or calls the wrong address");

    const auto branched = convert(withStore({}, {0x75, 0x10, 0x90}));
    require(branched.Trampolines.size() == 1 && branched.Trampolines[0].Length == 6, "A 256-bit store followed by a short conditional branch did not take the branch");
    const auto& jcc = branched.Trampolines[0];
    expected = halves;
    expected.insert(expected.end(), {0x0F, 0x85, 0, 0, 0, 0});
    require(body(jcc) == expected && jcc.Relocations.size() == 1 && jcc.Relocations[0].SiteTarget == 6 + 0x10, "A moved short conditional branch was not re-encoded with a 32-bit displacement to its target");

    const auto jumped = convert(withStore({}, {0xEB, 0x10, 0x90}));
    expected = halves;
    expected.insert(expected.end(), {0xE9, 0, 0, 0, 0});
    require(jumped.Trampolines.size() == 1 && body(jumped.Trampolines[0]) == expected && jumped.Trampolines[0].Relocations.size() == 1 && jumped.Trampolines[0].Relocations[0].SiteTarget == 6 + 0x10, "A moved short jump was not re-encoded with a 32-bit displacement to its target");

    const auto longBranched = convert(withStore({}, {0x0F, 0x85, 0x10, 0x00, 0x00, 0x00}));
    expected = halves;
    expected.insert(expected.end(), {0x0F, 0x85, 0, 0, 0, 0});
    require(longBranched.Trampolines.size() == 1 && longBranched.Trampolines[0].Length == 10 && body(longBranched.Trampolines[0]) == expected && longBranched.Trampolines[0].Relocations.size() == 1 && longBranched.Trampolines[0].Relocations[0].SiteTarget == 10 + 0x10, "A moved conditional branch with a 32-bit displacement does not reach its target");

    const auto entered = convert(withStore({0x48, 0x83, 0xC1, 0x01}, {0xC3, 0xEB, 0xFD}));
    expected = {0x48, 0x83, 0xC1, 0x01};
    expected.insert(expected.end(), halves.begin(), halves.end());
    require(entered.Trampolines.size() == 1 && entered.Trampolines[0].Offset == 0x200 && entered.Trampolines[0].Length == 8 && body(entered.Trampolines[0]) == expected, "A 256-bit store before a branch target did not take room from the instruction before it");

    Bytes adds;
    for (int index = 0; index < 4; ++index)
        adds.insert(adds.end(), {0x48, 0x83, 0xC1, 0x01});
    Bytes after = {0xC3, 0xEB, 0xFD};
    after.insert(after.end(), adds.begin(), adds.end());
    const auto island = convert(withStore({0xE8, 0x00, 0x00, 0x00, 0x00}, after));
    require(island.Trampolines.size() == 2 && island.KeptCount == 0, "A 256-bit store with no movable neighbours was not lowered through an island");
    const auto& moved = island.Trampolines[0];
    const auto& lowered = island.Trampolines[1];
    expected = adds;
    expected.insert(expected.end(), {0xE9, 0, 0, 0, 0});
    require(moved.Offset == 0x20C && moved.Length == 8 && body(moved) == expected && moved.Relocations.size() == 1 && moved.Relocations[0].SiteTarget == 16, "The instructions under the island were not moved and continued after it");
    expected = halves;
    expected.insert(expected.end(), {0xE9, 0, 0, 0, 0});
    require(lowered.Offset == 0x214 && lowered.Length == 5 && body(lowered) == expected && lowered.Relocations.size() == 1 && lowered.Relocations[0].SiteTarget == -11, "The island does not store both halves and return after the original store");
    require(Bytes(island.Bytes.begin() + 0x205, island.Bytes.begin() + 0x209) == Bytes{0xEB, 0x0D, 0xCC, 0xCC}, "The 256-bit store was not replaced by a short jump to its island");

    requireFailure([&] { (void)convert(withStore({0xE8, 0x00, 0x00, 0x00, 0x00}, {0xC3, 0xEB, 0xFD})); }, "A 256-bit store with no room for a stub jump was kept");

    const auto paired = convert(withStore({}, {0xC5, 0xFC, 0x11, 0x0E, 0xC3}));
    expected = halves;
    expected.insert(expected.end(), {0xC5, 0xF8, 0x11, 0x0E, 0xC4, 0xE3, 0x7D, 0x19, 0x4E, 0x10, 0x01});
    require(paired.Trampolines.size() == 1 && paired.Trampolines[0].Length == 8 && body(paired.Trampolines[0]) == expected && paired.Reports.size() == 1, "A 256-bit store followed by another was not lowered with it in one stub");

    const Bytes push = {0x48, 0x8D, 0x64, 0x24, 0xF8, 0x50, 0x48, 0x8D, 0x05, 0, 0, 0, 0, 0x48, 0x89, 0x44, 0x24, 0x08, 0x58};
    const auto ripCalled = convert(withStore({}, {0xFF, 0x15, 0x10, 0x00, 0x00, 0x00}));
    expected = halves;
    expected.insert(expected.end(), push.begin(), push.end());
    expected.insert(expected.end(), {0xFF, 0x25, 0, 0, 0, 0});
    const auto& ripCall = ripCalled.Trampolines[0];
    require(ripCalled.Trampolines.size() == 1 && ripCall.Length == 10 && body(ripCall) == expected, "A moved call through a RIP-relative pointer does not push its original return address and jump through the pointer");
    require(ripCall.Relocations.size() == 2 && ripCall.Relocations[0].SiteTarget == 10 && ripCall.Relocations[1].DisplacementOffset == halves.size() + 21 && ripCall.Relocations[1].SiteTarget == 10 + 0x10, "A moved call through a RIP-relative pointer returns to or reads the wrong address");

    const auto ripJumped = convert(withStore({}, {0xFF, 0x25, 0x10, 0x00, 0x00, 0x00}));
    expected = halves;
    expected.insert(expected.end(), {0xFF, 0x25, 0, 0, 0, 0});
    require(ripJumped.Trampolines.size() == 1 && body(ripJumped.Trampolines[0]) == expected && ripJumped.Trampolines[0].Relocations.size() == 1 && ripJumped.Trampolines[0].Relocations[0].SiteTarget == 10 + 0x10, "A moved jump through a RIP-relative pointer reads the wrong address");

    const auto relaxed = convert(withStore({}, {0x67, 0xE8, 0x10, 0x00, 0x00, 0x00}));
    expected = halves;
    expected.insert(expected.end(), push.begin(), push.end());
    expected.insert(expected.end(), {0xE9, 0, 0, 0, 0});
    require(relaxed.Trampolines.size() == 1 && relaxed.Trampolines[0].Length == 10 && body(relaxed.Trampolines[0]) == expected && relaxed.Trampolines[0].Relocations.size() == 2 && relaxed.Trampolines[0].Relocations[1].SiteTarget == 10 + 0x10, "A call with an address-size prefix was not moved like a call");

    const Bytes add = {0x48, 0x83, 0xC1, 0x01};
    for (const auto& follower : {Bytes{0xFF, 0xD0}, Bytes{0xE3, 0x10}, Bytes{0x66, 0x75, 0x10}, Bytes{0x66, 0x0F, 0xAE, 0x37}}) {
        const auto preceded = convert(withStore(add, follower));
        expected = add;
        expected.insert(expected.end(), halves.begin(), halves.end());
        require(!preceded.Trampolines.empty() && preceded.Trampolines[0].Offset == 0x200 && preceded.Trampolines[0].Length == 8 && body(preceded.Trampolines[0]) == expected, "A 256-bit store moved a following instruction that cannot leave its place instead of the one before it");
    }

    const Bytes fsRead = {0x66, 0x66, 0x66, 0x64, 0x48, 0x8B, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00};
    const Bytes gsRead = {0x65, 0x48, 0x8B, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00};
    for (const auto& segmentRead : {fsRead, gsRead}) {
        const auto beforeSegment = convert(withStore(add, segmentRead));
        expected = add;
        expected.insert(expected.end(), halves.begin(), halves.end());
        require(beforeSegment.Trampolines.size() == 1 && beforeSegment.Trampolines[0].Offset == 0x200 && beforeSegment.Trampolines[0].Length == 8 && body(beforeSegment.Trampolines[0]) == expected, "A 256-bit store moved a following FS or GS access, which the macOS writer cannot rewrite in a stub, instead of the instruction before it");
        const auto onlySegment = convert(withStore({}, segmentRead));
        expected = halves;
        expected.insert(expected.end(), segmentRead.begin(), segmentRead.end());
        require(onlySegment.Trampolines.size() == 1 && onlySegment.Trampolines[0].Offset == 0x200 && onlySegment.Trampolines[0].Length == store.size() + segmentRead.size() && body(onlySegment.Trampolines[0]) == expected, "A 256-bit store with no other movable neighbour did not fall back to the following FS or GS access");
    }

    Bytes throwing = {0xC3, 0xEB, 0xFD};
    for (int index = 0; index < 3; ++index)
        throwing.insert(throwing.end(), add.begin(), add.end());
    throwing.insert(throwing.end(), {0xFF, 0xD0});
    throwing.insert(throwing.end(), adds.begin(), adds.end());
    const auto farther = convert(withStore({0xE8, 0x00, 0x00, 0x00, 0x00}, throwing));
    require(farther.Trampolines.size() == 2 && farther.Trampolines[0].Offset == 0x21A && farther.Trampolines[1].Offset == 0x222, "An island that would move a register call was not passed over for the next one");
    require(Bytes(farther.Bytes.begin() + 0x205, farther.Bytes.begin() + 0x207) == Bytes{0xEB, 0x1B}, "The 256-bit store does not jump to the island after the register call");

    Bytes reentered = {0xFF, 0xD0, 0xEB, 0xF8};
    reentered.insert(reentered.end(), adds.begin(), adds.end());
    const auto beyondStore = convert(withStore(Bytes(adds.begin(), adds.begin() + 8), reentered));
    require(beyondStore.Trampolines.size() == 2 && beyondStore.Trampolines[0].Offset == 0x210 && beyondStore.Trampolines[1].Offset == 0x218, "An island before a 256-bit store that is a branch target took the store with it");
    require(Bytes(beyondStore.Bytes.begin() + 0x208, beyondStore.Bytes.begin() + 0x20A) == Bytes{0xEB, 0x0E}, "The 256-bit store that is a branch target does not jump to the island after it");

    const Bytes farCall = {0xE8, 0x40, 0x00, 0x00, 0x00};
    Bytes claimedFirst = {0xFF, 0xD0};
    claimedFirst.insert(claimedFirst.end(), adds.begin(), adds.end());
    const auto afterSequence = convert(withStore({0xC5, 0xF8, 0x53, 0xC1, 0x48, 0x83, 0xC1, 0x01}, claimedFirst));
    require(afterSequence.Trampolines.size() == 3 && afterSequence.Trampolines[0].Offset == 0x200 && afterSequence.Trampolines[0].Length == 8 && afterSequence.Trampolines[1].Offset == 0x20E && afterSequence.Trampolines[2].Offset == 0x216, "A 256-bit store moved an instruction already moved into the stub of the instruction before it");

    Bytes pendingRun = {0xFF, 0xD0, 0x48, 0x83, 0xC1, 0x01, 0xC5, 0xFC, 0x11, 0x47, 0x10};
    pendingRun.insert(pendingRun.end(), adds.begin(), adds.end());
    const auto pendingIsland = convert(withStore(farCall, pendingRun));
    require(pendingIsland.Trampolines.size() == 3 && pendingIsland.Trampolines[0].Offset == 0x214 && pendingIsland.Trampolines[1].Offset == 0x21C && pendingIsland.Trampolines[2].Offset == 0x20F && pendingIsland.Trampolines[2].Length == 5, "An island moved the call before a 256-bit store or another store that still needs its own stub");

    Bytes enteredRun = {0xFF, 0xD0};
    for (int index = 0; index < 6; ++index)
        enteredRun.insert(enteredRun.end(), add.begin(), add.end());
    enteredRun.insert(enteredRun.end(), {0xEB, 0xEE});
    const auto enteredIsland = convert(withStore(farCall, enteredRun));
    require(enteredIsland.Trampolines.size() == 2 && enteredIsland.Trampolines[0].Offset == 0x213 && enteredIsland.Trampolines[1].Offset == 0x21B, "An island was placed over a branch target");

    Bytes returning = {0xFF, 0xD0};
    for (int index = 0; index < 3; ++index)
        returning.insert(returning.end(), add.begin(), add.end());
    returning.push_back(0xC3);
    const auto returnIsland = convert(withStore(farCall, returning));
    expected = Bytes(adds.begin(), adds.begin() + 12);
    expected.push_back(0xC3);
    require(returnIsland.Trampolines.size() == 2 && returnIsland.Trampolines[0].Offset == 0x20B && body(returnIsland.Trampolines[0]) == expected, "Instructions moved for an island that end in a return jump back after it");

    const auto intoStore = convert(withStore({0x73, 0x01}, {0xC3}));
    expected = halves;
    expected.push_back(0xC3);
    require(intoStore.Trampolines.size() == 1 && intoStore.Trampolines[0].Offset == 0x202 && intoStore.Trampolines[0].Length == 5 && body(intoStore.Trampolines[0]) == expected, "A branch into the middle of a 256-bit store, where no scanned instruction starts, kept the store");

    const auto intoFollower = convert(withStore({0x73, 0x06}, {0x48, 0x83, 0xC1, 0x01, 0xC3}));
    require(intoFollower.Trampolines.size() == 1 && intoFollower.Trampolines[0].Offset == 0x202 && intoFollower.Trampolines[0].Length == 8, "A branch into the middle of an instruction moved with a 256-bit store kept the store");

    const auto afterBranch = convert(withStore({0x75, 0x10}, {0xFF, 0xD0}));
    expected = {0x0F, 0x85, 0, 0, 0, 0};
    expected.insert(expected.end(), halves.begin(), halves.end());
    require(afterBranch.Trampolines.size() == 1 && afterBranch.Trampolines[0].Offset == 0x200 && afterBranch.Trampolines[0].Length == 6 && body(afterBranch.Trampolines[0]) == expected && afterBranch.Trampolines[0].Relocations[0].SiteTarget == 2 + 0x10, "A 256-bit store after a conditional branch did not take the branch");

    Bytes far = adds;
    far.resize(8);
    for (int index = 0; index < 60; ++index)
        far.insert(far.end(), {0xFF, 0xD0});
    far.insert(far.end(), adds.begin(), adds.end());
    for (int index = 0; index < 70; ++index)
        far.insert(far.end(), {0xFF, 0xD0});
    Bytes tail;
    for (int index = 0; index < 11; ++index)
        tail.insert(tail.end(), {0xFF, 0xD0});
    tail.insert(tail.end(), {0x90, 0x90});
    requireFailure([&] { (void)convert(withStore(far, tail)); }, "A 256-bit store whose movable neighbours are all beyond a short jump was kept");
}

void converterClaimedFollower() {
    const auto converter = Codegen::MakeAmd64OnlyConverter(Codegen::Amd64OnlyTarget::Rosetta);
    for (const auto& [shortInstruction, name] : {std::pair{Bytes{0x0F, 0x53, 0xCA}, "RCPPS"}, std::pair{Bytes{0xF3, 0x0F, 0xC7, 0xF8}, "RDPID"}}) {
        Bytes file(0x300, 0xCC);
        Bytes text = {0x74, 0x06, 0xEB, 0x00, 0xC5, 0xFC, 0x11, 0x07};
        text.insert(text.end(), shortInstruction.begin(), shortInstruction.end());
        for (int index = 0; index < 4; ++index)
            text.insert(text.end(), {0x48, 0x01, 0xC8});
        text.insert(text.end(), 8, 0x90);
        text.push_back(0xC3);
        std::copy(text.begin(), text.end(), file.begin() + 0x200);
        const auto result = converter->Convert(file, {segmentHeader(text.size())});
        for (std::size_t left = 0; left < result.Trampolines.size(); ++left) {
            for (std::size_t right = left + 1; right < result.Trampolines.size(); ++right) {
                const auto& a = result.Trampolines[left];
                const auto& b = result.Trampolines[right];
                require(a.Offset + a.Length <= b.Offset || b.Offset + b.Length <= a.Offset, "A short instruction's site overlaps instructions another stub already moved");
            }
        }
        bool kept = false;
        for (const auto& report : result.Reports)
            kept = kept || (report.InstructionName == name && report.Lowering == Codegen::Amd64OnlyLowering::Kept);
        require(result.KeptCount == 1 && kept, "An optional short instruction followed by moved instructions was not kept");
    }
}

void converterJumpTableTargets() {
    const auto converter = Codegen::MakeAmd64OnlyConverter(Codegen::Amd64OnlyTarget::Rosetta);
    const Bytes leaTable = {0x48, 0x8D, 0x0D, 0xF9, 0x1F, 0x00, 0x00};
    const Bytes dispatch = {0x48, 0x63, 0x04, 0x81, 0x48, 0x01, 0xC8, 0xFF, 0xE0, 0x48, 0x83, 0xC1, 0x01, 0xC5, 0xFC, 0x11, 0x07, 0x48, 0x83, 0xC2, 0x01, 0xC3};
    const Bytes store = {0xC5, 0xFC, 0x11, 0x07};
    const Domain::ProgramHeader code{1, 5, 0x200, 0x1000, 0, 29, 29, 16};
    const Domain::ProgramHeader data{1, 4, 0x300, 0x3000, 0, 0x40, 0x40, 16};
    const auto table = [](const std::uint64_t base, const std::vector<std::uint64_t>& targets) {
        Bytes bytes;
        for (const auto target : targets) {
            const auto relative = static_cast<std::uint32_t>(target - base);
            for (std::size_t byte = 0; byte < 4; ++byte)
                bytes.push_back(static_cast<std::uint8_t>(relative >> (byte * 8)));
        }
        return bytes;
    };
    const auto convert = [&](const Bytes& first, const std::vector<std::pair<std::size_t, Bytes>>& placed, const std::vector<Domain::ProgramHeader>& headers) {
        Bytes file(0x400, 0xCC);
        std::copy(first.begin(), first.end(), file.begin() + 0x200);
        std::copy(dispatch.begin(), dispatch.end(), file.begin() + 0x207);
        for (const auto& [offset, bytes] : placed)
            std::copy(bytes.begin(), bytes.end(), file.begin() + static_cast<std::ptrdiff_t>(offset));
        return converter->Convert(file, headers);
    };
    const auto placedAt = [](const Codegen::ConvertResult& result, const Domain::FileByteOffset offset) {
        return result.Trampolines.size() == 1 && result.Trampolines[0].Offset == offset && result.Trampolines[0].Length == 8;
    };

    require(placedAt(convert(leaTable, {{0x300, table(0x3000, {0x1010, 0x1018})}, {0x330, store}}, {code, data}), 0x210), "A 256-bit store moved the instruction after it although a jump table enters that instruction");
    require(placedAt(convert(leaTable, {{0x300, table(0x3000, {0x1010, 0x1019, 0x1018})}}, {code, data}), 0x214), "A jump table was read past an entry that does not start an instruction");
    require(placedAt(convert({0x48, 0x8B, 0x0D, 0xF9, 0x1F, 0x00, 0x00}, {{0x300, table(0x3000, {0x1010, 0x1018})}}, {code, data}), 0x214), "A jump table was read at an address loaded rather than formed by LEA");
    require(placedAt(convert({0x48, 0x8D, 0x0D, 0xF7, 0x1F, 0x00, 0x00}, {{0x2FE, table(0x2FFE, {0x1018})}}, {code, data}), 0x214), "A jump table was read at an address outside every segment");
    require(placedAt(convert(leaTable, {{0x300, table(0x3000, {0x1010})}, {0x380, table(0x3000, {0x1018})}}, {code, data, {0x61000000, 4, 0x380, 0x3000, 0, 0x10, 0x10, 16}}), 0x214), "A jump table was read from a segment that is not loaded");
    require(placedAt(convert(leaTable, {{0x300, table(0x3000, {0x1010, 0x1018})}, {0x3A0, store}}, {code, data, {2, 5, 0x3A0, 0x2000, 0, 4, 4, 16}}), 0x210), "A segment that is not loaded was converted");
    requireFailure([&] { (void)convert(leaTable, {{0x300, table(0x3000, {0x1010, 0x1018})}}, {code, {1, 4, 0x300, 0x3000, 0, 0x1000, 0x1000, 16}}); }, "A jump table was read from a segment that exceeds the file");
}

void converterRosetta() {
    const auto converter = Codegen::MakeAmd64OnlyConverter(Codegen::Amd64OnlyTarget::Rosetta);
    Bytes file(0x300, 0xCC);
    const Bytes text = {
        0x0F, 0xC7, 0xF8,
        0xF3, 0x0F, 0xC7, 0xF9,
        0x48, 0x83, 0xC1, 0x01,
        0x66, 0x0F, 0xAE, 0x37,
        0xC3};
    std::copy(text.begin(), text.end(), file.begin() + 0x200);
    const auto result = converter->Convert(file, {segmentHeader(text.size())});
    require(result.ReplacedCount == 2 && result.Trampolines.size() == 1 && result.KeptCount == 0 && result.Reports.size() == 3, "RDSEED, RDPID and CLWB were not converted for Rosetta");
    const auto& site = result.Trampolines[0];
    require(site.Offset == 0x203 && site.Length == 8 && result.Reports[1].InstructionName == "RDPID", "RDPID did not absorb the following instruction");
    const Bytes add = {0x48, 0x83, 0xC1, 0x01};
    require(Bytes(site.Body.begin(), site.Body.begin() + 5) == Bytes{0xB9, 0, 0, 0, 0} && Bytes(site.Body.begin() + static_cast<std::ptrdiff_t>(site.ReturnBranchOffset - add.size()), site.Body.begin() + static_cast<std::ptrdiff_t>(site.ReturnBranchOffset)) == add, "RDPID stub does not load 0 and then run the absorbed instruction");
    auto expected = file;
    expected[0x202] = 0xF0;
    const Bytes nop = {0x0F, 0x1F, 0x40, 0x00};
    std::copy(nop.begin(), nop.end(), expected.begin() + 0x20B);
    require(result.Bytes == expected, "RDSEED or CLWB was replaced with the wrong bytes");
    auto beforeReturn = file;
    beforeReturn[0x207] = 0xC3;
    const auto kept = converter->Convert(beforeReturn, {segmentHeader(text.size())});
    require(kept.KeptCount == 1 && kept.Trampolines.empty() && kept.ReplacedCount == 2, "RDPID without room for a jump was not kept");
    const auto intel = Codegen::MakeAmd64OnlyConverter()->Convert(file, {segmentHeader(text.size())});
    require(intel.Reports.empty() && intel.Bytes == file, "The Intel target converted RDSEED, RDPID or CLWB");
}

void rosettaScalarCompares() {
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto match = [&](const Bytes& bytes) { return rosetta->Match(bytes.data(), bytes.size()); };
    const Bytes compare = {0xC5, 0xEA, 0xC2, 0xCA, 0x03};
    const auto lowered = match(compare);
    require(lowered && lowered->Lowering == Codegen::Amd64OnlyLowering::Trampoline && lowered->Optional && !lowered->RelocatableWindow && lowered->InstructionName == "scalar compare", "A Rosetta-hazard scalar compare was not lowered through an optional stub");
    require(lowered->ReturnBranchOffset == 5 && lowered->StubBody.size() == 10 && Bytes(lowered->StubBody.begin(), lowered->StubBody.begin() + 5) == compare && lowered->StubBody[5] == 0xE9 && lowered->Relocations.empty(), "The scalar compare stub does not run the compare unchanged before its return jump");
    const auto unaligned = match(Bytes{0xC5, 0xEA, 0xC2, 0x47, 0x68, 0x03});
    require(unaligned && Bytes(unaligned->StubBody.begin(), unaligned->StubBody.begin() + 6) == Bytes{0xC5, 0xEA, 0xC2, 0x47, 0x68, 0x03} && unaligned->Relocations.empty(), "A memory-form scalar compare does not move unchanged");
    const Bytes rip = {0xC5, 0xEA, 0xC2, 0x0D, 0x00, 0x01, 0x00, 0x00, 0x03};
    const auto ripLowered = match(rip);
    require(ripLowered && ripLowered->Relocations.size() == 1 && ripLowered->Relocations[0].DisplacementOffset == 4 && ripLowered->Relocations[0].InstructionEnd == 9 && ripLowered->Relocations[0].SiteTarget == 9 + 0x100, "A RIP-relative scalar compare was not relocated from its place in the site");
    require(Bytes(ripLowered->StubBody.begin() + 4, ripLowered->StubBody.begin() + 8) == Bytes(4, 0), "The RIP-relative scalar compare keeps its displacement in the stub");
    const auto vex3 = match(Bytes{0xC4, 0xE1, 0xEB, 0xC2, 0xCB, 0x03});
    require(vex3 && Bytes(vex3->StubBody.begin(), vex3->StubBody.begin() + 6) == Bytes{0xC4, 0xE1, 0xEB, 0xC2, 0xCB, 0x03}, "A VEX3 VCMPSD with a hazard predicate does not move unchanged");
    for (const Bytes& kept : {Bytes{0xC5, 0xEA, 0xC2, 0xCA, 0x00}, Bytes{0xC5, 0xEA, 0xC2, 0xCA, 0x01}, Bytes{0xC5, 0xEA, 0xC2, 0xCA, 0x02}, Bytes{0xC4, 0xE1, 0xEB, 0xC2, 0xCB, 0x01}})
        require(!match(kept).has_value(), "An EQ_OQ, LT_OS or LE_OS scalar compare must stay inline");
    for (const Bytes& other : {Bytes{0xC5, 0xE9, 0xC2, 0xCA, 0x03}, Bytes{0xC5, 0xE8, 0xC2, 0xCA, 0x03}, Bytes{0xC5, 0xEA, 0x2E, 0xC8}, Bytes{0xC5, 0xEA, 0xC2, 0xCA}, Bytes{0xC4, 0xE2, 0xEB, 0xC2, 0xCB, 0x03}, Bytes{0x62, 0xD1, 0x76, 0x08, 0xC2, 0xCB, 0x03}})
        require(!match(other).has_value(), "An instruction other than a Rosetta-hazard scalar compare was moved");
    require(!Codegen::MakeAmd64OnlyInstructionMatcher()->Match(compare.data(), compare.size()).has_value(), "A scalar compare was moved for an Intel host");
    const std::vector<std::span<const std::uint8_t>> pair = {std::span<const std::uint8_t>(compare), std::span<const std::uint8_t>(rip)};
    const auto sequence = rosetta->MatchSequence(pair, {});
    require(sequence && sequence->InstructionName == "scalar compare" && sequence->ReturnBranchOffset == 14 && sequence->Relocations.size() == 1 && sequence->Relocations[0].SiteTarget == 5 + 9 + 0x100, "A scalar compare after another is not emitted out of line in one stub");
}

void rosettaLegacyScalarCompares() {
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto match = [&](const Bytes& bytes) { return rosetta->Match(bytes.data(), bytes.size()); };
    for (const Bytes& moved : {Bytes{0xF3, 0x0F, 0xC2, 0xCA, 0x03}, Bytes{0xF2, 0x0F, 0xC2, 0xCA, 0x04}, Bytes{0xF2, 0x41, 0x0F, 0xC2, 0xC9, 0x05},
                               Bytes{0x64, 0xF3, 0x0F, 0xC2, 0x4F, 0x68, 0x06}, Bytes{0xF3, 0x2E, 0x0F, 0xC2, 0x44, 0x24, 0x08, 0x07}, Bytes{0xF3, 0x0F, 0xC2, 0xCA, 0x08}}) {
        const auto lowered = match(moved);
        require(lowered && lowered->Lowering == Codegen::Amd64OnlyLowering::Trampoline && lowered->Optional && lowered->InstructionName == "scalar compare", "A legacy Rosetta-hazard scalar compare was not lowered through an optional stub");
        require(lowered->ReturnBranchOffset == moved.size() && lowered->StubBody.size() == moved.size() + 5 && Bytes(lowered->StubBody.begin(), lowered->StubBody.begin() + static_cast<std::ptrdiff_t>(moved.size())) == moved && lowered->StubBody[moved.size()] == 0xE9 && lowered->Relocations.empty(), "A legacy scalar compare does not run unchanged before its return jump");
    }
    const auto rip = match(Bytes{0xF3, 0x0F, 0xC2, 0x0D, 0x00, 0x01, 0x00, 0x00, 0x06});
    require(rip && rip->Relocations.size() == 1 && rip->Relocations[0].DisplacementOffset == 4 && rip->Relocations[0].InstructionEnd == 9 && rip->Relocations[0].SiteTarget == 9 + 0x100, "A RIP-relative legacy CMPSS was not relocated from its place in the site");
    const auto ripRex = match(Bytes{0xF2, 0x48, 0x0F, 0xC2, 0x05, 0x00, 0x01, 0x00, 0x00, 0x07});
    require(ripRex && ripRex->Relocations.size() == 1 && ripRex->Relocations[0].DisplacementOffset == 5 && ripRex->Relocations[0].InstructionEnd == 10 && ripRex->Relocations[0].SiteTarget == 10 + 0x100, "A RIP-relative legacy CMPSD with REX was not relocated");
    for (const Bytes& kept : {Bytes{0xF3, 0x0F, 0xC2, 0xCA, 0x00}, Bytes{0xF3, 0x0F, 0xC2, 0xCA, 0x01}, Bytes{0xF2, 0x0F, 0xC2, 0xCA, 0x02}})
        require(!match(kept).has_value(), "A legacy EQ_OQ, LT_OS or LE_OS scalar compare must stay inline");
    for (const Bytes& other : {Bytes{0x0F, 0xC2, 0xCA, 0x03}, Bytes{0x66, 0x0F, 0xC2, 0xCA, 0x03}, Bytes{0x66, 0xF3, 0x0F, 0xC2, 0xCA, 0x03}, Bytes{0xF2, 0xF3, 0x0F, 0xC2, 0xCA, 0x03},
                               Bytes{0x67, 0xF3, 0x0F, 0xC2, 0x0D, 0x00, 0x01, 0x00, 0x00, 0x03}, Bytes{0xF0, 0xF3, 0x0F, 0xC2, 0xCA, 0x03}, Bytes{0x48, 0xF3, 0x0F, 0xC2, 0xCA, 0x03},
                               Bytes{0xF3, 0x0F, 0xC2, 0xCA}, Bytes{0xF3, 0x0F, 0xC3, 0xCA, 0x03}, Bytes{0xF3, 0x0F, 0x10, 0xCA}})
        require(!match(other).has_value(), "An instruction other than a legacy Rosetta-hazard scalar compare was moved");
    const Bytes compare = {0xF3, 0x0F, 0xC2, 0xCA, 0x03};
    require(!Codegen::MakeAmd64OnlyInstructionMatcher()->Match(compare.data(), compare.size()).has_value(), "A legacy scalar compare was moved for an Intel host");
}

void converterRosettaLegacyScalarCompare() {
    const auto converter = Codegen::MakeAmd64OnlyConverter(Codegen::Amd64OnlyTarget::Rosetta);
    Bytes file(0x300, 0xCC);
    const Bytes text = {
        0xF3, 0x0F, 0x10, 0x15, 0x10, 0x00, 0x00, 0x00,
        0x0F, 0x28, 0xC2,
        0xF3, 0x0F, 0xC2, 0xCA, 0x03,
        0xC5, 0xF8, 0x28, 0x0D, 0x20, 0x00, 0x00, 0x00,
        0xC5, 0xF8, 0x2E, 0xC8,
        0xC3};
    std::copy(text.begin(), text.end(), file.begin() + 0x200);
    const auto result = converter->Convert(file, {segmentHeader(text.size())});
    require(result.Trampolines.size() == 1 && result.Reports.size() == 1 && result.Reports[0].InstructionName == "scalar compare" && result.Reports[0].Offset == 0x20B && result.Reports[0].Lowering == Codegen::Amd64OnlyLowering::Trampoline, "The legacy compare in the fmax shape was not the only moved instruction");
    const auto& site = result.Trampolines[0];
    require(site.Offset == 0x20B && site.Length == 5 && Bytes(site.Body.begin(), site.Body.begin() + 5) == Bytes(text.begin() + 11, text.begin() + 16) && site.Relocations.empty(), "The legacy compare stub does not run the compare unchanged");
    const auto intel = Codegen::MakeAmd64OnlyConverter()->Convert(file, {segmentHeader(text.size())});
    require(intel.Reports.empty() && intel.Bytes == file, "The Intel target moved a legacy scalar compare");
}

void converterRosettaScalarCompare() {
    const auto converter = Codegen::MakeAmd64OnlyConverter(Codegen::Amd64OnlyTarget::Rosetta);
    Bytes file(0x300, 0xCC);
    const Bytes text = {
        0xC5, 0xFA, 0x10, 0x15, 0x10, 0x00, 0x00, 0x00,
        0xC5, 0xF8, 0x28, 0xC2,
        0xC5, 0xEA, 0xC2, 0xCA, 0x03,
        0xC5, 0xF8, 0x28, 0x0D, 0x20, 0x00, 0x00, 0x00,
        0xC5, 0xF8, 0x2E, 0xC8,
        0xC3};
    std::copy(text.begin(), text.end(), file.begin() + 0x200);
    const auto result = converter->Convert(file, {segmentHeader(text.size())});
    require(result.ReplacedCount == 0 && result.KeptCount == 0 && result.Trampolines.size() == 1 && result.Reports.size() == 1 && result.Reports[0].InstructionName == "scalar compare" && result.Reports[0].Offset == 0x20C && result.Reports[0].Lowering == Codegen::Amd64OnlyLowering::Trampoline, "The Rosetta-hazard compare in the fmax shape was not the only moved instruction");
    const auto& site = result.Trampolines[0];
    require(site.Offset == 0x20C && site.Length == 5 && site.ReturnBranchOffset == 5 && Bytes(site.Body.begin(), site.Body.begin() + 5) == Bytes(text.begin() + 12, text.begin() + 17) && Bytes(site.Body.begin() + 5, site.Body.begin() + 10) == Bytes{0xE9, 0, 0, 0, 0} && site.Relocations.empty(), "The compare stub does not run the compare unchanged and return");
    require(result.Bytes == file, "Moving the compare changed the file image");
    auto branchInside = file;
    const Bytes jumped = {0xEB, 0x01, 0xF3, 0x0F, 0xC2, 0xCA, 0x03, 0xC3};
    std::copy(jumped.begin(), jumped.end(), branchInside.begin() + 0x200);
    const auto kept = converter->Convert(branchInside, {segmentHeader(jumped.size())});
    require(kept.Trampolines.empty() && kept.KeptCount == 1 && kept.Reports.size() == 1 && kept.Reports[0].Lowering == Codegen::Amd64OnlyLowering::Kept && kept.Bytes == branchInside, "A branch past the prefix of the compare did not keep it native");
    auto branchIntoVex = file;
    const Bytes vexJumped = {0xEB, 0x02, 0xC5, 0xEA, 0xC2, 0xCA, 0x03, 0xC3};
    std::copy(vexJumped.begin(), vexJumped.end(), branchIntoVex.begin() + 0x200);
    const auto moved = converter->Convert(branchIntoVex, {segmentHeader(vexJumped.size())});
    require(moved.Trampolines.size() == 1 && moved.Trampolines[0].Offset == 0x202 && moved.KeptCount == 0, "A branch past the VEX prefix of the compare, where no instruction starts, kept the compare native");
    auto ripSource = file;
    const Bytes ripText = {0xC5, 0xEA, 0xC2, 0x0D, 0x00, 0x01, 0x00, 0x00, 0x03, 0xC3};
    std::copy(ripText.begin(), ripText.end(), ripSource.begin() + 0x200);
    const auto ripMoved = converter->Convert(ripSource, {segmentHeader(ripText.size())});
    require(ripMoved.Trampolines.size() == 1 && ripMoved.Trampolines[0].Relocations.size() == 1 && ripMoved.Trampolines[0].Relocations[0].SiteTarget == 9 + 0x100, "A RIP-relative compare source was not relocated");
    const auto intel = Codegen::MakeAmd64OnlyConverter()->Convert(file, {segmentHeader(text.size())});
    require(intel.Reports.empty() && intel.Bytes == file, "The Intel target moved a scalar compare");
}

void reciprocalOperands() {
    const auto vex2 = Codegen::DecodeReciprocal(Bytes{0xC5, 0xF8, 0x52, 0xD5}.data(), 4);
    require(vex2 && vex2->Operation == Codegen::ReciprocalOperation::ReciprocalSquareRoot && vex2->Destination == 2 && vex2->Source == 5, "VEX2 VRSQRTPS was not decoded");
    const auto vex2High = Codegen::DecodeReciprocal(Bytes{0xC5, 0x78, 0x53, 0xC1}.data(), 4);
    require(vex2High && vex2High->Operation == Codegen::ReciprocalOperation::Reciprocal && vex2High->Destination == 8 && vex2High->Source == 1, "VEX2 VRCPPS with a high destination was not decoded");
    const auto vex3 = Codegen::DecodeReciprocal(Bytes{0xC4, 0x41, 0x78, 0x52, 0xC9}.data(), 5);
    require(vex3 && vex3->Destination == 9 && vex3->Source == 9, "VEX3 VRSQRTPS with high registers was not decoded");
    require(!Codegen::DecodeReciprocal(Bytes{0xC5, 0xFC, 0x52, 0xD5}.data(), 4), "256-bit VRSQRTPS must stay native");
    require(!Codegen::DecodeReciprocal(Bytes{0xC5, 0xFA, 0x52, 0xD5}.data(), 4), "VRSQRTSS must stay native");
    require(!Codegen::DecodeReciprocal(Bytes{0xC5, 0xF8, 0x52, 0x10}.data(), 4), "Memory form must stay native");
    require(!Codegen::DecodeReciprocal(Bytes{0xC5, 0xF8, 0x51, 0xD5}.data(), 4), "VSQRTPS is not a reciprocal");
    require(vex2->Vex && vex2High->Vex && vex3->Vex, "VEX reciprocals must clear the upper vector bits");
    const auto legacy = Codegen::DecodeReciprocal(Bytes{0x0F, 0x52, 0xD5}.data(), 3);
    require(legacy && !legacy->Vex && legacy->Operation == Codegen::ReciprocalOperation::ReciprocalSquareRoot && legacy->Destination == 2 && legacy->Source == 5, "Legacy RSQRTPS was not decoded");
    const auto legacyHigh = Codegen::DecodeReciprocal(Bytes{0x45, 0x0F, 0x53, 0xC9}.data(), 4);
    require(legacyHigh && !legacyHigh->Vex && legacyHigh->Operation == Codegen::ReciprocalOperation::Reciprocal && legacyHigh->Destination == 9 && legacyHigh->Source == 9, "Legacy RCPPS with high registers was not decoded");
    for (const Bytes& unsupported : {Bytes{0xF3, 0x0F, 0x52, 0xD5}, Bytes{0xF3, 0x0F, 0x53, 0xD5}, Bytes{0x0F, 0x52, 0x10}, Bytes{0x0F, 0x53, 0x10}, Bytes{0x0F, 0x51, 0xD5}, Bytes{0x0F, 0x52}, Bytes{0x45, 0x0F, 0x53}})
        require(!Codegen::DecodeReciprocal(unsupported.data(), unsupported.size()), "An unsupported legacy reciprocal encoding was accepted");
}

void converterReciprocal() {
    const auto converter = Codegen::MakeAmd64OnlyConverter();
    Bytes file(0x300, 0xCC);
    const Bytes text = {
        0xC5, 0xF8, 0x52, 0xD5,
        0xC5, 0xE8, 0x59, 0xD1,
        0xC5, 0xF8, 0x53, 0xC1,
        0xC3};
    std::copy(text.begin(), text.end(), file.begin() + 0x200);
    const auto result = converter->Convert(file, {segmentHeader(text.size())});
    require(result.Trampolines.size() == 1 && result.KeptCount == 1 && result.Bytes == file, "VRSQRTPS was not lowered or the unmovable VRCPPS was not kept");
    const auto& site = result.Trampolines[0];
    require(site.Offset == 0x200 && site.Length == 8 && result.Reports[0].InstructionName == "VRSQRTPS", "VRSQRTPS did not absorb the following VMULPS");
    require(site.Body[site.ReturnBranchOffset - 4] == 0xC5 && site.Body[site.ReturnBranchOffset - 1] == 0xD1, "Absorbed VMULPS does not run before the return jump");
    require(result.Reports[1].InstructionName == "VRCPPS" && result.Reports[1].Lowering == Codegen::Amd64OnlyLowering::Kept && result.Reports[1].Offset == 0x208, "VRCPPS before a return was not reported as kept");
    auto branchInto = file;
    const Bytes jump = {0xEB, 0x00, 0xC5, 0xF8, 0x52, 0xD5, 0xC5, 0xE8, 0x59, 0xD1, 0xC3};
    std::copy(jump.begin(), jump.end(), branchInto.begin() + 0x200);
    const auto branched = converter->Convert(branchInto, {segmentHeader(jump.size())});
    require(branched.Trampolines.size() == 1 && branched.KeptCount == 0, "A branch to the reciprocal itself must still allow a stub");
    const Bytes jumpInside = {0xEB, 0x04, 0xC5, 0xF8, 0x52, 0xD5, 0xC5, 0xE8, 0x59, 0xD1, 0xC3};
    std::copy(jumpInside.begin(), jumpInside.end(), branchInto.begin() + 0x200);
    const auto inside = converter->Convert(branchInto, {segmentHeader(jumpInside.size())});
    require(inside.Trampolines.empty() && inside.KeptCount == 1 && inside.Bytes == branchInto, "A branch into the absorbed instruction must keep the reciprocal native");
    auto ripRelative = file;
    const Bytes load = {0xC5, 0xF8, 0x52, 0xD5, 0x8B, 0x05, 0x10, 0x00, 0x00, 0x00, 0xC3};
    std::copy(load.begin(), load.end(), ripRelative.begin() + 0x200);
    const auto moved = converter->Convert(ripRelative, {segmentHeader(load.size())});
    require(moved.Trampolines.size() == 1 && moved.KeptCount == 0 && moved.Bytes == ripRelative && moved.Trampolines[0].Length == 10, "VRSQRTPS did not absorb the following RIP-relative load");
    const auto& movedSite = moved.Trampolines[0];
    require(movedSite.Relocations.size() == 1 && movedSite.Relocations[0].DisplacementOffset == movedSite.ReturnBranchOffset - 4 && movedSite.Relocations[0].InstructionEnd == movedSite.ReturnBranchOffset && movedSite.Relocations[0].SiteTarget == 10 + 0x10, "Absorbed RIP-relative load has the wrong relocation");
    for (const std::uint8_t opcode : {std::uint8_t{0x52}, std::uint8_t{0x53}}) {
        const std::string name = opcode == 0x52 ? "RSQRTPS" : "RCPPS";
        for (const Bytes& instruction : {Bytes{0x0F, opcode, 0xD5}, Bytes{0x45, 0x0F, opcode, 0xC9}}) {
            auto legacyFile = file;
            Bytes legacyText = instruction;
            legacyText.insert(legacyText.end(), {0x90, 0x90, 0xC3});
            std::copy(legacyText.begin(), legacyText.end(), legacyFile.begin() + 0x200);
            const auto legacy = converter->Convert(legacyFile, {segmentHeader(legacyText.size())});
            require(legacy.Trampolines.size() == 1 && legacy.KeptCount == 0 && legacy.Reports[0].InstructionName == name && legacy.Trampolines[0].Length == 5, "Legacy reciprocal did not absorb enough instructions for a jump");
            legacyText = instruction;
            legacyText.push_back(0xC3);
            std::copy(legacyText.begin(), legacyText.end(), legacyFile.begin() + 0x200);
            const auto kept = converter->Convert(legacyFile, {segmentHeader(legacyText.size())});
            require(kept.Trampolines.empty() && kept.KeptCount == 1 && kept.Bytes == legacyFile && kept.Reports[0].InstructionName == name, "Legacy reciprocal before a return must stay native");
        }
    }
    auto sequenceFile = file;
    const Bytes sequenceText = {0x0F, 0x52, 0xD5, 0xC5, 0xF8, 0x53, 0xC1, 0xC3};
    std::copy(sequenceText.begin(), sequenceText.end(), sequenceFile.begin() + 0x200);
    const auto sequence = converter->Convert(sequenceFile, {segmentHeader(sequenceText.size())});
    require(sequence.Trampolines.size() == 1 && sequence.KeptCount == 0 && sequence.Trampolines[0].Length == 7 && sequence.Reports[0].InstructionName == "RSQRTPS", "Mixed legacy and VEX reciprocals were not lowered together");
}

void converterStrayRex() {
    const auto converter = Codegen::MakeAmd64OnlyConverter();
    const auto convert = [&](const Bytes& text) {
        Bytes file(0x300 + text.size(), 0xCC);
        std::copy(text.begin(), text.end(), file.begin() + 0x200);
        return std::pair{file, converter->Convert(file, {segmentHeader(text.size())})};
    };
    for (const Bytes& instruction : {Bytes{0x48, 0x64, 0x0F, 0x01, 0xFC, 0x90, 0x90, 0xC3}, Bytes{0x48, 0xF0, 0x0F, 0x01, 0xFC, 0x90, 0x90, 0xC3}})
        require(failureOffset([&] { (void)convert(instruction); }, "CLZERO with a FS or LOCK prefix after a stray REX was accepted") == 0x200, "Stray REX failure does not carry the file offset");
    const auto [addressFile, addressSize32] = convert({0x48, 0x67, 0x0F, 0x01, 0xFC, 0xC3});
    require(addressSize32.Trampolines.size() == 1 && addressSize32.Trampolines[0].Length == 5 && addressSize32.Reports[0].InstructionName == "CLZERO", "67h CLZERO after a stray REX was not lowered as one instruction");
    const auto [movntsFile, movnts] = convert({0x48, 0xF3, 0x0F, 0x2B, 0x00, 0xC3});
    auto expected = movntsFile;
    expected[0x203] = 0x11;
    require(movnts.ReplacedCount == 1 && movnts.Reports[0].InstructionName == "MOVNTSS" && movnts.Bytes == expected, "MOVNTSS after a stray REX was not rewritten");
    for (const Bytes& following : {Bytes{0x48, 0x64, 0x8B, 0x00}, Bytes{0x48, 0x67, 0x8B, 0x00}, Bytes{0x48, 0xF0, 0x01, 0x00}}) {
        Bytes text = {0x0F, 0x01, 0xFC};
        text.insert(text.end(), following.begin(), following.end());
        text.push_back(0xC3);
        const auto [file, result] = convert(text);
        require(result.Trampolines.size() == 1 && result.Trampolines[0].Length == 7, "Instruction with a stray REX was not absorbed whole");
        const auto& body = result.Trampolines[0].Body;
        const auto returnBranch = result.Trampolines[0].ReturnBranchOffset;
        require(Bytes(body.begin() + static_cast<std::ptrdiff_t>(returnBranch - following.size()), body.begin() + static_cast<std::ptrdiff_t>(returnBranch)) == following, "Absorbed instruction lost its prefixes");
    }
}

void rewriterStrayRex() {
    const Bytes code = {0x48, 0x2E, 0xE9, 0x01, 0x00, 0x00, 0x00, 0x90, 0xC3};
    const auto rewritten = Codegen::X64InstructionRewriter{}.Rewrite(code, {7, {0x66, 0x90}});
    const Bytes expected = {0x48, 0x2E, 0xE9, 0x02, 0x00, 0x00, 0x00, 0x66, 0x90, 0xC3};
    require(rewritten.Bytes == expected, "Branch with a stray REX was not adjusted by a length-changing rewrite");
}

void rewriterReferenceSites() {
    const std::vector<std::tuple<Bytes, std::uint64_t, Bytes>> cases = {
        {{0x90, 0x48, 0xB8, 0x05, 0xF0, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xC3}, 0,
         {0x66, 0x90, 0x48, 0xB8, 0x05, 0xF0, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xC3}},
        {{0xC5, 0xF9, 0x6F, 0x05, 0x01, 0x00, 0x00, 0x00, 0x90, 0xC3}, 8,
         {0xC5, 0xF9, 0x6F, 0x05, 0x02, 0x00, 0x00, 0x00, 0x66, 0x90, 0xC3}},
        {{0x66, 0x0F, 0x38, 0x00, 0x05, 0x01, 0x00, 0x00, 0x00, 0x90, 0xC3}, 9,
         {0x66, 0x0F, 0x38, 0x00, 0x05, 0x02, 0x00, 0x00, 0x00, 0x66, 0x90, 0xC3}},
        {{0xE2, 0x01, 0x90, 0xC3}, 2, {0xE2, 0x02, 0x66, 0x90, 0xC3}}};
    for (const auto& [code, offset, expected] : cases)
        require(Codegen::X64InstructionRewriter{}.Rewrite(code, {offset, {0x66, 0x90}}).Bytes == expected, "Length-changing rewrite adjusted an immediate or missed a VEX, three-byte or LOOP reference");
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
    auto lockedWait = file;
    const Bytes lockedWaitBytes = {0xF0, 0x0F, 0x01, 0xFB};
    std::copy(lockedWaitBytes.begin(), lockedWaitBytes.end(), lockedWait.begin() + 0x20F);
    require(failureOffset([&] { (void)converter->Convert(lockedWait, {segmentHeader(20)}); }, "LOCK MWAITX was accepted") == 0x20F, "Unsupported instruction failure does not carry the file offset");
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
    write<std::uint16_t>(bytes, 56, 7);
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
    const auto phOff = read<std::uint64_t>(output, 32);
    const auto phNum = read<std::uint16_t>(output, 56);
    for (std::uint16_t index = 0; index < phNum; ++index) {
        const auto header = phOff + index * 56;
        if (read<std::uint32_t>(output, header) != 1) continue;
        const auto vaddr = read<std::uint64_t>(output, header + 16);
        const auto memSize = read<std::uint64_t>(output, header + 40);
        if (address < vaddr || address >= vaddr + memSize) continue;
        require((read<std::uint32_t>(output, header + 4) & 1) != 0, "Linux stub segment is not executable");
        return read<std::uint64_t>(output, header + 8) + (address - vaddr);
    }
    throw std::runtime_error("Linux stub is not inside a PT_LOAD segment");
}

void linuxPlacement() {
    const auto source = elfFixture({0xEB, 0x06, 0xF2, 0x0F, 0x78, 0xDB, 0x08, 0x08, 0xC3});
    const auto headers = elfHeaders();
    const auto converted = Codegen::MakeAmd64OnlyConverter()->Convert(source, {headers[0]});
    require(converted.Trampolines.size() == 1 && converted.Bytes == source, "Linux fixture conversion produced unexpected results");
    auto patcher = linuxPatcher();
    const auto output = patcher.Patch(converted.Bytes, headers, {}, 0, "$ORIGIN/libs", true, false, converted.Trampolines);
    require(output[0x202] == 0xE9 && output[0x207] == 0x90, "Linux site was not replaced by a jump");
    const auto target = 0x1002 + 5 + static_cast<std::int64_t>(read<std::int32_t>(output, 0x203));
    require(target % 16 == 0 && target > 0x2100, "Linux stub is misaligned or inside the original image");
    const auto bodyOffset = executableFileOffset(output, static_cast<std::uint64_t>(target));
    auto expectedBody = kInsertqSelfBody;
    write<std::int32_t>(expectedBody, 10, static_cast<std::int32_t>(0x1008 - (target + 9 + 5)));
    const Bytes actualBody(output.begin() + static_cast<std::ptrdiff_t>(bodyOffset), output.begin() + static_cast<std::ptrdiff_t>(bodyOffset + expectedBody.size()));
    require(actualBody == expectedBody, "Linux stub body or return branch is wrong");
    auto altered = converted.Bytes;
    altered[0x205] = 0xDC;
    requireFailure([&] { (void)patcher.Patch(altered, headers, {}, 0, "$ORIGIN/libs", true, false, converted.Trampolines); }, "Changed Linux site bytes were accepted");
}

void linuxRipRelativePlacement() {
    Bytes text = {0x0F, 0x38, 0xCC, 0x15, 0x00, 0x00, 0x00, 0x00, 0xC3};
    write<std::int32_t>(text, 4, 0x2040 - 0x1008);
    const auto source = elfFixture(text);
    const auto headers = elfHeaders();
    const auto converted = Codegen::MakeAmd64OnlyConverter()->Convert(source, {headers[0]});
    require(converted.Trampolines.size() == 1 && converted.Trampolines[0].Relocations.size() == 1, "RIP-relative Linux fixture was not lowered with a relocation");
    auto patcher = linuxPatcher();
    const auto output = patcher.Patch(converted.Bytes, headers, {}, 0, "$ORIGIN/libs", true, false, converted.Trampolines);
    require(output[0x200] == 0xE9, "RIP-relative Linux site was not replaced by a jump");
    const auto stub = 0x1000 + 5 + static_cast<std::int64_t>(read<std::int32_t>(output, 0x201));
    const auto& relocation = converted.Trampolines[0].Relocations[0];
    const auto bodyOffset = executableFileOffset(output, static_cast<std::uint64_t>(stub));
    const auto operand = stub + static_cast<std::int64_t>(relocation.InstructionEnd) + read<std::int32_t>(output, bodyOffset + relocation.DisplacementOffset);
    require(operand == 0x2040, "Relocated RIP-relative operand does not reach the original address");
}

std::vector<std::array<std::uint64_t, 8>> stackHeaders(const Bytes& output) {
    const auto phOff = read<std::uint64_t>(output, 32);
    const auto phNum = read<std::uint16_t>(output, 56);
    std::vector<std::array<std::uint64_t, 8>> found;
    for (std::uint16_t index = 0; index < phNum; ++index) {
        const auto header = static_cast<std::size_t>(phOff) + index * 56;
        if (read<std::uint32_t>(output, header) != Elfpatcher::PT_GNU_STACK) continue;
        found.push_back({read<std::uint32_t>(output, header), read<std::uint32_t>(output, header + 4),
            read<std::uint64_t>(output, header + 8), read<std::uint64_t>(output, header + 16),
            read<std::uint64_t>(output, header + 24), read<std::uint64_t>(output, header + 32),
            read<std::uint64_t>(output, header + 40), read<std::uint64_t>(output, header + 48)});
    }
    return found;
}

void linuxStackSynthesis() {
    const auto patchOutput = [](std::uint16_t slots, std::vector<Domain::ProgramHeader> headers) {
        auto elf = elfFixture({0xC3});
        write<std::uint16_t>(elf, 56, slots);
        auto patcher = linuxPatcher();
        return patcher.Patch(std::move(elf), std::move(headers), {}, 0, "$ORIGIN/libs", true, false, {});
    };
    const Domain::ProgramHeader inputStack{Elfpatcher::PT_GNU_STACK, 6, 0, 0, 0, 0, 0x20000, 16};
    const std::vector<std::array<std::uint64_t, 8>> synthetic = {{Elfpatcher::PT_GNU_STACK, 6, 0, 0, 0, 0, 0, 16}};
    const std::vector<std::array<std::uint64_t, 8>> preserved = {{Elfpatcher::PT_GNU_STACK, 6, 0, 0, 0, 0, 0x20000, 16}};

    const auto exactFit = patchOutput(7, elfHeaders());
    require(read<std::uint64_t>(exactFit, 32) >= 0x400, "a full input table must move to make room for the synthetic stack");
    require(read<std::uint16_t>(exactFit, 56) == 8, "the output must use the reserved synthetic stack slot");
    require(stackHeaders(exactFit) == synthetic, "the synthetic stack must be PT_GNU_STACK with PF_R|PF_W and alignment 16");

    const auto spareSlot = patchOutput(8, elfHeaders());
    require(read<std::uint64_t>(spareSlot, 32) == 64, "the required count must not move a table that has a free slot");
    require(read<std::uint16_t>(spareSlot, 56) == 8, "the free slot must be filled by the synthetic stack");
    require(stackHeaders(spareSlot) == synthetic, "the synthetic stack must be PT_GNU_STACK with PF_R|PF_W and alignment 16");

    const auto roomyTable = patchOutput(9, elfHeaders());
    require(read<std::uint64_t>(roomyTable, 32) == 64, "the required count must not move a table with spare slots");
    require(read<std::uint16_t>(roomyTable, 56) == 8, "the output count must match the required count");
    require(stackHeaders(roomyTable) == synthetic, "the synthetic stack must be PT_GNU_STACK with PF_R|PF_W and alignment 16");

    const auto keptStackExact = patchOutput(8, {elfHeaders()[0], elfHeaders()[1], inputStack});
    require(read<std::uint64_t>(keptStackExact, 32) == 64, "an input stack must not over-reserve a header slot");
    require(read<std::uint16_t>(keptStackExact, 56) == 8, "an input stack must not grow the output count");
    require(stackHeaders(keptStackExact) == preserved, "a kept PT_GNU_STACK must stay unchanged");

    const auto keptStackShort = patchOutput(7, {elfHeaders()[0], elfHeaders()[1], inputStack});
    require(read<std::uint64_t>(keptStackShort, 32) >= 0x400, "a full input table must move when the kept stack does not fit");
    require(read<std::uint16_t>(keptStackShort, 56) == 8, "the moved table must carry the kept stack");
    require(stackHeaders(keptStackShort) == preserved, "a kept PT_GNU_STACK must stay unchanged");
}

}

#if (defined(__linux__) || defined(__APPLE__)) && defined(__x86_64__)
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
    const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto match = matcher->Match(site.data(), site.size());
    require(match && match->Lowering != Codegen::Amd64OnlyLowering::Unsupported, "Register form stub was not produced");
    auto body = match->Lowering == Codegen::Amd64OnlyLowering::InPlace ? match->ReplacementBytes : match->StubBody;
    const auto ret = body.size();
    body.push_back(0xC3);
    if (match->Lowering == Codegen::Amd64OnlyLowering::Trampoline) {
        const auto displacement = static_cast<std::int32_t>(ret - (match->ReturnBranchOffset + 5));
        std::memcpy(body.data() + match->ReturnBranchOffset + 1, &displacement, sizeof(displacement));
    }
    void* code = mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    require(code != MAP_FAILED, "cannot map executable memory for the stub");
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
    require(scratchOut[0] == scratchIn[0] && scratchOut[1] == scratchIn[1], "Register form stub clobbered a scratch register");
    require(spareOut[0] == spareIn[0] && spareOut[1] == spareIn[1], "Stub clobbered xmm1");
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

void reciprocalExecution() {
    const float inputs[][4] = {
        {1.0f, 4.0f, 0.25f, 2.0f},
        {0.0f, -0.0f, INFINITY, -1.0f},
        {1.00000012f, 0.99999994f, 3.0e-38f, 1.0e30f}};
    for (const auto& lanes : inputs) {
        const std::uint64_t source[2] = {packLanes(lanes, 0), packLanes(lanes, 2)};
        const std::uint64_t destination[2] = {0x1111111111111111ull, 0x2222222222222222ull};
        for (const bool squareRoot : {true, false}) for (const bool vex : {false, true}) {
            const std::uint8_t opcode = squareRoot ? 0x52 : 0x53;
            const Bytes distinct = vex ? Bytes{0xC5, 0xF8, opcode, 0xD5} : Bytes{0x0F, opcode, 0xD5};
            const Bytes same = vex ? Bytes{0xC5, 0xF8, opcode, 0xD2} : Bytes{0x0F, opcode, 0xD2};
            const auto distinctOut = runRegisterFormStub(distinct, destination, source);
            const auto sameOut = runRegisterFormStub(same, source, source);
            float distinctLanes[4];
            float sameLanes[4];
            std::memcpy(distinctLanes, distinctOut.data(), sizeof(distinctLanes));
            std::memcpy(sameLanes, sameOut.data(), sizeof(sameLanes));
            for (std::size_t lane = 0; lane < 4; ++lane) {
                const float expected = squareRoot ? 1.0f / std::sqrt(lanes[lane]) : 1.0f / lanes[lane];
                require(sameLane(expected, distinctLanes[lane]), "Packed reciprocal stub is not correctly rounded");
                require(sameLane(expected, sameLanes[lane]), "Packed reciprocal stub with equal operands is not correctly rounded");
            }
        }
    }
    const float one[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    const std::uint64_t ones[2] = {packLanes(one, 0), packLanes(one, 2)};
    const auto unit = runRegisterFormStub({0xC5, 0xF8, 0x52, 0xD5}, ones, ones);
    require(unit[0] == ones[0] && unit[1] == ones[1], "VRSQRTPS of 1.0 must be exactly 1.0 so a renormalised unit quaternion stays unit");
}

void registerFormExecution() {
    const Bytes extrqDistinct = {0x66, 0x0F, 0x79, 0xD5};
    const Bytes extrqSame = {0x66, 0x0F, 0x79, 0xD2};
    const Bytes insertqDistinct = {0xF2, 0x0F, 0x79, 0xD5};
    const Bytes insertqSame = {0xF2, 0x0F, 0x79, 0xD2};
    const std::uint64_t value = 0x9e3779b97f4a7c15ull;
    const std::uint64_t destination = 0x0f1e2d3c4b5a6978ull;
    for (const auto [length, index] : {std::pair{8u, 4u}, {0u, 0u}, {40u, 20u}, {63u, 1u}, {1u, 63u}, {16u, 48u}, {1u, 0u}, {32u, 32u}}) {
        const auto control = static_cast<std::uint64_t>(length) | (static_cast<std::uint64_t>(index) << 8) | 0xffffc000ull;
        require(runRegisterFormStub(extrqDistinct, {value, 0x1122334455667788ull}, {control, 0}) == std::array<std::uint64_t, 2>{extrqReference(value, control), 0}, "EXTRQ register form stub computed the wrong field");
        require(runRegisterFormStub(extrqSame, {control, 0x1122334455667788ull}, {control, 0x1122334455667788ull}) == std::array<std::uint64_t, 2>{extrqReference(control, control), 0}, "EXTRQ register form stub with equal operands computed the wrong field");
        const auto insertqControl = control | 0xC0ull;
        require(runRegisterFormStub(insertqDistinct, {destination, 0x1122334455667788ull}, {value, insertqControl})[0] == insertqReference(destination, value, insertqControl), "INSERTQ register form stub computed the wrong field");
        require(runRegisterFormStub(insertqSame, {value, insertqControl}, {value, insertqControl})[0] == insertqReference(value, value, insertqControl), "INSERTQ register form stub with equal operands computed the wrong field");
    }
}

void immediateFormExecution() {
    const std::uint64_t value = 0x9e3779b97f4a7c15ull;
    const std::uint64_t destination = 0x0f1e2d3c4b5a6978ull;
    const std::uint64_t high = 0x1122334455667788ull;
    for (const auto [length, index] : {std::pair{0u, 0u}, {24u, 40u}, {8u, 0u}, {16u, 0u}, {32u, 0u}, {8u, 40u}, {5u, 3u}, {63u, 1u}, {1u, 63u}}) {
        const auto control = static_cast<std::uint64_t>(length) | (static_cast<std::uint64_t>(index) << 8);
        const auto length8 = static_cast<std::uint8_t>(length);
        const auto index8 = static_cast<std::uint8_t>(index);
        require(runRegisterFormStub({0x66, 0x0F, 0x78, 0xC2, length8, index8}, {value, high}, {0, 0}) == std::array<std::uint64_t, 2>{extrqReference(value, control), 0}, "EXTRQ immediate form computed the wrong result");
        require(runRegisterFormStub({0xF2, 0x0F, 0x78, 0xD5, length8, index8}, {destination, high}, {value, high}) == std::array<std::uint64_t, 2>{insertqReference(destination, value, control), 0}, "INSERTQ immediate form computed the wrong result");
        require(runRegisterFormStub({0xF2, 0x0F, 0x78, 0xD2, length8, index8}, {value, high}, {value, high}) == std::array<std::uint64_t, 2>{insertqReference(value, value, control), 0}, "INSERTQ immediate form with equal operands computed the wrong result");
    }
}

void sha256Execution() {
    const std::uint64_t state[2] = {0x6a09e667bb67ae85ull, 0x3c6ef372a54ff53aull};
    const std::uint64_t words[2] = {0x510e527f9b05688cull, 0x1f83d9ab5be0cd19ull};
    for (const std::uint8_t opcode : {std::uint8_t{0xCB}, std::uint8_t{0xCC}, std::uint8_t{0xCD}}) {
        const Bytes distinct = {0x0F, 0x38, opcode, 0xD5};
        const Bytes same = {0x0F, 0x38, opcode, 0xD2};
        require(runRegisterFormStub(distinct, state, words) == sha256Reference(opcode, state, words, kStubScratch), "SHA-256 stub computed the wrong result");
        require(runRegisterFormStub(same, state, state) == sha256Reference(opcode, state, state, kStubScratch), "SHA-256 stub with equal operands computed the wrong result");
        for (const auto& memory : {Bytes{0x0F, 0x38, opcode, 0x11}, Bytes{0x0F, 0x38, opcode, 0x54, 0x24, 0x18}, Bytes{0x0F, 0x38, opcode, 0x54, 0x04, 0x08}, Bytes{0x2E, 0x41, 0x0F, 0x38, opcode, 0x14, 0x24}, Bytes{0x0F, 0x38, opcode, 0x91, 0x00, 0x00, 0x00, 0x00}})
            require(runRegisterFormStub(memory, state, words) == sha256Reference(opcode, state, words, kStubScratch), "SHA-256 memory form stub computed the wrong result");
    }
}

struct StubRun {
    std::uint64_t Rax;
    std::uint64_t Rcx;
    std::uint64_t Flags;
    std::uint64_t Xmm[16][2];
    std::uint64_t SiteAddress;
};

StubRun runStubBody(const Codegen::Amd64OnlyMatch& match, const std::uint64_t rax, const std::uint64_t (&xmmIn)[16][2], const Bytes& data = {}) {
    require(match.Lowering == Codegen::Amd64OnlyLowering::Trampoline, "Stub was not produced");
    auto body = match.StubBody;
    const auto ret = body.size();
    body.push_back(0xC3);
    const auto displacement = static_cast<std::int32_t>(ret - (match.ReturnBranchOffset + 5));
    std::memcpy(body.data() + match.ReturnBranchOffset + 1, &displacement, sizeof(displacement));
    require(body.size() <= 4096 && data.size() <= 4096, "Stub or its data does not fit in a page");
    auto* code = static_cast<std::uint8_t*>(mmap(nullptr, 3 * 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    require(code != MAP_FAILED, "cannot map executable memory for the stub");
    StubRun run{};
    run.SiteAddress = reinterpret_cast<std::uint64_t>(code) + 4096;
    Codegen::ApplyStubRelocations(std::span<std::uint8_t>(body).first(match.ReturnBranchOffset), match.Relocations, run.SiteAddress, reinterpret_cast<std::uint64_t>(code), 0);
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

void clzeroExecution() {
    const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher();
    const Bytes plain = {0x0F, 0x01, 0xFC};
    const Bytes addressSize32 = {0x67, 0x0F, 0x01, 0xFC};
    const Bytes add = {0x48, 0x83, 0xC0, 0x40};
    const std::vector<std::span<const std::uint8_t>> pair = {plain, plain};
    struct Case {
        std::optional<Codegen::Amd64OnlyMatch> Match;
        bool Low;
        std::uint64_t Junk;
        std::uint64_t Advance;
        const char* Name;
    };
    const std::vector<Case> cases = {
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
        require(item.Match.has_value(), item.Name);
        auto* buffer = static_cast<std::uint8_t*>(mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | (item.Low ? MAP_32BIT : 0), -1, 0));
        require(buffer != MAP_FAILED, "cannot map the CLZERO buffer");
        for (const std::size_t offset : {std::size_t{0x80}, std::size_t{0xAD}, std::size_t{0xFF}}) {
            std::memset(buffer, 0xA5, 4096);
            const auto address = reinterpret_cast<std::uint64_t>(buffer + offset);
            const auto run = runStubBody(*item.Match, address | item.Junk, xmmIn);
            const auto line = offset & ~std::size_t{63};
            for (std::size_t index = 0; index < 4096; ++index)
                require(buffer[index] == (index >= line && index < line + 64 ? 0x00 : 0xA5), "CLZERO stub did not clear exactly the addressed line");
            require(run.Rax == (address | item.Junk) + item.Advance && run.Rcx == 0x1122334455667788ull, "CLZERO stub changed rax or rcx");
            for (unsigned reg = 0; reg < 16; ++reg)
                require(run.Xmm[reg][0] == xmmIn[reg][0] && run.Xmm[reg][1] == xmmIn[reg][1], "CLZERO stub clobbered an xmm register");
            if (item.Advance == 0)
                require((run.Flags & 0x8D5) == (0x8D7 & 0x8D5), "CLZERO stub changed RFLAGS");
        }
        munmap(buffer, 4096);
    }
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

void sha1Execution() {
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
        const auto operands = Codegen::DecodeSha1(site.data(), site.size());
        const auto match = matcher->Match(site.data(), site.size());
        require(match.has_value(), "SHA-1 stub was not produced");
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
            const auto run = runStubBody(*match, rax, xmmIn);
            for (unsigned reg = 0; reg < 16; ++reg) {
                if (reg == operands.Destination)
                    require(std::memcmp(run.Xmm[reg], expected.data(), 16) == 0, "SHA-1 stub computed the wrong result");
                else
                    require(run.Xmm[reg][0] == xmmIn[reg][0] && run.Xmm[reg][1] == xmmIn[reg][1], "SHA-1 stub clobbered an xmm register");
            }
            if (run.Rax != rax || run.Rcx != 0x1122334455667788ull || (run.Flags & 0x8D5) != (0x8D7 & 0x8D5)) {
                std::cerr << "SHA-1 sample=" << sample << " site=" << std::hex;
                for (const auto byte : site)
                    std::cerr << static_cast<unsigned>(byte) << ' ';
                std::cerr << "rax=" << run.Rax << " expected=" << rax << " rcx=" << run.Rcx << " flags=" << run.Flags << std::dec << '\n';
                throw std::runtime_error("SHA-1 stub changed a general register or RFLAGS");
            }
        }
    }
}

void ripRelativeExecution() {
    const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher();
    std::uint64_t xmmIn[16][2];
    for (unsigned reg = 0; reg < 16; ++reg) {
        xmmIn[reg][0] = 0x9e3779b97f4a7c15ull * (reg + 1);
        xmmIn[reg][1] = 0xc2b2ae3d27d4eb4full * (reg + 3);
    }
    const std::uint64_t words[2] = {0x510e527f9b05688cull, 0x1f83d9ab5be0cd19ull};
    Bytes data(0x40, 0xA5);
    std::memcpy(data.data() + 0x20, words, sizeof(words));
    const std::uint64_t rax = 0x5A5A5A5A5A5A5A5Aull;
    const auto unchanged = [&](const StubRun& run, const std::uint32_t changed, const char* message) {
        for (unsigned reg = 0; reg < 16; ++reg)
            if ((changed & (1u << reg)) == 0)
                require(run.Xmm[reg][0] == xmmIn[reg][0] && run.Xmm[reg][1] == xmmIn[reg][1], message);
        require(run.Rcx == 0x1122334455667788ull && (run.Flags & 0x8D5) == (0x8D7 & 0x8D5), message);
    };
    for (const std::uint8_t opcode : {std::uint8_t{0xCB}, std::uint8_t{0xCC}, std::uint8_t{0xCD}}) {
        Bytes site = {0x0F, 0x38, opcode, 0x15, 0x00, 0x00, 0x00, 0x00};
        write<std::int32_t>(site, 4, 4096 + 0x20 - 8);
        const auto match = matcher->Match(site.data(), site.size());
        require(match && match->Relocations.size() == 1, "SHA-256 RIP-relative stub was not produced");
        const auto run = runStubBody(*match, rax, xmmIn, data);
        require(std::array<std::uint64_t, 2>{run.Xmm[2][0], run.Xmm[2][1]} == sha256Reference(opcode, xmmIn[2], words, xmmIn[0]), "SHA-256 RIP-relative stub did not read the original operand");
        require(run.Rax == rax, "SHA-256 RIP-relative stub changed rax");
        unchanged(run, 1u << 2, "SHA-256 RIP-relative stub clobbered a register or RFLAGS");
    }
    Bytes rounds = {0x0F, 0x3A, 0xCC, 0x15, 0x00, 0x00, 0x00, 0x00, 0x02};
    write<std::int32_t>(rounds, 4, 4096 + 0x20 - 9);
    const auto roundsMatch = matcher->Match(rounds.data(), rounds.size());
    require(roundsMatch && roundsMatch->Relocations.size() == 1, "SHA1RNDS4 RIP-relative stub was not produced");
    std::uint32_t x[4];
    std::uint32_t y[4];
    std::memcpy(x, xmmIn[2], sizeof(x));
    std::memcpy(y, words, sizeof(y));
    const auto expected = sha1Reference(Codegen::DecodeSha1(rounds.data(), rounds.size()), x, y);
    const auto roundsRun = runStubBody(*roundsMatch, rax, xmmIn, data);
    require(std::memcmp(roundsRun.Xmm[2], expected.data(), 16) == 0, "SHA1RNDS4 RIP-relative stub did not read the original operand");
    unchanged(roundsRun, 1u << 2, "SHA1RNDS4 RIP-relative stub clobbered a register or RFLAGS");
    const Bytes extrq = {0x66, 0x0F, 0x79, 0xCA};
    Bytes followers = {0xF3, 0x0F, 0x6F, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00};
    write<std::int32_t>(followers, 4, 4096 + 0x20 - 12);
    write<std::int32_t>(followers, 11, 4096 + 0x30 - 19);
    const auto moved = matcher->Match(extrq.data(), extrq.size(), followers);
    require(moved && moved->Relocations.size() == 2, "EXTRQ with RIP-relative followers was not lowered with two relocations");
    const auto movedRun = runStubBody(*moved, rax, xmmIn, data);
    require(movedRun.Xmm[3][0] == words[0] && movedRun.Xmm[3][1] == words[1], "Moved RIP-relative MOVDQU did not load from the original address");
    require(movedRun.Rax == movedRun.SiteAddress + 4096 + 0x30, "Moved RIP-relative LEA did not compute the original address");
    unchanged(movedRun, (1u << 1) | (1u << 3), "Stub with moved RIP-relative instructions clobbered a register or RFLAGS");
}

void rdpruExecution() {
    const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher();
    const Bytes site = {0x0F, 0x01, 0xFD};
    const auto match = matcher->Match(site.data(), site.size());
    require(match && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline, "RDPRU stub was not produced");
    auto body = match->StubBody;
    const auto ret = body.size();
    body.push_back(0xC3);
    const auto displacement = static_cast<std::int32_t>(ret - (match->ReturnBranchOffset + 5));
    std::memcpy(body.data() + match->ReturnBranchOffset + 1, &displacement, sizeof(displacement));
    void* code = mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    require(code != MAP_FAILED, "cannot map executable memory for the RDPRU stub");
    std::memcpy(code, body.data(), body.size());
    for (const std::uint32_t selector : {0u, 1u, 2u, 0xFFFFFFFFu}) {
        std::uint64_t raxOut = 0;
        std::uint64_t rdxOut = 0;
        std::uint64_t rcxOut = 0;
        std::uint64_t flagsOut = 0;
        std::uint64_t canaryOut = 0;
        asm volatile(
            "sub $128, %%rsp\n\t"
            "movabs $0xA5A5A5A5A5A5A5A5, %%rax\n\t"
            "lea -136(%%rsp), %%rdi\n\t"
            "mov $16, %%ecx\n\t"
            "rep stosq\n\t"
            "movl %[selector], %%ecx\n\t"
            "pushq $0x8D7\n\t"
            "popfq\n\t"
            "call *%[code]\n\t"
            "mov %%rax, %[rax]\n\t"
            "mov %%rdx, %[rdx]\n\t"
            "mov %%rcx, %[rcx]\n\t"
            "pushfq\n\t"
            "popq %[flags]\n\t"
            "xor %%r8, %%r8\n\t"
            "lea -136(%%rsp), %%rsi\n\t"
            "mov $16, %%ecx\n\t"
            "1:\n\t"
            "xor (%%rsi), %%r8\n\t"
            "add $8, %%rsi\n\t"
            "dec %%ecx\n\t"
            "jnz 1b\n\t"
            "mov %%r8, %[canary]\n\t"
            "add $128, %%rsp\n\t"
            : [rax] "=r"(raxOut), [rdx] "=r"(rdxOut), [rcx] "=r"(rcxOut), [flags] "=r"(flagsOut), [canary] "=r"(canaryOut)
            : [selector] "r"(selector), [code] "r"(code)
            : "rax", "rcx", "rdx", "rsi", "rdi", "r8", "memory", "cc");
        require(canaryOut == 0, "RDPRU stub clobbered the red zone");
        require(rcxOut == selector, "RDPRU stub changed ecx");
        require((flagsOut & 0x8D5) == (0x8D7 & 0x8D5), "RDPRU stub changed RFLAGS");
        if (selector <= 1)
            require((raxOut | rdxOut) != 0, "RDPRU MPERF/APERF selector did not yield the TSC approximation");
        else
            require(raxOut == 0 && rdxOut == 0, "RDPRU with an undefined selector did not yield zero");
    }
    munmap(code, 4096);
}

void mcommitExecution() {
    const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher();
    const Bytes site = {0xF3, 0x0F, 0x01, 0xFA};
    const auto match = matcher->Match(site.data(), site.size());
    require(match && match->Lowering == Codegen::Amd64OnlyLowering::InPlace, "MCOMMIT lowering was not produced");
    auto body = match->ReplacementBytes;
    body.push_back(0xC3);
    void* code = mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    require(code != MAP_FAILED, "cannot map executable memory for the MCOMMIT lowering");
    std::memcpy(code, body.data(), body.size());
    for (const std::uint64_t flagsIn : {std::uint64_t{0x8D6}, std::uint64_t{0x8D7}}) {
        std::uint64_t flagsOut = 0;
        asm volatile(
            "mov %[in], %%rax\n\t"
            "pushq %%rax\n\t"
            "popfq\n\t"
            "call *%[code]\n\t"
            "pushfq\n\t"
            "popq %[out]\n\t"
            : [out] "=r"(flagsOut)
            : [in] "r"(flagsIn), [code] "r"(code)
            : "rax", "memory", "cc");
        require((flagsOut & 1) == 1, "MCOMMIT lowering did not report committed");
        require((flagsOut & 0x8D4) == (flagsIn & 0x8D4), "MCOMMIT lowering changed flags other than CF");
    }
    munmap(code, 4096);
}

void wideStoreExecution() {
    const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const Bytes site = {0xC5, 0xFC, 0x11, 0x00};
    const auto match = matcher->Match(site.data(), site.size());
    require(match && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline, "256-bit store stub was not produced");
    auto body = match->StubBody;
    body[match->ReturnBranchOffset] = 0xC3;
    void* code = mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    require(code != MAP_FAILED, "cannot map executable memory for the stub");
    std::memcpy(code, body.data(), body.size());
    alignas(32) std::uint8_t source[32];
    for (std::size_t index = 0; index < sizeof(source); ++index) source[index] = static_cast<std::uint8_t>(index + 1);
    auto* pages = static_cast<std::uint8_t*>(mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    require(pages != MAP_FAILED, "cannot map the destination pages");
    for (const std::size_t offset : {std::size_t{0}, std::size_t{1}, std::size_t{4080}}) {
        std::memset(pages, 0, 8192);
        auto* destination = pages + offset;
        asm volatile(
            "vmovups (%[source]), %%ymm0\n\t"
            "sub $128, %%rsp\n\t"
            "call *%[code]\n\t"
            "add $128, %%rsp\n\t"
            :
            : [source] "r"(source), [code] "r"(code), "a"(destination)
            : "memory", "ymm0");
        require(std::memcmp(destination, source, sizeof(source)) == 0, "256-bit store stub did not write both halves");
        require(std::all_of(pages, destination, [](const std::uint8_t value) { return value == 0; }), "256-bit store stub wrote before its operand");
        require(std::all_of(destination + sizeof(source), pages + 8192, [](const std::uint8_t value) { return value == 0; }),
                "256-bit store stub wrote past its operand");
    }
    munmap(pages, 8192);
    munmap(code, 4096);
}
#else
void reciprocalExecution() {}
void registerFormExecution() {}
void sha1Execution() {}
void sha256Execution() {}
void clzeroExecution() {}
void immediateFormExecution() {}
void ripRelativeExecution() {}
void rdpruExecution() {}
void mcommitExecution() {}
#endif

#if defined(__x86_64__) && (defined(__linux__) || defined(__APPLE__))
void rdtscpExecution() {
    const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const Bytes site = {0x0F, 0x01, 0xF9};
    const auto match = matcher->Match(site.data(), site.size());
    require(match && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline, "RDTSCP stub was not produced");
    auto body = match->StubBody;
    body[match->ReturnBranchOffset] = 0xC3;
    void* code = mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    require(code != MAP_FAILED, "cannot map executable memory for the stub");
    std::memcpy(code, body.data(), body.size());
    std::uint64_t low = 0;
    std::uint64_t high = 0;
    std::uint64_t processor = 0;
    std::uint8_t carry = 0;
    asm volatile(
        "mov $-1, %%rcx\n\t"
        "sub $128, %%rsp\n\t"
        "stc\n\t"
        "call *%[code]\n\t"
        "setc %[carry]\n\t"
        "add $128, %%rsp\n\t"
        : "=&a"(low), "=&d"(high), "=&c"(processor), [carry] "=&q"(carry)
        : [code] "r"(code)
        : "memory", "cc");
    munmap(code, 4096);
    require(((high << 32) | low) != 0, "RDTSCP stub did not read the time stamp counter");
    require(processor == 0, "RDTSCP stub left a value other than processor 0 in RCX");
    require(carry == 1, "RDTSCP stub changed the flags");
}
#else
void rdtscpExecution() {}
void wideStoreExecution() {}
#endif

void scannerZeroTail() {
    const auto scanner = Codegen::MakeInstructionScanner();
    const Bytes code{0xC3, 0x00, 0x00, 0x00};
    require(scanner->ScanCodeSection(code, 0, code.size()).size() == 2, "Odd zero padding at segment tail must end the scan");
    const Bytes truncated{0xC3, 0x0F};
    requireFailure([&] { (void)scanner->ScanCodeSection(truncated, 0, truncated.size()); }, "Truncated non-zero tail must still fail");
}

void scannerUnwindRanges() {
    const Bytes slot{0x48, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x2E, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC3};
    const auto gap = Codegen::MakeInstructionScanner({}, {{16, 17}})->ScanCodeSection(slot, 0x1000, slot.size());
    require(gap.size() == 2 && gap[0].Offset == 0x1001 && gap[0].Length == 15 && gap[1].Offset == 0x1010 && gap[1].Length == 1, "An undecodable byte outside every unwind range must resume the scan at the next byte");
    requireFailure([&] { (void)Codegen::MakeInstructionScanner({}, {{0, 17}})->ScanCodeSection(slot, 0, slot.size()); }, "An undecodable byte inside an unwind range must still fail");
    requireFailure([&] { (void)Codegen::MakeInstructionScanner()->ScanCodeSection(slot, 0, slot.size()); }, "An undecodable byte without unwind ranges must still fail");
    const Bytes crossing{0xB8, 0x0F, 0x05, 0xC3, 0x90};
    const auto cut = Codegen::MakeInstructionScanner({}, {{3, 4}, {1, 3}})->ScanCodeSection(crossing, 0, crossing.size());
    require(cut.size() == 3 && cut[0].Offset == 1 && cut[0].Length == 2 && cut[1].Offset == 3 && cut[2].Offset == 4, "A gap instruction that crosses into an unwind range must end at the range");
    require(Codegen::MakeInstructionScanner()->ScanCodeSection(crossing, 0, crossing.size()).front().Length == 5, "Without unwind ranges the crossing instruction must decode whole");
}

int main() {
    try {
        decoderLengths();
        decoderRipRelative();
        decoderTwoByteOpcodeLengths();
        sse4aOperands();
        sha256Operands();
        sha1Operands();
        clzeroOperands();
        reciprocalOperands();
        matcherSubstitutions();
        goldenBodies();
        ripRelativeBodies();
        stubRelocations();
        registerFormExecution();
        immediateFormExecution();
        sha256Execution();
        sha1Execution();
        clzeroExecution();
        reciprocalExecution();
        ripRelativeExecution();
        rdpruExecution();
        mcommitExecution();
        converterSegment();
        converterRipRelativeFollower();
        converterSha256();
        converterSha1();
        converterMonitorWait();
        converterInPlaceFollower();
        converterClzero();
        rosettaSubstitutions();
        converterRosetta();
        rosettaScalarCompares();
        converterRosettaScalarCompare();
        rosettaLegacyScalarCompares();
        converterRosettaLegacyScalarCompare();
        rdtscpExecution();
        wideStoreExecution();
        wideStoreSplit();
        converterWideStoreWindows();
        converterClaimedFollower();
        converterJumpTableTargets();
        converterReciprocal();
        converterStrayRex();
        rewriterStrayRex();
    rewriterReferenceSites();
        converterFailureOffsets();
        linuxPlacement();
        linuxRipRelativePlacement();
        linuxStackSynthesis();
        scannerZeroTail();
        scannerUnwindRanges();
        std::cout << "AMD64-only converter tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
