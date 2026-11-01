#pragma once

#include "prx/libSceFreeType/include/Api.hpp"

extern "C" {
void APS5_VABI sceFreeTypeOlDoneGlyphSlot_nid_postfix(FreeType::GuestGlyphSlotRec* slot);
int APS5_VABI sceFreeTypeOlFtaSupportFormatBdf_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaSupportFormatPcf_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaSupportFormatPfr_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaSupportFormatWinFonts_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaSupportHinterAutoFitter_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaSupportRendererRaster1_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaSupportRendererSmooth_nid_postfix(void* library);

void APS5_VABI sceFreeTypeOlFtaAddModuleGxvalid_nid_postfix(void* library);
void APS5_VABI sceFreeTypeOlFtaAddModuleOtvalid_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModuleGxvalid_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModuleOtvalid_nid_postfix(void* library);

int APS5_VABI sceFreeTypeOlAlloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t size, void** output);
void APS5_VABI sceFreeTypeOlFree_nid_postfix(FreeType::GuestMemoryRec* memory, void** block);
void APS5_VABI sceFreeTypeOlFtaAddModuleAutofitter_nid_postfix(void* library);
void APS5_VABI sceFreeTypeOlFtaAddModuleBdf_nid_postfix(void* library);
void APS5_VABI sceFreeTypeOlFtaAddModuleCff_nid_postfix(void* library);
void APS5_VABI sceFreeTypeOlFtaAddModulePcf_nid_postfix(void* library);
void APS5_VABI sceFreeTypeOlFtaAddModulePfr_nid_postfix(void* library);
void APS5_VABI sceFreeTypeOlFtaAddModulePsaux_nid_postfix(void* library);
void APS5_VABI sceFreeTypeOlFtaAddModulePshinter_nid_postfix(void* library);
void APS5_VABI sceFreeTypeOlFtaAddModulePsnames_nid_postfix(void* library);
void APS5_VABI sceFreeTypeOlFtaAddModuleRaster1_nid_postfix(void* library);
void APS5_VABI sceFreeTypeOlFtaAddModuleSfnt_nid_postfix(void* library);
void APS5_VABI sceFreeTypeOlFtaAddModuleSmooth_nid_postfix(void* library);
void APS5_VABI sceFreeTypeOlFtaAddModuleT1cid_nid_postfix(void* library);
void APS5_VABI sceFreeTypeOlFtaAddModuleTruetype_nid_postfix(void* library);
void APS5_VABI sceFreeTypeOlFtaAddModuleType1_nid_postfix(void* library);
void APS5_VABI sceFreeTypeOlFtaAddModuleType42_nid_postfix(void* library);
void APS5_VABI sceFreeTypeOlFtaAddModuleWinfonts_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModuleAutofitter_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModuleBdf_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModuleCff_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModulePcf_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModulePfr_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModulePsaux_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModulePshinter_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModulePsnames_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModuleRaster1_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModuleSfnt_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModuleSmooth_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModuleT1cid_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModuleTruetype_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModuleType1_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModuleType42_nid_postfix(void* library);
int APS5_VABI sceFreeTypeOlFtaRemoveModuleWinfonts_nid_postfix(void* library);
}
