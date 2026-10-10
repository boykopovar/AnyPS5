#include "DecoderLengthCases.hpp"
#include <codegen/CodegenException.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
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
        std::cout << "Decoder length tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
