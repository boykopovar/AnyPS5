#ifndef CORE_LIBS_PRX_LIBSCEKEYBOARD_KEYBOARD_STRUCTS_H
#define CORE_LIBS_PRX_LIBSCEKEYBOARD_KEYBOARD_STRUCTS_H

#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"

constexpr int KEYBOARD_OK = 0;
constexpr int KEYBOARD_ERROR_INVALID_ARG = static_cast<int>(0x80da0001u);
constexpr int KEYBOARD_ERROR_INVALID_HANDLE = static_cast<int>(0x80da0003u);
constexpr int KEYBOARD_ERROR_ALREADY_OPENED = static_cast<int>(0x80da0004u);
constexpr int KEYBOARD_ERROR_NOT_INITIALIZED = static_cast<int>(0x80da0005u);
constexpr int KEYBOARD_MAX_DATA_NUM = 16;
constexpr int KEYBOARD_HANDLE = 1;
constexpr std::int32_t KEYBOARD_ARRANGEMENT_101 = 0;
constexpr std::int32_t KEYBOARD_ARRANGEMENT_106 = 1;

constexpr std::uint32_t KEYBOARD_LED_NUM_LOCK = 1;
constexpr std::uint32_t KEYBOARD_LED_CAPS_LOCK = 2;
constexpr std::uint32_t KEYBOARD_LED_SCROLL_LOCK = 4;
constexpr std::uint32_t KEYBOARD_LED_KANA = 16;
constexpr std::uint32_t KEYBOARD_MOD_LEFT_SHIFT = 2;
constexpr std::uint32_t KEYBOARD_MOD_RIGHT_SHIFT = 32;

constexpr std::uint16_t KEYBOARD_KEY_LEFT_CTRL = 0xe0;
constexpr std::uint16_t KEYBOARD_KEY_RIGHT_GUI = 0xe7;

static_assert(sizeof(KeyboardData) == 96);
static_assert(alignof(KeyboardData) == 8);
static_assert(offsetof(KeyboardData, timestamp) == 0);
static_assert(offsetof(KeyboardData, intercepted) == 8);
static_assert(offsetof(KeyboardData, connected) == 16);
static_assert(offsetof(KeyboardData, length) == 20);
static_assert(offsetof(KeyboardData, led) == 24);
static_assert(offsetof(KeyboardData, modifier_key) == 28);
static_assert(offsetof(KeyboardData, key_code) == 32);
static_assert(offsetof(KeyboardData, reserve2) == 64);
static_assert(sizeof(KeyboardCharData) == 20);
static_assert(offsetof(KeyboardCharData, length) == 4);
static_assert(offsetof(KeyboardCharData, char_code) == 8);

#endif
