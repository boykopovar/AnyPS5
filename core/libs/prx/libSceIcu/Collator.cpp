#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

namespace {

constexpr std::int32_t UcolDefault = -1;

} // namespace

extern "C" {

void APS5_VABI ucol_close_nid_postfix(UCollator* coll) {
    if (!coll) return;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ucol_getAttribute_nid_postfix(const UCollator* coll, std::int32_t, std::int32_t* status) {
    if (Icu::Failure(status) || !coll) {
        return UcolDefault;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ucol_getReorderCodes_nid_postfix(const UCollator*, std::int32_t*, std::int32_t, std::int32_t* status) {
    if (Icu::Failure(status)) {
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

USet* APS5_VABI ucol_getTailoredSet_nid_postfix(const UCollator*, std::int32_t* status) {
    if (Icu::Failure(status)) {
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

UCollator* APS5_VABI ucol_open_nid_postfix(const char*, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI ucol_setAttribute_nid_postfix(UCollator* coll, UColAttribute, UColAttributeValue, std::int32_t* status) {
    if (Icu::Failure(status) || coll == nullptr) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
