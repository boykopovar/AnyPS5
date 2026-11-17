#include "prx/libSceImeDialog/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"
#include "prx/libSceIme/include/ImeKeyboardView.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr int StatusRunning = 1;
constexpr int StatusFinished = 2;
constexpr std::uint32_t EndStatusOk = 0;
constexpr std::uint32_t EndStatusUserCanceled = 1;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

void Publish(ImeHostInputKind kind, const char* text = "", std::int32_t start = 0, std::int32_t length = 0) {
    ImeHostInput input{};
    input.kind = kind;
    std::strncpy(input.text, text, sizeof(input.text) - 1);
    input.start = start;
    input.length = length;
    ImePublishInput_nid_postfix(input);
}

Param DialogParam(char16_t* buffer, std::uint32_t maxLength) {
    Param param{};
    param.max_text_length = maxLength;
    param.input_text_buffer = buffer;
    return param;
}

ImeKeyboardView View() {
    ImeKeyboardView view;
    Require(ImeReadKeyboardView_nid_postfix(view), "the dialog shows no keyboard");
    return view;
}

std::uint32_t EndStatus() {
    Result result{};
    Require(sceImeDialogGetResult(&result) == 0, "no result");
    return result.endstatus;
}

void Open(const Param& param) {
    Require(sceImeDialogInit(&param, nullptr) == 0, "init");
    Require(sceImeDialogGetStatus() == StatusRunning, "not running");
}

void Close() {
    Require(sceImeDialogTerm() == 0, "term");
}

void CheckLifecycle() {
    char16_t buffer[16] = u"ab";
    const Param param = DialogParam(buffer, 8);
    Open(param);

    Require(View().preedit.empty(), "a fresh dialog shows a composition");
    Publish(ImeHostInputKind::Preedit, "\xe3\x81\x93", 1, 0);
    auto view = View();
    Require(view.preedit == u"\u3053" && view.preeditCursor == 1 && view.preeditSelection == 0, "the first edit is shown");
    Require(view.text == u"ab" && view.caret == 2, "the preedit entered the committed text");
    Publish(ImeHostInputKind::Preedit, "\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab", 1, 2);
    view = View();
    Require(view.preedit == u"\u3053\u3093\u306B" && view.preeditCursor == 1 && view.preeditSelection == 2, "the second edit replaces the first and keeps the selection");
    Require(view.text == u"ab", "a second edit entered the committed text");
    Require(std::u16string(buffer) == u"ab", "a preedit reached the guest buffer");

    Publish(ImeHostInputKind::Text, "\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab");
    view = View();
    Require(view.preedit.empty() && view.preeditCursor == 0 && view.preeditSelection == 0, "a commit left the preedit shown");
    Require(view.text == u"ab\u3053\u3093\u306B" && view.caret == 5, "a commit is inserted at the caret");
    Require(std::u16string(buffer) == u"ab", "the title sees text before the dialog is accepted");

    Publish(ImeHostInputKind::Preedit, "ni", 2, 0);
    Publish(ImeHostInputKind::Enter);
    Require(sceImeDialogGetStatus() == StatusRunning, "Enter while composing accepted the dialog");
    Require(View().preedit.empty(), "Enter did not discard the composition");
    Publish(ImeHostInputKind::Enter);
    Require(sceImeDialogGetStatus() == StatusFinished && EndStatus() == EndStatusOk, "the next Enter does not accept the dialog");
    Require(std::u16string(buffer) == u"ab\u3053\u3093\u306B", "a discarded preedit leaked into the accepted text");
    Close();
}

void CheckCancel() {
    char16_t buffer[16] = u"keep";
    const Param param = DialogParam(buffer, 8);
    Open(param);
    Publish(ImeHostInputKind::Preedit, "ni", 2, 0);
    Publish(ImeHostInputKind::Escape);
    Require(sceImeDialogGetStatus() == StatusRunning && View().preedit.empty(), "Escape while composing cancelled the dialog or kept the composition");
    Publish(ImeHostInputKind::Escape);
    Require(sceImeDialogGetStatus() == StatusFinished && EndStatus() == EndStatusUserCanceled, "the next Escape does not cancel the dialog");
    Require(std::u16string(buffer) == u"keep", "a cancelled dialog changed the buffer");
    Close();
}

