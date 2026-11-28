#include "prx/libkernel/include/Api.hpp"
#include <cinttypes>
#include <cstdio>
#include <cstdlib>

static constexpr std::int16_t EvfiltUser = -11;
static constexpr std::uint16_t EvAdd = 0x0001;
static constexpr std::uint16_t EvDelete = 0x0002;
static constexpr std::uint16_t EvEnable = 0x0004;
static constexpr std::uint16_t EvDisable = 0x0008;
static constexpr std::uint16_t EvOneshot = 0x0010;
static constexpr std::uint16_t EvClear = 0x0020;
static constexpr std::uint16_t EvDispatch = 0x0080;
static constexpr std::uint16_t EvForceOneshot = 0x0100;
static constexpr std::uint32_t NoteTrigger = 0x01000000;
static constexpr std::uint32_t NoteFfnop = 0x00000000;
static constexpr std::uint32_t NoteFfand = 0x40000000;
static constexpr std::uint32_t NoteFfcopy = 0xc0000000;

static void Require(int kq, bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "kqueue user state: %s\n", message);
        if (kq >= 0 && close_nid_postfix(kq) != 0)
            std::fprintf(stderr, "kqueue user state: cleanup close failed\n");
        std::exit(EXIT_FAILURE);
    }
}

static void Apply(int kq, std::uint16_t flags, std::uint32_t fflags, std::intptr_t data) {
    KernelEvent change{};
    change.ident = 90;
    change.filter = EvfiltUser;
    change.flags = flags;
    change.fflags = fflags;
    change.data = data;
    Require(kq, kevent_nid_postfix(kq, &change, 1, nullptr, 0, nullptr) == 0, "change failed");
}

static int Poll(int kq, KernelEvent& event) {
    const KernelTimespec zero{0, 0};
    return kevent_nid_postfix(kq, nullptr, 0, &event, 1, &zero);
}

static void RequireEvent(int kq, std::uint32_t fflags, std::intptr_t data, std::uint16_t flags = 0) {
    KernelEvent event{};
    Require(kq, Poll(kq, event) == 1, "expected one ready event");
    const bool matches = event.ident == 90 && event.filter == EvfiltUser && event.flags == flags &&
        event.fflags == fflags && event.data == data && event.udata == nullptr;
    if (!matches) {
        std::fprintf(stderr,
            "kqueue user state: ident=%" PRIuPTR " filter=%d flags=0x%x fflags=0x%" PRIx32
            " data=%" PRIdPTR " udata=%p; expected ident=90 filter=-11 flags=0x%x fflags=0x%" PRIx32
            " data=%" PRIdPTR " udata=null\n",
            event.ident, static_cast<int>(event.filter), static_cast<unsigned>(event.flags), event.fflags,
            event.data, event.udata, static_cast<unsigned>(flags), fflags, data);
    }
    Require(kq, matches, "event fields differ");
}

static void VerifyForcedOneshot(int kq) {
    KernelEvent event{};
    Apply(kq, EvAdd, 0, 0);
    Apply(kq, EvForceOneshot | EvClear, 0, 0);
    Require(kq, Poll(kq, event) == 1 && event.ident == 90 && event.filter == EvfiltUser &&
        event.flags == EvOneshot, "clear in the force change suppressed the one-shot event");
    Require(kq, Poll(kq, event) == 0, "forced one-shot event fired again");

    Apply(kq, EvAdd | EvDisable, 0, 0);
    Apply(kq, EvForceOneshot, 0, 0);
    Apply(kq, EvClear, 0, 0);
    Require(kq, Poll(kq, event) == 0, "disabled one-shot event fired");
    Apply(kq, EvEnable, 0, 0);
    Require(kq, Poll(kq, event) == 1 && event.ident == 90 && event.filter == EvfiltUser &&
        event.flags == EvOneshot, "clear while disabled suppressed the pending one-shot event");
    Require(kq, Poll(kq, event) == 0, "re-enabled one-shot event fired again");
}

static void VerifyDispatchState(int kq) {
    KernelEvent event{};
    Apply(kq, EvAdd | EvDispatch, 0x35, 19);
    Apply(kq, 0, NoteTrigger, 91);
    RequireEvent(kq, 0x35, 91, EvDispatch);
    Require(kq, Poll(kq, event) == 0, "dispatch left the note enabled");
    Apply(kq, EvEnable, NoteFfnop, 72);
    RequireEvent(kq, 0x35, 72, EvDispatch);
    Apply(kq, EvDelete, 0, 0);

    Apply(kq, EvAdd | EvDispatch | EvClear, 0x35, 19);
    Apply(kq, 0, NoteTrigger, 91);
    RequireEvent(kq, 0x35, 91, EvDispatch | EvClear);
    Apply(kq, EvEnable, NoteFfnop, 72);
    Require(kq, Poll(kq, event) == 0, "dispatch with clear kept the note active");
    Apply(kq, 0, NoteTrigger, 63);
    RequireEvent(kq, 0x35, 63, EvDispatch | EvClear);
    Apply(kq, EvDelete, 0, 0);
}

int main() {
    const int kq = kqueue_nid_postfix();
    Require(kq, kq >= 0, "queue creation failed");
    KernelEvent event{};
    Apply(kq, EvAdd, 0x35, 19);
    Require(kq, Poll(kq, event) == 0, "new note is active");
    Apply(kq, 0, NoteTrigger | NoteFfnop | 0x80, 91);
    Apply(kq, 0, NoteFfand | 0x1c, -7);
    RequireEvent(kq, 0x14, -7);
    RequireEvent(kq, 0x14, -7);
    Apply(kq, EvClear, NoteTrigger | NoteFfcopy | 0x81, -9);
    Require(kq, Poll(kq, event) == 0, "explicit clear retained activation");
    Apply(kq, 0, NoteTrigger | NoteFfnop | 0x02, 42);
    RequireEvent(kq, 0x81, 42);
    Apply(kq, EvDelete, 0, 0);
    VerifyForcedOneshot(kq);
    VerifyDispatchState(kq);
    Require(-1, close_nid_postfix(kq) == 0, "queue close failed");
    return 0;
}
