#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include <array>
#include <bit>
#include <stdexcept>
#include <source_location>
#include <string>
#include <vector>

using namespace ShaderRecompiler;

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) throw std::runtime_error("image PCK2/PCK4 regression at line " + std::to_string(location.line()));
}

constexpr std::array<std::uint32_t, 8> Encodings{0x70u, 0x71u, 0x73u, 0x74u, 0x76u, 0x77u, 0x79u, 0x7au};
constexpr std::array<RdnaOpcode, 8> Opcodes{RdnaOpcode::ImageLoadPck2, RdnaOpcode::ImageLoadPck4, RdnaOpcode::ImageLoadMipPck2, RdnaOpcode::ImageLoadMipPck4, RdnaOpcode::ImageStorePck2, RdnaOpcode::ImageStorePck4, RdnaOpcode::ImageStoreMipPck2, RdnaOpcode::ImageStoreMipPck4};

RdnaInstruction Decode(std::uint32_t encoding, std::uint32_t controls = 0u, std::uint32_t word1 = 0x00101e1eu, std::uint32_t mask = 15u, std::uint32_t dimension = 1u) {
    const std::array<std::uint32_t, 3> code{0xf0000000u | (dimension << 3u) | (encoding << 18u) | (mask << 8u) | controls, word1, 0x0000201fu};
    return DecodeRdnaMimg(0u, code, 0u);
}

void Check(std::uint32_t index, bool nsa, std::uint32_t mask) {
    const auto encoding = Encodings[index];
    const auto inst = Decode(encoding, nsa ? 2u : 0u, 0x00101e1eu, mask);
    const auto elements = encoding == 0x71u || encoding == 0x74u || encoding == 0x77u || encoding == 0x7au ? 4u : 2u;
    const bool mip = encoding == 0x73u || encoding == 0x74u || encoding == 0x79u || encoding == 0x7au;
    const auto dwords = static_cast<std::uint32_t>(std::popcount(mask));
    const bool store = encoding >= 0x76u;
    Require(inst.op == Opcodes[index] && IsImageOpcode(inst.op));
    Require(inst.dataDwordCount == dwords && inst.dataComponents == dwords);
    Require(inst.imageAddressComponents == (mip ? 3u : 2u));
    Require(inst.wordCount == (nsa ? 3u : 2u));
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    if (store) {
        bool refused = false;
        try { context.TranslateInstruction(inst); }
        catch (const std::runtime_error& error) { refused = std::string(error.what()).find("stores are not implemented") != std::string::npos; }
        Require(refused);
        return;
    }
    context.TranslateInstruction(inst);
    std::vector<IrValue*> reads;
    std::uint32_t writes = 0;
    for (auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::SetVectorRegister) ++writes;
        if (value->Opcode() == IrOpcode::ImageRead) {
            Require(writes == 0u);
            reads.push_back(value);
        }
    }
    Require(reads.size() == 1u && writes == dwords);
    const auto& memory = program.Resources().memoryInfo[reads[0]->Flags<MemoryFlags>().index];
    Require(memory.imageByElements == elements && memory.imagePacked && memory.dmask == mask && memory.dataDwords == dwords);
    Require(memory.imageHasMip == mip);
}

void CheckOrdinary(std::uint32_t encoding) {
    const auto inst = Decode(encoding);
    const bool store = (encoding & 8u) != 0u;
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(inst);
    std::uint32_t operations = 0;
    for (auto* value : block.Instructions()) {
        if (value->Opcode() == (store ? IrOpcode::ImageWrite : IrOpcode::ImageRead)) {
            const auto& memory = program.Resources().memoryInfo[value->Flags<MemoryFlags>().index];
            Require(memory.imageByElements == 0u && memory.dmask == 15u && memory.dataDwords == 4u);
            ++operations;
        }
    }
    Require(operations == 1u);
}

void Refused(std::uint32_t encoding, std::uint32_t control, std::uint32_t word1 = 0x00101e1eu, std::uint32_t mask = 15u, std::uint32_t dimension = 1u) {
    bool refused = false;
    try { (void)Decode(encoding, control, word1, mask, dimension); }
    catch (const std::runtime_error&) { refused = true; }
    Require(refused);
}

}

int main() {
    for (auto encoding : {0u, 1u, 8u, 9u}) CheckOrdinary(encoding);
    for (std::uint32_t index = 0; index < Encodings.size(); ++index) {
        for (const bool nsa : {false, true}) Check(index, nsa, 1u);
        for (std::uint32_t mask = 0u; mask < 16u; ++mask) {
            if (mask != 1u) Refused(Encodings[index], 0u, 0x00101e1eu, mask);
        }
        for (auto bits : {0x8000u, 0x10000u, 0x20000u}) Refused(Encodings[index], bits, 0x00101e1eu, 1u);
        for (auto bits : {0x40000000u, 0x80000000u}) Refused(Encodings[index], 0u, 0x00101e1eu | bits, 1u);
        Refused(Encodings[index], 0u, 0x00101effu, 1u);
        for (std::uint32_t dimension = 0; dimension < 8u; ++dimension) {
            if (dimension != 1u) Refused(Encodings[index], 0u, 0x00101e1eu, 1u, dimension);
        }
    }
}
