#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"

#include <limits>

using namespace FreeType;

extern "C" {

int APS5_VABI FT_Request_Size_nid_postfix(GuestFaceRec* face, const GuestSizeRequestRec* request) {
    GuestFace* owner = FaceOf(face);
    if (!owner || !owner->host->size || !request) return FT_Request_Size(owner ? owner->host : nullptr, nullptr);
    if (request->width < 0 || request->height < 0 || request->type >= FT_SIZE_REQUEST_TYPE_MAX) return FT_Err_Invalid_Argument;
    if (request->width > std::numeric_limits<FT_Long>::max() || request->height > std::numeric_limits<FT_Long>::max()) {
        NotImplemented_nid_no_patch(__func__);
    }
    FT_Size_RequestRec hostRequest{static_cast<FT_Size_Request_Type>(request->type), static_cast<FT_Long>(request->width),
        static_cast<FT_Long>(request->height), request->horiResolution, request->vertResolution};
    const FT_Error error = FT_Request_Size(owner->host, &hostRequest);
    SyncSize(*owner);
    return error;
}

}
