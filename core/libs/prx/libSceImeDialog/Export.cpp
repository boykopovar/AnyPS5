#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceImeDialogAbort(void) {
 return 0;
}

int sceImeDialogGetPanelPositionAndForm(PositionAndForm* form) {
 (void)form;
 return 0;
}

int sceImeDialogGetPanelSize(const Param* param, uint32_t* width, uint32_t* height) {
 (void)param;
 (void)width;
 (void)height;
 return 0;
}

int sceImeDialogGetPanelSizeExtended(const Param* param, const ExtendedParam* extended, uint32_t* width, uint32_t* height) {
 (void)param;
 (void)extended;
 (void)width;
 (void)height;
 return 0;
}

int sceImeDialogGetResult(Result* result) {
 (void)result;
 return 0;
}

int sceImeDialogGetStatus(void) {
 return 0;
}

int sceImeDialogInit(const Param* param, const ExtendedParam* extended) {
 (void)param;
 (void)extended;
 return 0;
}

int sceImeDialogTerm(void) {
 return 0;
}

}
