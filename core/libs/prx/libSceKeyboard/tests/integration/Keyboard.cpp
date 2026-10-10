#include "prx/libSceKeyboard/include/keyboard_structs.h"
#include "prx/libSceKeyboard/include/KeyboardState.hpp"
#include "prx/libSceVideoOut/include/KeyboardInput.hpp"
#include "SDL_events.h"
#include "SDL_keyboard.h"

#include <Testing/Test.hpp>

#include <initializer_list>
#include <string>

extern "C" {
int APS5_VABI sceKeyboardInit(void);
int APS5_VABI sceKeyboardOpen(int, std::int32_t, std::int32_t, const void*);
int APS5_VABI sceKeyboardClose(std::int32_t);
int APS5_VABI sceKeyboardRead(std::int32_t, KeyboardData*, std::int32_t);
int APS5_VABI sceKeyboardReadState(std::int32_t, KeyboardData*);
int APS5_VABI sceKeyboardGetKey2Char(std::int32_t, std::int32_t, std::uint32_t, std::uint32_t, std::uint16_t, KeyboardCharData*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr unsigned windowId = 7;
constexpr std::uint32_t leftShift = KEYBOARD_MOD_LEFT_SHIFT;
constexpr std::uint32_t rightShift = KEYBOARD_MOD_RIGHT_SHIFT;
constexpr std::uint32_t numLock = KEYBOARD_LED_NUM_LOCK;
constexpr std::uint32_t capsLock = KEYBOARD_LED_CAPS_LOCK;

class KeyboardSession {
public:
    KeyboardSession() {
        RequireEqual(sceKeyboardInit(), KEYBOARD_OK, "initialize the keyboard library");
        RequireEqual(sceKeyboardOpen(1, 0, 0, nullptr), KEYBOARD_HANDLE, "open the keyboard");
    }

    ~KeyboardSession() {
        KeyboardInputEvent reconnect{};
        reconnect.connectionChange = true;
        reconnect.connected = true;
        KeyboardPublishInput_nid_postfix(reconnect);
        KeyboardInputEvent clearLed{};
        clearLed.keyCode = 0x04;
        clearLed.pressed = false;
        clearLed.led = 0;
        KeyboardPublishInput_nid_postfix(clearLed);
        sceKeyboardClose(KEYBOARD_HANDLE);
    }

    KeyboardSession(const KeyboardSession&) = delete;
    KeyboardSession& operator=(const KeyboardSession&) = delete;

    void DrainInitialSample() {
        KeyboardData initial{};
        RequireEqual(sceKeyboardRead(KEYBOARD_HANDLE, &initial, 1), 1, "read the initial sample");
    }
};

SDL_Event KeyEvent(Uint32 type, SDL_Scancode scancode, Uint16 mod = 0, Uint8 repeat = 0) {
    SDL_Event event{};
    event.type = type;
    event.key.windowID = windowId;
    event.key.repeat = repeat;
    event.key.keysym.scancode = scancode;
    event.key.keysym.mod = mod;
    return event;
}

SDL_Event Window(SDL_WindowEventID windowEvent) {
    SDL_Event event{};
    event.type = SDL_WINDOWEVENT;
    event.window.windowID = windowId;
    event.window.event = windowEvent;
    return event;
}

void RequireNoSample(const char* message) {
    KeyboardData data{};
    RequireEqual(sceKeyboardRead(KEYBOARD_HANDLE, &data, 1), 0, message);
}

KeyboardData ReadOne(const char* message) {
    KeyboardData data{};
    RequireEqual(sceKeyboardRead(KEYBOARD_HANDLE, &data, 1), 1, message);
    return data;
}

struct CharExpectation {
    std::uint32_t led;
    std::uint32_t modifierKey;
    std::uint16_t keyCode;
    std::uint16_t expected;
};

std::string Describe(std::int32_t arrange, const CharExpectation& expectation) {
    return "arrangement " + std::to_string(arrange) + " led " + std::to_string(expectation.led) + " modifiers " +
           std::to_string(expectation.modifierKey) + " key " + std::to_string(expectation.keyCode);
}

void RequireCharacters(std::int32_t arrange, std::initializer_list<CharExpectation> expectations) {
    const KeyboardSession session;
    for (const auto& expectation : expectations) {
        const auto input = Describe(arrange, expectation);
        KeyboardCharData data{};
        RequireEqual(sceKeyboardGetKey2Char(KEYBOARD_HANDLE, arrange, expectation.led, expectation.modifierKey,
                                            expectation.keyCode, &data),
                     KEYBOARD_OK, input);
        RequireEqual(data.char_code, expectation.expected, input);
        RequireEqual(data.processed, data.char_code != 0, input + " processed flag");
        RequireEqual(data.length, data.processed ? 1 : 0, input + " length");
    }
}

const Case openBeforeInit{"Open_BeforeInit_FailsNotInitialized", [] {
    RequireEqual(sceKeyboardOpen(1, 0, 0, nullptr), KEYBOARD_ERROR_NOT_INITIALIZED, "open before init");
}};

const Case initTwice{"Init_CalledTwice_Succeeds", [] {
    RequireEqual(sceKeyboardInit(), KEYBOARD_OK, "first init");
    RequireEqual(sceKeyboardInit(), KEYBOARD_OK, "second init");
}};

const Case openInvalidArguments{"Open_NonZeroTypeOrIndex_FailsInvalidArg", [] {
    RequireEqual(sceKeyboardInit(), KEYBOARD_OK, "init");
    RequireEqual(sceKeyboardOpen(1, 0, 1, nullptr), KEYBOARD_ERROR_INVALID_ARG, "index 1");
    RequireEqual(sceKeyboardOpen(1, 1, 0, nullptr), KEYBOARD_ERROR_INVALID_ARG, "type 1");
}};

const Case key2CharBeforeOpen{"GetKey2Char_BeforeOpen_FailsInvalidHandle", [] {
    RequireEqual(sceKeyboardInit(), KEYBOARD_OK, "init");
    KeyboardCharData data{};
    RequireEqual(sceKeyboardGetKey2Char(KEYBOARD_HANDLE, KEYBOARD_ARRANGEMENT_101, 0, 0, 0x04, &data),
                 KEYBOARD_ERROR_INVALID_HANDLE, "key to char before open");
}};

const Case openTwice{"Open_AlreadyOpened_FailsAlreadyOpened", [] {
    const KeyboardSession session;
    RequireEqual(sceKeyboardOpen(1, 0, 0, nullptr), KEYBOARD_ERROR_ALREADY_OPENED, "second open");
}};

const Case readArguments{"Read_InvalidHandleOrArguments_Fails", [] {
    const KeyboardSession session;
    KeyboardData data[KEYBOARD_MAX_DATA_NUM + 1]{};
    RequireEqual(sceKeyboardRead(42, data, 1), KEYBOARD_ERROR_INVALID_HANDLE, "unknown handle");
    RequireEqual(sceKeyboardRead(KEYBOARD_HANDLE, nullptr, 1), KEYBOARD_ERROR_INVALID_ARG, "null buffer");
    RequireEqual(sceKeyboardRead(KEYBOARD_HANDLE, data, KEYBOARD_MAX_DATA_NUM + 1), KEYBOARD_ERROR_INVALID_ARG,
                 "too many samples");
    RequireEqual(sceKeyboardReadState(KEYBOARD_HANDLE, nullptr), KEYBOARD_ERROR_INVALID_ARG, "null state buffer");
}};

const Case openInitialSample{"Open_Fresh_QueuesOneConnectedEmptySample", [] {
    const KeyboardSession session;
    const auto initial = ReadOne("initial sample");
    Require(initial.connected, "initial sample is connected");
    RequireEqual(initial.length, 0, "initial key count");
    RequireEqual(initial.modifier_key, 0u, "initial modifiers");
    RequireNoSample("queue empty after the initial sample");
}};

const Case otherWindow{"HandleEvent_OtherWindow_IsIgnored", [] {
    KeyboardSession session;
    session.DrainInitialSample();
    KeyboardInput input;
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_A), 8);
    RequireNoSample("key for window 7 while listening to window 8");
}};

