#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;

constexpr short PollIn = 0x0001;
constexpr short PollPri = 0x0002;
constexpr short PollOut = 0x0004;
constexpr short PollErr = 0x0008;
constexpr short PollHup = 0x0010;
constexpr short PollNval = 0x0020;
constexpr short PollRdNorm = 0x0040;
constexpr short PollRdBand = 0x0080;
constexpr short PollWrBand = 0x0100;

constexpr int AfInet = 2;
constexpr int SockStream = 1;
constexpr int AfUnix = 1;
constexpr int ResourceNoFile = 8;

void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "assertion failed, errno %d\n", *__error_nid_postfix());
        std::abort();
    }
}

void AuditParameterBoundsAndPointerValidation() {
    Require(poll_nid_postfix(nullptr, 0, 0) == 0);
    Require(poll_nid_postfix(nullptr, 0, 5) == 0);

    *__error_nid_postfix() = 0;
    Require(poll_nid_postfix(nullptr, 1, 0) == -1 && *__error_nid_postfix() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(poll_nid_postfix(nullptr, 5, 0) == -1 && *__error_nid_postfix() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(poll_nid_postfix(reinterpret_cast<GuestPollDescriptor*>(std::uintptr_t{0x10}), 1, 0) == -1 && *__error_nid_postfix() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(poll_nid_postfix(reinterpret_cast<GuestPollDescriptor*>(std::uintptr_t{0x1000}), 4, 0) == -1 && *__error_nid_postfix() == GuestEfault);
}

void AuditTimeoutBounds() {
    GuestPollDescriptor dummy{-1, PollIn, 0};

    *__error_nid_postfix() = 0;
    Require(poll_nid_postfix(&dummy, 1, -2) == -1 && *__error_nid_postfix() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(poll_nid_postfix(&dummy, 1, -5) == -1 && *__error_nid_postfix() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(poll_nid_postfix(&dummy, 1, -100) == -1 && *__error_nid_postfix() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(poll_nid_postfix(nullptr, 0, -2) == -1 && *__error_nid_postfix() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(poll_nid_postfix(nullptr, 0, -50) == -1 && *__error_nid_postfix() == GuestEinval);

    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    GuestPollDescriptor r{pipeFds[0], PollIn, -1};
    Require(poll_nid_postfix(&r, 1, 0) == 0 && r.revents == 0);

    const auto start = std::chrono::steady_clock::now();
    Require(poll_nid_postfix(&r, 1, 20) == 0 && r.revents == 0);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    Require(elapsed >= std::chrono::milliseconds(15));

    GuestPollDescriptor w{pipeFds[1], PollOut, -1};
    Require(poll_nid_postfix(&w, 1, -1) == 1 && (w.revents & PollOut) != 0);

    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void AuditNfdsLimitsAndPrecedence() {
    GuestResourceLimit limit{};
    Require(getrlimit_nid_postfix(ResourceNoFile, &limit) == 0);

    GuestPollDescriptor single{-1, PollIn, 0};

    if (limit.rlim_max < 0xffffffff) {
        *__error_nid_postfix() = 0;
        Require(poll_nid_postfix(&single, 0xffffffffu, 0) == -1 && *__error_nid_postfix() == GuestEinval);

        *__error_nid_postfix() = 0;
        Require(poll_nid_postfix(nullptr, 0xffffffffu, 0) == -1 && *__error_nid_postfix() == GuestEinval);
    }

    if (limit.rlim_max >= 1024 && limit.rlim_max < 100000) {
        const auto overLimit = static_cast<std::uint32_t>(limit.rlim_max + 1);
        const auto atLimit = static_cast<std::uint32_t>(limit.rlim_max);

        *__error_nid_postfix() = 0;
        Require(poll_nid_postfix(&single, overLimit, 0) == -1 && *__error_nid_postfix() == GuestEinval);

        *__error_nid_postfix() = 0;
        Require(poll_nid_postfix(nullptr, overLimit, 0) == -1 && *__error_nid_postfix() == GuestEinval);

        *__error_nid_postfix() = 0;
        Require(poll_nid_postfix(reinterpret_cast<GuestPollDescriptor*>(std::uintptr_t{0x10}), overLimit, 0) == -1 && *__error_nid_postfix() == GuestEinval);

        *__error_nid_postfix() = 0;
        Require(poll_nid_postfix(nullptr, atLimit, 0) == -1 && *__error_nid_postfix() == GuestEfault);
    }
}

void AuditNegativeDescriptors() {
    GuestPollDescriptor neg{-1, PollIn | PollOut, 0x1234};
    Require(poll_nid_postfix(&neg, 1, 0) == 0 && neg.revents == 0);

    GuestPollDescriptor negMulti[4]{
        {-1, PollIn, -1},
        {-2, PollOut, -1},
        {-100, PollPri, -1},
        {-9999, PollIn | PollOut, -1}
    };
    Require(poll_nid_postfix(negMulti, 4, 0) == 0);
    for (int i = 0; i < 4; ++i) {
        Require(negMulti[i].revents == 0);
    }

    GuestPollDescriptor negWild{-1, static_cast<short>(0x7fff), -1};
    Require(poll_nid_postfix(&negWild, 1, 0) == 0 && negWild.revents == 0);

    int p[2]{-1, -1};
    Require(pipe_nid_postfix(p) == 0);

    GuestPollDescriptor mixed[3]{
        {-1, PollIn, 0x55},
        {p[1], PollOut, -1},
        {-50, PollOut, 0xaa}
    };
    Require(poll_nid_postfix(mixed, 3, 0) == 1);
    Require(mixed[0].revents == 0);
    Require((mixed[1].revents & PollOut) != 0);
    Require(mixed[2].revents == 0);

    Require(close_nid_postfix(p[0]) == 0);
    Require(close_nid_postfix(p[1]) == 0);
}

void AuditClosedDescriptorsAndPollNval() {
    const int sock = socket_nid_postfix(AfInet, SockStream, 0);
    Require(sock >= 0);
    Require(close_nid_postfix(sock) == 0);

    GuestPollDescriptor closedSock{sock, PollIn, 0};
    Require(poll_nid_postfix(&closedSock, 1, 0) == 1 && closedSock.revents == PollNval);

    GuestPollDescriptor closedSockNoEvents{sock, 0, 0};
    Require(poll_nid_postfix(&closedSockNoEvents, 1, 0) == 1 && closedSockNoEvents.revents == PollNval);

    GuestPollDescriptor closedSockOut{sock, PollOut, 0};
    Require(poll_nid_postfix(&closedSockOut, 1, 0) == 1 && closedSockOut.revents == PollNval);

    GuestPollDescriptor closedSockErr{sock, PollErr, 0};
    Require(poll_nid_postfix(&closedSockErr, 1, 0) == 1 && closedSockErr.revents == PollNval);

    GuestPollDescriptor closedSockHup{sock, PollHup, 0};
    Require(poll_nid_postfix(&closedSockHup, 1, 0) == 1 && closedSockHup.revents == PollNval);

    GuestPollDescriptor unopened{998, PollIn, 0};
    Require(poll_nid_postfix(&unopened, 1, 0) == 1 && unopened.revents == PollNval);

    GuestPollDescriptor closedBatch[3]{
        {991, PollIn, 0},
        {992, PollOut, 0},
        {993, 0, 0}
    };
    Require(poll_nid_postfix(closedBatch, 3, 0) == 3);
    for (int i = 0; i < 3; ++i) {
        Require(closedBatch[i].revents == PollNval);
    }

    GuestPollDescriptor mixedClosed[3]{
        {-1, PollIn, 0},
        {994, PollIn, 0},
        {-10, PollOut, 0}
    };
    Require(poll_nid_postfix(mixedClosed, 3, 0) == 1);
    Require(mixedClosed[0].revents == 0 && mixedClosed[1].revents == PollNval && mixedClosed[2].revents == 0);
}

void AuditPollEventsBitmaskAndPipeLifecycle() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    GuestPollDescriptor r{pipeFds[0], PollIn | PollRdNorm, -1};
    GuestPollDescriptor w{pipeFds[1], PollOut, -1};

    Require(poll_nid_postfix(&r, 1, 0) == 0 && r.revents == 0);
    Require(poll_nid_postfix(&w, 1, 0) == 1 && (w.revents & PollOut) != 0);

    char data[] = "poll";
    Require(write_nid_postfix(pipeFds[1], data, 4) == 4);

    Require(poll_nid_postfix(&r, 1, 0) == 1 && (r.revents & (PollIn | PollRdNorm)) != 0);

    Require(close_nid_postfix(pipeFds[1]) == 0);

    Require(poll_nid_postfix(&r, 1, 0) == 1 && (r.revents & PollIn) != 0);

    char buf[8]{};
    Require(read_nid_postfix(pipeFds[0], buf, sizeof(buf)) == 4);

    Require(poll_nid_postfix(&r, 1, 0) == 1 && (r.revents & PollHup) != 0);

    Require(close_nid_postfix(pipeFds[0]) == 0);

    Require(poll_nid_postfix(&r, 1, 0) == 1 && r.revents == PollNval);
    Require(poll_nid_postfix(&w, 1, 0) == 1 && w.revents == PollNval);
}

void AuditSocketPairEvents() {
    int pair[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);

    GuestPollDescriptor sp[2]{
        {pair[0], PollOut, -1},
        {pair[1], PollIn, -1}
    };
    Require(poll_nid_postfix(sp, 2, 0) == 1 && (sp[0].revents & PollOut) != 0 && sp[1].revents == 0);

    Require(send_nid_postfix(pair[0], "xyz", 3, 0) == 3);
    Require(poll_nid_postfix(&sp[1], 1, 0) == 1 && (sp[1].revents & PollIn) != 0);

    char drain[8]{};
    Require(recv_nid_postfix(pair[1], drain, sizeof(drain), 0) == 3);

    Require(close_nid_postfix(pair[0]) == 0);
    Require(close_nid_postfix(pair[1]) == 0);

    GuestPollDescriptor closedSp[2]{
        {pair[0], PollIn, 0},
        {pair[1], PollOut, 0}
    };
    Require(poll_nid_postfix(closedSp, 2, 0) == 2);
    Require(closedSp[0].revents == PollNval && closedSp[1].revents == PollNval);
}

void AuditUnsupportedEventFlagsRejection() {
    int p[2]{-1, -1};
    Require(pipe_nid_postfix(p) == 0);

    GuestPollDescriptor invalidEntry{p[0], static_cast<short>(PollIn | 0x0800), 0};
    bool threw = false;
    try {
        poll_nid_postfix(&invalidEntry, 1, 0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    GuestPollDescriptor validAcceptedFlags{p[0], static_cast<short>(PollIn | PollPri | PollOut | PollErr | PollHup | PollNval | PollRdNorm | PollRdBand | PollWrBand), 0};
    Require(poll_nid_postfix(&validAcceptedFlags, 1, 0) == 0 && validAcceptedFlags.revents == 0);

    GuestPollDescriptor invalidHighBit{p[0], static_cast<short>(PollIn | 0x0200), 0};
    bool threwHigh = false;
    try {
        poll_nid_postfix(&invalidHighBit, 1, 0);
    } catch (const std::runtime_error&) {
        threwHigh = true;
    }
    Require(threwHigh);

    Require(close_nid_postfix(p[0]) == 0);
    Require(close_nid_postfix(p[1]) == 0);
}

} // namespace

int main() {
    AuditParameterBoundsAndPointerValidation();
    AuditTimeoutBounds();
    AuditNfdsLimitsAndPrecedence();
    AuditNegativeDescriptors();
    AuditClosedDescriptorsAndPollNval();
    AuditPollEventsBitmaskAndPipeLifecycle();
    AuditSocketPairEvents();
    AuditUnsupportedEventFlagsRejection();
    return 0;
}
