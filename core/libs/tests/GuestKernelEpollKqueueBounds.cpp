#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEnoent = 2;
constexpr int PosixEbadf = 9;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;
constexpr int PosixEspipe = 29;
constexpr int PosixEopnotsupp = 45;

constexpr std::int16_t EvfiltRead = -1;
constexpr std::int16_t EvfiltWrite = -2;
constexpr std::int16_t EvfiltAio = -3;
constexpr std::int16_t EvfiltVnode = -4;
constexpr std::int16_t EvfiltTimer = -7;
constexpr std::int16_t EvfiltLio = -10;
constexpr std::int16_t EvfiltUser = -11;
constexpr std::int16_t EvfiltSendfile = -12;

constexpr std::uint16_t EvAdd = 0x0001;
constexpr std::uint16_t EvDelete = 0x0002;
constexpr std::uint16_t EvEnable = 0x0004;
constexpr std::uint16_t EvDisable = 0x0008;
constexpr std::uint16_t EvClear = 0x0020;

constexpr std::uint32_t NoteTrigger = 0x01000000;

constexpr int SceKernelErrorEbadf = static_cast<int>(0x80020009);
constexpr int SceKernelErrorEnoent = static_cast<int>(0x80020002);
constexpr int SceKernelErrorEinval = static_cast<int>(0x80020016);
constexpr int SceKernelErrorEfault = static_cast<int>(0x8002000e);
constexpr int SceKernelErrorEexist = static_cast<int>(0x80020011);
constexpr int SceKernelErrorEtimedout = static_cast<int>(0x8002003c);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int GetErrno() {
    return *__error_nid_postfix();
}

void TestKqueueLifecycleAndDescriptorBounds() {
    const int kq1 = kqueue_nid_postfix();
    Require(kq1 >= 0);

    const int kq2 = kqueue_nid_postfix();
    Require(kq2 >= 0);
    Require(kq1 != kq2);

    char buffer[8]{};
    Require(read_nid_postfix(kq1, buffer, sizeof(buffer)) == -1 && GetErrno() == PosixEopnotsupp);
    Require(write_nid_postfix(kq1, buffer, sizeof(buffer)) == -1 && GetErrno() == PosixEopnotsupp);
    Require(read_nid_postfix(kq1, buffer, 0) == 0);
    Require(write_nid_postfix(kq1, buffer, 0) == -1 && GetErrno() == PosixEopnotsupp);
    Require(lseek_nid_postfix(kq1, 0, 0) == -1 && GetErrno() == PosixEspipe);
    Require(fsync_nid_postfix(kq1) == -1 && GetErrno() == PosixEinval);
    Require(fdatasync_nid_postfix(kq1) == -1 && GetErrno() == PosixEinval);
    Require(flock_nid_postfix(kq1, 1) == -1 && GetErrno() == PosixEopnotsupp);

    Require(close_nid_postfix(kq1) == 0);
    Require(close_nid_postfix(kq1) == -1 && GetErrno() == PosixEbadf);
    Require(close_nid_postfix(kq2) == 0);

    const int invalidDescriptors[] = {-100, -1, 65535, 1000000};
    for (const int fd : invalidDescriptors) {
        Require(close_nid_postfix(fd) == -1 && GetErrno() == PosixEbadf);
    }
}

void TestKeventDescriptorAndParameterBounds() {
    KernelTimespec zeroTimeout{0, 0};
    const int invalidDescriptors[] = {-100, -1, 65535, 1000000};
    for (const int fd : invalidDescriptors) {
        Require(kevent_nid_postfix(fd, nullptr, 0, nullptr, 0, &zeroTimeout) == -1 && GetErrno() == PosixEbadf);
    }

    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    const int badNevents[] = {-100, -10, -1};
    for (const int badCount : badNevents) {
        Require(kevent_nid_postfix(kq, nullptr, 0, nullptr, badCount, &zeroTimeout) == -1 && GetErrno() == PosixEinval);
    }

    const int badNchanges[] = {1, 2, 10};
    for (const int badCount : badNchanges) {
        Require(kevent_nid_postfix(kq, nullptr, badCount, nullptr, 0, &zeroTimeout) == -1 && GetErrno() == PosixEfault);
    }

    const auto* faultChangelist = reinterpret_cast<const KernelEvent*>(0x10);
    Require(kevent_nid_postfix(kq, faultChangelist, 1, nullptr, 0, &zeroTimeout) == -1 && GetErrno() == PosixEfault);

    const auto* faultTimeout = reinterpret_cast<const KernelTimespec*>(0x10);
    Require(kevent_nid_postfix(kq, nullptr, 0, nullptr, 0, faultTimeout) == -1 && GetErrno() == PosixEfault);

    const KernelTimespec badSec{-1, 0};
    Require(kevent_nid_postfix(kq, nullptr, 0, nullptr, 0, &badSec) == -1 && GetErrno() == PosixEinval);

    const KernelTimespec badNsecNeg{0, -1};
    Require(kevent_nid_postfix(kq, nullptr, 0, nullptr, 0, &badNsecNeg) == -1 && GetErrno() == PosixEinval);

    const KernelTimespec badNsecHigh{0, 1000000000LL};
    Require(kevent_nid_postfix(kq, nullptr, 0, nullptr, 0, &badNsecHigh) == -1 && GetErrno() == PosixEinval);

    const KernelTimespec badNsecOvershoot{2, 2000000000LL};
    Require(kevent_nid_postfix(kq, nullptr, 0, nullptr, 0, &badNsecOvershoot) == -1 && GetErrno() == PosixEinval);

    KernelEvent eventOut[2]{};
    Require(kevent_nid_postfix(kq, nullptr, 0, eventOut, 2, &zeroTimeout) == 0);
    Require(kevent_nid_postfix(kq, nullptr, 0, nullptr, 0, nullptr) == 0);

    Require(close_nid_postfix(kq) == 0);
}

