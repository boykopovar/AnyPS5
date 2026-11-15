#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"

extern "C" {

void APS5_VABI sceLibcInternalPadCompleteDestructor_nid_no_patch(void* self) {
    _ZNSt4_PadD2Ev_nid_postfix(static_cast<GuestPad*>(self));
}
APS5_EXPORT("XyJPhPqpzMw", sceLibcInternalPadCompleteDestructor_nid_no_patch);

}
