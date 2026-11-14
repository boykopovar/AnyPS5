#include "prx/libSceIme/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

static_assert(sizeof(Param) == 96);
static_assert(offsetof(Param, option) == 32);
static_assert(offsetof(Param, reserved) == 88);

static constexpr int NotOpened = static_cast<int>(0x80bc0002u);

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

template <typename TCall>
static void RequireNotImplemented(TCall call, const std::string& name) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == name + " not implemented", "a pending ime function threw another error");
        return;
    }
    Require(false, "a pending ime function returned");
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
    sceImeParamInit(reinterpret_cast<Param*>(0x10));
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
    Require(sceImeGetPanelSize(reinterpret_cast<const Param*>(0x10), &width, &height) == InvalidAddress,
            "unwritable parameter");
    Require(sceImeGetPanelSize(&param, reinterpret_cast<uint32_t*>(0x10), &height) == InvalidAddress,
            "unwritable width");
    uint32_t unwritableWidth = width;
    Require(sceImeGetPanelSize(&param, &unwritableWidth, reinterpret_cast<uint32_t*>(0x10)) == InvalidAddress,
            "unwritable height");
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
    Require(sceImeSetText(nullptr, 4) == notOpened, "null text checks open panel first");
    Require(sceImeSetTextGeometry(TextAreaMode::Edit, &geometry) == notOpened, "geometry needs an open panel");
    Require(sceImeSetTextGeometry(TextAreaMode::Edit, nullptr) == notOpened, "null geometry checks open panel first");
    Require(sceImeClose_nid_postfix() == notOpened, "closing needs an open panel");
    Require(sceImeDisableController() == notOpened, "disabling the controller needs an open panel");
    PositionAndForm form;
    std::memset(&form, 0xa5, sizeof(form));
    Require(sceImeGetPanelPositionAndForm(&form) == notOpened, "panel position needs an open panel");
    Require(sceImeGetPanelPositionAndForm(nullptr) == notOpened, "panel position checks the panel first");
    Require(sceImeGetPanelPositionAndForm(reinterpret_cast<PositionAndForm*>(0x10)) == notOpened,
            "unwritable form checks the panel first");
    const auto* formBytes = reinterpret_cast<const unsigned char*>(&form);
    for (size_t i = 0; i < sizeof(form); ++i) Require(formBytes[i] == 0xa5, "panel position written without an open panel");
}

static void APS5_VABI IgnoreEvent(void*, const ImeEvent*) {}

static void CheckOpenPanel() {
    constexpr int InvalidAddress = static_cast<int>(0x80bc0031u);
    char16_t text[64] = {};
    alignas(16) unsigned char work[0x5000] = {};
    Param param{};
    sceImeParamInit(&param);
    param.user_id = 1;
    param.max_text_length = 32;
    param.input_text_buffer = text;
    param.work = work;
    param.handler = IgnoreEvent;
    Require(sceImeOpen_nid_postfix(&param, nullptr) == 0, "open ime panel");
    Require(sceImeGetPanelPositionAndForm(reinterpret_cast<PositionAndForm*>(0x10)) == InvalidAddress,
            "unwritable panel position and form");
    PositionAndForm form{};
    Require(sceImeGetPanelPositionAndForm(&form) == 0, "valid panel position and form");
    RequireNotImplemented([] { sceImeDisableController(); }, "sceImeDisableController");
    RequireNotImplemented([] { sceImeDisableController(); }, "sceImeDisableController");
    Require(sceImeGetPanelPositionAndForm(&form) == 0, "panel still open after the controller release threw");
    Require(sceImeClose_nid_postfix() == 0, "close ime panel");
    Require(sceImeDisableController() == NotOpened, "disabling the controller needs an open panel after close");
}

static void CheckKeyboardResourceIds() {
    constexpr int ConnectionFailed = static_cast<int>(0x80bc0004u);
    constexpr int InvalidAddress = static_cast<int>(0x80bc0031u);
    KeyboardResourceIdArray ids;
    const auto* bytes = reinterpret_cast<const unsigned char*>(&ids);
    // A PS5 (firmware 11.60, payload process) answers 0x80bc0002 and writes nothing while no keyboard is open, for
    // every user id it was tried with (-1, 0 and 0x12345678) and for a null array.
    for (const int32_t user : {1, -1, 0, 0x12345678}) {
        std::memset(&ids, 0xa5, sizeof(ids));
        Require(sceImeKeyboardGetResourceId(user, &ids) == NotOpened, "keyboard not opened");
        for (size_t i = 0; i < sizeof(ids); ++i) Require(bytes[i] == 0xa5, "outputs changed without an open keyboard");
    }
    Require(sceImeKeyboardGetResourceId(1, nullptr) == NotOpened, "null resource id array without an open keyboard");
    KeyboardParam param{};
    Require(sceImeKeyboardOpen(1, &param) == 0, "keyboard open failed");
    Require(sceImeKeyboardGetResourceId(1, nullptr) == InvalidAddress, "null resource id array");
    Require(sceImeKeyboardGetResourceId(1, reinterpret_cast<KeyboardResourceIdArray*>(0x10)) == InvalidAddress,
            "unwritable resource id array");
    std::memset(&ids, 0xa5, sizeof(ids));
    Require(sceImeKeyboardGetResourceId(1, &ids) == ConnectionFailed, "a keyboard was reported as connected");
    Require(ids.user_id == 1, "user not reported");
    for (uint32_t id : ids.resource_id) Require(id == 0, "resource id reported without a keyboard");
    std::memset(&ids, 0xa5, sizeof(ids));
    Require(sceImeKeyboardGetResourceId(2, &ids) == NotOpened, "keyboard of another user reported as opened");
    for (size_t i = 0; i < sizeof(ids); ++i) Require(bytes[i] == 0xa5, "outputs changed for another user");
    Require(sceImeKeyboardClose(1) == 0, "keyboard close failed");
    std::memset(&ids, 0xa5, sizeof(ids));
    Require(sceImeKeyboardGetResourceId(1, &ids) == NotOpened, "closed keyboard reported as opened");
    for (size_t i = 0; i < sizeof(ids); ++i) Require(bytes[i] == 0xa5, "outputs changed after the close");
}

static void CheckConsecutiveClauseConversion() {
    const std::uint8_t values[] = {0, 1, 2, 0xff};
    for (const std::uint8_t value : values) {
        Require(sceImeSetConsecutiveClauseConversionEnabled(value) == 0, "any value must succeed");
    }
}

int main() {
    CheckInitialization();
    CheckPanelSizes();
    CheckErrors();
    CheckClosedPanel();
    CheckOpenPanel();
    CheckKeyboardResourceIds();
    CheckConsecutiveClauseConversion();
}
