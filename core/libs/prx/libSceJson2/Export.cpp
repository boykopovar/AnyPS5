#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"


extern "C" {

// Initialization succeeds as on an offline console: the title tears its whole web API layer down on
// an init failure and later dereferences it anyway. Requests made through it fail as offline.
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void* self, const void* parameter) {
    (void)self;
    (void)parameter;
    return 0;
}

int APS5_VABI _ZN3sce4Json11InitializerC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce4Json11InitializerD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce4Json12MemAllocatorC2Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce4Json12MemAllocatorD2Ev(void* self) {
    (void)self;
    return 0;
}

}
