#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_TRUETYPE_TABLES_H

#include <cstdint>
#include <stdexcept>

using namespace FreeType;

extern "C" {

void* APS5_VABI FT_Get_Sfnt_Table_nid_postfix(GuestFaceRec* face, std::int32_t tag) {
    const GuestFace* owner = FaceOf(face);
    if (owner && sizeof(FT_Long) != 8) {
        throw std::runtime_error("FT_Get_Sfnt_Table: the LP64 table layouts need a 64-bit host FT_Long, which is not implemented");
    }
    return FT_Get_Sfnt_Table(owner ? owner->host : nullptr, static_cast<FT_Sfnt_Tag>(tag));
}

}
