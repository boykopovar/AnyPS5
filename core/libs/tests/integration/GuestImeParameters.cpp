#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

extern "C" {
void APS5_VABI sceImeParamInit(Param* param);
int APS5_VABI sceImeGetPanelSize(const Param* param, uint32_t* width, uint32_t* height);
int APS5_VABI sceImeClose_nid_postfix(void);
int APS5_VABI sceImeGetPanelPositionAndForm(PositionAndForm* form);
int APS5_VABI sceImeSetCaret(const Caret* caret);
int APS5_VABI sceImeSetText(const char16_t* text, uint32_t length);
int APS5_VABI sceImeSetTextGeometry(TextAreaMode mode, const TextGeometry* geometry);
int APS5_VABI sceImeKeyboardOpen(int32_t user_id, const KeyboardParam* param);
int APS5_VABI sceImeKeyboardClose(int32_t user_id);
int APS5_VABI sceImeKeyboardGetResourceId(int32_t user_id, KeyboardResourceIdArray* resource_ids);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

static_assert(sizeof(Param) == 96);
static_assert(offsetof(Param, option) == 32);
static_assert(offsetof(Param, reserved) == 88);

constexpr int NotOpened = static_cast<int>(0x80bc0002u);
constexpr int ConnectionFailed = static_cast<int>(0x80bc0004u);
constexpr int InvalidUserId = static_cast<int>(0x80bc0010u);
constexpr int InvalidType = static_cast<int>(0x80bc0011u);
constexpr int InvalidOption = static_cast<int>(0x80bc0015u);
constexpr int InvalidAddress = static_cast<int>(0x80bc0031u);
constexpr uint32_t WidthSentinel = 0x12345678;
constexpr uint32_t HeightSentinel = 0x87654321;
constexpr unsigned char Fill = 0xa5;

struct GuardedParam {
    std::array<unsigned char, 8> before;
    Param param;
    std::array<unsigned char, 8> after;
};

struct GuardedSize {
    uint32_t before;
    uint32_t value;
    uint32_t after;
};

template<typename TValue>
bool AllBytesEqual(const TValue& value, unsigned char expected) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        if (bytes[index] != expected) return false;
    }
    return true;
}

std::string PanelInput(uint32_t type, uint32_t options) {
    return "type " + std::to_string(type) + " option " + std::to_string(options);
}

class OpenKeyboard {
public:
    explicit OpenKeyboard(int32_t userId) : userId(userId) {
        const KeyboardParam param{};
        RequireEqual(sceImeKeyboardOpen(userId, &param), 0, "keyboard open");
        open = true;
    }

    ~OpenKeyboard() {
        if (!open) return;
        try {
            sceImeKeyboardClose(userId);
        } catch (...) {
        }
    }

    OpenKeyboard(const OpenKeyboard&) = delete;
    OpenKeyboard& operator=(const OpenKeyboard&) = delete;

    int Close() {
        open = false;
        return sceImeKeyboardClose(userId);
    }

private:
    int32_t userId;
    bool open = false;
};

const Case initClears{"ParamInit_FilledParam_SetsInvalidUserAndClearsTheRest", [] {
    GuardedParam guarded;
    std::memset(&guarded, Fill, sizeof(guarded));
    sceImeParamInit(nullptr);
    sceImeParamInit(&guarded.param);
    RequireEqual(guarded.param.user_id, -1, "initial user must be invalid");
    const auto* bytes = reinterpret_cast<const unsigned char*>(&guarded.param);
    for (std::size_t index = sizeof(guarded.param.user_id); index < sizeof(Param); ++index) {
        RequireEqual(bytes[index], static_cast<unsigned char>(0), "parameter byte " + std::to_string(index) + " must be cleared");
    }
    Require(AllBytesEqual(guarded.before, Fill), "parameter underrun");
    Require(AllBytesEqual(guarded.after, Fill), "parameter overrun");
}};

const Case initResets{"ParamInit_PreviouslySetParam_ResetsPreviousValues", [] {
    GuardedParam guarded;
    std::memset(&guarded, Fill, sizeof(guarded));
    sceImeParamInit(&guarded.param);
    guarded.param.type = 4;
    guarded.param.option = 0x4000;
    guarded.param.max_text_length = 32;
    sceImeParamInit(&guarded.param);
    RequireEqual(guarded.param.user_id, -1, "reinitialized user");
    RequireEqual(guarded.param.type, 0u, "reinitialized type");
    RequireEqual(guarded.param.option, 0u, "reinitialized option");
    RequireEqual(guarded.param.max_text_length, 0u, "reinitialized max text length");
}};

