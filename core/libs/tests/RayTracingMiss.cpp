#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include "Recompiler.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("ray tracing miss regression"); }
struct Form {
    std::uint32_t word0;
    std::uint32_t word1;
    RdnaOpcode opcode;
    std::uint32_t addressDwords;
};
static void Check(const Form& form) {
    const std::array<std::uint32_t, 2> code{form.word0, form.word1};
    const RdnaInstruction instruction = DecodeRdnaMimg(0u, code, 0u);
    Require(instruction.op == form.opcode);
    Require(instruction.family == RdnaInstructionFamily::MIMG);
    Require(IsImageOpcode(instruction.op));
    Require(instruction.imageAddressComponents == form.addressDwords);
    Require(GetRdnaImageAddressDwordCount(instruction.imageSampleFlags, instruction.imageAddressComponents) == form.addressDwords);
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    if (RayTracingStrict()) {
        bool thrown = false;
        try {
            context.TranslateInstruction(instruction);
        } catch (const std::runtime_error&) {
            thrown = true;
        }
        Require(thrown);
    } else {
        context.TranslateInstruction(instruction);
        bool emitted = false;
        for (auto* value : block.Instructions()) emitted = emitted || value->Opcode() == IrOpcode::ImageBvhIntersectRay;
        Require(emitted != RayTracingMiss());
    }
}
int main() {
    Check({0xF1981F01u, 0x00000000u, RdnaOpcode::ImageBvhIntersectRay, 11u});
    Check({0xF1981F01u, 0x40000000u, RdnaOpcode::ImageBvhIntersectRay, 8u});
    Check({0xF19C1F01u, 0x00000000u, RdnaOpcode::ImageBvh64IntersectRay, 12u});
    Check({0xF19C1F01u, 0x40000000u, RdnaOpcode::ImageBvh64IntersectRay, 9u});
}
