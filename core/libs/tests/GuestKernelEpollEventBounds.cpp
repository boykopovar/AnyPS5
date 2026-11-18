#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int PosixEbadf = 9;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;

constexpr short PollIn = 0x0001;
constexpr short PollPri = 0x0002;
constexpr short PollOut = 0x0004;
constexpr short PollErr = 0x0008;
constexpr short PollHup = 0x0010;
constexpr short PollNval = 0x0020;
constexpr short PollRdNorm = 0x0040;
constexpr short PollRdBand = 0x0080;
constexpr short PollWrBand = 0x0100;

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

void SetErrno(int err) {
    *__error_nid_postfix() = err;
}

void TestPollParameterBounds() {
    Require(poll_nid_postfix(nullptr, 0, 0) == 0);

    const int invalidTimeouts[] = {-1000, -100, -10, -5, -2};
    for (const int badTimeout : invalidTimeouts) {
        SetErrno(0);
        Require(poll_nid_postfix(nullptr, 0, badTimeout) == -1);
        Require(GetErrno() == PosixEinval);
    }

    const std::uint32_t counts[] = {1u, 2u, 10u, 100u};
    for (const std::uint32_t count : counts) {
        SetErrno(0);
        Require(poll_nid_postfix(nullptr, count, 0) == -1);
        Require(GetErrno() == PosixEfault);
    }

    auto* badPtr1 = reinterpret_cast<GuestPollDescriptor*>(std::uintptr_t{0x10});
    SetErrno(0);
    Require(poll_nid_postfix(badPtr1, 1, 0) == -1);
    Require(GetErrno() == PosixEfault);

    auto* badPtr2 = reinterpret_cast<GuestPollDescriptor*>(std::uintptr_t{0xdeadbeef});
    SetErrno(0);
    Require(poll_nid_postfix(badPtr2, 1, 0) == -1);
    Require(GetErrno() == PosixEfault);

    auto* page = static_cast<GuestPollDescriptor*>(mmap_nid_postfix(nullptr, 4096, 3, 0x1002, -1, 0));
    if (page != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1))) {
        page->descriptor = -1;
        page->events = PollIn;
        page->revents = 0;

        Require(mprotect_nid_postfix(page, 4096, 0) == 0);
        SetErrno(0);
        Require(poll_nid_postfix(page, 1, 0) == -1);
        Require(GetErrno() == PosixEfault);

        Require(mprotect_nid_postfix(page, 4096, 3) == 0);
        Require(poll_nid_postfix(page, 1, 0) == 0);
        Require(page->revents == 0);
        Require(munmap_nid_postfix(page, 4096) == 0);
    }
}

void TestPollDescriptorEventFlagsAndMaskBounds() {
    GuestPollDescriptor negativePfds[3]{};
    negativePfds[0].descriptor = -1;
    negativePfds[0].events = PollIn | PollOut;
    negativePfds[0].revents = static_cast<short>(0x5555);

    negativePfds[1].descriptor = -10;
    negativePfds[1].events = PollPri | PollRdNorm;
    negativePfds[1].revents = static_cast<short>(0xAAAA);

    negativePfds[2].descriptor = -100;
    negativePfds[2].events = PollRdBand | PollWrBand;
    negativePfds[2].revents = static_cast<short>(0x00FF);

    Require(poll_nid_postfix(negativePfds, 3, 0) == 0);
    Require(negativePfds[0].revents == 0);
    Require(negativePfds[1].revents == 0);
    Require(negativePfds[2].revents == 0);

    GuestPollDescriptor invalidPfds[2]{};
    invalidPfds[0].descriptor = 999999;
    invalidPfds[0].events = PollIn;
    invalidPfds[0].revents = 0;

    invalidPfds[1].descriptor = 65535;
    invalidPfds[1].events = PollOut;
    invalidPfds[1].revents = 0;

    Require(poll_nid_postfix(invalidPfds, 2, 0) == 2);
    Require(invalidPfds[0].revents == PollNval);
    Require(invalidPfds[1].revents == PollNval);

    const short customEventMasks[] = {
        0,
        PollIn,
        PollOut,
        PollPri,
        PollRdNorm,
        PollRdBand,
        PollWrBand,
        static_cast<short>(PollIn | PollOut | PollPri),
        static_cast<short>(PollRdNorm | PollRdBand | PollWrBand),
        static_cast<short>(0x0200),
        static_cast<short>(0x0400),
        static_cast<short>(0x8000),
        static_cast<short>(0x7F00),
        static_cast<short>(0xFFFF)
    };

    for (const short mask : customEventMasks) {
        GuestPollDescriptor testPfd{};
        testPfd.descriptor = -1;
        testPfd.events = mask;
        testPfd.revents = static_cast<short>(0x1234);

        Require(poll_nid_postfix(&testPfd, 1, 0) == 0);
        Require(testPfd.revents == 0);
    }
}

