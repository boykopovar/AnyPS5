#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

#if defined(_WIN32)

extern "C" {

int APS5_VABI FT_Activate_SizeForward_nid_no_patch(FreeType::GuestSizeRec* size) {
    return FT_Activate_Size_nid_postfix(size);
}
APS5_EXPORT("EBvr2CZnjVo", FT_Activate_SizeForward_nid_no_patch);

int APS5_VABI FT_Bitmap_ConvertForward_nid_no_patch(FreeType::GuestLibrary* library, const FreeType::GuestBitmap* source, FreeType::GuestBitmap* target, std::int32_t alignment) {
    return FT_Bitmap_Convert_nid_postfix(library, source, target, alignment);
}
APS5_EXPORT("cR-yQldyXvs", FT_Bitmap_ConvertForward_nid_no_patch);

int APS5_VABI FT_Bitmap_CopyForward_nid_no_patch(FreeType::GuestLibrary* library, const FreeType::GuestBitmap* source, FreeType::GuestBitmap* target) {
    return FT_Bitmap_Copy_nid_postfix(library, source, target);
}
APS5_EXPORT("1GL9pCFuiHQ", FT_Bitmap_CopyForward_nid_no_patch);

int APS5_VABI FT_Bitmap_DoneForward_nid_no_patch(FreeType::GuestLibrary* library, FreeType::GuestBitmap* bitmap) {
    return FT_Bitmap_Done_nid_postfix(library, bitmap);
}
APS5_EXPORT("Mu2upxmY2C8", FT_Bitmap_DoneForward_nid_no_patch);

void APS5_VABI FT_Bitmap_NewForward_nid_no_patch(FreeType::GuestBitmap* bitmap) {
    return FT_Bitmap_New_nid_postfix(bitmap);
}
APS5_EXPORT("-AeE58GkM+Q", FT_Bitmap_NewForward_nid_no_patch);

int APS5_VABI FT_Done_FaceForward_nid_no_patch(FreeType::GuestFaceRec* face) {
    return FT_Done_Face_nid_postfix(face);
}
APS5_EXPORT("OgFczHko0w8", FT_Done_FaceForward_nid_no_patch);

int APS5_VABI FT_Done_FreeTypeForward_nid_no_patch(FreeType::GuestLibrary* library) {
    return FT_Done_FreeType_nid_postfix(library);
}
APS5_EXPORT("dH02CpEEcYk", FT_Done_FreeTypeForward_nid_no_patch);

int APS5_VABI FT_Done_SizeForward_nid_no_patch(FreeType::GuestSizeRec* size) {
    return FT_Done_Size_nid_postfix(size);
}
APS5_EXPORT("-2rnxRCeHMs", FT_Done_SizeForward_nid_no_patch);

int APS5_VABI FT_Get_CID_From_Glyph_IndexForward_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t glyphIndex, std::uint32_t* cid) {
    return FT_Get_CID_From_Glyph_Index_nid_postfix(face, glyphIndex, cid);
}
APS5_EXPORT("NLwiYXdX5WU", FT_Get_CID_From_Glyph_IndexForward_nid_no_patch);

int APS5_VABI FT_Get_CID_Is_Internally_CID_KeyedForward_nid_no_patch(FreeType::GuestFaceRec* face, std::uint8_t* isCid) {
    return FT_Get_CID_Is_Internally_CID_Keyed_nid_postfix(face, isCid);
}
APS5_EXPORT("p-LFZYbDPDU", FT_Get_CID_Is_Internally_CID_KeyedForward_nid_no_patch);

int APS5_VABI FT_Get_CID_Registry_Ordering_SupplementForward_nid_no_patch(FreeType::GuestFaceRec* face, const char** registry, const char** ordering, std::int32_t* supplement) {
    return FT_Get_CID_Registry_Ordering_Supplement_nid_postfix(face, registry, ordering, supplement);
}
APS5_EXPORT("Pr3eI2uGgvU", FT_Get_CID_Registry_Ordering_SupplementForward_nid_no_patch);

