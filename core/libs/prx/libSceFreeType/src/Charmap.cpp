#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_TRUETYPE_TABLES_H

#include <cstddef>
#include <cstdint>
#include <stdexcept>

using namespace FreeType;

namespace {

FT_CharMap HostCharmap(const GuestCharMapRec* charmap) {
    if (!charmap || !charmap->face) return nullptr;
    const GuestFace* owner = FaceOf(static_cast<const GuestFaceRec*>(charmap->face));
    if (owner && owner->host) {
        for (std::size_t index = 0; index < owner->charmapPointers.size(); ++index) {
            if (owner->charmapPointers[index] == charmap) return owner->host->charmaps[index];
        }
    }
    throw std::runtime_error("FreeType: a charmap outside the guest face's read-only array is not implemented");
}

}

extern "C" {

int APS5_VABI FT_Get_Charmap_Index_nid_postfix(GuestCharMapRec* charmap) {
    return FT_Get_Charmap_Index(HostCharmap(charmap));
}

std::uint64_t APS5_VABI FT_Get_CMap_Language_ID_nid_postfix(GuestCharMapRec* charmap) {
    return FT_Get_CMap_Language_ID(HostCharmap(charmap));
}

std::int64_t APS5_VABI FT_Get_CMap_Format_nid_postfix(GuestCharMapRec* charmap) {
    return FT_Get_CMap_Format(HostCharmap(charmap));
}

}