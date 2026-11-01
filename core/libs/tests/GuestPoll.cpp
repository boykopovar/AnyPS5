#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket NativeInvalid = INVALID_SOCKET;
static void CloseNative(NativeSocket socket) { closesocket(socket); }
#else
using NativeSocket = int;
constexpr NativeSocket NativeInvalid = -1;
static void CloseNative(NativeSocket socket) { close(socket); }
#endif
static void Require(bool value) { if (!value) std::abort(); }
#ifndef _WIN32
static std::atomic<std::size_t> failAllocationOfSize{0};
void* operator new(std::size_t bytes) {
    if (bytes != 0 && failAllocationOfSize.load() == bytes) {
        failAllocationOfSize = 0;
        throw std::bad_alloc();
    }
    if (void* pointer = std::malloc(bytes != 0 ? bytes : 1)) return pointer;
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
#endif
constexpr short In = 0x1;
constexpr short Priority = 0x2;
constexpr short ReadBand = 0x80;
constexpr short Out = 0x4;
constexpr short Hup = 0x10;
constexpr short Invalid = 0x20;
constexpr short ReadNormal = 0x40;
constexpr short WriteBand = 0x100;
constexpr std::int16_t EvfiltUser = -11;
constexpr std::uint16_t EvAdd = 0x0001;
constexpr std::uint16_t EvClear = 0x0020;
constexpr std::uint32_t NoteTrigger = 0x01000000;
static KernelEvent UserEvent(std::uint16_t flags, std::uint32_t fflags) {
    KernelEvent change{};
    change.ident = 7;
    change.filter = EvfiltUser;
    change.flags = flags;
    change.fflags = fflags;
    return change;
}
static void PollsKqueues(int socket) {
    const int queue = kqueue_nid_postfix();
    Require(queue >= 0);
    GuestPollDescriptor kq{queue, static_cast<short>(In | ReadNormal | Out | Priority | ReadBand | WriteBand), -1};
    Require(poll_nid_postfix(&kq, 1, 0) == 0 && kq.revents == 0);
    const auto added = UserEvent(EvAdd | EvClear, 0);
    Require(kevent_nid_postfix(queue, &added, 1, nullptr, 0, nullptr) == 0);
    Require(poll_nid_postfix(&kq, 1, 0) == 0 && kq.revents == 0);
    const auto start = std::chrono::steady_clock::now();
    Require(poll_nid_postfix(&kq, 1, 30) == 0 && kq.revents == 0);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(25));
    std::thread trigger([queue] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        const auto triggered = UserEvent(0, NoteTrigger);
        Require(kevent_nid_postfix(queue, &triggered, 1, nullptr, 0, nullptr) == 0);
    });
    GuestPollDescriptor mixed[2]{{socket, In, -1}, {queue, static_cast<short>(In | Out), -1}};
    Require(poll_nid_postfix(mixed, 2, -1) == 1);
    trigger.join();
    Require(mixed[0].revents == 0 && mixed[1].revents == In);
    Require(poll_nid_postfix(&kq, 1, 0) == 1 && kq.revents == (In | ReadNormal));
    KernelEvent events[1]{};
    const KernelTimespec zero{0, 0};
    Require(kevent_nid_postfix(queue, nullptr, 0, events, 1, &zero) == 1);
    Require(poll_nid_postfix(&kq, 1, 0) == 0 && kq.revents == 0);
    Require(close_nid_postfix(queue) == 0);
    Require(poll_nid_postfix(&kq, 1, 0) == 1 && kq.revents == Invalid);
}
static void PollsFilesAndDirectories(int socket) {
    constexpr int ReadWrite = 0x0002, Create = 0x0200, DirectoryFlag = 0x00020000;
    const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string filePath = "anyps5-poll-file-" + stamp;
    const std::string directoryPath = "anyps5-poll-directory-" + stamp;
    const int file = open_nid_postfix(filePath.c_str(), ReadWrite | Create, 0644);
    Require(file >= 0);
    Require(mkdir_nid_postfix(directoryPath.c_str(), 0755) == 0);
    const int directory = open_nid_postfix(directoryPath.c_str(), DirectoryFlag, 0);
    Require(directory >= 0);
    GuestPollDescriptor vnode{file, static_cast<short>(In | Out | ReadNormal | Priority | ReadBand | WriteBand), -1};
    Require(poll_nid_postfix(&vnode, 1, 0) == 1 && vnode.revents == (In | Out | ReadNormal));
    vnode.events = static_cast<short>(Priority | ReadBand | WriteBand);
    Require(poll_nid_postfix(&vnode, 1, 0) == 0 && vnode.revents == 0);
    const auto start = std::chrono::steady_clock::now();
    Require(poll_nid_postfix(&vnode, 1, 30) == 0);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(25));
    GuestPollDescriptor folder{directory, static_cast<short>(In | Out), -1};
    Require(poll_nid_postfix(&folder, 1, -1) == 1 && folder.revents == (In | Out));
    GuestPollDescriptor mixed[3]{{socket, In, -1}, {file, Out, -1}, {directory, ReadNormal, -1}};
    Require(poll_nid_postfix(mixed, 3, -1) == 2);
    Require(mixed[0].revents == 0 && mixed[1].revents == Out && mixed[2].revents == ReadNormal);
    Require(close_nid_postfix(directory) == 0 && close_nid_postfix(file) == 0);
    Require(rmdir_nid_postfix(directoryPath.c_str()) == 0 && unlink_nid_postfix(filePath.c_str()) == 0);
}

