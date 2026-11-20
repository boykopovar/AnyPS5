#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>

namespace {

class PreservedErrno {
public:
    PreservedErrno() : saved(errno) {}
    ~PreservedErrno() { errno = saved; }

private:
    int saved;
};

template <typename T>
T CopyInPreservingErrno(const void* guest, const char* caller) {
    PreservedErrno guard;
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, guest, sizeof(value)))
        NotImplemented_nid_no_patch(caller);
    return value;
}

template <typename T>
void CopyOutPreservingErrno(void* guest, const T& value, const char* caller) {
    PreservedErrno guard;
    if (!GuestCopyOut_nid_no_patch(guest, &value, sizeof(value)))
        NotImplemented_nid_no_patch(caller);
}

constexpr std::int32_t Done = -1;
constexpr std::int32_t AttributeCount = 3;
constexpr std::int32_t AttributeValueCount = 5;

bool Succeeded(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    return CopyInPreservingErrno<std::int32_t>(status, caller) <= 0;
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
    if (CopyInPreservingErrno<std::int32_t>(status, __func__) > 0) return nullptr;
    if (locale == nullptr) {
        CopyOutPreservingErrno(status, Icu::IllegalArgument, __func__);
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
        CopyOutPreservingErrno(status, Icu::IllegalArgument, __func__);
    }
    if (value == AttributeValueCount) CopyOutPreservingErrno(status, Icu::IllegalArgument, __func__);
}

void APS5_VABI usearch_setOffset_nid_postfix(UStringSearch* search, std::int32_t, std::int32_t* status) {
    if (!Succeeded(status, __func__) || search == nullptr) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI usearch_setPattern_nid_postfix(UStringSearch* search, const char16_t* pattern, std::int32_t patternLength, std::int32_t* status) {
    if (!Succeeded(status, __func__)) return;
    if (search == nullptr || pattern == nullptr) {
        CopyOutPreservingErrno(status, Icu::IllegalArgument, __func__);
        return;
    }
    if (patternLength == -1 ? CopyInPreservingErrno<char16_t>(pattern, __func__) == 0 : patternLength == 0) {
        CopyOutPreservingErrno(status, Icu::IllegalArgument, __func__);
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI usearch_setText_nid_postfix(UStringSearch* search, const char16_t* text, std::int32_t textLength, std::int32_t* status) {
    if (!Succeeded(status, __func__)) return;
    if (search == nullptr || text == nullptr || textLength < -1 || textLength == 0) {
        CopyOutPreservingErrno(status, Icu::IllegalArgument, __func__);
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
