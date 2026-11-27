#pragma once
#include <cstdint>

namespace Icu::StringSearchHost {

void* Open(const char16_t* pattern, std::int32_t patternLength, const char16_t* text, std::int32_t textLength,
           const char* locale, void* breakIterator, std::int32_t& status);
void Close(void* search);

}

namespace Icu::StringSearchRegistry {

void Cleanup();

}
