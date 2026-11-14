#include "prx/libSceIcu/LocaleMap.hpp"
#include "prx/libc/include/General.hpp"
#include "locmap.h"
#include <cstdint>

namespace {

UErrorCode& Status(std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return *reinterpret_cast<UErrorCode*>(error);
}

}

extern "C" {

std::int32_t APS5_VABI uprv_convertToPosix_nid_postfix(std::uint32_t hostID, char* posixID, std::int32_t capacity,
                                                        std::int32_t* error) {
    auto& status = Status(error);
    if (capacity < 0 || (!posixID && capacity > 0)) APS5_INVALID_ARG_EX;
    char preflight = 0;
    return uprv_convertToPosix(hostID, posixID ? posixID : &preflight, capacity, &status);
}

std::uint32_t APS5_VABI uprv_convertToLCIDPlatform_nid_postfix(const char* localeID, std::int32_t* error) {
    return uprv_convertToLCIDPlatform(localeID, &Status(error));
}

std::uint32_t APS5_VABI uprv_convertToLCID_nid_postfix(const char* languageID, const char* posixID, std::int32_t* error) {
    return uprv_convertToLCID(languageID, posixID, &Status(error));
}

}
