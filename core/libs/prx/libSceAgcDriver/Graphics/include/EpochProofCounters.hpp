#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_EPOCHPROOFCOUNTERS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_EPOCHPROOFCOUNTERS_HPP

#include "prx/libSceAgcDriver/Execution/include/ProfileCounter.hpp"

namespace AgcDriver::Graphics {

struct EpochProofCounters {
    ProfileCounter resourceReuses;
    ProfileCounter resourceProofs;
    ProfileCounter storageReuses;
    ProfileCounter mirrorReuses;
    ProfileCounter mirrorRefreshes;
};

EpochProofCounters& EpochProofCounts();
bool EpochProofCountsProfiled();
void ReportEpochProofCounts();

}

#endif
