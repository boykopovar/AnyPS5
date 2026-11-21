#include "prx/libSceIcu/DateFormat.hpp"
#include "prx/libSceIcu/DefaultLocaleName.hpp"
#include "prx/libSceIcu/FieldPositionIterator.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "unicode/udat.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

namespace {

std::mutex formatMutex;
std::unordered_set<const UDateFormat*> formats;

void CopyIn(void* to, const void* from, std::size_t size, const char* caller) {
    const int saved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(to, from, size);
    errno = saved;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

void CopyOut(void* to, const void* from, std::size_t size, const char* caller) {
    const int saved = errno;
    const bool ok = GuestCopyOut_nid_no_patch(to, from, size);
    errno = saved;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

std::int32_t ReadStatus(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    std::int32_t value;
    CopyIn(&value, status, sizeof(value), caller);
    return value;
}

void StoreStatus(std::int32_t* status, std::int32_t previous, std::int32_t value, const char* caller) {
    if (value != previous) CopyOut(status, &value, sizeof(value), caller);
}

void RequireFormat(const UDateFormat* format, const char* caller) {
    if (!formats.contains(format)) NotImplemented_nid_no_patch(caller);
}

bool KnownStyle(std::int32_t style) {
    return (style >= UDAT_PATTERN && style <= UDAT_SHORT) ||
           (style >= UDAT_FULL_RELATIVE && style <= UDAT_SHORT_RELATIVE);
}

bool KnownSymbol(std::int32_t type) {
    return type >= UDAT_ERAS && type <= UDAT_AM_PMS_WIDE;
}

template <typename Char>
std::basic_string<Char> InputString(const Char* text, std::int32_t length, const char* caller) {
    std::basic_string<Char> copy;
    if (!text) return copy;
    auto address = reinterpret_cast<std::uintptr_t>(text);
    if (length > 0) {
        std::array<Char, 256> block;
        while (length > 0) {
            const auto units = std::min(length, static_cast<std::int32_t>(block.size()));
            const auto bytes = units * sizeof(Char);
            if (bytes - 1 > UINTPTR_MAX - address) NotImplemented_nid_no_patch(caller);
            CopyIn(block.data(), reinterpret_cast<const void*>(address), bytes, caller);
            copy.append(block.data(), units);
            length -= units;
            if (length && bytes > UINTPTR_MAX - address) NotImplemented_nid_no_patch(caller);
            address += bytes;
        }
    } else if (length == -1) {
        for (;;) {
            Char unit;
            CopyIn(&unit, reinterpret_cast<const void*>(address), sizeof(unit), caller);
            if (!unit) break;
            if (address > UINTPTR_MAX - sizeof(Char)) NotImplemented_nid_no_patch(caller);
            copy.push_back(unit);
            address += sizeof(Char);
        }
    }
    return copy;
}

void CopyOverflowWrites(char16_t* destination, const char16_t* zeroFilled, const std::u16string& marked,
                        const char* caller) {
    for (std::size_t i = 0; i < marked.size();) {
        const auto start = i;
        while (i < marked.size() && (marked[i] != 0xffff || zeroFilled[i] == 0xffff)) ++i;
        if (i == start) ++i;
        else CopyOut(destination + start, marked.data() + start, (i - start) * sizeof(char16_t), caller);
    }
}

template <typename Operation>
std::int32_t WriteResult(char16_t* destination, std::int32_t capacity, UErrorCode& error,
                         const char* caller, Operation operation) {
    char16_t empty{};
    if (capacity == 0) return operation(destination ? &empty : nullptr, 0, &error);

    std::array<char16_t, 128> small{};
    const auto initial = error;
    auto size = std::min(capacity, static_cast<std::int32_t>(small.size()));
    auto* buffer = small.data();
    std::u16string storage;
    for (;;) {
        error = initial;
        const auto length = operation(buffer, size, &error);
        if (length < 0 || (U_FAILURE(error) && error != U_BUFFER_OVERFLOW_ERROR)) return length;
        if (length < size || (length == size && size == capacity)) {
            const auto written = length < size ? length + 1 : length;
            CopyOut(destination, buffer, written * sizeof(char16_t), caller);
            return length;
        }
        if (size == capacity) {
            std::u16string marked(capacity, 0xffff);
            error = initial;
            const auto actual = operation(marked.data(), capacity, &error);
            if (actual < 0 || (U_FAILURE(error) && error != U_BUFFER_OVERFLOW_ERROR)) return actual;
            if (actual <= capacity) {
                const auto written = actual < capacity ? actual + 1 : actual;
                CopyOut(destination, marked.data(), written * sizeof(char16_t), caller);
            } else {
                CopyOverflowWrites(destination, buffer, marked, caller);
            }
            return actual;
        }
        size = length < capacity ? length + 1 : capacity;
        storage.assign(size, 0);
        buffer = storage.data();
    }
}

template <typename Action>
std::int32_t FormatResult(char16_t* result, std::int32_t capacity, std::int32_t* status,
                          const char* caller, Action action) {
    const auto incoming = ReadStatus(status, caller);
    if (incoming > 0) return -1;
    if (capacity < 0 || (!result && capacity != 0)) {
        StoreStatus(status, incoming, U_ILLEGAL_ARGUMENT_ERROR, caller);
        return -1;
    }
    auto error = static_cast<UErrorCode>(incoming);
    std::int32_t length = -1;
    try {
        const std::lock_guard lock(formatMutex);
        length = action(error);
    } catch (const std::bad_alloc&) {
        error = U_MEMORY_ALLOCATION_ERROR;
    }
    StoreStatus(status, incoming, error, caller);
    return length;
}

}

extern "C" {

void APS5_VABI udat_close_nid_postfix(UDateFormat* format) {
    if (!format) return;
    const std::lock_guard lock(formatMutex);
    if (!formats.erase(format)) NotImplemented_nid_no_patch(__func__);
    udat_close(format);
}

std::int32_t APS5_VABI udat_countSymbols_nid_postfix(const UDateFormat* format, std::int32_t type) {
    if (!format) return 0;
    const std::lock_guard lock(formatMutex);
    RequireFormat(format, __func__);
    if (!KnownSymbol(type)) return 0;
    return udat_countSymbols(format, static_cast<UDateFormatSymbolType>(type));
}

std::int32_t APS5_VABI udat_format_nid_postfix(const UDateFormat* format, double date, char16_t* result,
                                              std::int32_t capacity, UFieldPosition* position, std::int32_t* status) {
    const char* caller = __func__;
    return FormatResult(result, capacity, status, caller, [&](UErrorCode& error) {
        RequireFormat(format, caller);
        UFieldPosition field{};
        if (position) CopyIn(&field.field, position, sizeof(field.field), caller);
        const auto length = WriteResult(result, capacity, error, caller, [&](char16_t* out, int size, UErrorCode* ec) {
            return udat_format(format, date, out, size, position ? &field : nullptr, ec);
        });
        if (position) {
            CopyOut(&position->beginIndex, &field.beginIndex, sizeof(field.beginIndex), caller);
            CopyOut(&position->endIndex, &field.endIndex, sizeof(field.endIndex), caller);
        }
        return length;
    });
}

std::int32_t APS5_VABI udat_formatForFields_nid_postfix(const void* format, double date, char16_t* result,
                                                       std::int32_t capacity, void* iterator, std::int32_t* status) {
    const char* caller = __func__;
    return FormatResult(result, capacity, status, caller, [&](UErrorCode& error) {
        const auto* fmt = static_cast<const UDateFormat*>(format);
        RequireFormat(fmt, caller);
        auto* fields = static_cast<UFieldPositionIterator*>(iterator);
        std::unique_lock<std::mutex> lock;
        if (fields) lock = Icu::LockFieldPositionIterator_nid_no_patch(fields, caller);
        return WriteResult(result, capacity, error, caller, [&](char16_t* out, int size, UErrorCode* ec) {
            return udat_formatForFields(fmt, date, out, size, fields, ec);
        });
    });
}

const UCalendar* APS5_VABI udat_getCalendar_nid_postfix(const UDateFormat* format) {
    const std::lock_guard lock(formatMutex);
    RequireFormat(format, __func__);
    return udat_getCalendar(format);
}

std::int32_t APS5_VABI udat_getSymbols_nid_postfix(const void* format, std::int32_t type, std::int32_t index,
                                                 char16_t* result, std::int32_t capacity, std::int32_t* status) {
    if (!format) return -1;
    const char* caller = __func__;
    const std::lock_guard lock(formatMutex);
    const auto* fmt = static_cast<const UDateFormat*>(format);
    RequireFormat(fmt, caller);
    if (!KnownSymbol(type)) {
        if (index < 0) NotImplemented_nid_no_patch(caller);
        return 0;
    }
    const auto symbol = static_cast<UDateFormatSymbolType>(type);
    if (symbol != UDAT_LOCALIZED_CHARS) {
        if (index >= udat_countSymbols(fmt, symbol)) return 0;
        if (index < 0) NotImplemented_nid_no_patch(caller);
    }
    const auto incoming = ReadStatus(status, caller);
    auto error = static_cast<UErrorCode>(incoming);
    std::int32_t length;
    try {
        if (incoming > 0 || capacity < 0 || !result) {
            length = udat_getSymbols(fmt, symbol, index, nullptr, capacity, &error);
        } else {
            length = WriteResult(result, capacity, error, caller, [&](char16_t* out, int size, UErrorCode* ec) {
                return udat_getSymbols(fmt, symbol, index, out, size, ec);
            });
        }
    } catch (const std::bad_alloc&) {
        length = -1;
        error = U_MEMORY_ALLOCATION_ERROR;
    }
    StoreStatus(status, incoming, error, caller);
    return length;
}

UDateFormat* APS5_VABI udat_open_nid_postfix(std::int32_t timeStyle, std::int32_t dateStyle, const char* locale,
                                            const char16_t* zone, std::int32_t zoneLength, const char16_t* pattern,
                                            std::int32_t patternLength, std::int32_t* status) {
    if (!status) APS5_INVALID_ARG_EX;
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    if (!KnownStyle(timeStyle) || (timeStyle != UDAT_PATTERN && !KnownStyle(dateStyle))) {
        NotImplemented_nid_no_patch(__func__);
    }
    auto error = static_cast<UErrorCode>(incoming);
    try {
        const auto name = InputString(locale, -1, __func__);
        const auto zoneId = InputString(zone, zoneLength, __func__);
        const auto pat = InputString(timeStyle == UDAT_PATTERN ? pattern : nullptr, patternLength, __func__);
        std::unique_ptr<UDateFormat, decltype(&udat_close)> format(
            udat_open(static_cast<UDateFormatStyle>(timeStyle), static_cast<UDateFormatStyle>(timeStyle == UDAT_PATTERN ? UDAT_PATTERN : dateStyle),
                      locale ? name.c_str() : Icu::DefaultLocaleName(), zone ? zoneId.c_str() : nullptr, zoneLength,
                      pattern ? pat.c_str() : nullptr, patternLength, &error), &udat_close);
        if (!format) {
            StoreStatus(status, incoming, error, __func__);
            return nullptr;
        }
        const std::lock_guard lock(formatMutex);
        formats.insert(format.get());
        try {
            StoreStatus(status, incoming, error, __func__);
        } catch (...) {
            formats.erase(format.get());
            throw;
        }
        return format.release();
    } catch (const std::bad_alloc&) {
        StoreStatus(status, incoming, U_MEMORY_ALLOCATION_ERROR, __func__);
        return nullptr;
    }
}

std::int32_t APS5_VABI udat_toPattern_nid_postfix(const UDateFormat* format, std::int8_t localized, char16_t* result,
                                                 std::int32_t capacity, std::int32_t* status) {
    const char* caller = __func__;
    return FormatResult(result, capacity, status, caller, [&](UErrorCode& error) {
        if (!format) {
            error = U_ILLEGAL_ARGUMENT_ERROR;
            return -1;
        }
        RequireFormat(format, caller);
        return WriteResult(result, capacity, error, caller, [&](char16_t* out, int size, UErrorCode* ec) {
            return udat_toPattern(format, localized, out, size, ec);
        });
    });
}

}
