#pragma once

#include "prx/libSceFreeType/include/GuestList.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libSceFreeType/include/PostScriptInfo.hpp"
#include "SceTypes.hpp"

extern "C" {

std::int32_t APS5_VABI FT_Outline_GetInsideBorder_nid_postfix(const FreeType::GuestOutline* outline);
std::int32_t APS5_VABI FT_Outline_GetOutsideBorder_nid_postfix(const FreeType::GuestOutline* outline);

int APS5_VABI FT_Init_FreeType_nid_postfix(FreeType::GuestLibrary** alibrary);
int APS5_VABI FT_Done_FreeType_nid_postfix(FreeType::GuestLibrary* library);
int APS5_VABI FT_New_Face_nid_postfix(FreeType::GuestLibrary* library, const char* filepathname, std::int64_t faceIndex, FreeType::GuestFaceRec** aface);
int APS5_VABI FT_New_Memory_Face_nid_postfix(FreeType::GuestLibrary* library, const unsigned char* fileBase, std::int64_t fileSize, std::int64_t faceIndex, FreeType::GuestFaceRec** aface);
int APS5_VABI FT_Reference_Face_nid_postfix(FreeType::GuestFaceRec* face);
int APS5_VABI FT_Done_Face_nid_postfix(FreeType::GuestFaceRec* face);
int APS5_VABI FT_Select_Size_nid_postfix(FreeType::GuestFaceRec* face, std::int32_t strikeIndex);
int APS5_VABI FT_New_Size_nid_postfix(FreeType::GuestFaceRec* face, FreeType::GuestSizeRec** asize);
int APS5_VABI FT_Activate_Size_nid_postfix(FreeType::GuestSizeRec* size);
int APS5_VABI FT_Done_Size_nid_postfix(FreeType::GuestSizeRec* size);
int APS5_VABI FT_Set_Pixel_Sizes_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t pixelWidth, std::uint32_t pixelHeight);
int APS5_VABI FT_Set_Char_Size_nid_postfix(FreeType::GuestFaceRec* face, std::int64_t charWidth, std::int64_t charHeight, std::uint32_t horizontalResolution, std::uint32_t verticalResolution);
std::uint32_t APS5_VABI FT_Get_Char_Index_nid_postfix(FreeType::GuestFaceRec* face, std::uint64_t charcode);
int APS5_VABI FT_Get_Kerning_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t leftGlyph, std::uint32_t rightGlyph, std::uint32_t kernMode, FreeType::GuestVector* kerning);
int APS5_VABI FT_Get_PFR_Advance_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t index, std::int64_t* advance);
int APS5_VABI FT_Get_PFR_Kerning_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t left, std::uint32_t right, FreeType::GuestVector* vector);
int APS5_VABI FT_Get_PFR_Metrics_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t* outline, std::uint32_t* metrics, std::int64_t* xScale, std::int64_t* yScale);
int APS5_VABI FT_Load_Glyph_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t glyphIndex, std::int32_t loadFlags);
int APS5_VABI FT_Get_SubGlyph_Info_nid_postfix(FreeType::GuestGlyphSlotRec* slot, std::uint32_t subIndex, std::int32_t* index, std::uint32_t* flags,
                                              std::int32_t* arg1, std::int32_t* arg2, FreeType::GuestMatrix* transform);
