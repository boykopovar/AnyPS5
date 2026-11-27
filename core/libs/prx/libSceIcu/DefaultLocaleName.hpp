#pragma once

#include <string>

namespace Icu {
const char* DefaultLocaleName();
bool DefaultLocaleId(const char* locale);
std::string CanonicalLocaleName(const char* locale);
} // namespace Icu
