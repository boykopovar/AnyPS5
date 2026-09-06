#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceImeDialogAbort(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeDialogGetPanelPositionAndForm(PositionAndForm* form) {
 (void)form;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeDialogGetPanelSize(const Param* param, uint32_t* width, uint32_t* height) {
 (void)param;
 (void)width;
 (void)height;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeDialogGetPanelSizeExtended(const Param* param, const ExtendedParam* extended, uint32_t* width, uint32_t* height) {
 (void)param;
 (void)extended;
 (void)width;
 (void)height;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeDialogGetResult(Result* result) {
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeDialogGetStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeDialogInit(const Param* param, const ExtendedParam* extended) {
 (void)param;
 (void)extended;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceImeDialogTerm(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
