#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FLOAT32NANPROBE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FLOAT32NANPROBE_HPP

#include "Recompiler.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace AgcDriver {

class VulkanDevice;

struct Float32NanMismatch {
    std::uint32_t result = 0;
    std::uint32_t emulated = 0;
    std::uint32_t guarded = 0;
};

struct Float32NanProbeResult {
    std::uint32_t mismatches = 0;
    std::uint32_t compared = 0;
    std::optional<Float32NanMismatch> first;
};

std::span<const std::uint32_t> Float32NanProbeCode();

Float32NanProbeResult ProbeFloat32SingleNans(VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target);

}

#endif
