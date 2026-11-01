#include "prx/libSceNpProfileDialog/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceIme/include/PsnDialogNotice.hpp"

namespace {

constexpr int COMMON_DIALOG_STATUS_NONE = 0;
constexpr int COMMON_DIALOG_STATUS_INITIALIZED = 1;
constexpr int COMMON_DIALOG_STATUS_FINISHED = 3;
constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<int>(0x80B80005u);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B8000Du);
constexpr int COMMON_DIALOG_RESULT_USER_CANCELED = 1;

constexpr std::uint32_t NP_PROFILE_DIALOG_MODE_NORMAL = 1;
constexpr std::uint32_t NP_PROFILE_DIALOG_MODE_GRIEF_REPORT = 4;

static_assert(offsetof(SceCommonDialogBaseParam, magic) == 44);
static_assert(sizeof(SceCommonDialogBaseParam) == 48);

static_assert(offsetof(SceNpProfileDialogParam, size) == 48);
static_assert(offsetof(SceNpProfileDialogParam, mode) == 56);
static_assert(offsetof(SceNpProfileDialogParam, userId) == 60);
static_assert(offsetof(SceNpProfileDialogParam, targetOnlineId) == 64);
static_assert(offsetof(SceNpProfileDialogParam, userData) == 88);
static_assert(sizeof(SceNpProfileDialogParam) == 128);

static_assert(offsetof(SceNpProfileDialogParamA, size) == 48);
static_assert(offsetof(SceNpProfileDialogParamA, mode) == 56);
static_assert(offsetof(SceNpProfileDialogParamA, userId) == 60);
static_assert(offsetof(SceNpProfileDialogParamA, targetAccountId) == 64);
static_assert(offsetof(SceNpProfileDialogParamA, userData) == 80);
static_assert(sizeof(SceNpProfileDialogParamA) == 120);

static_assert(offsetof(SceNpProfileDialogResult, userAction) == 4);
static_assert(offsetof(SceNpProfileDialogResult, userData) == 8);
static_assert(offsetof(SceNpProfileDialogResult, reserved) == 16);
static_assert(sizeof(SceNpProfileDialogResult) == 48);

struct DialogState {
    std::mutex mutex;
    int status = COMMON_DIALOG_STATUS_NONE;
    void* userData = nullptr;
};

DialogState& Dialog() {
    static DialogState state;
    return state;
}

[[noreturn]] void Unsettled(const char* function, const std::string& what) {
    throw std::runtime_error(std::string(function) + ": " + what);
}

template <typename TParam>
int OpenDialog(const TParam* param, const char* function) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    if (dialog.status != COMMON_DIALOG_STATUS_INITIALIZED && dialog.status != COMMON_DIALOG_STATUS_FINISHED) {
        Unsettled(function, "status " + std::to_string(dialog.status) + " is neither INITIALIZED nor FINISHED, and the error for it is unsettled");
    }
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (param->size != sizeof(TParam)) {
        Unsettled(function, "param size " + std::to_string(param->size) + " is not the " + std::to_string(sizeof(TParam)) + "-byte layout this library reads");
    }
    if (param->mode < NP_PROFILE_DIALOG_MODE_NORMAL || param->mode > NP_PROFILE_DIALOG_MODE_GRIEF_REPORT) {
        Unsettled(function, "mode " + std::to_string(param->mode) + " is not a known profile dialog mode");
    }
    dialog.userData = param->userData;
    dialog.status = COMMON_DIALOG_STATUS_FINISHED;
    PsnDialogNoticeOffer_nid_postfix(PsnDialogNoticeKind::PsnFeature);
    return COMMON_DIALOG_OK;
}

}

extern "C" {

int APS5_VABI sceNpProfileDialogInitialize(void) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    if (dialog.status != COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    dialog.status = COMMON_DIALOG_STATUS_INITIALIZED;
    dialog.userData = nullptr;
    return COMMON_DIALOG_OK;
}

int APS5_VABI sceNpProfileDialogTerminate(void) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    if (dialog.status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    dialog.status = COMMON_DIALOG_STATUS_NONE;
    dialog.userData = nullptr;
    return COMMON_DIALOG_OK;
}

int APS5_VABI sceNpProfileDialogUpdateStatus(void) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    return dialog.status;
}

int APS5_VABI sceNpProfileDialogGetStatus(void) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    return dialog.status;
}

int APS5_VABI sceNpProfileDialogOpen(const SceNpProfileDialogParam* param) {
    return OpenDialog(param, __func__);
}

int APS5_VABI sceNpProfileDialogOpenA(const SceNpProfileDialogParamA* param) {
    return OpenDialog(param, __func__);
}

int APS5_VABI sceNpProfileDialogClose(void) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    if (dialog.status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    Unsettled(__func__, "status " + std::to_string(dialog.status) + " is not RUNNING, and the error for closing a dialog that is not running is unsettled");
}

int APS5_VABI sceNpProfileDialogGetResult(SceNpProfileDialogResult* result) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    if (dialog.status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (dialog.status != COMMON_DIALOG_STATUS_FINISHED) {
        if (result == nullptr) Unsettled(__func__, "a null result before the dialog finished has an unsettled error");
        return COMMON_DIALOG_ERROR_NOT_FINISHED;
    }
    if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    *result = SceNpProfileDialogResult{};
    result->result = COMMON_DIALOG_OK;
    result->userAction = COMMON_DIALOG_RESULT_USER_CANCELED;
    result->userData = dialog.userData;
    return COMMON_DIALOG_OK;
}

}
