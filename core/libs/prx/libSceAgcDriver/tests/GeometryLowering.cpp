#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/GeometryLowering.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

constexpr std::uint32_t RingMemory = 0u;
constexpr std::uint32_t OutputMemory = 1u;
constexpr std::uint32_t SharedSubtractions = 31u;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Geometry {
    IrProgram program;
    IrBuilder builder{program};

    Geometry() {
        program.Resources().stage = IrShaderStage::Geometry;
        program.Resources().memoryInfo.push_back(MemoryInfo{.kind = ResourceKind::Buffer, .offen = true});
        program.Resources().memoryInfo.push_back(MemoryInfo{.kind = ResourceKind::Buffer});
        auto& block = program.CreateBlock();
        program.BlockOrder().push_back(&block);
        BlockInfo info;
        info.id = block.Id();
        program.Metadata().blockInfo.push_back(info);
        program.SetEntryBlock(block);
        builder.SetInsertionPoint(block);
    }

    IrValue& resource() {
        return builder.Emit(IrOpcode::GetBufferResource, IrType::BufferResource, {&builder.Constant(0u), &builder.Constant(0u), &builder.Constant(0u), &builder.Constant(0u)});
    }
};

ShaderGeometryInputInfo Points() {
    ShaderGeometryInputInfo geometry;
    geometry.active = true;
    geometry.inputVertices = 1u;
    geometry.outputTopology = 2u;
    geometry.maxVertices = 4u;
    geometry.esgsItemDwords = 2u;
    geometry.vertexDwords = 5u;
    return geometry;
}

void RequireSharedAddressTermsEvaluatedOnce() {
    Geometry shader;
    auto& builder = shader.builder;
    auto& vertex = builder.Emit(IrOpcode::TessellationBase, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(TessellationBaseKind::GeometryVertex))});
    IrValue* zero = &builder.Constant(8u);
    for (std::uint32_t i = 0; i < SharedSubtractions; ++i) zero = &builder.Emit(IrOpcode::ISub32, IrType::U32, {zero, zero});
    auto& scaled = builder.Emit(IrOpcode::IMul32, IrType::U32, {&vertex, &builder.Constant(4u)});
    auto& offset = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&scaled, zero});
    auto& loaded = builder.Emit(IrOpcode::LoadBufferU32, IrType::U32, {&shader.resource(), &builder.Constant(0u), &offset, &builder.Constant(256u), &builder.ConstantBool(true)}, MemoryFlags{RingMemory, 0x40u});
    static_cast<void>(builder.Emit(IrOpcode::StoreBufferU32, IrType::Void, {&shader.resource(), &builder.Constant(0u), &builder.Constant(0u), &builder.Constant(0u), &loaded, &builder.ConstantBool(true)}, MemoryFlags{OutputMemory, 0x48u}));
    GeometryLowering{}.Lower(shader.program, Points());
    const IrValue* read = nullptr;
    for (IrBlock* block : shader.program.BlockOrder()) {
        for (IrValue* inst : block->Instructions()) {
            if (inst->Opcode() == IrOpcode::GetTessellationAttribute) read = inst;
            Require(inst->Opcode() != IrOpcode::LoadBufferU32, "the ESGS ring read was left as a buffer load");
        }
    }
    Require(read != nullptr && read->Argument(0)->Resolve()->ImmediateU32() == static_cast<std::uint32_t>(TessellationAttribute::GeometryInput) && read->Argument(1)->Resolve()->ImmediateU32() == 4u, "an ESGS ring read at the ES vertex offset times 4 plus 31 shared subtractions of 8 from itself, scalar offset 256, was not lowered to dword 1 of input vertex 0");
}

}

int main() {
    try {
        RequireSharedAddressTermsEvaluatedOnce();
        std::puts("geometry lowering tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
