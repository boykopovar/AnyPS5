#include "prx/libSceIcu/HostEnumeration.hpp"
#include "unicode/ucnv.h"
#include <cstdint>

extern "C" {

UEnumeration* APS5_VABI ucnv_openAllNames_nid_postfix(std::int32_t* status) {
    return Icu::OpenHostEnumeration(status, [](UErrorCode* local) { return ucnv_openAllNames(local); }, __func__);
}

}
