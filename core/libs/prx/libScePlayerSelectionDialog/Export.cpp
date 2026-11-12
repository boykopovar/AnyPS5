#include "prx/libScePlayerSelectionDialog/include/Api.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int COMMON_DIALOG_ERROR_BUSY = static_cast<int>(0x80B80007u);

std::atomic<int> g_status{COMMON_DIALOG_STATUS_NONE};

}

extern "C" {

int APS5_VABI scePlayerSelectionDialogInitialize(void) {
 int expected = COMMON_DIALOG_STATUS_NONE;
 if (!g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_INITIALIZED)) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
 return 0;
}

int APS5_VABI scePlayerSelectionDialogOpen(const PlayerSelectionDialogParam* param) {
 if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 PlayerSelectionDialogParam localParam{};
 const int savedErrno = errno;
 const bool copied = GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam));
 errno = savedErrno;
 if (!copied) return COMMON_DIALOG_ERROR_ARG_NULL;
 if (localParam.base.size != sizeof(localParam.base) || localParam.base.magic != static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(param)) - 1060003575u || !std::all_of(std::begin(localParam.base.reserved), std::end(localParam.base.reserved), [](std::uint8_t value) { return value == 0; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 const int status = g_status.load();
 if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (localParam.size != sizeof(localParam) || localParam.userId == -1) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 UserServiceLoginUserIdList users{};
 if (sceUserServiceGetLoginUserIdList(&users) != 0 || std::find(std::begin(users.user_id), std::end(users.user_id), localParam.userId) == std::end(users.user_id)) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 if (localParam.maxPlayers > 256 || localParam.mode > 3 || localParam.playerCount > 256) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 if (localParam.playerCount != 0 && localParam.players == nullptr) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 for (std::uint32_t index = 0; index < localParam.playerCount; ++index) {
  PlayerSelectionDialogPlayer player{};
  const int savedErrnoPlayer = errno;
  const bool copiedPlayer = GuestCopyIn_nid_no_patch(&player, &localParam.players[index], sizeof(player));
  errno = savedErrnoPlayer;
  if (!copiedPlayer) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
  if (player.accountId == 0 || player.option0 > 1 || player.option1 > 1 || player.mode > 2 || !std::all_of(std::begin(player.reserved), std::end(player.reserved), [](std::uint8_t value) { return value == 0; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
  if (localParam.maxPlayers == 1 && player.option0 == 1) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 }
 if (!std::all_of(std::begin(localParam.reserved), std::end(localParam.reserved), [](std::uint8_t value) { return value == 0; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 if (status == COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_BUSY;
 g_status = COMMON_DIALOG_STATUS_FINISHED;
 return 0;
}

int APS5_VABI scePlayerSelectionDialogClose(void) {
 if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (g_status.load() != COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_NOT_RUNNING;
 g_status = COMMON_DIALOG_STATUS_FINISHED;
 return 0;
}

int APS5_VABI scePlayerSelectionDialogUpdateStatus(void) {
 return g_status.load();
}

int APS5_VABI scePlayerSelectionDialogGetStatus(void) {
 return g_status.load();
}

int APS5_VABI scePlayerSelectionDialogGetResult(PlayerSelectionDialogResult* result) {
 if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 const int status = g_status.load();
 if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
 const std::int32_t canceled = COMMON_DIALOG_RESULT_USER_CANCELED;
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(&result->result, &canceled, sizeof(canceled));
 errno = savedErrno;
 if (!copied) return COMMON_DIALOG_ERROR_ARG_NULL;
 return COMMON_DIALOG_RESULT_USER_CANCELED;
}

int APS5_VABI scePlayerSelectionDialogParamInitialize(PlayerSelectionDialogParam* param) {
 if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 PlayerSelectionDialogParam local{};
 local.base.size = sizeof(local.base);
 local.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param->base)) - 1060003575u;
 local.size = sizeof(local);
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(param, &local, sizeof(local));
 errno = savedErrno;
 if (!copied) return COMMON_DIALOG_ERROR_ARG_NULL;
 return COMMON_DIALOG_OK;
}

int APS5_VABI scePlayerSelectionDialogTerminate(void) {
 if (g_status.exchange(COMMON_DIALOG_STATUS_NONE) == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 return 0;
}

}
