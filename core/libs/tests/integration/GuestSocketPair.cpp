#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <cstring>
#include <string>

struct PollDescriptor {
    int descriptor;
    short events;
    short revents;
};

extern "C" {
int APS5_VABI socketpair_nid_postfix(int, int, int, int*);
std::int64_t APS5_VABI send_nid_postfix(int, const void*, std::uint64_t, int);
std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int);
std::int64_t APS5_VABI recvfrom_nid_postfix(int, void*, std::uint64_t, int, void*, std::uint32_t*);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI getpeername_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI shutdown_nid_postfix(int, int);
int APS5_VABI fcntl_nid_postfix(int, int, ...);
int APS5_VABI poll_nid_postfix(PollDescriptor*, std::uint32_t, int);
int APS5_VABI close_nid_postfix(int);
int* APS5_VABI __error_nid_postfix();
}

namespace {

using NameQuery = int (APS5_VABI *)(int, void*, std::uint32_t*);
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int unixDomain = 1;
constexpr int stream = 1;
constexpr int datagram = 2;
constexpr int sequencedPacket = 5;
constexpr int closeOnExec = 0x10000000;
constexpr int nonBlocking = 0x20000000;
constexpr int noSignal = 0x20000;
constexpr int peek = 2;
constexpr int firstSocket = 0x10000000;
constexpr short pollIn = 0x1;
constexpr short pollOut = 0x4;
constexpr short pollHup = 0x10;
constexpr int getFlags = 3;
constexpr int setFlags = 4;
constexpr int readWrite = 2;
constexpr int nonBlockingFlag = 4;
constexpr int eagain = 35;
constexpr int ebadf = 9;
constexpr int efault = 14;
#ifndef _WIN32
constexpr int epipe = 32;
#endif

int Errno() {
    return *__error_nid_postfix();
}

class SocketPair {
public:
    explicit SocketPair(int type) {
        RequireEqual(socketpair_nid_postfix(unixDomain, type, 0, descriptors), 0, "socketpair type " + std::to_string(type));
        open[0] = true;
        open[1] = true;
    }

    ~SocketPair() {
        for (int index = 0; index < 2; ++index) {
            if (open[index]) close_nid_postfix(descriptors[index]);
        }
    }

    SocketPair(const SocketPair&) = delete;
    SocketPair& operator=(const SocketPair&) = delete;

    int operator[](int index) const noexcept { return descriptors[index]; }

