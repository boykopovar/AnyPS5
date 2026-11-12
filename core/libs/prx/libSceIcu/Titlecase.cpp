#include "prx/libSceIcu/Titlecase.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ubrk.h"
#include "unicode/ustring.h"
#include <cstdint>

namespace {

bool InvalidMapping(const char16_t* destination, std::int32_t capacity, const char16_t* source, std::int32_t length) {
    return capacity < 0 || (!destination && capacity > 0) || !source || length < -1;
}

}

extern "C" {

std::int32_t APS5_VABI u_strToTitle_nid_postfix(char16_t* destination, std::int32_t capacity, const char16_t* source,
                                                std::int32_t length, void* iterator, const char* locale, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    auto status = static_cast<UErrorCode>(*error);
    if (!locale && U_SUCCESS(status) && !InvalidMapping(destination, capacity, source, length)) {
        NotImplemented_nid_no_patch("u_strToTitle: default locale");
    }
    auto result = u_strToTitle(destination, capacity, source, length, static_cast<UBreakIterator*>(iterator), locale, &status);
    *error = status;
    return result;
}

}
