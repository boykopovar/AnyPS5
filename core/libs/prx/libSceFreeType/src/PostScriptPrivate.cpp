#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libSceFreeType/include/PostScriptPrivate.hpp"

#include FT_TYPE1_TABLES_H

#include <algorithm>

using namespace FreeType;

extern "C" int APS5_VABI FT_Get_PS_Font_Private_nid_postfix(GuestFaceRec* face, GuestPSPrivate* output) {
    const GuestFace* owner = FaceOf(face);
    PS_PrivateRec native{};
    const int error = FT_Get_PS_Font_Private(owner ? owner->host : nullptr, output ? &native : nullptr);
    if (error != 0) return error;
    output->uniqueId = native.unique_id;
    output->lenIV = native.lenIV;
    output->numBlueValues = native.num_blue_values;
    output->numOtherBlues = native.num_other_blues;
    output->numFamilyBlues = native.num_family_blues;
    output->numFamilyOtherBlues = native.num_family_other_blues;
    std::copy_n(native.blue_values, 14, output->blueValues);
    std::copy_n(native.other_blues, 10, output->otherBlues);
    std::copy_n(native.family_blues, 14, output->familyBlues);
    std::copy_n(native.family_other_blues, 10, output->familyOtherBlues);
    output->blueScale = native.blue_scale;
    output->blueShift = native.blue_shift;
    output->blueFuzz = native.blue_fuzz;
    output->standardWidth[0] = native.standard_width[0];
    output->standardHeight[0] = native.standard_height[0];
    output->numSnapWidths = native.num_snap_widths;
    output->numSnapHeights = native.num_snap_heights;
    output->forceBold = native.force_bold;
    output->roundStemUp = native.round_stem_up;
    std::copy_n(native.snap_widths, 13, output->snapWidths);
    std::copy_n(native.snap_heights, 13, output->snapHeights);
    output->expansionFactor = native.expansion_factor;
    output->languageGroup = native.language_group;
    output->password = native.password;
    std::copy_n(native.min_feature, 2, output->minFeature);
    return 0;
}