const Case panelSizes{"GetPanelSize_ValidTypesAndOptions_ReturnsPanelDimensions", [] {
    Param param;
    std::memset(&param, Fill, sizeof(param));
    for (uint32_t type = 0; type <= 4; ++type) {
        for (uint32_t options : {0u, 0x4000u, 0x7bffu}) {
            param.type = type;
            param.option = options;
            uint32_t width = 0;
            uint32_t height = 0;
            const auto input = PanelInput(type, options);
            RequireEqual(sceImeGetPanelSize(&param, &width, &height), 0, "valid panel query failed for " + input);
            const bool scaled = (options & 0x4000) != 0;
            RequireEqual(width, type == 4 ? (scaled ? 740u : 370u) : (scaled ? 1586u : 793u), "panel width for " + input);
            RequireEqual(height, type == 4 ? (scaled ? 804u : 402u) : (scaled ? 816u : 408u), "panel height for " + input);
        }
    }
}};

const Case panelSizeWrites{"GetPanelSize_ValidQuery_WritesOnlyOutputsAndKeepsParam", [] {
    Param param;
    std::memset(&param, Fill, sizeof(param));
    for (uint32_t type = 0; type <= 4; ++type) {
        for (uint32_t options : {0u, 0x4000u, 0x7bffu}) {
            param.type = type;
            param.option = options;
            const Param original = param;
            GuardedSize width{WidthSentinel, 0, HeightSentinel};
            GuardedSize height = width;
            const auto input = PanelInput(type, options);
            RequireEqual(sceImeGetPanelSize(&param, &width.value, &height.value), 0, "valid panel query failed for " + input);
            Require(width.before == WidthSentinel && width.after == HeightSentinel && height.before == WidthSentinel && height.after == HeightSentinel,
                "panel query wrote outside its outputs for " + input);
            Require(std::memcmp(&param, &original, sizeof(param)) == 0, "panel query modified parameters for " + input);
        }
    }
}};

const Case panelNullPointers{"GetPanelSize_NullPointer_FailsWithInvalidAddressAndKeepsOutputs", [] {
    const Param param{};
    uint32_t width = WidthSentinel;
    uint32_t height = HeightSentinel;
    RequireEqual(sceImeGetPanelSize(nullptr, &width, &height), InvalidAddress, "null parameter");
    RequireEqual(sceImeGetPanelSize(&param, nullptr, &height), InvalidAddress, "null width");
    RequireEqual(sceImeGetPanelSize(&param, &width, nullptr), InvalidAddress, "null height");
    RequireEqual(width, WidthSentinel, "width changed on error");
    RequireEqual(height, HeightSentinel, "height changed on error");
}};

const Case panelInvalidType{"GetPanelSize_TypeAboveFour_FailsWithInvalidTypeAndKeepsOutputs", [] {
    Param param{};
    uint32_t width = WidthSentinel;
    uint32_t height = HeightSentinel;
    for (uint32_t type : {5u, 0xffffffffu}) {
        param.type = type;
        RequireEqual(sceImeGetPanelSize(&param, &width, &height), InvalidType, "invalid type " + std::to_string(type));
    }
    RequireEqual(width, WidthSentinel, "width changed on error");
    RequireEqual(height, HeightSentinel, "height changed on error");
}};

const Case panelOptionBits{"GetPanelSize_EachOptionBit_AcceptsOnlyKnownOptions", [] {
    Param param{};
    for (uint32_t bit = 0; bit < 32; ++bit) {
        param.option = 1u << bit;
        const bool valid = (param.option & 0x7bffu) != 0;
        uint32_t width = WidthSentinel;
        uint32_t height = HeightSentinel;
        const auto input = "option bit " + std::to_string(bit);
        RequireEqual(sceImeGetPanelSize(&param, &width, &height), valid ? 0 : InvalidOption, "option bit validation for " + input);
        if (!valid) Require(width == WidthSentinel && height == HeightSentinel, "outputs changed on invalid " + input);
    }
}};

const Case panelTypePrecedence{"GetPanelSize_InvalidTypeAndOption_ReportsTheTypeError", [] {
    Param param{};
    param.type = 5;
    param.option = 0x80000000;
    uint32_t width = WidthSentinel;
    uint32_t height = HeightSentinel;
    RequireEqual(sceImeGetPanelSize(&param, &width, &height), InvalidType, "type error precedence");
    RequireEqual(width, WidthSentinel, "width changed on error");
    RequireEqual(height, HeightSentinel, "height changed on error");
}};

