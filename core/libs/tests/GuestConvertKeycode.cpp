#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

extern "C" {
int APS5_VABI sceConvertKeycodeGetImeKeyboardType(std::int32_t, std::uint32_t*);
int APS5_VABI sceConvertKeycodeGetCharacter(std::uint16_t, std::uint32_t, std::uint32_t, char16_t*);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr int invalidAddress = static_cast<int>(0x80BC0031u);
    constexpr int invalidUserId = static_cast<int>(0x80BC0010u);

    Require(sceConvertKeycodeGetImeKeyboardType(0, nullptr) == invalidAddress);
    Require(sceConvertKeycodeGetImeKeyboardType(-1, nullptr) == invalidAddress);

    std::uint32_t type = 0xFFFFFFFFu;
    Require(sceConvertKeycodeGetImeKeyboardType(-1, &type) == invalidUserId);
    Require(type == 0xFFFFFFFFu);

    Require(sceConvertKeycodeGetImeKeyboardType(0, &type) == 0);
    Require(type == 0);

    type = 0xFFFFFFFFu;
    Require(sceConvertKeycodeGetImeKeyboardType(1, &type) == 0);
    Require(type == 0);
    for (std::uint32_t state : {0u, 0x4000000u}) {
        char16_t character[3] = {u'?', 0, 0};
        Require(sceConvertKeycodeGetCharacter(0x04, state, 0, character) == 0);
        Require(character[0] == u'a');
        Require(sceConvertKeycodeGetCharacter(0x1E, state, 0, character) == 0);
        Require(character[0] == u'1');
        Require(sceConvertKeycodeGetCharacter(0x3A, state, 0, character) == 0);
        Require(character[0] == 0);
        Require(sceConvertKeycodeGetCharacter(0x64, state, 0, character) == 0);
        Require(character[0] == u'\\');
        Require(sceConvertKeycodeGetCharacter(0x32, state, 0, character) == 0);
        Require(character[0] == 0);
    }
    return 0;
}
