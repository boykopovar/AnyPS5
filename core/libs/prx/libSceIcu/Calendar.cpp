#include "prx/libSceIcu/CalendarBackend.hpp"
#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

constexpr std::int32_t AttributeCount = 5;

std::mutex g_calendarsMutex;
std::unordered_set<const void*> g_openCalendars;

void RegisterCalendar(const void* calendar) {
    if (!calendar) return;
    std::lock_guard lock(g_calendarsMutex);
    g_openCalendars.insert(calendar);
}

bool IsRegisteredCalendar(const void* calendar) {
    if (!calendar) return false;
    std::lock_guard lock(g_calendarsMutex);
    return g_openCalendars.contains(calendar);
}

bool UnregisterCalendar(const void* calendar) {
    if (!calendar) return false;
    std::lock_guard lock(g_calendarsMutex);
    return g_openCalendars.erase(calendar) > 0;
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

std::u16string ReadGuestU16String(const char16_t* source, std::int32_t length, const char* caller) {
    std::u16string text;
    if (length >= 0) {
        text.resize(static_cast<std::size_t>(length));
        if (length > 0) {
            const int savedErrno = errno;
            const bool ok = GuestCopyIn_nid_no_patch(text.data(), source, static_cast<std::size_t>(length) * sizeof(char16_t));
            errno = savedErrno;
            if (!ok) NotImplemented_nid_no_patch(caller);
        }
        return text;
    }
    for (;;) {
        char16_t ch = 0;
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(&ch, source + text.size(), sizeof(ch));
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(caller);
        if (ch == 0) return text;
        text.push_back(ch);
    }
}

void WriteGuestU16Output(char16_t* destination, std::int32_t capacity, const std::vector<char16_t>& source,
                         std::int32_t length, std::int32_t outgoing, std::int32_t* status, const char* caller) {
    if (capacity > 0 && length >= 0 && outgoing != Icu::IllegalArgument) {
        const std::size_t requested = static_cast<std::size_t>(length) + (length < capacity ? 1u : 0u);
        const std::size_t count = (std::min)(source.size(), (std::min)(static_cast<std::size_t>(capacity), requested));
        if (count > 0) {
            const int savedErrno = errno;
            const bool ok = GuestCopyOut_nid_no_patch(destination, source.data(), count * sizeof(char16_t));
            errno = savedErrno;
            if (!ok) NotImplemented_nid_no_patch(caller);
        }
    }
    (void)status;
}

}