const Case panelAddressPrecedence{"GetPanelSize_NullWidthInvalidTypeAndOption_ReportsTheAddressError", [] {
    Param param{};
    param.type = 5;
    param.option = 0x80000000;
    uint32_t height = HeightSentinel;
    RequireEqual(sceImeGetPanelSize(&param, nullptr, &height), InvalidAddress, "address error precedence");
    RequireEqual(height, HeightSentinel, "height changed on error");
}};

const Case caretClosed{"SetCaret_ClosedPanel_FailsWithNotOpened", [] {
    const Caret caret{};
    RequireEqual(sceImeSetCaret(&caret), NotOpened, "caret needs an open panel");
    RequireEqual(sceImeSetCaret(nullptr), NotOpened, "caret checks the panel first");
}};

const Case textClosed{"SetText_ClosedPanel_FailsWithNotOpened", [] {
    const char16_t text[] = u"text";
    RequireEqual(sceImeSetText(text, 4), NotOpened, "text needs an open panel");
}};

const Case geometryClosed{"SetTextGeometry_ClosedPanel_FailsWithNotOpened", [] {
    const TextGeometry geometry{};
    RequireEqual(sceImeSetTextGeometry(TextAreaMode::Edit, &geometry), NotOpened, "geometry needs an open panel");
}};

const Case closeClosed{"Close_ClosedPanel_FailsWithNotOpened", [] {
    RequireEqual(sceImeClose_nid_postfix(), NotOpened, "closing needs an open panel");
}};

const Case positionClosed{"GetPanelPositionAndForm_ClosedPanel_FailsWithoutWriting", [] {
    PositionAndForm form;
    std::memset(&form, Fill, sizeof(form));
    RequireEqual(sceImeGetPanelPositionAndForm(&form), NotOpened, "panel position needs an open panel");
    RequireEqual(sceImeGetPanelPositionAndForm(nullptr), NotOpened, "panel position checks the panel first");
    Require(AllBytesEqual(form, Fill), "panel position written without an open panel");
}};

const Case resourceIdsNullArray{"KeyboardGetResourceId_NullArray_FailsWithInvalidAddress", [] {
    RequireEqual(sceImeKeyboardGetResourceId(1, nullptr), InvalidAddress, "null resource id array");
}};

const Case resourceIdsInvalidUser{"KeyboardGetResourceId_InvalidUser_FailsWithoutWriting", [] {
    KeyboardResourceIdArray ids;
    std::memset(&ids, Fill, sizeof(ids));
    RequireEqual(sceImeKeyboardGetResourceId(-1, &ids), InvalidUserId, "invalid user");
    Require(AllBytesEqual(ids, Fill), "outputs changed on argument error");
}};

const Case resourceIdsUnopened{"KeyboardGetResourceId_UnopenedKeyboard_ReportsNotOpenedWithUserOnly", [] {
    KeyboardResourceIdArray ids;
    std::memset(&ids, Fill, sizeof(ids));
    RequireEqual(sceImeKeyboardGetResourceId(1, &ids), NotOpened, "keyboard not opened");
    RequireEqual(ids.user_id, 1, "user not reported for an unopened keyboard");
    for (uint32_t id : ids.resource_id) RequireEqual(id, 0u, "resource id reported for an unopened keyboard");
}};

const Case resourceIdsOpened{"KeyboardGetResourceId_OpenedKeyboardWithoutDevice_ReportsConnectionFailed", [] {
    const OpenKeyboard keyboard(1);
    KeyboardResourceIdArray ids;
    std::memset(&ids, Fill, sizeof(ids));
    RequireEqual(sceImeKeyboardGetResourceId(1, &ids), ConnectionFailed, "a keyboard was reported as connected");
    RequireEqual(ids.user_id, 1, "user not reported");
    for (uint32_t id : ids.resource_id) RequireEqual(id, 0u, "resource id reported without a keyboard");
}};

const Case resourceIdsOtherUser{"KeyboardGetResourceId_OtherUserWhileOneIsOpen_ReportsNotOpened", [] {
    const OpenKeyboard keyboard(1);
    KeyboardResourceIdArray ids;
    std::memset(&ids, Fill, sizeof(ids));
    RequireEqual(sceImeKeyboardGetResourceId(2, &ids), NotOpened, "keyboard of another user reported as opened");
    RequireEqual(ids.user_id, 2, "other user not reported");
}};

const Case resourceIdsClosed{"KeyboardGetResourceId_ClosedKeyboard_ReportsNotOpened", [] {
    OpenKeyboard keyboard(1);
    RequireEqual(keyboard.Close(), 0, "keyboard close");
    KeyboardResourceIdArray ids;
    std::memset(&ids, Fill, sizeof(ids));
    RequireEqual(sceImeKeyboardGetResourceId(1, &ids), NotOpened, "closed keyboard reported as opened");
}};

} // namespace
