#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <new>
#include <set>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }

#ifndef _WIN32
static std::atomic<std::size_t> lastAllocationSize{0};
static std::atomic<std::size_t> failAllocationOfSize{0};
static std::atomic<int> failAllocationAfter{0};
void* operator new(std::size_t bytes) {
    lastAllocationSize = bytes;
    if (bytes != 0 && failAllocationOfSize.load() == bytes && --failAllocationAfter == 0) {
        failAllocationOfSize = 0;
        throw std::bad_alloc();
    }
    if (void* pointer = std::malloc(bytes != 0 ? bytes : 1)) return pointer;
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
#endif

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

static bool NotConnected(int descriptor) {
    std::uint8_t name[16];
    std::memset(name, 0xaa, sizeof(name));
    std::uint32_t length = sizeof(name);
    *__error_nid_postfix() = 0;
    return getpeername_nid_postfix(descriptor, name, &length) == -1 && *__error_nid_postfix() == 57 && length == sizeof(name) && name[0] == 0xaa;
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

int main(int argc, char** argv) {
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
    Require(Unnamed(pair[0], getpeername_nid_postfix) && Unnamed(pair[1], getpeername_nid_postfix));
    Require(close_nid_postfix(pair[0]) == 0);
    Require(NotConnected(pair[1]) && Unnamed(pair[1], getsockname_nid_postfix));
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
    Require(status.st_dev == 0xFFFFFFFFu && status.st_ino != 0);
    const auto firstInode = status.st_ino;
    Require(sceKernelFstat(pair[0], &status) == 0 && status.st_dev == 0xFFFFFFFFu && status.st_ino == firstInode);
    Require(fstat_nid_disambig1_nid_postfix(pair[1], &status) == 0 && status.st_dev == 0xFFFFFFFFu);
    Require(status.st_ino != 0 && status.st_ino != firstInode);
    const auto secondInode = status.st_ino;
    const int duplicate = dup_nid_postfix(pair[1]);
    Require(duplicate >= 0 && fstat_nid_disambig1_nid_postfix(duplicate, &status) == 0 && status.st_ino == secondInode);
    Require(close_nid_postfix(duplicate) == 0);
    Require(fstat_nid_disambig1_nid_postfix(pair[1], &status) == 0 && status.st_ino == secondInode);
    const int inet = socket_nid_postfix(2, Stream, 0);
    Require(inet >= 0 && fstat_nid_disambig1_nid_postfix(inet, &status) == 0 && status.st_dev == 0 && status.st_ino == 0);
    Require(close_nid_postfix(inet) == 0);
    Require(send_nid_postfix(pair[0], "hello", 5, 0) == 5 && Readable(pair[1]));
    Require(sceKernelFstat(pair[1], &status) == 0 && status.st_mode == 0140666 && status.st_size == 5);
    Require(shutdown_nid_postfix(pair[0], 1) == 0);
    Require(fstat_nid_disambig1_nid_postfix(pair[0], &status) == 0 && status.st_mode == 0140444);
    Require(shutdown_nid_postfix(pair[1], 0) == 0);
    Require(fstat_nid_disambig1_nid_postfix(pair[1], &status) == 0 && status.st_mode == 0140222 && status.st_size == 0);
    Require(recv_nid_postfix(pair[1], received, sizeof(received), 0) == 0);
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
    const int copy = dup_nid_postfix(pair[0]);
    Require(copy >= 0 && close_nid_postfix(pair[0]) == 0);
    Require(Unnamed(pair[1], getpeername_nid_postfix) && Exchange(pair[1], copy, "copy"));
    Require(close_nid_postfix(copy) == 0 && NotConnected(pair[1]));
    Require(close_nid_postfix(pair[1]) == 0);

    Require(socketpair_nid_postfix(Unix, Datagram, 0, pair) == 0);
    Require(send_nid_postfix(pair[0], "c", 1, 0) == 1 && Readable(pair[1]));
    Require(shutdown_nid_postfix(pair[1], 0) == 0);
    KernelIovec slot{datagram, sizeof(datagram)};
    GuestMsghdr shut{sender, static_cast<std::uint32_t>(sizeof(sender)), &slot, 1, nullptr, 0, -1};
    Require(recvmsg_nid_postfix(pair[1], &shut, 0) == 0 && shut.nameLength == 0);
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
    for (int type : {Stream, Datagram}) {
        *__error_nid_postfix() = 0;
        Require(socketpair_nid_postfix(Unix, type, 0, reinterpret_cast<int*>(0x10)) == -1 && *__error_nid_postfix() == 14);
    }
    constexpr std::size_t page = 0x4000;
    void* memory = nullptr;
    Require(sceKernelMapFlexibleMemory(&memory, page * 2, 3, 0) == 0 && memory != nullptr);
    auto* base = static_cast<char*>(memory);
    Require(sceKernelMprotect(base + page, page, 0) == 0);
    for (auto* target : {base + page, base + page - 4}) {
        *__error_nid_postfix() = 0;
        Require(socketpair_nid_postfix(Unix, Stream, 0, reinterpret_cast<int*>(target)) == -1 && *__error_nid_postfix() == 14);
    }
    Require(sceKernelMprotect(memory, page * 2, 3) == 0 && sceKernelMunmap(reinterpret_cast<std::uint64_t>(memory), page * 2) == 0);
    int first[2] = {-1, -1};
    Require(socketpair_nid_postfix(Unix, Stream, 0, first) == 0);
    Require(close_nid_postfix(first[0]) == 0 && close_nid_postfix(first[1]) == 0);
    Require(socketpair_nid_postfix(Unix, Stream | CloseOnExec, 0, reinterpret_cast<int*>(0x10)) == -1);
    int second[2] = {-1, -1};
    Require(socketpair_nid_postfix(Unix, Stream, 0, second) == 0 && second[0] == first[0] && second[1] == first[1]);
    Require(close_nid_postfix(second[0]) == 0 && close_nid_postfix(second[1]) == 0);
#ifndef _WIN32
    {
        std::set<int> probe;
        probe.insert(1);
    }
    const std::size_t registryNodeSize = lastAllocationSize;
    for (const int occurrence : {1, 2}) {
        int expected[2] = {-1, -1};
        Require(socketpair_nid_postfix(Unix, Stream | CloseOnExec, 0, expected) == 0);
        Require(close_nid_postfix(expected[0]) == 0 && close_nid_postfix(expected[1]) == 0);
        failAllocationAfter = occurrence;
        failAllocationOfSize = registryNodeSize;
        int refused[2] = {-1, -1};
        *__error_nid_postfix() = 0;
        const int result = socketpair_nid_postfix(Unix, Stream | CloseOnExec, 0, refused);
        failAllocationOfSize = 0;
        Require(result == -1 && *__error_nid_postfix() == 12);
        int plain[2] = {-1, -1};
        Require(socketpair_nid_postfix(Unix, Stream, 0, plain) == 0 && plain[0] == expected[0] && plain[1] == expected[1]);
        Require(fcntl_nid_postfix(plain[0], 1, 0) == 0 && fcntl_nid_postfix(plain[1], 1, 0) == 0);
        Require(close_nid_postfix(plain[0]) == 0 && close_nid_postfix(plain[1]) == 0);
        int again[2] = {-1, -1};
        Require(socketpair_nid_postfix(Unix, Stream | CloseOnExec, 0, again) == 0 && again[0] == expected[0] && again[1] == expected[1]);
        Require(fcntl_nid_postfix(again[0], 1, 0) == 1 && fcntl_nid_postfix(again[1], 1, 0) == 1);
        Require(close_nid_postfix(again[0]) == 0 && close_nid_postfix(again[1]) == 0);
        *__error_nid_postfix() = 0;
        Require(close_nid_postfix(again[0]) == -1 && *__error_nid_postfix() == 9);
    }
#endif
    return 0;
}