extern "C" {

UCalendar* APS5_VABI ucal_clone_nid_postfix(const UCalendar* cal, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    if (!cal || !IsRegisteredCalendar(cal)) throw std::invalid_argument("ucal_clone: invalid calendar");
    auto local = incoming;
    void* cloned = Icu::CalendarBackend::Clone(cal, &local);
    WriteStatus(status, incoming, local, __func__);
    if (local <= 0 && cloned) RegisterCalendar(cloned);
    return static_cast<UCalendar*>(cloned);
}

void APS5_VABI ucal_close_nid_postfix(UCalendar* cal) {
    if (!cal) return;
    if (!UnregisterCalendar(cal)) NotImplemented_nid_no_patch(__func__);
    Icu::CalendarBackend::Close(cal);
}

std::int32_t APS5_VABI ucal_get_nid_postfix(const UCalendar* cal, UCalendarDateFields field, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return -1;
    if (!cal || !IsRegisteredCalendar(cal)) throw std::invalid_argument("ucal_get: invalid calendar");
    auto local = incoming;
    const auto result = Icu::CalendarBackend::Get(cal, field, &local);
    WriteStatus(status, incoming, local, __func__);
    return result;
}

std::int32_t APS5_VABI ucal_getAttribute_nid_postfix(const UCalendar* cal, std::int32_t attr) {
    if (attr < 0 || attr >= AttributeCount) return -1;
    if (!cal) return -1;
    if (!IsRegisteredCalendar(cal)) NotImplemented_nid_no_patch(__func__);
    return Icu::CalendarBackend::GetAttribute(cal, attr);
}

std::int32_t APS5_VABI ucal_getCanonicalTimeZoneID_nid_postfix(
    const char16_t* id,
    std::int32_t len,
    char16_t* result,
    std::int32_t resultCapacity,
    std::int8_t* isSystemID,
    std::int32_t* status) {
    if (!status) return 0;
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return 0;
    if (isSystemID) {
        const std::int8_t zero = 0;
        const int savedErrno = errno;
        const bool ok = GuestCopyOut_nid_no_patch(isSystemID, &zero, sizeof(zero));
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(__func__);
    }
    if (!id || len == 0 || !result || resultCapacity <= 0) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return 0;
    }
    const std::u16string hostId = ReadGuestU16String(id, len, __func__);
    std::vector<char16_t> buffer(resultCapacity > 0 ? static_cast<std::size_t>(resultCapacity) : 0u);
    auto local = incoming;
    std::int8_t system = 0;
    const std::int32_t length = Icu::CalendarBackend::GetCanonicalTimeZoneID(
        hostId.c_str(), static_cast<std::int32_t>(hostId.size()),
        resultCapacity > 0 ? buffer.data() : nullptr, resultCapacity, isSystemID ? &system : nullptr, &local);
    if (isSystemID) {
        const int savedErrno = errno;
        const bool ok = GuestCopyOut_nid_no_patch(isSystemID, &system, sizeof(system));
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(__func__);
    }
    WriteGuestU16Output(result, resultCapacity, buffer, length, local, status, __func__);
    WriteStatus(status, incoming, local, __func__);
    return length;
}

std::int32_t APS5_VABI ucal_getDayOfWeekType_nid_postfix(const UCalendar* cal, std::int32_t dayOfWeek, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return 0;
    if (!cal || !IsRegisteredCalendar(cal)) throw std::invalid_argument("ucal_getDayOfWeekType: invalid calendar");
    auto local = incoming;
    const auto result = Icu::CalendarBackend::GetDayOfWeekType(cal, dayOfWeek, &local);
    WriteStatus(status, incoming, local, __func__);
    return result;
}

std::int32_t APS5_VABI ucal_getDefaultTimeZone_nid_postfix(char16_t* result, std::int32_t resultCapacity, std::int32_t* ec) {
    if (!ec) return 0;
    const auto incoming = ReadStatus(ec, __func__);
    if (incoming > 0) return 0;
    if (result == nullptr ? resultCapacity != 0 : resultCapacity < 0) {
        WriteStatus(ec, incoming, Icu::IllegalArgument, __func__);
        return 0;
    }
    std::vector<char16_t> buffer(resultCapacity > 0 ? static_cast<std::size_t>(resultCapacity) : 0u);
    auto local = incoming;
    const std::int32_t length = Icu::CalendarBackend::GetDefaultTimeZone(
        resultCapacity > 0 ? buffer.data() : nullptr, resultCapacity, &local);
    WriteGuestU16Output(result, resultCapacity, buffer, length, local, ec, __func__);
    WriteStatus(ec, incoming, local, __func__);
    return length;
}

UEnumeration* APS5_VABI ucal_getKeywordValuesForLocale_nid_postfix(const char* key, const char* locale,
                                                                   std::int8_t commonlyUsed, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    std::string hostKey;
    std::string hostLocale;
    const char* keyPtr = key ? (hostKey = ReadGuestCString(key, __func__), hostKey.c_str()) : nullptr;
    const char* localePtr = locale ? (hostLocale = ReadGuestCString(locale, __func__), hostLocale.c_str()) : nullptr;
    auto local = incoming;
    UEnumeration* result = Icu::CalendarBackend::GetKeywordValuesForLocale(keyPtr, localePtr, commonlyUsed, &local);
    WriteStatus(status, incoming, local, __func__);
    return result;
}

