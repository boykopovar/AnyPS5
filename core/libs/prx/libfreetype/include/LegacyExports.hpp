#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI freetypeLegacyBitmapBlend_nid_no_patch();
int APS5_VABI freetypeLegacyFtcManagerNew_nid_no_patch();
int APS5_VABI freetypeLegacyStreamOpenGzip_nid_no_patch();
int APS5_VABI freetypeLegacyStreamOpenBzip2_nid_no_patch();
const char* APS5_VABI FT_Error_String_nid_postfix(std::int32_t errorCode);
int APS5_VABI freetypeLegacyStreamOpenLZW_nid_no_patch();
int APS5_VABI freetypeLegacyNewGlyph_nid_no_patch();

}
