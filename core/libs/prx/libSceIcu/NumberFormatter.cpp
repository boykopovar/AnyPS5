#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/ResultHandles.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

namespace {

Icu::ResultHandles<UFormattedNumber> g_results;

}

extern "C" {

void APS5_VABI unumf_close_nid_postfix(UNumberFormatter* formatter) {
    if (!formatter) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI unumf_closeResult_nid_postfix(UFormattedNumber* result) {
    g_results.Close(result);
}

UFormattedNumber* APS5_VABI unumf_openResult_nid_postfix(std::int32_t* status) {
    return g_results.Open(status);
}

}
