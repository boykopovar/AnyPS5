#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

struct Iovec {
    void* base;
    std::uint64_t length;
};

struct Message {
    void* name;
    std::uint32_t nameLength;
    Iovec* iov;
    int iovLength;
    void* control;
    std::uint32_t controlLength;
    int flags;
};

static_assert(sizeof(Message) == 48 && offsetof(Message, iov) == 16 && offsetof(Message, flags) == 44);

extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI socketpair_nid_postfix(int, int, int, int*);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI listen_nid_postfix(int, int);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI accept_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI shutdown_nid_postfix(int, int);
int APS5_VABI ioctl_nid_postfix(int, std::uint64_t, void*);
std::int64_t APS5_VABI send_nid_postfix(int, const void*, std::uint64_t, int);
std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int);
std::int64_t APS5_VABI recvfrom_nid_postfix(int, void*, std::uint64_t, int, void*, std::uint32_t*);
std::int64_t APS5_VABI recvmsg_nid_postfix(int, Message*, int);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sceNetInit_nid_postfix();
int APS5_VABI sceNetSocket(const char*, int, int, int);
int APS5_VABI sceNetBind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI sceNetListen(int, int);
int APS5_VABI sceNetGetsockname(int, void*, std::uint32_t*);
int APS5_VABI sceNetConnect(int, const void*, std::uint32_t);
int APS5_VABI sceNetAccept(int, void*, std::uint32_t*);
int APS5_VABI sceNetSocketClose(int);
int APS5_VABI sceNetShutdown(int, int);
int APS5_VABI sceNetSetsockopt(int, int, int, const void*, std::uint32_t);
std::int64_t APS5_VABI sceNetSend(int, const void*, std::size_t, int);
std::int64_t APS5_VABI sceNetRecv(int, void*, std::size_t, int);
std::int64_t APS5_VABI sceNetRecvfrom(int, void*, std::size_t, int, void*, std::uint32_t*);
std::int64_t APS5_VABI sceNetRecvmsg(int, Message*, int);
int* APS5_VABI sceNetErrnoLoc();
}

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Api {
    bool sce;
    int Socket(int type) const { return sce ? sceNetSocket("receive shutdown", 2, type, 0) : socket_nid_postfix(2, type, 0); }
    int Bind(int fd, const void* address) const { return sce ? sceNetBind_nid_postfix(fd, address, 16) : bind_nid_postfix(fd, address, 16); }
    int Connect(int fd, const void* address) const { return sce ? sceNetConnect(fd, address, 16) : connect_nid_postfix(fd, address, 16); }
    int Name(int fd, void* address, std::uint32_t* size) const { return sce ? sceNetGetsockname(fd, address, size) : getsockname_nid_postfix(fd, address, size); }
    int Close(int fd) const { return sce ? sceNetSocketClose(fd) : close_nid_postfix(fd); }
    int Shutdown(int fd, int how) const { return sce ? sceNetShutdown(fd, how) : shutdown_nid_postfix(fd, how); }
    int* Error() const { return sce ? sceNetErrnoLoc() : __error_nid_postfix(); }
    bool Failed(std::int64_t value, int error) const {
        return value == (sce ? static_cast<int>(0x80410100u | static_cast<unsigned>(error)) : -1) && *Error() == error;
    }
    std::int64_t Send(int fd, const void* data, std::size_t size) const {
        return sce ? sceNetSend(fd, data, size, 0) : send_nid_postfix(fd, data, size, 0);
    }
    std::int64_t Receive(int kind, int fd, Message& message, int flags = 0) const {
        if (kind == 2) return sce ? sceNetRecvmsg(fd, &message, flags) : recvmsg_nid_postfix(fd, &message, flags);
        const auto& data = message.iov[0];
        if (kind == 1) return sce ? sceNetRecvfrom(fd, data.base, data.length, flags, message.name, &message.nameLength)
            : recvfrom_nid_postfix(fd, data.base, data.length, flags, message.name, &message.nameLength);
        return sce ? sceNetRecv(fd, data.base, data.length, flags) : recv_nid_postfix(fd, data.base, data.length, flags);
    }
    void Nonblocking(int fd) const {
        int enabled = 1;
        Require((sce ? sceNetSetsockopt(fd, 0xffff, 0x1200, &enabled, sizeof(enabled))
            : ioctl_nid_postfix(fd, 0x8004667e, &enabled)) == 0, "set nonblocking");
    }
};

