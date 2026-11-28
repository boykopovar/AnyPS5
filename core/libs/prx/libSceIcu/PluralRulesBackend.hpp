#pragma once
#include <cstdint>

struct UEnumeration;

namespace Icu::PluralRulesBackend {

void* OpenForType(const char* locale, std::int32_t type, std::int32_t* status);
void Close(void* rules);
UEnumeration* GetKeywords(const void* rules, std::int32_t* status);

}
