#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_MEMORY_DWORDPATTERNFILL_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_MEMORY_DWORDPATTERNFILL_HPP

#include "Recompiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace AgcDriver::DriverDetail {

struct DwordPatternFill {
    std::uint64_t base = 0;
    std::size_t bytes = 0;
    std::array<std::uint32_t, 4> pattern{};
};

std::optional<DwordPatternFill> MatchDwordPatternFill(std::span<const std::uint32_t> packet, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute);
std::optional<DwordPatternFill> MatchConstantDwordFill(std::span<const std::uint32_t> packet, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute);
std::optional<DwordPatternFill> MatchClampedGlobalFill(std::span<const std::uint32_t> packet, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute);
std::optional<Graphics::MemoryValueFill> MatchMemoryValueBufferFill(std::span<const std::uint32_t> packet, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute);
std::optional<DwordPatternFill> MatchSgprPatternFill(std::span<const std::uint32_t> packet, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute);
bool IsByteFill(const std::array<std::uint32_t, 4>& pattern);
std::optional<Graphics::DccKeys> KeysFillMemo(const std::array<std::uint32_t, 4>& pattern, bool coversStorageKeys, bool memoryValueFill);

}

#endif
