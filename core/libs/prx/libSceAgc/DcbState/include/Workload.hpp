#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBSTATE_INCLUDE_WORKLOAD_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBSTATE_INCLUDE_WORKLOAD_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbSetWorkloadComplete(CommandBuffer* buf, uint32_t streamId, uint32_t workloadId);
uint32_t* APS5_VABI sceAgcDcbSetWorkloadsActive(CommandBuffer* buf, uint32_t streamId, const uint32_t* workloadIds, uint32_t workloadCount);
uint32_t* APS5_VABI sceAgcDcbSetWorkloadStreamInactive(CommandBuffer* buf, uint32_t streamId);

}

#endif
