#include "prx/libSceImeDialog/include/Api.hpp"
#include "prx/libSceIme/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"
#include "prx/libSceIme/include/ImeKeyboardView.hpp"
#include "prx/libSceVideoOut/include/ImeTextInput.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr unsigned WindowId = 7;
constexpr int StatusRunning = 1;
#ifdef __APPLE__
constexpr unsigned PasteModifier = KMOD_LGUI;
constexpr unsigned WrongModifier = KMOD_LCTRL;
#else
constexpr unsigned PasteModifier = KMOD_LCTRL;
constexpr unsigned WrongModifier = KMOD_LGUI;
#endif

std::string g_clipboard;
bool g_clipboardAvailable = true;
int g_reads = 0;
ImeTextInput g_window;

bool ReadClipboard(std::string& text) {
    ++g_reads;
    if (!g_clipboardAvailable) return false;
    text = g_clipboard;
    return true;
}

void APS5_VABI Ignore(void*, const ImeEvent*) {}

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

void Edit(const char* utf8, std::int32_t start, std::int32_t length, unsigned windowId = WindowId) {
    SDL_Event event{};
    event.type = SDL_TEXTEDITING;
    event.edit.windowID = windowId;
    std::strncpy(event.edit.text, utf8, sizeof(event.edit.text) - 1);
    event.edit.start = start;
    event.edit.length = length;
    g_window.HandleEvent(event, WindowId);
}

void Type(const char* utf8) {
    SDL_Event event{};
    event.type = SDL_TEXTINPUT;
    event.text.windowID = WindowId;
    std::strncpy(event.text.text, utf8, sizeof(event.text.text) - 1);
    g_window.HandleEvent(event, WindowId);
}

void Key(SDL_Scancode scancode, SDL_Keycode sym, unsigned modifiers, bool repeat = false, unsigned type = SDL_KEYDOWN, unsigned windowId = WindowId) {
    SDL_Event event{};
    event.type = type;
    event.key.windowID = windowId;
    event.key.keysym.scancode = scancode;
    event.key.keysym.sym = sym;
    event.key.keysym.mod = static_cast<Uint16>(modifiers);
    event.key.repeat = repeat ? 1 : 0;
    g_window.HandleEvent(event, WindowId);
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

void CheckEditingEvents() {
    char16_t buffer[16] = u"x";
    const Param param = DialogParam(buffer, 8);
    Open(param);
    Edit("k", 1, 0);
    Require(View().preedit == u"k" && View().preeditCursor == 1, "an SDL text editing event is not shown as a preedit");
    Edit("ka", 2, 0);
    Require(View().preedit == u"ka" && View().preeditCursor == 2, "a second edit does not replace the first");
    Edit("\xe3\x81\x8b", 1, 1, WindowId + 1);
    Require(View().preedit == u"ka", "an edit for another window was shown");
    Type("\xe3\x81\x8b");
    Edit("", 0, 0);
    Require(View().preedit.empty() && View().text == u"x\u304B" && std::u16string(buffer) == u"x", "the commit is not the only text and the preedit is not gone");
    Edit("", 0, 0);
    Require(View().text == u"x\u304B", "an empty edit changed the text");
    Edit("ni", 2, 0);
    Edit("", 0, 0);
    Require(View().preedit.empty(), "an empty edit does not clear the composition");
    Key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, 0);
    Require(sceImeDialogGetStatus() == 2, "Escape with no composition does not cancel");
    Close();

    Open(param);
    Edit("ni", 2, 0);
    Key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, 0);
    Require(sceImeDialogGetStatus() == StatusRunning && View().preedit.empty(), "Escape from the keyboard with a composition cancels the dialog");
    Close();

    Edit("late", 0, 0);
    Open(param);
    Require(View().preedit.empty(), "an edit with no dialog was kept");
    Close();
}

