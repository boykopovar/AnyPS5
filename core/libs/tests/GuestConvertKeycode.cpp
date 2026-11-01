#include "prx/libSceConvertKeycode/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "ProcessParametersFixture.hpp"
#include "prx/libSceConvertKeycode/ImeKeyboardType.hpp"
#include <array>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>

static void Require(bool value) { if (!value) std::abort(); }

static_assert(ImeKeyboardType::LookUp(INT32_MIN, 0, 0) == 0);
static_assert(ImeKeyboardType::LookUp(INT32_MAX, 0, 0) == 0);

static void SetLanguage(const char* value) {
#ifdef _WIN32
    Require(_putenv_s("ANYPS5_LANGUAGE", value != nullptr ? value : "") == 0);
#else
    Require((value != nullptr ? ::setenv("ANYPS5_LANGUAGE", value, 1) : ::unsetenv("ANYPS5_LANGUAGE")) == 0);
#endif
}

namespace {

struct FakeUser {
    int keyboardResult = 0;
    std::int32_t keyboardType = 0;
    int japaneseResult = 0;
    std::int32_t japaneseVariant = 0;
    int chineseResult = 0;
    std::int32_t chineseVariant = 0;
    int calls = 0;
};

int Resolve(const FakeUser& user, std::uint32_t* type, std::uint32_t sdkVersion = 0xFFFFFFFFu, bool ps4Client = false) {
    return ImeKeyboardType::Resolve(
        7, type, sdkVersion, ps4Client,
        [&](std::int32_t id, std::int32_t* value) { Require(id == 7); *value = user.keyboardType; return user.keyboardResult; },
        [&](std::int32_t id, std::int32_t* value) { Require(id == 7); *value = user.japaneseVariant; return user.japaneseResult; },
        [&](std::int32_t id, std::int32_t* value) { Require(id == 7); *value = user.chineseVariant; return user.chineseResult; });
}

std::uint32_t Map(std::int32_t keyboardType, std::int32_t japanese = 0, std::int32_t chinese = 0, std::uint32_t sdkVersion = 0xFFFFFFFFu, bool ps4Client = false) {
    FakeUser user;
    user.keyboardType = keyboardType;
    user.japaneseVariant = japanese;
    user.chineseVariant = chinese;
    std::uint32_t type = 0xFFFFFFFFu;
    Require(Resolve(user, &type, sdkVersion, ps4Client) == 0);
    return type;
}

void RequireImeKeyboardTypeMapping() {
    // The owner's table: UserService keyboard type -> IME keyboard type.
    constexpr std::pair<int, int> table[] = {
        {16, 1}, {17, 2}, {18, 3}, {19, 5}, {20, 4}, {21, 6}, {22, 7}, {23, 8}, {24, 10}, {25, 11}, {26, 9}, {27, 12},
        {28, 13}, {29, 14}, {30, 15}, {31, 16}, {32, 17}, {33, 18}, {34, 19}, {35, 20}, {36, 21}, {38, 24}, {40, 25},
        {41, 30}, {42, 31}, {43, 33}, {44, 32}, {45, 34}, {46, 35}, {47, 36}, {48, 37}, {49, 38}};
    for (const auto& [keyboard, ime] : table) {
        // 49 -> 38 is compatibility-sensitive; the table row is checked on a new SDK, the rule separately below.
        Require(Map(keyboard) == static_cast<std::uint32_t>(ime));
    }
    // Unmatched keyboard types succeed with IME type 0 (including 0, the gaps, and out-of-range values).
    for (int keyboard : {-1, 0, 1, 15, 50, 51, 1000, 0x7FFFFFFF, static_cast<int>(0x80000000u)}) Require(Map(keyboard) == 0);

    // Type 37: Japanese input variant; anything else is unmatched.
    Require(Map(37, 0) == 22 && Map(37, 1) == 23);
    Require(Map(37, 2) == 0 && Map(37, -1) == 0);
    Require(Map(37, 0, 4) == 22);  // the Traditional Chinese variant is ignored for 37
    // Type 39: Traditional Chinese input variant.
    Require(Map(39, 0, 0) == 26 && Map(39, 0, 1) == 27 && Map(39, 0, 2) == 28 && Map(39, 0, 3) == 29 && Map(39, 0, 4) == 39);
    Require(Map(39, 0, 5) == 0 && Map(39, 0, -1) == 0);
    Require(Map(39, 1, 0) == 26);  // the Japanese variant is ignored for 39

    // Variant query failure falls back to variant 0 and still succeeds; other types never query the variants.
    FakeUser user;
    std::uint32_t type = 0xFFFFFFFFu;
    user.keyboardType = 37; user.japaneseResult = static_cast<int>(0x80960001u); user.japaneseVariant = 1;
    Require(Resolve(user, &type) == 0 && type == 22);
    user = {};
    type = 0xFFFFFFFFu;
    user.keyboardType = 39; user.chineseResult = static_cast<int>(0x80960001u); user.chineseVariant = 4;
    Require(Resolve(user, &type) == 0 && type == 26);
    user = {};
    type = 0xFFFFFFFFu;
    user.keyboardType = 20; user.japaneseResult = 1; user.chineseResult = 1;
    Require(Resolve(user, &type) == 0 && type == 4);

    // SDK compatibility, applied after the lookup. IME 38 (from 49): PS5 SDK < 0x06000000, PS4 SDK < 0x10000000 -> 4.
    Require(Map(49, 0, 0, 0x05FFFFFFu, false) == 4);
    Require(Map(49, 0, 0, 0x06000000u, false) == 38);
    Require(Map(49, 0, 0, 0x0FFFFFFFu, true) == 4);
    Require(Map(49, 0, 0, 0x10000000u, true) == 38);
    // IME 39 (from 39 variant 4): PS5 SDK < 0x08000000 and every PS4 client -> 29.
    Require(Map(39, 0, 4, 0x07FFFFFFu, false) == 29);
    Require(Map(39, 0, 4, 0x08000000u, false) == 39);
    Require(Map(39, 0, 4, 0xFFFFFFFFu, true) == 29);
    // Other IME types are never rewritten.
    Require(Map(20, 0, 0, 0, false) == 4 && Map(36, 0, 0, 0, true) == 21 && Map(39, 0, 3, 0, true) == 29);

    // Errors leave the output untouched.
    type = 0x1234u;
    user = {};
    Require(ImeKeyboardType::Resolve(7, nullptr, 0, false, [](std::int32_t, std::int32_t*) { Require(false); return 0; }, [](std::int32_t, std::int32_t*) { return 0; }, [](std::int32_t, std::int32_t*) { return 0; }) == static_cast<int>(0x80BC0031u));
    user.keyboardResult = static_cast<int>(0x80960105u);
    Require(Resolve(user, &type) == static_cast<int>(0x80BC0010u) && type == 0x1234u);
    for (int other : {1, -1, static_cast<int>(0x80960002u), static_cast<int>(0x80960005u), static_cast<int>(0x80BC0010u)}) {
        user.keyboardResult = other;
        Require(Resolve(user, &type) == static_cast<int>(0x80BC00FFu) && type == 0x1234u);
    }
}

}  // namespace

