#include "prx/libSceMsgDialog/include/Api.hpp"
#include "prx/libSceMsgDialog/include/MsgDialogParam.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <thread>

namespace {

constexpr int StatusRunning = 2;
constexpr int StatusFinished = 3;
constexpr int ErrBusy = static_cast<int>(0x80B80007u);
constexpr int ErrNotRunning = static_cast<int>(0x80B8000Bu);

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "msg dialog: %s\n", what);
    std::abort();
}

struct UserMessage {
    std::int32_t buttonType;
    std::int32_t pad;
    const char* message;
};

struct Param {
    std::uint8_t bytes[MsgDialog::ParamBytes] = {};

    Param(std::int32_t mode, const UserMessage* user) {
        std::memcpy(bytes + 0x38, &mode, 4);
        std::memcpy(bytes + 0x40, &user, 8);
    }
};

ModalDialogState Session(std::size_t* chosen = nullptr) {
    std::size_t value = 99;
    const auto state = ModalDialogPoll_nid_postfix(value);
    if (chosen != nullptr) *chosen = value;
    return state;
}

void Headless() {
    UserMessage user{MsgDialog::ButtonTypeYesNo, 0, "Continue?"};
    Param param(MsgDialog::ModeUserMsg, &user);
    Require(!ModalDialogHasPresenter_nid_postfix(), "a presenter is registered");
    Require(sceMsgDialogInitialize() == 0, "initialize");
    Require(sceMsgDialogOpen(&param) == 0, "open without a presenter");
    Require(Session() == ModalDialogState::Idle, "a dialog was shown without a presenter");
    Require(sceMsgDialogGetStatus() == StatusRunning, "first poll without a presenter");
    Require(sceMsgDialogGetStatus() == StatusFinished, "second poll without a presenter");
    Require(sceMsgDialogTerminate() == 0, "terminate");
}

void Shown() {
    ModalDialogSetPresenter_nid_postfix(true);
    UserMessage user{MsgDialog::ButtonTypeYesNo, 0, "Continue?"};
    Param param(MsgDialog::ModeUserMsg, &user);
    Require(sceMsgDialogOpen(&param) == 0, "open with a presenter");
    Require(Session() == ModalDialogState::Showing, "the dialog was not shown");
    ModalDialogView view;
    Require(ModalDialogReadView_nid_postfix(view) && view.message == u"Continue?" && view.buttons.size() == 2, "the shown dialog is not the message");
    for (int poll = 0; poll < 5; ++poll) Require(sceMsgDialogGetStatus() == StatusRunning && sceMsgDialogUpdateStatus() == StatusRunning, "a shown dialog finished before it was answered");
    Require(sceMsgDialogOpen(&param) == ErrBusy, "a second open was accepted while the dialog ran");
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Next);
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(sceMsgDialogGetStatus() == StatusFinished, "an answered dialog did not finish");
    Require(sceMsgDialogClose() == ErrNotRunning, "a finished dialog could be closed");
    Require(sceMsgDialogOpen(&param) == 0 && Session() == ModalDialogState::Showing, "a new dialog was not shown after the answer");
    Require(sceMsgDialogClose() == 0, "close");
    Require(Session() == ModalDialogState::Closed, "close did not close the session");
    Require(sceMsgDialogGetStatus() == StatusFinished, "a closed dialog is still running");
    Require(sceMsgDialogOpen(&param) == 0 && Session() == ModalDialogState::Showing, "a new dialog was not shown after a close");
    Require(sceMsgDialogTerminate() == 0, "terminate");
    Require(Session() == ModalDialogState::Idle, "terminate left the session");
    Require(sceMsgDialogGetStatus() == 0, "status after terminate");
}

