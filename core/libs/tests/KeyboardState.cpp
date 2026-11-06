#include "SceTypes.hpp"

#include <cstdint>
#include <cstdio>

extern "C" {
int APS5_VABI sceKeyboardInit();
int APS5_VABI sceKeyboardOpen(int user_id, std::int32_t type, std::int32_t index, const void* param);
int APS5_VABI sceKeyboardClose(std::int32_t handle);
int APS5_VABI sceKeyboardRead(std::int32_t handle, KeyboardData* data, std::int32_t num);
int APS5_VABI sceKeyboardReadState(std::int32_t handle, KeyboardData* data);
int APS5_VABI sceKeyboardGetKey2Char(std::int32_t handle, std::int32_t arrange, std::uint32_t led, std::uint32_t modifier_key,
                                     std::uint16_t key_code, KeyboardCharData* char_data);
}

namespace {

constexpr std::uint32_t Shift = 0x01;
constexpr std::uint32_t Control = 0x02;
constexpr std::uint32_t CapsLed = 0x10;

int failures = 0;

void Require(const bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "KeyboardState: %s\n", what);
    ++failures;
}

std::uint16_t Char(const std::int32_t handle, const std::uint16_t code, const std::uint32_t modifier, const std::uint32_t led) {
    KeyboardCharData data{};
    if (sceKeyboardGetKey2Char(handle, 0, led, modifier, code, &data) != 0) return 0xFFFF;
    return data.processed ? data.char_code : 0;
}

}

int main() {
    KeyboardData data{};
    Require(sceKeyboardOpen(0, 0, 0, nullptr) < 0, "a keyboard opened before initialisation");
    Require(sceKeyboardInit() == 0, "the library did not initialise");

    const auto handle = sceKeyboardOpen(0, 0, 0, nullptr);
    Require(handle > 0, "the keyboard did not open");
    Require(sceKeyboardOpen(0, 0, 0, nullptr) < 0, "the same keyboard opened twice");
    Require(sceKeyboardOpen(1, 0, 0, nullptr) > 0, "a second user could not open a keyboard");
    Require(sceKeyboardOpen(0, 0, 3, nullptr) < 0, "an out of range port was accepted");

    Require(sceKeyboardReadState(handle, &data) == 0, "the state was refused");
    Require(!data.connected && data.length == 0, "a keyboard is reported without a device behind it");
    Require(sceKeyboardReadState(handle, nullptr) != 0, "a null output was accepted");
    Require(sceKeyboardReadState(9999, &data) != 0, "an unknown handle answered");
    Require(sceKeyboardRead(handle, &data, 1) == 0, "the buffered read was refused");
    Require(sceKeyboardRead(handle, &data, 0) != 0, "a zero length read was accepted");

    Require(Char(handle, 0x04, 0, 0) == 'a', "an unmodified letter is wrong");
    Require(Char(handle, 0x04, Shift, 0) == 'A', "shift does not produce a capital");
    Require(Char(handle, 0x04, 0, CapsLed) == 'A', "caps lock does not produce a capital");
    Require(Char(handle, 0x04, Shift, CapsLed) == 'a', "shift with caps lock does not cancel out");
    Require(Char(handle, 0x1D, 0, 0) == 'z', "the last letter is wrong");
    Require(Char(handle, 0x1E, 0, 0) == '1' && Char(handle, 0x27, 0, 0) == '0', "the digit row is wrong");
    Require(Char(handle, 0x1E, Shift, 0) == '!' && Char(handle, 0x27, Shift, 0) == ')', "the shifted digit row is wrong");
    Require(Char(handle, 0x2C, 0, 0) == ' ' && Char(handle, 0x28, 0, 0) == '\n', "space or return are wrong");
    Require(Char(handle, 0x04, Control, 0) == 0, "a control combination produced a character");
    Require(Char(handle, 0x65, 0, 0) == 0, "an unmapped key produced a character");

    Require(sceKeyboardClose(handle) == 0, "the keyboard did not close");
    Require(sceKeyboardClose(handle) != 0, "the keyboard closed twice");
    Require(sceKeyboardReadState(handle, &data) != 0, "a closed handle still reads");

    if (failures != 0) return 1;
    std::printf("Keyboard state tests passed\n");
    return 0;
}