void TestKeventFilterAndRegistrationBounds() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    KernelEvent badFlags{};
    badFlags.ident = 1;
    badFlags.filter = EvfiltUser;
    badFlags.flags = EvAdd | EvEnable | EvDisable;
    Require(kevent_nid_postfix(kq, &badFlags, 1, nullptr, 0, nullptr) == -1 && GetErrno() == PosixEinval);

    const std::int16_t positiveFilters[] = {1, 2, 5, 10, 100};
    for (const std::int16_t positiveFilter : positiveFilters) {
        KernelEvent badFilter{};
        badFilter.ident = 1;
        badFilter.filter = positiveFilter;
        badFilter.flags = EvAdd;
        Require(kevent_nid_postfix(kq, &badFilter, 1, nullptr, 0, nullptr) == -1 && GetErrno() == PosixEinval);
    }

    const std::int16_t unsupportedFilters[] = {EvfiltAio, EvfiltLio, EvfiltSendfile};
    for (const std::int16_t unsupportedFilter : unsupportedFilters) {
        KernelEvent unsupportedChange{};
        unsupportedChange.ident = 1;
        unsupportedChange.filter = unsupportedFilter;
        unsupportedChange.flags = EvAdd;
        Require(kevent_nid_postfix(kq, &unsupportedChange, 1, nullptr, 0, nullptr) == -1 && GetErrno() == PosixEperm);

        unsupportedChange.flags = EvDelete;
        Require(kevent_nid_postfix(kq, &unsupportedChange, 1, nullptr, 0, nullptr) == -1 && GetErrno() == PosixEnoent);
    }

    const std::int16_t descFilters[] = {EvfiltRead, EvfiltWrite, EvfiltVnode};
    for (const std::int16_t descFilter : descFilters) {
        KernelEvent selfChange{};
        selfChange.ident = static_cast<std::uintptr_t>(kq);
        selfChange.filter = descFilter;
        selfChange.flags = EvAdd;
        Require(kevent_nid_postfix(kq, &selfChange, 1, nullptr, 0, nullptr) == -1 && GetErrno() == PosixEinval);
    }

    KernelEvent badTimer{};
    badTimer.ident = 1;
    badTimer.filter = EvfiltTimer;
    badTimer.flags = EvAdd;
    badTimer.data = -1;
    Require(kevent_nid_postfix(kq, &badTimer, 1, nullptr, 0, nullptr) == -1 && GetErrno() == PosixEinval);

    badTimer.data = -100;
    Require(kevent_nid_postfix(kq, &badTimer, 1, nullptr, 0, nullptr) == -1 && GetErrno() == PosixEinval);

    badTimer.data = 1000;
    badTimer.fflags = 0xff000000;
    Require(kevent_nid_postfix(kq, &badTimer, 1, nullptr, 0, nullptr) == -1 && GetErrno() == PosixEinval);

    KernelEvent nonExistent{};
    nonExistent.ident = 9999;
    nonExistent.filter = EvfiltUser;
    nonExistent.flags = EvDelete;
    Require(kevent_nid_postfix(kq, &nonExistent, 1, nullptr, 0, nullptr) == -1 && GetErrno() == PosixEnoent);

    KernelEvent userEvent{};
    userEvent.ident = 42;
    userEvent.filter = EvfiltUser;
    userEvent.flags = EvAdd | EvClear;
    Require(kevent_nid_postfix(kq, &userEvent, 1, nullptr, 0, nullptr) == 0);

    userEvent.flags = 0;
    userEvent.fflags = NoteTrigger;
    Require(kevent_nid_postfix(kq, &userEvent, 1, nullptr, 0, nullptr) == 0);

    KernelEvent triggered[2]{};
    KernelTimespec zeroTimeout{0, 0};
    Require(kevent_nid_postfix(kq, nullptr, 0, triggered, 2, &zeroTimeout) == 1);
    Require(triggered[0].ident == 42);
    Require(triggered[0].filter == EvfiltUser);

    userEvent.flags = EvDelete;
    userEvent.fflags = 0;
    Require(kevent_nid_postfix(kq, &userEvent, 1, nullptr, 0, nullptr) == 0);
    Require(kevent_nid_postfix(kq, &userEvent, 1, nullptr, 0, nullptr) == -1 && GetErrno() == PosixEnoent);

    Require(close_nid_postfix(kq) == 0);
}

