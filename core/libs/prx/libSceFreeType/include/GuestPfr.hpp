#ifndef CORE_LIBS_PRX_LIBSCEFREETYPE_INCLUDE_GUESTPFR_HPP
#define CORE_LIBS_PRX_LIBSCEFREETYPE_INCLUDE_GUESTPFR_HPP

#include "prx/libSceFreeType/include/GuestFreeType.hpp"

extern "C" {

int FreeTypePfrAdvance_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t index, std::int64_t* advance);
int FreeTypePfrKerning_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t left, std::uint32_t right, FreeType::GuestVector* vector);
int FreeTypePfrMetrics_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t* outline, std::uint32_t* metrics, std::int64_t* xScale, std::int64_t* yScale);

}

#endif
