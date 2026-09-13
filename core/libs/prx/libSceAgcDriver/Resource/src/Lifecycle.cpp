#include "prx/libSceAgcDriver/Resource/include/Lifecycle.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t APS5_VABI sceAgcDriverInitResourceRegistration(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t APS5_VABI sceAgcDriverQueryResourceRegistrationUserMemoryRequirements(uint64_t* size_in_bytes) {
 (void)size_in_bytes;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
