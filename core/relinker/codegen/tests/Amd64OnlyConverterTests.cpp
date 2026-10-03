#include <codegen/IAmd64OnlyConverter.hpp>
#include <codegen/CodegenException.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/x86/Sse4aLowering.hpp>
#include <codegen/x86/Sse4aOperands.hpp>
#include <codegen/x86/Sha256Operands.hpp>
#include <codegen/x86/ClzeroOperands.hpp>
#include <codegen/x86/ClzeroLowering.hpp>
#include <codegen/x86/DecodedInstruction.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/x86/X64InstructionRewriter.hpp>
#include <codegen/IInstructionScanner.hpp>
#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <elfpatcher/general/ProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/general/SectionHeaderTableBuilder.hpp>
#include <elfpatcher/general/SegmentFilter.hpp>
#include <elfpatcher/linux/LinuxElfPatcher.hpp>
#include <io/ByteWriter.hpp>
#include <array>
#include <cstring>
#include <cstdint>
#ifdef __linux__
#include <sys/mman.h>
#endif
#include <functional>
#include <optional>
#include <span>
#include <vector>
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
        {0x0F, 0x38, 0xCB, 0xCA}, {0x45, 0x0F, 0x38, 0xCC, 0xE1}, {0x0F, 0x38, 0xCD, 0x08}, {0x0F, 0x38, 0xCB, 0x0D, 0x10, 0x00, 0x00, 0x00},
        {0x48, 0x66, 0xB8, 0x34, 0x12}, {0x66, 0x48, 0xB8, 1, 2, 3, 4, 5, 6, 7, 8}, {0x41, 0x48, 0xB8, 1, 2, 3, 4, 5, 6, 7, 8},
        {0x48, 0x64, 0x8B, 0x00}, {0x48, 0xF3, 0x0F, 0x2B, 0x00}, {0x48, 0x67, 0x0F, 0x01, 0xFC}, {0x48, 0x48, 0x0F, 0x01, 0xFC}};
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
    requireFailure([] { const Bytes bytes = {0x0F, 0x38, 0xCC, 0x15, 0, 0, 0, 0}; (void)Codegen::DecodeSha256(bytes.data(), bytes.size()); }, "SHA-256 RIP-relative operand was accepted");
    requireFailure([] { const Bytes bytes = {0x0F, 0x38, 0xCC, 0x54, 0x24}; (void)Codegen::DecodeSha256(bytes.data(), bytes.size()); }, "Truncated SHA-256 memory operand was accepted");
    requireFailure([] { const Bytes bytes = {0x66, 0x0F, 0x38, 0xCB, 0xCA}; (void)Codegen::DecodeSha256(bytes.data(), bytes.size()); }, "Prefixed 0F 38 CB was decoded as SHA-256");
    requireFailure([] { const Bytes bytes = {0x0F, 0x38, 0xC9, 0xCA}; (void)Codegen::DecodeSha256(bytes.data(), bytes.size()); }, "SHA-1 was decoded as SHA-256");
    check({0x41, 0x2E, 0x0F, 0x38, 0xCC, 0xCA}, Codegen::Sha256Operation::Msg1, 1, 2);
    check({0x2E, 0x41, 0x0F, 0x38, 0xCC, 0xCA}, Codegen::Sha256Operation::Msg1, 1, 10);
    const Bytes rounds = {0x0F, 0x38, 0xCB, 0xCA};
    require(Codegen::DecodedInstruction{rounds.data(), rounds.size()}.IsShaNi(), "SHA256RNDS2 is not recognised as SHA-NI");
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
    require(rdpru && rdpru->Lowering == Codegen::Amd64OnlyLowering::Unsupported && rdpru->InstructionName == "RDPRU", "RDPRU was not reported as unsupported");
    const auto registerForm = match({0x66, 0x0F, 0x79, 0xCA});
    require(registerForm && registerForm->Lowering == Codegen::Amd64OnlyLowering::Trampoline && registerForm->InstructionName == "EXTRQ register form", "EXTRQ register form was not lowered through a stub");
    const auto insertqRegisterForm = match({0xF2, 0x0F, 0x79, 0xCA});
    require(insertqRegisterForm && insertqRegisterForm->Lowering == Codegen::Amd64OnlyLowering::Trampoline && insertqRegisterForm->InstructionName == "INSERTQ register form", "INSERTQ register form was not lowered through a stub");
    require(!match({0x66, 0x0F, 0x2B, 0x07}) && !match({0x0F, 0x2B, 0x07}) && !match({0x48, 0x8B, 0x05, 0, 0, 0, 0}), "Ordinary instruction was matched");
    for (const auto& [bytes, name] : {std::pair{Bytes{0x0F, 0x38, 0xCB, 0xCA}, "SHA256RNDS2"}, {Bytes{0x0F, 0x38, 0xCC, 0xCA}, "SHA256MSG1"}, {Bytes{0x45, 0x0F, 0x38, 0xCD, 0xE1}, "SHA256MSG2"}}) {
        const auto sha256 = match(bytes);
        require(sha256 && sha256->Lowering == Codegen::Amd64OnlyLowering::Trampoline && sha256->InstructionName == name, "SHA-256 instruction was not lowered through a stub");
    }
    require(!match({0x0F, 0x38, 0xC9, 0xCA}), "SHA-1 instruction was matched");
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
    auto rdpru = file;
    rdpru[0x20F] = 0x0F;
    rdpru[0x210] = 0x01;
    rdpru[0x211] = 0xFD;
    rdpru[0x212] = 0x90;
    requireFailure([&] { (void)converter->Convert(rdpru, {segmentHeader(20)}); }, "RDPRU was silently kept");
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
    const Bytes ripMessage = {0x0F, 0x38, 0xCD, 0x2D, 0x00, 0x00, 0x00, 0x00, 0xC3};
    std::copy(ripMessage.begin(), ripMessage.end(), ripRelative.begin() + 0x20C);
    require(failureOffset([&] { (void)converter->Convert(ripRelative, {segmentHeader(text.size() + 3)}); }, "SHA-256 RIP-relative form was accepted") == 0x20C, "SHA-256 operand failure does not carry the file offset");
}