const Case keySequence{"HandleEvent_KeySequence_QueuesModifiersKeysAndLeds", [] {
    KeyboardSession session;
    session.DrainInitialSample();
    KeyboardInput input;
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_LSHIFT, KMOD_NUM), windowId);
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_A, KMOD_NUM | KMOD_LSHIFT), windowId);
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_A, KMOD_NUM | KMOD_LSHIFT, 1), windowId);
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_RETURN, KMOD_NUM | KMOD_LSHIFT), windowId);
    input.HandleEvent(KeyEvent(SDL_KEYUP, SDL_SCANCODE_A, KMOD_NUM | KMOD_LSHIFT), windowId);
    KeyboardData data[KEYBOARD_MAX_DATA_NUM]{};
    RequireEqual(sceKeyboardRead(KEYBOARD_HANDLE, data, KEYBOARD_MAX_DATA_NUM), 4, "four samples, repeat ignored");
    RequireEqual(data[0].length, 0, "shift sample key count");
    RequireEqual(data[0].modifier_key, 2u, "shift sample modifiers");
    RequireEqual(data[0].led, numLock, "shift sample led");
    RequireEqual(data[1].length, 1, "A sample key count");
    RequireEqual(data[1].key_code[0], 0x04, "A sample key");
    RequireEqual(data[2].length, 2, "return sample key count");
    RequireEqual(data[2].key_code[0], 0x04, "return sample first key");
    RequireEqual(data[2].key_code[1], 0x28, "return sample second key");
    RequireEqual(data[3].length, 1, "release sample key count");
    RequireEqual(data[3].key_code[0], 0x28, "release sample remaining key");
    RequireEqual(data[3].modifier_key, 2u, "release sample modifiers");
    Require(data[0].timestamp <= data[3].timestamp, "timestamps are monotonic");
}};

