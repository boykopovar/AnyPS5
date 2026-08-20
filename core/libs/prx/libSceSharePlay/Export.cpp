#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceSharePlayInitialize(void* heap, size_t heap_size) {
 (void)heap;
 (void)heap_size;
 return 0;
}

int sceSharePlayTerminate(void) {
 return 0;
}

}
