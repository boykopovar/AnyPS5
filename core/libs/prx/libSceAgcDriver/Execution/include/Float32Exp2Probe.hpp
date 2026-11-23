#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FLOAT32EXP2PROBE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FLOAT32EXP2PROBE_HPP

#include "Recompiler.hpp"
#include <cstdint>

namespace AgcDriver {

class VulkanDevice;

struct Float32Exp2ProbeResult {
    std::uint32_t mismatches = 0;
    std::uint32_t compared = 0;
};

Float32Exp2ProbeResult ProbeFloat32HostExp2(VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target, bool host);

}

#endif
