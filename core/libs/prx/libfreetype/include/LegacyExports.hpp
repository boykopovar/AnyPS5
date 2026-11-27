#pragma once

#include "SceTypes.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"

namespace LegacyFreeType { struct GuestStreamRec; }

extern "C" {

int APS5_VABI freetypeLegacyFtcManagerNew_nid_no_patch(FreeType::GuestLibrary* library, std::uint32_t maxFaces, std::uint32_t maxSizes,
                                                      std::uint64_t maxBytes, FreeType::GuestFaceRequester requester, void* reqData, void** manager);
int APS5_VABI FT_Stream_OpenGzip_nid_postfix(LegacyFreeType::GuestStreamRec* stream, LegacyFreeType::GuestStreamRec* source);
int APS5_VABI FT_Stream_OpenBzip2_nid_postfix(LegacyFreeType::GuestStreamRec* stream, LegacyFreeType::GuestStreamRec* source);
const char* APS5_VABI FT_Error_String_nid_postfix(std::int32_t errorCode);
int APS5_VABI FT_Stream_OpenLZW_nid_postfix(LegacyFreeType::GuestStreamRec* stream, LegacyFreeType::GuestStreamRec* source);
int APS5_VABI freetypeLegacyNewGlyph_nid_no_patch(FreeType::GuestLibrary* library, std::uint32_t format, FreeType::GuestGlyphRec** target);

}
