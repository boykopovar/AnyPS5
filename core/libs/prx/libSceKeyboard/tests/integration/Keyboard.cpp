#include "prx/libSceKeyboard/include/keyboard_structs.h"
#include "prx/libSceKeyboard/include/KeyboardState.hpp"
#include "prx/libSceVideoOut/include/KeyboardInput.hpp"
#include "SDL_events.h"
#include "SDL_keyboard.h"

#include <cstdlib>

extern "C" {
int APS5_VABI sceKeyboardInit(void);
int APS5_VABI sceKeyboardOpen(int, std::int32_t, std::int32_t, const void*);
int APS5_VABI sceKeyboardClose(std::int32_t);
int APS5_VABI sceKeyboardRead(std::int32_t, KeyboardData*, std::int32_t);
int APS5_VABI sceKeyboardReadState(std::int32_t, KeyboardData*);
int APS5_VABI sceKeyboardGetKey2Char(std::int32_t, std::int32_t, std::uint32_t, std::uint32_t, std::uint16_t, KeyboardCharData*);
}

static void Require(bool value) { if (!value) std::abort(); }

static SDL_Event KeyEvent(Uint32 type, SDL_Scancode scancode, Uint16 mod = 0, Uint8 repeat = 0) {
    SDL_Event event{};
    event.type = type;
    event.key.windowID = 7;
    event.key.repeat = repeat;
    event.key.keysym.scancode = scancode;
    event.key.keysym.mod = mod;
    return event;
}

static std::uint16_t Char(std::uint32_t led, std::uint32_t modifierKey, std::uint16_t keyCode, std::int32_t arrange = KEYBOARD_ARRANGEMENT_101) {
    KeyboardCharData data{};
    Require(sceKeyboardGetKey2Char(KEYBOARD_HANDLE, arrange, led, modifierKey, keyCode, &data) == KEYBOARD_OK);
    Require(data.processed == (data.char_code != 0) && data.length == (data.processed ? 1 : 0));
    return data.char_code;
}

static std::uint16_t Jis(std::uint32_t modifierKey, std::uint16_t keyCode, std::uint32_t led = 0) {
    return Char(led, modifierKey, keyCode, KEYBOARD_ARRANGEMENT_106);
}

