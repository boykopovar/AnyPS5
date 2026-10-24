#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

using namespace FreeType;

extern "C" {

int APS5_VABI FT_Get_Track_Kerning_nid_postfix(GuestFaceRec* face, std::int64_t pointSize, std::int32_t degree, std::int64_t* kerning) {
    const GuestFace* owner = FaceOf(face);
    if (!owner || !kerning) return FT_Get_Track_Kerning(owner ? owner->host : nullptr, 0, degree, nullptr);
    if (pointSize < std::numeric_limits<FT_Fixed>::min() || pointSize > std::numeric_limits<FT_Fixed>::max()) {
        throw std::runtime_error("FT_Get_Track_Kerning: a point size outside the host FT_Fixed range is not implemented");
    }
    FT_Fixed result = 0;
    const FT_Error error = FT_Get_Track_Kerning(owner->host, static_cast<FT_Fixed>(pointSize), degree, &result);
    if (error != 0) return error;
    GuestArena::HostWrite write(kerning, sizeof(*kerning));
    *kerning = result;
    return 0;
}

}