static void StandardDescriptorFile(int descriptor) {
    const int saved = dup_nid_postfix(descriptor);
    const std::string path = "anyps5-poll-input-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const int file = open_nid_postfix(path.c_str(), 0x0002 | 0x0200, 0644);
    Require(file >= 0);
    Require(dup2_nid_postfix(file, descriptor) == descriptor);
    if (file != descriptor) Require(close_nid_postfix(file) == 0);
    GuestPollDescriptor input{descriptor, static_cast<short>(In | Out | ReadNormal | Priority), -1};
    const int ready = poll_nid_postfix(&input, 1, 0);
    if (ready != 1) std::fprintf(stderr, "poll on a regular file at descriptor %d returned %d instead of 1\n", descriptor, ready);
    Require(ready == 1 && input.revents == (In | Out | ReadNormal));
    input.events = Priority;
    Require(poll_nid_postfix(&input, 1, 0) == 0 && input.revents == 0);
    if (saved >= 0) {
        Require(dup2_nid_postfix(saved, descriptor) == descriptor);
        Require(close_nid_postfix(saved) == 0);
    } else Require(close_nid_postfix(descriptor) == 0);
    Require(unlink_nid_postfix(path.c_str()) == 0);
}

static void TestPipes(int socket) {
    int ends[2];
    Require(pipe_nid_postfix(ends) == 0);
    GuestPollDescriptor empty[2]{{ends[0], In, -1}, {ends[1], Out, -1}};
    Require(poll_nid_postfix(empty, 2, 0) == 1 && empty[0].revents == 0 && empty[1].revents == Out);
    GuestPollDescriptor reader{ends[0], In | ReadNormal, -1};
    const auto start = std::chrono::steady_clock::now();
    Require(poll_nid_postfix(&reader, 1, 50) == 0 && reader.revents == 0);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40));
    const auto shortWaitStart = std::chrono::steady_clock::now();
    for (int i = 0; i < 20; ++i) Require(poll_nid_postfix(&reader, 1, 2) == 0 && reader.revents == 0);
    const auto shortWait = std::chrono::steady_clock::now() - shortWaitStart;
    Require(shortWait >= std::chrono::milliseconds(40) && shortWait < std::chrono::milliseconds(200));
    Require(write_nid_postfix(ends[1], "wake", 4) == 4);
    Require(poll_nid_postfix(&reader, 1, 1000) == 1 && reader.revents == (In | ReadNormal));
    GuestPollDescriptor writer{ends[1], In, -1};
    Require(poll_nid_postfix(&writer, 1, 0) == 0 && writer.revents == 0);
    GuestPollDescriptor readerOut{ends[0], Out, -1};
    bool threw = false;
    try { poll_nid_postfix(&readerOut, 1, 0); } catch (const std::runtime_error&) { threw = true; }
    Require(threw);
    GuestPollDescriptor mixed[2]{{socket, In, -1}, {ends[0], In, -1}};
    Require(poll_nid_postfix(mixed, 2, 1000) == 1 && mixed[0].revents == 0 && mixed[1].revents == In);
    char buffer[8]{};
    Require(read_nid_postfix(ends[0], buffer, sizeof(buffer)) == 4);
    Require(poll_nid_postfix(mixed, 2, 0) == 0 && mixed[1].revents == 0);
    std::thread late([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        Require(write_nid_postfix(ends[1], "x", 1) == 1);
    });
    Require(poll_nid_postfix(mixed, 2, 2000) == 1 && mixed[1].revents == In);
    late.join();
    std::thread lateInfinite([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        Require(write_nid_postfix(ends[1], "y", 1) == 1);
    });
    Require(read_nid_postfix(ends[0], buffer, sizeof(buffer)) == 1);
    const auto waitStart = std::chrono::steady_clock::now();
    Require(poll_nid_postfix(&reader, 1, -1) == 1 && reader.revents == (In | ReadNormal));
    Require(std::chrono::steady_clock::now() - waitStart >= std::chrono::milliseconds(20));
    lateInfinite.join();
    Require(read_nid_postfix(ends[0], buffer, sizeof(buffer)) == 1);
    Require(write_nid_postfix(ends[1], "z", 1) == 1);
    Require(close_nid_postfix(ends[1]) == 0);
    Require(poll_nid_postfix(&reader, 1, 0) == 1 && reader.revents == (In | ReadNormal | Hup));
    Require(read_nid_postfix(ends[0], buffer, sizeof(buffer)) == 1);
    Require(poll_nid_postfix(&reader, 1, 0) == 1 && reader.revents == (In | ReadNormal | Hup));
    Require(read_nid_postfix(ends[0], buffer, sizeof(buffer)) == 0);
    GuestPollDescriptor closedPipe{ends[1], In, -1};
    Require(poll_nid_postfix(&closedPipe, 1, 0) == 1 && closedPipe.revents == Invalid);
    Require(close_nid_postfix(ends[0]) == 0);
    int full[2];
    Require(pipe_nid_postfix(full) == 0);
    static char block[4096];
    std::int64_t queued = 0;
    GuestPollDescriptor fill{full[1], Out, -1};
    while (poll_nid_postfix(&fill, 1, 0) == 1) {
        Require(fill.revents == Out && queued < (1 << 20));
        Require(write_nid_postfix(full[1], block, 256) == 256);
        queued += 256;
    }
    Require(queued > 0 && fill.revents == 0);
    while (queued > 0) {
        const std::int64_t taken = read_nid_postfix(full[0], block, static_cast<std::uint64_t>(std::min<std::int64_t>(queued, sizeof(block))));
        Require(taken > 0);
        queued -= taken;
    }
    Require(poll_nid_postfix(&fill, 1, 0) == 1 && fill.revents == Out);
    Require(close_nid_postfix(full[0]) == 0);
    Require(close_nid_postfix(full[1]) == 0);
    int other[2];
    Require(pipe_nid_postfix(other) == 0);
    Require(close_nid_postfix(other[0]) == 0);
    GuestPollDescriptor orphan{other[1], Out, -1};
    Require(poll_nid_postfix(&orphan, 1, 0) == 1 && orphan.revents == Hup);
    orphan.events = In | Out;
    Require(poll_nid_postfix(&orphan, 1, 0) == 1 && orphan.revents == (In | Hup));
    Require(close_nid_postfix(other[1]) == 0);
}
static void PollsUrgentData() {
    const int listener = socket_nid_postfix(2, 1, 0);
    Require(listener >= 0);
    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &size) == 0);
    Require(listen_nid_postfix(listener, 4) == 0);
    const int client = socket_nid_postfix(2, 1, 0);
    Require(client >= 0 && fcntl_nid_postfix(client, 4, 4) == 0);
    if (connect_nid_postfix(client, address.data(), address.size()) != 0)
        Require(*__error_nid_postfix() == 36);
    GuestPollDescriptor connecting{client, Priority | Out, -1};
    Require(poll_nid_postfix(&connecting, 1, 1000) == 1 && connecting.revents == Out);
    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0);
    GuestPollDescriptor pair[2]{{server, Priority | Out, -1}, {client, In | Priority | Out | ReadBand, -1}};
    Require(poll_nid_postfix(pair, 2, 0) == 2 && pair[0].revents == Out && pair[1].revents == Out);
    char byte = 'u';
    Require(send_nid_postfix(client, &byte, 1, 1) == -1 && *__error_nid_postfix() == 45);
    Require(recv_nid_postfix(server, &byte, 1, 1) == -1 && *__error_nid_postfix() == 45);
    const NativeSocket peer = ::socket(AF_INET, SOCK_STREAM, 0);
    Require(peer != NativeInvalid);
    sockaddr_in native{};
    native.sin_family = AF_INET;
    std::memcpy(&native.sin_port, address.data() + 2, 2);
    std::memcpy(&native.sin_addr, address.data() + 4, 4);
    Require(::connect(peer, reinterpret_cast<const sockaddr*>(&native), sizeof(native)) == 0);
    const int urgent = accept_nid_postfix(listener, nullptr, nullptr);
    Require(urgent >= 0);
    GuestPollDescriptor waiting{urgent, Priority, -1};
    Require(poll_nid_postfix(&waiting, 1, 0) == 0 && waiting.revents == 0);
    Require(::send(peer, "u", 1, MSG_OOB) == 1);
    Require(poll_nid_postfix(&waiting, 1, 1000) == 1 && waiting.revents == Priority);
    GuestPollDescriptor bands{urgent, In | Priority | Out, -1};
    Require(poll_nid_postfix(&bands, 1, 0) == 1 && bands.revents == (Priority | Out));
