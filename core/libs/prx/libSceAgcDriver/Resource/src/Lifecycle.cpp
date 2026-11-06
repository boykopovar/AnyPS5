#include "prx/libSceAgcDriver/Resource/include/Lifecycle.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr uint32_t SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE = 0x8A6C9018;

extern "C" {

uint32_t APS5_VABI sceAgcDriverInitResourceRegistration(void) {
 return 0;
}

uint32_t APS5_VABI sceAgcDriverQueryResourceRegistrationUserMemoryRequirements(uint64_t* size_in_bytes, uint32_t count_a, uint32_t count_b) {
    // Registration is unavailable, as in the neighboring registration APIs.
    // The observed caller handles this status before reading size_in_bytes.
    (void)size_in_bytes;
    (void)count_a;
    (void)count_b;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

}
