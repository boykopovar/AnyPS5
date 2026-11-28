#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FRAGMENTBARYCENTRICPROBE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FRAGMENTBARYCENTRICPROBE_HPP

#include "prx/libSceAgcDriver/Graphics/include/State.hpp"

#include <cstdint>

namespace AgcDriver {

struct FragmentBarycentricProbeMode {
    std::uint32_t mismatches = 0;
    std::uint32_t compared = 0;
};

struct FragmentBarycentricProbeResult {
    FragmentBarycentricProbeMode perspective;
    FragmentBarycentricProbeMode linear;
    [[nodiscard]] bool Correct() const {
        return perspective.compared != 0u && linear.compared != 0u && perspective.mismatches == 0u && linear.mismatches == 0u;
    }
};

FragmentBarycentricProbeResult ProbeFragmentBarycentricBuiltins(const Graphics::Context& context, bool readFragCoord);

}

#endif
