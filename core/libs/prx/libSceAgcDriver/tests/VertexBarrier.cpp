#include "Recompiler.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>

namespace {

constexpr std::array<std::uint32_t, 4> BarrierExport{0xbf8a0000u, 0xf80008cfu, 0u, 0xbf810000u};
constexpr std::array<std::uint32_t, 6> BarrierLdsExport{0xd8340000u, 0u, 0xbf8a0000u, 0xf80008cfu, 0u, 0xbf810000u};
constexpr std::array<std::uint32_t, 6> LeadingBarrierLds{0xbf8a0000u, 0xd8340000u, 0u, 0xf80008cfu, 0u, 0xbf810000u};
constexpr std::array<std::uint32_t, 7> ScalarBarrierLds{0xbe8003c1u, 0xbf8a0000u, 0xd8340000u, 0u, 0xf80008cfu, 0u, 0xbf810000u};

constexpr std::array<std::uint32_t, 1> VertexCapabilities{spv::CapabilityClipDistance};

ShaderRecompiler::RecompileRequest vertexRequest(std::span<const std::uint32_t> code) {
    using namespace ShaderRecompiler;
    RecompileRequest request{};
    request.shader = {ShaderStage::Vertex, 0x10000u, code, 0, {}};
    request.context.waveSize = 32;
    request.context.vertex = ShaderVertexStageInfo{};
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 32;
    request.target.supportedCapabilities = VertexCapabilities;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    return request;
}

bool hasControlBarrier(const ShaderRecompiler::RecompileResult& result) {
    const auto& words = result.spirv.Words();
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpControlBarrier) return true;
    }
    return false;
}

void verifyBarrierWithoutMemory() {
    const auto result = ShaderRecompiler::Recompile(vertexRequest(BarrierExport));
    if (hasControlBarrier(result)) throw std::runtime_error("a vertex program's s_barrier became a workgroup OpControlBarrier, which the Vertex execution model does not allow");
}

void verifyBarrierBeforeLds() {
    for (const auto code : {std::span<const std::uint32_t>(LeadingBarrierLds), std::span<const std::uint32_t>(ScalarBarrierLds)}) {
        const auto result = ShaderRecompiler::Recompile(vertexRequest(code));
        if (hasControlBarrier(result)) throw std::runtime_error("an s_barrier before every memory access of a vertex program became a workgroup OpControlBarrier");
    }
}

void verifyBarrierWithLds() {
    try {
        (void)ShaderRecompiler::Recompile(vertexRequest(BarrierLdsExport));
    } catch (const std::exception& error) {
        if (std::string(error.what()).find("s_barrier in a vertex program that accesses LDS or GDS or writes memory is not implemented") != std::string::npos) return;
        throw;
    }
    throw std::runtime_error("a vertex program with s_barrier and an LDS write compiled");
}

} // namespace

int main() {
    try {
        verifyBarrierWithoutMemory();
        verifyBarrierBeforeLds();
        verifyBarrierWithLds();
        std::cout << "vertex barrier tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        const std::string message(error.what());
        std::cerr << message.substr(0, message.find("RecompileRequest:")) << '\n';
        return 1;
    }
}