#ifdef _WIN32
    GuestPollDescriptor band{urgent, ReadBand, -1};
    Require(poll_nid_postfix(&band, 1, 0) == 1 && band.revents == ReadBand);
#endif
    CloseNative(peer);
    Require(close_nid_postfix(urgent) == 0);
    Require(close_nid_postfix(server) == 0);
    Require(close_nid_postfix(client) == 0);
    Require(close_nid_postfix(listener) == 0);
}
static void PollsAStreamWhosePeerClosed() {
    const int listener = socket_nid_postfix(2, 1, 0);
    Require(listener >= 0);
    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &size) == 0 && listen_nid_postfix(listener, 4) == 0);
    const int client = socket_nid_postfix(2, 1, 0);
    Require(client >= 0 && connect_nid_postfix(client, address.data(), address.size()) == 0);
    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0);

    Require(shutdown_nid_postfix(client, 1) == 0);
    GuestPollDescriptor wait{client, static_cast<short>(In | Out), 0};
    Require(poll_nid_postfix(&wait, 1, 1000) == 1 && wait.revents == Out);
    wait = {server, In, 0};
    Require(poll_nid_postfix(&wait, 1, 1000) == 1 && wait.revents == In);
    wait = {server, static_cast<short>(In | Out), 0};
    Require(poll_nid_postfix(&wait, 1, 1000) == 1 && wait.revents == (In | Out));
    wait = {server, ReadNormal, 0};
    Require(poll_nid_postfix(&wait, 1, 1000) == 1 && wait.revents == ReadNormal);
