#include "prx/libSceIcu/CharsetDetection.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucsdet.h"
#include <cstdint>

namespace {

UErrorCode& Status(std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return *reinterpret_cast<UErrorCode*>(error);
}

UCharsetDetector* Native(Icu::CharsetDetector* detector) {
    return reinterpret_cast<UCharsetDetector*>(detector);
}

const UCharsetMatch* Native(const Icu::CharsetMatch* match) {
    return reinterpret_cast<const UCharsetMatch*>(match);
}

}

extern "C" {

Icu::CharsetDetector* APS5_VABI ucsdet_open_nid_postfix(std::int32_t* error) {
    return reinterpret_cast<Icu::CharsetDetector*>(ucsdet_open(&Status(error)));
}

void APS5_VABI ucsdet_close_nid_postfix(Icu::CharsetDetector* detector) {
    ucsdet_close(Native(detector));
}

void APS5_VABI ucsdet_setText_nid_postfix(Icu::CharsetDetector* detector, const char* text, std::int32_t length,
                                          std::int32_t* error) {
    auto& status = Status(error);
    if (U_FAILURE(status)) return;
    if (!detector || (!text && length == -1)) APS5_INVALID_ARG_EX;
    ucsdet_setText(Native(detector), text, length, &status);
}

const Icu::CharsetMatch** APS5_VABI ucsdet_detectAll_nid_postfix(Icu::CharsetDetector* detector, std::int32_t* found,
                                                                 std::int32_t* error) {
    auto& status = Status(error);
    if (U_FAILURE(status)) return nullptr;
    if (!detector || !found) APS5_INVALID_ARG_EX;
    return reinterpret_cast<const Icu::CharsetMatch**>(ucsdet_detectAll(Native(detector), found, &status));
}

const char* APS5_VABI ucsdet_getName_nid_postfix(const Icu::CharsetMatch* match, std::int32_t* error) {
    auto& status = Status(error);
    if (U_FAILURE(status)) return nullptr;
    if (!match) APS5_INVALID_ARG_EX;
    return ucsdet_getName(Native(match), &status);
}

std::int32_t APS5_VABI ucsdet_getConfidence_nid_postfix(const Icu::CharsetMatch* match, std::int32_t* error) {
    auto& status = Status(error);
    if (U_FAILURE(status)) return 0;
    if (!match) APS5_INVALID_ARG_EX;
    return ucsdet_getConfidence(Native(match), &status);
}

std::int8_t APS5_VABI ucsdet_enableInputFilter_nid_postfix(Icu::CharsetDetector* detector, std::int8_t filter) {
    return ucsdet_enableInputFilter(Native(detector), filter);
}

}
