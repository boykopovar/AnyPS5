#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceRemoteplayGetConnectionStatus(int user_id, int* status) {
 (void)user_id;
 (void)status;
 return 0;
}

int sceRemoteplayInitialize(void* heap, size_t heap_size) {
 (void)heap;
 (void)heap_size;
 return 0;
}

int sceRemoteplayTerminate(void) {
 return 0;
}

}
