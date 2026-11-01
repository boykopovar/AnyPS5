#include "prx/libSceMsgDialog.native/libSceMsgDialog.native.hpp"
#include "prx/libSceMsgDialog/include/MsgDialogParam.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <thread>

extern "C" {
extern int SceMsgDialogNativeModuleLoaded_nid_no_patch;

}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kStatusInitialized = 1;
constexpr int kStatusFinished = 3;
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80B80004u);
constexpr int kErrNotInitialized = static_cast<int>(0x80B80003u);
constexpr int kErrNotFinished = static_cast<int>(0x80B80005u);
constexpr int kErrNotRunning = static_cast<int>(0x80B8000Bu);
constexpr int kErrArgNull = static_cast<int>(0x80B8000Du);
constexpr int kButtonIdOk = 1;
constexpr int kButtonIdNo = 2;
constexpr int kStatusRunning = 2;

struct UserMessage {
    std::int32_t buttonType;
    std::int32_t pad;
    const char* message;
};

struct UserParam {
    std::uint8_t bytes[MsgDialog::ParamBytes] = {};

    UserParam(std::int32_t mode, const UserMessage* user) {
        std::memcpy(bytes + 0x38, &mode, 4);
        std::memcpy(bytes + 0x40, &user, 8);
    }
};

ModalDialogState Session() {
    std::size_t chosen = 0;
    return ModalDialogPoll_nid_postfix(chosen);
}

void CancelAndFallbacks() {
    MsgDialogResult result{};
    UserMessage yesNo{MsgDialog::ButtonTypeYesNo, 0, "Continue?"};
    UserParam param(MsgDialog::ModeUserMsg, &yesNo);
    ModalDialogSetPresenter_nid_postfix(true);
    Require(sceMsgDialogInitialize() == 0);
    Require(sceMsgDialogOpen(&param) == 0);
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Cancel);
    Require(sceMsgDialogGetStatus() == kStatusFinished);
    Require(sceMsgDialogGetResult(&result) == 0 && result.button_id == kButtonIdNo);

    UserMessage okOnly{MsgDialog::ButtonTypeOk, 0, "Saved"};
    UserParam okParam(MsgDialog::ModeUserMsg, &okOnly);
    Require(sceMsgDialogOpen(&okParam) == 0 && sceMsgDialogGetStatus() == kStatusRunning);
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Cancel);
    Require(sceMsgDialogGetStatus() == kStatusFinished);
    Require(sceMsgDialogGetResult(&result) == 0 && result.button_id == kButtonIdOk);

    UserMessage none{MsgDialog::ButtonTypeNone, 0, "Wait"};
    UserParam noneParam(MsgDialog::ModeUserMsg, &none);
    Require(sceMsgDialogOpen(&noneParam) == 0 && sceMsgDialogGetStatus() == kStatusFinished && Session() != ModalDialogState::Showing);
    UserParam progress(MsgDialog::ModeProgressBar, &yesNo);
    Require(sceMsgDialogOpen(&progress) == 0 && sceMsgDialogGetStatus() == kStatusFinished && Session() != ModalDialogState::Showing);
    UserParam system(MsgDialog::ModeSystemMsg, &yesNo);
    Require(sceMsgDialogOpen(&system) == 0 && sceMsgDialogGetStatus() == kStatusFinished && Session() != ModalDialogState::Showing);
    result = MsgDialogResult{};
    Require(sceMsgDialogGetResult(&result) == 0 && result.button_id == kButtonIdOk);

    ModalDialogSetPresenter_nid_postfix(false);
    UserMessage focusNo{MsgDialog::ButtonTypeYesNoFocusNo, 0, "Delete?"};
    UserParam focusParam(MsgDialog::ModeUserMsg, &focusNo);
    Require(sceMsgDialogOpen(&focusParam) == 0 && sceMsgDialogGetStatus() == kStatusFinished && Session() != ModalDialogState::Showing);
    result = MsgDialogResult{};
    Require(sceMsgDialogGetResult(&result) == 0 && result.button_id == kButtonIdOk);
    Require(sceMsgDialogTerminate() == 0);
}

