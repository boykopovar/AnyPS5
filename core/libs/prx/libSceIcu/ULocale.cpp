#include "prx/libSceIcu/ULocale.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ulocale.h"

namespace {

UErrorCode& Status(std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return *reinterpret_cast<UErrorCode*>(error);
}

}

extern "C" {

ULocale* APS5_VABI ulocale_openForLocaleID_nid_postfix(const char* locale, std::int32_t length,
                                                        std::int32_t* error) {
    return ulocale_openForLocaleID(locale, length, &Status(error));
}

void APS5_VABI ulocale_close_nid_postfix(ULocale* locale) {
    ulocale_close(locale);
}

const char* APS5_VABI ulocale_getLocaleID_nid_postfix(const ULocale* locale) {
    return ulocale_getLocaleID(locale);
}

bool APS5_VABI ulocale_isBogus_nid_postfix(const ULocale* locale) {
    return ulocale_isBogus(locale);
}

}
