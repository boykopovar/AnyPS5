#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include FT_STROKER_H

#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr bool SharedPoints = sizeof(FT_Pos) == sizeof(std::int64_t);

static_assert(!SharedPoints || (sizeof(FT_Vector) == sizeof(FreeType::GuestVector) &&
                                offsetof(FT_Vector, x) == offsetof(FreeType::GuestVector, x) &&
                                offsetof(FT_Vector, y) == offsetof(FreeType::GuestVector, y)));

}

void FreeTypeOtStrokerExport(void* stroker, FreeType::GuestOutline* outline) {
    if (!stroker || !outline) return;
    if constexpr (SharedPoints) {
        FT_Outline native{outline->n_contours, outline->n_points,
                          reinterpret_cast<FT_Vector*>(outline->points), outline->tags,
                          outline->contours, outline->flags};
        FT_Stroker_Export(static_cast<FT_Stroker>(stroker), &native);
        outline->n_contours = static_cast<std::uint16_t>(native.n_contours);
        outline->n_points = static_cast<std::uint16_t>(native.n_points);
    } else {
        NotImplemented_nid_no_patch("sceFreeTypeOtStrokerExport_nid_no_patch");
    }
}
