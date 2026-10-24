#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libc/include/General.hpp"

extern "C" int APS5_VABI sceUserServiceGetLoginUserIdList(UserServiceLoginUserIdList* users);

namespace {

constexpr int COMMON_DIALOG_STATUS_NONE = 0;
constexpr int COMMON_DIALOG_STATUS_INITIALIZED = 1;
constexpr int COMMON_DIALOG_STATUS_RUNNING = 2;
constexpr int COMMON_DIALOG_STATUS_FINISHED = 3;
constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<int>(0x80B80005u);
constexpr int COMMON_DIALOG_ERROR_BUSY = static_cast<int>(0x80B80007u);
constexpr int COMMON_DIALOG_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80B8000Au);
constexpr int COMMON_DIALOG_ERROR_NOT_RUNNING = static_cast<int>(0x80B8000Bu);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B8000Du);
constexpr int COMMON_DIALOG_RESULT_USER_CANCELED = 1;

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
 if (!ValidateCommonDialogBaseParam(param->base)) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 const int status = g_status.load();
 if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (param->size != sizeof(*param) || param->userId == -1) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 UserServiceLoginUserIdList users{};
 if (sceUserServiceGetLoginUserIdList(&users) != 0 || std::find(std::begin(users.user_id), std::end(users.user_id), param->userId) == std::end(users.user_id)) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 if (param->maxPlayers > 256 || param->mode > 3 || param->playerCount > 256) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 if (param->playerCount != 0 && param->players == nullptr) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 for (std::uint32_t index = 0; index < param->playerCount; ++index) {
  const auto& player = param->players[index];
  if (player.accountId == 0 || player.option0 > 1 || player.option1 > 1 || player.mode > 2 || !std::all_of(std::begin(player.reserved), std::end(player.reserved), [](std::uint8_t value) { return value == 0; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
  if (param->maxPlayers == 1 && player.option0 == 1) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 }
 if (!std::all_of(std::begin(param->reserved), std::end(param->reserved), [](std::uint8_t value) { return value == 0; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
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
 result->result = COMMON_DIALOG_RESULT_USER_CANCELED;
 return COMMON_DIALOG_RESULT_USER_CANCELED;
}

int APS5_VABI scePlayerSelectionDialogParamInitialize(void* param) {
 if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePlayerSelectionDialogTerminate(void) {
 if (g_status.exchange(COMMON_DIALOG_STATUS_NONE) == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 return 0;
}

}
