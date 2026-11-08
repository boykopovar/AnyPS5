#include "prx/libSceSaveDataDialog.native/include/Api.hpp"
#include "prx/libSceSaveDataDialog/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include "SceTypes.hpp"
#include "prx/libSceSaveDataDialog.native/SaveDataDialog.hpp"
#include "prx/libc/include/General.hpp"

static int g_status = SAVE_DATA_DIALOG_STATUS_NONE;
static int g_mode = 0;
static void* g_user_data = nullptr;
static char g_dir_name[32] = {};

extern "C" {

int APS5_VABI sceSaveDataDialogInitialize(void) {
 if (g_status != SAVE_DATA_DIALOG_STATUS_NONE) {
  return SAVE_DATA_DIALOG_ERROR_ALREADY_INITIALIZED;
 }
 g_status = SAVE_DATA_DIALOG_STATUS_INITIALIZED;
 g_mode = 0;
 g_user_data = nullptr;
 g_dir_name[0] = '\0';
 return SAVE_DATA_DIALOG_OK;
}

int APS5_VABI sceSaveDataDialogGetStatus(void) {
 return g_status;
}

int APS5_VABI sceSaveDataDialogUpdateStatus(void) {
 return g_status;
}

int APS5_VABI sceSaveDataDialogGetResult(void* result) {
 if (g_status != SAVE_DATA_DIALOG_STATUS_FINISHED) {
  return SAVE_DATA_DIALOG_ERROR_NOT_FINISHED;
 }
 if (result == nullptr) {
  return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
 }
 void* dir_name = nullptr;
 const int savedErrno = errno;
 if (!GuestCopyIn_nid_no_patch(&dir_name, static_cast<const char*>(result) + offsetof(SaveDataDialogResult, dir_name), sizeof(dir_name))) {
  errno = savedErrno;
  return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
 }
 struct ResultHeader {
  std::int32_t mode;
  std::int32_t result;
  std::int32_t button_id;
 } header{g_mode, SAVE_DATA_DIALOG_RESULT_OK, SAVE_DATA_DIALOG_BUTTON_ID_OK};
 if (!GuestCopyOut_nid_no_patch(result, &header, sizeof(header))) {
  errno = savedErrno;
  return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
 }
 void* user_data = g_user_data;
 if (!GuestCopyOut_nid_no_patch(static_cast<char*>(result) + offsetof(SaveDataDialogResult, user_data), &user_data, sizeof(user_data))) {
  errno = savedErrno;
  return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
 }
 if (dir_name != nullptr) {
  const std::size_t len = std::strlen(g_dir_name) + 1;
  if (!GuestCopyOut_nid_no_patch(dir_name, g_dir_name, len)) {
   errno = savedErrno;
   return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
  }
 }
 errno = savedErrno;
 return SAVE_DATA_DIALOG_OK;
}

int APS5_VABI sceSaveDataDialogOpen(const void* param) {
 if (g_status != SAVE_DATA_DIALOG_STATUS_INITIALIZED && g_status != SAVE_DATA_DIALOG_STATUS_FINISHED) {
  return SAVE_DATA_DIALOG_ERROR_INVALID_STATE;
 }
 if (param == nullptr) {
  return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
 }
 SaveDataDialogParam localParam{};
 const int savedErrno = errno;
 const bool readParam = GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam));
 errno = savedErrno;
 if (!readParam) {
  return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
 }
 g_mode = localParam.mode;
 g_user_data = localParam.user_data;
 g_dir_name[0] = '\0';
 if (localParam.items != nullptr) {
  SaveDataDialogItems localItems{};
  const bool readItems = GuestCopyIn_nid_no_patch(&localItems, localParam.items, sizeof(localItems));
  errno = savedErrno;
  if (!readItems) {
   return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
  }
  if (localItems.dir_names != nullptr) {
   for (std::uint32_t i = 0; i < localItems.dir_names_num; i++) {
    SaveDataDirName localName{};
    const bool readName = GuestCopyIn_nid_no_patch(&localName, &localItems.dir_names[i], sizeof(localName));
    errno = savedErrno;
    if (!readName) {
     return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
    }
    if (localName.data[0] != '\0') {
     std::snprintf(g_dir_name, sizeof(g_dir_name), "%s", localName.data);
     break;
    }
   }
  }
 }
 g_status = g_mode == SAVE_DATA_DIALOG_MODE_PROGRESS_BAR
  ? SAVE_DATA_DIALOG_STATUS_RUNNING : SAVE_DATA_DIALOG_STATUS_FINISHED;
 return SAVE_DATA_DIALOG_OK;
}

int APS5_VABI sceSaveDataDialogClose(const void* closeParam) {
 (void)closeParam;
 if (g_status != SAVE_DATA_DIALOG_STATUS_RUNNING) {
  return SAVE_DATA_DIALOG_ERROR_NOT_RUNNING;
 }
 g_status = SAVE_DATA_DIALOG_STATUS_FINISHED;
 return SAVE_DATA_DIALOG_OK;
}

int APS5_VABI sceSaveDataDialogIsReadyToDisplay(void) {
 return 1;
}

int APS5_VABI sceSaveDataDialogTerminate(void) {
 if (g_status == SAVE_DATA_DIALOG_STATUS_NONE) {
  return SAVE_DATA_DIALOG_ERROR_NOT_INITIALIZED;
 }
 g_status = SAVE_DATA_DIALOG_STATUS_NONE;
 g_mode = 0;
 g_user_data = nullptr;
 g_dir_name[0] = '\0';
 return SAVE_DATA_DIALOG_OK;
}

int APS5_VABI sceSaveDataDialogProgressBarInc(int target, std::uint32_t delta) {
 (void)delta;
 if (g_status != SAVE_DATA_DIALOG_STATUS_RUNNING) {
  return SAVE_DATA_DIALOG_ERROR_NOT_RUNNING;
 }
 if (target != 0) {
  return SAVE_DATA_DIALOG_ERROR_PARAM;
 }
 return SAVE_DATA_DIALOG_OK;
}

int APS5_VABI sceSaveDataDialogProgressBarSetValue(int target, std::uint32_t rate) {
 if (g_status != SAVE_DATA_DIALOG_STATUS_RUNNING) {
  return SAVE_DATA_DIALOG_ERROR_NOT_RUNNING;
 }
 if (target != 0 || rate > 100) {
  return SAVE_DATA_DIALOG_ERROR_PARAM;
 }
 return SAVE_DATA_DIALOG_OK;
}

}
