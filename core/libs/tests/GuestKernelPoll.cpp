#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr short PollIn = 0x0001;
constexpr short PollPri = 0x0002;
constexpr short PollOut = 0x0004;
constexpr short PollErr = 0x0008;
constexpr short PollHup = 0x0010;
constexpr short PollNval = 0x0020;

constexpr int AfInet = 2;
constexpr int SockStream = 1;
constexpr int AfUnix = 1;

void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "assertion failed, errno %d\n", *__error_nid_postfix());
        std::abort();
    }
}

void AuditNegativeDescriptors() {
    GuestPollDescriptor neg{-1, PollIn | PollOut, 0x1234};
    Require(poll_nid_postfix(&neg, 1, 0) == 0 && neg.revents == 0);

    GuestPollDescriptor negZero{-1, 0, -1};
    Require(poll_nid_postfix(&negZero, 1, 0) == 0 && negZero.revents == 0);

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

} // namespace

int main() {
    AuditNegativeDescriptors();
    AuditClosedDescriptorsAndPollNval();
    AuditSocketPairEvents();
    return 0;
}