std::int32_t APS5_VABI ucal_getTimeZoneDisplayName_nid_postfix(const void* calendar, std::int32_t displayNameType,
                                                               const char* locale, char16_t* result,
                                                               std::int32_t resultLength, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return -1;
    if (!calendar || !IsRegisteredCalendar(calendar)) throw std::invalid_argument("ucal_getTimeZoneDisplayName: invalid calendar");
    if (result == nullptr ? resultLength != 0 : resultLength < 0) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return -1;
    }
    std::string hostLocale;
    const char* localePtr = locale ? (hostLocale = ReadGuestCString(locale, __func__), hostLocale.c_str()) : uloc_getDefault();
    std::vector<char16_t> buffer(resultLength > 0 ? static_cast<std::size_t>(resultLength) : 0u);
    auto local = incoming;
    const std::int32_t length = Icu::CalendarBackend::GetTimeZoneDisplayName(
        calendar, displayNameType, localePtr, resultLength > 0 ? buffer.data() : nullptr, resultLength, &local);
    WriteGuestU16Output(result, resultLength, buffer, length, local, status, __func__);
    WriteStatus(status, incoming, local, __func__);
    return length;
}

UCalendar* APS5_VABI ucal_open_nid_postfix(const char16_t* zoneId, std::int32_t length, const char* locale,
                                           std::int32_t calendarType, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    std::u16string hostZone;
    const char16_t* zonePtr = nullptr;
    std::int32_t zoneLen = length;
    if (zoneId != nullptr) {
        hostZone = ReadGuestU16String(zoneId, length, __func__);
        zonePtr = hostZone.c_str();
        zoneLen = static_cast<std::int32_t>(hostZone.size());
    }
    std::string hostLocale;
    const char* localePtr = locale ? (hostLocale = ReadGuestCString(locale, __func__), hostLocale.c_str()) : uloc_getDefault();
    auto local = incoming;
    void* calendar = Icu::CalendarBackend::Open(zonePtr, zoneLen, localePtr, calendarType, &local);
    WriteStatus(status, incoming, local, __func__);
    if (local <= 0 && calendar) RegisterCalendar(calendar);
    return static_cast<UCalendar*>(calendar);
}

UEnumeration* APS5_VABI ucal_openTimeZoneIDEnumeration_nid_postfix(std::int32_t zoneType, const char* region,
                                                                   const std::int32_t* rawOffset, std::int32_t* ec) {
    const auto incoming = ReadStatus(ec, __func__);
    if (incoming > 0) return nullptr;
    std::string hostRegion;
    const char* regionPtr = region ? (hostRegion = ReadGuestCString(region, __func__), hostRegion.c_str()) : nullptr;
    std::int32_t offsetValue = 0;
    const std::int32_t* offsetPtr = nullptr;
    if (rawOffset != nullptr) {
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(&offsetValue, rawOffset, sizeof(offsetValue));
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(__func__);
        offsetPtr = &offsetValue;
    }
    return Icu::CalendarBackend::OpenTimeZoneIDEnumeration(zoneType, regionPtr, offsetPtr, ec);
}

UEnumeration* APS5_VABI ucal_openTimeZones_nid_postfix(std::int32_t* ec) {
    return Icu::CalendarBackend::OpenTimeZones(ec);
}

void APS5_VABI ucal_setGregorianChange_nid_postfix(UCalendar* cal, UDate date, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return;
    if (!cal) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return;
    }
    if (!IsRegisteredCalendar(cal)) NotImplemented_nid_no_patch(__func__);
    auto local = incoming;
    Icu::CalendarBackend::SetGregorianChange(cal, date, &local);
    WriteStatus(status, incoming, local, __func__);
}

void APS5_VABI ucal_setMillis_nid_postfix(UCalendar* cal, UDate dateTime, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return;
    if (!cal || !IsRegisteredCalendar(cal)) throw std::invalid_argument("ucal_setMillis: invalid calendar");
    auto local = incoming;
    Icu::CalendarBackend::SetMillis(cal, dateTime, &local);
    WriteStatus(status, incoming, local, __func__);
}

}
