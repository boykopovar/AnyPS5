#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {
constexpr std::int16_t EvfiltTimer = -7;
constexpr std::uint16_t EvAdd = 0x0001;
constexpr std::uint16_t EvDelete = 0x0002;
constexpr std::uint16_t EvOneshot = 0x0010;
constexpr std::uint16_t EvError = 0x4000;
constexpr std::uint32_t NoteMseconds = 0x0002;
constexpr std::uint32_t NoteUseconds = 0x0004;

constexpr int GuestEnoent = 2;
constexpr int GuestEbadf = 9;
constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "kqueue timer check failed at line %u, errno %d\n", static_cast<unsigned>(location.line()), *__error_nid_postfix());
    std::abort();
}

void AuditInvalidDescriptorsAndPointers() {
    KernelEvent change{};
    change.ident = 1;
    change.filter = EvfiltTimer;
    change.flags = EvAdd | EvOneshot;
    change.fflags = NoteUseconds;
    change.data = 1000;

    *__error_nid_postfix() = 0;
    Require(kevent_nid_postfix(-1, &change, 1, nullptr, 0, nullptr) == -1 && *__error_nid_postfix() == GuestEbadf);

    *__error_nid_postfix() = 0;
    Require(kevent_nid_postfix(9999, &change, 1, nullptr, 0, nullptr) == -1 && *__error_nid_postfix() == GuestEbadf);

    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    *__error_nid_postfix() = 0;
    Require(kevent_nid_postfix(kq, nullptr, 1, nullptr, 0, nullptr) == -1 && *__error_nid_postfix() == GuestEfault);

    *__error_nid_postfix() = 0;
    KernelEvent outEvent{};
    Require(kevent_nid_postfix(kq, nullptr, 0, &outEvent, -1, nullptr) == -1 && *__error_nid_postfix() == GuestEinval);

    Require(close_nid_postfix(kq) == 0);
}

void AuditNegativeIntervalAndInvalidFlags() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    KernelEvent change{};
    change.ident = 10;
    change.filter = EvfiltTimer;
    change.flags = EvAdd | EvOneshot;
    change.fflags = NoteUseconds;
    change.data = -1;

    *__error_nid_postfix() = 0;
    Require(kevent_nid_postfix(kq, &change, 1, nullptr, 0, nullptr) == -1 && *__error_nid_postfix() == GuestEinval);

    KernelEvent errEvent{};
    Require(kevent_nid_postfix(kq, &change, 1, &errEvent, 1, nullptr) == 1);
    Require((errEvent.flags & EvError) != 0 && errEvent.data == GuestEinval);

    change.data = -1000;
    *__error_nid_postfix() = 0;
    Require(kevent_nid_postfix(kq, &change, 1, nullptr, 0, nullptr) == -1 && *__error_nid_postfix() == GuestEinval);

    change.data = 5000;
    change.fflags = 0x8000;
    *__error_nid_postfix() = 0;
    Require(kevent_nid_postfix(kq, &change, 1, nullptr, 0, nullptr) == -1 && *__error_nid_postfix() == GuestEinval);

    change.fflags = NoteUseconds | 0x01;
    *__error_nid_postfix() = 0;
    Require(kevent_nid_postfix(kq, &change, 1, nullptr, 0, nullptr) == -1 && *__error_nid_postfix() == GuestEinval);

    change.fflags = 0x0010;
    *__error_nid_postfix() = 0;
    Require(kevent_nid_postfix(kq, &change, 1, nullptr, 0, nullptr) == -1 && *__error_nid_postfix() == GuestEinval);

    change.flags = EvDelete;
    change.ident = 999;
    change.fflags = 0;
    change.data = 0;
    *__error_nid_postfix() = 0;
    Require(kevent_nid_postfix(kq, &change, 1, nullptr, 0, nullptr) == -1 && *__error_nid_postfix() == GuestEnoent);

    Require(close_nid_postfix(kq) == 0);
}

void AuditValidTimerRegistrationAndFiring() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    KernelEvent change{};
    change.ident = 1;
    change.filter = EvfiltTimer;
    change.flags = EvAdd | EvOneshot;
    change.fflags = NoteUseconds;
    change.data = 10000;
    change.udata = 123;

    KernelEvent event{};
    Require(kevent_nid_postfix(kq, &change, 1, &event, 1, nullptr) >= 0);

    *__error_nid_postfix() = 0;
    change.ident = 2;
    change.fflags = NoteMseconds;
    change.data = 5;
    change.udata = 456;
    Require(kevent_nid_postfix(kq, &change, 1, &event, 1, nullptr) >= 0);

    change.flags = EvDelete;
    change.ident = 1;
    change.fflags = 0;
    change.data = 0;
    Require(kevent_nid_postfix(kq, &change, 1, nullptr, 0, nullptr) == 0);

    Require(close_nid_postfix(kq) == 0);
}
} // namespace

int main() {
    AuditInvalidDescriptorsAndPointers();
    AuditNegativeIntervalAndInvalidFlags();
    AuditValidTimerRegistrationAndFiring();
    return 0;
}