struct Pair {
    const Api& api;
    int receiver = -1;
    int sender = -1;
    int listener = -1;
    explicit Pair(const Api& value) : api(value) {}
    ~Pair() {
        if (receiver >= 0) api.Close(receiver);
        if (sender >= 0) api.Close(sender);
        if (listener >= 0) api.Close(listener);
    }
    void Open(int type) {
        if (type > 100) {
            int descriptors[2]{};
            Require(socketpair_nid_postfix(1, type - 100, 0, descriptors) == 0, "socketpair");
            receiver = descriptors[0];
            sender = descriptors[1];
            return;
        }
        std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
        std::uint32_t size = address.size();
        listener = api.Socket(type);
        Require(listener >= 0 && api.Bind(listener, address.data()) == 0, "bind loopback");
        Require(api.Name(listener, address.data(), &size) == 0 && size == 16, "bound address");
        sender = api.Socket(type);
        Require(sender >= 0, "sender socket");
        if (type == 1) {
            Require((api.sce ? sceNetListen(listener, 1) : listen_nid_postfix(listener, 1)) == 0, "listen");
            Require(api.Connect(sender, address.data()) == 0, "connect TCP");
            receiver = api.sce ? sceNetAccept(listener, nullptr, nullptr) : accept_nid_postfix(listener, nullptr, nullptr);
            Require(receiver >= 0, "accept");
        } else {
            receiver = listener;
            listener = -1;
            Require(api.Connect(sender, address.data()) == 0, "connect UDP sender");
            size = address.size();
            Require(api.Name(sender, address.data(), &size) == 0 && api.Connect(receiver, address.data()) == 0, "connect UDP receiver");
        }
    }
};

struct Buffer {
    std::array<std::uint8_t, 10> data;
    std::array<std::uint8_t, 32> address;
    std::array<std::uint8_t, 8> control;
    Iovec vectors[2];
    Message message;
    explicit Buffer(int kind) : vectors{{data.data() + 1, kind == 2 ? 4u : 8u}, {data.data() + 5, 4}},
        message{address.data(), 32, vectors, kind == 2 ? 2 : 1, control.data(), 8, 0x7fff} {
        data.fill(0xa5);
        address.fill(0xb6);
        control.fill(0xc7);
    }
};

