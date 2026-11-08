#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

void APS5_VABI uldn_close_nid_postfix(ULocaleDisplayNames* names) {
    if (!names) return;
    NotImplemented_nid_no_patch(__func__);
}

ULocaleDisplayNames* APS5_VABI uldn_openForContext_nid_postfix(const char*, std::int32_t*, std::int32_t, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

}
