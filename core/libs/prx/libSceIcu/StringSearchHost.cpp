#include "prx/libSceIcu/StringSearchHost.hpp"
#include "unicode/usearch.h"

namespace Icu::StringSearchHost {

void* Open(const char16_t* pattern, std::int32_t patternLength, const char16_t* text, std::int32_t textLength,
           const char* locale, void* breakIterator, std::int32_t& status) {
    auto code = static_cast<UErrorCode>(status);
    auto* search = usearch_open(pattern, patternLength, text, textLength, locale, static_cast<UBreakIterator*>(breakIterator), &code);
    status = code;
    return search;
}

void Close(void* search) {
    usearch_close(static_cast<UStringSearch*>(search));
}

}
