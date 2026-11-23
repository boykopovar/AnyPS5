#include "prx/libSceIcu/BreakRules.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ubrk.h"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

static_assert(sizeof(UParseError) == 72 && offsetof(UParseError, line) == 0 &&
    offsetof(UParseError, offset) == 4 && offsetof(UParseError, preContext) == 8 && offsetof(UParseError, postContext) == 40);

std::mutex TextMutex;
std::unordered_map<UBreakIterator*, std::shared_ptr<std::u16string>> TextBacking;

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* caller) {
    const int saved = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = saved;
    if (!copied) NotImplemented_nid_no_patch(caller);
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* caller) {
    const int saved = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = saved;
    if (!copied) NotImplemented_nid_no_patch(caller);
}

struct Status {
    std::int32_t* address;
    std::int32_t incoming;
    UErrorCode value;
    const char* caller;

    Status(std::int32_t* source, const char* name) : address(source), incoming(0), value(U_ZERO_ERROR), caller(name) {
        if (!source) APS5_INVALID_ARG_EX;
        CopyIn(&incoming, source, sizeof(incoming), caller);
        value = static_cast<UErrorCode>(incoming);
    }

    void Commit() const {
        const auto outgoing = static_cast<std::int32_t>(value);
        if (outgoing != incoming) CopyOut(address, &outgoing, sizeof(outgoing), caller);
    }
};

std::u16string CopyText(const char16_t* source, std::int32_t length, const char* caller) {
    if (!source) return {};
    if (length < -1) APS5_INVALID_ARG_EX;
    std::u16string result;
    if (length >= 0) {
        result.resize(static_cast<std::size_t>(length));
        if (length) CopyIn(result.data(), source, result.size() * sizeof(char16_t), caller);
        return result;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(source);
    constexpr auto maximum = std::numeric_limits<std::uintptr_t>::max();
    for (;;) {
        const auto offset = result.size() * sizeof(char16_t);
        if (result.size() == static_cast<std::size_t>(INT32_MAX) ||
            base > maximum - (sizeof(char16_t) - 1) || offset > maximum - base - (sizeof(char16_t) - 1))
            NotImplemented_nid_no_patch(caller);
        char16_t unit = 0;
        CopyIn(&unit, reinterpret_cast<const void*>(base + offset), sizeof(unit), caller);
        if (!unit) return result;
        result.push_back(unit);
    }
}

void RetainText(UBreakIterator* iterator, const std::shared_ptr<std::u16string>& text) {
    if (!iterator || !text) return;
    const std::lock_guard lock(TextMutex);
    TextBacking.emplace(iterator, text);
}

std::shared_ptr<std::u16string> FindText(const UBreakIterator* iterator) {
    const std::lock_guard lock(TextMutex);
    const auto found = TextBacking.find(const_cast<UBreakIterator*>(iterator));
    return found == TextBacking.end() ? nullptr : found->second;
}

struct Closer {
    void operator()(UBreakIterator* iterator) const { Icu::CloseBreakIterator_nid_no_patch(iterator); }
};

using Iterator = std::unique_ptr<UBreakIterator, Closer>;

}

namespace Icu {

void CloseBreakIterator_nid_no_patch(void* iterator) {
    std::shared_ptr<std::u16string> backing;
    {
        const std::lock_guard lock(TextMutex);
        const auto found = TextBacking.find(static_cast<UBreakIterator*>(iterator));
        if (found != TextBacking.end()) {
            backing = std::move(found->second);
            TextBacking.erase(found);
        }
    }
    ubrk_close(static_cast<UBreakIterator*>(iterator));
}

}