const Case readState{"ReadState_KeysHeld_ReturnsCurrentSnapshot", [] {
    KeyboardSession session;
    session.DrainInitialSample();
    KeyboardInput input;
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_LSHIFT, KMOD_NUM), windowId);
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_A, KMOD_NUM | KMOD_LSHIFT), windowId);
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_RETURN, KMOD_NUM | KMOD_LSHIFT), windowId);
    input.HandleEvent(KeyEvent(SDL_KEYUP, SDL_SCANCODE_A, KMOD_NUM | KMOD_LSHIFT), windowId);
    KeyboardData state{};
    RequireEqual(sceKeyboardReadState(KEYBOARD_HANDLE, &state), KEYBOARD_OK, "read state");
    Require(state.connected, "state is connected");
    RequireEqual(state.length, 1, "state key count");
    RequireEqual(state.key_code[0], 0x28, "state key");
    RequireEqual(state.modifier_key, 2u, "state modifiers");
}};

const Case capsLockLed{"HandleEvent_CapsLockPressed_ReportsBothLeds", [] {
    KeyboardSession session;
    session.DrainInitialSample();
    KeyboardInput input;
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_CAPSLOCK, KMOD_NUM | KMOD_CAPS | KMOD_LSHIFT), windowId);
    RequireEqual(ReadOne("caps lock sample").led, numLock | capsLock, "num and caps lock leds");
}};

const Case focusLost{"HandleEvent_FocusLost_ReleasesKeysAndDropsInput", [] {
    KeyboardSession session;
    session.DrainInitialSample();
    KeyboardInput input;
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_LSHIFT), windowId);
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_A, KMOD_LSHIFT), windowId);
    KeyboardData held[2]{};
    RequireEqual(sceKeyboardRead(KEYBOARD_HANDLE, held, 2), 2, "held key samples");
    input.HandleEvent(Window(SDL_WINDOWEVENT_FOCUS_LOST), windowId);
    const auto released = ReadOne("focus lost sample");
    Require(released.connected, "still connected after focus loss");
    RequireEqual(released.length, 0, "keys released on focus loss");
    RequireEqual(released.modifier_key, 0u, "modifiers released on focus loss");
    input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_B), windowId);
    RequireNoSample("key while unfocused");
}};

const Case focusGained{"HandleEvent_FocusGainedWhileConnected_QueuesNothing", [] {
    KeyboardSession session;
    session.DrainInitialSample();
    KeyboardInput input;
    input.HandleEvent(Window(SDL_WINDOWEVENT_FOCUS_LOST), windowId);
    ReadOne("focus lost sample");
    input.HandleEvent(Window(SDL_WINDOWEVENT_FOCUS_GAINED), windowId);
    RequireNoSample("focus regained while connected");
}};

