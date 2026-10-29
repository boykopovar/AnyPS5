#include "prx/libSceImeDialog/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"
#include "prx/libSceIme/include/ImeKeyboardView.hpp"
#include "prx/libSceVideoOut/include/ImeTextInput.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr int StatusNone = 0;
constexpr int StatusRunning = 1;
constexpr int StatusFinished = 2;
constexpr std::uint32_t EndStatusOk = 0;
constexpr std::uint32_t EndStatusUserCanceled = 1;
constexpr std::uint32_t EndStatusAborted = 2;
constexpr int ErrorBusy = static_cast<int>(0x80bc0001u);
constexpr int ErrorSuspending = static_cast<int>(0x80bc0009u);
constexpr int ErrorNotOpened = static_cast<int>(0x80bc0002u);
constexpr int ErrorNotInUse = static_cast<int>(0x80bc0107u);
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

constexpr std::uint32_t PadCross = 0x4000;
constexpr std::uint32_t PadCircle = 0x2000;
constexpr std::uint32_t PadTriangle = 0x1000;
constexpr std::uint32_t PadSquare = 0x8000;
constexpr std::uint32_t PadR2 = 0x0200;
constexpr std::uint32_t PadL1 = 0x0400;
constexpr std::uint32_t PadRight = 0x0020;

void Pad(std::uint32_t buttons) {
    ImePublishPad_nid_postfix({buttons, 128, 128, 0});
}

void Tap(std::uint32_t button) {
    Pad(button);
    Pad(0);
}

