#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
struct PollDescriptor {
    int descriptor;
    short events;
    short revents;
};
extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI listen_nid_postfix(int, int);
int APS5_VABI accept_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
std::int64_t APS5_VABI send_nid_postfix(int, const void*, std::uint64_t, int);
std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI open_nid_postfix(const char*, int, int);
int APS5_VABI poll_nid_postfix(PollDescriptor*, std::uint32_t, int);
int APS5_VABI pipe_nid_postfix(int*);
std::int64_t APS5_VABI read_nid_postfix(int, void*, std::uint64_t);
std::int64_t APS5_VABI write_nid_postfix(int, const char*, std::int64_t);
int APS5_VABI fcntl_nid_postfix(int, int, ...);
int* APS5_VABI __error_nid_postfix();
}
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
constexpr short In = 0x1;
constexpr short Priority = 0x2;
constexpr short ReadBand = 0x80;
constexpr short Out = 0x4;
constexpr short Hup = 0x10;
constexpr short Invalid = 0x20;
constexpr short ReadNormal = 0x40;
static void TestRegularFile(int socket) {
    const std::string path = "anyps5-poll-regular-file-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin";
    const int file = open_nid_postfix(path.c_str(), 0x602, 0644);
    Require(file >= 0);
    PollDescriptor one{file, In, 7};
    Require(poll_nid_postfix(&one, 1, 0) == 1 && one.revents == In);
    one = {file, Out, 7};
    Require(poll_nid_postfix(&one, 1, 0) == 1 && one.revents == Out);
    one = {file, In | ReadNormal | Out, 7};
    Require(poll_nid_postfix(&one, 1, 0) == 1 && one.revents == (In | ReadNormal | Out));
    one = {file, In, 7};
    const auto readyStart = std::chrono::steady_clock::now();
    Require(poll_nid_postfix(&one, 1, 2000) == 1 && one.revents == In);
    Require(std::chrono::steady_clock::now() - readyStart < std::chrono::milliseconds(1000));
    one = {file, Priority | ReadBand, 7};
    Require(poll_nid_postfix(&one, 1, 0) == 0 && one.revents == 0);
    const auto waitStart = std::chrono::steady_clock::now();
    Require(poll_nid_postfix(&one, 1, 50) == 0 && one.revents == 0);
    Require(std::chrono::steady_clock::now() - waitStart >= std::chrono::milliseconds(40));
    PollDescriptor mixed[3]{{socket, In, 7}, {file, In, 7}, {-1, In, 7}};
    const auto mixedStart = std::chrono::steady_clock::now();
    Require(poll_nid_postfix(mixed, 3, 2000) == 1);
    Require(mixed[0].revents == 0 && mixed[1].revents == In && mixed[2].revents == 0);
    Require(std::chrono::steady_clock::now() - mixedStart < std::chrono::milliseconds(1000));
    Require(close_nid_postfix(file) == 0);
    one = {file, In, 0};
    Require(poll_nid_postfix(&one, 1, 0) == 1 && one.revents == Invalid);
    std::filesystem::remove(path);
}
static void TestPipes(int socket) {
    int ends[2];
    Require(pipe_nid_postfix(ends) == 0);
    PollDescriptor empty[2]{{ends[0], In, -1}, {ends[1], Out, -1}};
    Require(poll_nid_postfix(empty, 2, 0) == 1 && empty[0].revents == 0 && empty[1].revents == Out);
    PollDescriptor reader{ends[0], In | ReadNormal, -1};
    const auto start = std::chrono::steady_clock::now();
    Require(poll_nid_postfix(&reader, 1, 50) == 0 && reader.revents == 0);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40));
    Require(write_nid_postfix(ends[1], "wake", 4) == 4);
    Require(poll_nid_postfix(&reader, 1, 1000) == 1 && reader.revents == (In | ReadNormal));
    PollDescriptor writer{ends[1], In, -1};
    Require(poll_nid_postfix(&writer, 1, 0) == 0 && writer.revents == 0);
    PollDescriptor readerOut{ends[0], Out, -1};
    bool threw = false;
    try { poll_nid_postfix(&readerOut, 1, 0); } catch (const std::runtime_error&) { threw = true; }
    Require(threw);
    PollDescriptor mixed[2]{{socket, In, -1}, {ends[0], In, -1}};
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
    PollDescriptor closedPipe{ends[1], In, -1};
    Require(poll_nid_postfix(&closedPipe, 1, 0) == 1 && closedPipe.revents == Invalid);
    Require(close_nid_postfix(ends[0]) == 0);
    int full[2];
    Require(pipe_nid_postfix(full) == 0);
    static char block[4096];
    std::int64_t queued = 0;
    PollDescriptor fill{full[1], Out, -1};
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
    PollDescriptor orphan{other[1], Out, -1};
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
        Require(*__error_nid_postfix() == 35 || *__error_nid_postfix() == 36);
    PollDescriptor connecting{client, Priority | Out, -1};
    Require(poll_nid_postfix(&connecting, 1, 1000) == 1 && connecting.revents == Out);
    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0);
    PollDescriptor pair[2]{{server, Priority | Out, -1}, {client, In | Priority | Out | ReadBand, -1}};
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
    PollDescriptor waiting{urgent, Priority, -1};
    Require(poll_nid_postfix(&waiting, 1, 0) == 0 && waiting.revents == 0);
    Require(::send(peer, "u", 1, MSG_OOB) == 1);
    Require(poll_nid_postfix(&waiting, 1, 1000) == 1 && waiting.revents == Priority);
    PollDescriptor bands{urgent, In | Priority | Out, -1};
    Require(poll_nid_postfix(&bands, 1, 0) == 1 && bands.revents == (Priority | Out));
