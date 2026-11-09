#include "prx/libSceAgcDriver/Resource/include/Lifecycle.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr uint32_t AGC_EINVAL = 0x80020065u;

extern "C" {

uint32_t APS5_VABI sceAgcDriverInitResourceRegistration(void) {
 return 0;
}

uint32_t APS5_VABI sceAgcDriverQueryResourceRegistrationUserMemoryRequirements(uint64_t* size_in_bytes) {
 if (!size_in_bytes) return AGC_EINVAL;
 *size_in_bytes = 0x4000ULL;
 return 0;
}

}