std::int64_t APS5_VABI FT_Get_CMap_FormatForward_nid_no_patch(FreeType::GuestCharMapRec* charmap) {
    return FT_Get_CMap_Format_nid_postfix(charmap);
}
APS5_EXPORT("MXgrzq0nh1Q", FT_Get_CMap_FormatForward_nid_no_patch);

std::uint64_t APS5_VABI FT_Get_CMap_Language_IDForward_nid_no_patch(FreeType::GuestCharMapRec* charmap) {
    return FT_Get_CMap_Language_ID_nid_postfix(charmap);
}
APS5_EXPORT("2UZg+FAym4c", FT_Get_CMap_Language_IDForward_nid_no_patch);

std::uint32_t APS5_VABI FT_Get_Char_IndexForward_nid_no_patch(FreeType::GuestFaceRec* face, std::uint64_t charcode) {
    return FT_Get_Char_Index_nid_postfix(face, charcode);
}
APS5_EXPORT("obnSzeb-KXk", FT_Get_Char_IndexForward_nid_no_patch);

int APS5_VABI FT_Get_Charmap_IndexForward_nid_no_patch(FreeType::GuestCharMapRec* charmap) {
    return FT_Get_Charmap_Index_nid_postfix(charmap);
}
APS5_EXPORT("edmDMsPE7Zg", FT_Get_Charmap_IndexForward_nid_no_patch);

std::uint16_t APS5_VABI FT_Get_FSType_FlagsForward_nid_no_patch(FreeType::GuestFaceRec* face) {
    return FT_Get_FSType_Flags_nid_postfix(face);
}
APS5_EXPORT("H4eyE4ez7O4", FT_Get_FSType_FlagsForward_nid_no_patch);

std::uint64_t APS5_VABI FT_Get_First_CharForward_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t* glyphIndex) {
    return FT_Get_First_Char_nid_postfix(face, glyphIndex);
}
APS5_EXPORT("qoysbJFu5sI", FT_Get_First_CharForward_nid_no_patch);

int APS5_VABI FT_Get_GaspForward_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t ppem) {
    return FT_Get_Gasp_nid_postfix(face, ppem);
}
APS5_EXPORT("l4azYkX86Ds", FT_Get_GaspForward_nid_no_patch);

int APS5_VABI FT_Get_Glyph_NameForward_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t glyphIndex, void* buffer, std::uint32_t bufferSize) {
    return FT_Get_Glyph_Name_nid_postfix(face, glyphIndex, buffer, bufferSize);
}
APS5_EXPORT("tGmYdLIYT98", FT_Get_Glyph_NameForward_nid_no_patch);

int APS5_VABI FT_Get_KerningForward_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t leftGlyph, std::uint32_t rightGlyph, std::uint32_t kernMode, FreeType::GuestVector* kerning) {
    return FT_Get_Kerning_nid_postfix(face, leftGlyph, rightGlyph, kernMode, kerning);
}
APS5_EXPORT("LPtlB241JSg", FT_Get_KerningForward_nid_no_patch);

std::uint32_t APS5_VABI FT_Get_Name_IndexForward_nid_no_patch(FreeType::GuestFaceRec* face, const char* glyphName) {
    return FT_Get_Name_Index_nid_postfix(face, glyphName);
}
APS5_EXPORT("sZzliEQrICM", FT_Get_Name_IndexForward_nid_no_patch);

std::uint64_t APS5_VABI FT_Get_Next_CharForward_nid_no_patch(FreeType::GuestFaceRec* face, std::uint64_t charcode, std::uint32_t* glyphIndex) {
    return FT_Get_Next_Char_nid_postfix(face, charcode, glyphIndex);
}
APS5_EXPORT("egPbre4QGBI", FT_Get_Next_CharForward_nid_no_patch);

const char* APS5_VABI FT_Get_Postscript_NameForward_nid_no_patch(FreeType::GuestFaceRec* face) {
    return FT_Get_Postscript_Name_nid_postfix(face);
}
APS5_EXPORT("GVnNjpJWCXM", FT_Get_Postscript_NameForward_nid_no_patch);