void TestPollParameterBounds() {
    const int badTimeouts[] = {-100, -10, -2};
    for (const int badTimeout : badTimeouts) {
        Require(poll_nid_postfix(nullptr, 0, badTimeout) == -1 && GetErrno() == PosixEinval);
    }

    const std::uint32_t badCounts[] = {1u, 5u, 100u};
    for (const std::uint32_t badCount : badCounts) {
        Require(poll_nid_postfix(nullptr, badCount, 0) == -1 && GetErrno() == PosixEfault);
    }

    auto* faultDesc = reinterpret_cast<GuestPollDescriptor*>(0x10);
    Require(poll_nid_postfix(faultDesc, 1, 0) == -1 && GetErrno() == PosixEfault);

    Require(poll_nid_postfix(nullptr, 0, 0) == 0);

    GuestPollDescriptor pfds[2]{};
    pfds[0].fd = -1;
    pfds[0].events = 1;
    pfds[1].fd = -5;
    pfds[1].events = 4;
    Require(poll_nid_postfix(pfds, 2, 0) == 0);
    Require(pfds[0].revents == 0 && pfds[1].revents == 0);

    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    GuestPollDescriptor kqPfd{};
    kqPfd.fd = kq;
    kqPfd.events = 1;
    Require(poll_nid_postfix(&kqPfd, 1, 0) == 0);
    Require(kqPfd.revents == 0);

    Require(close_nid_postfix(kq) == 0);
}

void TestSceKernelEqueueBounds() {
    KernelEqueue eq{};
    Require(sceKernelCreateEqueue(nullptr, "bounds") == SceKernelErrorEinval);
    Require(sceKernelCreateEqueue(&eq, nullptr) == SceKernelErrorEinval);
    Require(sceKernelCreateEqueue(nullptr, nullptr) == SceKernelErrorEinval);

    const KernelEqueue invalidHandles[] = {0ull, 1ull, 0xdeadbeefull, ~0ull};
    for (const KernelEqueue invalidHandle : invalidHandles) {
        Require(sceKernelDeleteEqueue(invalidHandle) == SceKernelErrorEbadf);
    }

    Require(sceKernelCreateEqueue(&eq, "bounds_queue") == 0);
    Require(eq != 0);

    KernelEvent outEvents[2]{};
    int outCount = -1;
    KernelUseconds timeoutUsec = 0;
    Require(sceKernelWaitEqueue(0, outEvents, 2, &outCount, &timeoutUsec) == SceKernelErrorEbadf);
    Require(sceKernelWaitEqueue(eq, nullptr, 2, &outCount, &timeoutUsec) == SceKernelErrorEfault);
    Require(sceKernelWaitEqueue(eq, outEvents, 0, &outCount, &timeoutUsec) == SceKernelErrorEinval);
    Require(sceKernelWaitEqueue(eq, outEvents, -1, &outCount, &timeoutUsec) == SceKernelErrorEinval);
    Require(sceKernelWaitEqueue(eq, outEvents, 2, nullptr, &timeoutUsec) == SceKernelErrorEinval);

    outCount = -1;
    Require(sceKernelWaitEqueue(eq, outEvents, 2, &outCount, &timeoutUsec) == SceKernelErrorEtimedout);
    Require(outCount == 0);

    Require(sceKernelAddUserEvent(0, 100) == SceKernelErrorEbadf);
    Require(sceKernelAddUserEvent(eq, 100) == 0);
    Require(sceKernelAddUserEvent(eq, 100) == SceKernelErrorEexist);
    Require(sceKernelDeleteUserEvent(0, 100) == SceKernelErrorEbadf);
    Require(sceKernelDeleteUserEvent(eq, 100) == 0);
    Require(sceKernelDeleteUserEvent(eq, 100) == SceKernelErrorEnoent);

    Require(sceKernelDeleteEqueue(eq) == 0);
    Require(sceKernelDeleteEqueue(eq) == SceKernelErrorEbadf);
    Require(sceKernelWaitEqueue(eq, outEvents, 2, &outCount, &timeoutUsec) == SceKernelErrorEbadf);
}

} // namespace

int main() {
    TestKqueueLifecycleAndDescriptorBounds();
    TestKeventDescriptorAndParameterBounds();
    TestKeventFilterAndRegistrationBounds();
    TestPollParameterBounds();
    TestSceKernelEqueueBounds();
    return 0;
}
