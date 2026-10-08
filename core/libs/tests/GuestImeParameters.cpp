#include "SceTypes.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
void APS5_VABI sceImeParamInit(Param* param);
int APS5_VABI sceImeGetPanelSize(const Param* param, uint32_t* width, uint32_t* height);
int APS5_VABI sceImeClose_nid_postfix(void);
int APS5_VABI sceImeSetCaret(const Caret* caret);
int APS5_VABI sceImeSetText(const char16_t* text, uint32_t length);
int APS5_VABI sceImeSetTextGeometry(TextAreaMode mode, const TextGeometry* geometry);
int APS5_VABI sceImeKeyboardOpen(int32_t user_id, const KeyboardParam* param);
int APS5_VABI sceImeKeyboardClose(int32_t user_id);
int APS5_VABI sceImeKeyboardGetResourceId(int32_t user_id, KeyboardResourceIdArray* resource_ids);
}

static_assert(sizeof(Param) == 96);
static_assert(offsetof(Param, option) == 32);
static_assert(offsetof(Param, reserved) == 88);

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

static void CheckInitialization() {
    struct GuardedParam {
        std::array<unsigned char, 8> before;
        Param param;
        std::array<unsigned char, 8> after;
    } guarded;
    std::memset(&guarded, 0xa5, sizeof(guarded));
    sceImeParamInit(nullptr);
    sceImeParamInit(&guarded.param);
    Require(guarded.param.user_id == -1, "initial user must be invalid");
    const auto* bytes = reinterpret_cast<const unsigned char*>(&guarded.param);
    for (size_t i = sizeof(guarded.param.user_id); i < sizeof(Param); ++i) {
        Require(bytes[i] == 0, "all remaining parameter bytes must be cleared");
    }
    for (unsigned char value : guarded.before) Require(value == 0xa5, "parameter underrun");
    for (unsigned char value : guarded.after) Require(value == 0xa5, "parameter overrun");
    guarded.param.type = 4;
    guarded.param.option = 0x4000;
    guarded.param.max_text_length = 32;
    sceImeParamInit(&guarded.param);
    Require(guarded.param.user_id == -1 && guarded.param.type == 0 &&
            guarded.param.option == 0 && guarded.param.max_text_length == 0,
            "reinitialization must reset previous values");
}

static void CheckPanelSizes() {
    Param param;
    std::memset(&param, 0xa5, sizeof(param));
    for (uint32_t type = 0; type <= 4; ++type) {
        for (uint32_t options : {0u, 0x4000u, 0x7bffu}) {
            param.type = type;
            param.option = options;
            const Param original = param;
            struct GuardedSize { uint32_t before; uint32_t value; uint32_t after; };
            GuardedSize width{0x12345678, 0, 0x87654321};
            GuardedSize height = width;
            Require(sceImeGetPanelSize(&param, &width.value, &height.value) == 0,
                    "valid panel query failed");
            const bool scaled = (options & 0x4000) != 0;
            Require(width.value == (type == 4 ? (scaled ? 740u : 370u) : (scaled ? 1586u : 793u)),
                    "incorrect panel width");
            Require(height.value == (type == 4 ? (scaled ? 804u : 402u) : (scaled ? 816u : 408u)),
                    "incorrect panel height");
            Require(width.before == 0x12345678 && width.after == 0x87654321 &&
                    height.before == 0x12345678 && height.after == 0x87654321,
                    "panel query wrote outside its outputs");
            Require(std::memcmp(&param, &original, sizeof(param)) == 0,
                    "panel query modified parameters");
        }
    }
}

