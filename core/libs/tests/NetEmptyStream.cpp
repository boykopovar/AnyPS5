#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <string_view>

struct NetIovec {
    void* base;
    std::uint64_t length;
};

struct NetMsghdr {
    void* name;
    std::uint32_t nameLength;
    NetIovec* iov;
    int iovLength;
    void* control;
    std::uint32_t controlLength;
    int flags;
};

extern "C" {
int APS5_VABI sceNetSocket(const char*, int, int, int);
int APS5_VABI sceNetSocketClose(int);
int APS5_VABI sceNetBind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI sceNetGetsockname(int, void*, std::uint32_t*);
int APS5_VABI sceNetListen(int, int);
int APS5_VABI sceNetConnect(int, const void*, std::uint32_t);
int APS5_VABI sceNetAccept(int, void*, std::uint32_t*);
int APS5_VABI sceNetSetsockopt(int, int, int, const void*, std::uint32_t);
std::int64_t APS5_VABI sceNetSend(int, const void*, std::size_t, int);
std::int64_t APS5_VABI sceNetSendto(int, const void*, std::size_t, int, const void*, std::uint32_t);
std::int64_t APS5_VABI sceNetRecv(int, void*, std::size_t, int);
std::int64_t APS5_VABI sceNetRecvfrom(int, void*, std::size_t, int, void*, std::uint32_t*);
std::int64_t APS5_VABI sceNetRecvmsg(int, NetMsghdr*, int);
int* APS5_VABI sceNetErrnoLoc();
}

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "empty stream: %s\n", message);
    std::exit(1);
}

void SetOption(int descriptor, int option, int value) {
    Require(sceNetSetsockopt(descriptor, 0xffff, option, &value, sizeof(value)) == 0, "set option");
}

struct Pair {
    int sender;
    int receiver;

    explicit Pair(bool datagram) {
        const int listener = sceNetSocket("empty-listener", 2, datagram ? 2 : 1, 0);
        sender = sceNetSocket("empty-sender", 2, datagram ? 2 : 1, 0);
        Require(listener >= 0 && sender >= 0, "create sockets");
        std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
        Require(sceNetBind_nid_postfix(listener, address.data(), address.size()) == 0, "bind receiver");
        std::uint32_t size = address.size();
        Require(sceNetGetsockname(listener, address.data(), &size) == 0, "get receiver name");
        if (!datagram) Require(sceNetListen(listener, 1) == 0, "listen");
        Require(sceNetConnect(sender, address.data(), size) == 0, "connect sender");
        if (datagram) receiver = listener;
        else {
            receiver = sceNetAccept(listener, nullptr, nullptr);
            Require(receiver >= 0, "accept");
            Require(sceNetSocketClose(listener) == 0, "close listener");
        }
        SetOption(receiver, 0x1106, 2000000);
    }

    ~Pair() {
        Require(sceNetSocketClose(receiver) == 0 && sceNetSocketClose(sender) == 0, "close pair");
    }

    void Send() {
        Require(sceNetSend(sender, "x", 1, 0) == 1, "send byte");
    }
};

struct Result {
    std::int64_t count;
    int error;
};

