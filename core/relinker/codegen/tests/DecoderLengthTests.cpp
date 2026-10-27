#include "DecoderLengthCases.hpp"
#include "DecoderMetadataCases.hpp"
#include <codegen/CodegenException.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Bytes parse(std::string_view hex) {
    Bytes bytes;
    for (std::size_t index = 0; index + 1 < hex.size(); index += 2) {
        bytes.push_back(static_cast<std::uint8_t>(std::stoul(std::string(hex.substr(index, 2)), nullptr, 16)));
    }
    return bytes;
}

std::size_t decodedLength(const Codegen::X64InstructionDecoder& decoder, const Bytes& bytes, const std::string& name) {
    try {
        return decoder.Decode(bytes.data(), bytes.size());
    } catch (const Codegen::CodegenException& error) {
        throw std::runtime_error("Cannot decode " + name + ": " + error.what());
    }
}

bool truncationRejected(const Codegen::X64InstructionDecoder& decoder, const Bytes& bytes, std::size_t available) {
    try {
        (void)decoder.Decode(bytes.data(), available);
    } catch (const Codegen::CodegenException&) {
        return true;
    }
    return false;
}

}

int main() {
    try {
        const Codegen::X64InstructionDecoder decoder;
        for (const auto& testCase : kDecoderLengthCases) {
            const std::string name = testCase.Bytes;
            auto bytes = parse(testCase.Bytes);
            require(bytes.size() == testCase.Length, "Decoder length case " + name + " does not hold its own length");
            require(decodedLength(decoder, bytes, name) == testCase.Length, "Wrong length for " + name);
            require(truncationRejected(decoder, bytes, bytes.size() - 1), "A truncated " + name + " was accepted");
            bytes.insert(bytes.end(), 15, 0x90);
            require(decodedLength(decoder, bytes, name + " followed by NOPs") == testCase.Length, "Wrong length for " + name + " followed by NOPs");
        }
        for (const auto& testCase : std::vector<std::pair<std::size_t, Bytes>>{
            {14, {0x90}},
            {12, {0x68, 0x00, 0x00}},
            {8, {0x8b, 0x84, 0x25, 0x00, 0x00, 0x00, 0x00}}
        }) {
            Bytes longest(testCase.first, 0x66);
            longest.insert(longest.end(), testCase.second.begin(), testCase.second.end());
            require(longest.size() == 15, "Maximum-length instruction fixture is not 15 bytes");
            require(decoder.Decode(longest.data(), longest.size()) == 15, "Valid 15-byte instruction was rejected");
            require(decoder.DecodeInstruction(longest.data(), longest.size()).Length == 15, "Valid 15-byte instruction metadata was rejected");
            Bytes withTrailingNops = longest;
            withTrailingNops.insert(withTrailingNops.end(), 15, 0x90);
            require(decoder.Decode(withTrailingNops.data(), withTrailingNops.size()) == 15, "Valid maximum-length instruction consumed trailing bytes");
            longest.insert(longest.begin(), 0x66);
            require(truncationRejected(decoder, longest, longest.size()), "Instruction longer than 15 bytes was accepted");
            try {
                (void)decoder.DecodeInstruction(longest.data(), longest.size());
                throw std::runtime_error("Instruction metadata accepted a length above 15 bytes");
            } catch (const Codegen::CodegenException&) {
            }
        }
        for (const auto& testCase : kDecoderRipRelativeCases) {
            const std::string name = testCase.Bytes;
            auto bytes = parse(testCase.Bytes);
            require(bytes.size() == testCase.Length, "RIP-relative case " + name + " does not hold its own length");
            bytes.insert(bytes.end(), 15, 0x90);
            require(decodedLength(decoder, bytes, name) == testCase.Length, "Wrong length for " + name);
            const auto info = decoder.DecodeInstruction(bytes.data(), bytes.size());
            require(info.HasRipRelativeDisp, "RIP-relative operand of " + name + " was not reported");
            require(info.RipRelativeDispOffset == testCase.DisplacementOffset, "Wrong RIP-relative displacement offset for " + name);
        }
        for (const auto& testCase : kDecoderBranchCases) {
            const std::string name = testCase.Bytes;
            auto bytes = parse(testCase.Bytes);
            require(bytes.size() == testCase.Length, "Branch case " + name + " does not hold its own length");
            bytes.insert(bytes.end(), 15, 0x90);
            const auto info = decoder.DecodeInstruction(bytes.data(), bytes.size());
            require(info.Length == testCase.Length, "Wrong length for " + name);
            require(info.FlowKind == testCase.FlowKind, "Wrong control flow kind for " + name);
            require(info.HasBranchTarget == testCase.HasBranchTarget, "Wrong branch target presence for " + name);
            require(!testCase.HasBranchTarget || info.BranchDisp == testCase.BranchDisp, "Wrong branch displacement for " + name);
        }
        for (const auto& testCase : kDecoderTruncatedBranchCases) {
            const std::string name = testCase.Bytes;
            auto bytes = parse(testCase.Bytes);
            require(bytes.size() == testCase.Length, "Truncated branch case " + name + " does not hold its own length");
            bytes.insert(bytes.end(), 15, 0x90);
            const auto info = decoder.DecodeInstruction(bytes.data(), bytes.size());
            require(info.Length == testCase.Length, "Wrong length for " + name);
            require(info.FlowKind == testCase.FlowKind, "Wrong control flow kind for " + name);
            require(info.TruncatesBranchTarget && !info.HasBranchTarget, "Branch target of " + name + " was reported without its 16-bit truncation");
        }
        for (const auto& testCase : kDecoderBranchCases) {
            const auto bytes = parse(testCase.Bytes);
            require(!decoder.DecodeInstruction(bytes.data(), bytes.size()).TruncatesBranchTarget, "Branch target of " + std::string(testCase.Bytes) + " was reported as truncated");
        }
        for (const auto segment : {0x64, 0x65}) {
            for (const auto opcode : {0x6c, 0x6d, 0xaa, 0xab, 0xae, 0xaf, 0x6e, 0x6f, 0xa4, 0xa5, 0xa6, 0xa7, 0xac, 0xad}) {
                const std::vector<std::uint8_t> bytes{static_cast<std::uint8_t>(segment), static_cast<std::uint8_t>(opcode)};
                const auto info = decoder.DecodeInstruction(bytes.data(), bytes.size());
                const bool fixedEs = opcode == 0x6c || opcode == 0x6d || opcode == 0xaa || opcode == 0xab || opcode == 0xae || opcode == 0xaf;
                require(info.HasMemoryOperand, "A string instruction lost its implicit memory operand");
                require(info.SegmentPrefix == (fixedEs ? 0 : segment), "Wrong effective segment for a string instruction");
            }
        }
        const auto threeDNow = parse("0f0f0d11223344a6");
        const auto threeDNowInfo = decoder.DecodeInstruction(threeDNow.data(), threeDNow.size());
        require(threeDNowInfo.HasModRm && threeDNowInfo.ModRmByte == 0x0d, "3DNow! ModRM was not reported");
        require(threeDNowInfo.HasRipRelativeDisp && threeDNowInfo.RipRelativeDispOffset == 3, "3DNow! RIP-relative displacement was not reported");
        for (const auto& testCase : std::vector<std::pair<const char*, bool>>{
            {"6590", false},
            {"654889c8", false},
            {"658d0511111111", false},
            {"650f1f0511111111", false},
            {"65488b0c2578030000", true},
            {"65a4", true},
            {"650fa2", false},
            {"650ff7c1", true},
            {"65660ff7c1", true},
            {"65c5f9f7c1", true},
            {"65c4e179f7c1", true},
            {"650f01c8", true},
            {"650f01fa", true},
            {"650f01fc", true},
            {"650f01c9", false},
            {"650f01fd", false},
            {"65f30faef0", true},
            {"f20faef0", false},
            {"660faef0", false},
            {"f30faee8", false},
            {"ff10", true},
            {"ffe0", false},
        }) {
            const auto bytes = parse(testCase.first);
            require(decoder.DecodeInstruction(bytes.data(), bytes.size()).HasMemoryOperand == testCase.second,
                    "Wrong memory operand presence for " + std::string(testCase.first));
        }
        std::cout << "Decoder length tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
