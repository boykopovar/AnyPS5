#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/include/PlacementNewExports.hpp"

#include <cstddef>

extern "C" {

void* APS5_VABI _ZN7bmalloc15IsoHeapImplBasenwEmPv(std::size_t, void* location) {
    return location;
}

void* APS5_VABI _ZN9Inspector14ConsoleMessagenwEm10NotNullTagPv(std::size_t, unsigned int, void* location) {
    return location;
}

void* APS5_VABI _ZN9Inspector22InspectorDebuggerAgentnwEmPv(std::size_t, void* location) {
    return location;
}

}
