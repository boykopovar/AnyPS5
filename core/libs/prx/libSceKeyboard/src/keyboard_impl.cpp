#include "prx/libSceKeyboard/include/KeyboardState.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Time/include/Time.hpp"

#include <algorithm>
#include <array>
#include <mutex>

namespace {
std::mutex keyboardMutex;
std::array<KeyboardData, KEYBOARD_MAX_DATA_NUM> history{};
int head = 0;
int count = 0;
bool initialized = false;
bool opened = false;
bool connected = true;
std::uint32_t led = 0;
std::uint32_t modifiers = 0;
std::array<std::uint16_t, KEYBOARD_MAX_KEYCODES> keys{};
int keyCount = 0;

constexpr char US_NORMAL[] = "1234567890\n\0\b\t -=[]\\\0;'`,./";
constexpr char US_SHIFTED[] = "!@#$%^&*()\n\0\b\t _+{}|\0:\"~<>?";
constexpr char JIS_NORMAL[] = "1234567890\n\0\b\t -^@[]];:\0,./";
constexpr char JIS_SHIFTED[] = "!\"#$%&'()\0\n\0\b\t =~`{}}+*\0<>?";
constexpr char KEYPAD_DIGITS[] = "1234567890.";

// JIS kana layout (hiragana). Index = keyCode - 0x1e, same span as JIS_NORMAL (0x1e..0x38).
// Code points are \u escapes so the file doesn't depend on source encoding.
constexpr std::array<char16_t, 27> KANA_NORMAL = {
    u'\u306c', u'\u3075', u'\u3042', u'\u3046', u'\u3048',  // 1-5: nu fu a u e
    u'\u304a', u'\u3084', u'\u3086', u'\u3088', u'\u308f',  // 6-0: o ya yu yo wa
    u'\n', 0, u'\b', u'\t', u' ',                           // Enter, Esc, BS, Tab, Space
    u'\u307b', u'\u3078', u'\u309b', u'\u309c',             // ho he dakuten handakuten
    u'\u3080', u'\u3080',                                   // mu (0x31 mirrors 0x32 like JIS_NORMAL)
    u'\u308c', u'\u3051', 0,                                // re ke (0x35 unused)
    u'\u306d', u'\u308b', u'\u3081'                         // ne ru me
};
constexpr std::array<char16_t, 27> KANA_SHIFTED = {
    u'\u306c', u'\u3075', u'\u3041', u'\u3045', u'\u3047',  // nu fu small-a small-u small-e
    u'\u3049', u'\u3083', u'\u3085', u'\u3087', u'\u3092',  // small-o small-ya small-yu small-yo wo
    u'\n', 0, u'\b', u'\t', u' ',
    u'\u307b', u'\u3078', u'\u309b', u'\u300c',             // ho he dakuten open-bracket
    u'\u300d', u'\u300d',                                   // close-bracket
    u'\u308c', u'\u3051', 0,                                // re ke
    u'\u3001', u'\u3002', u'\u30fb'                         // comma period middle-dot
};
// Letters A..Z (keyCode 0x04..0x1d)
constexpr std::array<char16_t, 26> KANA_LETTERS = {
    u'\u3061', u'\u3053', u'\u305d', u'\u3057', u'\u3044', u'\u306f', // a chi, b ko, c so, d shi, e i, f ha
    u'\u304d', u'\u304f', u'\u306b', u'\u307e', u'\u306e', u'\u308a', // g ki, h ku, i ni, j ma, k no, l ri
    u'\u3082', u'\u307f', u'\u3089', u'\u305b', u'\u305f', u'\u3059', // m mo, n mi, o ra, p se, q ta, r su
    u'\u3068', u'\u304b', u'\u306a', u'\u3072', u'\u3066', u'\u3055', // s to, t ka, u na, v hi, w te, x sa
    u'\u3093', u'\u3064'                                              // y n, z tsu
};

KeyboardData snapshot() {
    KeyboardData data{};
    data.timestamp = sceKernelGetProcessTime();
    data.connected = connected;
    data.length = keyCount;
    data.led = led;
    data.modifier_key = modifiers;
    std::copy_n(keys.begin(), keyCount, data.key_code);
    return data;
}

void enqueue(const KeyboardData& data) {
    if (count == KEYBOARD_MAX_DATA_NUM) {
        head = (head + 1) % KEYBOARD_MAX_DATA_NUM;
        --count;
    }
    history[(head + count) % KEYBOARD_MAX_DATA_NUM] = data;
    ++count;
}

void clearKeys() {
    modifiers = 0;
    keyCount = 0;
}

bool updateKey(std::uint16_t keyCode, bool pressed) {
    if (keyCode >= KEYBOARD_KEY_LEFT_CTRL && keyCode <= KEYBOARD_KEY_RIGHT_GUI) {
        const std::uint32_t bit = 1u << (keyCode - KEYBOARD_KEY_LEFT_CTRL);
        const std::uint32_t updated = pressed ? modifiers | bit : modifiers & ~bit;
        if (updated == modifiers) return false;
        modifiers = updated;
        return true;
    }
    const auto end = keys.begin() + keyCount;
    const auto found = std::find(keys.begin(), end, keyCode);
    if (pressed) {
        if (found != end || keyCount == static_cast<int>(KEYBOARD_MAX_KEYCODES)) return false;
        keys[keyCount++] = keyCode;
        return true;
    }
    if (found == end) return false;
    std::copy(found + 1, end, found);
    --keyCount;
    return true;
}

std::uint16_t character(bool jis, std::uint32_t ledState, std::uint32_t modifierKey, std::uint16_t keyCode) {
    const bool shift = (modifierKey & (KEYBOARD_MOD_LEFT_SHIFT | KEYBOARD_MOD_RIGHT_SHIFT)) != 0;
    if (keyCode >= 0x04 && keyCode <= 0x1d) {
        const bool upper = shift != ((ledState & KEYBOARD_LED_CAPS_LOCK) != 0);
        return static_cast<std::uint16_t>((upper ? 'A' : 'a') + keyCode - 0x04);
    }
    if (keyCode >= 0x1e && keyCode <= 0x38) {
        const char* table = jis ? (shift ? JIS_SHIFTED : JIS_NORMAL) : (shift ? US_SHIFTED : US_NORMAL);
        return static_cast<unsigned char>(table[keyCode - 0x1e]);
    }
    if (jis && keyCode == 0x87) return shift ? '_' : '\\';
    if (jis && keyCode == 0x89) return shift ? '|' : '\\';
    switch (keyCode) {
        case 0x54: return '/';
        case 0x55: return '*';
        case 0x56: return '-';
        case 0x57: return '+';
        case 0x58: return '\n';
        default: break;
    }
    if (keyCode >= 0x59 && keyCode <= 0x63 && (ledState & KEYBOARD_LED_NUM_LOCK) != 0) {
        return static_cast<unsigned char>(KEYPAD_DIGITS[keyCode - 0x59]);
    }
    return 0;
}

std::uint16_t kanaCharacter(std::uint32_t ledState, std::uint32_t modifierKey, std::uint16_t keyCode) {
    const bool shift = (modifierKey & (KEYBOARD_MOD_LEFT_SHIFT | KEYBOARD_MOD_RIGHT_SHIFT)) != 0;
    if (keyCode >= 0x04 && keyCode <= 0x1d) {
        // Only E (small i) and Z (small tsu) have shifted forms; caps lock doesn't apply in kana mode.
        if (shift && keyCode == 0x08) return u'\u3043';
        if (shift && keyCode == 0x1d) return u'\u3063';
        return KANA_LETTERS[keyCode - 0x04];
    }
    if (keyCode >= 0x1e && keyCode <= 0x38) {
        return (shift ? KANA_SHIFTED : KANA_NORMAL)[keyCode - 0x1e];
    }
    if (keyCode == 0x87) return u'\u308d';  // ro (shift gives the same)
    if (keyCode == 0x89) return u'\u30fc';  // prolonged sound mark
    // Keypad, enter, etc. behave the same as in non-kana mode
    return character(true, ledState, modifierKey, keyCode);
}
}

