#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/GeometryLowering.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "Recompiler.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ShaderRecompiler;

namespace {

constexpr std::uint32_t RingMemory = 0u;
constexpr std::uint32_t OutputMemory = 1u;
constexpr std::uint32_t SharedSubtractions = 31u;
constexpr std::uint32_t MaxGeometryVertices = 1024u;
constexpr std::uint32_t MaxVertexDwords = 0x7fffu;
constexpr std::uint32_t WrappingGeometryVertices = 0x4000001u;
constexpr std::array<std::uint32_t, 7> CopySlotOne{0x34020082u, 0xe0305040u, 0x80400201u, 0xbf8c0000u, 0xf80008cfu, 0x02020202u, 0xbf810000u};
constexpr std::array<std::uint32_t, 7> CopySlotZero{0x34020082u, 0xe0305000u, 0x80400201u, 0xbf8c0000u, 0xf80008cfu, 0x02020202u, 0xbf810000u};

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

template <typename Function>
bool RefusedWith(Function&& function, const std::string& reason) {
    try {
        function();
    } catch (const std::exception& error) {
        return std::string(error.what()).find(reason) != std::string::npos;
    }
    return false;
}

ShaderGeometryInputInfo LargestRing() {
    auto geometry = Points();
    geometry.maxVertices = MaxGeometryVertices;
    geometry.vertexDwords = MaxVertexDwords;
    return geometry;
}

std::vector<std::uint32_t> LowerGsVsStore(const ShaderGeometryInputInfo& geometry, std::uint32_t bytes) {
    Geometry shader;
    auto& builder = shader.builder;
    auto& base = builder.Emit(IrOpcode::TessellationBase, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(TessellationBaseKind::GsVsOffset))});
    static_cast<void>(builder.Emit(IrOpcode::StoreBufferU32, IrType::Void, {&shader.resource(), &builder.Constant(0u), &builder.Constant(bytes), &base, &builder.Constant(7u), &builder.ConstantBool(true)}, MemoryFlags{RingMemory, 0x50u}));
    GeometryLowering{}.Lower(shader.program, geometry);
    std::vector<std::uint32_t> addresses;
    for (IrBlock* block : shader.program.BlockOrder()) {
        for (IrValue* inst : block->Instructions()) {
            if (inst->Opcode() == IrOpcode::SetTessellationAttribute && inst->Argument(0)->Resolve()->ImmediateU32() == static_cast<std::uint32_t>(TessellationAttribute::GeometryRing)) addresses.push_back(inst->Argument(1)->Resolve()->ImmediateU32());
        }
    }
    return addresses;
}

void RequireLargestRingAddressed() {
    const auto lastByte = (MaxGeometryVertices * MaxVertexDwords - 1u) * 4u;
    Require(LowerGsVsStore(LargestRing(), lastByte) == std::vector<std::uint32_t>{lastByte}, "a GSVS ring store at the last dword of a 1024-vertex, 32767-dword ring was not lowered to that dword");
    Require(RefusedWith([&] { static_cast<void>(LowerGsVsStore(LargestRing(), lastByte + 4u)); }, "a GSVS ring store outside VGT_GS_VERT_ITEMSIZE times VGT_GS_MAX_VERT_OUT dwords"), "a GSVS ring store one dword past a 1024-vertex, 32767-dword ring was accepted");
    const auto exports = ParseGeometryCopyProgram(CopySlotZero, MaxGeometryVertices, MaxVertexDwords, std::nullopt);
    Require(exports.size() == 1u && exports[0].values == std::array<std::uint32_t, 4>{0u, 0u, 0u, 0u}, "a copy program read of slot 0 of a 1024-vertex, 32767-dword ring was not parsed");
    Require(RefusedWith([&] { static_cast<void>(ParseGeometryCopyProgram(CopySlotOne, MaxGeometryVertices, MaxVertexDwords, std::nullopt)); }, "which is not one of the 32767 vertex dwords of 65536 bytes each"), "a copy program read at 64 bytes was accepted for a ring whose slots are 65536 bytes");
}

void RequireWrappingRingRefused() {
    const std::string reason = "do not fit in 32-bit ring addresses";
    Require(RefusedWith([] { static_cast<void>(ParseGeometryCopyProgram(CopySlotOne, WrappingGeometryVertices, 5u, std::nullopt)); }, reason), "a copy program was parsed for a VGT_GS_MAX_VERT_OUT whose 64-byte slot stride wraps 32 bits");
    auto geometry = Points();
    geometry.maxVertices = WrappingGeometryVertices;
    Require(RefusedWith([&] { static_cast<void>(LowerGsVsStore(geometry, 0u)); }, reason), "a geometry program was lowered for a VGT_GS_MAX_VERT_OUT whose ring size wraps 32 bits");
    Require(RefusedWith([] { static_cast<void>(GeometryRingDwords(0xffffffffu, 0xffffffffu)); }, reason), "a geometry ring of 2^32-1 vertices of 2^32-1 dwords was accepted");
}

}

int main() {
    try {
        RequireSharedAddressTermsEvaluatedOnce();
        RequireLargestRingAddressed();
        RequireWrappingRingRefused();
        std::puts("geometry lowering tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