int APS5_VABI FT_Get_Sfnt_NameForward_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t index, FreeType::GuestSfntName* name) {
    return FT_Get_Sfnt_Name_nid_postfix(face, index, name);
}
APS5_EXPORT("B4-zPp0EATE", FT_Get_Sfnt_NameForward_nid_no_patch);

std::uint32_t APS5_VABI FT_Get_Sfnt_Name_CountForward_nid_no_patch(FreeType::GuestFaceRec* face) {
    return FT_Get_Sfnt_Name_Count_nid_postfix(face);
}
APS5_EXPORT("fbRv4M+XzL8", FT_Get_Sfnt_Name_CountForward_nid_no_patch);

const char* APS5_VABI FT_Get_X11_Font_FormatForward_nid_no_patch(FreeType::GuestFaceRec* face) {
    return FT_Get_X11_Font_Format_nid_postfix(face);
}
APS5_EXPORT("aWMUKhiwUbk", FT_Get_X11_Font_FormatForward_nid_no_patch);

int APS5_VABI FT_Init_FreeTypeForward_nid_no_patch(FreeType::GuestLibrary** alibrary) {
    return FT_Init_FreeType_nid_postfix(alibrary);
}
APS5_EXPORT("GNsiLzm4SH8", FT_Init_FreeTypeForward_nid_no_patch);

void APS5_VABI FT_List_AddForward_nid_no_patch(FreeType::GuestListRec* list, FreeType::GuestListNodeRec* node) {
    return FT_List_Add_nid_postfix(list, node);
}
APS5_EXPORT("ZuYkeoBhJ1Y", FT_List_AddForward_nid_no_patch);

void APS5_VABI FT_List_FinalizeForward_nid_no_patch(FreeType::GuestListRec* list, FreeType::GuestListDestructor destroy, FreeType::GuestMemoryRec* memory, void* user) {
    return FT_List_Finalize_nid_postfix(list, destroy, memory, user);
}
APS5_EXPORT("0SFUOd2K6n0", FT_List_FinalizeForward_nid_no_patch);

FreeType::GuestListNodeRec* APS5_VABI FT_List_FindForward_nid_no_patch(FreeType::GuestListRec* list, void* data) {
    return FT_List_Find_nid_postfix(list, data);
}
APS5_EXPORT("ld-zX5i4Y4I", FT_List_FindForward_nid_no_patch);

void APS5_VABI FT_List_InsertForward_nid_no_patch(FreeType::GuestListRec* list, FreeType::GuestListNodeRec* node) {
    return FT_List_Insert_nid_postfix(list, node);
}
APS5_EXPORT("7VnJtqQSwUU", FT_List_InsertForward_nid_no_patch);

int APS5_VABI FT_List_IterateForward_nid_no_patch(FreeType::GuestListRec* list, FreeType::GuestListIterator iterator, void* user) {
    return FT_List_Iterate_nid_postfix(list, iterator, user);
}
APS5_EXPORT("h32Nw3vc8UE", FT_List_IterateForward_nid_no_patch);

void APS5_VABI FT_List_RemoveForward_nid_no_patch(FreeType::GuestListRec* list, FreeType::GuestListNodeRec* node) {
    return FT_List_Remove_nid_postfix(list, node);
}
APS5_EXPORT("ioVn-Xe8050", FT_List_RemoveForward_nid_no_patch);

void APS5_VABI FT_List_UpForward_nid_no_patch(FreeType::GuestListRec* list, FreeType::GuestListNodeRec* node) {
    return FT_List_Up_nid_postfix(list, node);
}
APS5_EXPORT("QgnaYvwDlPs", FT_List_UpForward_nid_no_patch);

int APS5_VABI FT_Load_CharForward_nid_no_patch(FreeType::GuestFaceRec* face, std::uint64_t charCode, std::int32_t loadFlags) {
    return FT_Load_Char_nid_postfix(face, charCode, loadFlags);
}
APS5_EXPORT("wPQ0rMK4-JU", FT_Load_CharForward_nid_no_patch);

