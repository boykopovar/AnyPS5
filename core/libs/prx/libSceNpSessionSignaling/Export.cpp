#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceNpSessionSignalingInitialize(void* param) {
 (void)param;
 return 0;
}

}
