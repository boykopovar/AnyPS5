#pragma once

#include "SceTypes.hpp"
#include <cstdint>

namespace NKWebKit {
struct PointValue;
struct SizeValue;
struct KeyboardEvent;
struct MouseEvent;
struct WheelEvent;
}

extern "C" {

NKWebKit::KeyboardEvent APS5_VABI WKKeyboardEventMake(std::int32_t type, std::uint8_t inputType, const char* text, std::uint32_t length, const char* keyIdentifier, std::int32_t virtualKeyCode, const char* key, const char* code, std::int32_t caretOffset, std::uint32_t attributes, std::uint32_t modifiers);
NKWebKit::MouseEvent APS5_VABI WKMouseEventMake(std::int32_t type, std::int32_t button, NKWebKit::PointValue position, std::uint32_t modifiers);
NKWebKit::WheelEvent APS5_VABI WKWheelEventMake(std::int32_t type, NKWebKit::PointValue position, NKWebKit::SizeValue delta, NKWebKit::SizeValue wheelTicks, std::uint32_t modifiers);

}