void CloseAndTerminate() {
    MsgDialogResult result{};
    UserMessage yesNo{MsgDialog::ButtonTypeYesNo, 0, "Continue?"};
    UserParam param(MsgDialog::ModeUserMsg, &yesNo);
    ModalDialogSetPresenter_nid_postfix(true);
    Require(sceMsgDialogInitialize() == 0);
    Require(sceMsgDialogOpen(&param) == 0 && Session() == ModalDialogState::Showing);
    Require(sceMsgDialogClose() == 0);
    Require(Session() == ModalDialogState::Closed);
    Require(sceMsgDialogGetStatus() == kStatusFinished);
    Require(sceMsgDialogClose() == kErrNotRunning);
    Require(sceMsgDialogGetResult(&result) == 0 && result.button_id == kButtonIdOk);

    Require(sceMsgDialogOpen(&param) == 0 && Session() == ModalDialogState::Showing);
    UserMessage okOnly{MsgDialog::ButtonTypeOk, 0, "Saved"};
    UserParam okParam(MsgDialog::ModeUserMsg, &okOnly);
    Require(sceMsgDialogOpen(&okParam) == 0 && Session() == ModalDialogState::Showing);
    ModalDialogView view;
    Require(ModalDialogReadView_nid_postfix(view) && view.message == u"Saved" && view.buttons.size() == 1);
    Require(sceMsgDialogGetStatus() == kStatusRunning);
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(sceMsgDialogGetStatus() == kStatusFinished);

    Require(sceMsgDialogOpen(&param) == 0 && Session() == ModalDialogState::Showing);
    Require(sceMsgDialogTerminate() == 0);
    Require(Session() == ModalDialogState::Idle);
    Require(!ModalDialogCapturesInput_nid_postfix());
    ModalDialogSetPresenter_nid_postfix(false);
}

void OtherLibraryDialog() {
    ModalDialogSetPresenter_nid_postfix(true);
    ModalDialogRequest other;
    other.owner = ModalDialogOwnerErrorDialog;
    other.message = u"Error";
    other.buttons = {u"OK"};
    Require(ModalDialogOpen_nid_postfix(other));
    Require(sceMsgDialogInitialize() == 0);
    Require(sceMsgDialogTerminate() == 0);
    std::size_t chosen = 0;
    Require(ModalDialogPoll_nid_postfix(chosen, ModalDialogOwnerErrorDialog) == ModalDialogState::Showing && ModalDialogCapturesInput_nid_postfix());
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(ModalDialogPoll_nid_postfix(chosen, ModalDialogOwnerErrorDialog) == ModalDialogState::Answered && chosen == 0);

    UserMessage yesNo{MsgDialog::ButtonTypeYesNo, 0, "Continue?"};
    UserParam param(MsgDialog::ModeUserMsg, &yesNo);
    ModalDialogReset_nid_postfix();
    Require(sceMsgDialogInitialize() == 0);
    Require(sceMsgDialogOpen(&param) == 0 && Session() == ModalDialogState::Showing);
    ModalDialogReset_nid_postfix(ModalDialogOwnerErrorDialog);
    Require(Session() == ModalDialogState::Showing && sceMsgDialogGetStatus() == kStatusRunning);
    Require(sceMsgDialogTerminate() == 0);
    Require(Session() == ModalDialogState::Idle);
    ModalDialogSetPresenter_nid_postfix(false);
}

void WithoutFlips() {
    MsgDialogResult result{};
    UserMessage focusNo{MsgDialog::ButtonTypeYesNoFocusNo, 0, "Delete?"};
    UserParam param(MsgDialog::ModeUserMsg, &focusNo);
    ModalDialogSetPresenter_nid_postfix(true);
    ModalDialogSetShowTimeout_nid_postfix(40);
    Require(sceMsgDialogInitialize() == 0);
    Require(sceMsgDialogOpen(&param) == 0);
    Require(sceMsgDialogGetStatus() == kStatusRunning);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    Require(sceMsgDialogGetStatus() == kStatusFinished);
    Require(sceMsgDialogGetResult(&result) == 0 && result.button_id == kButtonIdOk);
    Require(!ModalDialogCapturesInput_nid_postfix() && Session() == ModalDialogState::Unshown);

    Require(sceMsgDialogOpen(&param) == 0 && sceMsgDialogGetStatus() == kStatusRunning);
    ModalDialogView view;
    Require(ModalDialogReadView_nid_postfix(view));
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    Require(sceMsgDialogUpdateStatus() == kStatusRunning);
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(sceMsgDialogUpdateStatus() == kStatusFinished);
    Require(sceMsgDialogGetResult(&result) == 0 && result.button_id == kButtonIdNo);
    Require(sceMsgDialogTerminate() == 0);
    ModalDialogSetShowTimeout_nid_postfix(ModalDialogShowTimeoutMilliseconds);
    ModalDialogSetPresenter_nid_postfix(false);
}