int APS5_VABI FT_Get_Advance_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t glyphIndex, std::int32_t flags, std::int64_t* advance);
int APS5_VABI FT_Get_Advances_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t start, std::uint32_t count, std::int32_t flags, std::int64_t* advances);
int APS5_VABI FT_Get_Gasp_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t ppem);
int APS5_VABI FT_Get_CID_Registry_Ordering_Supplement_nid_postfix(FreeType::GuestFaceRec* face, const char** registry, const char** ordering, std::int32_t* supplement);
int APS5_VABI FT_Get_CID_Is_Internally_CID_Keyed_nid_postfix(FreeType::GuestFaceRec* face, std::uint8_t* isCid);
int APS5_VABI FT_Get_CID_From_Glyph_Index_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t glyphIndex, std::uint32_t* cid);
int APS5_VABI FT_Load_Char_nid_postfix(FreeType::GuestFaceRec* face, std::uint64_t charCode, std::int32_t loadFlags);
int APS5_VABI FT_Outline_Get_Orientation_nid_postfix(FreeType::GuestOutline* outline);
int APS5_VABI FT_Outline_Copy_nid_postfix(const FreeType::GuestOutline* source, FreeType::GuestOutline* target);
int APS5_VABI FT_Outline_Decompose_nid_postfix(FreeType::GuestOutline* outline, const FreeType::GuestOutlineFuncs* functions, void* user);
int APS5_VABI FT_Render_Glyph_nid_postfix(FreeType::GuestGlyphSlotRec* slot, std::int32_t renderMode);
int APS5_VABI FT_Select_Charmap_nid_postfix(FreeType::GuestFaceRec* face, std::int32_t encoding);
int APS5_VABI FT_Set_Charmap_nid_postfix(FreeType::GuestFaceRec* face, FreeType::GuestCharMapRec* charmap);
std::uint64_t APS5_VABI FT_Get_First_Char_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t* glyphIndex);
std::uint64_t APS5_VABI FT_Get_Next_Char_nid_postfix(FreeType::GuestFaceRec* face, std::uint64_t charcode, std::uint32_t* glyphIndex);
void APS5_VABI FT_Set_Transform_nid_postfix(FreeType::GuestFaceRec* face, const FreeType::GuestMatrix* matrix, const FreeType::GuestVector* delta);
int APS5_VABI FT_Get_Charmap_Index_nid_postfix(FreeType::GuestCharMapRec* charmap);
std::uint64_t APS5_VABI FT_Get_CMap_Language_ID_nid_postfix(FreeType::GuestCharMapRec* charmap);
std::int64_t APS5_VABI FT_Get_CMap_Format_nid_postfix(FreeType::GuestCharMapRec* charmap);
const char* APS5_VABI FT_Get_X11_Font_Format_nid_postfix(FreeType::GuestFaceRec* face);
std::uint16_t APS5_VABI FT_Get_FSType_Flags_nid_postfix(FreeType::GuestFaceRec* face);
FreeType::GuestListNodeRec* APS5_VABI FT_List_Find_nid_postfix(FreeType::GuestListRec* list, void* data);
void APS5_VABI FT_List_Add_nid_postfix(FreeType::GuestListRec* list, FreeType::GuestListNodeRec* node);
void APS5_VABI FT_List_Insert_nid_postfix(FreeType::GuestListRec* list, FreeType::GuestListNodeRec* node);
void APS5_VABI FT_List_Remove_nid_postfix(FreeType::GuestListRec* list, FreeType::GuestListNodeRec* node);
void APS5_VABI FT_List_Up_nid_postfix(FreeType::GuestListRec* list, FreeType::GuestListNodeRec* node);
int APS5_VABI FT_List_Iterate_nid_postfix(FreeType::GuestListRec* list, FreeType::GuestListIterator iterator, void* user);
void APS5_VABI FT_List_Finalize_nid_postfix(FreeType::GuestListRec* list, FreeType::GuestListDestructor destroy, FreeType::GuestMemoryRec* memory, void* user);
int APS5_VABI FT_Get_Glyph_Name_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t glyphIndex, void* buffer, std::uint32_t bufferSize);
std::uint32_t APS5_VABI FT_Get_Name_Index_nid_postfix(FreeType::GuestFaceRec* face, const char* glyphName);
const char* APS5_VABI FT_Get_Postscript_Name_nid_postfix(FreeType::GuestFaceRec* face);
int APS5_VABI FT_Get_PS_Font_Info_nid_postfix(FreeType::GuestFaceRec* face, FreeType::GuestPSFontInfo* info);
int APS5_VABI FT_Has_PS_Glyph_Names_nid_postfix(FreeType::GuestFaceRec* face);
int APS5_VABI sceFreeTypeOtHasPSGlyphNames_nid_no_patch();
std::uint32_t APS5_VABI FT_Get_Sfnt_Name_Count_nid_postfix(FreeType::GuestFaceRec* face);
int APS5_VABI FT_Get_Sfnt_Name_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t index, FreeType::GuestSfntName* name);
int APS5_VABI FT_Sfnt_Table_Info_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t tableIndex, std::uint64_t* tag, std::uint64_t* length);
int APS5_VABI FT_Load_Sfnt_Table_nid_postfix(FreeType::GuestFaceRec* face, std::uint64_t tag, std::int64_t offset, unsigned char* buffer, std::uint64_t* length);
int APS5_VABI FT_Outline_Check_nid_postfix(FreeType::GuestOutline* outline);
void APS5_VABI FT_Outline_Get_CBox_nid_postfix(const FreeType::GuestOutline* outline, FreeType::GuestBBox* box);
int APS5_VABI FT_Outline_Get_BBox_nid_postfix(FreeType::GuestOutline* outline, FreeType::GuestBBox* boundingBox);
int APS5_VABI sceFreeTypeOtOutlineGetBBox_nid_no_patch();
void APS5_VABI FT_Outline_Translate_nid_postfix(const FreeType::GuestOutline* outline, std::int64_t xOffset, std::int64_t yOffset);
void APS5_VABI FT_Outline_Reverse_nid_postfix(FreeType::GuestOutline* outline);
int APS5_VABI FT_Request_Size_nid_postfix(FreeType::GuestFaceRec* face, const FreeType::GuestSizeRequestRec* request);
int APS5_VABI FT_Property_Get_nid_postfix(FreeType::GuestLibrary* library, const char* moduleName, const char* propertyName, void* value);
int APS5_VABI sceFreeTypeOtPropertyGet_nid_no_patch();
int APS5_VABI FT_Property_Set_nid_postfix(FreeType::GuestLibrary* library, const char* moduleName, const char* propertyName, const void* value);
int APS5_VABI sceFreeTypeOtPropertySet_nid_no_patch();