int APS5_VABI FT_Load_GlyphForward_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t glyphIndex, std::int32_t loadFlags) {
    return FT_Load_Glyph_nid_postfix(face, glyphIndex, loadFlags);
}
APS5_EXPORT("M156RuYoUbU", FT_Load_GlyphForward_nid_no_patch);

int APS5_VABI FT_Load_Sfnt_TableForward_nid_no_patch(FreeType::GuestFaceRec* face, std::uint64_t tag, std::int64_t offset, unsigned char* buffer, std::uint64_t* length) {
    return FT_Load_Sfnt_Table_nid_postfix(face, tag, offset, buffer, length);
}
APS5_EXPORT("09vKaCuNyqI", FT_Load_Sfnt_TableForward_nid_no_patch);

int APS5_VABI FT_New_FaceForward_nid_no_patch(FreeType::GuestLibrary* library, const char* filepathname, std::int64_t faceIndex, FreeType::GuestFaceRec** aface) {
    return FT_New_Face_nid_postfix(library, filepathname, faceIndex, aface);
}
APS5_EXPORT("j-uMQE+unFY", FT_New_FaceForward_nid_no_patch);

int APS5_VABI FT_New_Memory_FaceForward_nid_no_patch(FreeType::GuestLibrary* library, const unsigned char* fileBase, std::int64_t fileSize, std::int64_t faceIndex, FreeType::GuestFaceRec** aface) {
    return FT_New_Memory_Face_nid_postfix(library, fileBase, fileSize, faceIndex, aface);
}
APS5_EXPORT("40Wa2neSLM0", FT_New_Memory_FaceForward_nid_no_patch);

int APS5_VABI FT_New_SizeForward_nid_no_patch(FreeType::GuestFaceRec* face, FreeType::GuestSizeRec** asize) {
    return FT_New_Size_nid_postfix(face, asize);
}
APS5_EXPORT("zyezaHweY3U", FT_New_SizeForward_nid_no_patch);

int APS5_VABI FT_Outline_CheckForward_nid_no_patch(FreeType::GuestOutline* outline) {
    return FT_Outline_Check_nid_postfix(outline);
}
APS5_EXPORT("C4a3dGgs+Cc", FT_Outline_CheckForward_nid_no_patch);

void APS5_VABI FT_Outline_Get_CBoxForward_nid_no_patch(const FreeType::GuestOutline* outline, FreeType::GuestBBox* box) {
    return FT_Outline_Get_CBox_nid_postfix(outline, box);
}
APS5_EXPORT("8ZxmGwESvSQ", FT_Outline_Get_CBoxForward_nid_no_patch);

void APS5_VABI FT_Outline_ReverseForward_nid_no_patch(FreeType::GuestOutline* outline) {
    return FT_Outline_Reverse_nid_postfix(outline);
}
APS5_EXPORT("bH12z-W8uIM", FT_Outline_ReverseForward_nid_no_patch);

void APS5_VABI FT_Outline_TranslateForward_nid_no_patch(const FreeType::GuestOutline* outline, std::int64_t xOffset, std::int64_t yOffset) {
    return FT_Outline_Translate_nid_postfix(outline, xOffset, yOffset);
}
APS5_EXPORT("7HoSJ30RWXk", FT_Outline_TranslateForward_nid_no_patch);

int APS5_VABI FT_Reference_FaceForward_nid_no_patch(FreeType::GuestFaceRec* face) {
    return FT_Reference_Face_nid_postfix(face);
}
APS5_EXPORT("HkSJXCHCQCU", FT_Reference_FaceForward_nid_no_patch);

int APS5_VABI FT_Render_GlyphForward_nid_no_patch(FreeType::GuestGlyphSlotRec* slot, std::int32_t renderMode) {
    return FT_Render_Glyph_nid_postfix(slot, renderMode);
}
APS5_EXPORT("TCRmigbq7lc", FT_Render_GlyphForward_nid_no_patch);

