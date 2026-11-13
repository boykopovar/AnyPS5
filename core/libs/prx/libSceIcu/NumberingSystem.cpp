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

const char* APS5_VABI unumsys_getName_nid_postfix(const UNumberingSystem*) {
    NotImplemented_nid_no_patch(__func__);
}

std::int8_t APS5_VABI unumsys_isAlgorithmic_nid_postfix(const UNumberingSystem*) {
    NotImplemented_nid_no_patch(__func__);
}

UEnumeration* APS5_VABI unumsys_openAvailableNames_nid_postfix(std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

}
