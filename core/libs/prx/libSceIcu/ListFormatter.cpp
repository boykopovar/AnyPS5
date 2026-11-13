#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

void APS5_VABI ulistfmt_close_nid_postfix(UListFormatter* listfmt) {
    if (!listfmt) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI ulistfmt_closeResult_nid_postfix(UFormattedList* result) {
    if (!result) return;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ulistfmt_format_nid_postfix(const UListFormatter*, const char16_t* const[],
                                                   const std::int32_t*, std::int32_t, char16_t* result,
                                                   std::int32_t resultCapacity, std::int32_t* status) {
    if (Icu::Failure(status)) return -1;
    if ((result == nullptr) ? resultCapacity != 0 : resultCapacity < 0) {
        *status = Icu::IllegalArgument;
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI ulistfmt_formatStringsToResult_nid_postfix(const UListFormatter*, const char16_t* const[],
                                                          const std::int32_t*, std::int32_t, UFormattedList* uresult,
                                                          std::int32_t* status) {
    if (Icu::Failure(status)) return;
    if (!uresult) {
        *status = Icu::IllegalArgument;
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

UListFormatter* APS5_VABI ulistfmt_openForType_nid_postfix(const char*, UListFormatterType, UListFormatterWidth,
                                                           std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

UFormattedList* APS5_VABI ulistfmt_openResult_nid_postfix(std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

}
