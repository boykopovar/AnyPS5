#include <cstdint>
#include <cstddef>
#include <cstring>
#include <map>
#include <mutex>
#include "SceTypes.hpp"

namespace {

constexpr int SCE_KEYBOARD_ERROR_INVALID_ARG = static_cast<int>(0x80DF0002);
constexpr int SCE_KEYBOARD_ERROR_INVALID_PORT = static_cast<int>(0x80DF0003);
constexpr int SCE_KEYBOARD_ERROR_INVALID_HANDLE = static_cast<int>(0x80DF0004);
constexpr int SCE_KEYBOARD_ERROR_NOT_INITIALIZED = static_cast<int>(0x80DF0007);
constexpr int SCE_KEYBOARD_ERROR_ALREADY_OPENED = static_cast<int>(0x80DF0008);

constexpr std::uint32_t ModifierShift = 0x01;
constexpr std::uint32_t ModifierControl = 0x02;
constexpr std::uint32_t ModifierCapsLock = 0x10;
constexpr std::uint16_t UsageA = 0x04;
constexpr std::uint16_t UsageZ = 0x1D;
constexpr std::uint16_t Usage1 = 0x1E;
constexpr std::uint16_t Usage9 = 0x26;
constexpr std::uint16_t Usage0 = 0x27;
constexpr std::uint16_t UsageReturn = 0x28;
constexpr std::uint16_t UsageEscape = 0x29;
constexpr std::uint16_t UsageBackspace = 0x2A;
constexpr std::uint16_t UsageTab = 0x2B;
constexpr std::uint16_t UsageSpace = 0x2C;

struct Handle {
    int userId = 0;
    std::int32_t type = 0;
    std::int32_t index = 0;
};

std::mutex g_lock;
bool g_initialised = false;
std::map<std::int32_t, Handle> g_handles;
std::int32_t g_nextHandle = 1;

std::uint16_t shiftedSymbol(const std::uint16_t code) {
    switch (code) {
    case Usage1: return '!';
    case Usage1 + 1: return '@';
    case Usage1 + 2: return '#';
    case Usage1 + 3: return '$';
    case Usage1 + 4: return '%';
    case Usage1 + 5: return '^';
    case Usage1 + 6: return '&';
    case Usage1 + 7: return '*';
    case Usage9: return '(';
    case Usage0: return ')';
    default: return 0;
    }
}

std::uint16_t translate(const std::uint16_t code, const std::uint32_t modifier, const std::uint32_t led) {
    const bool shift = (modifier & ModifierShift) != 0;
    const bool caps = (led & ModifierCapsLock) != 0;
    if (code >= UsageA && code <= UsageZ) {
        const auto letter = static_cast<std::uint16_t>(code - UsageA);
        const bool upper = shift != caps;
        return static_cast<std::uint16_t>((upper ? 'A' : 'a') + letter);
    }
    if (code >= Usage1 && code <= Usage0) {
        if (shift) return shiftedSymbol(code);
        return code == Usage0 ? static_cast<std::uint16_t>('0') : static_cast<std::uint16_t>('1' + (code - Usage1));
    }
    switch (code) {
    case UsageReturn: return '\n';
    case UsageEscape: return 0x1B;
    case UsageBackspace: return '\b';
    case UsageTab: return '\t';
    case UsageSpace: return ' ';
    default: return 0;
    }
}

void fillDisconnected(KeyboardData* data) {
    std::memset(data, 0, sizeof(KeyboardData));
    data->connected = false;
    data->length = 0;
}

}

extern "C" {

int APS5_VABI sceKeyboardInit() {
    std::lock_guard lock(g_lock);
    g_initialised = true;
    return 0;
}

int APS5_VABI sceKeyboardOpen(int user_id, std::int32_t type, std::int32_t index, const void* param) {
    (void)param;
    if (index != 0) return SCE_KEYBOARD_ERROR_INVALID_PORT;
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_KEYBOARD_ERROR_NOT_INITIALIZED;
    for (const auto& [handle, open] : g_handles) {
        if (open.userId == user_id && open.type == type && open.index == index) return SCE_KEYBOARD_ERROR_ALREADY_OPENED;
    }
    const auto handle = g_nextHandle++;
    g_handles[handle] = Handle{user_id, type, index};
    return handle;
}

int APS5_VABI sceKeyboardClose(std::int32_t handle) {
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_KEYBOARD_ERROR_NOT_INITIALIZED;
    return g_handles.erase(handle) != 0 ? 0 : SCE_KEYBOARD_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceKeyboardReadState(std::int32_t handle, KeyboardData* data) {
    if (data == nullptr) return SCE_KEYBOARD_ERROR_INVALID_ARG;
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_KEYBOARD_ERROR_NOT_INITIALIZED;
    if (!g_handles.contains(handle)) return SCE_KEYBOARD_ERROR_INVALID_HANDLE;
    fillDisconnected(data);
    return 0;
}

int APS5_VABI sceKeyboardRead(std::int32_t handle, KeyboardData* data, std::int32_t num) {
    if (data == nullptr || num <= 0) return SCE_KEYBOARD_ERROR_INVALID_ARG;
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_KEYBOARD_ERROR_NOT_INITIALIZED;
    if (!g_handles.contains(handle)) return SCE_KEYBOARD_ERROR_INVALID_HANDLE;
    fillDisconnected(data);
    return 0;
}

int APS5_VABI sceKeyboardGetKey2Char(std::int32_t handle, std::int32_t arrange, std::uint32_t led, std::uint32_t modifier_key,
                                     std::uint16_t key_code, KeyboardCharData* char_data) {
    (void)arrange;
    if (char_data == nullptr) return SCE_KEYBOARD_ERROR_INVALID_ARG;
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_KEYBOARD_ERROR_NOT_INITIALIZED;
    if (!g_handles.contains(handle)) return SCE_KEYBOARD_ERROR_INVALID_HANDLE;
    std::memset(char_data, 0, sizeof(KeyboardCharData));
    if ((modifier_key & ModifierControl) != 0) return 0;
    const auto character = translate(key_code, modifier_key, led);
    if (character == 0) return 0;
    char_data->processed = true;
    char_data->length = 1;
    char_data->char_code = character;
    return 0;
}

}
