#include "prx/libSceIcu/Api.hpp"
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

constexpr std::int32_t SearchDone = -1;
constexpr std::int32_t AttributeValueCount = 5;

std::int32_t ReadStatus(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    return CopyInPreservingErrno<std::int32_t>(status, caller);
}

}

extern "C" {

UCollator* APS5_VABI usearch_getCollator_nid_postfix(const UStringSearch* strsrch) {
    if (!strsrch) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI usearch_getMatchedLength_nid_postfix(const UStringSearch* strsrch) {
    if (!strsrch) return SearchDone;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI usearch_next_nid_postfix(UStringSearch* strsrch, std::int32_t* status) {
    if (ReadStatus(status, __func__) > 0 || !strsrch) return SearchDone;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI usearch_reset_nid_postfix(UStringSearch* strsrch) {
    if (!strsrch) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI usearch_setAttribute_nid_postfix(UStringSearch* strsrch, std::int32_t, std::int32_t value, std::int32_t* status) {
    if (ReadStatus(status, __func__) <= 0 && strsrch) NotImplemented_nid_no_patch(__func__);
    if (value == AttributeValueCount) CopyOutPreservingErrno(status, Icu::IllegalArgument, __func__);
}

void APS5_VABI usearch_setOffset_nid_postfix(UStringSearch* strsrch, std::int32_t, std::int32_t* status) {
    if (ReadStatus(status, __func__) > 0 || !strsrch) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI usearch_setPattern_nid_postfix(UStringSearch* strsrch, const char16_t* pattern, std::int32_t patternLength, std::int32_t* status) {
    if (ReadStatus(status, __func__) > 0) return;
    if (!strsrch || !pattern || (patternLength == -1 ? u_strlen_nid_postfix(pattern) : patternLength) == 0) {
        CopyOutPreservingErrno(status, Icu::IllegalArgument, __func__);
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI usearch_setText_nid_postfix(UStringSearch* strsrch, const char16_t* text, std::int32_t textLength, std::int32_t* status) {
    if (ReadStatus(status, __func__) > 0) return;
    if (!strsrch || !text || textLength < -1 || textLength == 0) {
        CopyOutPreservingErrno(status, Icu::IllegalArgument, __func__);
        return;
    }
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

}
