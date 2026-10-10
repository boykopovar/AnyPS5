#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceImeKeyboardOpen(int32_t user_id, const KeyboardParam* param);
int APS5_VABI sceImeKeyboardClose(int32_t user_id);
int APS5_VABI sceImeKeyboardGetInfo(uint32_t resource_id, KeyboardInfo* info);
int APS5_VABI sceImeKeyboardSetMode(int32_t user_id, uint32_t mode);
}

static_assert(sizeof(KeyboardInfo) == 36);

constexpr int NotOpened = static_cast<int>(0x80bc0002u);
constexpr int InvalidUserId = static_cast<int>(0x80bc0010u);
constexpr int NoResourceId = static_cast<int>(0x80bc0023u);
constexpr int InvalidMode = static_cast<int>(0x80bc0024u);
constexpr int InvalidAddress = static_cast<int>(0x80bc0031u);

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

static void RequireUntouched(const KeyboardInfo& info) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&info);
    for (size_t i = 0; i < sizeof(info); ++i) Require(bytes[i] == 0xa5, "keyboard info written");
}

static void CheckClosed() {
    KeyboardInfo info;
    std::memset(&info, 0xa5, sizeof(info));
    Require(sceImeKeyboardGetInfo(0, nullptr) == InvalidAddress, "null info");
    Require(sceImeKeyboardGetInfo(0, &info) == NotOpened, "info without an open keyboard");
    RequireUntouched(info);
    Require(sceImeKeyboardSetMode(-1, 0) == InvalidUserId, "set mode invalid user");
    Require(sceImeKeyboardSetMode(1, 0) == NotOpened, "set mode without an open keyboard");
    Require(sceImeKeyboardSetMode(1, 0x80) == NotOpened, "set mode checks the mode before the keyboard");
}

static void CheckOpened() {
    KeyboardParam param{};
    Require(sceImeKeyboardOpen(1, &param) == 0, "keyboard open failed");
    KeyboardInfo info;
    std::memset(&info, 0xa5, sizeof(info));
    Require(sceImeKeyboardGetInfo(0, nullptr) == InvalidAddress, "null info with an open keyboard");
    Require(sceImeKeyboardGetInfo(0, &info) == NoResourceId, "resource id 0 reported as a keyboard");
    Require(sceImeKeyboardGetInfo(0x12345678, &info) == NoResourceId, "unknown resource id reported as a keyboard");
    RequireUntouched(info);
    Require(sceImeKeyboardSetMode(-1, 0) == InvalidUserId, "set mode invalid user with an open keyboard");
    Require(sceImeKeyboardSetMode(2, 0) == NotOpened, "set mode for a user without an open keyboard");
    Require(sceImeKeyboardSetMode(1, 0) == 0, "mode 0 rejected");
    Require(sceImeKeyboardSetMode(1, 0x7f) == 0, "all mode bits rejected");
    Require(sceImeKeyboardSetMode(1, 0x41) == 0, "manual mode without format characters rejected");
    Require(sceImeKeyboardSetMode(1, 0x80) == InvalidMode, "mode bit 7 accepted");
    Require(sceImeKeyboardSetMode(1, 0x80000001u) == InvalidMode, "mode bit 31 accepted");
    Require(sceImeKeyboardClose(1) == 0, "keyboard close failed");
    Require(sceImeKeyboardGetInfo(0, &info) == NotOpened, "info after close");
    Require(sceImeKeyboardSetMode(1, 0) == NotOpened, "set mode after close");
}

int main() {
    CheckClosed();
    CheckOpened();
}