int main() {
    static std::array<std::byte, 64> process{};
    Require(ProcessParametersFixture::Install(process.data(), process.size()));
    constexpr std::uint32_t sdkVersion = 0x09000000u;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    constexpr int invalidAddress = static_cast<int>(0x80BC0031u);
    constexpr int invalidUserId = static_cast<int>(0x80BC0010u);

    Require(sceConvertKeycodeGetImeKeyboardType(0, nullptr) == invalidAddress);
    Require(sceConvertKeycodeGetImeKeyboardType(-1, nullptr) == invalidAddress);

    std::uint16_t virtualKey = 0xABCD;
    Require(sceConvertKeycodeGetVirtualKeycode(4, 0, nullptr) == invalidAddress);
    Require(sceConvertKeycodeGetVirtualKeycode(4, 0, &virtualKey) == static_cast<int>(0x80BC001Du));
    Require(virtualKey == 0xABCD);
    Require(sceConvertKeycodeGetVirtualKeycode(4, 40, &virtualKey) == static_cast<int>(0x80BC001Du));
    Require(sceConvertKeycodeGetVirtualKeycode(4, 4, &virtualKey) == 0 && virtualKey == 65);
    Require(sceConvertKeycodeGetVirtualKeycode(4, 9, &virtualKey) == 0 && virtualKey == 81);
    Require(sceConvertKeycodeGetVirtualKeycode(100, 4, &virtualKey) == 0 && virtualKey == 255);
    Require(sceConvertKeycodeGetVirtualKeycode(136, 24, &virtualKey) == 0 && virtualKey == 21);
    Require(sceConvertKeycodeGetVirtualKeycode(65535, 4, &virtualKey) == 0 && virtualKey == 255);
    for (std::uint32_t layout : {2u, 3u, 12u, 31u, 39u}) {
        virtualKey = 0xABCD;
        bool rejected = false;
        try {
            sceConvertKeycodeGetVirtualKeycode(28, layout, &virtualKey);

        } catch (const std::runtime_error&) {
            rejected = true;
        }
        Require(rejected && virtualKey == 0xABCD);
    }

    // The exported function with the real UserService: unknown users fail, the initial user reports the US-English default (keyboard type 20 -> IME 4).
    std::uint32_t type = 0xFFFFFFFFu;
    Require(sceConvertKeycodeGetImeKeyboardType(-1, &type) == invalidUserId);
    Require(sceConvertKeycodeGetImeKeyboardType(0, &type) == invalidUserId);
    Require(type == 0xFFFFFFFFu);
    SetLanguage(nullptr);
    Require(sceConvertKeycodeGetImeKeyboardType(0x10000000, &type) == 0 && type == 4);
    constexpr std::uint32_t imeTypeByLanguage[] = {22, 4, 9, 6, 2, 13, 14, 18, 19, 24, 26, 25, 8, 20, 1, 15, 16, 17, 5, 21, 7, 30, 11, 32, 37, 33, 36, 31, 35, 34};
    for (int language = 0; language < 30; ++language) {
        SetLanguage(std::to_string(language).c_str());
        type = 0xFFFFFFFFu;
        Require(sceConvertKeycodeGetImeKeyboardType(0x10000000, &type) == 0 && type == imeTypeByLanguage[language]);
    }
    SetLanguage("30");
    Require(sceConvertKeycodeGetImeKeyboardType(0x10000000, &type) == 0 && type == 4);
    SetLanguage(nullptr);

    Require(ImeKeyboardType::IsPs4TitleId("CUSA00000"));
    Require(ImeKeyboardType::IsPs4TitleId("CUSA"));
    Require(!ImeKeyboardType::IsPs4TitleId("PPSA00000"));
    Require(!ImeKeyboardType::IsPs4TitleId(""));
    Require(!ImeKeyboardType::IsPs4TitleId("CUS"));
    Require(!ImeKeyboardType::IsPs4TitleId("cusa00000"));
    Require(!ImeKeyboardType::IsPs4TitleId("XCUSA0000"));
    Require(!ImeKeyboardType::IsPs4TitleId("HOMEBREW1"));

    RequireImeKeyboardTypeMapping();
    Require(sceConvertKeycodeGetCharacter(4, 0, 0, nullptr) == invalidAddress);
    char16_t character = u'?';
    Require(sceConvertKeycodeGetCharacter(4, 0, 0, &character) == static_cast<int>(0x80BC001Du));
    Require(character == u'?');
    Require(sceConvertKeycodeGetCharacter(4, 0, 40, &character) == static_cast<int>(0x80BC001Du));
    for (std::uint32_t state : {0u, 0x4000000u}) {
        Require(sceConvertKeycodeGetCharacter(4, state, 4, &character) == 0 && character == u'a');
        Require(sceConvertKeycodeGetCharacter(30, state, 4, &character) == 0 && character == u'1');
        Require(sceConvertKeycodeGetCharacter(58, state, 4, &character) == 0 && character == 0);
        Require(sceConvertKeycodeGetCharacter(65535, state, 4, &character) == 0 && character == 0);
    }
    Require(sceConvertKeycodeGetCharacter(4, 0x200, 4, &character) == 0 && character == u'A');
    Require(sceConvertKeycodeGetCharacter(4, 0x20000, 4, &character) == 0 && character == u'A');
    Require(sceConvertKeycodeGetCharacter(4, 0x20200, 4, &character) == 0 && character == u'a');
    Require(sceConvertKeycodeGetCharacter(30, 0x200, 4, &character) == 0 && character == u'!');
    Require(sceConvertKeycodeGetCharacter(89, 0, 4, &character) == 0 && character == 0);
    Require(sceConvertKeycodeGetCharacter(89, 0x10000, 4, &character) == 0 && character == u'1');
    Require(sceConvertKeycodeGetCharacter(89, 0x10200, 4, &character) == 0 && character == u'1');
    Require(sceConvertKeycodeGetCharacter(4, 0, 9, &character) == 0 && character == u'q');
    Require(sceConvertKeycodeGetCharacter(28, 0, 2, &character) == 0 && character == u'z');
    Require(sceConvertKeycodeGetCharacter(20, 0x4000, 2, &character) == 0 && character == u'@');
    Require(sceConvertKeycodeGetCharacter(47, 0x20000, 3, &character) == 0 && character == 220);
    Require(sceConvertKeycodeGetCharacter(47, 0x20200, 3, &character) == 0 && character == 200);
    Require(sceConvertKeycodeGetCharacter(4, 0x4000000, 24, &character) == 0 && character == 0x3141);
    Require(sceConvertKeycodeGetCharacter(5, 0x4000000, 30, &character) == 0 && character == 1575);
    Require(sceConvertKeycodeGetCharacter(5, 0x4000200, 30, &character) == 0 && character == 1570);
    Require(sceConvertKeycodeGetCharacter(30, 0x4000300, 30, &character) == 0 && character == 0x200D);
    Require(sceConvertKeycodeGetCharacter(31, 0x4000300, 30, &character) == 0 && character == 0x200C);
    Require(sceConvertKeycodeGetCharacter(32, 0x4000300, 30, &character) == 0 && character == 0x200E);
    Require(sceConvertKeycodeGetCharacter(33, 0x4000300, 30, &character) == 0 && character == 0x200F);
    for (std::uint32_t layout = 1; layout <= 30; ++layout) {
        for (std::uint16_t key : {83, 224, 225, 226, 227, 228, 229, 230, 231}) {
            Require(sceConvertKeycodeGetCharacter(key, 0xFFFFFFFFu, layout, &character) == 0 && character == 0);
        }
    }
    KeyboardData keyData{};
    keyData.length = 1;
    keyData.key_code[0] = 4;
    char16_t kbdChar = u'?';
    Require(sceConvertKeycodeGetCharacterFromKeyboardData(nullptr, 0, 0, 4, &kbdChar) == static_cast<int>(0x80BC0031u));
    Require(sceConvertKeycodeGetCharacterFromKeyboardData(&keyData, 0, 0, 4, nullptr) == static_cast<int>(0x80BC0031u));
    for (std::int32_t badIndex : {-1, 1, 16}) {
        kbdChar = u'?';
        bool rejected = false;
        try {
            sceConvertKeycodeGetCharacterFromKeyboardData(&keyData, badIndex, 0, 4, &kbdChar);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        Require(rejected && kbdChar == u'?');
    }
    for (std::uint32_t layout : {31u, 39u}) {
        kbdChar = u'?';
        bool rejected = false;
        try {
            sceConvertKeycodeGetCharacterFromKeyboardData(&keyData, 0, 0, layout, &kbdChar);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        Require(rejected && kbdChar == u'?');
    }
    Require(sceConvertKeycodeGetCharacterFromKeyboardData(&keyData, 0, 0, 0, &kbdChar) == static_cast<int>(0x80BC001Du));
    Require(sceConvertKeycodeGetCharacterFromKeyboardData(&keyData, 0, 0, 40, &kbdChar) == static_cast<int>(0x80BC001Du));
    Require(sceConvertKeycodeGetCharacterFromKeyboardData(&keyData, 0, 0, 4, &kbdChar) == 0 && kbdChar == u'a');
    Require(sceConvertKeycodeGetCharacterFromKeyboardData(&keyData, 0, 0x200, 4, &kbdChar) == 0 && kbdChar == u'A');
    return 0;
}
