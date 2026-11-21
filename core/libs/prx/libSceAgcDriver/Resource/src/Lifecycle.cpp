#include "prx/libSceAgcDriver/Resource/include/Lifecycle.hpp"

#include <cerrno>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr uint32_t AGC_EINVAL = 0x80020065u;
static constexpr uint32_t RESOURCE_REGISTRATION_UNAVAILABLE = 0x8A6C9018u;

extern "C" {

uint32_t APS5_VABI sceAgcDriverInitResourceRegistration(void) {
 return RESOURCE_REGISTRATION_UNAVAILABLE;
}

uint32_t APS5_VABI sceAgcDriverQueryResourceRegistrationUserMemoryRequirements(uint64_t* size_in_bytes) {
 if (size_in_bytes != nullptr) {
  const uint64_t size = 0;
  const int savedErrno = errno;
  const bool copied = GuestCopyOut_nid_no_patch(size_in_bytes, &size, sizeof(size));
  errno = savedErrno;
  if (!copied) return AGC_EINVAL;
 }
 return RESOURCE_REGISTRATION_UNAVAILABLE;
}

}
