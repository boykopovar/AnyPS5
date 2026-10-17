#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"
#include "prx/libSceVideoOut/include/ImeTextInput.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
int APS5_VABI sceImeDialogInit(const Param*, const ExtendedParam*);
int APS5_VABI sceImeDialogGetStatus(void);
int APS5_VABI sceImeDialogGetResult(Result*);
int APS5_VABI sceImeDialogAbort(void);
int APS5_VABI sceImeDialogTerm(void);
}

namespace {

constexpr int StatusNone = 0;
constexpr int StatusRunning = 1;
constexpr int StatusFinished = 2;
constexpr std::uint32_t EndStatusOk = 0;
constexpr std::uint32_t EndStatusUserCanceled = 1;
constexpr std::uint32_t EndStatusAborted = 2;
constexpr int ErrorBusy = static_cast<int>(0x80bc0001u);
constexpr int ErrorNotOpened = static_cast<int>(0x80bc0002u);
constexpr int ErrorInvalidMaxTextLength = static_cast<int>(0x80bc0016u);
constexpr int ErrorInvalidInputTextBuffer = static_cast<int>(0x80bc0017u);
constexpr int ErrorInvalidAddress = static_cast<int>(0x80bc0031u);

void Require(bool value) { if (!value) std::abort(); }

constexpr unsigned WindowId = 7;
ImeTextInput g_window;

void Type(const char* utf8) {
    SDL_Event event{};
    event.type = SDL_TEXTINPUT;
    event.text.windowID = WindowId;
    std::strncpy(event.text.text, utf8, sizeof(event.text.text) - 1);
    g_window.HandleEvent(event, WindowId);
}

void Press(SDL_Scancode scancode) {
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.windowID = WindowId;
    event.key.keysym.scancode = scancode;
    g_window.HandleEvent(event, WindowId);
}

Param DialogParam(char16_t* buffer, std::uint32_t maxLength) {
    Param param{};
    param.max_text_length = maxLength;
    param.input_text_buffer = buffer;
    return param;
}

std::uint32_t EndStatus() {
    Result result{};
    Require(sceImeDialogGetResult(&result) == 0);
    return result.endstatus;
}

void Open(const Param& param) {
    Require(sceImeDialogInit(&param, nullptr) == 0);
    Require(sceImeDialogGetStatus() == StatusRunning);
    Require(ImeCapturesKeyboard_nid_postfix());
}

void Close() {
    Require(sceImeDialogTerm() == 0);
    Require(sceImeDialogGetStatus() == StatusNone);
}

}

int main() {
    char16_t buffer[16] = u"Hi";
    const Param param = DialogParam(buffer, 8);
    Require(!ImeCapturesKeyboard_nid_postfix());
    Require(sceImeDialogGetStatus() == StatusNone);
    Require(sceImeDialogTerm() == ErrorNotOpened);

    Open(param);
    for (int poll = 0; poll < 8; ++poll) Require(sceImeDialogGetStatus() == StatusRunning);
    Require(sceImeDialogInit(&param, nullptr) == ErrorBusy);
    Result early{};
    Require(sceImeDialogGetResult(&early) == ErrorNotOpened);
    Type("\xc3\xa9");
    Type("\xf0\x9f\x98\x80");
    Require(std::u16string(buffer) == u"Hi");
    Press(SDL_SCANCODE_RETURN);
    Require(sceImeDialogGetStatus() == StatusFinished);
    Require(!ImeCapturesKeyboard_nid_postfix());
    Require(EndStatus() == EndStatusOk);
    Require(std::u16string(buffer) == u"Hi\u00e9\U0001F600");
    Require(sceImeDialogAbort() == ErrorNotOpened);
    Close();
    Require(sceImeDialogTerm() == ErrorNotOpened);

    buffer[0] = u'\0';
    Open(param);
    Type("abc");
    Press(SDL_SCANCODE_HOME);
    Press(SDL_SCANCODE_DELETE);
    Press(SDL_SCANCODE_END);
    Type("\xf0\x9f\x98\x80");
    Press(SDL_SCANCODE_BACKSPACE);
    Press(SDL_SCANCODE_LEFT);
    Type("X");
    Type("1234");
    Press(SDL_SCANCODE_END);
    Type("\xf0\x9f\x98\x80");
    Type("98");
    Type("Z");
    Press(SDL_SCANCODE_RETURN);
    Require(EndStatus() == EndStatusOk);
    Require(std::u16string(buffer) == u"bX1234c9");
    Close();

    buffer[0] = u'\0';
    Open(param);
    Press(SDL_SCANCODE_RETURN);
    Require(EndStatus() == EndStatusOk);
    Require(buffer[0] == u'\0');
    Close();

    std::memcpy(buffer, u"keep", sizeof(u"keep"));
    Open(param);
    Type("zz");
    Press(SDL_SCANCODE_ESCAPE);
    Require(sceImeDialogGetStatus() == StatusFinished);
    Require(EndStatus() == EndStatusUserCanceled);
    Require(std::u16string(buffer) == u"keep");
    Close();

    Open(param);
    Require(sceImeDialogAbort() == 0);
    Require(sceImeDialogGetStatus() == StatusFinished);
    Require(!ImeCapturesKeyboard_nid_postfix());
    Require(EndStatus() == EndStatusAborted);
    Type("ignored");
    Press(SDL_SCANCODE_RETURN);
    Require(std::u16string(buffer) == u"keep");
    Close();

    Require(sceImeDialogInit(nullptr, nullptr) == ErrorInvalidAddress);
    const Param empty = DialogParam(buffer, 0);
    Require(sceImeDialogInit(&empty, nullptr) == ErrorInvalidMaxTextLength);
    const Param tooLong = DialogParam(buffer, 2049);
    Require(sceImeDialogInit(&tooLong, nullptr) == ErrorInvalidMaxTextLength);
    const Param noBuffer = DialogParam(nullptr, 8);
    Require(sceImeDialogInit(&noBuffer, nullptr) == ErrorInvalidInputTextBuffer);
    Require(sceImeDialogGetStatus() == StatusNone);
    Require(!ImeCapturesKeyboard_nid_postfix());
}
