#include "prx/libSceDbgThreadSanitizer/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI __tsan_acquire(void*) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI __tsan_release(void*) {
    NotImplemented_nid_no_patch(__func__);
}

}
