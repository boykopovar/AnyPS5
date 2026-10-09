#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

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

static void Require(bool value) { if (!value) std::abort(); }

constexpr int Unix = 1;
constexpr int Stream = 1;
constexpr int Datagram = 2;
constexpr int SequencedPacket = 5;
constexpr int CloseOnExec = 0x10000000;
constexpr int NonBlocking = 0x20000000;
constexpr int NoSignal = 0x20000;
constexpr int Peek = 2;
constexpr int FirstSocket = 0x10000000;
constexpr short In = 0x1;
constexpr short Out = 0x4;
constexpr short Hup = 0x10;

static bool Fails(int domain, int type, int protocol, int error) {
    int pair[2] = {-7, -7};
    *__error_nid_postfix() = 0;
    const int result = socketpair_nid_postfix(domain, type, protocol, pair);
    return result == -1 && *__error_nid_postfix() == error && pair[0] == -7 && pair[1] == -7;
}

static bool Readable(int descriptor, short accepted = In) {
    PollDescriptor waited{descriptor, In, 0};
    return poll_nid_postfix(&waited, 1, 2000) == 1 && (waited.revents & accepted) != 0;
}

static bool Exchange(int from, int to, const char* text) {
    char received[32]{};
    const auto length = static_cast<std::uint64_t>(std::strlen(text) + 1);
    if (send_nid_postfix(from, text, length, NoSignal) != static_cast<std::int64_t>(length)) return false;
    if (!Readable(to)) return false;
    if (recv_nid_postfix(to, received, sizeof(received), 0) != static_cast<std::int64_t>(length)) return false;
    return std::strcmp(received, text) == 0;
}

static bool Unnamed(int descriptor, int (APS5_VABI *query)(int, void*, std::uint32_t*)) {
    std::uint8_t name[28];
    std::memset(name, 0xaa, sizeof(name));
    std::uint32_t length = sizeof(name);
    if (query(descriptor, name, &length) != 0 || length != 16 || name[0] != 16 || name[1] != 1 || name[16] != 0xaa) return false;
    length = 4;
    std::memset(name, 0xaa, sizeof(name));
    return query(descriptor, name, &length) == 0 && length == 4 && name[0] == 16 && name[1] == 1 && name[4] == 0xaa;
}

int main() {
    int pair[2] = {-1, -1};
    Require(socketpair_nid_postfix(Unix, Stream, 0, pair) == 0);
    Require(pair[0] >= FirstSocket && pair[1] >= FirstSocket && pair[0] != pair[1]);
    Require(Exchange(pair[0], pair[1], "one way"));
    Require(Exchange(pair[1], pair[0], "other way"));
    Require(Unnamed(pair[0], getsockname_nid_postfix) && Unnamed(pair[1], getpeername_nid_postfix));

    PollDescriptor polled[2] = {{pair[1], In, 0}, {pair[0], Out, 0}};
    Require(poll_nid_postfix(polled, 2, 0) == 1 && polled[0].revents == 0 && (polled[1].revents & Out) != 0);
    const char ping[] = "ping";
    Require(send_nid_postfix(pair[0], ping, sizeof(ping), 0) == sizeof(ping));
    polled[0].revents = 0;
    Require(poll_nid_postfix(polled, 1, 1000) == 1 && (polled[0].revents & In) != 0);
    char peeked[8]{};
    Require(recv_nid_postfix(pair[1], peeked, sizeof(peeked), Peek) == sizeof(ping) && std::strcmp(peeked, ping) == 0);
    char received[8]{};
    std::uint8_t from[28];
    std::uint32_t fromLength = sizeof(from);
    Require(recvfrom_nid_postfix(pair[1], received, sizeof(received), 0, from, &fromLength) == sizeof(ping) && fromLength == 0);
    Require(std::strcmp(received, ping) == 0);

    Require(fcntl_nid_postfix(pair[1], 3) == 2);
    Require(fcntl_nid_postfix(pair[1], 4, 4) == 0);
    Require(fcntl_nid_postfix(pair[1], 3) == 6);
    Require(recv_nid_postfix(pair[1], received, sizeof(received), 0) == -1 && *__error_nid_postfix() == 35);

    Require(shutdown_nid_postfix(pair[0], 1) == 0);
    Require(Readable(pair[1], In | Hup) && recv_nid_postfix(pair[1], received, sizeof(received), 0) == 0);
    Require(Exchange(pair[1], pair[0], "still open"));
    Require(close_nid_postfix(pair[0]) == 0);
    Require(Readable(pair[1], In | Hup) && recv_nid_postfix(pair[1], received, sizeof(received), 0) == 0);
#ifndef _WIN32
    Require(send_nid_postfix(pair[1], ping, sizeof(ping), NoSignal) == -1 && *__error_nid_postfix() == 32);
#endif
    Require(close_nid_postfix(pair[1]) == 0);
    Require(close_nid_postfix(pair[1]) == -1 && *__error_nid_postfix() == 9);

    Require(socketpair_nid_postfix(Unix, Stream | NonBlocking | CloseOnExec, 0, pair) == 0);
    Require(fcntl_nid_postfix(pair[0], 3) == 6 && fcntl_nid_postfix(pair[1], 3) == 6);
    Require(recv_nid_postfix(pair[0], received, sizeof(received), 0) == -1 && *__error_nid_postfix() == 35);
    Require(Exchange(pair[1], pair[0], "wake up"));
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);

    Require(socketpair_nid_postfix(Unix, Datagram, 0, pair) == 0);
    Require(fcntl_nid_postfix(pair[0], 3) == 2);
    Require(send_nid_postfix(pair[0], "a", 1, 0) == 1);
    Require(send_nid_postfix(pair[0], "bb", 2, 0) == 2);
    char datagram[16]{};
    std::uint8_t sender[28];
    std::uint32_t senderLength = sizeof(sender);
    Require(Readable(pair[1]));
    Require(recvfrom_nid_postfix(pair[1], datagram, sizeof(datagram), 0, sender, &senderLength) == 1 && datagram[0] == 'a');
    Require(senderLength == 16 && sender[0] == 16 && sender[1] == 1);
    Require(recv_nid_postfix(pair[1], datagram, sizeof(datagram), 0) == 2 && datagram[0] == 'b' && datagram[1] == 'b');
    Require(Exchange(pair[1], pair[0], "back"));
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);

#ifndef _WIN32
    Require(socketpair_nid_postfix(Unix, SequencedPacket, 0, pair) == 0);
    Require(send_nid_postfix(pair[0], "a", 1, 0) == 1);
    Require(send_nid_postfix(pair[0], "bb", 2, 0) == 2);
    Require(recv_nid_postfix(pair[1], datagram, sizeof(datagram), 0) == 1);
    Require(recv_nid_postfix(pair[1], datagram, sizeof(datagram), 0) == 2);
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
#endif

    Require(Fails(2, Stream, 0, 45));
    Require(Fails(28, Datagram, 0, 45));
    Require(Fails(3, Stream, 0, 47));
    Require(Fails(Unix, 0, 0, 43));
    Require(Fails(Unix, 3, 0, 41));
    Require(Fails(Unix, 3, 6, 43));
    Require(Fails(Unix, Stream | 0x01000000, 0, 41));
    Require(Fails(Unix, Stream, 6, 43));
    *__error_nid_postfix() = 0;
    Require(socketpair_nid_postfix(Unix, Stream, 0, nullptr) == -1 && *__error_nid_postfix() == 14);
    return 0;
}
