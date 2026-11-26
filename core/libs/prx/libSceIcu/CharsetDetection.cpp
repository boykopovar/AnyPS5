#include "prx/libSceIcu/CharsetDetection.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucsdet.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>

namespace {

bool EnterStatus(std::int32_t* error, UErrorCode& status, std::int32_t& previous, const char* caller) {
    if (!error) APS5_INVALID_ARG_EX;
    if (!GuestCopyIn_nid_no_patch(&previous, error, sizeof(previous))) NotImplemented_nid_no_patch(caller);
    status = static_cast<UErrorCode>(previous);
    if (U_FAILURE(status)) return false;
    if (!GuestWritable_nid_no_patch(error, sizeof(*error))) NotImplemented_nid_no_patch(caller);
    return true;
}

void LeaveStatus(std::int32_t* error, UErrorCode status, std::int32_t previous, const char* caller) {
    const auto updated = static_cast<std::int32_t>(status);
    if (updated != previous && !GuestCopyOut_nid_no_patch(error, &updated, sizeof(updated))) NotImplemented_nid_no_patch(caller);
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
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return nullptr;
    auto* detector = reinterpret_cast<Icu::CharsetDetector*>(ucsdet_open(&status));
    LeaveStatus(error, status, previous, __func__);
    return detector;
}

void APS5_VABI ucsdet_close_nid_postfix(Icu::CharsetDetector* detector) {
    ucsdet_close(Native(detector));
}

void APS5_VABI ucsdet_setText_nid_postfix(Icu::CharsetDetector* detector, const char* text, std::int32_t length,
                                          std::int32_t* error) {
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return;
    if (!detector || (!text && length == -1)) APS5_INVALID_ARG_EX;
    ucsdet_setText(Native(detector), text, length, &status);
    LeaveStatus(error, status, previous, __func__);
}

const Icu::CharsetMatch** APS5_VABI ucsdet_detectAll_nid_postfix(Icu::CharsetDetector* detector, std::int32_t* found,
                                                                 std::int32_t* error) {
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return nullptr;
    if (!detector || !found) APS5_INVALID_ARG_EX;
    std::int32_t hostFound = 0;
    if (!GuestCopyIn_nid_no_patch(&hostFound, found, sizeof(hostFound)) ||
        !GuestWritable_nid_no_patch(found, sizeof(*found))) {
        NotImplemented_nid_no_patch(__func__);
    }
    const auto** matches =
        reinterpret_cast<const Icu::CharsetMatch**>(ucsdet_detectAll(Native(detector), &hostFound, &status));
    if (!GuestCopyOut_nid_no_patch(found, &hostFound, sizeof(hostFound))) NotImplemented_nid_no_patch(__func__);
    LeaveStatus(error, status, previous, __func__);
    return matches;
}

const char* APS5_VABI ucsdet_getName_nid_postfix(const Icu::CharsetMatch* match, std::int32_t* error) {
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return nullptr;
    if (!match) APS5_INVALID_ARG_EX;
    const auto* name = ucsdet_getName(Native(match), &status);
    LeaveStatus(error, status, previous, __func__);
    return name;
}

std::int32_t APS5_VABI ucsdet_getConfidence_nid_postfix(const Icu::CharsetMatch* match, std::int32_t* error) {
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t previous = 0;
    if (!EnterStatus(error, status, previous, __func__)) return 0;
    if (!match) APS5_INVALID_ARG_EX;
    const auto confidence = ucsdet_getConfidence(Native(match), &status);
    LeaveStatus(error, status, previous, __func__);
    return confidence;
}

std::int8_t APS5_VABI ucsdet_enableInputFilter_nid_postfix(Icu::CharsetDetector* detector, std::int8_t filter) {
    return ucsdet_enableInputFilter(Native(detector), filter);
}

}
