#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceGameLiveStreamingInitialize(size_t heap_size) {
 (void)heap_size;
 return 0;
}

int sceGameLiveStreamingTerminate(void) {
 return 0;
}

}
