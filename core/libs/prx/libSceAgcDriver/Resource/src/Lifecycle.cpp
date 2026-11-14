#include "prx/libSceAgcDriver/Resource/include/Lifecycle.hpp"

#include <cerrno>
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
 const uint64_t size = 0x4000ULL;
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(size_in_bytes, &size, sizeof(size));
 errno = savedErrno;
 if (!copied) return AGC_EINVAL;
 return 0;
}

}
