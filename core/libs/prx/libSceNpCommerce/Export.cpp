#include "prx/libSceNpCommerce/include/Api.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include "prx/libc/include/ApplicationHeap.hpp"
#include "SceTypes.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceIme/include/PsnDialogNotice.hpp"

namespace {

int g_status = COMMON_DIALOG_STATUS_NONE;
std::uint64_t g_userData = 0;

std::uint32_t compiledSdkVersion() {
 const auto* process = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
 std::uint32_t version;
 std::memcpy(&version, process + 16, sizeof(version));
 return version;
}

bool isLoggedIn(std::int32_t userId) {
 UserServiceLoginUserIdList users{};
 return sceUserServiceGetLoginUserIdList(&users) == 0 && userId != -1 && std::find(std::begin(users.user_id), std::end(users.user_id), userId) != std::end(users.user_id);
}

bool validateParam(const NpCommerceDialogParam& param) {
 const auto sdkVersion = compiledSdkVersion();
 if (sdkVersion >= 0x01700000 && (param.size != sizeof(param) || !std::all_of(std::begin(param.reserved), std::end(param.reserved), [](std::uint8_t value) { return value == 0; }) || !isLoggedIn(param.userId) || param.option > 7)) return false;
 switch (param.mode) {
 case 0:
  return param.productCount <= 1 && (param.products != nullptr) == (param.productCount != 0);
 case 1:
  return param.productCount == 1 && param.products != nullptr;
 case 2:
  return param.productCount <= 1 && (param.productCount != 0 ? param.products != nullptr : sdkVersion < 0x01700000 || param.products == nullptr);
 case 3:
  return param.productCount >= 1 && param.productCount <= 10 && param.products != nullptr;
 case 4:
  return param.productCount <= 10 && (param.productCount != 0 ? param.products != nullptr : sdkVersion < 0x01700000 || param.products == nullptr);
 case 5:
  return param.productCount == 0 && (sdkVersion >= 0x03500000 ? param.argument == 1 : (param.argument & 3) != 0 && (sdkVersion < 0x01700000 || param.argument < 4));
 default:
  return false;
 }
}

bool validateParam2(const NpCommerceDialogParam2& param) {
 if (!isLoggedIn(param.userId) || param.size != sizeof(param)) return false;
 if (param.option >= 8 && (param.contentMode == 0 || (param.contentMode == 1 && param.option >= 100))) return false;
 if (!std::all_of(std::begin(param.reserved), std::end(param.reserved), [](std::uint8_t value) { return value == 0; })) return false;
 bool valid = false;
 switch (param.mode) {
 case 0:
  valid = param.contentMode < 2 && param.productCount <= 1 && (param.products != nullptr) == (param.productCount != 0);
  break;
 case 1:
  valid = param.contentMode < 2 && param.productCount == 1 && param.products != nullptr;
  break;
 case 2:
  valid = param.productCount <= 1 && (param.products != nullptr) == (param.productCount != 0);
  break;
 case 3:
  valid = param.contentMode < 2 && param.productCount >= 1 && param.productCount <= 10 && param.products != nullptr;
  break;
 case 4:
  valid = param.contentMode < 2 && param.productCount <= 10 && (param.products != nullptr) == (param.productCount != 0);
  break;
 case 5:
  valid = param.productCount == 0 && param.argument == 1;
  break;
 default:
  return false;
 }
 if (!valid) return false;
 if (param.contentMode != 0 && (param.contentMode != 1 || param.mode != 1)) throw std::runtime_error("commerce dialog: extended content process version is not implemented");
 return true;
}

}

