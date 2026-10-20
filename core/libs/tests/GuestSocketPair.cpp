#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }

template<typename TFunction>
static bool ThrowsRuntimeError(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

constexpr int Unix = 1;
constexpr int Stream = 1;
constexpr int Datagram = 2;
constexpr int SequencedPacket = 5;
constexpr int CloseOnExec = 0x10000000;
constexpr int NonBlocking = 0x20000000;
constexpr int NoSignal = 0x20000;
constexpr int Peek = 2;
constexpr int DescriptorSetSize = 1024;
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
    GuestPollDescriptor waited{descriptor, In, 0};
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

static bool ReceiveFaults(int descriptor, GuestMsghdr* message) {
    *__error_nid_postfix() = 0;
    return recvmsg_nid_postfix(descriptor, message, 0) == -1 && *__error_nid_postfix() == 14;
}

static bool Queue(int descriptor, int receiver, const char* text) {
    const auto length = static_cast<std::int64_t>(std::strlen(text));
    return send_nid_postfix(descriptor, text, static_cast<std::uint64_t>(length), 0) == length && Readable(receiver);
}

static void CheckReceiveMessageFaults() {
    constexpr std::size_t page = 0x4000;
    void* memory = nullptr;
    Require(sceKernelMapFlexibleMemory(&memory, page * 3, 3, 0) == 0 && memory != nullptr);
    auto* writable = static_cast<char*>(memory);
    auto* readOnly = writable + page;
    auto* inaccessible = writable + 2 * page;
    char data[16]{};
    KernelIovec good{data, sizeof(data)};
    auto* readOnlyMessage = reinterpret_cast<GuestMsghdr*>(readOnly);
    *readOnlyMessage = {nullptr, 0, &good, 1, nullptr, 0, -1};
    auto* readOnlyVector = reinterpret_cast<KernelIovec*>(readOnly + 64);
    *readOnlyVector = {data, sizeof(data)};
    Require(sceKernelMprotect(readOnly, page, 1) == 0 && sceKernelMprotect(inaccessible, page, 0) == 0);
    int pair[2] = {-1, -1};
    Require(socketpair_nid_postfix(Unix, Datagram, 0, pair) == 0);

    Require(Queue(pair[0], pair[1], "x"));
    Require(ReceiveFaults(pair[1], reinterpret_cast<GuestMsghdr*>(0x1000)));
    Require(ReceiveFaults(pair[1], reinterpret_cast<GuestMsghdr*>(inaccessible)));
    GuestMsghdr message{nullptr, 0, reinterpret_cast<KernelIovec*>(0x1000), 1, nullptr, 0, -1};
    Require(ReceiveFaults(pair[1], &message));
    message.iov = reinterpret_cast<KernelIovec*>(inaccessible);
    Require(ReceiveFaults(pair[1], &message));
    char peeked[4]{};
    Require(recv_nid_postfix(pair[1], peeked, sizeof(peeked), Peek) == 1 && peeked[0] == 'x');

    KernelIovec vector{reinterpret_cast<void*>(0x1000), sizeof(data)};
    message = {nullptr, 0, &vector, 1, nullptr, 0, -1};
    Require(ReceiveFaults(pair[1], &message));
    vector.base = inaccessible;
    Require(Queue(pair[0], pair[1], "x") && ReceiveFaults(pair[1], &message));
    vector.base = readOnly + 128;
    Require(Queue(pair[0], pair[1], "x") && ReceiveFaults(pair[1], &message));
    message = {reinterpret_cast<void*>(0x1000), 16, &good, 1, nullptr, 0, -1};
    Require(Queue(pair[0], pair[1], "x") && ReceiveFaults(pair[1], &message));
    message.name = readOnly + 128;
    Require(Queue(pair[0], pair[1], "x") && ReceiveFaults(pair[1], &message));
    Require(Queue(pair[0], pair[1], "x") && ReceiveFaults(pair[1], readOnlyMessage));
    Require(fcntl_nid_postfix(pair[1], 4, 4) == 0);
    Require(recv_nid_postfix(pair[1], peeked, sizeof(peeked), 0) == -1 && *__error_nid_postfix() == 35);
    Require(fcntl_nid_postfix(pair[1], 4, 0) == 0);

    std::uint8_t name[32];
    std::memset(name, 0xa5, sizeof(name));
    vector = {writable, 1u << 20};
    message = {name, UINT32_MAX, &vector, 1, nullptr, 7, -1};
    Require(Queue(pair[0], pair[1], "abcd") && recvmsg_nid_postfix(pair[1], &message, 0) == 4);
    Require(std::memcmp(writable, "abcd", 4) == 0 && message.flags == 0 && message.controlLength == 0);
    Require(message.nameLength == 16 && name[0] == 16 && name[1] == Unix && name[16] == 0xa5);
    KernelIovec vectors[2]{{writable, 2}, {inaccessible, 16}};
    message = {nullptr, 0, vectors, 2, nullptr, 0, -1};
    Require(Queue(pair[0], pair[1], "ef") && recvmsg_nid_postfix(pair[1], &message, 0) == 2 && std::memcmp(writable, "ef", 2) == 0);
    message = {readOnly + 128, 0, readOnlyVector, 1, nullptr, 0, -1};
    Require(Queue(pair[0], pair[1], "gh") && recvmsg_nid_postfix(pair[1], &message, 0) == 2);
    Require(std::memcmp(data, "gh", 2) == 0 && message.nameLength == 0);

    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
    Require(sceKernelMprotect(memory, page * 3, 3) == 0 && sceKernelMunmap(reinterpret_cast<std::uint64_t>(memory), page * 3) == 0);
}

static bool SendFaults(int descriptor, const GuestMsghdr* message) {
    *__error_nid_postfix() = 0;
    return sendmsg_nid_postfix(descriptor, message, 0) == -1 && *__error_nid_postfix() == 14;
}

static void CheckSendMessageFaults() {
    constexpr std::size_t page = 0x4000;
    void* memory = nullptr;
    Require(sceKernelMapFlexibleMemory(&memory, page * 3, 3, 0) == 0 && memory != nullptr);
    auto* writable = static_cast<char*>(memory);
    auto* readOnly = writable + page;
    auto* inaccessible = writable + 2 * page;
    std::memcpy(readOnly, "ro", 2);
    auto* readOnlyVector = reinterpret_cast<KernelIovec*>(readOnly + 64);
    *readOnlyVector = {readOnly, 2};
    auto* readOnlyMessage = reinterpret_cast<GuestMsghdr*>(readOnly + 128);
    *readOnlyMessage = {nullptr, 0, readOnlyVector, 1, nullptr, 0, 0};
    char data[4] = {'a', 'b', 'c', 'd'};
    KernelIovec good{data, sizeof(data)};
    Require(sceKernelMprotect(readOnly, page, 1) == 0 && sceKernelMprotect(inaccessible, page, 0) == 0);
    int pair[2] = {-1, -1};
    Require(socketpair_nid_postfix(Unix, Datagram, 0, pair) == 0);

    Require(SendFaults(pair[0], reinterpret_cast<GuestMsghdr*>(0x1000)));
    Require(SendFaults(pair[0], reinterpret_cast<GuestMsghdr*>(inaccessible)));
    GuestMsghdr message{nullptr, 0, reinterpret_cast<KernelIovec*>(0x1000), 1, nullptr, 0, 0};
    Require(SendFaults(pair[0], &message));
    message.iov = reinterpret_cast<KernelIovec*>(inaccessible);
    Require(SendFaults(pair[0], &message));
    KernelIovec vector{reinterpret_cast<void*>(0x1000), sizeof(data)};
    message.iov = &vector;
    Require(SendFaults(pair[0], &message));
    vector.base = inaccessible;
    Require(SendFaults(pair[0], &message));
    KernelIovec second[2] = {good, {inaccessible, 2}};
    message.iov = second;
    message.iovLength = 2;
    Require(SendFaults(pair[0], &message));
    message = {reinterpret_cast<void*>(0x1000), 16, &good, 1, nullptr, 0, 0};
    Require(SendFaults(pair[0], &message));
    message = {inaccessible, 16, &good, 1, nullptr, 0, 0};
    Require(SendFaults(pair[0], &message));
    message = {nullptr, 0, &vector, 1, nullptr, 0, 0};
    *__error_nid_postfix() = 0;
    Require(sendmsg_nid_postfix(-1, &message, 0) == -1 && *__error_nid_postfix() == 9);
    char peeked[4]{};
    Require(fcntl_nid_postfix(pair[1], 4, 4) == 0);
    *__error_nid_postfix() = 0;
    Require(recv_nid_postfix(pair[1], peeked, sizeof(peeked), 0) == -1 && *__error_nid_postfix() == 35);
    Require(fcntl_nid_postfix(pair[1], 4, 0) == 0);

    Require(sendmsg_nid_postfix(pair[0], readOnlyMessage, 0) == 2);
    Require(recv_nid_postfix(pair[1], peeked, sizeof(peeked), 0) == 2 && std::memcmp(peeked, "ro", 2) == 0);
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
    Require(sceKernelMprotect(memory, page * 3, 3) == 0 && sceKernelMunmap(reinterpret_cast<std::uint64_t>(memory), page * 3) == 0);
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "sendmsg-memory") == 0) {
        CheckSendMessageFaults();
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "recvmsg-memory") == 0) {
        CheckReceiveMessageFaults();
        return 0;
    }
    Require(argc == 1);
    int pair[2] = {-1, -1};
    Require(socketpair_nid_postfix(Unix, Stream, 0, pair) == 0);
    Require(pair[0] >= 0 && pair[0] < DescriptorSetSize && pair[1] >= 0 && pair[1] < DescriptorSetSize && pair[0] != pair[1]);
    Require(Exchange(pair[0], pair[1], "one way"));
    Require(Exchange(pair[1], pair[0], "other way"));
    Require(Unnamed(pair[0], getsockname_nid_postfix) && Unnamed(pair[1], getpeername_nid_postfix));

    GuestPollDescriptor polled[2] = {{pair[1], In, 0}, {pair[0], Out, 0}};
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
    Require(fcntl_nid_postfix(pair[0], 1) == 0 && fcntl_nid_postfix(pair[1], 1) == 0);
    Require(fcntl_nid_postfix(pair[1], 4, 4) == 0);
    Require(fcntl_nid_postfix(pair[1], 3) == 6);
    Require(recv_nid_postfix(pair[1], received, sizeof(received), 0) == -1 && *__error_nid_postfix() == 35);

    Require(shutdown_nid_postfix(pair[0], 1) == 0);
    Require(Readable(pair[1], In | Hup) && recv_nid_postfix(pair[1], received, sizeof(received), 0) == 0);
    Require(Exchange(pair[1], pair[0], "still open"));
    Require(close_nid_postfix(pair[0]) == 0);
    Require(Readable(pair[1], In | Hup) && recv_nid_postfix(pair[1], received, sizeof(received), 0) == 0);
    std::int64_t sent = 0;
    for (int i = 0; i < 100 && sent >= 0; ++i) sent = send_nid_postfix(pair[1], ping, sizeof(ping), NoSignal);
    Require(sent == -1 && *__error_nid_postfix() == 32);
    Require(close_nid_postfix(pair[1]) == 0);
    Require(close_nid_postfix(pair[1]) == -1 && *__error_nid_postfix() == 9);

    Require(socketpair_nid_postfix(Unix, Stream | NonBlocking | CloseOnExec, 0, pair) == 0);
    Require(fcntl_nid_postfix(pair[0], 3) == 6 && fcntl_nid_postfix(pair[1], 3) == 6);
    Require(fcntl_nid_postfix(pair[0], 1) == 1 && fcntl_nid_postfix(pair[1], 1) == 1);
    Require(recv_nid_postfix(pair[0], received, sizeof(received), 0) == -1 && *__error_nid_postfix() == 35);
    Require(Exchange(pair[1], pair[0], "wake up"));
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);

    Require(socketpair_nid_postfix(Unix, Stream, 0, pair) == 0);
    FileStat status{};
    Require(fstat_nid_disambig1_nid_postfix(pair[0], &status) == 0);
    Require(status.st_mode == 0140666 && status.st_size == 0 && status.st_blksize != 0 && status.st_nlink == 0);
    Require(send_nid_postfix(pair[0], "hello", 5, 0) == 5 && Readable(pair[1]));
    Require(sceKernelFstat(pair[1], &status) == 0 && status.st_mode == 0140666 && status.st_size == 5);
    Require(shutdown_nid_postfix(pair[0], 1) == 0);
    Require(fstat_nid_disambig1_nid_postfix(pair[0], &status) == 0 && status.st_mode == 0140444);
