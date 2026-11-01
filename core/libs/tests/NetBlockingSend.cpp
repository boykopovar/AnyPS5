#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
static int InterceptSend(SOCKET socket, const char* buffer, int length, int flags);
static int InterceptSendto(SOCKET socket, const char* buffer, int length, int flags, const sockaddr* address, int addressLength);
#define send(...) InterceptSend(__VA_ARGS__)
#define sendto(...) InterceptSendto(__VA_ARGS__)
#endif
#include "../prx/libSceNet/Export.cpp"
#ifdef _WIN32
#undef send
#undef sendto
#endif
#include <future>
#include <string_view>
#include <thread>

#ifdef _WIN32
static int InterceptSend(SOCKET socket, const char* buffer, int length, int flags) {
    return ::send(socket, buffer, std::min(length, 16384), flags);
}
static int InterceptSendto(SOCKET socket, const char* buffer, int length, int flags, const sockaddr* address, int addressLength) {
    return ::sendto(socket, buffer, std::min(length, 16384), flags, address, addressLength);
}
#endif

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "blocking send: %s\n", message);
    std::exit(1);
}

static void SetOption(int descriptor, int option, int value) {
    Require(sceNetSetsockopt(descriptor, NET_SOL_SOCKET, option, &value, sizeof(value)) == 0, "set socket option");
}

struct Connection {
    int listener = sceNetSocket("blocking-listener", 2, 1, 0);
    int sender = sceNetSocket("blocking-sender", 2, 1, 0);
    int receiver = -1;

    Connection() {
        Require(listener >= 0 && sender >= 0, "create TCP sockets");
        SetOption(listener, NET_SO_RCVBUF, 4096);
        SetOption(sender, NET_SO_SNDBUF, 4096);
        std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
        Require(sceNetBind_nid_postfix(listener, address.data(), address.size()) == 0, "bind listener");
        std::uint32_t size = address.size();
        Require(sceNetGetsockname(listener, address.data(), &size) == 0, "get listener address");
        Require(sceNetListen(listener, 1) == 0, "listen");
        Require(sceNetConnect(sender, address.data(), size) == 0, "connect");
        receiver = sceNetAccept(listener, nullptr, nullptr);
        Require(receiver >= 0, "accept");
        SetOption(receiver, NET_SO_RCVBUF, 1024 * 1024);
        SetOption(receiver, 0x1106, 2000000);
    }

    ~Connection() {
        for (const int descriptor : {receiver, sender, listener})
            Require(sceNetSocketClose(descriptor) == 0, "close socket");
    }
};

static std::int64_t Send(std::string_view api, int descriptor, std::vector<char>& bytes, int flags = 0) {
    if (api == "send") return sceNetSend(descriptor, bytes.data(), bytes.size(), flags);
    std::array<std::uint8_t, 16> address{};
    std::uint32_t size = address.size();
    Require(sceNetGetpeername(descriptor, address.data(), &size) == 0, "get destination address");
    if (api == "sendto") return sceNetSendto(descriptor, bytes.data(), bytes.size(), flags, address.data(), size);
    Require(api == "sendmsg", "unknown API");
    std::array<NetIovec, 2> vectors{{{bytes.data(), bytes.size() / 2},
        {bytes.data() + bytes.size() / 2, bytes.size() - bytes.size() / 2}}};
    const NetMsghdr message{address.data(), size, vectors.data(), static_cast<int>(vectors.size()), nullptr, 0, 0};
    return sceNetSendmsg(descriptor, &message, flags);
}

int main(int argc, char** argv) {
    Require(argc == 2, "one API argument required");
    const std::string_view api(argv[1]);
    std::vector<char> bytes(4 * 1024 * 1024);
    for (std::size_t index = 0; index < bytes.size(); ++index)
        bytes[index] = static_cast<char>((index * 31 + index / 65536) % 256);
    {
        Connection connection;
        auto sending = std::async(std::launch::async, [&] { return Send(api, connection.sender, bytes); });
        const auto early = sending.wait_for(std::chrono::milliseconds(50));
#ifdef _WIN32
        if (early == std::future_status::ready) {
            std::fprintf(stderr, "early send returned %lld of %zu bytes\n", static_cast<long long>(sending.get()), bytes.size());
            Require(false, "send returned before the peer drained its buffer");
        }
#else
        static_cast<void>(early);
#endif
        std::array<char, 65536> buffer{};
        std::size_t received = 0;
        while (received < bytes.size()) {
            const auto count = sceNetRecv(connection.receiver, buffer.data(), std::min(buffer.size(), bytes.size() - received), 0);
            Require(count > 0, "send did not deliver the whole payload");
            Require(std::equal(buffer.begin(), buffer.begin() + count, bytes.begin() + received), "send changed the payload");
            received += static_cast<std::size_t>(count);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Require(sending.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "send did not complete");
        Require(sending.get() == static_cast<std::int64_t>(bytes.size()), "send returned a short count");
        std::printf("%.*s: sent=%zu, delivered=%zu\n", static_cast<int>(api.size()), api.data(), bytes.size(), received);
    }
    {
        Connection connection;
#ifndef _WIN32
        SetOption(connection.receiver, NET_SO_RCVBUF, 4096);
#endif
        SetOption(connection.sender, 0x1105, 100000);
        auto started = std::chrono::steady_clock::now();
        const auto partial = Send(api, connection.sender, bytes);
        Require(partial > 0 && partial < static_cast<std::int64_t>(bytes.size()), "timeout must return queued bytes");
        Require(std::chrono::steady_clock::now() - started >= std::chrono::milliseconds(80), "partial send returned before its timeout");
        bool emptyTimeout = false;
        for (int attempt = 0; attempt < 64; ++attempt) {
            const auto result = Send(api, connection.sender, bytes);
            if (result < 0) {
                Require(*sceNetErrnoLoc() == NET_EAGAIN, "timeout without progress must return EAGAIN");
                emptyTimeout = true;
                break;
            }
            Require(result > 0 && result < static_cast<std::int64_t>(bytes.size()), "invalid timed byte count");
        }
        Require(emptyTimeout, "peer never exhausted the send buffers");
    }
    {
        Connection connection;
        auto sending = std::async(std::launch::async, [&] { return Send(api, connection.sender, bytes); });
        const auto entered = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        char peeked = 0;
        bool arrived = false;
        while (!arrived && std::chrono::steady_clock::now() < entered) {
            arrived = sceNetRecv(connection.receiver, &peeked, 1, NET_MSG_PEEK | NET_MSG_DONTWAIT) > 0;
            if (!arrived) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Require(arrived, "send did not reach the receiver before abort");
        Require(sending.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout, "send did not wait before abort");
        Require(sceNetSocketAbort(connection.sender, 0) == 0, "abort a blocked send");
        Require(sending.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "abort did not wake a blocked send");
        const auto partial = sending.get();
        Require(partial > 0 && partial < static_cast<std::int64_t>(bytes.size()), "error discarded queued bytes");
    }
    {
        Connection connection;
        SetOption(connection.sender, NET_SO_NBIO, 1);
        const auto partial = Send(api, connection.sender, bytes);
        Require(partial > 0 && partial <= static_cast<std::int64_t>(bytes.size()), "invalid guest nonblocking send result");
#ifdef _WIN32
        Require(partial <= 16384, "guest nonblocking send did not return its first host result");
#endif
    }
    std::puts("timeout, abort and guest nonblocking checks passed");
}
