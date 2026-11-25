#include "prx/libSceMsgDialog/include/HostMessageBox.hpp"
#include "prx/libSceMsgDialog/include/MsgDialogRequest.hpp"
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {

void Require(bool value, const char* what) {
    if (!value) {
        std::fprintf(stderr, "msg_dialog_request: %s\n", what);
        std::abort();
    }
}

MsgDialog::ParsedParam Parsed(int buttonType, const char* text = "Question?") {
    MsgDialog::ParsedParam parsed;
    parsed.hasUserMessage = true;
    parsed.hasText = true;
    parsed.buttonType = buttonType;
    parsed.text = text;
    return parsed;
}

void CheckHostMessageBoxSpec() {
    ModalDialogRequest empty;
    Require(!MsgDialog::BuildHostMessageBoxSpec("Empty", empty).has_value(), "empty button list was accepted");

    ModalDialogRequest oversized;
    oversized.buttons = {u"One", u"Two", u"Three", u"Four"};
    Require(!MsgDialog::BuildHostMessageBoxSpec("Too many", oversized).has_value(), "oversized button list was accepted");

    ModalDialogRequest outOfRangeFocus;
    outOfRangeFocus.buttons = {u"Yes", u"No"};
    outOfRangeFocus.focus = ModalDialogNoButton;
    outOfRangeFocus.cancel = 1;
    const auto focusSpec = MsgDialog::BuildHostMessageBoxSpec("Continue?", outOfRangeFocus);
    Require(focusSpec.has_value(), "valid button list was rejected");
    Require(focusSpec->buttons.size() == 2, "button count mismatch");
    Require(focusSpec->buttons[0].label == "Yes" && focusSpec->buttons[0].isFocus && !focusSpec->buttons[0].isCancel, "focus did not fall back to button 0");
    Require(focusSpec->buttons[1].label == "No" && !focusSpec->buttons[1].isFocus && focusSpec->buttons[1].isCancel, "cancel button flags mismatch");

    ModalDialogRequest validFocus;
    validFocus.buttons = {u"Yes", u"No"};
    validFocus.focus = 1;
    validFocus.cancel = 1;
    const std::string_view embeddedNul("Hello\0World", 11);
    const auto nulSpec = MsgDialog::BuildHostMessageBoxSpec(embeddedNul, validFocus);
    Require(nulSpec.has_value(), "message with embedded NUL was rejected");
    Require(nulSpec->message == "HelloWorld", "embedded NUL byte was not stripped");
    Require(!nulSpec->buttons[0].isFocus && nulSpec->buttons[1].isFocus && nulSpec->buttons[1].isCancel, "valid focus was not preserved");
}

}

int main() {
    ModalDialogRequest request;
    Require(MsgDialog::BuildRequest(Parsed(MsgDialog::ButtonTypeOk), request), "OK was refused");
    Require(request.message == u"Question?" && request.buttons.size() == 1 && request.buttons[0] == u"OK", "OK buttons");
    Require(request.focus == 0 && request.cancel == 0, "OK focus and cancel");

    Require(MsgDialog::BuildRequest(Parsed(MsgDialog::ButtonTypeYesNo), request), "Yes and No was refused");
    Require(request.buttons.size() == 2 && request.buttons[0] == u"Yes" && request.buttons[1] == u"No", "Yes and No buttons");
    Require(request.focus == 0 && request.cancel == 1, "Yes and No focus and cancel");

    Require(MsgDialog::BuildRequest(Parsed(MsgDialog::ButtonTypeOkCancel), request), "OK and Cancel was refused");
    Require(request.buttons.size() == 2 && request.buttons[0] == u"OK" && request.buttons[1] == u"Cancel", "OK and Cancel buttons");
    Require(request.focus == 0 && request.cancel == 1, "OK and Cancel focus and cancel");

    Require(MsgDialog::BuildRequest(Parsed(MsgDialog::ButtonTypeYesNoFocusNo), request), "Yes and No with focus on No was refused");
    Require(request.buttons.size() == 2 && request.focus == 1 && request.cancel == 1, "Yes and No with focus on No");

    Require(MsgDialog::BuildRequest(Parsed(MsgDialog::ButtonTypeOk, "caf\xC3\xA9"), request) && request.message == u"café", "the message is not decoded");

    const ModalDialogRequest kept = request;
    Require(!MsgDialog::BuildRequest(Parsed(MsgDialog::ButtonTypeNone), request), "a dialog without buttons was presented");
    Require(!MsgDialog::BuildRequest(Parsed(5), request), "an unknown button type was presented");
    Require(!MsgDialog::BuildRequest(Parsed(-1), request), "a negative button type was presented");
    MsgDialog::ParsedParam progress = Parsed(MsgDialog::ButtonTypeOk);
    progress.mode = MsgDialog::ModeProgressBar;
    Require(!MsgDialog::BuildRequest(progress, request), "a progress bar was presented");
    MsgDialog::ParsedParam system = Parsed(MsgDialog::ButtonTypeOk);
    system.mode = MsgDialog::ModeSystemMsg;
    Require(!MsgDialog::BuildRequest(system, request), "a system message was presented");
    MsgDialog::ParsedParam noMessage = Parsed(MsgDialog::ButtonTypeOk);
    noMessage.hasText = false;
    Require(!MsgDialog::BuildRequest(noMessage, request), "a dialog without a message pointer was presented");
    MsgDialog::ParsedParam noUser = Parsed(MsgDialog::ButtonTypeOk);
    noUser.hasUserMessage = false;
    Require(!MsgDialog::BuildRequest(noUser, request), "a dialog without a user message was presented");
    Require(request.message == kept.message && request.buttons == kept.buttons && request.focus == kept.focus, "a refused request changed the output");

    Require(MsgDialog::ButtonIdOf(0) == MsgDialog::ButtonIdOk && MsgDialog::ButtonIdOf(1) == MsgDialog::ButtonIdNo, "button ids");

    CheckHostMessageBoxSpec();
}
