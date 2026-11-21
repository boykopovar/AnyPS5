#include "unicode/udata.h"
#include "unicode/ucnv.h"
#include <cstdint>

extern "C" U_EXPORT std::uint16_t SetupOpenStandardNamesData(const void* data, std::int32_t* error) {
    UErrorCode status = static_cast<UErrorCode>(*error);
    udata_setCommonData(data, &status);
    *error = status;
    return status == 0 ? ucnv_countStandards() : 0;
}
