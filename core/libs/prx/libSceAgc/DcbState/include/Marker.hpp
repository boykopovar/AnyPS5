#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBSTATE_INCLUDE_MARKER_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBSTATE_INCLUDE_MARKER_HPP

#include <cstdint>
#include "SceTypes.hpp"

namespace Agc::Marker {

std::uint32_t* Push(CommandBuffer* buf, const char* str, const char* function);
std::uint32_t* Pop(CommandBuffer* buf, const char* function);

}

#endif