#ifndef _WIN32
    wait = {server, static_cast<short>(In | Priority | ReadBand), 0};
    Require(poll_nid_postfix(&wait, 1, 1000) == 1 && wait.revents == In);
#endif
    wait = {server, Out, 0};
    Require(poll_nid_postfix(&wait, 1, 1000) == 1 && wait.revents == Out);
    char byte;
    Require(recv_nid_postfix(server, &byte, 1, 0) == 0);

    Require(shutdown_nid_postfix(server, 1) == 0);
    wait = {server, In, 0};
    Require(poll_nid_postfix(&wait, 1, 1000) == 1 && wait.revents == (In | Hup));
    wait = {client, In, 0};
    Require(poll_nid_postfix(&wait, 1, 1000) == 1 && wait.revents == (In | Hup));
    for (const int descriptor : {client, server, listener}) Require(close_nid_postfix(descriptor) == 0);
}

#ifndef _WIN32
static void PollsAHungUpSocketWithoutPollout() {
    const int listener = socket_nid_postfix(2, 1, 0);
    Require(listener >= 0);
    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &size) == 0 && listen_nid_postfix(listener, 1) == 0);
    const int client = socket_nid_postfix(2, 1, 0);
    Require(client >= 0 && connect_nid_postfix(client, address.data(), address.size()) == 0);
    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0);
    Require(close_nid_postfix(client) == 0);
    char byte = 0;
    GuestPollDescriptor ended{server, In, 0};
    Require(poll_nid_postfix(&ended, 1, 1000) == 1);
    Require(recv_nid_postfix(server, &byte, 1, 0) == 0);
    Require(shutdown_nid_postfix(server, 1) == 0);
    GuestPollDescriptor hungUp{server, static_cast<short>(In | Out), 0};
    Require(poll_nid_postfix(&hungUp, 1, 1000) == 1 && hungUp.revents == (In | Hup));
    Require(close_nid_postfix(server) == 0 && close_nid_postfix(listener) == 0);
}
#endif

