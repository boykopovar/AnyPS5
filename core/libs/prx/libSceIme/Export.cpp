#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceImeClose_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeGetPanelSize(const Param* param, uint32_t* width, uint32_t* height) {
 (void)param;
 (void)width;
 (void)height;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeKeyboardClose(int32_t user_id) {
 (void)user_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeKeyboardGetInfo(uint32_t resource_id, KeyboardInfo* info) {
 (void)resource_id;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeKeyboardGetResourceId(int32_t user_id, KeyboardResourceIdArray* resource_ids) {
 (void)user_id;
 (void)resource_ids;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeKeyboardOpen(int32_t user_id, const KeyboardParam* param) {
 (void)user_id;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeKeyboardSetMode(int32_t user_id, uint32_t mode) {
 (void)user_id;
 (void)mode;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeOpen_nid_postfix(const Param* param, const ExtendedParam* extended) {
 (void)param;
 (void)extended;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void sceImeParamInit(Param* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
}

int sceImeSetCaret(const Caret* caret) {
 (void)caret;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeSetText(const char16_t* text, uint32_t length) {
 (void)text;
 (void)length;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeSetTextGeometry(TextAreaMode mode, const TextGeometry* geometry) {
 (void)mode;
 (void)geometry;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeUpdate(EventHandler handler) {
 (void)handler;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
