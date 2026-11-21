#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Text.hpp"
#include "UnicodeString.hpp"
#include <cstdint>

extern "C" {
Icu::Text* APS5_VABI utext_setup_nid_postfix(Icu::Text*, std::int32_t, std::int32_t*);
Icu::Text* APS5_VABI utext_openUChars_nid_postfix(Icu::Text*, const char16_t*, std::int64_t, std::int32_t*);
Icu::Text* APS5_VABI utext_openConstUnicodeString_nid_postfix(Icu::Text*, const Icu::Abi::UnicodeString*, std::int32_t*);
}
