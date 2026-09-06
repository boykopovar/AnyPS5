#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceSharePlayInitialize(void* heap, size_t heap_size) {
 (void)heap;
 (void)heap_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSharePlayTerminate(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
