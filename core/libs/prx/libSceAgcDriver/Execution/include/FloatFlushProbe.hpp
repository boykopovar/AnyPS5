#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FLOATFLUSHPROBE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FLOATFLUSHPROBE_HPP

#include "Recompiler.hpp"

#include <cstdint>

namespace AgcDriver {

class VulkanDevice;

struct FloatFlushProbeResult {
    std::uint32_t mismatches = 0;
    std::uint32_t compared = 0;
};

FloatFlushProbeResult ProbeFloat32FlushToZero(VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target);

}

#endif
