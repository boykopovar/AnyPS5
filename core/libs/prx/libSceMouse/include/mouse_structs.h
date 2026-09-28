#ifndef CORE_LIBS_PRX_LIBSCEMOUSE_MOUSE_STRUCTS_H
#define CORE_LIBS_PRX_LIBSCEMOUSE_MOUSE_STRUCTS_H

#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"

struct MouseOpenParam {
    std::uint8_t behaviorFlag;
    std::uint8_t reserved[7];
};

constexpr int MOUSE_OK = 0;
constexpr int MOUSE_ERROR_INVALID_ARG = static_cast<int>(0x80df0001u);
constexpr int MOUSE_ERROR_INVALID_HANDLE = static_cast<int>(0x80df0003u);
constexpr int MOUSE_ERROR_ALREADY_OPENED = static_cast<int>(0x80df0004u);
constexpr int MOUSE_ERROR_NOT_INITIALIZED = static_cast<int>(0x80df0005u);
constexpr int MOUSE_MAX_DATA_NUM = 64;
constexpr int MOUSE_HANDLE = 1;
constexpr std::uint8_t MOUSE_OPEN_PARAM_MERGED = 1;

static_assert(sizeof(MouseOpenParam) == 8);
static_assert(sizeof(MouseData) == 40);
static_assert(alignof(MouseData) == 8);
static_assert(offsetof(MouseData, timestamp) == 0);
static_assert(offsetof(MouseData, connected) == 8);
static_assert(offsetof(MouseData, buttons) == 12);
static_assert(offsetof(MouseData, x_axis) == 16);
static_assert(offsetof(MouseData, y_axis) == 20);
static_assert(offsetof(MouseData, wheel) == 24);
static_assert(offsetof(MouseData, tilt) == 28);
static_assert(offsetof(MouseData, reserved) == 32);

#endif