int main() {
    for (const int descriptor : {0, 1, 2}) StandardDescriptorFile(descriptor);
    PollsUrgentData();
    PollsAStreamWhosePeerClosed();
#ifndef _WIN32
    PollsAHungUpSocketWithoutPollout();
#endif
    const int listener = socket_nid_postfix(2, 1, 0);
    Require(listener >= 0 && listener < 1024);
    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &size) == 0);
    Require(listen_nid_postfix(listener, 4) == 0);
    GuestPollDescriptor single{listener, In, -1};
    Require(poll_nid_postfix(&single, 1, 0) == 0 && single.revents == 0);
    const auto start = std::chrono::steady_clock::now();
    Require(poll_nid_postfix(&single, 1, 50) == 0);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40));
    const int client = socket_nid_postfix(2, 1, 0);
    Require(client >= 0 && connect_nid_postfix(client, address.data(), address.size()) == 0);
    Require(poll_nid_postfix(&single, 1, 1000) == 1 && single.revents == In);
    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0);
    GuestPollDescriptor pair[3]{{server, In, 0}, {-1, In, 7}, {client, Out, 0}};
    Require(poll_nid_postfix(pair, 3, 1000) == 1);
    Require(pair[0].revents == 0 && pair[1].revents == 0 && pair[2].revents == Out);
    const char message[] = "poll";
    Require(send_nid_postfix(client, message, sizeof(message), 0) == sizeof(message));
    pair[2].events = In;
    Require(poll_nid_postfix(pair, 3, 1000) == 1 && pair[0].revents == In && pair[2].revents == 0);
    char buffer[8]{};
    Require(recv_nid_postfix(server, buffer, sizeof(buffer), 0) == sizeof(message));
    Require(close_nid_postfix(client) == 0);
    Require(poll_nid_postfix(pair, 1, 1000) == 1 && (pair[0].revents & (In | Hup)) != 0);
    Require(recv_nid_postfix(server, buffer, sizeof(buffer), 0) == 0);
    GuestPollDescriptor closed[2]{{client, In, 0}, {listener, In, 0}};
    Require(poll_nid_postfix(closed, 2, -1) == 1 && closed[0].revents == Invalid && closed[1].revents == 0);
    Require(poll_nid_postfix(nullptr, 0, 10) == 0);
    const auto shortWaitStart = std::chrono::steady_clock::now();
    for (int i = 0; i < 20; ++i) Require(poll_nid_postfix(nullptr, 0, 1) == 0);
    const auto shortWait = std::chrono::steady_clock::now() - shortWaitStart;
    Require(shortWait >= std::chrono::milliseconds(20) && shortWait < std::chrono::milliseconds(200));
    Require(poll_nid_postfix(nullptr, 1, 0) == -1 && *__error_nid_postfix() == 14);
    Require(poll_nid_postfix(&single, 1, -2) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(poll_nid_postfix(reinterpret_cast<GuestPollDescriptor*>(std::uintptr_t{0x10}), 1, 0) == -1 && *__error_nid_postfix() == 14);
    GuestResourceLimit fileLimit{};
    Require(getrlimit_nid_postfix(8, &fileLimit) == 0);
    if (fileLimit.rlim_max < 0xffffffff) {
        *__error_nid_postfix() = 0;
        Require(poll_nid_postfix(&single, 0xffffffffu, 0) == -1 && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(poll_nid_postfix(nullptr, 0xffffffffu, 0) == -1 && *__error_nid_postfix() == 22);
    }
    if (fileLimit.rlim_max >= 1024 && fileLimit.rlim_max < 100000) {
        const auto atLimit = static_cast<std::uint32_t>(fileLimit.rlim_max);
        *__error_nid_postfix() = 0;
        Require(poll_nid_postfix(nullptr, atLimit, 0) == -1 && *__error_nid_postfix() == 14);
        *__error_nid_postfix() = 0;
        Require(poll_nid_postfix(nullptr, atLimit + 1, 0) == -1 && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(poll_nid_postfix(reinterpret_cast<GuestPollDescriptor*>(std::uintptr_t{0x10}), atLimit + 1, 0) == -1 && *__error_nid_postfix() == 22);
    }
#ifndef _WIN32
    GuestPollDescriptor allocated[3]{{-1, In, 5}, {-1, In, 6}, {-1, In, 7}};
    failAllocationOfSize = sizeof(allocated);
    *__error_nid_postfix() = 0;
    Require(poll_nid_postfix(allocated, 3, 0) == -1 && *__error_nid_postfix() == 12);
    Require(failAllocationOfSize == 0);
    Require(allocated[0].revents == 5 && allocated[1].revents == 6 && allocated[2].revents == 7);
    Require(poll_nid_postfix(allocated, 3, 0) == 0 && allocated[0].revents == 0);
#endif
    void* page = nullptr;
    Require(sceKernelMapFlexibleMemory(&page, 0x4000, 3, 0) == 0 && page != nullptr);
    auto* readOnly = static_cast<GuestPollDescriptor*>(page);
    *readOnly = {-1, In, 7};
    Require(sceKernelMprotect(page, 0x4000, 1) == 0);
    *__error_nid_postfix() = 0;
    Require(poll_nid_postfix(readOnly, 1, 0) == -1 && *__error_nid_postfix() == 14);
    Require(readOnly->descriptor == -1 && readOnly->revents == 7);
    TestPipes(listener);
    PollsFilesAndDirectories(listener);
    Require(close_nid_postfix(server) == 0);
    for (const char* path : {"/dev/urandom", "/dev/random"}) {
        const int random = open_nid_postfix(path, 0, 0);
        Require(random >= 0);
        GuestPollDescriptor device{random, static_cast<short>(In | Out | Priority), -1};
        Require(poll_nid_postfix(&device, 1, 0) == 1 && device.revents == In);
        device.events = static_cast<short>(ReadNormal | Out);
        Require(poll_nid_postfix(&device, 1, 0) == 1 && device.revents == ReadNormal);
        device.events = static_cast<short>(Out | Priority | ReadBand | WriteBand);
        Require(poll_nid_postfix(&device, 1, -1) == 1 && device.revents == (Out | Priority | ReadBand | WriteBand));
        GuestPollDescriptor mixed[2]{{listener, In, 0}, {random, Out, 0}};
        Require(poll_nid_postfix(mixed, 2, -1) == 1 && mixed[0].revents == 0 && mixed[1].revents == Out);
        Require(close_nid_postfix(random) == 0);
    }
    GuestPollDescriptor console[3]{{0, static_cast<short>(In | ReadNormal | Out), -1}, {1, static_cast<short>(In | Out | WriteBand), -1}, {2, Out, -1}};
    Require(poll_nid_postfix(console, 3, 0) == 2);
    Require(console[0].revents == 0 && console[1].revents == Out && console[2].revents == Out);
    GuestPollDescriptor input{0, In, -1};
    const auto inputStart = std::chrono::steady_clock::now();
    Require(poll_nid_postfix(&input, 1, 30) == 0 && input.revents == 0);
    Require(std::chrono::steady_clock::now() - inputStart >= std::chrono::milliseconds(25));
    PollsKqueues(listener);
    Require(close_nid_postfix(listener) == 0);
}