void ShownDialog() {
    UserMessage yesNo{MsgDialog::ButtonTypeYesNo, 0, "Continue?"};
    UserParam param(MsgDialog::ModeUserMsg, &yesNo);
    MsgDialogResult result{};
    ModalDialogSetPresenter_nid_postfix(true);
    Require(sceMsgDialogInitialize() == 0);
    Require(sceMsgDialogOpen(&param) == 0);
    Require(Session() == ModalDialogState::Showing);
    for (int poll = 0; poll < 3; ++poll) Require(sceMsgDialogUpdateStatus() == kStatusRunning && sceMsgDialogGetStatus() == kStatusRunning);
    Require(sceMsgDialogGetResult(&result) == kErrNotFinished);
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Next);
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(sceMsgDialogGetStatus() == kStatusFinished);
    result = MsgDialogResult{};
    Require(sceMsgDialogGetResult(&result) == 0 && result.result == 0 && result.button_id == kButtonIdNo);

    Require(sceMsgDialogOpen(&param) == 0 && sceMsgDialogGetStatus() == kStatusRunning);
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(sceMsgDialogUpdateStatus() == kStatusFinished);
    Require(sceMsgDialogGetResult(&result) == 0 && result.button_id == kButtonIdOk);

    UserMessage focusNo{MsgDialog::ButtonTypeYesNoFocusNo, 0, "Delete?"};
    UserParam focusParam(MsgDialog::ModeUserMsg, &focusNo);
    Require(sceMsgDialogOpen(&focusParam) == 0);
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(sceMsgDialogGetStatus() == kStatusFinished);
    Require(sceMsgDialogGetResult(&result) == 0 && result.button_id == kButtonIdNo);
    Require(sceMsgDialogTerminate() == 0);
    ModalDialogSetPresenter_nid_postfix(false);
}

}

int main() {
    Require(SceMsgDialogNativeModuleLoaded_nid_no_patch == 1);
    std::uint8_t param[0x60]{};
    MsgDialogResult result{};

    Require(sceMsgDialogTerminate() == kErrNotInitialized);
    Require(sceMsgDialogClose() == kErrNotRunning);
    Require(sceMsgDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceMsgDialogOpen(param) == kErrNotInitialized);
    Require(sceMsgDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceMsgDialogGetResult(&result) == kErrNotInitialized);
    Require(sceMsgDialogUpdateStatus() == 0);
    Require(sceMsgDialogGetStatus() == 0);

    Require(sceMsgDialogInitialize() == 0);
    Require(sceMsgDialogInitialize() == kErrAlreadyInitialized);
    Require(sceMsgDialogClose() == kErrNotRunning);
    Require(sceMsgDialogGetResult(&result) == kErrNotFinished);
    Require(sceMsgDialogGetResult(nullptr) == kErrArgNull);
    Require(sceMsgDialogOpen(nullptr) == kErrArgNull);
    Require(sceMsgDialogUpdateStatus() == kStatusInitialized);
    Require(sceMsgDialogGetStatus() == kStatusInitialized);

    Require(sceMsgDialogProgressBarSetValue(0, 50) == kErrNotRunning);
    Require(sceMsgDialogOpen(param) == 0);
    Require(sceMsgDialogUpdateStatus() == kStatusFinished);
    Require(sceMsgDialogGetStatus() == kStatusFinished);
    Require(sceMsgDialogOpen(nullptr) == kErrArgNull);
    Require(sceMsgDialogOpen(param) == 0);
    Require(sceMsgDialogProgressBarInc(0, 10) == kErrNotRunning);
    Require(sceMsgDialogProgressBarSetMsg(0, "progress") == kErrNotRunning);
    Require(sceMsgDialogProgressBarSetValue(0, 100) == kErrNotRunning);
    Require(sceMsgDialogClose() == kErrNotRunning);
    Require(sceMsgDialogUpdateStatus() == kStatusFinished);
    Require(sceMsgDialogGetStatus() == kStatusFinished);

    result = MsgDialogResult{};
    result.button_id = -1;
    Require(sceMsgDialogGetResult(&result) == 0);
    Require(result.result == 0);
    Require(result.button_id == kButtonIdOk);

    Require(sceMsgDialogTerminate() == 0);
    Require(sceMsgDialogTerminate() == kErrNotInitialized);
    Require(sceMsgDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceMsgDialogOpen(param) == kErrNotInitialized);
    Require(sceMsgDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceMsgDialogGetResult(&result) == kErrNotInitialized);
    Require(sceMsgDialogClose() == kErrNotRunning);
    Require(sceMsgDialogGetStatus() == 0);

    ShownDialog();
    CancelAndFallbacks();
    CloseAndTerminate();
    WithoutFlips();
    OtherLibraryDialog();
}