int APS5_VABI FT_Request_SizeForward_nid_no_patch(FreeType::GuestFaceRec* face, const FreeType::GuestSizeRequestRec* request) {
    return FT_Request_Size_nid_postfix(face, request);
}
APS5_EXPORT("PaDk9bdmdhU", FT_Request_SizeForward_nid_no_patch);

int APS5_VABI FT_Select_CharmapForward_nid_no_patch(FreeType::GuestFaceRec* face, std::int32_t encoding) {
    return FT_Select_Charmap_nid_postfix(face, encoding);
}
APS5_EXPORT("G4v5vCrrp40", FT_Select_CharmapForward_nid_no_patch);

int APS5_VABI FT_Select_SizeForward_nid_no_patch(FreeType::GuestFaceRec* face, std::int32_t strikeIndex) {
    return FT_Select_Size_nid_postfix(face, strikeIndex);
}
APS5_EXPORT("LlM3ydj7W1s", FT_Select_SizeForward_nid_no_patch);

int APS5_VABI FT_Set_Char_SizeForward_nid_no_patch(FreeType::GuestFaceRec* face, std::int64_t charWidth, std::int64_t charHeight, std::uint32_t horizontalResolution, std::uint32_t verticalResolution) {
    return FT_Set_Char_Size_nid_postfix(face, charWidth, charHeight, horizontalResolution, verticalResolution);
}
APS5_EXPORT("xuY99Qcb108", FT_Set_Char_SizeForward_nid_no_patch);

int APS5_VABI FT_Set_CharmapForward_nid_no_patch(FreeType::GuestFaceRec* face, FreeType::GuestCharMapRec* charmap) {
    return FT_Set_Charmap_nid_postfix(face, charmap);
}
APS5_EXPORT("Caez01BS2mE", FT_Set_CharmapForward_nid_no_patch);

int APS5_VABI FT_Set_Pixel_SizesForward_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t pixelWidth, std::uint32_t pixelHeight) {
    return FT_Set_Pixel_Sizes_nid_postfix(face, pixelWidth, pixelHeight);
}
APS5_EXPORT("lJxMIwj81SQ", FT_Set_Pixel_SizesForward_nid_no_patch);

void APS5_VABI FT_Set_TransformForward_nid_no_patch(FreeType::GuestFaceRec* face, const FreeType::GuestMatrix* matrix, const FreeType::GuestVector* delta) {
    return FT_Set_Transform_nid_postfix(face, matrix, delta);
}
APS5_EXPORT("bcM5xfX2SeA", FT_Set_TransformForward_nid_no_patch);

int APS5_VABI FT_Sfnt_Table_InfoForward_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t tableIndex, std::uint64_t* tag, std::uint64_t* length) {
    return FT_Sfnt_Table_Info_nid_postfix(face, tableIndex, tag, length);
}
APS5_EXPORT("oiRP-fp3SN4", FT_Sfnt_Table_InfoForward_nid_no_patch);

int APS5_VABI FT_Outline_Get_BBoxForward_nid_no_patch(FreeType::GuestOutline* outline, FreeType::GuestBBox* boundingBox) {
    return FT_Outline_Get_BBox_nid_postfix(outline, boundingBox);
}
APS5_EXPORT("0Aal-TIC4so", FT_Outline_Get_BBoxForward_nid_no_patch);

int APS5_VABI FT_Get_AdvanceForward_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t glyphIndex, std::int32_t flags, std::int64_t* advance) {
    return FT_Get_Advance_nid_postfix(face, glyphIndex, flags, advance);
}
APS5_EXPORT("dMSDtQsYuJM", FT_Get_AdvanceForward_nid_no_patch);

int APS5_VABI FT_Get_AdvancesForward_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t start, std::uint32_t count, std::int32_t flags, std::int64_t* advances) {
    return FT_Get_Advances_nid_postfix(face, start, count, flags, advances);
}
APS5_EXPORT("l55oKq6zmrA", FT_Get_AdvancesForward_nid_no_patch);

