#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

namespace {

constexpr std::int32_t PluralTypeCount = 2;

} // namespace

extern "C" {

void APS5_VABI uplrules_close_nid_postfix(UPluralRules* uplrules) {
    if (!uplrules) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

UPluralRules* APS5_VABI uplrules_openForType_nid_postfix(const char*, std::int32_t type, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    if (type >= PluralTypeCount) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
