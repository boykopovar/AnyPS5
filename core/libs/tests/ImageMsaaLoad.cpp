#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include <array>
#include <stdexcept>
#include <vector>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("image msaa load regression"); }
static std::uint32_t Word0(std::uint32_t dmask, std::uint32_t dimension) {
    return (0x3cu << 26u) | (dmask << 8u) | (dimension << 3u) | 1u;
}
static RdnaInstruction Decode(std::uint32_t word0, std::uint32_t word1) {
    const std::array<std::uint32_t, 2> code{word0, word1};
    return DecodeRdnaMimg(0u, code, 0u);
}
static bool Rejects(std::uint32_t word0, std::uint32_t word1) {
    try {
        (void)Decode(word0, word1);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}
static void Check(std::uint32_t dimension, bool a16, std::uint32_t components, std::uint32_t fragmentDword, std::uint32_t fragmentShift) {
    const RdnaInstruction instruction = Decode(Word0(0x4u, dimension), (a16 ? 0x40000000u : 0u) | (8u << 8u));
    Require(instruction.op == RdnaOpcode::ImageMsaaLoad);
    Require(instruction.family == RdnaInstructionFamily::MIMG);
    Require(IsImageOpcode(instruction.op));
    Require(instruction.imageOpcodeId == 0x80u);
    Require(instruction.imageAddressComponents == components);
    Require(instruction.dataComponents == 4u);
    Require(instruction.dataDwordCount == 4u);
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);
    std::vector<IrValue*> reads;
    for (auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::ImageRead) {
            reads.push_back(value);
        }
    }
    Require(reads.size() == 4u);
    for (std::uint32_t fragment = 0u; fragment < 4u; ++fragment) {
        IrValue* address = reads[fragment]->Argument(1)->Resolve();
        Require(address->Opcode() == IrOpcode::MakeImageAddress);
        IrValue* word = address->Argument(fragmentDword)->Resolve();
        if (fragment == 0u) {
            Require(word->Opcode() != IrOpcode::IAdd32);
            continue;
        }
        Require(word->Opcode() == IrOpcode::IAdd32);
        IrValue* offset = word->Argument(1)->Resolve();
        Require(offset->HasImmediate() && offset->ImmediateU32() == fragment << fragmentShift);
        const auto& memory = program.Resources().memoryInfo[reads[fragment]->Flags<MemoryFlags>().index];
        Require(memory.dmask == 0x4u && memory.dataDwords == 1u);
    }
}
int main() {
    Check(6u, false, 3u, 2u, 0u);
    Check(7u, false, 4u, 3u, 0u);
    Check(6u, true, 3u, 1u, 0u);
    Check(7u, true, 4u, 1u, 16u);
    Require(Rejects(Word0(0x1u, 1u), 0u));
    Require(Rejects(Word0(0x3u, 6u), 0u));
    Require(Rejects(Word0(0x1u, 6u), 0x80000000u));
    Require(Rejects(Word0(0x1u, 6u) | 0x00010000u, 0u));
}
