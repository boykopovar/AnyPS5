#ifndef CORE_WASM_RUNTIME_GPU_DRAWSTAGES_HPP
#define CORE_WASM_RUNTIME_GPU_DRAWSTAGES_HPP

#include "Shaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "Recompiler.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace WasmGpu {

struct StageProgram {
    ShaderRecompiler::ProgramRole role;
    ShaderRecompiler::ShaderStage stage;
    std::shared_ptr<const ShaderBinary> shader;
    std::size_t codeOffset;
    std::uint32_t userDataBase;
    std::uint32_t firstUserSgpr;
    std::vector<std::uint32_t> userData;
    std::optional<ShaderRecompiler::ShaderVertexStageInfo> vertex;
    std::vector<AgcDriver::Graphics::DecodeRead> reads;

    [[nodiscard]] ShaderRecompiler::ShaderBinary Binary() const;
};

struct DrawStages {
    AgcDriver::Graphics::State state;
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    AgcDriver::Pm4::DrawParameters draw{};
    std::vector<StageProgram> programs;
};

DrawStages DecodeDraw(std::span<const std::uint32_t> packet, const AgcDriver::QueueState& queue);

struct DrawRequests {
    DrawStages stages;
    std::unique_ptr<AgcDriver::ShaderMemory> shaderMemory;
    std::vector<ShaderRecompiler::LinkedProgram> linked;
    std::vector<std::vector<ShaderRecompiler::MemoryRegion>> memory;
    std::vector<ShaderRecompiler::RecompileRequest> requests;
    std::vector<std::shared_ptr<const ShaderRecompiler::ResourceCapture>> captures;
};

std::unique_ptr<DrawRequests> BuildDrawRequests(DrawStages stages);

}  // namespace WasmGpu

#endif