int main() {
    KeyboardData data[16]{};
    KeyboardCharData charData{};
    Require(sceKeyboardOpen(1, 0, 0, nullptr) == KEYBOARD_ERROR_NOT_INITIALIZED);
    Require(sceKeyboardInit() == KEYBOARD_OK);
    Require(sceKeyboardInit() == KEYBOARD_OK);
    Require(sceKeyboardOpen(1, 0, 1, nullptr) == KEYBOARD_ERROR_INVALID_ARG);
    Require(sceKeyboardOpen(1, 1, 0, nullptr) == KEYBOARD_ERROR_INVALID_ARG);
    Require(sceKeyboardGetKey2Char(KEYBOARD_HANDLE, KEYBOARD_ARRANGEMENT_101, 0, 0, 0x04, &charData) == KEYBOARD_ERROR_INVALID_HANDLE);
    Require(sceKeyboardOpen(1, 0, 0, nullptr) == KEYBOARD_HANDLE);
    Require(sceKeyboardOpen(1, 0, 0, nullptr) == KEYBOARD_ERROR_ALREADY_OPENED);
    Require(sceKeyboardRead(42, data, 1) == KEYBOARD_ERROR_INVALID_HANDLE);
    Require(sceKeyboardRead(KEYBOARD_HANDLE, nullptr, 1) == KEYBOARD_ERROR_INVALID_ARG);
    Require(sceKeyboardRead(KEYBOARD_HANDLE, data, 17) == KEYBOARD_ERROR_INVALID_ARG);
    Require(sceKeyboardReadState(KEYBOARD_HANDLE, nullptr) == KEYBOARD_ERROR_INVALID_ARG);
    Require(sceKeyboardRead(KEYBOARD_HANDLE, data, 1) == 1);
    Require(data[0].connected && data[0].length == 0 && data[0].modifier_key == 0);
    Require(sceKeyboardRead(KEYBOARD_HANDLE, data, 1) == 0);

    KeyboardInput input;
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_A), 8);
    Require(sceKeyboardRead(KEYBOARD_HANDLE, data, 1) == 0);
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_LSHIFT, KMOD_NUM), 7);
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_A, KMOD_NUM | KMOD_LSHIFT), 7);
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_A, KMOD_NUM | KMOD_LSHIFT, 1), 7);
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_RETURN, KMOD_NUM | KMOD_LSHIFT), 7);
    input.HandleEvent(KeyEvent(SDL_KEYUP, SDL_SCANCODE_A, KMOD_NUM | KMOD_LSHIFT), 7);
    Require(sceKeyboardRead(KEYBOARD_HANDLE, data, 16) == 4);
    Require(data[0].length == 0 && data[0].modifier_key == 2 && data[0].led == KEYBOARD_LED_NUM_LOCK);
    Require(data[1].length == 1 && data[1].key_code[0] == 0x04);
    Require(data[2].length == 2 && data[2].key_code[0] == 0x04 && data[2].key_code[1] == 0x28);
    Require(data[3].length == 1 && data[3].key_code[0] == 0x28 && data[3].modifier_key == 2);
    Require(data[0].timestamp <= data[3].timestamp);
    Require(sceKeyboardReadState(KEYBOARD_HANDLE, data) == KEYBOARD_OK);
    Require(data[0].connected && data[0].length == 1 && data[0].key_code[0] == 0x28 && data[0].modifier_key == 2);

    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_CAPSLOCK, KMOD_NUM | KMOD_CAPS | KMOD_LSHIFT), 7);
    Require(sceKeyboardRead(KEYBOARD_HANDLE, data, 1) == 1);
    Require(data[0].led == (KEYBOARD_LED_NUM_LOCK | KEYBOARD_LED_CAPS_LOCK));

    SDL_Event window{};
    window.type = SDL_WINDOWEVENT;
    window.window.windowID = 7;
    window.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    input.HandleEvent(window, 7);
    Require(sceKeyboardRead(KEYBOARD_HANDLE, data, 1) == 1);
    Require(data[0].connected && data[0].length == 0 && data[0].modifier_key == 0);
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_B), 7);
    Require(sceKeyboardRead(KEYBOARD_HANDLE, data, 1) == 0);
    window.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    input.HandleEvent(window, 7);
    Require(sceKeyboardRead(KEYBOARD_HANDLE, data, 1) == 0);
    window.window.event = SDL_WINDOWEVENT_CLOSE;
    input.HandleEvent(window, 7);
    Require(sceKeyboardRead(KEYBOARD_HANDLE, data, 1) == 1 && !data[0].connected);
    window.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    input.HandleEvent(window, 7);
    Require(sceKeyboardRead(KEYBOARD_HANDLE, data, 1) == 1 && data[0].connected);

    for (std::uint16_t key = 0x04; key < 0x04 + 20; ++key) {
        KeyboardInputEvent press{};
        press.keyCode = key;
        press.pressed = true;
        KeyboardPublishInput_nid_postfix(press);
    }
    Require(sceKeyboardRead(KEYBOARD_HANDLE, data, 16) == 16);
    Require(data[15].length == 16 && data[15].key_code[15] == 0x13);
    Require(sceKeyboardRead(KEYBOARD_HANDLE, data, 1) == 0);

    Require(Char(0, 0, 0x04) == 'a' && Char(0, KEYBOARD_MOD_LEFT_SHIFT, 0x1d) == 'Z');
    Require(Char(KEYBOARD_LED_CAPS_LOCK, 0, 0x04) == 'A' && Char(KEYBOARD_LED_CAPS_LOCK, KEYBOARD_MOD_RIGHT_SHIFT, 0x04) == 'a');
    Require(Char(KEYBOARD_LED_CAPS_LOCK, 0, 0x1e) == '1' && Char(0, KEYBOARD_MOD_LEFT_SHIFT, 0x1e) == '!');
    Require(Char(0, 0, 0x27) == '0' && Char(0, KEYBOARD_MOD_LEFT_SHIFT, 0x27) == ')');
    Require(Char(0, 0, 0x28) == '\n' && Char(0, 0, 0x2a) == '\b' && Char(0, 0, 0x2b) == '\t' && Char(0, 0, 0x2c) == ' ');
    Require(Char(0, 0, 0x29) == 0 && Char(0, 0, 0x32) == 0 && Char(0, 0, 0x3a) == 0 && Char(0, 0, 0xe1) == 0);
    Require(Char(0, 0, 0x31) == '\\' && Char(0, KEYBOARD_MOD_LEFT_SHIFT, 0x31) == '|');
    Require(Char(0, 0, 0x34) == '\'' && Char(0, KEYBOARD_MOD_LEFT_SHIFT, 0x34) == '"');
    Require(Char(0, 0, 0x38) == '/' && Char(0, KEYBOARD_MOD_LEFT_SHIFT, 0x38) == '?');
    Require(Char(0, 0, 0x59) == 0 && Char(KEYBOARD_LED_NUM_LOCK, 0, 0x59) == '1');
    Require(Char(KEYBOARD_LED_NUM_LOCK, 0, 0x62) == '0' && Char(KEYBOARD_LED_NUM_LOCK, 0, 0x63) == '.');
    Require(Char(0, 0, 0x54) == '/' && Char(0, 0, 0x57) == '+' && Char(0, 0, 0x58) == '\n');
    Require(Char(0, 0, 0x87) == 0 && Char(0, 0, 0x89) == 0);

    constexpr std::uint32_t shift = KEYBOARD_MOD_LEFT_SHIFT;
    Require(Jis(0, 0x04) == 'a' && Jis(shift, 0x04) == 'A' && Jis(KEYBOARD_MOD_RIGHT_SHIFT, 0x04, KEYBOARD_LED_CAPS_LOCK) == 'a');
    Require(Jis(0, 0x1e) == '1' && Jis(shift, 0x1e) == '!' && Jis(shift, 0x1f) == '"' && Jis(shift, 0x23) == '&');
    Require(Jis(shift, 0x24) == '\'' && Jis(shift, 0x25) == '(' && Jis(shift, 0x26) == ')' && Jis(0, 0x27) == '0' && Jis(shift, 0x27) == 0);
    Require(Jis(0, 0x28) == '\n' && Jis(0, 0x29) == 0 && Jis(0, 0x2a) == '\b' && Jis(0, 0x2b) == '\t' && Jis(shift, 0x2c) == ' ');
    Require(Jis(0, 0x2d) == '-' && Jis(shift, 0x2d) == '=' && Jis(0, 0x2e) == '^' && Jis(shift, 0x2e) == '~');
    Require(Jis(0, 0x2f) == '@' && Jis(shift, 0x2f) == '`' && Jis(0, 0x30) == '[' && Jis(shift, 0x30) == '{');
    Require(Jis(0, 0x31) == ']' && Jis(shift, 0x31) == '}' && Jis(0, 0x32) == ']' && Jis(shift, 0x32) == '}');
    Require(Jis(0, 0x33) == ';' && Jis(shift, 0x33) == '+' && Jis(0, 0x34) == ':' && Jis(shift, 0x34) == '*');
    Require(Jis(0, 0x35) == 0 && Jis(shift, 0x35) == 0 && Jis(0, 0x36) == ',' && Jis(shift, 0x37) == '>' && Jis(shift, 0x38) == '?');
    Require(Jis(0, 0x87) == '\\' && Jis(shift, 0x87) == '_' && Jis(0, 0x89) == '\\' && Jis(shift, 0x89) == '|');
    Require(Jis(0, 0x59, KEYBOARD_LED_NUM_LOCK) == '1' && Jis(0, 0x59) == 0 && Jis(0, 0x55) == '*' && Jis(0, 0xe1) == 0);
    Require(sceKeyboardGetKey2Char(KEYBOARD_HANDLE, KEYBOARD_ARRANGEMENT_101, 0, 0, 0x04, nullptr) == KEYBOARD_ERROR_INVALID_ARG);
    Require(sceKeyboardGetKey2Char(KEYBOARD_HANDLE, KEYBOARD_ARRANGEMENT_106, 0, 0, 0x04, nullptr) == KEYBOARD_ERROR_INVALID_ARG);
    Require(sceKeyboardGetKey2Char(KEYBOARD_HANDLE, 2, 0, 0, 0x04, &charData) == KEYBOARD_ERROR_INVALID_ARG);

    Require(sceKeyboardClose(KEYBOARD_HANDLE) == KEYBOARD_OK);
    Require(sceKeyboardRead(KEYBOARD_HANDLE, data, 1) == KEYBOARD_ERROR_INVALID_HANDLE);
    Require(sceKeyboardReadState(KEYBOARD_HANDLE, data) == KEYBOARD_ERROR_INVALID_HANDLE);
    Require(sceKeyboardClose(KEYBOARD_HANDLE) == KEYBOARD_ERROR_INVALID_HANDLE);
    Require(sceKeyboardOpen(1, 0, 0, nullptr) == KEYBOARD_HANDLE);
    Require(sceKeyboardRead(KEYBOARD_HANDLE, data, 1) == 1 && data[0].length == 0);
    Require(sceKeyboardClose(KEYBOARD_HANDLE) == KEYBOARD_OK);
}
