#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/ResultHandles.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

namespace {

Icu::ResultHandles<UFormattedDateInterval> g_results;

}

extern "C" {

void APS5_VABI udtitvfmt_close_nid_postfix(UDateIntervalFormat* formatter) {
    if (!formatter) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI udtitvfmt_closeResult_nid_postfix(UFormattedDateInterval* result) {
    g_results.Close(result);
}

void APS5_VABI udtitvfmt_formatCalendarToResult_nid_postfix(const UDateIntervalFormat*, UCalendar*, UCalendar*, UFormattedDateInterval* result, std::int32_t* status) {
    if (Icu::Failure(status)) return;
    if (!result) {
        *status = Icu::IllegalArgument;
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI udtitvfmt_formatToResult_nid_postfix(const UDateIntervalFormat*, double, double, UFormattedDateInterval* result, std::int32_t* status) {
    if (Icu::Failure(status)) return;
    if (!result) {
        *status = Icu::IllegalArgument;
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

UDateIntervalFormat* APS5_VABI udtitvfmt_open_nid_postfix(const char*, const char16_t* skeleton, std::int32_t skeletonLength, const char16_t* tzID, std::int32_t tzIDLength, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    if ((!skeleton ? skeletonLength != 0 : skeletonLength < -1) || (!tzID ? tzIDLength != 0 : tzIDLength < -1)) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

UFormattedDateInterval* APS5_VABI udtitvfmt_openResult_nid_postfix(std::int32_t* status) {
    return g_results.Open(status);
}

}
