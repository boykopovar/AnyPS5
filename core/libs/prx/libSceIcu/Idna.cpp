#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

namespace {

struct InfoSize {
    std::int16_t size;
};

} // namespace

extern "C" {

void APS5_VABI uidna_close_nid_postfix(UIDNA* idna) {
    if (!idna) return;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI uidna_nameToASCII_nid_postfix(const UIDNA*, const char16_t* name, std::int32_t length, char16_t* dest,
                             std::int32_t capacity, UIDNAInfo* pInfo, std::int32_t* pErrorCode) {
    if (Icu::Failure(pErrorCode)) return 0;
    if (!pInfo || reinterpret_cast<const InfoSize*>(pInfo)->size < 16) {
        *pErrorCode = Icu::IllegalArgument;
        return 0;
    }
    if ((name == nullptr) ? length != 0 : length < -1) {
        *pErrorCode = Icu::IllegalArgument;
        return 0;
    }
    if ((dest == nullptr) ? capacity != 0 : capacity < 0) {
        *pErrorCode = Icu::IllegalArgument;
        return 0;
    }
    if (dest == name && name != nullptr) {
        *pErrorCode = Icu::IllegalArgument;
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI uidna_nameToUnicode_nid_postfix(const UIDNA*, const char16_t* name, std::int32_t length, char16_t* dest,
                             std::int32_t capacity, UIDNAInfo* pInfo, std::int32_t* pErrorCode) {
    if (Icu::Failure(pErrorCode)) return 0;
    if (!pInfo || reinterpret_cast<const InfoSize*>(pInfo)->size < 16) {
        *pErrorCode = Icu::IllegalArgument;
        return 0;
    }
    if ((name == nullptr) ? length != 0 : length < -1) {
        *pErrorCode = Icu::IllegalArgument;
        return 0;
    }
    if ((dest == nullptr) ? capacity != 0 : capacity < 0) {
        *pErrorCode = Icu::IllegalArgument;
        return 0;
    }
    if (dest == name && name != nullptr) {
        *pErrorCode = Icu::IllegalArgument;
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

UIDNA* APS5_VABI uidna_openUTS46_nid_postfix(std::uint32_t, std::int32_t* pErrorCode) {
    if (Icu::Failure(pErrorCode)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

}
