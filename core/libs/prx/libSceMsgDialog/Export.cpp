// libSceMsgDialog: message dialog state machine (NONE -> INITIALIZED -> RUNNING -> FINISHED) without a visible dialog.
// The dialog "closes" with the affirmative button after a couple of status polls, so a title waiting for the user
// never blocks. The message text is logged for the first calls.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <chrono>

#include "SceTypes.hpp"
#include "HitLog.hpp"
#include "prx/libc/include/General.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;
constexpr int kStatusRunning = 2;
constexpr int kStatusFinished = 3;

constexpr int kErrOk = 0;
constexpr int kErrNotInitialized = static_cast<int>(0x80B80003u);
constexpr int kErrNotFinished = static_cast<int>(0x80B80005u);
constexpr int kErrBusy = static_cast<int>(0x80B80007u);
constexpr int kErrNotRunning = static_cast<int>(0x80B8000Bu);
constexpr int kErrArgNull = static_cast<int>(0x80B8000Du);

constexpr int kModeUserMsg = 1;
constexpr int kModeProgressBar = 2;
constexpr int kModeSystemMsg = 3;

constexpr int kButtonTypeOk = 0;
constexpr int kButtonTypeYesNo = 1;
constexpr int kButtonTypeNone = 2;
constexpr int kButtonTypeOkCancel = 3;
constexpr int kButtonTypeYesNoFocusNo = 4;

constexpr int kButtonIdOk = 1; // OK / YES / BUTTON1
constexpr int kButtonIdNo = 2; // NO / BUTTON2
constexpr int kResultOk = 0;

std::mutex g_lock;
int g_status = kStatusNone;
int g_mode = kModeUserMsg;
int g_buttonId = kButtonIdOk;
int g_pollsLeft = 0;
std::chrono::steady_clock::time_point g_openedAt;

bool Readable(const void* p, std::size_t n) {
#ifdef _WIN32
    if (p == nullptr)
        return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT)
        return false;
    if ((mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0)
        return false;
    const auto end = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    return reinterpret_cast<std::uintptr_t>(p) + n <= end;
#else
    return p != nullptr;
#endif
}

// Best-effort inspection of SceMsgDialogParam: { SceCommonDialogBaseParam base (0x30); size_t size; int mode (0x38);
// UserMessageParam* userMsg (0x40); ProgressBarParam* progress (0x48); SystemMessageParam* system (0x50); ... }.
void Inspect(const void* param) {
    const std::uint8_t* bytes = static_cast<const std::uint8_t*>(param);
    g_mode = kModeUserMsg;
    g_buttonId = kButtonIdOk;
    if (!Readable(bytes, 0x60))
        return;
    std::int32_t mode = 0;
    std::memcpy(&mode, bytes + 0x38, 4);
    if (mode >= kModeUserMsg && mode <= kModeSystemMsg)
        g_mode = mode;
    if (g_mode != kModeUserMsg)
        return;
    const std::uint8_t* userMsg = nullptr;
    std::memcpy(&userMsg, bytes + 0x40, 8);
    if (!Readable(userMsg, 16))
        return;
    std::int32_t buttonType = 0;
    std::memcpy(&buttonType, userMsg, 4);
    const char* msg = nullptr;
    std::memcpy(&msg, userMsg + 8, 8);
    if (buttonType == kButtonTypeYesNoFocusNo)
        g_buttonId = kButtonIdNo;
    if (Readable(msg, 4)) {
        char text[161] = {};
        for (int i = 0; i < 160 && Readable(msg + i, 1) && msg[i] != '\0'; ++i)
            text[i] = (msg[i] >= 0x20 || msg[i] == '\n') ? msg[i] : '?';
        APS5_HIT("MSGDIALOG", "open: buttons=%d text=\"%s\" -> auto answer", buttonType, text);
    }
}

int Poll() {
    std::lock_guard g(g_lock);
    if (g_status == kStatusRunning) {
        bool done = false;
        if (g_mode == kModeProgressBar) {
            // Progress dialogs are ended by the title (sceMsgDialogClose); the cap only prevents a wait without end.
            done = std::chrono::steady_clock::now() - g_openedAt > std::chrono::seconds(30);
        } else {
            done = --g_pollsLeft <= 0;
        }
        if (done)
            g_status = kStatusFinished;
    }
    return g_status;
}
} // namespace

extern "C" {

int APS5_VABI sceMsgDialogInitialize(void) {
    std::lock_guard g(g_lock);
    if (g_status == kStatusNone)
        g_status = kStatusInitialized;
    APS5_HIT("MSGDIALOG", "sceMsgDialogInitialize");
    return kErrOk;
}
int APS5_VABI sceMsgDialogTerminate(void) {
    std::lock_guard g(g_lock);
    const bool was = g_status != kStatusNone;
    g_status = kStatusNone;
    return was ? kErrOk : kErrNotInitialized;
}
int APS5_VABI sceMsgDialogOpen(const void* param) {
    if (param == nullptr)
        return kErrArgNull;
    std::lock_guard g(g_lock);
    if (g_status == kStatusRunning)
        return kErrBusy;
    // A title that skipped sceMsgDialogInitialize still gets its dialog (and its FINISHED status).
    Inspect(param);
    g_pollsLeft = 2;
    g_openedAt = std::chrono::steady_clock::now();
    g_status = kStatusRunning;
    return kErrOk;
}
int APS5_VABI sceMsgDialogGetStatus(void) { return Poll(); }
int APS5_VABI sceMsgDialogUpdateStatus(void) { return Poll(); }
// SceMsgDialogResult: { int32 mode; int32 result; int32 buttonId; uint8 reserved[32] }.
int APS5_VABI sceMsgDialogGetResult(void* result) {
    if (result == nullptr)
        return kErrArgNull;
    std::lock_guard g(g_lock);
    if (g_status != kStatusFinished)
        return kErrNotFinished;
    std::int32_t out[3] = {g_mode, kResultOk, g_buttonId};
    std::memcpy(result, out, sizeof(out));
    return kErrOk;
}
int APS5_VABI sceMsgDialogClose(void) {
    std::lock_guard g(g_lock);
    if (g_status != kStatusRunning)
        return kErrNotRunning;
    g_status = kStatusFinished;
    return kErrOk;
}
int APS5_VABI sceMsgDialogProgressBarInc(int target, std::uint32_t delta) {
    (void)target;
    (void)delta;
    std::lock_guard g(g_lock);
    return g_status == kStatusRunning ? kErrOk : kErrNotRunning;
}
int APS5_VABI sceMsgDialogProgressBarSetMsg(int target, const char* msg) {
    (void)target;
    (void)msg;
    std::lock_guard g(g_lock);
    return g_status == kStatusRunning ? kErrOk : kErrNotRunning;
}
int APS5_VABI sceMsgDialogProgressBarSetValue(int target, std::uint32_t rate) {
    (void)target;
    (void)rate;
    std::lock_guard g(g_lock);
    return g_status == kStatusRunning ? kErrOk : kErrNotRunning;
}
}
