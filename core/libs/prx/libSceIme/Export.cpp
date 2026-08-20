#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceImeClose(void) {
 return 0;
}

int sceImeGetPanelSize(const Param* param, uint32_t* width, uint32_t* height) {
 (void)param;
 (void)width;
 (void)height;
 return 0;
}

int sceImeKeyboardClose(int32_t user_id) {
 (void)user_id;
 return 0;
}

int sceImeKeyboardGetInfo(uint32_t resource_id, KeyboardInfo* info) {
 (void)resource_id;
 (void)info;
 return 0;
}

int sceImeKeyboardGetResourceId(int32_t user_id, KeyboardResourceIdArray* resource_ids) {
 (void)user_id;
 (void)resource_ids;
 return 0;
}

int sceImeKeyboardOpen(int32_t user_id, const KeyboardParam* param) {
 (void)user_id;
 (void)param;
 return 0;
}

int sceImeKeyboardSetMode(int32_t user_id, uint32_t mode) {
 (void)user_id;
 (void)mode;
 return 0;
}

int sceImeOpen(const Param* param, const ExtendedParam* extended) {
 (void)param;
 (void)extended;
 return 0;
}

void sceImeParamInit(Param* param) {
 (void)param;
}

int sceImeSetCaret(const Caret* caret) {
 (void)caret;
 return 0;
}

int sceImeSetText(const char16_t* text, uint32_t length) {
 (void)text;
 (void)length;
 return 0;
}

int sceImeSetTextGeometry(TextAreaMode mode, const TextGeometry* geometry) {
 (void)mode;
 (void)geometry;
 return 0;
}

int sceImeUpdate(EventHandler handler) {
 (void)handler;
 return 0;
}

}
