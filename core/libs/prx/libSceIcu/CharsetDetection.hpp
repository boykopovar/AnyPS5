#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {
struct CharsetDetector;
struct CharsetMatch;
} // namespace Icu

extern "C" {
Icu::CharsetDetector* APS5_VABI ucsdet_open_nid_postfix(std::int32_t*);
void APS5_VABI ucsdet_close_nid_postfix(Icu::CharsetDetector*);
void APS5_VABI ucsdet_setText_nid_postfix(Icu::CharsetDetector*, const char*, std::int32_t, std::int32_t*);
const Icu::CharsetMatch** APS5_VABI ucsdet_detectAll_nid_postfix(Icu::CharsetDetector*, std::int32_t*, std::int32_t*);
const char* APS5_VABI ucsdet_getName_nid_postfix(const Icu::CharsetMatch*, std::int32_t*);
std::int32_t APS5_VABI ucsdet_getConfidence_nid_postfix(const Icu::CharsetMatch*, std::int32_t*);
std::int8_t APS5_VABI ucsdet_enableInputFilter_nid_postfix(Icu::CharsetDetector*, std::int8_t);
}