const Case closeAndReconnect{"HandleEvent_WindowCloseThenFocus_DisconnectsThenReconnects", [] {
    KeyboardSession session;
    session.DrainInitialSample();
    KeyboardInput input;
    input.HandleEvent(Window(SDL_WINDOWEVENT_CLOSE), windowId);
    Require(!ReadOne("close sample").connected, "disconnected after window close");
    input.HandleEvent(Window(SDL_WINDOWEVENT_FOCUS_GAINED), windowId);
    Require(ReadOne("reconnect sample").connected, "connected after focus gained");
}};

const Case overflow{"Publish_MoreKeysThanCapacity_KeepsSixteenKeysAndSamples", [] {
    KeyboardSession session;
    session.DrainInitialSample();
    for (std::uint16_t key = 0x04; key < 0x04 + 20; ++key) {
        KeyboardInputEvent press{};
        press.keyCode = key;
        press.pressed = true;
        KeyboardPublishInput_nid_postfix(press);
    }
    KeyboardData data[KEYBOARD_MAX_DATA_NUM]{};
    RequireEqual(sceKeyboardRead(KEYBOARD_HANDLE, data, KEYBOARD_MAX_DATA_NUM), KEYBOARD_MAX_DATA_NUM, "full queue");
    RequireEqual(data[15].length, 16, "newest sample key count");
    RequireEqual(data[15].key_code[15], 0x13, "sixteenth key");
    RequireNoSample("queue drained");
}};

const Case usLetters{"GetKey2Char_Us101Letters_HonorShiftAndCapsLock", [] {
    RequireCharacters(KEYBOARD_ARRANGEMENT_101, {
        {0, 0, 0x04, 'a'},
        {0, leftShift, 0x1d, 'Z'},
        {capsLock, 0, 0x04, 'A'},
        {capsLock, rightShift, 0x04, 'a'},
    });
}};

const Case usSymbols{"GetKey2Char_Us101DigitsAndSymbols_MapUsLayout", [] {
    RequireCharacters(KEYBOARD_ARRANGEMENT_101, {
        {capsLock, 0, 0x1e, '1'},
        {0, leftShift, 0x1e, '!'},
        {0, 0, 0x27, '0'},
        {0, leftShift, 0x27, ')'},
        {0, 0, 0x31, '\\'},
        {0, leftShift, 0x31, '|'},
        {0, 0, 0x34, '\''},
        {0, leftShift, 0x34, '"'},
        {0, 0, 0x38, '/'},
        {0, leftShift, 0x38, '?'},
    });
}};

const Case usControls{"GetKey2Char_Us101ControlKeys_MapOrReturnZero", [] {
    RequireCharacters(KEYBOARD_ARRANGEMENT_101, {
        {0, 0, 0x28, '\n'},
        {0, 0, 0x2a, '\b'},
        {0, 0, 0x2b, '\t'},
        {0, 0, 0x2c, ' '},
        {0, 0, 0x29, 0},
        {0, 0, 0x32, 0},
        {0, 0, 0x3a, 0},
        {0, 0, 0xe1, 0},
        {0, 0, 0x87, 0},
        {0, 0, 0x89, 0},
    });
}};

const Case usKeypad{"GetKey2Char_Us101Keypad_DigitsNeedNumLock", [] {
    RequireCharacters(KEYBOARD_ARRANGEMENT_101, {
        {0, 0, 0x59, 0},
        {numLock, 0, 0x59, '1'},
        {numLock, 0, 0x62, '0'},
        {numLock, 0, 0x63, '.'},
        {0, 0, 0x54, '/'},
        {0, 0, 0x57, '+'},
        {0, 0, 0x58, '\n'},
    });
}};

const Case jisLetters{"GetKey2Char_Jis106Letters_HonorShiftAndCapsLock", [] {
    RequireCharacters(KEYBOARD_ARRANGEMENT_106, {
        {0, 0, 0x04, 'a'},
        {0, leftShift, 0x04, 'A'},
        {capsLock, rightShift, 0x04, 'a'},
    });
}};

const Case jisDigits{"GetKey2Char_Jis106ShiftedDigits_MapJisLayout", [] {
    RequireCharacters(KEYBOARD_ARRANGEMENT_106, {
        {0, 0, 0x1e, '1'},
        {0, leftShift, 0x1e, '!'},
        {0, leftShift, 0x1f, '"'},
        {0, leftShift, 0x23, '&'},
        {0, leftShift, 0x24, '\''},
        {0, leftShift, 0x25, '('},
        {0, leftShift, 0x26, ')'},
        {0, 0, 0x27, '0'},
        {0, leftShift, 0x27, 0},
    });
}};

