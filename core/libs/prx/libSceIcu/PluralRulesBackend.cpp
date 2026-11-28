#include "prx/libSceIcu/PluralRulesBackend.hpp"
#include "prx/libSceIcu/HostEnumeration.hpp"
#include "unicode/locid.h"
#include "unicode/plurrule.h"
#include "unicode/strenum.h"
#include "ustrenum.h"
#include <cstdint>

namespace Icu::PluralRulesBackend {

void* OpenForType(const char* locale, std::int32_t type, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    icu::PluralRules* rules = icu::PluralRules::forLocale(icu::Locale(locale), static_cast<UPluralType>(type), local);
    *status = static_cast<std::int32_t>(local);
    return rules;
}

void Close(void* rules) {
    delete static_cast<icu::PluralRules*>(rules);
}

UEnumeration* GetKeywords(const void* rules, std::int32_t* status) {
    return Icu::OpenHostEnumeration(
        status,
        [rules](UErrorCode* local) -> UEnumeration* {
            icu::StringEnumeration* senum = static_cast<const icu::PluralRules*>(rules)->getKeywords(*local);
            if (U_FAILURE(*local)) return nullptr;
            if (!senum) {
                *local = U_MEMORY_ALLOCATION_ERROR;
                return nullptr;
            }
            return uenum_openFromStringEnumeration(senum, local);
        },
        "uplrules_getKeywords_nid_postfix");
}

}
