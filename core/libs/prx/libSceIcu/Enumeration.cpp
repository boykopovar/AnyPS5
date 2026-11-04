#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

namespace {

constexpr std::int32_t UnsupportedError = 16;

bool Failed(const UEnumeration* enumeration, std::int32_t* status) {
    if (!enumeration) return true;
    if (!status) APS5_INVALID_ARG_EX;
    return *status > 0;
}

}

extern "C" {

void APS5_VABI uenum_close_nid_postfix(UEnumeration* enumeration) {
    if (!enumeration) return;
    if (!enumeration->close) {
        free_nid_postfix(enumeration);
        return;
    }
    if (enumeration->baseContext) free_nid_postfix(enumeration->baseContext);
    enumeration->close(enumeration);
}

std::int32_t APS5_VABI uenum_count_nid_postfix(UEnumeration* enumeration, std::int32_t* status) {
    if (Failed(enumeration, status)) return -1;
    if (enumeration->count) return enumeration->count(enumeration, status);
    *status = UnsupportedError;
    return -1;
}

const char16_t* APS5_VABI uenum_unext_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (Failed(enumeration, status)) return nullptr;
    if (enumeration->uNext) return enumeration->uNext(enumeration, resultLength, status);
    *status = UnsupportedError;
    return nullptr;
}

const char* APS5_VABI uenum_next_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (Failed(enumeration, status)) return nullptr;
    if (!enumeration->next) {
        *status = UnsupportedError;
        return nullptr;
    }
    std::int32_t ignoredLength = 0;
    return enumeration->next(enumeration, resultLength ? resultLength : &ignoredLength, status);
}

}
