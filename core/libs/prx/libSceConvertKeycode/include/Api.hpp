#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceConvertKeycodeGetImeKeyboardType(int32_t userId, uint32_t* type);
int APS5_VABI sceConvertKeycodeGetVirtualKeycode(std::uint16_t keycode, std::uint32_t keyboardType, std::uint16_t* virtualKeycode);
int APS5_VABI sceConvertKeycodeGetCharacterFromKeyboardData(const KeyboardData* keyData, std::int32_t index, std::uint32_t state, std::uint32_t keyboardType, char16_t* character);
int APS5_VABI sceConvertKeycodeGetCharacter(std::uint16_t keycode, std::uint32_t state, std::uint32_t type, char16_t* character);

}