namespace Keyboard {

int Initialize() {
    std::lock_guard lock(keyboardMutex);
    initialized = true;
    return KEYBOARD_OK;
}

int Open(int userId, std::int32_t type, std::int32_t index) {
    std::lock_guard lock(keyboardMutex);
    if (!initialized) return KEYBOARD_ERROR_NOT_INITIALIZED;
    if (userId < 0 || type != 0 || index != 0) return KEYBOARD_ERROR_INVALID_ARG;
    if (opened) return KEYBOARD_ERROR_ALREADY_OPENED;
    opened = true;
    clearKeys();
    head = 0;
    count = 0;
    enqueue(snapshot());
    return KEYBOARD_HANDLE;
}

int Close(std::int32_t handle) {
    std::lock_guard lock(keyboardMutex);
    if (!initialized) return KEYBOARD_ERROR_NOT_INITIALIZED;
    if (handle != KEYBOARD_HANDLE || !opened) return KEYBOARD_ERROR_INVALID_HANDLE;
    opened = false;
    clearKeys();
    head = 0;
    count = 0;
    return KEYBOARD_OK;
}

int Read(std::int32_t handle, KeyboardData* data, std::int32_t num) {
    std::lock_guard lock(keyboardMutex);
    if (!initialized) return KEYBOARD_ERROR_NOT_INITIALIZED;
    if (handle != KEYBOARD_HANDLE || !opened) return KEYBOARD_ERROR_INVALID_HANDLE;
    if (data == nullptr || num <= 0 || num > KEYBOARD_MAX_DATA_NUM) return KEYBOARD_ERROR_INVALID_ARG;
    const int available = std::min(count, num);
    for (int i = 0; i < available; ++i) {
        data[i] = history[head];
        head = (head + 1) % KEYBOARD_MAX_DATA_NUM;
    }
    count -= available;
    return available;
}

int ReadState(std::int32_t handle, KeyboardData* data) {
    std::lock_guard lock(keyboardMutex);
    if (!initialized) return KEYBOARD_ERROR_NOT_INITIALIZED;
    if (handle != KEYBOARD_HANDLE || !opened) return KEYBOARD_ERROR_INVALID_HANDLE;
    if (data == nullptr) return KEYBOARD_ERROR_INVALID_ARG;
    *data = snapshot();
    return KEYBOARD_OK;
}

int GetKey2Char(std::int32_t handle, std::int32_t arrange, std::uint32_t ledState, std::uint32_t modifierKey, std::uint16_t keyCode, KeyboardCharData* charData) {
    {
        std::lock_guard lock(keyboardMutex);
        if (!initialized) return KEYBOARD_ERROR_NOT_INITIALIZED;
        if (handle != KEYBOARD_HANDLE || !opened) return KEYBOARD_ERROR_INVALID_HANDLE;
    }
    if (charData == nullptr) return KEYBOARD_ERROR_INVALID_ARG;
    if (arrange != KEYBOARD_ARRANGEMENT_101 && arrange != KEYBOARD_ARRANGEMENT_106) return KEYBOARD_ERROR_INVALID_ARG;
    *charData = {};
    const bool is106 = arrange == KEYBOARD_ARRANGEMENT_106;
    if (is106 && (ledState & KEYBOARD_LED_KANA) != 0) {
        charData->char_code = kanaCharacter(ledState, modifierKey, keyCode);
    } else {
        charData->char_code = character(is106, ledState, modifierKey, keyCode);
    }
    charData->processed = charData->char_code != 0;
    charData->length = charData->processed ? 1 : 0;
    return KEYBOARD_OK;
}

void Publish(const KeyboardInputEvent& event) {
    std::lock_guard lock(keyboardMutex);
    if (event.connectionChange) {
        if (connected == event.connected) return;
        connected = event.connected;
        if (!connected) clearKeys();
    } else if (!connected) {
        return;
    }
    if (!opened) return;
    if (event.resetKeys) clearKeys();
    if (event.keyCode != 0) {
        const bool ledChanged = led != event.led;
        led = event.led;
        if (!updateKey(event.keyCode, event.pressed) && !ledChanged) return;
    }
    enqueue(snapshot());
}

}
