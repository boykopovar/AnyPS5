#include "prx/libSceDbgThreadSanitizer/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI __tsan_acquire(void* addr) {
    (void)addr;
}

void APS5_VABI __tsan_release(void* addr) {
    (void)addr;
}

}
