#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {
struct Converter;
} // namespace Icu

extern "C" {
void APS5_VABI ucnv_fixFileSeparator_nid_postfix(const Icu::Converter* converter, char16_t* source, std::int32_t length);
}