static void CheckErrors() {
    constexpr int InvalidAddress = static_cast<int>(0x80bc0031u);
    constexpr int InvalidType = static_cast<int>(0x80bc0011u);
    constexpr int InvalidOption = static_cast<int>(0x80bc0015u);
    Param param{};
    uint32_t width = 0x12345678;
    uint32_t height = 0x87654321;
    Require(sceImeGetPanelSize(nullptr, &width, &height) == InvalidAddress, "null parameter");
    Require(sceImeGetPanelSize(&param, nullptr, &height) == InvalidAddress, "null width");
    Require(sceImeGetPanelSize(&param, &width, nullptr) == InvalidAddress, "null height");
    for (uint32_t type : {5u, 0xffffffffu}) {
        param.type = type;
        Require(sceImeGetPanelSize(&param, &width, &height) == InvalidType, "invalid type");
    }
    param.type = 0;
    for (uint32_t bit = 0; bit < 32; ++bit) {
        param.option = 1u << bit;
        const bool valid = (param.option & 0x7bffu) != 0;
        uint32_t queriedWidth = width;
        uint32_t queriedHeight = height;
        Require(sceImeGetPanelSize(&param, &queriedWidth, &queriedHeight) == (valid ? 0 : InvalidOption),
                "option bit validation");
        if (!valid) Require(queriedWidth == width && queriedHeight == height, "outputs changed on invalid option");
    }
    param.type = 5;
    param.option = 0x80000000;
    Require(sceImeGetPanelSize(&param, &width, &height) == InvalidType, "type error precedence");
    Require(sceImeGetPanelSize(&param, nullptr, &height) == InvalidAddress, "address error precedence");
    Require(width == 0x12345678 && height == 0x87654321, "outputs changed on error");
}

static void CheckClosedPanel() {
    constexpr int notOpened = static_cast<int>(0x80bc0002u);
    Caret caret{};
    TextGeometry geometry{};
    const char16_t text[] = u"text";
    Require(sceImeSetCaret(&caret) == notOpened, "caret needs an open panel");
    Require(sceImeSetCaret(nullptr) == notOpened, "caret checks the panel first");
    Require(sceImeSetText(text, 4) == notOpened, "text needs an open panel");
    Require(sceImeSetTextGeometry(TextAreaMode::Edit, &geometry) == notOpened, "geometry needs an open panel");
    Require(sceImeClose_nid_postfix() == notOpened, "closing needs an open panel");
}

static void CheckKeyboardResourceIds() {
    constexpr int NotOpened = static_cast<int>(0x80bc0002u);
    constexpr int ConnectionFailed = static_cast<int>(0x80bc0004u);
    constexpr int InvalidUserId = static_cast<int>(0x80bc0010u);
    constexpr int InvalidAddress = static_cast<int>(0x80bc0031u);
    KeyboardResourceIdArray ids;
    std::memset(&ids, 0xa5, sizeof(ids));
    Require(sceImeKeyboardGetResourceId(1, nullptr) == InvalidAddress, "null resource id array");
    Require(sceImeKeyboardGetResourceId(-1, &ids) == InvalidUserId, "invalid user");
    const auto* bytes = reinterpret_cast<const unsigned char*>(&ids);
    for (size_t i = 0; i < sizeof(ids); ++i) Require(bytes[i] == 0xa5, "outputs changed on argument error");
    Require(sceImeKeyboardGetResourceId(1, &ids) == NotOpened, "keyboard not opened");
    Require(ids.user_id == 1, "user not reported for an unopened keyboard");
    for (uint32_t id : ids.resource_id) Require(id == 0, "resource id reported for an unopened keyboard");
    KeyboardParam param{};
    Require(sceImeKeyboardOpen(1, &param) == 0, "keyboard open failed");
    std::memset(&ids, 0xa5, sizeof(ids));
    Require(sceImeKeyboardGetResourceId(1, &ids) == ConnectionFailed, "a keyboard was reported as connected");
    Require(ids.user_id == 1, "user not reported");
    for (uint32_t id : ids.resource_id) Require(id == 0, "resource id reported without a keyboard");
    Require(sceImeKeyboardGetResourceId(2, &ids) == NotOpened, "keyboard of another user reported as opened");
    Require(ids.user_id == 2, "other user not reported");
    Require(sceImeKeyboardClose(1) == 0, "keyboard close failed");
    Require(sceImeKeyboardGetResourceId(1, &ids) == NotOpened, "closed keyboard reported as opened");
}

int main() {
    CheckInitialization();
    CheckPanelSizes();
    CheckErrors();
    CheckClosedPanel();
    CheckKeyboardResourceIds();
}
