#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>

namespace {

bool Failure(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    std::int32_t value = 0;
    const bool ok = GuestCopyIn_nid_no_patch(&value, status, sizeof(value));
    if (!ok) NotImplemented_nid_no_patch(caller);
    return value > 0;
}

void WriteStatus(std::int32_t* status, std::int32_t value, const char* caller) {
    const bool ok = GuestCopyOut_nid_no_patch(status, &value, sizeof(value));
    if (!ok) NotImplemented_nid_no_patch(caller);
}

std::int32_t DisplayName(const ULocaleDisplayNames* names, bool codes, const char16_t* result, std::int32_t maxResultSize, std::int32_t* status, const char* caller) {
    if (Failure(status, caller)) return 0;
    if (!names || !codes || (!result && maxResultSize > 0) || maxResultSize < 0) {
        WriteStatus(status, Icu::IllegalArgument, caller);
        return 0;
    }
    NotImplemented_nid_no_patch(caller);
}

}

extern "C" {

void APS5_VABI uldn_close_nid_postfix(ULocaleDisplayNames* names) {
    if (!names) return;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI uldn_keyValueDisplayName_nid_postfix(const ULocaleDisplayNames* names, const char* key, const char* value, char16_t* result, std::int32_t maxResultSize, std::int32_t* status) {
    return DisplayName(names, key && value, result, maxResultSize, status, __func__);
}

std::int32_t APS5_VABI uldn_localeDisplayName_nid_postfix(const ULocaleDisplayNames* names, const char* locale, char16_t* result, std::int32_t maxResultSize, std::int32_t* status) {
    return DisplayName(names, locale != nullptr, result, maxResultSize, status, __func__);
}

ULocaleDisplayNames* APS5_VABI uldn_openForContext_nid_postfix(const char*, std::int32_t*, std::int32_t, std::int32_t* status) {
    if (Failure(status, __func__)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI uldn_regionDisplayName_nid_postfix(const ULocaleDisplayNames* names, const char* region, char16_t* result, std::int32_t maxResultSize, std::int32_t* status) {
    return DisplayName(names, region != nullptr, result, maxResultSize, status, __func__);
}

std::int32_t APS5_VABI uldn_scriptDisplayName_nid_postfix(const ULocaleDisplayNames* names, const char* script, char16_t* result, std::int32_t maxResultSize, std::int32_t* status) {
    return DisplayName(names, script != nullptr, result, maxResultSize, status, __func__);
}

}