void CheckEditKeysAndPad() {
    char16_t buffer[16] = u"abc";
    const Param param = DialogParam(buffer, 8);
    Open(param);
    Publish(ImeHostInputKind::Preedit, "ni", 2, 0);
    for (const auto kind : {ImeHostInputKind::Backspace, ImeHostInputKind::Delete, ImeHostInputKind::Left, ImeHostInputKind::Right, ImeHostInputKind::Home, ImeHostInputKind::End}) Publish(kind);
    auto view = View();
    Require(view.text == u"abc" && view.caret == 3 && view.preedit == u"ni", "an editing key acted while composing");
    Publish(ImeHostInputKind::Preedit, "");
    Publish(ImeHostInputKind::Left);
    Publish(ImeHostInputKind::Backspace);
    view = View();
    Require(view.text == u"ac" && view.caret == 1, "editing keys do not work after the composition ends");
    Close();
}

void CheckClosing() {
    char16_t buffer[16] = u"ab";
    const Param param = DialogParam(buffer, 8);
    Open(param);
    Publish(ImeHostInputKind::Preedit, "ni", 2, 0);
    Require(sceImeDialogAbort() == 0, "abort");
    ImeKeyboardView view;
    Require(!ImeReadKeyboardView_nid_postfix(view), "a finished dialog still shows a keyboard");
    Close();

    Open(param);
    Require(View().preedit.empty(), "a composition survived into the next dialog");
    Publish(ImeHostInputKind::Preedit, "ni", 2, 0);
    Require(sceImeDialogForceClose() == 0, "force close");
    Publish(ImeHostInputKind::Preedit, "late", 0, 0);
    Open(param);
    Require(View().preedit.empty(), "a preedit after the close reached the next dialog");
    Publish(ImeHostInputKind::Backspace);
    Require(View().text == u"a", "a composition after the close froze the next dialog");
    Close();

    Publish(ImeHostInputKind::Preedit, "late", 0, 0);
    Open(param);
    Require(View().preedit.empty(), "a preedit before init reached the dialog");
    Close();
}

void CheckSelectionAndCaretOffsets() {
    char16_t buffer[16] = u"";
    const Param param = DialogParam(buffer, 8);
    Open(param);
    Publish(ImeHostInputKind::Preedit, "a\xf0\x9f\x98\x80z", 2, 1);
    auto view = View();
    Require(view.preedit == u"a\U0001F600z" && view.preeditCursor == 3 && view.preeditSelection == 1, "offsets count a surrogate pair as one character");
    Publish(ImeHostInputKind::Preedit, "abc", 40, 40);
    view = View();
    Require(view.preeditCursor == 3 && view.preeditSelection == 0, "offsets past the end are clamped");
    Publish(ImeHostInputKind::Preedit, "abc", -2, -2);
    view = View();
    Require(view.preeditCursor == 0 && view.preeditSelection == 0, "negative offsets are zero");
    Close();
}

void CheckMalformedPreedit() {
    char16_t buffer[16] = u"";
    const Param param = DialogParam(buffer, 8);
    Open(param);
    ImeHostInput input{};
    input.kind = ImeHostInputKind::Preedit;
    std::memset(input.text, 'a', sizeof(input.text));
    input.text[30] = '\xe3';
    input.text[31] = '\x81';
    ImePublishInput_nid_postfix(input);
    Require(View().preedit == std::u16string(30, u'a'), "a preedit cut inside a character shows half of it");
    Publish(ImeHostInputKind::Preedit, "\xff\x01" "b", 0, 0);
    Require(View().preedit == u"\uFFFDb", "an invalid byte is not replaced and a control character is not removed");
    Close();
}

}

int main() {
    CheckLifecycle();
    CheckCancel();
    CheckEditKeysAndPad();
    CheckClosing();
    CheckSelectionAndCaretOffsets();
    CheckMalformedPreedit();
}