void TestPipePollEventTransitionsAndHangup() {
    int fds[2]{-1, -1};
    Require(pipe_nid_postfix(fds) == 0);
    Require(fds[0] >= 0 && fds[1] >= 0);

    GuestPollDescriptor readPfd{};
    readPfd.descriptor = fds[0];
    readPfd.events = PollIn | PollRdNorm;
    readPfd.revents = 0;

    GuestPollDescriptor writePfd{};
    writePfd.descriptor = fds[1];
    writePfd.events = PollOut;
    writePfd.revents = 0;

    Require(poll_nid_postfix(&readPfd, 1, 0) == 0);
    Require(readPfd.revents == 0);

    Require(poll_nid_postfix(&writePfd, 1, 0) == 1);
    Require((writePfd.revents & PollOut) != 0);

    const char writeBytes[4] = {'p', 'i', 'p', 'e'};
    Require(write_nid_postfix(fds[1], writeBytes, sizeof(writeBytes)) == 4);

    readPfd.revents = 0;
    Require(poll_nid_postfix(&readPfd, 1, 0) == 1);
    Require((readPfd.revents & PollIn) != 0);

    char readBytes[4]{};
    Require(read_nid_postfix(fds[0], readBytes, sizeof(readBytes)) == 4);

    readPfd.revents = 0;
    Require(poll_nid_postfix(&readPfd, 1, 0) == 0);
    Require(readPfd.revents == 0);

    Require(close_nid_postfix(fds[1]) == 0);

    readPfd.revents = 0;
    Require(poll_nid_postfix(&readPfd, 1, 0) == 1);
    Require((readPfd.revents & (PollHup | PollIn)) != 0);

    Require(close_nid_postfix(fds[0]) == 0);

    readPfd.revents = 0;
    Require(poll_nid_postfix(&readPfd, 1, 0) == 1);
    Require(readPfd.revents == PollNval);
}

void TestMixedPollArrayBounds() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    GuestPollDescriptor mixed[4]{};
    mixed[0].descriptor = -1;
    mixed[0].events = PollIn;
    mixed[0].revents = static_cast<short>(0x7777);

    mixed[1].descriptor = 88888;
    mixed[1].events = PollIn;
    mixed[1].revents = 0;

    mixed[2].descriptor = pipeFds[1];
    mixed[2].events = PollOut;
    mixed[2].revents = 0;

    mixed[3].descriptor = -99;
    mixed[3].events = PollOut;
    mixed[3].revents = static_cast<short>(0x8888);

    Require(poll_nid_postfix(mixed, 4, 0) == 2);
    Require(mixed[0].revents == 0);
    Require(mixed[1].revents == PollNval);
    Require((mixed[2].revents & PollOut) != 0);
    Require(mixed[3].revents == 0);

    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void TestKernelEventAccessorsAndBounds() {
    KernelEvent ev{};
    ev.ident = 12345;
    ev.filter = -1;
    ev.flags = 0x0001;
    ev.fflags = 0x00000004;
    ev.data = 4096;
    int userVal = 42;
    ev.udata = &userVal;

    Require(sceKernelGetEventId(&ev) == 12345);
    Require(sceKernelGetEventFilter(&ev) == -1);
    Require(sceKernelGetEventFflags(&ev) == 4);
    Require(sceKernelGetEventData(&ev) == 4096);
    Require(sceKernelGetEventUserData(&ev) == &userVal);
    Require(sceKernelGetEventError(&ev) == 0);

    ev.ident = UINTPTR_MAX;
    ev.filter = -7;
    ev.flags = 0x0020;
    ev.fflags = 0xFFFFFFFFu;
    ev.data = -1;
    ev.udata = nullptr;

    Require(sceKernelGetEventId(&ev) == UINTPTR_MAX);
    Require(sceKernelGetEventFilter(&ev) == -7);
    Require(sceKernelGetEventFflags(&ev) == 0xFFFFFFFFu);
    Require(sceKernelGetEventData(&ev) == -1);
    Require(sceKernelGetEventUserData(&ev) == nullptr);
    Require(sceKernelGetEventError(&ev) == 0);

    bool caughtData = false;
    try {
        sceKernelGetEventData(nullptr);
    } catch (const std::runtime_error&) {
        caughtData = true;
    }
    Require(caughtData);

    bool caughtId = false;
    try {
        sceKernelGetEventId(nullptr);
    } catch (const std::runtime_error&) {
        caughtId = true;
    }
    Require(caughtId);

    bool caughtFilter = false;
    try {
        sceKernelGetEventFilter(nullptr);
    } catch (const std::runtime_error&) {
        caughtFilter = true;
    }
    Require(caughtFilter);

    bool caughtFflags = false;
    try {
        sceKernelGetEventFflags(nullptr);
    } catch (const std::runtime_error&) {
        caughtFflags = true;
    }
    Require(caughtFflags);

    bool caughtError = false;
    try {
        sceKernelGetEventError(nullptr);
    } catch (const std::runtime_error&) {
        caughtError = true;
    }
    Require(caughtError);

    bool caughtUserData = false;
    try {
        sceKernelGetEventUserData(nullptr);
    } catch (const std::runtime_error&) {
        caughtUserData = true;
    }
    Require(caughtUserData);
}

