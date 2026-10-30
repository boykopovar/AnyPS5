#include "prx/libSceMsgDialog/include/MsgDialogParam.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value, const char* what) {
    if (!value) {
        std::fprintf(stderr, "msg_dialog_param: %s\n", what);
        std::abort();
    }
}

struct UserMessage {
    std::int32_t buttonType;
    std::int32_t pad;
    const char* message;
};

struct Param {
    std::uint8_t bytes[MsgDialog::ParamBytes] = {};

    void setMode(std::int32_t mode) { std::memcpy(bytes + 0x38, &mode, 4); }
    void setUserMessage(const UserMessage* user) { std::memcpy(bytes + 0x40, &user, 8); }
};

}

int main() {
    Require(!MsgDialog::ParseParam(nullptr).hasUserMessage, "a null parameter has a user message");
    Require(MsgDialog::ParseParam(nullptr).mode == MsgDialog::ModeUserMsg, "a null parameter is not a user message");

    Param empty;
    auto parsed = MsgDialog::ParseParam(&empty);
    Require(parsed.mode == MsgDialog::ModeUserMsg, "mode 0 is not treated as a user message");
    Require(!parsed.hasUserMessage && !parsed.hasText, "a null user message pointer was read");

    Param outOfRange;
    outOfRange.setMode(9);
    Require(MsgDialog::ParseParam(&outOfRange).mode == MsgDialog::ModeUserMsg, "mode 9 is not treated as a user message");

    for (const int mode : {MsgDialog::ModeProgressBar, MsgDialog::ModeSystemMsg}) {
        UserMessage user{MsgDialog::ButtonTypeYesNo, 0, "ignored"};
        Param other;
        other.setMode(mode);
        other.setUserMessage(&user);
        parsed = MsgDialog::ParseParam(&other);
        Require(parsed.mode == mode, "the mode was not kept");
        Require(!parsed.hasUserMessage && !parsed.hasText, "a user message was read for another mode");
    }

    UserMessage user{MsgDialog::ButtonTypeYesNoFocusNo, 0, "Overwrite the save?\nThis cannot be undone."};
    Param param;
    param.setMode(MsgDialog::ModeUserMsg);
    param.setUserMessage(&user);
    parsed = MsgDialog::ParseParam(&param);
    Require(parsed.mode == MsgDialog::ModeUserMsg, "user message mode");
    Require(parsed.hasUserMessage && parsed.hasText, "user message not read");
    Require(parsed.buttonType == MsgDialog::ButtonTypeYesNoFocusNo, "button type");
    Require(parsed.text == "Overwrite the save?\nThis cannot be undone.", "message text");

    UserMessage noText{MsgDialog::ButtonTypeOkCancel, 0, nullptr};
    param.setUserMessage(&noText);
    parsed = MsgDialog::ParseParam(&param);
    Require(parsed.hasUserMessage && !parsed.hasText, "a null message pointer was read");
    Require(parsed.buttonType == MsgDialog::ButtonTypeOkCancel, "button type without a message");

    static char longText[MsgDialog::MaxMessageBytes + 64];
    std::memset(longText, 'a', sizeof(longText) - 1);
    UserMessage longMessage{MsgDialog::ButtonTypeOk, 0, longText};
    param.setUserMessage(&longMessage);
    Require(MsgDialog::ParseParam(&param).text.size() == MsgDialog::MaxMessageBytes, "a long message was not capped");

    const std::string logged = MsgDialog::LogText(std::string("a\tb\nc") + std::string(200, 'x'));
    Require(logged.size() == 160, "the logged text is not capped at 160");
    Require(logged.compare(0, 5, "a?b\nc") == 0, "control characters are not masked in the log");
}
