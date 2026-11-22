#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>

extern "C" {

void* APS5_VABI _ZN7bmalloc15IsoHeapImplBasenwEmPv(std::size_t size, void* location);
void* APS5_VABI _ZN9Inspector14ConsoleMessagenwEm10NotNullTagPv(std::size_t size, unsigned int tag, void* location);
void* APS5_VABI _ZN9Inspector22InspectorDebuggerAgentnwEmPv(std::size_t size, void* location);

}
