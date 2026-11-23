#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <limits>

using namespace FreeType;

namespace {

GuestFace* ReadGuestFace(GuestFaceRec* face, const char* caller) {
    if (!face) return nullptr;
    void* internal = nullptr;
    if (!GuestCopyIn_nid_no_patch(&internal, &face->internal, sizeof(internal))) NotImplemented_nid_no_patch(caller);
    return static_cast<GuestFace*>(internal);
}

}

extern "C" {

int APS5_VABI FT_Request_Size_nid_postfix(GuestFaceRec* face, const GuestSizeRequestRec* request) {
    GuestFace* owner = ReadGuestFace(face, __func__);
    if (!owner || !owner->host->size || !request) return FT_Request_Size(owner ? owner->host : nullptr, nullptr);
    GuestSizeRequestRec guestRequest{};
    if (!GuestCopyIn_nid_no_patch(&guestRequest, request, sizeof(guestRequest))) NotImplemented_nid_no_patch(__func__);
    if (guestRequest.width < 0 || guestRequest.height < 0 || guestRequest.type >= FT_SIZE_REQUEST_TYPE_MAX) return FT_Err_Invalid_Argument;
    if (guestRequest.width > std::numeric_limits<FT_Long>::max() || guestRequest.height > std::numeric_limits<FT_Long>::max()) {
        NotImplemented_nid_no_patch(__func__);
    }
    FT_Size_RequestRec hostRequest{static_cast<FT_Size_Request_Type>(guestRequest.type), static_cast<FT_Long>(guestRequest.width),
        static_cast<FT_Long>(guestRequest.height), guestRequest.horiResolution, guestRequest.vertResolution};
    const FT_Error error = FT_Request_Size(owner->host, &hostRequest);
    SyncSize(*owner);
    return error;
}

}