void TestSceKernelEqueueEventBounds() {
    KernelEqueue eq{};
    Require(sceKernelCreateEqueue(nullptr, "bounds") == SceKernelErrorEinval);
    Require(sceKernelCreateEqueue(&eq, nullptr) == SceKernelErrorEinval);
    Require(sceKernelCreateEqueue(nullptr, nullptr) == SceKernelErrorEinval);

    const KernelEqueue badHandles[] = {0ull, 1ull, 0xdeadbeefull, ~0ull};
    for (const KernelEqueue badHandle : badHandles) {
        Require(sceKernelDeleteEqueue(badHandle) == SceKernelErrorEbadf);
    }

    Require(sceKernelCreateEqueue(&eq, "epoll_event_bounds_queue") == 0);
    Require(eq != 0);

    KernelEvent events[2]{};
    int outCount = -1;
    KernelUseconds timo = 0;

    Require(sceKernelWaitEqueue(0, events, 2, &outCount, &timo) == SceKernelErrorEbadf);
    Require(sceKernelWaitEqueue(eq, nullptr, 2, &outCount, &timo) == SceKernelErrorEfault);
    Require(sceKernelWaitEqueue(eq, events, 0, &outCount, &timo) == SceKernelErrorEinval);
    Require(sceKernelWaitEqueue(eq, events, -1, &outCount, &timo) == SceKernelErrorEinval);
    Require(sceKernelWaitEqueue(eq, events, 2, nullptr, &timo) == SceKernelErrorEinval);

    outCount = -1;
    Require(sceKernelWaitEqueue(eq, events, 2, &outCount, &timo) == SceKernelErrorEtimedout);
    Require(outCount == 0);

    Require(sceKernelAddReadEvent(0, 10, 100, nullptr) == SceKernelErrorEbadf);
    Require(sceKernelDeleteReadEvent(0, 10) == SceKernelErrorEbadf);
    Require(sceKernelAddWriteEvent(0, 10, 100, nullptr) == SceKernelErrorEbadf);
    Require(sceKernelDeleteWriteEvent(0, 10) == SceKernelErrorEbadf);

    Require(sceKernelAddReadEvent(eq, -1, 100, nullptr) == SceKernelErrorEbadf);
    Require(sceKernelDeleteReadEvent(eq, -1) == SceKernelErrorEbadf);
    Require(sceKernelAddWriteEvent(eq, -1, 100, nullptr) == SceKernelErrorEbadf);
    Require(sceKernelDeleteWriteEvent(eq, -1) == SceKernelErrorEbadf);

    Require(sceKernelAddUserEvent(0, 50) == SceKernelErrorEbadf);
    Require(sceKernelAddUserEvent(eq, 50) == 0);
    Require(sceKernelAddUserEvent(eq, 50) == SceKernelErrorEexist);

    int triggerData = 999;
    Require(sceKernelTriggerUserEvent(0, 50, &triggerData) == SceKernelErrorEbadf);
    Require(sceKernelTriggerUserEvent(eq, 50, &triggerData) == 0);

    outCount = -1;
    Require(sceKernelWaitEqueue(eq, events, 2, &outCount, &timo) == 0);
    Require(outCount == 1);
    Require(sceKernelGetEventId(&events[0]) == 50);
    Require(sceKernelGetEventUserData(&events[0]) == &triggerData);

    Require(sceKernelDeleteUserEvent(0, 50) == SceKernelErrorEbadf);
    Require(sceKernelDeleteUserEvent(eq, 50) == 0);
    Require(sceKernelDeleteUserEvent(eq, 50) == SceKernelErrorEnoent);

    Require(sceKernelDeleteEqueue(eq) == 0);
    Require(sceKernelDeleteEqueue(eq) == SceKernelErrorEbadf);
    Require(sceKernelWaitEqueue(eq, events, 2, &outCount, &timo) == SceKernelErrorEbadf);
}

void TestSocketPollEventBounds() {
    const int sock = socket_nid_postfix(2, 1, 0);
    if (sock >= 0) {
        GuestPollDescriptor pfd{};
        pfd.descriptor = sock;
        pfd.events = PollOut;
        pfd.revents = 0;

        const int pollRes = poll_nid_postfix(&pfd, 1, 0);
        Require(pollRes >= 0);

        Require(close_nid_postfix(sock) == 0);

        pfd.revents = 0;
        Require(poll_nid_postfix(&pfd, 1, 0) == 1);
        Require(pfd.revents == PollNval);
    }
}

} // namespace

int main() {
    TestPollParameterBounds();
    TestPollDescriptorEventFlagsAndMaskBounds();
    TestPipePollEventTransitionsAndHangup();
    TestMixedPollArrayBounds();
    TestKernelEventAccessorsAndBounds();
    TestSceKernelEqueueEventBounds();
    TestSocketPollEventBounds();
    return 0;
}
