#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"

static bool g_initialized = false;

extern "C" {

int sceCommonDialogInitialize(void) {
 if (g_initialized) {
  return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
 }
 g_initialized = true;
 return COMMON_DIALOG_OK;
}

bool sceCommonDialogIsUsed(void) {
 return false;
}

}