    int Close(int index) {
        open[index] = false;
        return close_nid_postfix(descriptors[index]);
    }

private:
    int descriptors[2] = {-1, -1};
    bool open[2] = {false, false};
};

bool Readable(int descriptor, short accepted = pollIn) {
    PollDescriptor waited{descriptor, pollIn, 0};
    return poll_nid_postfix(&waited, 1, 2000) == 1 && (waited.revents & accepted) != 0;
}

void RequireExchange(int from, int to, const char* text) {
    const std::string context = std::string("exchange \"") + text + "\"";
    char received[32]{};
    const auto length = static_cast<std::int64_t>(std::strlen(text) + 1);
    RequireEqual(send_nid_postfix(from, text, static_cast<std::uint64_t>(length), noSignal), length, context + " send");
    Require(Readable(to), context + " peer readable");
    RequireEqual(recv_nid_postfix(to, received, sizeof(received), 0), length, context + " recv");
    RequireEqual(std::string(received), std::string(text), context + " payload");
}

void RequireEof(int descriptor, const char* context) {
    char received[8]{};
    Require(Readable(descriptor, pollIn | pollHup), std::string(context) + " readable or hung up");
    RequireEqual(recv_nid_postfix(descriptor, received, sizeof(received), 0), std::int64_t{0}, std::string(context) + " recv");
}

void RequireUnnamed(int descriptor, NameQuery query, const char* queryName) {
    const std::string context = queryName;
    std::uint8_t name[28];
    std::memset(name, 0xaa, sizeof(name));
    std::uint32_t length = sizeof(name);
    RequireEqual(query(descriptor, name, &length), 0, context + " full buffer");
    RequireEqual(length, 16u, context + " full length");
    RequireEqual(name[0], std::uint8_t{16}, context + " sa_len");
    RequireEqual(name[1], std::uint8_t{1}, context + " sa_family");
    RequireEqual(name[16], std::uint8_t{0xaa}, context + " byte after address untouched");
    length = 4;
    std::memset(name, 0xaa, sizeof(name));
    RequireEqual(query(descriptor, name, &length), 0, context + " truncated buffer");
    RequireEqual(length, 4u, context + " truncated length");
    RequireEqual(name[0], std::uint8_t{16}, context + " truncated sa_len");
    RequireEqual(name[1], std::uint8_t{1}, context + " truncated sa_family");
    RequireEqual(name[4], std::uint8_t{0xaa}, context + " byte after truncated buffer untouched");
}

void RequireRecvFails(int descriptor, int error, const char* context) {
    char received[8]{};
    RequireEqual(recv_nid_postfix(descriptor, received, sizeof(received), 0), std::int64_t{-1}, std::string(context) + " recv");
    RequireEqual(Errno(), error, std::string(context) + " errno");
}

const Case streamDescriptors{"Socketpair_UnixStream_ReturnsTwoDistinctSocketDescriptors", [] {
    SocketPair pair(stream);
    Require(pair[0] >= firstSocket, "first descriptor " + std::to_string(pair[0]) + " in socket range");
    Require(pair[1] >= firstSocket, "second descriptor " + std::to_string(pair[1]) + " in socket range");
    Require(pair[0] != pair[1], "descriptors differ");
}};

const Case streamExchange{"Socketpair_UnixStream_ExchangesDataBothWays", [] {
    SocketPair pair(stream);
    RequireExchange(pair[0], pair[1], "one way");
    RequireExchange(pair[1], pair[0], "other way");
}};

const Case streamNames{"SocketName_UnixStreamPair_ReportsUnnamedUnixAddressAndTruncates", [] {
    SocketPair pair(stream);
    RequireUnnamed(pair[0], getsockname_nid_postfix, "getsockname");
    RequireUnnamed(pair[1], getpeername_nid_postfix, "getpeername");
}};

const Case pollIdle{"Poll_IdleStreamPair_ReportsWritableButNotReadable", [] {
    SocketPair pair(stream);
    PollDescriptor polled[2] = {{pair[1], pollIn, 0}, {pair[0], pollOut, 0}};
    RequireEqual(poll_nid_postfix(polled, 2, 0), 1, "ready descriptors");
    RequireEqual(polled[0].revents, short{0}, "read side revents");
    Require((polled[1].revents & pollOut) != 0, "write side reports POLLOUT");
}};

const Case pollAfterSend{"Poll_AfterPeerSends_ReportsReadable", [] {
    SocketPair pair(stream);
    const char ping[] = "ping";
    RequireEqual(send_nid_postfix(pair[0], ping, sizeof(ping), 0), std::int64_t{sizeof(ping)}, "send");
    PollDescriptor polled{pair[1], pollIn, 0};
    RequireEqual(poll_nid_postfix(&polled, 1, 1000), 1, "ready descriptors");
    Require((polled.revents & pollIn) != 0, "read side reports POLLIN");
}};

const Case peekThenRecvfrom{"Recv_PeekThenRecvfrom_ReturnsSameDataAndNoStreamAddress", [] {
    SocketPair pair(stream);
    const char ping[] = "ping";
    RequireEqual(send_nid_postfix(pair[0], ping, sizeof(ping), 0), std::int64_t{sizeof(ping)}, "send");
    Require(Readable(pair[1]), "peer readable");
    char peeked[8]{};
    RequireEqual(recv_nid_postfix(pair[1], peeked, sizeof(peeked), peek), std::int64_t{sizeof(ping)}, "peek length");
    RequireEqual(std::string(peeked), std::string(ping), "peeked payload");
    char received[8]{};
    std::uint8_t from[28];
    std::uint32_t fromLength = sizeof(from);
    RequireEqual(recvfrom_nid_postfix(pair[1], received, sizeof(received), 0, from, &fromLength), std::int64_t{sizeof(ping)},
                 "recvfrom length");
    RequireEqual(fromLength, 0u, "recvfrom address length");
    RequireEqual(std::string(received), std::string(ping), "received payload");
}};

const Case setNonBlocking{"Fcntl_SetNonBlocking_UpdatesFlagsAndEmptyRecvFailsWithEagain", [] {
    SocketPair pair(stream);
    RequireEqual(fcntl_nid_postfix(pair[1], getFlags), readWrite, "initial flags");
    RequireEqual(fcntl_nid_postfix(pair[1], setFlags, nonBlockingFlag), 0, "set non-blocking");
    RequireEqual(fcntl_nid_postfix(pair[1], getFlags), readWrite | nonBlockingFlag, "updated flags");
    RequireRecvFails(pair[1], eagain, "empty non-blocking");
}};

const Case shutdownWrite{"Shutdown_WriteSide_PeerReadsEofAndCanStillSend", [] {
    SocketPair pair(stream);
    RequireEqual(shutdown_nid_postfix(pair[0], 1), 0, "shutdown write side");
    RequireEof(pair[1], "after shutdown");
    RequireExchange(pair[1], pair[0], "still open");
}};

const Case closePeer{"Close_Peer_RemainingEndReadsEof", [] {
    SocketPair pair(stream);
    RequireEqual(pair.Close(0), 0, "close first end");
    RequireEof(pair[1], "after peer close");
}};

#ifndef _WIN32
const Case sendClosedPeer{"Send_PeerClosedWithNoSignal_FailsWithEpipe", [] {
    SocketPair pair(stream);
    RequireEqual(pair.Close(0), 0, "close first end");
    RequireEof(pair[1], "after peer close");
    const char ping[] = "ping";
    RequireEqual(send_nid_postfix(pair[1], ping, sizeof(ping), noSignal), std::int64_t{-1}, "send");
    RequireEqual(Errno(), epipe, "send errno");
}};
#endif

const Case closeTwice{"Close_AlreadyClosedSocket_FailsWithEbadf", [] {
    SocketPair pair(stream);
    RequireEqual(pair.Close(1), 0, "first close");
    RequireEqual(close_nid_postfix(pair[1]), -1, "second close");
    RequireEqual(Errno(), ebadf, "second close errno");
}};

const Case creationFlags{"Socketpair_NonBlockingAndCloseOnExecType_AppliesNonBlockingToBothEnds", [] {
    SocketPair pair(stream | nonBlocking | closeOnExec);
    RequireEqual(fcntl_nid_postfix(pair[0], getFlags), readWrite | nonBlockingFlag, "first end flags");
    RequireEqual(fcntl_nid_postfix(pair[1], getFlags), readWrite | nonBlockingFlag, "second end flags");
    RequireRecvFails(pair[0], eagain, "empty non-blocking");
    RequireExchange(pair[1], pair[0], "wake up");
}};

const Case datagramFlags{"Socketpair_UnixDatagram_IsBlockingReadWrite", [] {
    SocketPair pair(datagram);
    RequireEqual(fcntl_nid_postfix(pair[0], getFlags), readWrite, "flags");
}};

const Case datagramBoundaries{"Socketpair_UnixDatagram_PreservesBoundariesAndReportsUnnamedSender", [] {
    SocketPair pair(datagram);
    RequireEqual(send_nid_postfix(pair[0], "a", 1, 0), std::int64_t{1}, "send a");
    RequireEqual(send_nid_postfix(pair[0], "bb", 2, 0), std::int64_t{2}, "send bb");
    char received[16]{};
    std::uint8_t sender[28];
    std::uint32_t senderLength = sizeof(sender);
    Require(Readable(pair[1]), "peer readable");
    RequireEqual(recvfrom_nid_postfix(pair[1], received, sizeof(received), 0, sender, &senderLength), std::int64_t{1},
                 "first datagram length");
    RequireEqual(received[0], 'a', "first datagram payload");
    RequireEqual(senderLength, 16u, "sender length");
    RequireEqual(sender[0], std::uint8_t{16}, "sender sa_len");
    RequireEqual(sender[1], std::uint8_t{1}, "sender sa_family");
    RequireEqual(recv_nid_postfix(pair[1], received, sizeof(received), 0), std::int64_t{2}, "second datagram length");
    RequireEqual(received[0], 'b', "second datagram first byte");
    RequireEqual(received[1], 'b', "second datagram second byte");
}};

const Case datagramReply{"Socketpair_UnixDatagram_ExchangesDataBackToFirstEnd", [] {
    SocketPair pair(datagram);
    RequireExchange(pair[1], pair[0], "back");
}};

#ifndef _WIN32
const Case sequencedBoundaries{"Socketpair_UnixSequencedPacket_PreservesBoundaries", [] {
    SocketPair pair(sequencedPacket);
    char received[16]{};
    RequireEqual(send_nid_postfix(pair[0], "a", 1, 0), std::int64_t{1}, "send a");
    RequireEqual(send_nid_postfix(pair[0], "bb", 2, 0), std::int64_t{2}, "send bb");
    RequireEqual(recv_nid_postfix(pair[1], received, sizeof(received), 0), std::int64_t{1}, "first packet length");
    RequireEqual(recv_nid_postfix(pair[1], received, sizeof(received), 0), std::int64_t{2}, "second packet length");
}};
#endif

struct InvalidPair {
    int domain;
    int type;
    int protocol;
    int error;
};

const Case invalidArguments{"Socketpair_UnsupportedArguments_FailWithErrnoAndLeaveOutputUntouched", [] {
    const InvalidPair inputs[] = {
        {2, stream, 0, 45},
        {28, datagram, 0, 45},
        {3, stream, 0, 47},
        {unixDomain, 0, 0, 43},
        {unixDomain, 3, 0, 41},
        {unixDomain, 3, 6, 43},
        {unixDomain, stream | 0x01000000, 0, 41},
        {unixDomain, stream, 6, 43},
    };
    for (const auto& input : inputs) {
        const std::string context = "domain " + std::to_string(input.domain) + " type " + std::to_string(input.type) +
                                    " protocol " + std::to_string(input.protocol);
        int pair[2] = {-7, -7};
        *__error_nid_postfix() = 0;
        const int result = socketpair_nid_postfix(input.domain, input.type, input.protocol, pair);
        const int error = Errno();
        if (result == 0) {
            close_nid_postfix(pair[0]);
            close_nid_postfix(pair[1]);
        }
        RequireEqual(result, -1, context + " result");
        RequireEqual(error, input.error, context + " errno");
        RequireEqual(pair[0], -7, context + " first output");
        RequireEqual(pair[1], -7, context + " second output");
    }
}};

const Case nullOutput{"Socketpair_NullOutput_FailsWithEfault", [] {
    *__error_nid_postfix() = 0;
    RequireEqual(socketpair_nid_postfix(unixDomain, stream, 0, nullptr), -1, "result");
    RequireEqual(Errno(), efault, "errno");
}};

} // namespace
