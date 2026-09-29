#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceSharePlayInitialize(void* heap, size_t heap_size) {
    // No Share Play session can ever start; the library only has to accept its lifecycle.
    if (heap == nullptr || heap_size == 0) APS5_INVALID_ARG_EX;
    return 0;
}

int APS5_VABI sceSharePlayTerminate(void) {
    return 0;
}

}