#ifndef _WIN32
    Require(shutdown_nid_postfix(pair[1], 0) == 0);
    Require(fstat_nid_disambig1_nid_postfix(pair[1], &status) == 0 && status.st_mode == 0140666 && status.st_size == 5);
    Require(recv_nid_postfix(pair[1], received, sizeof(received), 0) == 5);
#else
    Require(recv_nid_postfix(pair[1], received, sizeof(received), 0) == 5);
    Require(shutdown_nid_postfix(pair[1], 0) == 0);
#endif
    Require(fstat_nid_disambig1_nid_postfix(pair[1], &status) == 0 && status.st_mode == 0140222 && status.st_size == 0);
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
    *__error_nid_postfix() = 0;
    Require(fstat_nid_disambig1_nid_postfix(pair[0], &status) == -1 && *__error_nid_postfix() == 9);
    Require(sceKernelFstat(pair[1], &status) == static_cast<int>(0x80020009u));

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

#ifdef __APPLE__
    Require(ThrowsRuntimeError([&] { socketpair_nid_postfix(Unix, SequencedPacket, 0, pair); }));
#elif !defined(_WIN32)
    Require(socketpair_nid_postfix(Unix, SequencedPacket, 0, pair) == 0);
    Require(send_nid_postfix(pair[0], "a", 1, 0) == 1);
    Require(send_nid_postfix(pair[0], "bb", 2, 0) == 2);
    Require(recv_nid_postfix(pair[1], datagram, sizeof(datagram), 0) == 1);
    Require(recv_nid_postfix(pair[1], datagram, sizeof(datagram), 0) == 2);
    Require(send_nid_postfix(pair[0], "record", 6, 0) == 6);
    Require(send_nid_postfix(pair[0], "next", 4, 0) == 4);
    char part[2]{};
    KernelIovec piece{part, sizeof(part)};
    GuestMsghdr message{nullptr, 0, &piece, 1, nullptr, 0, -1};
    const auto truncated = recvmsg_nid_postfix(pair[1], &message, 0);
    if (truncated != 2 || message.flags != 0x10) {
        std::fprintf(stderr, "recvmsg of a 6-byte SOCK_SEQPACKET record into 2 bytes returned %lld with msg_flags 0x%x, expected 2 with MSG_TRUNC (0x10)\n",
            static_cast<long long>(truncated), static_cast<unsigned>(message.flags));
        std::abort();
    }
    Require(std::memcmp(part, "re", 2) == 0);
    char whole[8]{};
    piece = {whole, sizeof(whole)};
    message.flags = -1;
    Require(recvmsg_nid_postfix(pair[1], &message, 0) == 4 && message.flags == 0 && std::memcmp(whole, "next", 4) == 0);
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
#endif
    CheckReceiveMessageFaults();

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
