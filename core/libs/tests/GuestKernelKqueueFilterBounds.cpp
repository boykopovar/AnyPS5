#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEnoent = 2;
constexpr int PosixEbadf = 9;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;

constexpr std::int16_t EvfiltRead = -1;
constexpr std::int16_t EvfiltWrite = -2;
constexpr std::int16_t EvfiltAio = -3;
constexpr std::int16_t EvfiltVnode = -4;
constexpr std::int16_t EvfiltProc = -5;
constexpr std::int16_t EvfiltSignal = -6;
constexpr std::int16_t EvfiltTimer = -7;
constexpr std::int16_t EvfiltProcdesc = -8;
constexpr std::int16_t EvfiltFs = -9;
constexpr std::int16_t EvfiltLio = -10;
constexpr std::int16_t EvfiltUser = -11;
constexpr std::int16_t EvfiltSendfile = -12;

constexpr std::uint16_t EvAdd = 0x0001;
constexpr std::uint16_t EvDelete = 0x0002;
constexpr std::uint16_t EvEnable = 0x0004;
constexpr std::uint16_t EvDisable = 0x0008;
constexpr std::uint16_t EvReceipt = 0x0040;
constexpr std::uint16_t EvError = 0x4000;

constexpr std::uint32_t NoteTrigger = 0x01000000;
constexpr std::uint32_t NoteSeconds = 0x0001;
constexpr std::uint32_t NoteMseconds = 0x0002;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int Error() {
    return *__error_nid_postfix();
}

void SetError(int err) {
    *__error_nid_postfix() = err;
}

KernelEvent MakeChange(std::uintptr_t ident, std::int16_t filter, std::uint16_t flags, std::uint32_t fflags = 0, std::intptr_t data = 0, void* udata = nullptr) {
    KernelEvent ev{};
    ev.ident = ident;
    ev.filter = filter;
    ev.flags = flags;
    ev.fflags = fflags;
    ev.data = data;
    ev.udata = udata;
    return ev;
}

