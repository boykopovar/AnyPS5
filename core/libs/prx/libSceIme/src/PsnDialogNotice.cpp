#include "prx/libSceIme/include/PsnDialogNotice.hpp"

#include <chrono>
#include <cstdlib>
#include <mutex>
#include <optional>

namespace {

std::mutex g_mutex;
std::optional<PsnDialogNoticeMode> g_mode;
bool g_shown = false;
std::uint64_t g_lastShown = 0;

PsnDialogNoticeMode Mode() {
    if (!g_mode) g_mode = ParsePsnDialogNoticeMode(std::getenv("ANYPS5_PSN_DIALOGS"));
    return *g_mode;
}

std::uint64_t NowMilliseconds() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

}

extern "C" {

void PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode mode) {
    std::lock_guard lock(g_mutex);
    g_mode = mode;
    g_shown = false;
    g_lastShown = 0;
}

bool PsnDialogNoticeOfferAt_nid_postfix(PsnDialogNoticeKind kind, std::uint64_t nowMilliseconds) {
    std::lock_guard lock(g_mutex);
    if (Mode() != PsnDialogNoticeMode::Show) return false;
    if (g_shown && nowMilliseconds >= g_lastShown && nowMilliseconds - g_lastShown < PsnDialogNoticeIntervalMilliseconds) return false;
    if (!ModalDialogOpen_nid_postfix(PsnDialogNoticeRequest(kind))) return false;
    g_shown = true;
    g_lastShown = nowMilliseconds;
    return true;
}

bool PsnDialogNoticeOffer_nid_postfix(PsnDialogNoticeKind kind) {
    return PsnDialogNoticeOfferAt_nid_postfix(kind, NowMilliseconds());
}

}
