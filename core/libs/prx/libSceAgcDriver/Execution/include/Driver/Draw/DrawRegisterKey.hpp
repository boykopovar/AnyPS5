#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_DRIVER_DRAW_DRAWREGISTERKEY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_DRIVER_DRAW_DRAWREGISTERKEY_HPP

#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"

namespace AgcDriver::DriverDetail {

struct DrawRegisterStateKey {
    std::uint64_t exact;
    std::uint64_t shape;
    bool operator==(const DrawRegisterStateKey&) const = default;
};

DrawRegisterStateKey RegisterStateKey(const QueueState& queue, bool allUserWords = false, bool incremental = true);

}

#endif