void TestKqueueDescriptorBounds() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    SetError(0);
    Require(kevent_nid_postfix(-1, nullptr, 0, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEbadf);

    SetError(0);
    Require(kevent_nid_postfix(-100, nullptr, 0, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEbadf);

    SetError(0);
    Require(kevent_nid_postfix(999999, nullptr, 0, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEbadf);

    int ends[2] = {-1, -1};
    Require(pipe_nid_postfix(ends) == 0);
    SetError(0);
    Require(kevent_nid_postfix(ends[0], nullptr, 0, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEbadf);
    Require(close_nid_postfix(ends[0]) == 0);
    Require(close_nid_postfix(ends[1]) == 0);

    const int tempKq = kqueue_nid_postfix();
    Require(tempKq >= 0);
    Require(close_nid_postfix(tempKq) == 0);
    SetError(0);
    Require(kevent_nid_postfix(tempKq, nullptr, 0, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEbadf);

    Require(close_nid_postfix(kq) == 0);
}

void TestKeventParameterBounds() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    SetError(0);
    Require(kevent_nid_postfix(kq, nullptr, 0, nullptr, -1, nullptr) == -1);
    Require(Error() == PosixEinval);

    SetError(0);
    Require(kevent_nid_postfix(kq, nullptr, 0, nullptr, -50, nullptr) == -1);
    Require(Error() == PosixEinval);

    SetError(0);
    Require(kevent_nid_postfix(kq, nullptr, 1, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(kevent_nid_postfix(kq, nullptr, 10, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEfault);

    const KernelTimespec badSec{-1, 0};
    SetError(0);
    Require(kevent_nid_postfix(kq, nullptr, 0, nullptr, 0, &badSec) == -1);
    Require(Error() == PosixEinval);

    const KernelTimespec badNsecNeg{0, -1};
    SetError(0);
    Require(kevent_nid_postfix(kq, nullptr, 0, nullptr, 0, &badNsecNeg) == -1);
    Require(Error() == PosixEinval);

    const KernelTimespec badNsecHigh{0, 1000000000LL};
    SetError(0);
    Require(kevent_nid_postfix(kq, nullptr, 0, nullptr, 0, &badNsecHigh) == -1);
    Require(Error() == PosixEinval);

    const KernelTimespec badNsecHigher{0, 2000000000LL};
    SetError(0);
    Require(kevent_nid_postfix(kq, nullptr, 0, nullptr, 0, &badNsecHigher) == -1);
    Require(Error() == PosixEinval);

    SetError(0);
    Require(kevent_nid_postfix(kq, nullptr, 0, nullptr, 0, reinterpret_cast<const KernelTimespec*>(0x10)) == -1);
    Require(Error() == PosixEfault);

    Require(close_nid_postfix(kq) == 0);
}

void TestPositiveAndZeroFilterBounds() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    const std::int16_t positiveFilters[] = {1, 2, 5, 42, 1000, 32767};
    for (const auto f : positiveFilters) {
        KernelEvent change = MakeChange(1, f, EvAdd);
        SetError(0);
        Require(kevent_nid_postfix(kq, &change, 1, nullptr, 0, nullptr) == -1);
        Require(Error() == PosixEinval);

        KernelEvent out{};
        SetError(0);
        Require(kevent_nid_postfix(kq, &change, 1, &out, 1, nullptr) == 1);
        Require((out.flags & EvError) != 0);
        Require(out.data == PosixEinval);
        Require(sceKernelGetEventError(&out) == PosixEinval);
        Require(sceKernelGetEventFilter(&out) == f);
        Require(sceKernelGetEventId(&out) == 1);

        KernelEvent receiptChange = MakeChange(2, f, static_cast<std::uint16_t>(EvAdd | EvReceipt));
        KernelEvent receiptOut{};
        SetError(0);
        Require(kevent_nid_postfix(kq, &receiptChange, 1, &receiptOut, 1, nullptr) == 1);
        Require((receiptOut.flags & EvError) != 0);
        Require(receiptOut.data == PosixEinval);
    }

    KernelEvent zeroFilter = MakeChange(10, 0, EvAdd);
    SetError(0);
    Require(kevent_nid_postfix(kq, &zeroFilter, 1, nullptr, 0, nullptr) == 0);
    Require(Error() == 0);

    const KernelTimespec zeroTime{0, 0};
    KernelEvent pollOut{};
    Require(kevent_nid_postfix(kq, &zeroFilter, 1, &pollOut, 1, &zeroTime) == 0);

    Require(close_nid_postfix(kq) == 0);
}

void TestNegativeFilterBoundaryAndUnsupported() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    const std::int16_t unsupportedFilters[] = {-13, -14, -50, -100, -32768};
    for (const auto f : unsupportedFilters) {
        KernelEvent change = MakeChange(1, f, EvAdd);
        bool threw = false;
        try {
            kevent_nid_postfix(kq, &change, 1, nullptr, 0, nullptr);
        } catch (const std::runtime_error& err) {
            threw = std::string(err.what()).find("kevent: filter") != std::string::npos;
        }
        Require(threw);
    }

    Require(close_nid_postfix(kq) == 0);
}

void TestRestrictedFilterBounds() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    const std::int16_t restrictedFilters[] = {EvfiltAio, EvfiltLio, EvfiltSendfile};
    for (const auto f : restrictedFilters) {
        KernelEvent noAdd = MakeChange(10, f, 0);
        SetError(0);
        Require(kevent_nid_postfix(kq, &noAdd, 1, nullptr, 0, nullptr) == -1);
        Require(Error() == PosixEnoent);

        KernelEvent outNoAdd{};
        Require(kevent_nid_postfix(kq, &noAdd, 1, &outNoAdd, 1, nullptr) == 1);
        Require((outNoAdd.flags & EvError) != 0);
        Require(outNoAdd.data == PosixEnoent);
        Require(outNoAdd.fflags == 0);

        KernelEvent withAdd = MakeChange(10, f, EvAdd);
        SetError(0);
        Require(kevent_nid_postfix(kq, &withAdd, 1, nullptr, 0, nullptr) == -1);
        Require(Error() == PosixEperm);

        KernelEvent outWithAdd{};
        Require(kevent_nid_postfix(kq, &withAdd, 1, &outWithAdd, 1, nullptr) == 1);
        Require((outWithAdd.flags & EvError) != 0);
        Require(outWithAdd.data == PosixEperm);
        Require(outWithAdd.fflags == 0);
    }

    Require(close_nid_postfix(kq) == 0);
}

void TestProcdescFilterBounds() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    KernelEvent hugeIdent = MakeChange(static_cast<std::uintptr_t>(INT_MAX) + 1, EvfiltProcdesc, EvAdd);
    SetError(0);
    Require(kevent_nid_postfix(kq, &hugeIdent, 1, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEbadf);

    KernelEvent selfKq = MakeChange(static_cast<std::uintptr_t>(kq), EvfiltProcdesc, EvAdd);
    SetError(0);
    Require(kevent_nid_postfix(kq, &selfKq, 1, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEinval);

    KernelEvent badFd = MakeChange(999999, EvfiltProcdesc, EvAdd);
    SetError(0);
    Require(kevent_nid_postfix(kq, &badFd, 1, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEbadf);

    int ends[2] = {-1, -1};
    Require(pipe_nid_postfix(ends) == 0);

    KernelEvent openNoAdd = MakeChange(static_cast<std::uintptr_t>(ends[0]), EvfiltProcdesc, 0);
    SetError(0);
    Require(kevent_nid_postfix(kq, &openNoAdd, 1, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEnoent);

    KernelEvent openWithAdd = MakeChange(static_cast<std::uintptr_t>(ends[0]), EvfiltProcdesc, EvAdd);
    SetError(0);
    Require(kevent_nid_postfix(kq, &openWithAdd, 1, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEinval);

    Require(close_nid_postfix(ends[0]) == 0);
    Require(close_nid_postfix(ends[1]) == 0);

    Require(close_nid_postfix(kq) == 0);
}

void TestDescriptorFiltersBounds() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    const std::int16_t descFilters[] = {EvfiltRead, EvfiltWrite, EvfiltVnode};
    for (const auto f : descFilters) {
        KernelEvent huge = MakeChange(static_cast<std::uintptr_t>(INT_MAX) + 1, f, EvAdd);
        SetError(0);
        Require(kevent_nid_postfix(kq, &huge, 1, nullptr, 0, nullptr) == -1);
        Require(Error() == PosixEbadf);

        KernelEvent self = MakeChange(static_cast<std::uintptr_t>(kq), f, EvAdd);
        SetError(0);
        Require(kevent_nid_postfix(kq, &self, 1, nullptr, 0, nullptr) == -1);
        Require(Error() == PosixEinval);

        KernelEvent closed = MakeChange(999999, f, EvAdd);
        SetError(0);
        Require(kevent_nid_postfix(kq, &closed, 1, nullptr, 0, nullptr) == -1);
        Require(Error() == PosixEbadf);
    }

    Require(close_nid_postfix(kq) == 0);
}

void TestFlagConflictAndLifecycleBounds() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    KernelEvent conflict = MakeChange(1, EvfiltUser, static_cast<std::uint16_t>(EvEnable | EvDisable));
    SetError(0);
    Require(kevent_nid_postfix(kq, &conflict, 1, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEinval);

    KernelEvent delNonExist = MakeChange(8888, EvfiltUser, EvDelete);
    SetError(0);
    Require(kevent_nid_postfix(kq, &delNonExist, 1, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEnoent);

    KernelEvent disNonExist = MakeChange(8889, EvfiltUser, EvDisable);
    SetError(0);
    Require(kevent_nid_postfix(kq, &disNonExist, 1, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEnoent);

    Require(close_nid_postfix(kq) == 0);
}

void TestTimerFilterBounds() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    KernelEvent negTimer = MakeChange(1, EvfiltTimer, EvAdd, 0, -1);
    SetError(0);
    Require(kevent_nid_postfix(kq, &negTimer, 1, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEinval);

    KernelEvent negTimerBig = MakeChange(1, EvfiltTimer, EvAdd, 0, -1000);
    SetError(0);
    Require(kevent_nid_postfix(kq, &negTimerBig, 1, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEinval);

    KernelEvent badFflags = MakeChange(1, EvfiltTimer, EvAdd, 0x0010, 100);
    SetError(0);
    Require(kevent_nid_postfix(kq, &badFflags, 1, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEinval);

    KernelEvent badFflagsComb = MakeChange(1, EvfiltTimer, EvAdd, static_cast<std::uint32_t>(NoteSeconds | NoteMseconds), 100);
    SetError(0);
    Require(kevent_nid_postfix(kq, &badFflagsComb, 1, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEinval);

    KernelEvent validTimer = MakeChange(2, EvfiltTimer, EvAdd, NoteMseconds, 1000);
    Require(kevent_nid_postfix(kq, &validTimer, 1, nullptr, 0, nullptr) == 0);

    KernelEvent delTimer = MakeChange(2, EvfiltTimer, EvDelete);
    Require(kevent_nid_postfix(kq, &delTimer, 1, nullptr, 0, nullptr) == 0);

    SetError(0);
    Require(kevent_nid_postfix(kq, &delTimer, 1, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEnoent);

    Require(close_nid_postfix(kq) == 0);
}

void TestProcessFilterBounds() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    const auto myPid = static_cast<std::uintptr_t>(getpid_nid_postfix());
    KernelEvent foreignProc = MakeChange(myPid + 12345, EvfiltProc, EvAdd);
    bool threw = false;
    try {
        kevent_nid_postfix(kq, &foreignProc, 1, nullptr, 0, nullptr);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    KernelEvent badNotes = MakeChange(myPid, EvfiltProc, EvAdd, 0x01);
    bool badNotesThrew = false;
    try {
        kevent_nid_postfix(kq, &badNotes, 1, nullptr, 0, nullptr);
    } catch (const std::runtime_error&) {
        badNotesThrew = true;
    }
    Require(badNotesThrew);

    Require(close_nid_postfix(kq) == 0);
}

void TestUserFilterLifecycleBounds() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    KernelEvent add = MakeChange(700, EvfiltUser, EvAdd);
    Require(kevent_nid_postfix(kq, &add, 1, nullptr, 0, nullptr) == 0);

    KernelEvent trigger = MakeChange(700, EvfiltUser, 0, NoteTrigger);
    Require(kevent_nid_postfix(kq, &trigger, 1, nullptr, 0, nullptr) == 0);

    const KernelTimespec zeroTime{0, 0};
    KernelEvent out{};
    Require(kevent_nid_postfix(kq, nullptr, 0, &out, 1, &zeroTime) == 1);
    Require(out.ident == 700);
    Require(out.filter == EvfiltUser);

    KernelEvent del = MakeChange(700, EvfiltUser, EvDelete);
    Require(kevent_nid_postfix(kq, &del, 1, nullptr, 0, nullptr) == 0);

    SetError(0);
    Require(kevent_nid_postfix(kq, &del, 1, nullptr, 0, nullptr) == -1);
    Require(Error() == PosixEnoent);

    Require(close_nid_postfix(kq) == 0);
}

void TestEventGetterHelpers() {
    KernelEvent ev{};
    ev.ident = 12345;
    ev.filter = EvfiltUser;
    ev.flags = EvError;
    ev.fflags = 0x55aa;
    ev.data = 42;
    int dummy = 0;
    ev.udata = &dummy;

    Require(sceKernelGetEventId(&ev) == 12345);
    Require(sceKernelGetEventFilter(&ev) == EvfiltUser);
    Require(sceKernelGetEventFflags(&ev) == 0x55aa);
    Require(sceKernelGetEventData(&ev) == 42);
    Require(sceKernelGetEventUserData(&ev) == &dummy);
    Require(sceKernelGetEventError(&ev) == 42);

    ev.flags = 0;
    Require(sceKernelGetEventError(&ev) == 0);
}

} // namespace

int main() {
    TestKqueueDescriptorBounds();
    TestKeventParameterBounds();
    TestPositiveAndZeroFilterBounds();
    TestNegativeFilterBoundaryAndUnsupported();
    TestRestrictedFilterBounds();
    TestProcdescFilterBounds();
    TestDescriptorFiltersBounds();
    TestFlagConflictAndLifecycleBounds();
    TestTimerFilterBounds();
    TestProcessFilterBounds();
    TestUserFilterLifecycleBounds();
    TestEventGetterHelpers();
    return 0;
}