void CheckPasteShortcuts() {
    char16_t buffer[64] = u"";
    const Param param = DialogParam(buffer, 32);
    g_window.SetClipboardProvider(&ReadClipboard);
    g_clipboard = "ab";
    Open(param);

    g_reads = 0;
    Key(SDL_SCANCODE_V, SDLK_v, PasteModifier);
    Require(g_reads == 1 && View().text == u"ab", "Ctrl+V does not paste");
    Key(SDL_SCANCODE_INSERT, SDLK_INSERT, KMOD_LSHIFT);
    Require(g_reads == 2 && View().text == u"abab", "Shift+Insert does not paste");
    Key(SDL_SCANCODE_V, SDLK_v, PasteModifier | KMOD_LSHIFT);
    Require(g_reads == 3 && View().text == u"ababab", "Ctrl+Shift+V does not paste");

    Key(SDL_SCANCODE_V, SDLK_v, PasteModifier, true);
    Key(SDL_SCANCODE_INSERT, SDLK_INSERT, KMOD_LSHIFT, true);
    Key(SDL_SCANCODE_V, SDLK_v, PasteModifier | KMOD_LALT);
    Key(SDL_SCANCODE_V, SDLK_v, 0);
    Key(SDL_SCANCODE_V, SDLK_v, WrongModifier);
    Key(SDL_SCANCODE_INSERT, SDLK_INSERT, 0);
    Key(SDL_SCANCODE_V, SDLK_v, PasteModifier, false, SDL_KEYUP);
    Key(SDL_SCANCODE_V, SDLK_v, PasteModifier, false, SDL_KEYDOWN, WindowId + 1);
    Key(SDL_SCANCODE_C, SDLK_c, PasteModifier);
    Key(SDL_SCANCODE_X, SDLK_x, PasteModifier);
    Key(SDL_SCANCODE_A, SDLK_a, PasteModifier);
    Require(g_reads == 3 && View().text == u"ababab", "a key that is not a paste read the clipboard or changed the text");

    g_clipboardAvailable = false;
    Key(SDL_SCANCODE_V, SDLK_v, PasteModifier);
    Require(g_reads == 4 && View().text == u"ababab", "an unavailable clipboard changed the text");
    g_clipboardAvailable = true;
    g_clipboard.clear();
    Key(SDL_SCANCODE_V, SDLK_v, PasteModifier);
    Require(g_reads == 5 && View().text == u"ababab", "an empty clipboard changed the text");
    g_clipboard = "cd";

    Edit("ni", 2, 0);
    Key(SDL_SCANCODE_V, SDLK_v, PasteModifier);
    Require(View().text == u"ababab", "Ctrl+V pasted while a composition was shown");
    Edit("", 0, 0);
    Key(SDL_SCANCODE_V, SDLK_v, PasteModifier);
    Require(View().text == u"abababcd", "Ctrl+V does not paste once the composition ends");
    Require(std::u16string(buffer).empty(), "a paste reached the buffer before accept");
    Close();
}

void CheckNothingCaptured() {
    g_window.SetClipboardProvider(&ReadClipboard);
    g_clipboard = "secret";
    g_reads = 0;
    Require(!ImeCapturesKeyboard_nid_postfix(), "something is capturing");
    Key(SDL_SCANCODE_V, SDLK_v, PasteModifier);
    Key(SDL_SCANCODE_INSERT, SDLK_INSERT, KMOD_LSHIFT);
    Require(g_reads == 0, "the clipboard was read while no dialog captures the keyboard");

    char16_t buffer[16] = u"";
    const Param param = DialogParam(buffer, 16);
    Open(param);
    Require(sceImeDialogAbort() == 0, "abort");
    Key(SDL_SCANCODE_V, SDLK_v, PasteModifier);
    Require(g_reads == 0, "the clipboard was read after the dialog finished");
    Close();

    ImeTextInput bare;
    Open(param);
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.windowID = WindowId;
    event.key.keysym.scancode = SDL_SCANCODE_V;
    event.key.keysym.sym = SDLK_v;
    event.key.keysym.mod = static_cast<Uint16>(PasteModifier);
    bare.HandleEvent(event, WindowId);
    Require(View().text.empty() && g_reads == 0, "a window with no clipboard provider pasted");
    Close();
}

void CheckHugeClipboard() {
    char16_t buffer[2049] = u"";
    const Param param = DialogParam(buffer, 2048);
    g_window.SetClipboardProvider(&ReadClipboard);
    g_clipboard.assign(4u * 1024u * 1024u, 'x');
    Open(param);
    Key(SDL_SCANCODE_V, SDLK_v, PasteModifier);
    Require(View().text.size() == 2048, "a huge clipboard is not cut at the dialog limit");
    Close();
}

void CheckSession() {
    struct Buffers {
        char16_t text[2049] = {};
        alignas(16) unsigned char work[0x5000] = {};
    } buffers;
    Param param;
    sceImeParamInit(&param);
    param.user_id = 1;
    param.max_text_length = 16;
    param.input_text_buffer = buffers.text;
    param.option = 0x00000001;
    param.work = buffers.work;
    param.handler = Ignore;
    g_window.SetClipboardProvider(&ReadClipboard);
    g_clipboard = "a\r\nb";
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open");
    Key(SDL_SCANCODE_V, SDLK_v, PasteModifier);
    Edit("ni", 2, 0);
    Key(SDL_SCANCODE_V, SDLK_v, PasteModifier);
    Require(sceImeUpdate(param.handler) == 0, "update");
    Require(std::u16string(buffers.text) == u"a\nb", "the session text is not the single paste with a normalised break");
    Require(sceImeClose_nid_postfix() == 0, "close");
}

}

int main() {
    CheckEditingEvents();
    CheckPasteShortcuts();
    CheckNothingCaptured();
    CheckHugeClipboard();
    CheckSession();
}
