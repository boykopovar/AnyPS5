#include "DrawStages.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "WgslBackend/WgslTarget.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

namespace WasmGpu {
namespace {

using ShaderRecompiler::ProgramRole;
using ShaderRecompiler::ShaderStage;

void Require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error("AGC driver: " + reason);
}

std::uint32_t ReadOptional(const AgcDriver::Registers& registers, std::uint32_t offset) {
    const auto found = registers.find(offset);
    return found == registers.end() ? 0u : found->second;
}

std::uint32_t ReadRequired(const AgcDriver::Registers& registers, std::uint32_t offset) {
    const auto found = registers.find(offset);
    Require(found != registers.end(), "required shader register has not been written");
    return found->second;
}

std::uint64_t ProgramAddress(const AgcDriver::QueueState& queue, std::uint32_t base) {
    const auto high = ReadOptional(queue.shader, base + 1);
    Require((high & ~0xffu) == 0, "reserved graphics program address bits are set");
    return (static_cast<std::uint64_t>(ReadOptional(queue.shader, base)) << 8u) | (static_cast<std::uint64_t>(high) << 40u);
}

StageProgram Prepare(const AgcDriver::QueueState& queue, ProgramRole role, std::uint64_t address, std::uint8_t type, ShaderStage stage, std::uint32_t rsrc2, std::uint32_t userDataBase) {
    auto shader = FindShader(address, {type});
    Require((address - shader->codeAddress) % sizeof(std::uint32_t) == 0, "graphics entry point is not dword aligned");
    const auto resources = ReadOptional(queue.shader, rsrc2);
    const auto userCount = ((resources >> 1u) & 0x1fu) | (((resources >> 27u) & 1u) << 5u);
    Require(userCount <= 32, "graphics user SGPR count exceeds the register bank");
    const auto codeOffset = static_cast<std::size_t>((address - shader->codeAddress) / sizeof(std::uint32_t));
    StageProgram program{role, stage, std::move(shader), codeOffset, userDataBase, stage == ShaderStage::Fragment ? 0u : 8u, {}, std::nullopt, {}};
    for (std::uint32_t index = 0; index < userCount; ++index) program.userData.push_back(ReadOptional(queue.shader, userDataBase + index));
    return program;
}

std::uint32_t PositionExportLayout(std::span<const std::uint32_t> code, std::uint32_t paClVsOutCntl) {
    const auto bits = paClVsOutCntl & AgcDriver::Graphics::PositionExportLayoutBits;
    if (bits == 0) return 0;
    const auto decoded = ShaderRecompiler::RdnaInstructionDecoder{}.Decode(code);
    const bool used = std::any_of(decoded.instructions.begin(), decoded.instructions.end(), [](const auto& instruction) {
        return instruction.family == ShaderRecompiler::RdnaInstructionFamily::EXP && instruction.exportTarget >= 0x0du && instruction.exportTarget <= 0x0fu && instruction.exportEnableMask != 0;
    });
    return used ? bits : 0;
}

}  // namespace

ShaderRecompiler::ShaderBinary StageProgram::Binary() const {
    return {stage, shader->codeAddress + codeOffset * sizeof(std::uint32_t), std::span(shader->code).subspan(codeOffset), shader->headerAddress, shader->header};
}

DrawStages DecodeDraw(std::span<const std::uint32_t> packet, const AgcDriver::QueueState& queue) {
    namespace Graphics = AgcDriver::Graphics;
    DrawStages result{Graphics::DecodeState(queue, false)};
    result.draw = AgcDriver::Pm4::ResolveDraw(packet, queue);
    Require(!result.draw.indirect, "indirect draws are not implemented in WebAssembly");
    const auto& state = result.state;
    Require(state.stages.path == Graphics::ShaderPath::Vertex, "only the vertex shader path is implemented in WebAssembly");
    auto& vertex = result.programs.emplace_back(Prepare(queue, ProgramRole::Main, ProgramAddress(queue, 0xc8), 2, ShaderStage::Vertex, 0x8b, 0x8c));
    vertex.vertex = Graphics::DecodeVertexStageInfo(vertex.shader->header, vertex.shader->headerAddress, vertex.userData, &vertex.reads, false, 8u - vertex.firstUserSgpr);
    vertex.vertex->paClVsOutCntl = PositionExportLayout(std::span(vertex.shader->code).subspan(vertex.codeOffset), ReadRequired(queue.context, 0x207)) | state.layerExports;
    const bool pixelSkipped = Graphics::PixelProgramSkipped(queue);
    const auto pixelAddress = ProgramAddress(queue, 0x008);
    if (!pixelSkipped && pixelAddress != 0) result.programs.push_back(Prepare(queue, ProgramRole::Fragment, pixelAddress, 1, ShaderStage::Fragment, 0x00b, 0x00c));
    auto& pixel = result.pixel;
    pixel = Graphics::DecodePixelStageInfo(queue.context, Graphics::ExportMappings(state), pixelSkipped);
    pixel.targetExportPacking = Graphics::ExportPackings(state);
    pixel.dualSourceBlend = state.dualSourceBlend;
    pixel.alphaToCoverage = state.alphaToCoverage;
    if (pixel.ancillary) pixel.rasterPrimitive = Graphics::RasterPrimitiveOf(state);
    if (pixel.ancillary) pixel.layerExported = state.layerExports != 0;
    pixel.ditheredAlphaToCoverage = state.ditheredAlphaToCoverage;
    return result;
}

std::unique_ptr<DrawRequests> BuildDrawRequests(DrawStages stages) {
    auto result = std::make_unique<DrawRequests>();
    result->stages = std::move(stages);
    const auto& draw = result->stages;
    std::vector<ShaderRecompiler::MemoryRegion> initial;
    for (const auto& program : draw.programs) {
        initial.push_back({program.shader->codeAddress, std::as_bytes(std::span(program.shader->code))});
        initial.push_back({program.shader->headerAddress, program.shader->header});
        result->linked.push_back({program.role, program.Binary(), program.userDataBase, program.firstUserSgpr, program.userData});
    }
    result->shaderMemory = std::make_unique<AgcDriver::ShaderMemory>(initial);
    result->memory.resize(draw.programs.size());
    result->requests.reserve(draw.programs.size());
    for (std::size_t index = 0; index < draw.programs.size(); ++index) {
        const auto& program = draw.programs[index];
        const bool fragment = program.stage == ShaderStage::Fragment;
        auto& memory = result->memory[index];
        memory = result->shaderMemory->Regions();
        auto& request = result->requests.emplace_back(ShaderRecompiler::RecompileRequest{
                program.Binary(),
                {fragment ? draw.state.stages.fragmentWaveSize : draw.state.stages.vertexWaveSize, program.firstUserSgpr, program.userData, std::nullopt, fragment ? std::optional(draw.pixel) : std::nullopt, program.vertex, memory, program.shader->floatMode},
                ShaderRecompiler::WebGpuTarget(),
                {0, 0, 0, 0},
                ShaderRecompiler::GraphicsCompileContext{program.firstUserSgpr, result->linked, std::nullopt, std::nullopt, {draw.draw.indexAddress, draw.draw.indexCount, draw.draw.indexSize, draw.draw.instanceCount}}});
        result->captures.push_back(result->shaderMemory->Capture(request));
        memory = result->shaderMemory->Regions();
        for (const auto& read : program.reads) memory.push_back({read.address, std::as_bytes(std::span(read.bytes))});
        request.context.memory = memory;
    }
    return result;
}

}  // namespace WasmGpu
