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

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

void Paste(const std::string& utf8) {
    ImePublishPaste_nid_postfix(utf8.data(), utf8.size());
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

void Open(const Param& param) {
    Require(sceImeDialogInit(&param, nullptr) == 0, "init");
    Require(sceImeDialogGetStatus() == StatusRunning, "not running");
}

void Close() {
    Require(sceImeDialogTerm() == 0, "term");
}

void CheckInsertAtCaret() {
    char16_t buffer[16] = u"ab";
    const Param param = DialogParam(buffer, 8);
    Open(param);
    Publish(ImeHostInputKind::Left);
    Paste("XY");
    auto view = View();
    Require(view.text == u"aXYb" && view.caret == 3, "a paste lands at the caret and moves it");
    Require(std::u16string(buffer) == u"ab", "a paste reached the buffer before the dialog was accepted");
    Close();
}

void CheckLineBreaksAndControls() {
    char16_t buffer[16] = u"";
    const Param param = DialogParam(buffer, 16);
    Open(param);
    Paste(std::string("a\r\nb\nc\td\x01" "e\x7f" "f", 13));
    Require(View().text == u"abcdef", "line breaks and controls are not removed from a paste");
    Close();
}

void CheckMaxLength() {
    char16_t buffer[16] = u"abc";
    const Param param = DialogParam(buffer, 5);
    Open(param);
    Paste("d\xf0\x9f\x98\x80");
    Require(View().text == u"abcd", "a paste split a surrogate pair at the limit");
    Paste("ef");
    Require(View().text == u"abcde", "a paste went past the maximum length");
    Paste("g");
    Require(View().text == u"abcde", "a paste into a full dialog changed it");
    Close();

    char16_t wide[16] = u"";
    const Param roomForPair = DialogParam(wide, 4);
    Open(roomForPair);
    Paste("x\xf0\x9f\x98\x80\xf0\x9f\x98\x80");
    Require(View().text == u"x\U0001F600", "a pair that fits is not kept whole");
    Close();
}

void CheckHugeClipboard() {
    char16_t buffer[2049] = u"";
    const Param param = DialogParam(buffer, 2048);
    Open(param);
    const std::string huge(8u * 1024u * 1024u, 'x');
    Paste(huge);
    Require(View().text.size() == 2048, "a huge clipboard did not fill the dialog to its limit exactly");
    Close();
}

void CheckInvalidUtf8() {
    char16_t buffer[16] = u"";
    const Param param = DialogParam(buffer, 16);
    Open(param);
    Paste("a\xff" "b\xe3\x81");
    Require(View().text == u"a\uFFFDb\uFFFD", "an invalid clipboard is not replaced");
    Close();
}

void CheckNothingCaptured() {
    Require(!ImeCapturesKeyboard_nid_postfix(), "something is capturing");
    Paste("ignored");
    char16_t buffer[16] = u"";
    const Param param = DialogParam(buffer, 16);
    Open(param);
    Require(View().text.empty(), "a paste before init was kept");
    Close();
    Paste("ignored");
    Open(param);
    Require(View().text.empty(), "a paste after term was kept");
    Require(sceImeDialogAbort() == 0, "abort");
    Paste("ignored");
    ImeKeyboardView view;
    Require(!ImeReadKeyboardView_nid_postfix(view), "a finished dialog shows a keyboard");
    Close();
}

void CheckPasteWhileComposing() {
    char16_t buffer[16] = u"ab";
    const Param param = DialogParam(buffer, 16);
    Open(param);
    Publish(ImeHostInputKind::Preedit, "ni", 2, 0);
    Paste("XY");
    Require(View().text == u"ab", "a paste went in while a composition was shown");
    Publish(ImeHostInputKind::Preedit, "");
    Paste("XY");
    Require(View().text == u"abXY", "a paste does not work once the composition ends");
    Close();
}

}

int main() {
    CheckNothingCaptured();
    CheckInsertAtCaret();
    CheckLineBreaksAndControls();
    CheckMaxLength();
    CheckHugeClipboard();
    CheckInvalidUtf8();
    CheckPasteWhileComposing();
}
