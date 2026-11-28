#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

namespace {

template <typename T>
T CopyIn(const void* guest, const char* caller) {
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, guest, sizeof(value)))
        NotImplemented_nid_no_patch(caller);
    return value;
}

template <typename T>
void CopyOut(void* guest, const T& value, const char* caller) {
    if (!GuestCopyOut_nid_no_patch(guest, &value, sizeof(value)))
        NotImplemented_nid_no_patch(caller);
}

constexpr std::int32_t Done = -1;
constexpr std::int32_t AttributeCount = 3;
constexpr std::int32_t AttributeValueCount = 5;

bool Succeeded(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    return CopyIn<std::int32_t>(status, caller) <= 0;
}

}

extern "C" {

UCollator* APS5_VABI usearch_getCollator_nid_postfix(const UStringSearch* search) {
    if (search == nullptr) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI usearch_getMatchedLength_nid_postfix(const UStringSearch* search) {
    if (search == nullptr) return Done;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI usearch_next_nid_postfix(UStringSearch* search, std::int32_t* status) {
    if (!Succeeded(status, __func__) || search == nullptr) return Done;
    NotImplemented_nid_no_patch(__func__);
}

UStringSearch* APS5_VABI usearch_open_nid_postfix(const char16_t*, std::int32_t, const char16_t*, std::int32_t, const char* locale, UBreakIterator*, std::int32_t* status) {
    Icu::RequireStatus(status);
    if (CopyIn<std::int32_t>(status, __func__) > 0) return nullptr;
    if (locale == nullptr) {
        CopyOut(status, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI usearch_reset_nid_postfix(UStringSearch* search) {
    if (search == nullptr) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI usearch_setAttribute_nid_postfix(UStringSearch* search, std::int32_t attribute, std::int32_t value, std::int32_t* status) {
    if (Succeeded(status, __func__) && search != nullptr) {
        if (attribute >= 0 && attribute < AttributeCount) NotImplemented_nid_no_patch(__func__);
        CopyOut(status, Icu::IllegalArgument, __func__);
    }
    if (value == AttributeValueCount) CopyOut(status, Icu::IllegalArgument, __func__);
}

void APS5_VABI usearch_setOffset_nid_postfix(UStringSearch* search, std::int32_t, std::int32_t* status) {
    if (!Succeeded(status, __func__) || search == nullptr) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI usearch_setPattern_nid_postfix(UStringSearch* search, const char16_t* pattern, std::int32_t patternLength, std::int32_t* status) {
    if (!Succeeded(status, __func__)) return;
    if (search == nullptr || pattern == nullptr) {
        CopyOut(status, Icu::IllegalArgument, __func__);
        return;
    }
    if (patternLength == -1 ? CopyIn<char16_t>(pattern, __func__) == 0 : patternLength == 0) {
        CopyOut(status, Icu::IllegalArgument, __func__);
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI usearch_setText_nid_postfix(UStringSearch* search, const char16_t* text, std::int32_t textLength, std::int32_t* status) {
    if (!Succeeded(status, __func__)) return;
    if (search == nullptr || text == nullptr || textLength < -1 || textLength == 0) {
        CopyOut(status, Icu::IllegalArgument, __func__);
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
