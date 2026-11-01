#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_WORKERSAMPLER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_WORKERSAMPLER_HPP

#include <cstdint>

namespace AgcDriver {

void StartWorkerSampler(std::uint32_t queue = 0);
void StartCommitterSampler();
void StopWorkerSampler();

}

#endif