void APS5_VABI FT_Bitmap_New_nid_postfix(FreeType::GuestBitmap* bitmap);
void APS5_VABI FT_Bitmap_Init_nid_postfix(FreeType::GuestBitmap* bitmap);
int APS5_VABI FT_Bitmap_Copy_nid_postfix(FreeType::GuestLibrary* library, const FreeType::GuestBitmap* source, FreeType::GuestBitmap* target);
int APS5_VABI FT_Bitmap_Convert_nid_postfix(FreeType::GuestLibrary* library, const FreeType::GuestBitmap* source, FreeType::GuestBitmap* target, std::int32_t alignment);
int APS5_VABI FT_Bitmap_Done_nid_postfix(FreeType::GuestLibrary* library, FreeType::GuestBitmap* bitmap);
std::int64_t APS5_VABI FT_Cos_nid_postfix(std::int64_t angle);
std::int64_t APS5_VABI FT_Sin_nid_postfix(std::int64_t angle);
std::int64_t APS5_VABI FT_Tan_nid_postfix(std::int64_t angle);
void APS5_VABI FT_Vector_Unit_nid_postfix(FreeType::GuestVector* vector, std::int64_t angle);
std::int64_t APS5_VABI FT_Angle_Diff_nid_postfix(std::int64_t first, std::int64_t second);
int APS5_VABI FT_Stroker_GetCounts_nid_postfix(void* stroker, std::uint32_t* points, std::uint32_t* contours);
int APS5_VABI FT_Stroker_CubicTo_nid_postfix(void* stroker, const FreeType::GuestVector* control1, const FreeType::GuestVector* control2, const FreeType::GuestVector* to);
void APS5_VABI FT_Stroker_Done_nid_postfix(void* stroker);
int APS5_VABI FT_Stroker_GetBorderCounts_nid_postfix(void* stroker, std::int32_t border, std::uint32_t* points, std::uint32_t* contours);
int APS5_VABI FT_Stroker_EndSubPath_nid_postfix(void* stroker);
int APS5_VABI FT_Stroker_LineTo_nid_postfix(void* stroker, const FreeType::GuestVector* to);
int APS5_VABI FT_Stroker_New_nid_postfix(FreeType::GuestLibrary* library, void** stroker);
void APS5_VABI FT_Stroker_Set_nid_postfix(void* stroker, std::int64_t radius, std::int32_t lineCap, std::int32_t lineJoin, std::int64_t miterLimit);
void APS5_VABI FT_Stroker_Rewind_nid_postfix(void* stroker);
int APS5_VABI FT_Stroker_BeginSubPath_nid_postfix(void* stroker, const FreeType::GuestVector* to, std::uint8_t open);
int APS5_VABI FT_Stroker_ConicTo_nid_postfix(void* stroker, const FreeType::GuestVector* control, const FreeType::GuestVector* to);
int APS5_VABI ft_corner_is_flat_nid_postfix(std::int64_t in_x, std::int64_t in_y, std::int64_t out_x, std::int64_t out_y);
int APS5_VABI ft_corner_orientation_nid_postfix(std::int64_t in_x, std::int64_t in_y, std::int64_t out_x, std::int64_t out_y);

}
