#include "prx/libSceIcu/StringSearchHost.hpp"
#include "cmemory.h"
#include "unicode/usearch.h"
#include "usrchimp.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>

namespace {

std::mutex& OpenMutex() {
    static std::mutex mutex;
    return mutex;
}

std::atomic_bool& PatternAllocationFailure() {
    static std::atomic_bool failure;
    return failure;
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

std::int32_t GetAttribute(const void* search, std::int32_t attribute) {
    return usearch_getAttribute(static_cast<const UStringSearch*>(search), static_cast<USearchAttribute>(attribute));
}

void Reset(void* search) {
    usearch_reset(static_cast<UStringSearch*>(search));
}

void SetOffset(void* search, std::int32_t position, std::int32_t& status) {
    auto code = static_cast<UErrorCode>(status);
    usearch_setOffset(static_cast<UStringSearch*>(search), position, &code);
    status = code;
}

void SetAttribute(void* search, std::int32_t attribute, std::int32_t value, std::int32_t& status) {
    auto code = static_cast<UErrorCode>(status);
    usearch_setAttribute(static_cast<UStringSearch*>(search), static_cast<USearchAttribute>(attribute),
                         static_cast<USearchAttributeValue>(value), &code);
    status = code;
}

void SetPattern(void* search, const char16_t* pattern, std::int32_t patternLength, std::int32_t& status) {
    auto code = static_cast<UErrorCode>(status);
    auto* native = static_cast<UStringSearch*>(search);
    const auto previousText = native->pattern.text;
    const auto previousTextLength = native->pattern.textLength;
    const auto previousCes = native->pattern.ces;
    const auto previousCesLength = native->pattern.cesLength;
    const auto previousPces = native->pattern.pces;
    const auto previousPcesLength = native->pattern.pcesLength;
    const auto previousPrefixAccents = native->pattern.hasPrefixAccents;
    const auto previousSuffixAccents = native->pattern.hasSuffixAccents;
    std::array<std::int32_t, INITIAL_ARRAY_SIZE_> previousCesBuffer{};
    std::copy_n(native->pattern.cesBuffer, previousCesBuffer.size(), previousCesBuffer.begin());
    native->pattern.ces = native->pattern.cesBuffer;
    native->pattern.cesLength = 0;
    native->pattern.pces = nullptr;
    native->pattern.pcesLength = 0;
    if (PatternAllocationFailure().exchange(false)) {
        code = U_MEMORY_ALLOCATION_ERROR;
    } else {
        usearch_setPattern(native, pattern, patternLength, &code);
    }
    if (U_FAILURE(code)) {
        if (native->pattern.ces != native->pattern.cesBuffer && native->pattern.ces) uprv_free(native->pattern.ces);
        if (native->pattern.pces != native->pattern.pcesBuffer && native->pattern.pces) uprv_free(native->pattern.pces);
        native->pattern.text = previousText;
        native->pattern.textLength = previousTextLength;
        native->pattern.ces = previousCes;
        native->pattern.cesLength = previousCesLength;
        native->pattern.pces = previousPces;
        native->pattern.pcesLength = previousPcesLength;
        native->pattern.hasPrefixAccents = previousPrefixAccents;
        native->pattern.hasSuffixAccents = previousSuffixAccents;
        std::copy(previousCesBuffer.begin(), previousCesBuffer.end(), native->pattern.cesBuffer);
    } else {
        if (previousCes != native->pattern.cesBuffer && previousCes) uprv_free(previousCes);
        if (previousPces != native->pattern.pcesBuffer && previousPces) uprv_free(previousPces);
    }
    status = code;
}

void FailNextPatternAllocation() {
    PatternAllocationFailure() = true;
}

void SetText(void* search, const char16_t* text, std::int32_t textLength, std::int32_t& status) {
    auto code = static_cast<UErrorCode>(status);
    usearch_setText(static_cast<UStringSearch*>(search), text, textLength, &code);
    status = code;
}

std::int32_t Next(void* search, std::int32_t& status) {
    auto code = static_cast<UErrorCode>(status);
    const auto result = usearch_next(static_cast<UStringSearch*>(search), &code);
    status = code;
    return result;
}

void* GetCollator(const void* search) {
    return usearch_getCollator(static_cast<const UStringSearch*>(search));
}

}
