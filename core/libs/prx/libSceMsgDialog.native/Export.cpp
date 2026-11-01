#include "prx/libSceMsgDialog.native/libSceMsgDialog.native.hpp"
#include "prx/libSceMsgDialog/include/MsgDialogRequest.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
std::atomic<int> g_status{0};
std::atomic<bool> g_presented{false};

constexpr int COMMON_DIALOG_STATUS_RUNNING = 2;
constexpr int COMMON_DIALOG_STATUS_FINISHED = 3;
constexpr int COMMON_DIALOG_RESULT_OK = 0;
constexpr int COMMON_DIALOG_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80B80004u);
constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<int>(0x80B80005u);
constexpr int COMMON_DIALOG_ERROR_NOT_RUNNING = static_cast<int>(0x80B8000Bu);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B8000Du);
constexpr int BUTTON_ID_OK = 1;

std::atomic<int> g_buttonId{BUTTON_ID_OK};

int Status() {
    int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_RUNNING && g_presented.load()) {
        std::size_t chosen = 0;
        const ModalDialogState state = ModalDialogPoll_nid_postfix(chosen, ModalDialogOwnerMsgDialog);
        if (state == ModalDialogState::Showing) return status;
        if (state == ModalDialogState::Answered) g_buttonId = MsgDialog::ButtonIdOf(chosen);
        int expected = COMMON_DIALOG_STATUS_RUNNING;
        g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_FINISHED);
        status = g_status.load();
    }
    return status;
}
}

extern "C" {

int SceMsgDialogNativeModuleLoaded_nid_no_patch = 1;

int APS5_VABI sceMsgDialogClose(void) {
    if (Status() != COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_NOT_RUNNING;
    if (g_presented.load()) ModalDialogClose_nid_postfix(ModalDialogOwnerMsgDialog);
    int expected = COMMON_DIALOG_STATUS_RUNNING;
    if (!g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_FINISHED)) return COMMON_DIALOG_ERROR_NOT_RUNNING;
    return 0;
}

int APS5_VABI sceMsgDialogGetResult(MsgDialogResult* result) {
    const int status = Status();
    if (status == 0) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
    *result = MsgDialogResult{};
    result->result = COMMON_DIALOG_RESULT_OK;
    result->button_id = g_buttonId.load();
    return 0;
}

int APS5_VABI sceMsgDialogInitialize(void) {
    int expected = 0;
    if (!g_status.compare_exchange_strong(expected, 1)) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    return 0;
}

int APS5_VABI sceMsgDialogOpen(const void* param) {
    const int status = g_status.load();
    if (status == 0) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (g_presented.load()) ModalDialogClose_nid_postfix(ModalDialogOwnerMsgDialog);
    ModalDialogRequest request;
    bool presented = MsgDialog::BuildRequest(MsgDialog::ParseParam(param), request);
    request.owner = ModalDialogOwnerMsgDialog;
    presented = presented && ModalDialogOpen_nid_postfix(request);
    g_buttonId = BUTTON_ID_OK;
    g_presented = presented;
    g_status = presented ? COMMON_DIALOG_STATUS_RUNNING : COMMON_DIALOG_STATUS_FINISHED;
    return 0;
}

int APS5_VABI sceMsgDialogTerminate(void) {
    if (g_status.exchange(0) == 0) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    g_presented = false;
    ModalDialogReset_nid_postfix(ModalDialogOwnerMsgDialog);
    return 0;
}

int APS5_VABI sceMsgDialogUpdateStatus(void) {
    return Status();
}

int APS5_VABI sceMsgDialogProgressBarInc(int target, std::uint32_t delta) {
    (void)target;
    (void)delta;
    return g_status.load() == COMMON_DIALOG_STATUS_RUNNING ? 0 : COMMON_DIALOG_ERROR_NOT_RUNNING;
}

int APS5_VABI sceMsgDialogProgressBarSetMsg(int target, const char* msg) {
    (void)target;
    (void)msg;
    return g_status.load() == COMMON_DIALOG_STATUS_RUNNING ? 0 : COMMON_DIALOG_ERROR_NOT_RUNNING;
}

int APS5_VABI sceMsgDialogProgressBarSetValue(int target, std::uint32_t rate) {
    (void)target;
    (void)rate;
    return g_status.load() == COMMON_DIALOG_STATUS_RUNNING ? 0 : COMMON_DIALOG_ERROR_NOT_RUNNING;
}

int APS5_VABI sceMsgDialogGetStatus(void) {
    return Status();
}

}
