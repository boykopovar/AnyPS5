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
ShaderRecompiler::ShaderComputeStageInfo DecodeComputeStageInfo(const Registers& shader);
// A guest range DecodeVertexStageInfo read (an attribute word, a vertex V#) with the bytes as read:
// the draw cache validates them by value with the stage's capture (design_cpu_final rule RD).
struct DecodeRead {
    std::uint64_t address;
    std::vector<std::byte> bytes;
};
class VertexStagePlan {
public:
    VertexStagePlan(std::span<const std::byte> header, std::uint64_t headerAddress);
    ShaderRecompiler::ShaderVertexStageInfo Read(std::span<const std::uint32_t> userData, std::vector<DecodeRead>* reads = nullptr) const;

private:
    struct Attribute {
        std::uint8_t semantic;
        std::uint8_t destination;
        std::uint8_t components;
    };
    struct ReadRun {
        std::uint16_t first;
        std::uint16_t count;
    };
    std::int32_t bufferRegister = -1;
    std::int32_t attributeRegister = -1;
    std::vector<Attribute> attributes;
    std::vector<ReadRun> attributeReads;
};
ShaderRecompiler::ShaderVertexStageInfo DecodeVertexStageInfo(std::span<const std::byte> header, std::uint64_t headerAddress, std::span<const std::uint32_t> userData, std::vector<DecodeRead>* reads = nullptr);

}

#endif
