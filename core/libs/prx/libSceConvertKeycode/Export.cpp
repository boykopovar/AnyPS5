#include <cstdint>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "HitLog.hpp"
#include "prx/libc/include/General.hpp"

namespace {
constexpr int kErrInvalidAddress = static_cast<int>(0x80BC0031u);
constexpr int kErrInvalidUserId = static_cast<int>(0x80BC0010u);
constexpr std::uint32_t kIgnoredStateBits = 0x4000000u;

char16_t UsCharacter(std::uint16_t keycode) {
    static constexpr char16_t kPunctuation[] = u"-=[]\\\0;'`,./";
    static constexpr char16_t kKeypad[] = u"/*-+\0" u"1234567890.";
    if (keycode >= 0x04 && keycode <= 0x1D) return static_cast<char16_t>(u'a' + (keycode - 0x04));
    if (keycode >= 0x1E && keycode <= 0x26) return static_cast<char16_t>(u'1' + (keycode - 0x1E));
    if (keycode == 0x27) return u'0';
    if (keycode == 0x2C) return u' ';
    if (keycode >= 0x2D && keycode <= 0x38) return kPunctuation[keycode - 0x2D];
    if (keycode == 0x64) return u'\\';
    if (keycode >= 0x54 && keycode <= 0x63) return kKeypad[keycode - 0x54];
    return 0;
}
}  // namespace

extern "C" {

int APS5_VABI sceConvertKeycodeGetImeKeyboardType(int32_t userId, uint32_t* type) {
 if (type == nullptr) return kErrInvalidAddress;
 if (userId < 0) return kErrInvalidUserId;
 *type = 0;
 return 0;
}
int APS5_VABI sceConvertKeycodeGetVirtualKeycode(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
 APS5_HIT("CONVERTKEYCODE", "sceConvertKeycodeGetVirtualKeycode args 0x%llx 0x%llx 0x%llx 0x%llx -> error (signature unknown)",
          static_cast<unsigned long long>(a0), static_cast<unsigned long long>(a1), static_cast<unsigned long long>(a2), static_cast<unsigned long long>(a3));
 return kErrInvalidAddress;
}

int APS5_VABI sceConvertKeycodeGetCharacterFromKeyboardData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceConvertKeycodeGetCharacter(std::uint16_t keycode, std::uint32_t state, std::uint32_t type, char16_t* character) {
    if (character == nullptr) return kErrInvalidAddress;
    if ((state & ~kIgnoredStateBits) != 0) throw std::runtime_error("sceConvertKeycodeGetCharacter: unsupported state " + std::to_string(state));
    if (type != 0) throw std::runtime_error("sceConvertKeycodeGetCharacter: unsupported keyboard type " + std::to_string(type));
    *character = UsCharacter(keycode);
    return 0;
}

}
