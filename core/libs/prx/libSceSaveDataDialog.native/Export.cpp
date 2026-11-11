#include "prx/libSceSaveDataDialog.native/include/Api.hpp"
#include "prx/libSceSaveDataDialog/include/Api.hpp"
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
 if (!GuestCopyIn_nid_no_patch(&dir_name, static_cast<const char*>(result) + offsetof(SaveDataDialogResult, dir_name), sizeof(dir_name))) {
  return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
 }
 struct ResultHeader {
  std::int32_t mode;
  std::int32_t result;
  std::int32_t button_id;
 } header{g_mode, SAVE_DATA_DIALOG_RESULT_OK, SAVE_DATA_DIALOG_BUTTON_ID_OK};
 if (!GuestCopyOut_nid_no_patch(result, &header, sizeof(header))) {
  return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
 }
 void* user_data = g_user_data;
 if (!GuestCopyOut_nid_no_patch(static_cast<char*>(result) + offsetof(SaveDataDialogResult, user_data), &user_data, sizeof(user_data))) {
  return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
 }
 if (dir_name != nullptr) {
  const std::size_t len = std::strlen(g_dir_name) + 1;
  if (!GuestCopyOut_nid_no_patch(dir_name, g_dir_name, len)) {
   return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
  }
 }
 return SAVE_DATA_DIALOG_OK;
}

int APS5_VABI sceSaveDataDialogOpen(const void* param) {
 if (g_status != SAVE_DATA_DIALOG_STATUS_INITIALIZED && g_status != SAVE_DATA_DIALOG_STATUS_FINISHED) {
  return SAVE_DATA_DIALOG_ERROR_INVALID_STATE;
 }
 if (param == nullptr) {
  return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
 }
 const int savedErrno = errno;
 SaveDataDialogParam p{};
 if (!GuestCopyIn_nid_no_patch(&p, param, sizeof(p))) {
  errno = savedErrno;
  return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
 }
 SaveDataDialogItems items{};
 if (p.items != nullptr && !GuestCopyIn_nid_no_patch(&items, p.items, sizeof(items))) {
  errno = savedErrno;
  return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
 }
 char dir_name[sizeof(g_dir_name)] = {};
 if (items.dir_names != nullptr) {
  for (std::uint32_t i = 0; i < items.dir_names_num; i++) {
   SaveDataDirName name{};
   if (!GuestCopyIn_nid_no_patch(&name, &items.dir_names[i], sizeof(name))) {
    errno = savedErrno;
    return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
   }
   if (name.data[0] != '\0') {
    if (std::memchr(name.data, '\0', sizeof(name.data)) == nullptr) {
     errno = savedErrno;
     return SAVE_DATA_DIALOG_ERROR_ARG_NULL;
    }
    std::snprintf(dir_name, sizeof(dir_name), "%s", name.data);
    break;
   }
  }
 }
 errno = savedErrno;
 g_mode = p.mode;
 g_user_data = p.user_data;
 std::memcpy(g_dir_name, dir_name, sizeof(g_dir_name));
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
