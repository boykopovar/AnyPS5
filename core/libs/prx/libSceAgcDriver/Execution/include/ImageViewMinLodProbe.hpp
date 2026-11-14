#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_IMAGEVIEWMINLODPROBE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_IMAGEVIEWMINLODPROBE_HPP

#include "Recompiler.hpp"

#include <cstdint>

namespace AgcDriver {

class VulkanDevice;

struct ImageViewMinLodProbeResult {
    std::uint32_t mismatches = 0;
    std::uint32_t compared = 0;
    std::uint32_t controlMismatches = 0;
    std::uint32_t controlCompared = 0;
};

ImageViewMinLodProbeResult ProbeImageViewMinLod(VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target);

}

#endif
