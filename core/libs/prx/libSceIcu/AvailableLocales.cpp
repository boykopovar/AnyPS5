#include "prx/libSceIcu/DataName.hpp"
#include "prx/libSceIcu/HostEnumeration.hpp"
#include "prx/libSceIcu/include/LegacyExports.hpp"
#include "unicode/uloc.h"
#include "unicode/ures.h"
#include <cstdint>
#include <string>

extern "C" {

UEnumeration* APS5_VABI uloc_openAvailableByType_nid_postfix(std::int32_t type, std::int32_t* status) {
    return Icu::OpenHostEnumeration(
        status,
        [type](UErrorCode* local) -> UEnumeration* {
            if (type < 0 || type >= static_cast<std::int32_t>(ULOC_AVAILABLE_COUNT)) {
                *local = U_ILLEGAL_ARGUMENT_ERROR;
                return nullptr;
            }
            return uloc_openAvailableByType(static_cast<ULocAvailableType>(type), local);
        },
        __func__);
}

UEnumeration* APS5_VABI ures_openAvailableLocales_nid_postfix(const char* path, std::int32_t* status) {
    return Icu::OpenHostEnumeration(
        status,
        [path, caller = __func__](UErrorCode* local) -> UEnumeration* {
            std::string pathCopy;
            const char* hostPath = Icu::CopyDataName(path, pathCopy, *local, caller);
            if (U_FAILURE(*local)) return nullptr;
            return ures_openAvailableLocales(hostPath, local);
        },
        __func__);
}

}
