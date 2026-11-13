#ifndef CORE_LIBS_PRX_LIBSCECONVERTKEYCODE_IMEKEYBOARDTYPE_HPP
#define CORE_LIBS_PRX_LIBSCECONVERTKEYCODE_IMEKEYBOARDTYPE_HPP

#include <cstdint>
#include <string_view>

// sceConvertKeycodeGetImeKeyboardType: the UserService keyboard type (plus the Japanese and Traditional Chinese
// input variants) to IME keyboard type mapping and the SDK compatibility rules, as specified by the repository
// owner (see docs/dev/debt/libSceConvertKeycode.md). Header-only so the guest test drives every row and error path
// with fake UserService getters.
namespace ImeKeyboardType {

constexpr int kErrInvalidAddress = static_cast<int>(0x80BC0031u);
constexpr int kErrInvalidUserId = static_cast<int>(0x80BC0010u);
constexpr int kErrGeneric = static_cast<int>(0x80BC00FFu);
constexpr int kUserServiceUnknownUser = static_cast<int>(0x80960105u);

constexpr std::int32_t kJapaneseKeyboard = 37;
constexpr std::int32_t kTraditionalChineseKeyboard = 39;

// Index = UserService keyboard type - 16. Types 37 and 39 are resolved through their variants; 0 = no mapping.
constexpr std::int32_t kFirstMappedType = 16;
constexpr std::uint32_t kImeTypeByKeyboardType[] = {
    1, 2, 3, 5, 4, 6, 7, 8, 10, 11, 9, 12, 13, 14, 15, 16, 17, 18, 19, 20,  // 16..35
    21, 0, 24, 0, 25, 30, 31, 33, 32, 34, 35, 36, 37, 38,                    // 36..49 (37 and 39 are variants)
};
constexpr std::int32_t kMappedTypeCount = static_cast<std::int32_t>(sizeof(kImeTypeByKeyboardType) / sizeof(kImeTypeByKeyboardType[0]));

constexpr std::uint32_t LookUp(std::int32_t keyboardType, std::int32_t japaneseVariant, std::int32_t traditionalChineseVariant) {
    if (keyboardType == kJapaneseKeyboard) {
        constexpr std::uint32_t japanese[] = {22, 23};
        return japaneseVariant >= 0 && japaneseVariant < 2 ? japanese[japaneseVariant] : 0;
    }
    if (keyboardType == kTraditionalChineseKeyboard) {
        constexpr std::uint32_t traditionalChinese[] = {26, 27, 28, 29, 39};
        return traditionalChineseVariant >= 0 && traditionalChineseVariant < 5 ? traditionalChinese[traditionalChineseVariant] : 0;
    }
    const std::int64_t index = std::int64_t{keyboardType} - kFirstMappedType;
    return index >= 0 && index < kMappedTypeCount ? kImeTypeByKeyboardType[index] : 0;
}

// Applied after the lookup. 38 -> 4 below PS5 SDK 0x06000000 / PS4 SDK 0x10000000; 39 -> 29 below PS5 SDK 0x08000000
// and for every PS4 client.
constexpr std::uint32_t ApplySdkCompatibility(std::uint32_t imeType, std::uint32_t sdkVersion, bool ps4Client) {
    if (imeType == 38 && sdkVersion < (ps4Client ? 0x10000000u : 0x06000000u)) return 4;
    if (imeType == 39 && (ps4Client || sdkVersion < 0x08000000u)) return 29;
    return imeType;
}

constexpr bool IsPs4TitleId(std::string_view titleId) {
    return titleId.substr(0, 4) == "CUSA";
}

template <typename GetKeyboardType, typename GetJapaneseInputType, typename GetTraditionalChineseInputType>
int Resolve(std::int32_t userId, std::uint32_t* type, std::uint32_t sdkVersion, bool ps4Client, GetKeyboardType getKeyboardType, GetJapaneseInputType getJapaneseInputType, GetTraditionalChineseInputType getTraditionalChineseInputType) {
    if (type == nullptr) return kErrInvalidAddress;
    std::int32_t keyboardType = 0;
    const int result = getKeyboardType(userId, &keyboardType);
    if (result == kUserServiceUnknownUser) return kErrInvalidUserId;
    if (result != 0) return kErrGeneric;
    std::int32_t japaneseVariant = 0;
    std::int32_t traditionalChineseVariant = 0;
    if (keyboardType == kJapaneseKeyboard && getJapaneseInputType(userId, &japaneseVariant) != 0) japaneseVariant = 0;
    if (keyboardType == kTraditionalChineseKeyboard && getTraditionalChineseInputType(userId, &traditionalChineseVariant) != 0) traditionalChineseVariant = 0;
    *type = ApplySdkCompatibility(LookUp(keyboardType, japaneseVariant, traditionalChineseVariant), sdkVersion, ps4Client);
    return 0;
}

}  // namespace ImeKeyboardType

#endif
