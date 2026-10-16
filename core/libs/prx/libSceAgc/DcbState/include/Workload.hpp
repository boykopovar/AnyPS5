#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBSTATE_INCLUDE_WORKLOAD_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBSTATE_INCLUDE_WORKLOAD_HPP

#include <cstdint>
#include "SceTypes.hpp"

namespace Agc::Workload {

std::uint32_t* WriteActive(CommandBuffer* buf, std::uint32_t streamId, const std::uint32_t* workloadIds, std::uint32_t workloadCount, const char* function);
std::uint32_t* WriteComplete(CommandBuffer* buf, std::uint32_t streamId, std::uint32_t workloadId, const char* function);
std::uint32_t* WriteStreamInactive(CommandBuffer* buf, std::uint32_t streamId, const char* function);

}

#endif
