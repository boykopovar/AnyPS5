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
    std::int32_t incoming = U_ZERO_ERROR;
    if (status && (!GuestWritable_nid_no_patch(status, sizeof(*status)) ||
                   !GuestCopyIn_nid_no_patch(&incoming, status, sizeof(incoming)))) return;
    auto nativeStatus = static_cast<UErrorCode>(incoming);
    udata_setFileAccess(static_cast<UDataFileAccess>(access), &nativeStatus);
    const std::int32_t outgoing = nativeStatus;
    if (status && incoming <= 0 && outgoing != incoming) GuestCopyOut_nid_no_patch(status, &outgoing, sizeof(outgoing));
}

}
