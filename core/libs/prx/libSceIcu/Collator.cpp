#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>

namespace {

constexpr std::int32_t UcolDefault = -1;

bool Failure(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    const int savedErrno = errno;
    std::int32_t value = 0;
    const bool ok = GuestCopyIn_nid_no_patch(&value, status, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
    return value > 0;
}

void WriteStatus(std::int32_t* status, std::int32_t value, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(status, &value, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

bool CollationKeyword(const char* keyword, const char* caller) {
    constexpr char expected[] = "collation";
    for (std::size_t index = 0; index < sizeof(expected); ++index) {
        char unit = 0;
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(&unit, keyword + index, sizeof(unit));
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(caller);
        if (unit != expected[index]) return false;
    }
    return true;
}

} // namespace

extern "C" {

void APS5_VABI ucol_close_nid_postfix(UCollator* coll) {
    if (!coll) return;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ucol_getAttribute_nid_postfix(const UCollator* coll, std::int32_t, std::int32_t* status) {
    if (Failure(status, __func__) || !coll) {
        return UcolDefault;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI ucol_getContractionsAndExpansions_nid_postfix(const UCollator* coll, USet*, USet*, std::int8_t, std::int32_t* status) {
    if (Failure(status, __func__)) return;
    if (!coll) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

UEnumeration* APS5_VABI ucol_getKeywordValues_nid_postfix(const char* keyword, std::int32_t* status) {
    if (Failure(status, __func__)) return nullptr;
    if (!keyword || !CollationKeyword(keyword, __func__)) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

UEnumeration* APS5_VABI ucol_getKeywordValuesForLocale_nid_postfix(const char*, const char*, std::int8_t, std::int32_t* status) {
    if (Failure(status, __func__)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ucol_getReorderCodes_nid_postfix(const UCollator*, std::int32_t*, std::int32_t, std::int32_t* status) {
    if (Failure(status, __func__)) {
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

const char16_t* APS5_VABI ucol_getRules_nid_postfix(const UCollator*, std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

USet* APS5_VABI ucol_getTailoredSet_nid_postfix(const UCollator*, std::int32_t* status) {
    if (Failure(status, __func__)) {
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

UCollator* APS5_VABI ucol_open_nid_postfix(const char*, std::int32_t* status) {
    if (Failure(status, __func__)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI ucol_setAttribute_nid_postfix(UCollator* coll, UColAttribute, UColAttributeValue, std::int32_t* status) {
    if (Failure(status, __func__) || coll == nullptr) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ucol_strcollIter_nid_postfix(const UCollator* coll, UCharIterator* sIter, UCharIterator* tIter, std::int32_t* status) {
    if (!status || Failure(status, __func__)) return 0;
    if (!sIter || !tIter || !coll) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