extern "C" {

// The store dialog has nothing to show: every open finishes at once as cancelled by the user.
int APS5_VABI sceNpCommerceDialogInitialize() {
 if (g_status != COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
 g_status = COMMON_DIALOG_STATUS_INITIALIZED;
 return 0;
}

int APS5_VABI sceNpCommerceDialogOpen(const NpCommerceDialogParam* param) {
 if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 NpCommerceDialogParam localParam{};
 const int savedErrno = errno;
 const bool copied = GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam));
 errno = savedErrno;
 if (!copied) return COMMON_DIALOG_ERROR_ARG_NULL;
 if (localParam.base.size != sizeof(localParam.base) || localParam.base.magic != static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(param)) - 1060003575u || !std::all_of(std::begin(localParam.base.reserved), std::end(localParam.base.reserved), [](std::uint8_t value) { return value == 0; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 if (g_status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (!validateParam(localParam)) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 g_userData = localParam.userData;
 g_status = COMMON_DIALOG_STATUS_FINISHED;
 PsnDialogNoticeOffer_nid_postfix(PsnDialogNoticeKind::PsnFeature);
 return 0;
}

int APS5_VABI sceNpCommerceDialogUpdateStatus(void) {
 return g_status;
}

int APS5_VABI sceNpCommerceDialogGetStatus(void) {
 return g_status;
}

int APS5_VABI sceNpCommerceDialogGetResult(NpCommerceDialogResult* result) {
 if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 if (g_status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (g_status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
 const std::int32_t cancelResult = COMMON_DIALOG_RESULT_USER_CANCELED;
 const std::uint8_t authorized = 0;
 const std::uint64_t userData = g_userData;
 const int savedErrno = errno;
 if (!GuestCopyOut_nid_no_patch(&result->result, &cancelResult, sizeof(cancelResult)) ||
     !GuestCopyOut_nid_no_patch(&result->authorized, &authorized, sizeof(authorized)) ||
     !GuestCopyOut_nid_no_patch(&result->userData, &userData, sizeof(userData))) {
  errno = savedErrno;
  return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 }
 const auto sdkVersion = compiledSdkVersion();
 if (sdkVersion >= 0x01700000) {
  const std::uint8_t zeroReserved[sizeof(result->reserved)] = {0};
  if (!GuestCopyOut_nid_no_patch(result->reserved, zeroReserved, sizeof(zeroReserved))) {
   errno = savedErrno;
   return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
  }
 }
 errno = savedErrno;
 return COMMON_DIALOG_RESULT_USER_CANCELED;
}

int APS5_VABI sceNpCommerceDialogClose(void) {
 if (g_status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (g_status != COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_NOT_RUNNING;
 g_status = COMMON_DIALOG_STATUS_FINISHED;
 return 0;
}

int APS5_VABI sceNpCommerceDialogTerminate() {
 if (g_status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 g_status = COMMON_DIALOG_STATUS_NONE;
 return 0;
}

int APS5_VABI sceNpCommerceHidePsStoreIcon(void) {
 return 0;
}

int APS5_VABI sceNpCommerceShowPsStoreIcon(int pos) {
 (void)pos;
 return 0;
}

int APS5_VABI sceNpCommerceSetPsStoreIconLayout(int layout) {
 (void)layout;
 return 0;
}

int APS5_VABI sceNpCommerceDialogOpen2(const NpCommerceDialogParam2* param) {
 if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 NpCommerceDialogParam2 localParam{};
 const int savedErrno = errno;
 const bool copied = GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam));
 errno = savedErrno;
 if (!copied) return COMMON_DIALOG_ERROR_ARG_NULL;
 if (localParam.base.size != sizeof(localParam.base) || localParam.base.magic != static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(param)) - 1060003575u || !std::all_of(std::begin(localParam.base.reserved), std::end(localParam.base.reserved), [](std::uint8_t value) { return value == 0; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 if (g_status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (!validateParam2(localParam)) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 g_userData = localParam.userData;
 g_status = COMMON_DIALOG_STATUS_FINISHED;
 PsnDialogNoticeOffer_nid_postfix(PsnDialogNoticeKind::PsnFeature);
 return 0;
}

int APS5_VABI sceNpCommerceDialogInitializeInternal() {
    return sceNpCommerceDialogInitialize();
}

}
