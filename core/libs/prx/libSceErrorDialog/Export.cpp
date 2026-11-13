#include "prx/libSceErrorDialog/include/Api.hpp"
#include "prx/libSceErrorDialog/include/ErrorDialogRequest.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>

#include "SceTypes.hpp"
#include "HitLog.hpp"
#include "prx/libc/include/General.hpp"

namespace {
constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;
constexpr int kStatusRunning = 2;
constexpr int kStatusFinished = 3;

constexpr int kErrOk = 0;
constexpr int kErrNotInitialized = static_cast<int>(0x80ED0001u);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80ED0002u);
constexpr int kErrParam = static_cast<int>(0x80ED0003u);
constexpr int kErrUnexpectedFatal = static_cast<int>(0x80ED0004u);
constexpr int kErrInvalidState = static_cast<int>(0x80ED0005u);

constexpr std::size_t kParamSize = 16;

std::mutex g_lock;
int g_status = kStatusNone;
bool g_presented = false;

void Refresh() {
    if (g_status != kStatusRunning || !g_presented) return;
    std::size_t chosen = 0;
    if (ModalDialogPoll_nid_postfix(chosen, ModalDialogOwnerErrorDialog) != ModalDialogState::Showing) g_status = kStatusFinished;
}
}  // namespace

extern "C" {

int APS5_VABI sceErrorDialogInitialize(void) {
 std::lock_guard g(g_lock);
 if (g_status != kStatusNone) return kErrAlreadyInitialized;
 g_status = kStatusInitialized;
 return kErrOk;
}

int APS5_VABI sceErrorDialogOpen(const void* param) {
 std::lock_guard g(g_lock);
 if (g_status != kStatusInitialized && g_status != kStatusFinished) return kErrInvalidState;
 std::uint8_t buffer[kParamSize];
 if (param == nullptr || !GuestCopyIn_nid_no_patch(buffer, param, kParamSize)) return kErrParam;
 std::int32_t size = 0;
 std::memcpy(&size, buffer, sizeof(size));
 if (static_cast<std::size_t>(size) != kParamSize) return kErrParam;
 std::int32_t errorCode = 0;
 std::int32_t userId = 0;
 std::memcpy(&errorCode, buffer + 4, sizeof(errorCode));
 std::memcpy(&userId, buffer + 8, sizeof(userId));
 ModalDialogRequest request = ErrorDialog::BuildRequest(errorCode);
 request.owner = ModalDialogOwnerErrorDialog;
 g_presented = ModalDialogOpen_nid_postfix(request);
 APS5_HIT("ERRORDIALOG", "open: error_code=0x%08X user_id=%d -> %s", errorCode, userId, g_presented ? "shown" : "auto answer");
 g_status = kStatusRunning;
 return kErrOk;
}

int APS5_VABI sceErrorDialogUpdateStatus(void) {
 std::lock_guard g(g_lock);
 Refresh();
 if (g_status == kStatusRunning && !g_presented) g_status = kStatusFinished;
 return g_status;
}

int APS5_VABI sceErrorDialogGetStatus(void) {
 std::lock_guard g(g_lock);
 Refresh();
 return g_status;
}

int APS5_VABI sceErrorDialogClose(void) {
 std::lock_guard g(g_lock);
 if (g_status != kStatusRunning) return kErrInvalidState;
 if (g_presented) ModalDialogClose_nid_postfix(ModalDialogOwnerErrorDialog);
 g_status = kStatusFinished;
 return kErrOk;
}

int APS5_VABI sceErrorDialogTerminate(void) {
 std::lock_guard g(g_lock);
 if (g_status == kStatusNone) return kErrNotInitialized;
 g_status = kStatusNone;
 g_presented = false;
 ModalDialogReset_nid_postfix(ModalDialogOwnerErrorDialog);
 return kErrOk;
}

int APS5_VABI sceErrorDialogOpenDetail() {
    return kErrUnexpectedFatal;
}

int APS5_VABI sceErrorDialogOpenWithReport() {
    return kErrUnexpectedFatal;
}

}
