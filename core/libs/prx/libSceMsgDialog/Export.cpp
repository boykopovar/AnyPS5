#include "prx/libSceMsgDialog/include/Api.hpp"
#include "prx/libSceMsgDialog/include/MsgDialogParam.hpp"
#include "prx/libSceMsgDialog/include/MsgDialogRequest.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <chrono>

#include "SceTypes.hpp"
#include "HitLog.hpp"
#include "prx/libc/include/General.hpp"

namespace {
constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;
constexpr int kStatusRunning = 2;
constexpr int kStatusFinished = 3;

constexpr int kErrOk = 0;
constexpr int kErrNotInitialized = static_cast<int>(0x80B80003u);
constexpr int kErrBusy = static_cast<int>(0x80B80007u);
constexpr int kErrNotRunning = static_cast<int>(0x80B8000Bu);
constexpr int kErrArgNull = static_cast<int>(0x80B8000Du);

constexpr int kButtonIdOk = MsgDialog::ButtonIdOk;
constexpr int kButtonIdNo = MsgDialog::ButtonIdNo;
constexpr int kModeUserMsg = MsgDialog::ModeUserMsg;
constexpr int kModeProgressBar = MsgDialog::ModeProgressBar;

std::mutex g_lock;
int g_status = kStatusNone;
int g_mode = kModeUserMsg;
int g_buttonId = kButtonIdOk;
int g_pollsLeft = 0;
bool g_presented = false;
std::chrono::steady_clock::time_point g_openedAt;

void Inspect(const MsgDialog::ParsedParam& parsed) {
    g_mode = parsed.mode;
    g_buttonId = kButtonIdOk;
    if (parsed.hasUserMessage && parsed.buttonType == MsgDialog::ButtonTypeYesNoFocusNo) g_buttonId = kButtonIdNo;
}

bool Present(const MsgDialog::ParsedParam& parsed) {
    ModalDialogRequest request;
    if (!MsgDialog::BuildRequest(parsed, request)) return false;
    request.owner = ModalDialogOwnerMsgDialog;
    return ModalDialogOpen_nid_postfix(request);
}

int Poll() {
    std::lock_guard g(g_lock);
    if (g_status == kStatusRunning && g_presented) {
        std::size_t chosen = 0;
        const ModalDialogState state = ModalDialogPoll_nid_postfix(chosen, ModalDialogOwnerMsgDialog);
        if (state == ModalDialogState::Answered) g_buttonId = MsgDialog::ButtonIdOf(chosen);
        if (state != ModalDialogState::Showing) g_status = kStatusFinished;
    } else if (g_status == kStatusRunning) {
        bool done = false;
        if (g_mode == kModeProgressBar) {
            done = std::chrono::steady_clock::now() - g_openedAt > std::chrono::seconds(30);
        } else {
            done = --g_pollsLeft <= 0;
        }
        if (done) g_status = kStatusFinished;
    }
    return g_status;
}
}

extern "C" {

int APS5_VABI sceMsgDialogInitialize(void) {
 std::lock_guard g(g_lock);
 if (g_status == kStatusNone) g_status = kStatusInitialized;
 APS5_HIT("MSGDIALOG", "sceMsgDialogInitialize");
 return kErrOk;
}
int APS5_VABI sceMsgDialogTerminate(void) {
 std::lock_guard g(g_lock);
 const bool was = g_status != kStatusNone;
 g_status = kStatusNone;
 g_presented = false;
 ModalDialogReset_nid_postfix(ModalDialogOwnerMsgDialog);
 return was ? kErrOk : kErrNotInitialized;
}
int APS5_VABI sceMsgDialogOpen(const void* param) {
 if (param == nullptr) return kErrArgNull;
 std::lock_guard g(g_lock);
 if (g_status == kStatusRunning) return kErrBusy;
 const MsgDialog::ParsedParam parsed = MsgDialog::ParseParam(param);
 Inspect(parsed);
 g_presented = Present(parsed);
 if (parsed.hasText) APS5_HIT("MSGDIALOG", "open: buttons=%d text=\"%s\" -> %s", parsed.buttonType, MsgDialog::LogText(parsed.text).c_str(), g_presented ? "shown" : "auto answer");
 g_pollsLeft = 2;
 g_openedAt = std::chrono::steady_clock::now();
 g_status = kStatusRunning;
 return kErrOk;
}
int APS5_VABI sceMsgDialogGetStatus(void) {
 return Poll();
}
int APS5_VABI sceMsgDialogUpdateStatus(void) {
 return Poll();
}
int APS5_VABI sceMsgDialogClose(void) {
 std::lock_guard g(g_lock);
 if (g_status != kStatusRunning) return kErrNotRunning;
 if (g_presented) ModalDialogClose_nid_postfix(ModalDialogOwnerMsgDialog);
 g_status = kStatusFinished;
 return kErrOk;
}
int APS5_VABI sceMsgDialogProgressBarInc(int target, std::uint32_t delta) {
 (void)target;
 (void)delta;
 std::lock_guard g(g_lock);
 return g_status == kStatusRunning ? kErrOk : kErrNotRunning;
}
int APS5_VABI sceMsgDialogProgressBarSetMsg(int target, const char* msg) {
 (void)target;
 (void)msg;
 std::lock_guard g(g_lock);
 return g_status == kStatusRunning ? kErrOk : kErrNotRunning;
}
int APS5_VABI sceMsgDialogProgressBarSetValue(int target, std::uint32_t rate) {
 (void)target;
 (void)rate;
 std::lock_guard g(g_lock);
 return g_status == kStatusRunning ? kErrOk : kErrNotRunning;
}
}
