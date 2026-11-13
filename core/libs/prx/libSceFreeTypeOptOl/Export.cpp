#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestPfr.hpp"

extern "C" {

int APS5_VABI FT_Get_PFR_Advance_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t index, std::int64_t* advance) {
    return FreeTypePfrAdvance_nid_no_patch(face, index, advance);
}

int APS5_VABI FT_Get_PFR_Kerning_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t left, std::uint32_t right, FreeType::GuestVector* vector) {
    return FreeTypePfrKerning_nid_no_patch(face, left, right, vector);
}

int APS5_VABI FT_Get_PFR_Metrics_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t* outline, std::uint32_t* metrics, std::int64_t* xScale, std::int64_t* yScale) {
    return FreeTypePfrMetrics_nid_no_patch(face, outline, metrics, xScale, yScale);
}

}
