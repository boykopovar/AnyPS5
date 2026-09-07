#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include "SceTypes.hpp"
#include "prx/libSceSaveDataDialog.native/SaveDataDialog.hpp"

static int g_status = SAVE_DATA_DIALOG_STATUS_NONE;
static int g_mode = 0;
static void* g_user_data = nullptr;
static char g_dir_name[32] = {};

extern "C" {

int sceSaveDataDialogInitialize(void) {
 if (g_status != SAVE_DATA_DIALOG_STATUS_NONE) {
  return SAVE_DATA_DIALOG_ERROR_ALREADY_INITIALIZED;
 }
 g_status = SAVE_DATA_DIALOG_STATUS_INITIALIZED;
 g_mode = 0;
 g_user_data = nullptr;
 g_dir_name[0] = '\0';
 return SAVE_DATA_DIALOG_OK;
}

int sceSaveDataDialogGetStatus(void) {
 return g_status;
}

int sceSaveDataDialogUpdateStatus(void) {
 return g_status;
}

int sceSaveDataDialogGetResult(void* result) {
 if (result == nullptr) {
  return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
 }
 auto* r = static_cast<SaveDataDialogResult*>(result);
 r->mode = g_mode;
 r->result = SAVE_DATA_DIALOG_RESULT_OK;
 r->button_id = SAVE_DATA_DIALOG_BUTTON_ID_OK;
 r->user_data = g_user_data;
 if (r->dir_name != nullptr && g_dir_name[0] != '\0') {
  std::snprintf(static_cast<SaveDataDirName*>(r->dir_name)->data,
   sizeof(SaveDataDirName::data), "%s", g_dir_name);
 }
 return SAVE_DATA_DIALOG_OK;
}

int sceSaveDataDialogOpen(const void* param) {
 if (g_status != SAVE_DATA_DIALOG_STATUS_INITIALIZED && g_status != SAVE_DATA_DIALOG_STATUS_FINISHED) {
  return SAVE_DATA_DIALOG_ERROR_INVALID_STATE;
 }
 if (param == nullptr) {
  return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
 }
 const auto* p = static_cast<const SaveDataDialogParam*>(param);
 g_mode = p->mode;
 g_user_data = p->user_data;
 g_dir_name[0] = '\0';
 const auto* items = static_cast<const SaveDataDialogItems*>(p->items);
 if (items != nullptr && items->dir_names != nullptr) {
  for (std::uint32_t i = 0; i < items->dir_names_num; i++) {
   const char* name = items->dir_names[i].data;
   if (name[0] != '\0') {
    std::snprintf(g_dir_name, sizeof(g_dir_name), "%s", name);
    break;
   }
  }
 }
 g_status = SAVE_DATA_DIALOG_STATUS_FINISHED;
 return SAVE_DATA_DIALOG_OK;
}

int sceSaveDataDialogClose(const void* closeParam) {
 (void)closeParam;
 g_status = SAVE_DATA_DIALOG_STATUS_FINISHED;
 return SAVE_DATA_DIALOG_OK;
}

int sceSaveDataDialogIsReadyToDisplay(void) {
 return 1;
}

int sceSaveDataDialogTerminate(void) {
 g_status = SAVE_DATA_DIALOG_STATUS_NONE;
 g_mode = 0;
 g_user_data = nullptr;
 g_dir_name[0] = '\0';
 return SAVE_DATA_DIALOG_OK;
}

int sceSaveDataDialogProgressBarInc(int target, std::uint32_t delta) {
 (void)target;
 (void)delta;
 return SAVE_DATA_DIALOG_OK;
}

int sceSaveDataDialogProgressBarSetValue(int target, std::uint32_t rate) {
 (void)target;
 (void)rate;
 return SAVE_DATA_DIALOG_OK;
}

}