static void CheckShutdown(const Api& api, int kind, int type, int how, bool nonblocking) {
    Pair pair(api);
    pair.Open(type);
    if (nonblocking) api.Nonblocking(pair.receiver);
    Require(api.Shutdown(pair.receiver, how) == 0, "shutdown receive");
    for (int flags : {0, 2, 0}) {
        Buffer buffer(kind);
        *api.Error() = 73;
        const auto result = api.Receive(kind, pair.receiver, buffer.message, flags);
        if (result != 0) throw std::runtime_error("shutdown receive returned " + std::to_string(result) + " errno=" + std::to_string(*api.Error()));
        Require(*api.Error() == 73, "EOF must preserve guest errno");
        for (auto byte : buffer.data) Require(byte == 0xa5, "EOF changed data or guard bytes");
        for (auto byte : buffer.address) Require(byte == 0xb6, "EOF changed address bytes");
        for (auto byte : buffer.control) Require(byte == 0xc7, "EOF changed control bytes");
        if (kind != 0) Require(buffer.message.nameLength == 0, "EOF source address length");
        if (kind == 2) Require(buffer.message.flags == flags && buffer.message.controlLength == 0, "EOF message outputs");
        buffer.message.name = nullptr;
        Require(api.Receive(kind, pair.receiver, buffer.message) == 0, "EOF without source address");
    }
    Buffer invalid(kind);
    Require(api.Failed(api.Receive(kind, pair.receiver, invalid.message, 1), 45), "shutdown must not bypass flag validation");
    invalid.vectors[0].base = nullptr;
    Require(api.Failed(api.Receive(kind, pair.receiver, invalid.message), api.sce && kind != 2 ? 22 : 14), "shutdown must not bypass buffer validation");
    if (how == 0) {
        const char byte = 'x';
        Require(api.Send(pair.receiver, &byte, 1) == 1, "SHUT_RD must preserve sending");
        Buffer response(kind);
        if (type == 1) response.message.name = nullptr;
        Require(api.Receive(kind, pair.sender, response.message) == 1 && response.data[1] == 'x', "peer must receive after SHUT_RD");
    } else {
        const char byte = 'x';
        Require(api.Send(pair.receiver, &byte, 1) < 0, "SHUT_RDWR send must remain an error");
    }
    const int closed = pair.receiver;
    Require(api.Close(closed) == 0, "close receiver");
    pair.receiver = -1;
    Buffer buffer(kind);
    Require(api.Failed(api.Receive(kind, closed, buffer.message), 9), "closed descriptor must remain an error");
}

static void CheckControls(const Api& api, int kind, int type) {
    Pair pair(api);
    pair.Open(type);
    if (type == 1) Require(api.Shutdown(pair.receiver, 1) == 0, "shutdown send");
    else {
        const char byte = 0;
        Require(api.Send(pair.sender, &byte, 0) == 0, "empty datagram send");
        Buffer empty(kind);
        Require(api.Receive(kind, pair.receiver, empty.message) == 0, "empty datagram receive");
        if (kind != 0) Require(empty.message.nameLength == 16 && empty.address[1] == 2, "empty datagram must retain source address");
    }
    const char byte = 'y';
    Require(api.Send(pair.sender, &byte, 1) == 1, "control send");
    Buffer buffer(kind);
    if (type == 1) buffer.message.name = nullptr;
    Require(api.Receive(kind, pair.receiver, buffer.message) == 1 && buffer.data[1] == 'y', "normal receive after control");
    api.Nonblocking(pair.receiver);
    Buffer waiting(kind);
    Require(api.Failed(api.Receive(kind, pair.receiver, waiting.message), 35), "would-block must remain an error");
}

int main(int argc, char** argv) {
    if (argc != 3 || (std::strcmp(argv[1], "kernel") && std::strcmp(argv[1], "sce"))) return 2;
    const int kind = std::atoi(argv[2]);
    if (kind < 0 || kind > 2) return 2;
    const Api api{std::strcmp(argv[1], "sce") == 0};
    if (api.sce) Require(sceNetInit_nid_postfix() == 0, "initialize SceNet");
    int failures = 0;
    for (int type : {1, 2, 101, 102}) {
        if (api.sce && type > 100) continue;
        for (int how : {0, 2}) {
            for (bool nonblocking : {false, true}) {
                try { CheckShutdown(api, kind, type, how, nonblocking); }
                catch (const std::exception& error) {
                    std::fprintf(stderr, "%s kind=%d type=%d how=%d nonblocking=%d: %s\n", argv[1], kind, type, how, nonblocking, error.what());
                    ++failures;
                }
            }
        }
        if (type > 100) continue;
        try { CheckControls(api, kind, type); }
        catch (const std::exception& error) {
            std::fprintf(stderr, "%s kind=%d type=%d controls: %s\n", argv[1], kind, type, error.what());
            ++failures;
        }
    }
    return failures ? 1 : 0;
}