void converterMonitorWait() {
    const auto converter = Codegen::MakeAmd64OnlyConverter();
    Bytes file(0x300, 0xCC);
    const Bytes text = {0x0F, 0x01, 0xFA, 0x0F, 0x01, 0xFB, 0x2E, 0x0F, 0x01, 0xFB, 0xC3};
    std::copy(text.begin(), text.end(), file.begin() + 0x200);
    const auto result = converter->Convert(file, {segmentHeader(text.size())});
    require(result.ReplacedCount == 3 && result.Trampolines.empty() && result.Reports.size() == 3, "MONITORX/MWAITX were not replaced in place");
    require(result.Reports[0].InstructionName == "MONITORX" && result.Reports[1].InstructionName == "MWAITX" && result.Reports[2].Offset == 0x206 && result.Reports[2].ReplacementLength == 4 && result.Reports[2].Lowering == Codegen::Amd64OnlyLowering::InPlace, "MONITORX/MWAITX reports are wrong");
    auto expected = file;
    const Bytes replaced = {0x0F, 0x1F, 0x00, 0xF3, 0x90, 0x90, 0xF3, 0x90, 0x66, 0x90, 0xC3};
    std::copy(replaced.begin(), replaced.end(), expected.begin() + 0x200);
    require(result.Bytes == expected, "MONITORX/MWAITX were replaced with the wrong bytes");
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

void converterStrayRex() {
    const auto converter = Codegen::MakeAmd64OnlyConverter();
    const auto convert = [&](const Bytes& text) {
        Bytes file(0x300, 0xCC);
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
    auto rdpru = file;
    const Bytes rdpruBytes = {0x0F, 0x01, 0xFD, 0x90};
    std::copy(rdpruBytes.begin(), rdpruBytes.end(), rdpru.begin() + 0x20F);
    require(failureOffset([&] { (void)converter->Convert(rdpru, {segmentHeader(20)}); }, "RDPRU was accepted") == 0x20F, "Unsupported instruction failure does not carry the file offset");
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
    require(match && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline, "Register form stub was not produced");
    auto body = match->StubBody;
    const auto ret = body.size();
    body.push_back(0xC3);
    const auto displacement = static_cast<std::int32_t>(ret - (match->ReturnBranchOffset + 5));
    std::memcpy(body.data() + match->ReturnBranchOffset + 1, &displacement, sizeof(displacement));
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

void registerFormExecution() {
    const Bytes extrqDistinct = {0x66, 0x0F, 0x79, 0xD5};
    const Bytes extrqSame = {0x66, 0x0F, 0x79, 0xD2};
    const Bytes insertqDistinct = {0xF2, 0x0F, 0x79, 0xD5};
    const Bytes insertqSame = {0xF2, 0x0F, 0x79, 0xD2};
    const std::uint64_t value = 0x9e3779b97f4a7c15ull;
    const std::uint64_t destination = 0x0f1e2d3c4b5a6978ull;
    for (const auto [length, index] : {std::pair{8u, 4u}, {0u, 0u}, {40u, 20u}, {63u, 1u}, {1u, 63u}, {16u, 48u}, {1u, 0u}, {32u, 32u}}) {
        const auto control = static_cast<std::uint64_t>(length) | (static_cast<std::uint64_t>(index) << 8) | 0xffffc000ull;
        require(runRegisterFormStub(extrqDistinct, {value, 0x1122334455667788ull}, {control, 0})[0] == extrqReference(value, control), "EXTRQ register form stub computed the wrong field");
        require(runRegisterFormStub(extrqSame, {control, 0}, {control, 0})[0] == extrqReference(control, control), "EXTRQ register form stub with equal operands computed the wrong field");
        const auto insertqControl = control | 0xC0ull;
        require(runRegisterFormStub(insertqDistinct, {destination, 0x1122334455667788ull}, {value, insertqControl})[0] == insertqReference(destination, value, insertqControl), "INSERTQ register form stub computed the wrong field");
        require(runRegisterFormStub(insertqSame, {value, insertqControl}, {value, insertqControl})[0] == insertqReference(value, value, insertqControl), "INSERTQ register form stub with equal operands computed the wrong field");
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

struct ClzeroRun {
    std::uint64_t Rax;
    std::uint64_t Rcx;
    std::uint64_t Flags;
    std::uint64_t Xmm[16][2];
};

ClzeroRun runClzeroBody(const Codegen::Amd64OnlyMatch& match, const std::uint64_t rax, const std::uint64_t (&xmmIn)[16][2]) {
    require(match.Lowering == Codegen::Amd64OnlyLowering::Trampoline, "CLZERO stub was not produced");
    auto body = match.StubBody;
    const auto ret = body.size();
    body.push_back(0xC3);
    const auto displacement = static_cast<std::int32_t>(ret - (match.ReturnBranchOffset + 5));
    std::memcpy(body.data() + match.ReturnBranchOffset + 1, &displacement, sizeof(displacement));
    void* code = mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    require(code != MAP_FAILED, "cannot map executable memory for the stub");
    std::memcpy(code, body.data(), body.size());
    ClzeroRun run{};
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
    munmap(code, 4096);
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
            const auto run = runClzeroBody(*item.Match, address | item.Junk, xmmIn);
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
#else
void registerFormExecution() {}
void sha256Execution() {}
void clzeroExecution() {}
#endif

void scannerZeroTail() {
    const auto scanner = Codegen::MakeInstructionScanner();
    const Bytes code{0xC3, 0x00, 0x00, 0x00};
    require(scanner->ScanCodeSection(code, 0, code.size()).size() == 2, "Odd zero padding at segment tail must end the scan");
    const Bytes truncated{0xC3, 0x0F};
    requireFailure([&] { (void)scanner->ScanCodeSection(truncated, 0, truncated.size()); }, "Truncated non-zero tail must still fail");
}

int main() {
    try {
        decoderLengths();
        sse4aOperands();
        sha256Operands();
        clzeroOperands();
        matcherSubstitutions();
        goldenBodies();
        registerFormExecution();
        sha256Execution();
        clzeroExecution();
        converterSegment();
        converterSha256();
        converterMonitorWait();
        converterClzero();
        converterStrayRex();
        rewriterStrayRex();
        converterFailureOffsets();
        linuxPlacement();
        scannerZeroTail();
        std::cout << "AMD64-only converter tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
