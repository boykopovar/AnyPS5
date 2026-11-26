#include "prx/libSceIcu/LocaleDisplayNamesBackend.hpp"
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

std::mutex g_displayNamesMutex;
std::unordered_set<const void*> g_openDisplayNames;

void RegisterDisplayNames(const void* names) {
    if (!names) return;
    std::lock_guard lock(g_displayNamesMutex);
    g_openDisplayNames.insert(names);
}

bool IsRegisteredDisplayNames(const void* names) {
    if (!names) return false;
    std::lock_guard lock(g_displayNamesMutex);
    return g_openDisplayNames.contains(names);
}

bool UnregisterDisplayNames(const void* names) {
    if (!names) return false;
    std::lock_guard lock(g_displayNamesMutex);
    return g_openDisplayNames.erase(names) > 0;
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

void WriteGuestU16Result(char16_t* result, std::int32_t maxResultSize, const std::vector<char16_t>& buffer,
                         std::int32_t length, std::int32_t outgoing, const char* caller) {
    if (maxResultSize > 0 && length >= 0 && outgoing != Icu::IllegalArgument) {
        const std::size_t count = (std::min)(static_cast<std::size_t>(maxResultSize),
                                             static_cast<std::size_t>(length) + (length < maxResultSize ? 1u : 0u));
        if (count > 0) {
            const int savedErrno = errno;
            const bool ok = GuestCopyOut_nid_no_patch(result, buffer.data(), count * sizeof(char16_t));
            errno = savedErrno;
            if (!ok) NotImplemented_nid_no_patch(caller);
        }
    }
}

template<typename TCall>
std::int32_t RunDisplayName(const ULocaleDisplayNames* names, bool validArgs, char16_t* result,
                            std::int32_t maxResultSize, std::int32_t* status, const char* caller, TCall call) {
    const auto incoming = ReadStatus(status, caller);
    if (incoming > 0) return 0;
    if (!names || !validArgs || (!result && maxResultSize > 0) || maxResultSize < 0) {
        WriteStatus(status, incoming, Icu::IllegalArgument, caller);
        return 0;
    }
    if (!IsRegisteredDisplayNames(names)) throw std::invalid_argument(std::string(caller) + ": invalid handle");
    std::vector<char16_t> buffer(maxResultSize > 0 ? static_cast<std::size_t>(maxResultSize) : 0u);
    auto local = incoming;
    const std::int32_t length = call(maxResultSize > 0 ? buffer.data() : nullptr, &local);
    WriteGuestU16Result(result, maxResultSize, buffer, length, local, caller);
    WriteStatus(status, incoming, local, caller);
    return length;
}

}

extern "C" {

void APS5_VABI uldn_close_nid_postfix(ULocaleDisplayNames* names) {
    if (!names) return;
    if (!UnregisterDisplayNames(names)) NotImplemented_nid_no_patch(__func__);
    Icu::LocaleDisplayNamesBackend::Close(names);
}

std::int32_t APS5_VABI uldn_keyValueDisplayName_nid_postfix(const ULocaleDisplayNames* names, const char* key,
                                                             const char* value, char16_t* result,
                                                             std::int32_t maxResultSize, std::int32_t* status) {
    return RunDisplayName(names, key && value, result, maxResultSize, status, __func__,
                          [&](char16_t* out, std::int32_t* local) {
                              const std::string hostKey = ReadGuestCString(key, __func__);
                              const std::string hostValue = ReadGuestCString(value, __func__);
                              return Icu::LocaleDisplayNamesBackend::KeyValueDisplayName(
                                  names, hostKey.c_str(), hostValue.c_str(), out, maxResultSize, local);
                          });
}

std::int32_t APS5_VABI uldn_localeDisplayName_nid_postfix(const ULocaleDisplayNames* names, const char* locale,
                                                            char16_t* result, std::int32_t maxResultSize,
                                                            std::int32_t* status) {
    return RunDisplayName(names, locale != nullptr, result, maxResultSize, status, __func__,
                          [&](char16_t* out, std::int32_t* local) {
                              const std::string hostLocale = ReadGuestCString(locale, __func__);
                              return Icu::LocaleDisplayNamesBackend::LocaleDisplayName(
                                  names, hostLocale.c_str(), out, maxResultSize, local);
                          });
}

ULocaleDisplayNames* APS5_VABI uldn_openForContext_nid_postfix(const char* locale, std::int32_t* contexts,
                                                               std::int32_t length, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    if (length < 0 || (length > 0 && !contexts)) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    std::string hostLocale;
    const char* localePtr = locale ? (hostLocale = ReadGuestCString(locale, __func__), hostLocale.c_str()) : nullptr;
    std::vector<std::int32_t> hostContexts;
    if (contexts && length > 0) {
        hostContexts.resize(static_cast<std::size_t>(length));
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(
            hostContexts.data(), contexts, static_cast<std::size_t>(length) * sizeof(std::int32_t));
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(__func__);
    }
    auto local = incoming;
    void* names = Icu::LocaleDisplayNamesBackend::OpenForContext(
        localePtr, hostContexts.empty() ? nullptr : hostContexts.data(), length, &local);
    WriteStatus(status, incoming, local, __func__);
    if (local <= 0 && names) RegisterDisplayNames(names);
    return static_cast<ULocaleDisplayNames*>(names);
}

std::int32_t APS5_VABI uldn_regionDisplayName_nid_postfix(const ULocaleDisplayNames* names, const char* region,
                                                            char16_t* result, std::int32_t maxResultSize,
                                                            std::int32_t* status) {
    return RunDisplayName(names, region != nullptr, result, maxResultSize, status, __func__,
                          [&](char16_t* out, std::int32_t* local) {
                              const std::string hostRegion = ReadGuestCString(region, __func__);
                              return Icu::LocaleDisplayNamesBackend::RegionDisplayName(
                                  names, hostRegion.c_str(), out, maxResultSize, local);
                          });
}

std::int32_t APS5_VABI uldn_scriptDisplayName_nid_postfix(const ULocaleDisplayNames* names, const char* script,
                                                            char16_t* result, std::int32_t maxResultSize,
                                                            std::int32_t* status) {
    return RunDisplayName(names, script != nullptr, result, maxResultSize, status, __func__,
                          [&](char16_t* out, std::int32_t* local) {
                              const std::string hostScript = ReadGuestCString(script, __func__);
                              return Icu::LocaleDisplayNamesBackend::ScriptDisplayName(
                                  names, hostScript.c_str(), out, maxResultSize, local);
                          });
}

}