const Case jisControls{"GetKey2Char_Jis106ControlKeys_MapOrReturnZero", [] {
    RequireCharacters(KEYBOARD_ARRANGEMENT_106, {
        {0, 0, 0x28, '\n'},
        {0, 0, 0x29, 0},
        {0, 0, 0x2a, '\b'},
        {0, 0, 0x2b, '\t'},
        {0, leftShift, 0x2c, ' '},
        {0, 0, 0xe1, 0},
    });
}};

const Case jisSymbols{"GetKey2Char_Jis106Symbols_MapJisLayout", [] {
    RequireCharacters(KEYBOARD_ARRANGEMENT_106, {
        {0, 0, 0x2d, '-'},
        {0, leftShift, 0x2d, '='},
        {0, 0, 0x2e, '^'},
        {0, leftShift, 0x2e, '~'},
        {0, 0, 0x2f, '@'},
        {0, leftShift, 0x2f, '`'},
        {0, 0, 0x30, '['},
        {0, leftShift, 0x30, '{'},
        {0, 0, 0x31, ']'},
        {0, leftShift, 0x31, '}'},
        {0, 0, 0x32, ']'},
        {0, leftShift, 0x32, '}'},
        {0, 0, 0x33, ';'},
        {0, leftShift, 0x33, '+'},
        {0, 0, 0x34, ':'},
        {0, leftShift, 0x34, '*'},
        {0, 0, 0x35, 0},
        {0, leftShift, 0x35, 0},
        {0, 0, 0x36, ','},
        {0, leftShift, 0x37, '>'},
        {0, leftShift, 0x38, '?'},
        {0, 0, 0x87, '\\'},
        {0, leftShift, 0x87, '_'},
        {0, 0, 0x89, '\\'},
        {0, leftShift, 0x89, '|'},
    });
}};

const Case jisKeypad{"GetKey2Char_Jis106Keypad_DigitsNeedNumLock", [] {
    RequireCharacters(KEYBOARD_ARRANGEMENT_106, {
        {numLock, 0, 0x59, '1'},
        {0, 0, 0x59, 0},
        {0, 0, 0x55, '*'},
    });
}};

const Case key2CharInvalidArguments{"GetKey2Char_NullOutputOrUnknownArrangement_FailsInvalidArg", [] {
    const KeyboardSession session;
    KeyboardCharData data{};
    RequireEqual(sceKeyboardGetKey2Char(KEYBOARD_HANDLE, KEYBOARD_ARRANGEMENT_101, 0, 0, 0x04, nullptr),
                 KEYBOARD_ERROR_INVALID_ARG, "null output with US layout");
    RequireEqual(sceKeyboardGetKey2Char(KEYBOARD_HANDLE, KEYBOARD_ARRANGEMENT_106, 0, 0, 0x04, nullptr),
                 KEYBOARD_ERROR_INVALID_ARG, "null output with JIS layout");
    RequireEqual(sceKeyboardGetKey2Char(KEYBOARD_HANDLE, 2, 0, 0, 0x04, &data), KEYBOARD_ERROR_INVALID_ARG,
                 "arrangement 2");
}};

const Case closeHandle{"Close_OpenHandle_InvalidatesHandle", [] {
    const KeyboardSession session;
    RequireEqual(sceKeyboardClose(KEYBOARD_HANDLE), KEYBOARD_OK, "close");
    KeyboardData data{};
    RequireEqual(sceKeyboardRead(KEYBOARD_HANDLE, &data, 1), KEYBOARD_ERROR_INVALID_HANDLE, "read after close");
    RequireEqual(sceKeyboardReadState(KEYBOARD_HANDLE, &data), KEYBOARD_ERROR_INVALID_HANDLE, "read state after close");
    RequireEqual(sceKeyboardClose(KEYBOARD_HANDLE), KEYBOARD_ERROR_INVALID_HANDLE, "second close");
}};

const Case reopen{"Open_AfterClose_QueuesFreshEmptySample", [] {
    {
        KeyboardSession first;
        KeyboardInput input;
        input.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_A), windowId);
    }
    const KeyboardSession session;
    RequireEqual(ReadOne("initial sample after reopen").length, 0, "keys reset by reopen");
}};

} // namespace
