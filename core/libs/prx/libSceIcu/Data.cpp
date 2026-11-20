#include "prx/libSceIcu/Data.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/udata.h"

extern "C" {

void APS5_VABI udata_setCommonData_nid_postfix(const void* data, std::int32_t* status) {
    if (!status || !GuestWritable_nid_no_patch(status, sizeof(*status))) return;
    auto nativeStatus = static_cast<UErrorCode>(*status);
    udata_setCommonData(data, &nativeStatus);
    *status = nativeStatus;
}

void APS5_VABI udata_setFileAccess_nid_postfix(std::int32_t access, std::int32_t* status) {
    if (status && !GuestWritable_nid_no_patch(status, sizeof(*status))) return;
    udata_setFileAccess(static_cast<UDataFileAccess>(access), nullptr);
}

}