std::u16string SelectedKey() {
    ImeKeyboardView view;
    Require(ImeReadKeyboardView_nid_postfix(view));
    for (const auto& key : view.keys) {
        if (key.selected) return key.label;
    }
    return {};
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

    Require(sceImeDialogForceClose() == ErrorNotInUse);
    Open(param);
    Type("zz");
    Require(sceImeDialogForceClose() == 0);
    Require(sceImeDialogGetStatus() == StatusNone);
    Require(!ImeCapturesKeyboard_nid_postfix());
    Type("ignored");
    Press(SDL_SCANCODE_RETURN);
    Require(std::u16string(buffer) == u"keep");
    Result closed{};
    Require(sceImeDialogGetResult(&closed) == ErrorNotOpened);
    Require(sceImeDialogTerm() == ErrorNotOpened);
    Require(sceImeDialogForceClose() == ErrorNotInUse);

    Open(param);
    Type("!");
    Press(SDL_SCANCODE_RETURN);
    Require(sceImeDialogGetStatus() == StatusFinished);
    Require(sceImeDialogForceClose() == 0);
    Require(sceImeDialogGetStatus() == StatusNone);
    Require(std::u16string(buffer) == u"keep!");

    Open(param);
    Require(sceImeDialogAbort() == 0);
    Require(sceImeDialogForceClose() == 0);
    Require(sceImeDialogGetStatus() == StatusNone);
    Open(param);
    Close();

    Require(sceImeDialogGetPanelPositionAndForm(nullptr) == ErrorNotOpened);
    std::uint32_t panelWidth = 0;
    std::uint32_t panelHeight = 0;
    Require(sceImeDialogGetPanelSize(nullptr, &panelWidth, &panelHeight) == 0 && panelWidth == 1920 && panelHeight == 1080);
    panelWidth = 0;
    panelHeight = 0;
    Require(sceImeDialogGetPanelSizeExtended(nullptr, nullptr, &panelWidth, &panelHeight) == 0 && panelWidth == 1920 && panelHeight == 1080);
    Require(sceImeDialogInit(nullptr, nullptr) == ErrorInvalidAddress);
    const Param empty = DialogParam(buffer, 0);
    Require(sceImeDialogInit(&empty, nullptr) == ErrorInvalidMaxTextLength);
    const Param tooLong = DialogParam(buffer, 2049);
    Require(sceImeDialogInit(&tooLong, nullptr) == ErrorInvalidMaxTextLength);
    const Param noBuffer = DialogParam(nullptr, 8);
    Require(sceImeDialogInit(&noBuffer, nullptr) == ErrorInvalidInputTextBuffer);
    Require(sceImeDialogGetStatus() == StatusNone);
    Require(!ImeCapturesKeyboard_nid_postfix());

    std::uint32_t star = 0xa5a5a5a5u;
    const auto starAddress = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(&star));
    Require(sceImeDialogGetCurrentStarState(starAddress) == ErrorNotInUse && star == 0xa5a5a5a5u);
    Require(sceImeDialogSetPanelPosition(640, 360) == ErrorNotInUse);
    Require(sceImeDialogInitInternal(nullptr, nullptr) == ErrorInvalidAddress);
    Require(sceImeDialogInitInternal(&empty, nullptr) == ErrorInvalidMaxTextLength);
    Require(sceImeDialogGetStatus() == StatusNone);
    Require(sceImeDialogInitInternal(&param, nullptr) == 0);
    Require(sceImeDialogGetStatus() == StatusRunning && ImeCapturesKeyboard_nid_postfix());
    Require(sceImeDialogInitInternal(&param, nullptr) == ErrorBusy);
    Require(sceImeDialogGetCurrentStarState(0) == ErrorInvalidAddress);
    Require(sceImeDialogGetCurrentStarState(starAddress) == 0 && star == 0);
    Require(sceImeDialogSetPanelPosition(640, 360) == ErrorSuspending);
    Require(sceImeDialogAbort() == 0);
    Require(sceImeDialogSetPanelPosition(0, 0) == ErrorSuspending);
    Close();

    Require(!ImeCapturesPad_nid_postfix());
    buffer[0] = u'\0';
    Open(param);
    Require(ImeCapturesPad_nid_postfix());
    Pad(0);
    Require(SelectedKey() == u"g");
    Tap(PadCross);
    Tap(PadRight);
    Require(SelectedKey() == u"h");
    Tap(PadCross);
    Tap(PadTriangle);
    Tap(PadSquare);
    Tap(PadL1);
    Tap(PadCross);
    ImeKeyboardView view;
    Require(ImeReadKeyboardView_nid_postfix(view) && view.text == u"ghh" && view.caret == 2 && view.rows == 6 && view.columns == 10);
    Require(std::u16string(buffer).empty());
    Tap(PadR2);
    Require(sceImeDialogGetStatus() == StatusFinished);
    Require(EndStatus() == EndStatusOk);
    Require(std::u16string(buffer) == u"ghh");
    Require(!ImeCapturesPad_nid_postfix() && !ImeCapturesKeyboard_nid_postfix());
    Require(!ImeReadKeyboardView_nid_postfix(view));
    Close();

    std::memcpy(buffer, u"Old", sizeof(u"Old"));
    Open(param);
    Pad(0);
    Tap(PadCross);
    Tap(PadCircle);
    Require(sceImeDialogGetStatus() == StatusFinished);
    Require(EndStatus() == EndStatusUserCanceled);
    Require(std::u16string(buffer) == u"Old");
    Require(!ImeCapturesPad_nid_postfix());
    Close();

    Open(param);
    Pad(0);
    Tap(PadCross);
    Require(sceImeDialogAbort() == 0);
    Require(EndStatus() == EndStatusAborted && !ImeCapturesPad_nid_postfix());
    Tap(PadR2);
    Require(std::u16string(buffer) == u"Old");
    Close();

    Open(param);
    Pad(0);
    Tap(PadCross);
    Require(sceImeDialogTerm() == 0);
    Require(sceImeDialogGetStatus() == StatusNone && !ImeCapturesPad_nid_postfix());
    Open(param);
    Pad(0);
    Tap(PadCross);
    Require(sceImeDialogForceClose() == 0);
    Require(sceImeDialogGetStatus() == StatusNone && !ImeCapturesPad_nid_postfix());
    Tap(PadR2);
    Require(std::u16string(buffer) == u"Old");

    buffer[0] = u'\0';
    Open(param);
    Pad(PadCross | PadR2);
    Pad(PadCross | PadR2);
    Require(sceImeDialogGetStatus() == StatusRunning);
    Pad(0);
    Tap(PadCross);
    Type("ab");
    Tap(PadCross);
    Press(SDL_SCANCODE_RETURN);
    Require(EndStatus() == EndStatusOk);
    Require(std::u16string(buffer) == u"gabg");
    Close();

    std::memcpy(buffer, u"a", sizeof(u"a"));
    const Param narrow = DialogParam(buffer, 2);
    Open(narrow);
    Pad(0);
    Type("\xf0\x9f\x98\x80");
    Tap(PadCross);
    Tap(PadCross);
    Tap(PadR2);
    Require(EndStatus() == EndStatusOk);
    Require(std::u16string(buffer) == u"ag");
    Close();

    buffer[0] = u'\0';
    ExtendedParam noController{};
    noController.disable_device = 0x00000001;
    Require(sceImeDialogInit(&param, &noController) == 0);
    Require(ImeCapturesPad_nid_postfix());
    Pad(0);
    Tap(PadCross);
    Tap(PadR2);
    Require(sceImeDialogGetStatus() == StatusRunning);
    Press(SDL_SCANCODE_RETURN);
    Require(EndStatus() == EndStatusOk && buffer[0] == u'\0');
    Close();

    ExtendedParam symbols{};
    symbols.option = 0x00000002;
    symbols.priority = 2;
    Require(sceImeDialogInit(&param, &symbols) == 0);
    Pad(0);
    Require(SelectedKey() == u"!");
    Tap(PadCross);
    Tap(PadR2);
    Require(std::u16string(buffer) == u"!");
    Close();
}
