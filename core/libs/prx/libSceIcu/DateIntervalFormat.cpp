#include "prx/libSceIcu/DateIntervalFormatBackend.hpp"
#include "prx/libSceIcu/FormattedValueBackend.hpp"
#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>

namespace {

std::mutex g_dateIntervalMutex;
std::unordered_set<const void*> g_openFormatters;
std::unordered_set<const void*> g_openResults;

void RegisterFormatter(const void* formatter) {
    if (!formatter) return;
    std::lock_guard lock(g_dateIntervalMutex);
    g_openFormatters.insert(formatter);
}

bool IsRegisteredFormatter(const void* formatter) {
    if (!formatter) return false;
    std::lock_guard lock(g_dateIntervalMutex);
    return g_openFormatters.contains(formatter);
}

bool UnregisterFormatter(const void* formatter) {
    if (!formatter) return false;
    std::lock_guard lock(g_dateIntervalMutex);
    return g_openFormatters.erase(formatter) > 0;
}

void RegisterResult(const void* result) {
    if (!result) return;
    std::lock_guard lock(g_dateIntervalMutex);
    g_openResults.insert(result);
}

bool IsRegisteredResult(const void* result) {
    if (!result) return false;
    std::lock_guard lock(g_dateIntervalMutex);
    return g_openResults.contains(result);
}

bool UnregisterResult(const void* result) {
    if (!result) return false;
    std::lock_guard lock(g_dateIntervalMutex);
    return g_openResults.erase(result) > 0;
}

std::int32_t ReadStatus(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    const int savedErrno = errno;
    std::int32_t value = 0;
    const bool ok = GuestCopyIn_nid_no_patch(&value, status, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
    return value;
}

void WriteStatus(std::int32_t* status, std::int32_t incoming, std::int32_t outgoing, const char* caller) {
    if (outgoing == incoming) return;
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(status, &outgoing, sizeof(outgoing));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

std::string ReadGuestCString(const char* source, const char* caller) {
    std::string text;
    for (;;) {
        char chunk[0x1000];
        const auto address = reinterpret_cast<std::uintptr_t>(source) + text.size();
        const std::size_t count = sizeof(chunk) - address % sizeof(chunk);
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(chunk, source + text.size(), count);
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(caller);
        const auto* end = static_cast<const char*>(std::memchr(chunk, 0, count));
        text.append(chunk, end ? static_cast<std::size_t>(end - chunk) : count);
        if (end) return text;
    }
}

std::u16string ReadGuestU16(const char16_t* source, std::int32_t length, const char* caller) {
    if (length >= 0) {
        std::u16string text(static_cast<std::size_t>(length), u'\0');
        if (length > 0) {
            const int savedErrno = errno;
            const bool ok = GuestCopyIn_nid_no_patch(
                text.data(), source, static_cast<std::size_t>(length) * sizeof(char16_t));
            errno = savedErrno;
            if (!ok) NotImplemented_nid_no_patch(caller);
        }
        return text;
    }
    std::u16string text;
    for (std::size_t index = 0;; ++index) {
        char16_t ch = 0;
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(&ch, source + index, sizeof(ch));
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(caller);
        if (ch == 0) return text;
        text.push_back(ch);
    }
}

}

extern "C" {

void APS5_VABI udtitvfmt_close_nid_postfix(UDateIntervalFormat* formatter) {
    if (!formatter) return;
    if (!UnregisterFormatter(formatter)) NotImplemented_nid_no_patch(__func__);
    Icu::DateIntervalFormatBackend::Close(formatter);
}

void APS5_VABI udtitvfmt_closeResult_nid_postfix(UFormattedDateInterval* result) {
    if (!result) return;
    if (!UnregisterResult(result)) NotImplemented_nid_no_patch(__func__);
    Icu::FormattedValueBackend::UnregisterOwner(result);
    Icu::DateIntervalFormatBackend::CloseResult(result);
}

void APS5_VABI udtitvfmt_formatCalendarToResult_nid_postfix(const UDateIntervalFormat*, UCalendar*, UCalendar*,
                                                            UFormattedDateInterval* result, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return;
    if (!result) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI udtitvfmt_formatToResult_nid_postfix(const UDateIntervalFormat* formatter, double fromDate,
                                                    double toDate, UFormattedDateInterval* result,
                                                    std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return;
    if (!result) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return;
    }
    if (!IsRegisteredResult(result)) NotImplemented_nid_no_patch(__func__);
    if (!formatter) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return;
    }
    if (!IsRegisteredFormatter(formatter)) NotImplemented_nid_no_patch(__func__);
    auto local = incoming;
    Icu::DateIntervalFormatBackend::FormatToResult(formatter, fromDate, toDate, result, &local);
    WriteStatus(status, incoming, local, __func__);
}

UDateIntervalFormat* APS5_VABI udtitvfmt_open_nid_postfix(const char* locale, const char16_t* skeleton,
                                                          std::int32_t skeletonLength, const char16_t* tzID,
                                                          std::int32_t tzIDLength, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    if ((!skeleton ? skeletonLength != 0 : skeletonLength < -1) || (!tzID ? tzIDLength != 0 : tzIDLength < -1)) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    std::string hostLocale;
    const char* localePtr = locale ? (hostLocale = ReadGuestCString(locale, __func__), hostLocale.c_str()) : uloc_getDefault();
    const std::u16string hostSkeleton =
        skeleton ? ReadGuestU16(skeleton, skeletonLength, __func__) : std::u16string{};
    const std::u16string hostTzID = tzID ? ReadGuestU16(tzID, tzIDLength, __func__) : std::u16string{};
    auto local = incoming;
    void* formatter = Icu::DateIntervalFormatBackend::Open(
        localePtr, skeleton ? hostSkeleton.c_str() : nullptr, static_cast<std::int32_t>(hostSkeleton.size()),
        tzID ? hostTzID.c_str() : nullptr, static_cast<std::int32_t>(hostTzID.size()), &local);
    WriteStatus(status, incoming, local, __func__);
    if (local <= 0 && formatter) RegisterFormatter(formatter);
    return static_cast<UDateIntervalFormat*>(formatter);
}

UFormattedDateInterval* APS5_VABI udtitvfmt_openResult_nid_postfix(std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    auto local = incoming;
    void* result = Icu::DateIntervalFormatBackend::OpenResult(&local);
    WriteStatus(status, incoming, local, __func__);
    if (local <= 0 && result) RegisterResult(result);
    return static_cast<UFormattedDateInterval*>(result);
}

const UFormattedValue* APS5_VABI udtitvfmt_resultAsValue_nid_postfix(const UFormattedDateInterval* result,
                                                                     std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    if (!result) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    if (!IsRegisteredResult(result)) NotImplemented_nid_no_patch(__func__);
    auto local = incoming;
    const UFormattedValue* value = Icu::DateIntervalFormatBackend::ResultAsValue(result, &local);
    WriteStatus(status, incoming, local, __func__);
    if (local <= 0 && value) Icu::FormattedValueBackend::Register(value, result);
    return value;
}

}
