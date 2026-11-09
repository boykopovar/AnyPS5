#ifndef CORE_LIBS_PRX_LIBSCEAGC_COMMAND_INCLUDE_DRAW_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_COMMAND_INCLUDE_DRAW_HPP

#include "prx/libSceAgc/Command/include/Packet.hpp"

namespace Agc::Command {

std::uint32_t DrawInitiator(std::uint64_t modifier, bool indexed, const char* function);
std::uint64_t DrawPatchOffsets(std::uint64_t modifier, const char* function);
std::uint32_t DrawIndexLocation(std::uint64_t modifier);

}

#endif
