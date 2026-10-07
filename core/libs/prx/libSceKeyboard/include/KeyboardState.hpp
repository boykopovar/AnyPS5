#ifndef CORE_LIBS_PRX_LIBSCEKEYBOARD_KEYBOARDSTATE_HPP
#define CORE_LIBS_PRX_LIBSCEKEYBOARD_KEYBOARDSTATE_HPP

#include "prx/libSceKeyboard/include/keyboard_structs.h"

struct KeyboardInputEvent {
    std::uint16_t keyCode = 0;
    bool pressed = false;
    std::uint32_t led = 0;
    bool resetKeys = false;
    bool connectionChange = false;
    bool connected = true;
};

namespace Keyboard {
int Initialize();
int Open(int userId, std::int32_t type, std::int32_t index);
int Close(std::int32_t handle);
int Read(std::int32_t handle, KeyboardData* data, std::int32_t num);
int ReadState(std::int32_t handle, KeyboardData* data);
int GetKey2Char(std::int32_t handle, std::int32_t arrange, std::uint32_t led, std::uint32_t modifierKey,
                std::uint16_t keyCode, KeyboardCharData* charData);
void Publish(const KeyboardInputEvent& event);
}

extern "C" void KeyboardPublishInput_nid_postfix(const KeyboardInputEvent& event);

#endif
