#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SHADERINPUTSTATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SHADERINPUTSTATE_HPP

#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "Recompiler.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace AgcDriver::Graphics {

ShaderRecompiler::ShaderPixelStageInfo DecodePixelStageInfo(const Registers& context, const std::array<std::uint8_t, 8>& exportMappings, bool nullProgram = false);
bool PixelEarlyZ(std::uint32_t shaderControl);
ShaderRecompiler::ShaderPixelStageInfo DefaultUnexportedInputs(ShaderRecompiler::ShaderPixelStageInfo pixel, std::span<const std::uint32_t> parameterExports);
ShaderRecompiler::ShaderComputeStageInfo DecodeComputeStageInfo(const Registers& shader, std::span<const std::byte> header);
// A guest range DecodeVertexStageInfo read (an attribute word, a vertex V#) with the bytes as read:
// the draw cache validates them by value with the stage's capture (design_cpu_final rule RD).
struct DecodeRead {
    std::uint64_t address;
    std::array<std::byte, 16> data{};
    std::uint32_t size = 0;
    std::span<const std::byte> Bytes() const { return {data.data(), size}; }
};
ShaderRecompiler::ShaderVertexStageInfo DecodeVertexStageInfo(std::span<const std::byte> header, std::uint64_t headerAddress, std::span<const std::uint32_t> userData, std::vector<DecodeRead>* reads = nullptr, bool staticAbi = false, std::uint32_t hiddenUserWords = 0);

}

#endif
