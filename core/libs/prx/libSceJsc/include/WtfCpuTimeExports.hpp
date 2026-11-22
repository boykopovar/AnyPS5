#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

namespace Jsc::WtfAbi {

struct CPUTime {
    double cpuTime;
    double userTime;
    double systemTime;
};

struct OptionalCPUTime {
    bool hasValue;
    CPUTime value;
};

static_assert(sizeof(OptionalCPUTime) == 32);

} // namespace Jsc::WtfAbi

extern "C" {

Jsc::WtfAbi::OptionalCPUTime APS5_VABI _ZN3WTF7CPUTime3getEv();

}
