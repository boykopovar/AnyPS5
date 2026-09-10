#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceImeClose_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeGetPanelSize(const Param* param, uint32_t* width, uint32_t* height) {
 (void)param;
 (void)width;
 (void)height;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeKeyboardClose(int32_t user_id) {
 (void)user_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeKeyboardGetInfo(uint32_t resource_id, KeyboardInfo* info) {
 (void)resource_id;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeKeyboardGetResourceId(int32_t user_id, KeyboardResourceIdArray* resource_ids) {
 (void)user_id;
 (void)resource_ids;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeKeyboardOpen(int32_t user_id, const KeyboardParam* param) {
 (void)user_id;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeKeyboardSetMode(int32_t user_id, uint32_t mode) {
 (void)user_id;
 (void)mode;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeOpen_nid_postfix(const Param* param, const ExtendedParam* extended) {
 (void)param;
 (void)extended;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void APS5_VABI sceImeParamInit(Param* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceImeSetCaret(const Caret* caret) {
 (void)caret;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeSetText(const char16_t* text, uint32_t length) {
 (void)text;
 (void)length;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeSetTextGeometry(TextAreaMode mode, const TextGeometry* geometry) {
 (void)mode;
 (void)geometry;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeUpdate(EventHandler handler) {
 (void)handler;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
