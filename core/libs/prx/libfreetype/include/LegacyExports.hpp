#pragma once

#include "SceTypes.hpp"

namespace LegacyFreeType { struct GuestStreamRec; }

extern "C" {

int APS5_VABI freetypeLegacyBitmapBlend_nid_no_patch();
int APS5_VABI freetypeLegacyFtcManagerNew_nid_no_patch();
int APS5_VABI FT_Stream_OpenGzip_nid_postfix(LegacyFreeType::GuestStreamRec* stream, LegacyFreeType::GuestStreamRec* source);
int APS5_VABI freetypeLegacyStreamOpenBzip2_nid_no_patch();
int APS5_VABI freetypeLegacyErrorString_nid_no_patch();
int APS5_VABI FT_Stream_OpenLZW_nid_postfix(LegacyFreeType::GuestStreamRec* stream, LegacyFreeType::GuestStreamRec* source);
int APS5_VABI freetypeLegacyNewGlyph_nid_no_patch();

}
