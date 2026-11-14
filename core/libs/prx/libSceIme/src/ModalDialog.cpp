#include "prx/libSceIme/include/ModalDialog.hpp"

#include <atomic>
#include <chrono>
#include <mutex>

namespace {

constexpr std::uint32_t PadButtonUp = 0x0010;
constexpr std::uint32_t PadButtonRight = 0x0020;
constexpr std::uint32_t PadButtonDown = 0x0040;
constexpr std::uint32_t PadButtonLeft = 0x0080;
constexpr std::uint32_t PadButtonCircle = 0x2000;
constexpr std::uint32_t PadButtonCross = 0x4000;
constexpr std::uint8_t StickLow = 64;
constexpr std::uint8_t StickHigh = 192;
constexpr std::uint32_t StickLeft = 0x80000000;
constexpr std::uint32_t StickRight = 0x40000000;

struct Session {
    std::uint32_t owner = ModalDialogAnyOwner;
    ModalDialogState state = ModalDialogState::Idle;
    ModalDialogRequest request;
    std::size_t focus = 0;
    std::size_t chosen = ModalDialogNoButton;
    std::uint32_t held = 0;
    bool armed = false;
    bool drawn = false;
    std::chrono::steady_clock::time_point openedAt;
};

std::mutex g_mutex;
Session g_session;
std::atomic<bool> g_presenter{false};
std::atomic<std::uint32_t> g_showTimeout{ModalDialogShowTimeoutMilliseconds};

bool InputOwnedByIme() {
    return ImeCapturesKeyboard_nid_postfix() || ImeCapturesPad_nid_postfix();
}

bool OwnedBy(const Session& session, std::uint32_t owner) {
    return owner == ModalDialogAnyOwner || session.owner == owner;
}

void Expire(Session& session) {
    if (session.state != ModalDialogState::Showing || session.drawn) return;
    if (std::chrono::steady_clock::now() - session.openedAt >= std::chrono::milliseconds(g_showTimeout.load())) session.state = ModalDialogState::Unshown;
}

void Apply(Session& session, ModalDialogInputKind kind) {
    if (session.state != ModalDialogState::Showing) return;
    switch (kind) {
        case ModalDialogInputKind::Previous:
            if (session.focus > 0) --session.focus;
            break;
        case ModalDialogInputKind::Next:
            if (session.focus + 1 < session.request.buttons.size()) ++session.focus;
            break;
        case ModalDialogInputKind::Confirm:
            session.chosen = session.focus;
            session.state = ModalDialogState::Answered;
            break;
        case ModalDialogInputKind::Cancel:
            if (session.request.cancel == ModalDialogNoButton) break;
            session.chosen = session.request.cancel;
            session.state = ModalDialogState::Answered;
            break;
    }
}

}

extern "C" {

void ModalDialogSetPresenter_nid_postfix(bool present) {
    g_presenter = present;
}

bool ModalDialogHasPresenter_nid_postfix() {
    return g_presenter.load();
}

void ModalDialogSetShowTimeout_nid_postfix(std::uint32_t milliseconds) {
    g_showTimeout = milliseconds;
}

bool ModalDialogOpen_nid_postfix(const ModalDialogRequest& request) {
    if (!g_presenter.load()) return false;
    if (request.buttons.empty() || request.buttons.size() > ModalDialogMaxButtons) return false;
    if (request.focus >= request.buttons.size()) return false;
    if (request.cancel != ModalDialogNoButton && request.cancel >= request.buttons.size()) return false;
    std::lock_guard lock(g_mutex);
    if (g_session.state == ModalDialogState::Showing) return false;
    g_session = Session{};
    g_session.owner = request.owner;
    g_session.request = request;
    g_session.focus = request.focus;
    g_session.state = ModalDialogState::Showing;
    g_session.openedAt = std::chrono::steady_clock::now();
    return true;
}

ModalDialogState ModalDialogPoll_nid_postfix(std::size_t& chosen, std::uint32_t owner) {
    std::lock_guard lock(g_mutex);
    Expire(g_session);
    if (!OwnedBy(g_session, owner)) {
        chosen = ModalDialogNoButton;
        return ModalDialogState::Idle;
    }
    chosen = g_session.chosen;
    return g_session.state;
}

void ModalDialogClose_nid_postfix(std::uint32_t owner) {
    std::lock_guard lock(g_mutex);
    if (g_session.state == ModalDialogState::Showing && OwnedBy(g_session, owner)) g_session.state = ModalDialogState::Closed;
}

void ModalDialogReset_nid_postfix(std::uint32_t owner) {
    std::lock_guard lock(g_mutex);
    if (OwnedBy(g_session, owner)) g_session = Session{};
}

bool ModalDialogReadView_nid_postfix(ModalDialogView& view) {
    if (InputOwnedByIme()) return false;
    std::lock_guard lock(g_mutex);
    Expire(g_session);
    if (g_session.state != ModalDialogState::Showing) return false;
    g_session.drawn = true;
    view.message = g_session.request.message;
    view.buttons.clear();
    for (std::size_t index = 0; index < g_session.request.buttons.size(); ++index) {
        view.buttons.push_back({g_session.request.buttons[index], index == g_session.focus});
    }
    return true;
}

bool ModalDialogCapturesInput_nid_postfix() {
    if (InputOwnedByIme()) return false;
    std::lock_guard lock(g_mutex);
    Expire(g_session);
    return g_session.state == ModalDialogState::Showing;
}

void ModalDialogPublishInput_nid_postfix(ModalDialogInputKind kind) {
    if (InputOwnedByIme()) return;
    std::lock_guard lock(g_mutex);
    Expire(g_session);
    Apply(g_session, kind);
}

void ModalDialogPublishPad_nid_postfix(const ImeHostPad& pad) {
    if (InputOwnedByIme()) return;
    std::lock_guard lock(g_mutex);
    Session& session = g_session;
    Expire(session);
    if (session.state != ModalDialogState::Showing) return;
    std::uint32_t now = pad.buttons;
    if (pad.leftX < StickLow) now |= StickLeft;
    if (pad.leftX > StickHigh) now |= StickRight;
    const std::uint32_t pressed = now & ~session.held;
    session.held = now;
    if ((now & (PadButtonCross | PadButtonCircle)) == 0) session.armed = true;
    if ((pressed & (PadButtonLeft | PadButtonUp | StickLeft)) != 0) Apply(session, ModalDialogInputKind::Previous);
    if ((pressed & (PadButtonRight | PadButtonDown | StickRight)) != 0) Apply(session, ModalDialogInputKind::Next);
    if (!session.armed) return;
    if ((pressed & PadButtonCross) != 0) Apply(session, ModalDialogInputKind::Confirm);
    else if ((pressed & PadButtonCircle) != 0) Apply(session, ModalDialogInputKind::Cancel);
}

}
