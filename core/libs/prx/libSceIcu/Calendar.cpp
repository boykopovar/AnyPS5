#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/CalendarBackend.hpp"
#include "prx/libSceIcu/Enumeration.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using Native = std::unique_ptr<void, decltype(&IcuCalendarBackend::Close_nid_no_patch)>;
struct Entry {
    Native native;
    std::mutex mutex;
    explicit Entry(Native value) : native(std::move(value)) {}
};
std::mutex RegistryMutex;
std::unordered_map<const UCalendar*, std::shared_ptr<Entry>> Handles;
std::vector<std::unique_ptr<UEnumeration>> Tokens;

std::int32_t ReadStatus(const std::int32_t* pointer) {
    std::int32_t value;
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(value))) NotImplemented_nid_no_patch(__func__);
    return value;
}
void WriteStatus(std::int32_t* pointer, std::int32_t before, std::int32_t after) {
    if (before != after && !GuestCopyOut_nid_no_patch(pointer, &after, sizeof(after))) NotImplemented_nid_no_patch(__func__);
}
std::shared_ptr<Entry> Find(const UCalendar* handle) {
    const std::lock_guard lock(RegistryMutex);
    const auto found = Handles.find(handle);
    if (found == Handles.end()) NotImplemented_nid_no_patch(__func__);
    return found->second;
}
void APS5_VABI RefuseClose_nid_no_patch(UEnumeration*) { NotImplemented_nid_no_patch(__func__); }
std::int32_t APS5_VABI RefuseCount_nid_no_patch(UEnumeration*, std::int32_t*) { NotImplemented_nid_no_patch(__func__); }
const char* APS5_VABI RefuseNext_nid_no_patch(UEnumeration*, std::int32_t*, std::int32_t*) { NotImplemented_nid_no_patch(__func__); }
const char16_t* APS5_VABI RefuseUNext_nid_no_patch(UEnumeration*, std::int32_t*, std::int32_t*) { NotImplemented_nid_no_patch(__func__); }
void APS5_VABI RefuseReset_nid_no_patch(UEnumeration*, std::int32_t*) { NotImplemented_nid_no_patch(__func__); }

UCalendar* Publish(Native native, std::int32_t* error, std::int32_t before, std::int32_t status) {
    if (!native) {
        WriteStatus(error, before, status);
        return nullptr;
    }
    auto owner = std::make_shared<Entry>(std::move(native));
    auto token = std::make_unique<UEnumeration>(UEnumeration{nullptr, nullptr, RefuseClose_nid_no_patch,
        RefuseCount_nid_no_patch, RefuseUNext_nid_no_patch, RefuseNext_nid_no_patch, RefuseReset_nid_no_patch});
    auto* handle = reinterpret_cast<UCalendar*>(token.get());
    WriteStatus(error, before, status);
    const std::lock_guard lock(RegistryMutex);
    Tokens.push_back(std::move(token));
    Handles.emplace(handle, std::move(owner));
    return handle;
}

std::string ReadLocale(const char* locale) {
    if (!locale) NotImplemented_nid_no_patch(__func__);
    auto address = reinterpret_cast<std::uintptr_t>(locale);
    std::string result;
    for (;;) {
        char value;
        if (!GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const void*>(address), sizeof(value))) NotImplemented_nid_no_patch(__func__);
        if (!value) return result;
        if (address == UINTPTR_MAX) APS5_INVALID_ARG_EX;
        result.push_back(value);
        ++address;
    }
}

std::u16string ReadZone(const char16_t* zone, std::int32_t length) {
    if (!zone) NotImplemented_nid_no_patch(__func__);
    auto address = reinterpret_cast<std::uintptr_t>(zone);
    std::u16string result;
    if (length >= 0) {
        std::array<char16_t, 256> chunk;
        auto remaining = static_cast<std::size_t>(length);
        while (remaining) {
            const auto count = std::min(remaining, chunk.size());
            const auto bytes = count * sizeof(char16_t);
            if (address > UINTPTR_MAX - bytes ||
                !GuestCopyIn_nid_no_patch(chunk.data(), reinterpret_cast<const void*>(address), bytes)) NotImplemented_nid_no_patch(__func__);
            result.append(chunk.data(), count);
            remaining -= count;
            address += bytes;
        }
        return result;
    }
    for (;;) {
        char16_t value;
        if (!GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const void*>(address), sizeof(value))) NotImplemented_nid_no_patch(__func__);
        if (!value) return result;
        if (result.size() == static_cast<std::size_t>(INT32_MAX) || address > UINTPTR_MAX - sizeof(value)) APS5_INVALID_ARG_EX;
        result.push_back(value);
        address += sizeof(value);
    }
}

