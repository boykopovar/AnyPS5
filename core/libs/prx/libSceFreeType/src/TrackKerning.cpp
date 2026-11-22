#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/libkernel.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdint>
#include <limits>
#include <stdexcept>

using namespace FreeType;

extern "C" {

int APS5_VABI FT_Get_Track_Kerning_nid_postfix(GuestFaceRec* face, std::int64_t pointSize, std::int32_t degree, std::int64_t* kerning) {
    const GuestFace* owner = FaceOf(face);
    if (!owner || !kerning) return FT_Get_Track_Kerning(owner ? owner->host : nullptr, 0, degree, nullptr);
    GuestArena::HostWrite write(kerning, sizeof(*kerning));
    if (!write.Open() || !GuestWritable_nid_no_patch(kerning, sizeof(*kerning))) return FT_Err_Invalid_Argument;
    if (pointSize < std::numeric_limits<FT_Fixed>::min() || pointSize > std::numeric_limits<FT_Fixed>::max()) {
        throw std::runtime_error("FT_Get_Track_Kerning: a point size outside the host FT_Fixed range is not implemented");
    }
    const auto query = [&](FT_Fixed& result) {
        return FT_Get_Track_Kerning(owner->host, static_cast<FT_Fixed>(pointSize), degree, &result);
    };
    FT_Fixed result = 0;
    if (const FT_Error error = query(result)) return error;
    if (result == 0) {
        FT_Fixed probe = 1;
        if (const FT_Error error = query(probe)) return error;
        if (probe == 1) return 0;
    }
    const std::int64_t value = result;
    if (!GuestCopyOut_nid_no_patch(kerning, &value, sizeof(value))) return FT_Err_Invalid_Argument;
    return 0;
}

}
