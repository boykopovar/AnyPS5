#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceImeDialogAbort(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeDialogGetPanelPositionAndForm(PositionAndForm* form) {
 (void)form;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeDialogGetPanelSize(const Param* param, uint32_t* width, uint32_t* height) {
 (void)param;
 (void)width;
 (void)height;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeDialogGetPanelSizeExtended(const Param* param, const ExtendedParam* extended, uint32_t* width, uint32_t* height) {
 (void)param;
 (void)extended;
 (void)width;
 (void)height;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeDialogGetResult(Result* result) {
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeDialogGetStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeDialogInit(const Param* param, const ExtendedParam* extended) {
 (void)param;
 (void)extended;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeDialogTerm(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