extern "C" {

UBreakIterator* APS5_VABI ubrk_openRules_nid_postfix(const char16_t* rules, std::int32_t rulesLength,
    const char16_t* text, std::int32_t textLength, UParseError* parseError, std::int32_t* error) {
    if (!error) return nullptr;
    Status status(error, __func__);
    if (U_FAILURE(status.value)) return nullptr;
    try {
        const auto copiedRules = CopyText(rules, rulesLength, __func__);
        auto copiedText = std::make_shared<std::u16string>(CopyText(text, textLength, __func__));
        UParseError parsed{};
        if (parseError) CopyIn(&parsed, parseError, sizeof(parsed), __func__);
        const auto before = parsed;
        Iterator iterator(ubrk_openRules(rules ? copiedRules.c_str() : nullptr, rulesLength,
            text ? copiedText->c_str() : nullptr, textLength, parseError ? &parsed : nullptr, &status.value));
        RetainText(iterator.get(), copiedText);
        if (parseError && std::memcmp(&before, &parsed, sizeof(parsed)) != 0)
            CopyOut(parseError, &parsed, sizeof(parsed), __func__);
        status.Commit();
        return iterator.release();
    } catch (const std::bad_alloc&) {
        status.value = U_MEMORY_ALLOCATION_ERROR;
        status.Commit();
        return nullptr;
    }
}

std::int32_t APS5_VABI ubrk_getRuleStatusVec_nid_postfix(UBreakIterator* iterator, std::int32_t* values,
    std::int32_t capacity, std::int32_t* error) {
    Status status(error, __func__);
    if (U_FAILURE(status.value)) return 0;
    if (!iterator || (!values && capacity > 0)) APS5_INVALID_ARG_EX;
    try {
        UErrorCode probeStatus = U_ZERO_ERROR;
        const auto count = ubrk_getRuleStatusVec(iterator, nullptr, 0, &probeStatus);
        if (count < 0) NotImplemented_nid_no_patch(__func__);
        std::vector<std::int32_t> copied(static_cast<std::size_t>(std::min(std::max(capacity, 0), count)));
        const auto result = ubrk_getRuleStatusVec(iterator, copied.data(), capacity, &status.value);
        if (!copied.empty()) CopyOut(values, copied.data(), copied.size() * sizeof(copied[0]), __func__);
        status.Commit();
        return result;
    } catch (const std::bad_alloc&) {
        status.value = U_MEMORY_ALLOCATION_ERROR;
        status.Commit();
        return 0;
    }
}

const char* APS5_VABI ubrk_getLocaleByType_nid_postfix(const UBreakIterator* iterator,
    std::int32_t type, std::int32_t* error) {
    Status status(error, __func__);
    if (U_FAILURE(status.value)) return nullptr;
    if (!iterator || (type != ULOC_VALID_LOCALE && type != ULOC_ACTUAL_LOCALE)) {
        status.value = U_ILLEGAL_ARGUMENT_ERROR;
        status.Commit();
        return nullptr;
    }
    const auto* result = FindText(iterator) ? "" :
        ubrk_getLocaleByType(iterator, static_cast<ULocDataLocaleType>(type), &status.value);
    status.Commit();
    return result;
}

UBreakIterator* APS5_VABI ubrk_safeClone_nid_postfix(const UBreakIterator* iterator,
    void*, std::int32_t* bufferSize, std::int32_t* error) {
    if (!error) return nullptr;
    Status status(error, __func__);
    if (U_FAILURE(status.value)) return nullptr;
    if (!iterator) {
        status.value = U_ILLEGAL_ARGUMENT_ERROR;
        status.Commit();
        return nullptr;
    }
    if (bufferSize) {
        std::int32_t incoming = 0;
        CopyIn(&incoming, bufferSize, sizeof(incoming), __func__);
        const std::int32_t required = 1;
        CopyOut(bufferSize, &required, sizeof(required), __func__);
        if (!incoming) return nullptr;
    }
    try {
        const auto text = FindText(iterator);
        Iterator copy(ubrk_clone(iterator, &status.value));
        RetainText(copy.get(), text);
        if (copy) status.value = U_SAFECLONE_ALLOCATED_WARNING;
        status.Commit();
        return copy.release();
    } catch (const std::bad_alloc&) {
        status.value = U_MEMORY_ALLOCATION_ERROR;
        status.Commit();
        return nullptr;
    }
}

}
