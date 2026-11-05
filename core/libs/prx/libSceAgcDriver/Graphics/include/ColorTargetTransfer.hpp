#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COLORTARGETTRANSFER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COLORTARGETTRANSFER_HPP

#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <span>

namespace AgcDriver::Graphics {

void ReadColorTarget(const ColorTarget& target, std::span<std::byte> destination);
void WriteColorTarget(const ColorTarget& target, std::span<const std::byte> source);

}

#endif
