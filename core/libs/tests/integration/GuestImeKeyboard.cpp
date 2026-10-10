#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstring>
#include <exception>
#include <string>

extern "C" {
int APS5_VABI sceImeKeyboardOpen(int32_t user_id, const KeyboardParam* param);
int APS5_VABI sceImeKeyboardClose(int32_t user_id);
int APS5_VABI sceImeKeyboardGetInfo(uint32_t resource_id, KeyboardInfo* info);
int APS5_VABI sceImeKeyboardSetMode(int32_t user_id, uint32_t mode);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

static_assert(sizeof(KeyboardInfo) == 36);

constexpr int notOpened = static_cast<int>(0x80bc0002u);
constexpr int invalidUserId = static_cast<int>(0x80bc0010u);
constexpr int noResourceId = static_cast<int>(0x80bc0023u);
constexpr int invalidMode = static_cast<int>(0x80bc0024u);
constexpr int invalidAddress = static_cast<int>(0x80bc0031u);
constexpr unsigned char filler = 0xa5;

KeyboardInfo FilledInfo() {
    KeyboardInfo info;
    std::memset(&info, filler, sizeof(info));
    return info;
}

void RequireUntouched(const KeyboardInfo& info) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&info);
    for (std::size_t index = 0; index < sizeof(info); ++index) {
        RequireEqual(bytes[index], filler, "keyboard info byte " + std::to_string(index));
    }
}

class OpenKeyboard {
public:
    OpenKeyboard() {
        const KeyboardParam param{};
        RequireEqual(sceImeKeyboardOpen(1, &param), 0, "keyboard open");
    }

    ~OpenKeyboard() {
        if (!open) return;
        try {
            sceImeKeyboardClose(1);
        } catch (const std::exception&) {
        }
    }

    OpenKeyboard(const OpenKeyboard&) = delete;
    OpenKeyboard& operator=(const OpenKeyboard&) = delete;

    int Close() {
        open = false;
        return sceImeKeyboardClose(1);
    }

private:
    bool open = true;
};

const Case closedInfoNull{"GetInfo_NullInfoWithoutKeyboard_ReturnsInvalidAddress", [] {
    RequireEqual(sceImeKeyboardGetInfo(0, nullptr), invalidAddress, "null info");
}};

const Case closedInfo{"GetInfo_WithoutKeyboard_ReturnsNotOpenedWithoutWriting", [] {
    auto info = FilledInfo();
    RequireEqual(sceImeKeyboardGetInfo(0, &info), notOpened, "info without an open keyboard");
    RequireUntouched(info);
}};

const Case closedSetModeInvalidUser{"SetMode_InvalidUserWithoutKeyboard_ReturnsInvalidUserId", [] {
    RequireEqual(sceImeKeyboardSetMode(-1, 0), invalidUserId, "set mode for user -1");
}};

const Case closedSetMode{"SetMode_WithoutKeyboard_ReturnsNotOpenedBeforeCheckingMode", [] {
    RequireEqual(sceImeKeyboardSetMode(1, 0), notOpened, "set mode 0");
    RequireEqual(sceImeKeyboardSetMode(1, 0x80), notOpened, "set invalid mode 0x80");
}};

const Case openInfoNull{"GetInfo_NullInfoWithKeyboard_ReturnsInvalidAddress", [] {
    const OpenKeyboard keyboard;
    RequireEqual(sceImeKeyboardGetInfo(0, nullptr), invalidAddress, "null info with an open keyboard");
}};

const Case openInfoUnknownResource{"GetInfo_UnknownResourceWithKeyboard_ReturnsNoResourceIdWithoutWriting", [] {
    const OpenKeyboard keyboard;
    auto info = FilledInfo();
    RequireEqual(sceImeKeyboardGetInfo(0, &info), noResourceId, "resource id 0");
    RequireEqual(sceImeKeyboardGetInfo(0x12345678, &info), noResourceId, "resource id 0x12345678");
    RequireUntouched(info);
}};

const Case openSetModeInvalidUser{"SetMode_InvalidUserWithKeyboard_ReturnsInvalidUserId", [] {
    const OpenKeyboard keyboard;
    RequireEqual(sceImeKeyboardSetMode(-1, 0), invalidUserId, "set mode for user -1");
}};

const Case openSetModeOtherUser{"SetMode_UserWithoutKeyboard_ReturnsNotOpened", [] {
    const OpenKeyboard keyboard;
    RequireEqual(sceImeKeyboardSetMode(2, 0), notOpened, "set mode for user 2");
}};

const Case openSetModeValid{"SetMode_SupportedModeBits_Succeeds", [] {
    const OpenKeyboard keyboard;
    for (const uint32_t mode : {0x0u, 0x7fu, 0x41u}) {
        RequireEqual(sceImeKeyboardSetMode(1, mode), 0, "mode " + std::to_string(mode));
    }
}};

const Case openSetModeInvalid{"SetMode_UnsupportedModeBits_ReturnsInvalidMode", [] {
    const OpenKeyboard keyboard;
    for (const uint32_t mode : {0x80u, 0x80000001u}) {
        RequireEqual(sceImeKeyboardSetMode(1, mode), invalidMode, "mode " + std::to_string(mode));
    }
}};

const Case afterClose{"Close_OpenKeyboard_RestoresNotOpenedState", [] {
    OpenKeyboard keyboard;
    auto info = FilledInfo();
    RequireEqual(keyboard.Close(), 0, "keyboard close");
    RequireEqual(sceImeKeyboardGetInfo(0, &info), notOpened, "info after close");
    RequireEqual(sceImeKeyboardSetMode(1, 0), notOpened, "set mode after close");
}};

} // namespace
