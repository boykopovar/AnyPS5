#include "prx/libSceIme/include/ModalDialog.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {

constexpr std::uint32_t Left = 0x0080;
constexpr std::uint32_t Right = 0x0020;
constexpr std::uint32_t Circle = 0x2000;
constexpr std::uint32_t Cross = 0x4000;

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL %s\n", what);
    std::exit(1);
}

void IgnoreText(ImeHostInputKind, const char16_t*, std::size_t) {}

ModalDialogRequest YesNo(std::size_t focus, std::size_t cancel) {
    ModalDialogRequest request;
    request.message = u"Overwrite?";
    request.buttons = {u"Yes", u"No"};
    request.focus = focus;
    request.cancel = cancel;
    return request;
}

ImeHostPad Pad(std::uint32_t buttons, std::uint8_t leftX = 128) {
    ImeHostPad pad;
    pad.buttons = buttons;
    pad.leftX = leftX;
    return pad;
}

std::size_t Chosen(ModalDialogState expected) {
    std::size_t chosen = 7;
    Require(ModalDialogPoll_nid_postfix(chosen) == expected, "unexpected state");
    return chosen;
}

}

int main() {
    Require(!ModalDialogHasPresenter_nid_postfix(), "a presenter is registered at start");
    Require(!ModalDialogOpen_nid_postfix(YesNo(0, 1)), "opened without a presenter");
    Chosen(ModalDialogState::Idle);
    Require(!ModalDialogCapturesInput_nid_postfix(), "captures input while idle");

    ModalDialogSetPresenter_nid_postfix(true);
    Require(ModalDialogHasPresenter_nid_postfix(), "presenter not registered");
    ModalDialogRequest none;
    Require(!ModalDialogOpen_nid_postfix(none), "opened without buttons");
    ModalDialogRequest four = YesNo(0, 1);
    four.buttons = {u"a", u"b", u"c", u"d"};
    Require(!ModalDialogOpen_nid_postfix(four), "opened with four buttons");
    Require(!ModalDialogOpen_nid_postfix(YesNo(2, 1)), "opened with a focus past the buttons");
    Require(!ModalDialogOpen_nid_postfix(YesNo(0, 2)), "opened with a cancel button past the buttons");
    Chosen(ModalDialogState::Idle);

    Require(ModalDialogOpen_nid_postfix(YesNo(0, 1)), "valid request refused");
    Require(!ModalDialogOpen_nid_postfix(YesNo(0, 1)), "second open accepted while showing");
    Chosen(ModalDialogState::Showing);
    Require(ModalDialogCapturesInput_nid_postfix(), "does not capture input while showing");
    ModalDialogView view;
    Require(ModalDialogReadView_nid_postfix(view), "no view while showing");
    Require(view.message == u"Overwrite?" && view.buttons.size() == 2, "view content");
    Require(view.buttons[0].selected && !view.buttons[1].selected && view.buttons[1].label == u"No", "view focus");

    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Previous);
    Require(ModalDialogReadView_nid_postfix(view) && view.buttons[0].selected, "focus moved before the first button");
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Next);
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Next);
    Require(ModalDialogReadView_nid_postfix(view) && view.buttons[1].selected, "focus moved past the last button");
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(Chosen(ModalDialogState::Answered) == 1, "confirm did not choose the focused button");
    Require(!ModalDialogCapturesInput_nid_postfix(), "captures input once answered");
    Require(!ModalDialogReadView_nid_postfix(view), "view after the answer");
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Previous);
    Require(Chosen(ModalDialogState::Answered) == 1, "an answered dialog changed");

    Require(ModalDialogOpen_nid_postfix(YesNo(1, 1)), "reopen refused after an answer");
    Chosen(ModalDialogState::Showing);
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Cancel);
    Require(Chosen(ModalDialogState::Answered) == 1, "cancel did not choose the cancel button");

    ModalDialogRequest noCancel = YesNo(0, ModalDialogNoButton);
    Require(ModalDialogOpen_nid_postfix(noCancel), "request without cancel refused");
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Cancel);
    Chosen(ModalDialogState::Showing);
    ModalDialogClose_nid_postfix();
    Chosen(ModalDialogState::Closed);
    Require(!ModalDialogCapturesInput_nid_postfix(), "captures input once closed");
    ModalDialogReset_nid_postfix();
    Chosen(ModalDialogState::Idle);

    Require(ModalDialogOpen_nid_postfix(YesNo(0, 1)), "reopen refused after a reset");
    ModalDialogPublishPad_nid_postfix(Pad(Cross));
    Chosen(ModalDialogState::Showing);
    ModalDialogPublishPad_nid_postfix(Pad(0));
    ModalDialogPublishPad_nid_postfix(Pad(Right));
    Require(ModalDialogReadView_nid_postfix(view) && view.buttons[1].selected, "D-pad right did not move the focus");
    ModalDialogPublishPad_nid_postfix(Pad(Right));
    ModalDialogPublishPad_nid_postfix(Pad(0));
    ModalDialogPublishPad_nid_postfix(Pad(0, 20));
    Require(ModalDialogReadView_nid_postfix(view) && view.buttons[0].selected, "stick left did not move the focus");
    ModalDialogPublishPad_nid_postfix(Pad(0, 128));
    ModalDialogPublishPad_nid_postfix(Pad(Left));
    Require(ModalDialogReadView_nid_postfix(view) && view.buttons[0].selected, "focus moved before the first button");
    ModalDialogPublishPad_nid_postfix(Pad(0));
    ModalDialogPublishPad_nid_postfix(Pad(Circle));
    Require(Chosen(ModalDialogState::Answered) == 1, "Circle did not cancel");

    Require(ModalDialogOpen_nid_postfix(YesNo(0, 1)), "reopen refused");
    ModalDialogPublishPad_nid_postfix(Pad(Cross | Circle));
    ModalDialogPublishPad_nid_postfix(Pad(Cross | Circle));
    Chosen(ModalDialogState::Showing);
    ModalDialogPublishPad_nid_postfix(Pad(0));
    ModalDialogPublishPad_nid_postfix(Pad(Cross));
    Require(Chosen(ModalDialogState::Answered) == 0, "Cross did not confirm the focused button");

    Require(ModalDialogOpen_nid_postfix(YesNo(0, 1)), "reopen refused before the IME check");
    ImeSetHostTextSink_nid_no_patch(&IgnoreText);
    Require(!ModalDialogCapturesInput_nid_postfix(), "captures input while the IME owns the keyboard");
    Require(!ModalDialogReadView_nid_postfix(view), "view while the IME owns the keyboard");
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Chosen(ModalDialogState::Showing);
    ImeSetHostTextSink_nid_no_patch(nullptr);
    Require(ModalDialogCapturesInput_nid_postfix(), "does not resume after the IME releases the keyboard");
    Require(ModalDialogReadView_nid_postfix(view), "no view after the IME releases the keyboard");
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(Chosen(ModalDialogState::Answered) == 0, "confirm after the IME released the keyboard");

    ModalDialogSetPresenter_nid_postfix(false);
    Require(!ModalDialogOpen_nid_postfix(YesNo(0, 1)), "opened after the presenter went away");

    ModalDialogSetPresenter_nid_postfix(true);
    ModalDialogReset_nid_postfix();
    ModalDialogSetShowTimeout_nid_postfix(40);
    Require(ModalDialogOpen_nid_postfix(YesNo(0, 1)), "reopen refused before the timeout checks");
    Chosen(ModalDialogState::Showing);
    Require(ModalDialogCapturesInput_nid_postfix(), "a new dialog does not capture input before the timeout");
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    Require(!ModalDialogCapturesInput_nid_postfix(), "an undrawn dialog still captures input after the timeout");
    Chosen(ModalDialogState::Unshown);
    Require(!ModalDialogReadView_nid_postfix(view), "an undrawn dialog was drawn after the timeout");
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Chosen(ModalDialogState::Unshown);
    Require(ModalDialogOpen_nid_postfix(YesNo(0, 1)), "an unshown dialog blocks the next one");
    Require(ModalDialogReadView_nid_postfix(view), "a new dialog is not drawn");
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    Require(ModalDialogCapturesInput_nid_postfix(), "a dialog that was drawn stopped capturing input");
    Chosen(ModalDialogState::Showing);
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(Chosen(ModalDialogState::Answered) == 0, "a drawn dialog could not be answered after the timeout");
    ModalDialogReset_nid_postfix();
    ModalDialogSetShowTimeout_nid_postfix(ModalDialogShowTimeoutMilliseconds);

    ModalDialogRequest owned = YesNo(0, 1);
    owned.owner = ModalDialogOwnerMsgDialog;
    Require(ModalDialogOpen_nid_postfix(owned), "an owned dialog was refused");
    std::size_t other = 7;
    Require(ModalDialogPoll_nid_postfix(other, ModalDialogOwnerErrorDialog) == ModalDialogState::Idle && other == ModalDialogNoButton, "another library sees the dialog");
    Require(ModalDialogPoll_nid_postfix(other, ModalDialogOwnerMsgDialog) == ModalDialogState::Showing, "the owner does not see its dialog");
    ModalDialogReset_nid_postfix(ModalDialogOwnerErrorDialog);
    ModalDialogClose_nid_postfix(ModalDialogOwnerErrorDialog);
    Require(ModalDialogPoll_nid_postfix(other, ModalDialogOwnerMsgDialog) == ModalDialogState::Showing && ModalDialogCapturesInput_nid_postfix(), "another library reset or closed the dialog");
    Require(ModalDialogReadView_nid_postfix(view), "the dialog is gone after another library's reset");
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Next);
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(ModalDialogPoll_nid_postfix(other, ModalDialogOwnerMsgDialog) == ModalDialogState::Answered && other == 1, "the owner's answer was lost");
    ModalDialogReset_nid_postfix(ModalDialogOwnerErrorDialog);
    Require(ModalDialogPoll_nid_postfix(other, ModalDialogOwnerMsgDialog) == ModalDialogState::Answered, "another library erased the answer");
    ModalDialogReset_nid_postfix(ModalDialogOwnerMsgDialog);
    Require(ModalDialogPoll_nid_postfix(other, ModalDialogOwnerMsgDialog) == ModalDialogState::Idle, "the owner could not reset its dialog");

    Require(ModalDialogOpen_nid_postfix(owned), "the dialog did not open again");
    ModalDialogClose_nid_postfix(ModalDialogOwnerMsgDialog);
    Require(ModalDialogPoll_nid_postfix(other, ModalDialogOwnerMsgDialog) == ModalDialogState::Closed, "the owner could not close its dialog");
    ModalDialogReset_nid_postfix();
    ModalDialogSetPresenter_nid_postfix(false);
}
