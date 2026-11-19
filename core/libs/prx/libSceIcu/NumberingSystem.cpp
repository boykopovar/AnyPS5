#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

void APS5_VABI unumsys_close_nid_postfix(UNumberingSystem* unumsys) {
    if (!unumsys) return;
    NotImplemented_nid_no_patch(__func__);
}

UNumberingSystem* APS5_VABI unumsys_open_nid_postfix(const char*, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

UNumberingSystem* APS5_VABI unumsys_openByName_nid_postfix(const char*, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

}
