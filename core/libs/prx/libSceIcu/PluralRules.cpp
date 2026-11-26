#include "prx/libSceIcu/PluralRulesBackend.hpp"
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

constexpr std::int32_t PluralTypeCount = 2;

std::mutex g_pluralRulesMutex;
std::unordered_set<const void*> g_openPluralRules;

void RegisterPluralRules(const void* rules) {
    if (!rules) return;
    std::lock_guard lock(g_pluralRulesMutex);
    g_openPluralRules.insert(rules);
}

bool IsRegisteredPluralRules(const void* rules) {
    if (!rules) return false;
    std::lock_guard lock(g_pluralRulesMutex);
    return g_openPluralRules.contains(rules);
}

bool UnregisterPluralRules(const void* rules) {
    if (!rules) return false;
    std::lock_guard lock(g_pluralRulesMutex);
    return g_openPluralRules.erase(rules) > 0;
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

}

extern "C" {

void APS5_VABI uplrules_close_nid_postfix(UPluralRules* uplrules) {
    if (!uplrules) {
        return;
    }
    if (!UnregisterPluralRules(uplrules)) NotImplemented_nid_no_patch(__func__);
    Icu::PluralRulesBackend::Close(uplrules);
}

UEnumeration* APS5_VABI uplrules_getKeywords_nid_postfix(const UPluralRules* uplrules, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    if (!uplrules) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    if (!IsRegisteredPluralRules(uplrules)) NotImplemented_nid_no_patch(__func__);
    return Icu::PluralRulesBackend::GetKeywords(uplrules, status);
}

UPluralRules* APS5_VABI uplrules_openForType_nid_postfix(const char* locale, std::int32_t type, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    if (type >= PluralTypeCount) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    std::string hostLocale;
    const char* localePtr = locale ? (hostLocale = ReadGuestCString(locale, __func__), hostLocale.c_str()) : nullptr;
    auto local = incoming;
    void* rules = Icu::PluralRulesBackend::OpenForType(localePtr, type, &local);
    WriteStatus(status, incoming, local, __func__);
    if (rules) RegisterPluralRules(rules);
    return static_cast<UPluralRules*>(rules);
}

std::int32_t APS5_VABI uplrules_selectWithFormat_nid_postfix(const UPluralRules* uplrules, double, const UNumberFormat* fmt, char16_t* keyword, std::int32_t capacity, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return 0;
    if (!uplrules || !fmt || (keyword == nullptr ? capacity != 0 : capacity < 0)) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