void NotPresented() {
    UserMessage none{MsgDialog::ButtonTypeNone, 0, "Wait"};
    Param withoutButtons(MsgDialog::ModeUserMsg, &none);
    Require(sceMsgDialogOpen(&withoutButtons) == 0 && Session() == ModalDialogState::Idle, "a dialog without buttons was shown");
    Require(sceMsgDialogGetStatus() == StatusRunning && sceMsgDialogGetStatus() == StatusFinished, "a dialog without buttons did not finish on the second poll");
    UserMessage user{MsgDialog::ButtonTypeOk, 0, "Progress"};
    Param progress(MsgDialog::ModeProgressBar, &user);
    Require(sceMsgDialogOpen(&progress) == 0 && Session() == ModalDialogState::Idle, "a progress bar was shown");
    Require(sceMsgDialogClose() == 0, "close of the progress bar");
    Param system(MsgDialog::ModeSystemMsg, &user);
    Require(sceMsgDialogOpen(&system) == 0 && Session() == ModalDialogState::Idle, "a system message was shown");
    Require(sceMsgDialogGetStatus() == StatusRunning && sceMsgDialogGetStatus() == StatusFinished, "a system message did not finish on the second poll");
    UserMessage busy{MsgDialog::ButtonTypeOk, 0, "Other"};
    ModalDialogRequest other;
    other.message = u"Other";
    other.buttons = {u"OK"};
    Require(ModalDialogOpen_nid_postfix(other), "the session did not open");
    Param param(MsgDialog::ModeUserMsg, &busy);
    Require(sceMsgDialogOpen(&param) == 0, "open while another dialog is showing");
    Require(sceMsgDialogGetStatus() == StatusRunning && sceMsgDialogGetStatus() == StatusFinished, "a dialog refused by the session did not finish on the second poll");
    ModalDialogReset_nid_postfix();
}

void OtherLibraryDialog() {
    ModalDialogSetPresenter_nid_postfix(true);
    ModalDialogRequest other;
    other.owner = ModalDialogOwnerErrorDialog;
    other.message = u"Error";
    other.buttons = {u"OK"};
    Require(ModalDialogOpen_nid_postfix(other), "the other library's dialog did not open");
    Require(sceMsgDialogInitialize() == 0, "initialize");
    Require(sceMsgDialogTerminate() == 0, "terminate");
    std::size_t chosen = 0;
    Require(ModalDialogPoll_nid_postfix(chosen, ModalDialogOwnerErrorDialog) == ModalDialogState::Showing && ModalDialogCapturesInput_nid_postfix(), "terminating libSceMsgDialog ended another library's dialog");
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(ModalDialogPoll_nid_postfix(chosen, ModalDialogOwnerErrorDialog) == ModalDialogState::Answered && chosen == 0, "the other library's dialog was not answerable");
    ModalDialogReset_nid_postfix();
    ModalDialogSetPresenter_nid_postfix(false);
}

void WithoutFlips() {
    ModalDialogSetPresenter_nid_postfix(true);
    ModalDialogSetShowTimeout_nid_postfix(40);
    UserMessage user{MsgDialog::ButtonTypeYesNoFocusNo, 0, "Continue?"};
    Param param(MsgDialog::ModeUserMsg, &user);
    Require(sceMsgDialogOpen(&param) == 0, "open without flips");
    Require(sceMsgDialogGetStatus() == StatusRunning, "the dialog finished before the bound");
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    Require(sceMsgDialogGetStatus() == StatusFinished, "a dialog that was never drawn kept the title waiting");
    Require(!ModalDialogCapturesInput_nid_postfix() && Session() == ModalDialogState::Unshown, "an undrawn dialog still holds the session");
    Require(sceMsgDialogOpen(&param) == 0 && sceMsgDialogUpdateStatus() == StatusRunning, "a new dialog was refused after the bound");
    ModalDialogView view;
    Require(ModalDialogReadView_nid_postfix(view), "the new dialog was not drawn");
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    Require(sceMsgDialogGetStatus() == StatusRunning, "a drawn dialog finished without an answer");
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(sceMsgDialogGetStatus() == StatusFinished, "a drawn dialog could not be answered after the bound");
    Require(sceMsgDialogTerminate() == 0, "terminate");
    ModalDialogSetShowTimeout_nid_postfix(ModalDialogShowTimeoutMilliseconds);
    ModalDialogSetPresenter_nid_postfix(false);
}

}

int main() {
    Headless();
    Shown();
    NotPresented();
    WithoutFlips();
    OtherLibraryDialog();
    ModalDialogSetPresenter_nid_postfix(false);
}
