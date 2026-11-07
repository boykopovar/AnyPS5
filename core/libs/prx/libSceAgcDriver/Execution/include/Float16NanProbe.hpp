#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FLOAT16NANPROBE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FLOAT16NANPROBE_HPP

#include "Recompiler.hpp"

#include <cstdint>

namespace AgcDriver {

class VulkanDevice;

struct Float16NanProbeResult {
    std::uint32_t mismatches = 0;
    std::uint32_t compared = 0;
};

Float16NanProbeResult ProbeFloat16HostNans(VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target);

}

#endif