Result Receive(std::string_view api, int descriptor, int flags, bool nullBuffer, bool datagram) {
    char payload = 'p';
    char control = 'c';
    std::array<unsigned char, 16> address;
    address.fill(0xaa);
    std::uint32_t addressLength = address.size();
    std::int64_t count;
    if (api == "recv") count = sceNetRecv(descriptor, nullBuffer ? nullptr : &payload, 0, flags);
    else if (api == "recvfrom")
        count = sceNetRecvfrom(descriptor, nullBuffer ? nullptr : &payload, 0, flags, address.data(), &addressLength);
    else {
        NetIovec vector{&payload, 0};
        NetMsghdr message{address.data(), addressLength, nullBuffer ? nullptr : &vector, nullBuffer ? 0 : 1, &control, 1, 0x4000};
        count = sceNetRecvmsg(descriptor, &message, flags);
        if (count >= 0) {
            Require(message.controlLength == 0, "control length is not zero");
            Require(message.flags == (datagram ? 0x10 : 0), "unexpected output flags");
            addressLength = message.nameLength;
        }
    }
    const int error = *sceNetErrnoLoc();
    Require(payload == 'p' && control == 'c', "empty receive modified payload or control storage");
    if (count >= 0 && api != "recv") {
        Require(addressLength == (datagram ? 16u : 0u), "wrong sender length for an empty receive");
        if (!datagram) Require(std::all_of(address.begin(), address.end(), [](unsigned char byte) { return byte == 0xaa; }),
            "empty stream receive wrote a sender address");
    }
    return {count, error};
}

void Check(std::string_view api, std::string_view mode) {
    const bool datagram = mode == "datagram";
    Pair pair(datagram);
    if (mode == "errors") {
        auto result = Receive(api, -1, 0, false, false);
        Require(result.count < 0 && result.error == 9, "invalid descriptor became an empty success");
        result = Receive(api, pair.receiver, 1, false, false);
        Require(result.count < 0 && result.error == 45, "unsupported flags became an empty success");
        const int fresh = sceNetSocket("empty-unconnected", 2, 1, 0);
        Require(fresh >= 0, "create unconnected socket");
        result = Receive(api, fresh, 0, false, false);
        std::fprintf(stderr, "unconnected result=%lld errno=%d\n", static_cast<long long>(result.count), result.error);
        Require(result.count < 0 && result.error == 57, "fresh stream socket did not report ENOTCONN");
        std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
        Require(sceNetBind_nid_postfix(fresh, address.data(), address.size()) == 0, "bind unconnected socket");
        result = Receive(api, fresh, 0, false, false);
        Require(result.count < 0 && result.error == 57, "bound stream socket did not report ENOTCONN");
        Require(sceNetListen(fresh, 1) == 0, "listen on unconnected socket");
        result = Receive(api, fresh, 0, false, false);
        Require(result.count < 0 && result.error == 57, "listening socket did not report ENOTCONN");
        Require(sceNetSocketClose(fresh) == 0, "close unconnected socket");
        return;
    }
    if (mode == "queued" || datagram) pair.Send();
    if (mode == "nonblocking") SetOption(pair.receiver, 0x1200, 1);
    const int flags = mode == "dontwait" ? 0x80 : mode == "peek" ? 0x82 : 0;
    auto pending = std::async(std::launch::async, [&] { return Receive(api, pair.receiver, flags, mode == "nullbuf", datagram); });
    const bool ready = pending.wait_for(std::chrono::milliseconds(500)) == std::future_status::ready;
    if (!ready) {
        pair.Send();
        Require(pending.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "cleanup receive did not finish");
    }
    const auto result = pending.get();
    Require(ready, "zero-length receive waited for peer data");
    Require(result.count == 0, "zero-length receive did not return zero");
    if (mode == "queued") {
        char byte = 0;
        Require(sceNetRecv(pair.receiver, &byte, 1, 0) == 1 && byte == 'x', "empty stream receive consumed data");
    }
    if (datagram) {
        char byte = 0;
        Require(sceNetRecv(pair.receiver, &byte, 1, 0x80) < 0 && *sceNetErrnoLoc() == 35,
            "zero-length datagram receive did not consume its packet");
    }
    std::printf("%.*s/%.*s: zero return, output sentinels and transport semantics preserved\n",
        static_cast<int>(api.size()), api.data(), static_cast<int>(mode.size()), mode.data());
}

}

int main(int argc, char** argv) {
    Require(argc == 3, "expected API and mode");
    const std::string_view api(argv[1]);
    Require(api == "recv" || api == "recvfrom" || api == "recvmsg", "unknown API");
    Check(api, argv[2]);
}
