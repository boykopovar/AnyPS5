#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

void APS5_VABI udatpg_close_nid_postfix(UDateTimePatternGenerator* dtpg) {
    if (!dtpg) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI udatpg_getBestPattern_nid_postfix(UDateTimePatternGenerator*, const char16_t* skeleton, std::int32_t length,
                                                         char16_t*, std::int32_t, std::int32_t* status) {
    if (Icu::Failure(status)) {
        return 0;
    }
    if (!skeleton && length != 0) {
        *status = Icu::IllegalArgument;
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI udatpg_getBestPatternWithOptions_nid_postfix(
    UDateTimePatternGenerator*,
    const char16_t* skeleton,
    std::int32_t length,
    UDateTimePatternMatchOptions,
    char16_t*,
    std::int32_t,
    std::int32_t* pErrorCode) {
    if (Icu::Failure(pErrorCode)) {
        return 0;
    }
    if (!skeleton && length != 0) {
        *pErrorCode = Icu::IllegalArgument;
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI udatpg_getFieldDisplayName_nid_postfix(const UDateTimePatternGenerator*,
                                                                        std::int32_t,
                                                                        std::int32_t,
                                                                        char16_t* fieldName,
                                                                        std::int32_t capacity,
                                                                        std::int32_t* status) {
    if (Icu::Failure(status)) return -1;
    if (fieldName == nullptr ? capacity != 0 : capacity < 0) {
        *status = Icu::IllegalArgument;
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI udatpg_getSkeleton_nid_postfix(UDateTimePatternGenerator*, const char16_t* pattern, std::int32_t length,
                                                     char16_t*, std::int32_t, std::int32_t* errorCode) {
    if (Icu::Failure(errorCode)) return 0;
    if (!pattern && length != 0) {
        *errorCode = Icu::IllegalArgument;
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

UDateTimePatternGenerator* APS5_VABI udatpg_open_nid_postfix(const char*, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

}
