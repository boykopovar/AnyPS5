#include "prx/libSceConvertKeycode/include/Api.hpp"
#include "SceTypes.hpp"
#include "ProcessParametersFixture.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
#ifdef _WIN32
    if (_putenv_s("ANYPS5_LANGUAGE", "") != 0) return 2;
#else
    if (::unsetenv("ANYPS5_LANGUAGE") != 0) return 2;
#endif
    static std::array<std::byte, 64> process{};
    constexpr std::uint32_t sdkVersion = 0x09000000u;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    if (!ProcessParametersFixture::Install(process.data(), process.size())) return 2;
    std::uint32_t type = 0xFFFFFFFFu;
    std::uint16_t virtualKey = 0;
    char16_t character = 0;
    if (sceConvertKeycodeGetVirtualKeycode(0, 0, nullptr) != static_cast<int>(0x80BC0031u)) return 1;
    if (sceConvertKeycodeGetVirtualKeycode(0, 0, &virtualKey) != static_cast<int>(0x80BC001Du)) return 1;
    if (sceConvertKeycodeGetVirtualKeycode(0, 40, &virtualKey) != static_cast<int>(0x80BC001Du)) return 1;
    if (sceConvertKeycodeGetCharacter(0, 0, 0, nullptr) != static_cast<int>(0x80BC0031u)) return 1;
    if (sceConvertKeycodeGetCharacter(0, 0, 0, &character) != static_cast<int>(0x80BC001Du)) return 1;
    if (sceConvertKeycodeGetCharacter(0, 0, 40, &character) != static_cast<int>(0x80BC001Du)) return 1;
    if (sceConvertKeycodeGetImeKeyboardType(0x10000000, nullptr) != static_cast<int>(0x80BC0031u)) return 1;
    if (argc > 1 && std::strcmp(argv[1], "ps4") == 0) {
        constexpr int rejected = static_cast<int>(0x80BC001Du);
        KeyboardData keyData{};
        keyData.length = 1;
        keyData.key_code[0] = 4;
        const auto setSdkVersion = [](std::uint32_t version) { std::memcpy(process.data() + 16, &version, sizeof(version)); };
        setSdkVersion(0x04FFFFFFu);
        virtualKey = 0xABCD;
        character = u'?';
        if (sceConvertKeycodeGetVirtualKeycode(28, 31, &virtualKey) != rejected || virtualKey != 0xABCD) return 1;
        if (sceConvertKeycodeGetCharacter(28, 0, 31, &character) != rejected || character != u'?') return 1;
        if (sceConvertKeycodeGetCharacterFromKeyboardData(&keyData, 0, 0, 31, &character) != rejected || character != u'?') return 1;
        setSdkVersion(0x05000000u);
        if (sceConvertKeycodeGetVirtualKeycode(28, 31, &virtualKey) != 0 || virtualKey != 89) return 1;
        if (sceConvertKeycodeGetVirtualKeycode(28, 38, &virtualKey) != rejected) return 1;
        setSdkVersion(0x08FFFFFFu);
        if (sceConvertKeycodeGetVirtualKeycode(28, 2, &virtualKey) != 0 || virtualKey != 89) return 1;
        setSdkVersion(0x09000000u);
        if (sceConvertKeycodeGetVirtualKeycode(28, 2, &virtualKey) != 0 || virtualKey != 90) return 1;
        setSdkVersion(0x0FFFFFFFu);
        if (sceConvertKeycodeGetVirtualKeycode(28, 38, &virtualKey) != rejected) return 1;
        setSdkVersion(0x10000000u);
        if (sceConvertKeycodeGetVirtualKeycode(28, 38, &virtualKey) != 0 || virtualKey != 89) return 1;
        setSdkVersion(0xFFFFFFFFu);
        if (sceConvertKeycodeGetVirtualKeycode(28, 39, &virtualKey) != rejected) return 1;
        if (sceConvertKeycodeGetCharacter(28, 0, 39, &character) != rejected) return 1;
        setSdkVersion(sdkVersion);
    }
    return sceConvertKeycodeGetImeKeyboardType(0x10000000, &type) == 0 && type == 4 ? 0 : 1;
}
