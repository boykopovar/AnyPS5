#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FLOAT16CONVERSIONPROBE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FLOAT16CONVERSIONPROBE_HPP

#include "Recompiler.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace AgcDriver {

class VulkanDevice;

struct Float16ConversionMismatch {
    std::uint32_t input = 0;
    std::uint32_t emulated = 0;
    std::uint32_t native = 0;
};

struct Float16ConversionProbeDirection {
    bool probed = false;
    std::uint32_t mismatches = 0;
    std::uint32_t compared = 0;
    std::optional<Float16ConversionMismatch> first;
};

struct Float16ConversionProbeResult {
    Float16ConversionProbeDirection widen;
    Float16ConversionProbeDirection narrow;
};

std::vector<std::uint32_t> Float16NarrowProbeInputs();

Float16ConversionProbeResult ProbeFloat16Conversions(VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target);

}

#endif
