#include "prx/libSceIcu/StringSearchHost.hpp"
#include "unicode/usearch.h"
#include <mutex>

namespace {

std::mutex& OpenMutex() {
    static std::mutex mutex;
    return mutex;
}

}

namespace Icu::StringSearchHost {

void* Open(const char16_t* pattern, std::int32_t patternLength, const char16_t* text, std::int32_t textLength,
           const char* locale, void* breakIterator, std::int32_t& status) {
    const std::lock_guard lock(OpenMutex());
    auto code = static_cast<UErrorCode>(status);
    auto* search = usearch_open(pattern, patternLength, text, textLength, locale, static_cast<UBreakIterator*>(breakIterator), &code);
    status = code;
    return search;
}

void Close(void* search) {
    usearch_close(static_cast<UStringSearch*>(search));
}

std::int32_t GetMatchedLength(const void* search) {
    return usearch_getMatchedLength(static_cast<const UStringSearch*>(search));
}

void Reset(void* search) {
    usearch_reset(static_cast<UStringSearch*>(search));
}

void SetOffset(void* search, std::int32_t position, std::int32_t& status) {
    auto code = static_cast<UErrorCode>(status);
    usearch_setOffset(static_cast<UStringSearch*>(search), position, &code);
    status = code;
}

}
