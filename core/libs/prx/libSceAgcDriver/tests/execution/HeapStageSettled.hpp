#pragma once

#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapStages.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"

#include <cstdint>
#include <span>

inline AgcDriver::DriverDetail::HeapStageMaterialized MaterializeHeapStageSettled(const ShaderRecompiler::PreparedShaderInvocation& invocation, std::span<const std::uint32_t> userData,
                                                                                  ShaderRecompiler::MemoryRegionView memory, std::uint64_t shaderBase) {
    for (;;) {
        try {
            return AgcDriver::DriverDetail::MaterializeHeapStage(invocation, userData, memory, shaderBase);
        } catch (const AgcDriver::DriverDetail::DrawCompilePending&) {
            AgcDriver::DriverDetail::AwaitLibraryPrebuilds();
        }
    }
}