#ifdef _WIN32
    PollDescriptor band{urgent, ReadBand, -1};
    Require(poll_nid_postfix(&band, 1, 0) == 1 && band.revents == ReadBand);
#endif
    CloseNative(peer);
    Require(close_nid_postfix(urgent) == 0);
    Require(close_nid_postfix(server) == 0);
    Require(close_nid_postfix(client) == 0);
    Require(close_nid_postfix(listener) == 0);
}
int main() {
    PollsUrgentData();
    const int listener = socket_nid_postfix(2, 1, 0);
    Require(listener >= 0 && listener < 1024);
    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &size) == 0);
    Require(listen_nid_postfix(listener, 4) == 0);
    PollDescriptor single{listener, In, -1};
    Require(poll_nid_postfix(&single, 1, 0) == 0 && single.revents == 0);
    const auto start = std::chrono::steady_clock::now();
    Require(poll_nid_postfix(&single, 1, 50) == 0);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40));
    const int client = socket_nid_postfix(2, 1, 0);
    Require(client >= 0 && connect_nid_postfix(client, address.data(), address.size()) == 0);
    Require(poll_nid_postfix(&single, 1, 1000) == 1 && single.revents == In);
    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0);
    PollDescriptor pair[3]{{server, In, 0}, {-1, In, 7}, {client, Out, 0}};
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
    PollDescriptor closed[2]{{client, In, 0}, {listener, In, 0}};
    Require(poll_nid_postfix(closed, 2, -1) == 1 && closed[0].revents == Invalid && closed[1].revents == 0);
    Require(poll_nid_postfix(nullptr, 0, 10) == 0);
    const auto shortWaitStart = std::chrono::steady_clock::now();
    for (int i = 0; i < 20; ++i) Require(poll_nid_postfix(nullptr, 0, 1) == 0);
    const auto shortWait = std::chrono::steady_clock::now() - shortWaitStart;
    Require(shortWait >= std::chrono::milliseconds(20) && shortWait < std::chrono::milliseconds(200));
    Require(poll_nid_postfix(nullptr, 1, 0) == -1 && *__error_nid_postfix() == 14);
    Require(poll_nid_postfix(&single, 1, -2) == -1 && *__error_nid_postfix() == 22);
    TestPipes(listener);
    TestRegularFile(listener);
    Require(close_nid_postfix(server) == 0);
    const int file = open_nid_postfix("/dev/urandom", 0, 0);
    Require(file >= 0);
    PollDescriptor mixed[2]{{listener, In, 0}, {file, In, 0}};
    bool unsupported = false;
    try {
        poll_nid_postfix(mixed, 2, 0);
    } catch (const std::runtime_error&) {
        unsupported = true;
    }
    Require(unsupported);
    Require(close_nid_postfix(file) == 0);
    Require(close_nid_postfix(listener) == 0);
}