int APS5_VABI FT_Stroker_NewForward_nid_no_patch(FreeType::GuestLibrary* library, void** stroker) {
    return FT_Stroker_New_nid_postfix(library, stroker);
}
APS5_EXPORT("eqGCaeizvWM", FT_Stroker_NewForward_nid_no_patch);

void APS5_VABI FT_Stroker_DoneForward_nid_no_patch(void* stroker) {
    return FT_Stroker_Done_nid_postfix(stroker);
}
APS5_EXPORT("+RLa-6fbshA", FT_Stroker_DoneForward_nid_no_patch);

int APS5_VABI FT_Stroker_GetCountsForward_nid_no_patch(void* stroker, std::uint32_t* points, std::uint32_t* contours) {
    return FT_Stroker_GetCounts_nid_postfix(stroker, points, contours);
}
APS5_EXPORT("rzIWh7FJnmU", FT_Stroker_GetCountsForward_nid_no_patch);

int APS5_VABI FT_Stroker_BeginSubPathForward_nid_no_patch(void* stroker, const FreeType::GuestVector* to, std::uint8_t open) {
    return FT_Stroker_BeginSubPath_nid_postfix(stroker, to, open);
}
APS5_EXPORT("xj3tNJptfLI", FT_Stroker_BeginSubPathForward_nid_no_patch);

int APS5_VABI FT_Stroker_LineToForward_nid_no_patch(void* stroker, const FreeType::GuestVector* to) {
    return FT_Stroker_LineTo_nid_postfix(stroker, to);
}
APS5_EXPORT("cKndiZnt0EA", FT_Stroker_LineToForward_nid_no_patch);

int APS5_VABI FT_Stroker_EndSubPathForward_nid_no_patch(void* stroker) {
    return FT_Stroker_EndSubPath_nid_postfix(stroker);
}
APS5_EXPORT("pBqh-Uh-gns", FT_Stroker_EndSubPathForward_nid_no_patch);

void APS5_VABI FT_Stroker_SetForward_nid_no_patch(void* stroker, std::int64_t radius, std::int32_t lineCap, std::int32_t lineJoin, std::int64_t miterLimit) {
    return FT_Stroker_Set_nid_postfix(stroker, radius, lineCap, lineJoin, miterLimit);
}
APS5_EXPORT("B3uU5aDZn94", FT_Stroker_SetForward_nid_no_patch);

void APS5_VABI FT_Stroker_RewindForward_nid_no_patch(void* stroker) {
    return FT_Stroker_Rewind_nid_postfix(stroker);
}
APS5_EXPORT("Po8kyd3DlIw", FT_Stroker_RewindForward_nid_no_patch);

int APS5_VABI FT_Stroker_GetBorderCountsForward_nid_no_patch(void* stroker, std::int32_t border, std::uint32_t* points, std::uint32_t* contours) {
    return FT_Stroker_GetBorderCounts_nid_postfix(stroker, border, points, contours);
}
APS5_EXPORT("LfyUi-LkdCk", FT_Stroker_GetBorderCountsForward_nid_no_patch);

int APS5_VABI FT_Stroker_ConicToForward_nid_no_patch(void* stroker, const FreeType::GuestVector* control, const FreeType::GuestVector* to) {
    return FT_Stroker_ConicTo_nid_postfix(stroker, control, to);
}
APS5_EXPORT("TPKhRt1QIeE", FT_Stroker_ConicToForward_nid_no_patch);

int APS5_VABI FT_Stroker_CubicToForward_nid_no_patch(void* stroker, const FreeType::GuestVector* control1, const FreeType::GuestVector* control2, const FreeType::GuestVector* to) {
    return FT_Stroker_CubicTo_nid_postfix(stroker, control1, control2, to);
}
APS5_EXPORT("6+yXRlVrjbc", FT_Stroker_CubicToForward_nid_no_patch);

}

#endif