void Set(UCalendar* handle, double time, bool cutover, std::int32_t* error) {
    const auto before = ReadStatus(error);
    if (before > 0) return;
    if (cutover && !handle) {
        WriteStatus(error, before, 1);
        return;
    }
    const auto owner = Find(handle);
    const std::lock_guard lock(owner->mutex);
    auto status = before;
    IcuCalendarBackend::Set_nid_no_patch(owner->native.get(), time, cutover, &status);
    WriteStatus(error, before, status);
}

} // namespace

extern "C" {

UCalendar* APS5_VABI ucal_open_nid_postfix(const char16_t* zone, std::int32_t length, const char* locale,
                                        std::int32_t type, std::int32_t* error) {
    const auto before = ReadStatus(error);
    if (before > 0) return nullptr;
    try {
        const auto zoneInput = ReadZone(zone, length);
        const auto localeInput = ReadLocale(locale);
        auto status = before;
        Native native(IcuCalendarBackend::Open_nid_no_patch(zoneInput.data(), static_cast<std::int32_t>(zoneInput.size()),
            localeInput.c_str(), type, &status), IcuCalendarBackend::Close_nid_no_patch);
        return Publish(std::move(native), error, before, status);
    } catch (const std::bad_alloc&) {
        WriteStatus(error, before, 7);
        return nullptr;
    }
}

UCalendar* APS5_VABI ucal_clone_nid_postfix(const UCalendar* handle, std::int32_t* error) {
    const auto before = ReadStatus(error);
    if (before > 0) return nullptr;
    try {
        const auto owner = Find(handle);
        const std::lock_guard lock(owner->mutex);
        auto status = before;
        Native native(IcuCalendarBackend::Clone_nid_no_patch(owner->native.get(), &status), IcuCalendarBackend::Close_nid_no_patch);
        return Publish(std::move(native), error, before, status);
    } catch (const std::bad_alloc&) {
        WriteStatus(error, before, 7);
        return nullptr;
    }
}

void APS5_VABI ucal_close_nid_postfix(UCalendar* handle) {
    if (!handle) return;
    std::shared_ptr<Entry> closing;
    {
        const std::lock_guard lock(RegistryMutex);
        const auto found = Handles.find(handle);
        if (found == Handles.end()) NotImplemented_nid_no_patch(__func__);
        closing = std::move(found->second);
        Handles.erase(found);
    }
}

std::int32_t APS5_VABI ucal_get_nid_postfix(const UCalendar* handle, UCalendarDateFields field, std::int32_t* error) {
    const auto before = ReadStatus(error);
    if (before > 0) return -1;
    if (field < 0 || field >= 24) {
        WriteStatus(error, before, 1);
        return -1;
    }
    const auto owner = Find(handle);
    const std::lock_guard lock(owner->mutex);
    auto status = before;
    const auto result = IcuCalendarBackend::Get_nid_no_patch(owner->native.get(), field, &status);
    WriteStatus(error, before, status);
    return result;
}

void APS5_VABI ucal_setMillis_nid_postfix(UCalendar* handle, UDate time, std::int32_t* status) {
    Set(handle, time, false, status);
}

void APS5_VABI ucal_setGregorianChange_nid_postfix(UCalendar* handle, UDate time, std::int32_t* status) {
    Set(handle, time, true, status);
}

std::int32_t APS5_VABI ucal_getCanonicalTimeZoneID_nid_postfix(
    const char16_t* id,
    std::int32_t len,
    char16_t* result,
    std::int32_t resultCapacity,
    std::int8_t* isSystemID,
    std::int32_t* status) {
    if (!status || *status > 0) {
        return 0;
    }
    if (isSystemID) {
        *isSystemID = 0;
    }
    if (!id || len == 0 || !result || resultCapacity <= 0) {
        *status = Icu::IllegalArgument;
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ucal_getDefaultTimeZone_nid_postfix(char16_t*, std::int32_t, std::int32_t* ec) {
    if (!ec || *ec > 0) return 0;
    NotImplemented_nid_no_patch(__func__);
}

UEnumeration* APS5_VABI ucal_getKeywordValuesForLocale_nid_postfix(const char*, const char*, std::int8_t, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ucal_getTimeZoneDisplayName_nid_postfix(const void*, std::int32_t, const char*, char16_t*,
                                                               std::int32_t, std::int32_t* status) {
    if (Icu::Failure(status)) return -1;
    NotImplemented_nid_no_patch(__func__);
}

}
