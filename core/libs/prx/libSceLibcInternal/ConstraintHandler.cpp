#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libc/include/SonyCrt.hpp"

extern "C" {

void APS5_VABI _Fail_s_nid_postfix(const char* message, void* pointer, int error) {
    SonyInvokeConstraintHandler_nid_no_patch(message, pointer, error);
}

}
