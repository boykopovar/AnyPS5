#include "prx/libcurl/src/CurlHost.hpp"
#include "prx/libcurl/src/CurlSockets.hpp"
#include "prx/libcurl/libcurl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Socket/include/SocketBorrow.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <source_location>
#include <string>
#include <thread>
#include <vector>

static void Require(bool condition, std::source_location where = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "socket test failed at %s:%u\n", where.file_name(), where.line());
        std::abort();
    }
}

static int NativeClose(curl_socket_t socket) {
#ifdef _WIN32
    return ::closesocket(socket);
#else
    return ::close(socket);
#endif
}

static std::vector<int> Descriptors() {
    std::vector<int> result;
    KernelListSockets_nid_no_patch(&result);
    std::sort(result.begin(), result.end());
    return result;
}

static bool Describes(int descriptor) {
    std::shared_ptr<void> owner;
    std::uintptr_t native = 0;
    return KernelLookupSocketNative_nid_no_patch(descriptor, &owner, &native);
}

static const std::string Body("A\0B\xff\r\n", 6);

class Server {
public:
    explicit Server(bool keepAlive = false, bool ipv6 = false, bool holdResponse = false) : releaseResponse(!holdResponse) {
        listener = ::socket(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
        Require(listener != CURL_SOCKET_BAD);
        sockaddr_storage address{};
        auto* v4 = reinterpret_cast<sockaddr_in*>(&address);
        auto* v6 = reinterpret_cast<sockaddr_in6*>(&address);
        const auto size = ipv6 ? sizeof(*v6) : sizeof(*v4);
        if (ipv6) {
            v6->sin6_family = AF_INET6;
            v6->sin6_addr = in6addr_loopback;
        } else {
            v4->sin_family = AF_INET;
            v4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        }
        Require(::bind(listener, reinterpret_cast<const sockaddr*>(&address), static_cast<int>(size)) == 0);
        Require(::listen(listener, 4) == 0);
#ifdef _WIN32
        int length = sizeof(address);
#else
        socklen_t length = sizeof(address);
#endif
        Require(::getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) == 0);
        port = ntohs(ipv6 ? v6->sin6_port : v4->sin_port);
        worker = std::thread([this, keepAlive] {
            const auto peer = ::accept(listener, nullptr, nullptr);
            if (peer == CURL_SOCKET_BAD) return;
#ifdef _WIN32
            const DWORD timeout = 3000;
#else
            const timeval timeout{3, 0};
#endif
            Require(::setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0);
            std::string request;
            char bytes[512];
            while (request.find("\r\n\r\n") == std::string::npos) {
                const auto received = ::recv(peer, bytes, sizeof(bytes), 0);
                if (received <= 0) { NativeClose(peer); return; }
                request.append(bytes, static_cast<std::size_t>(received));
            }
            {
                std::unique_lock lock(mutex);
                cv.wait(lock, [this] { return releaseResponse; });
            }
            const auto response = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(Body.size()) +
                (keepAlive ? "\r\nConnection: keep-alive\r\n\r\n" : "\r\nConnection: close\r\n\r\n") + Body;
            std::size_t sent = 0;
            while (sent < response.size()) {
#ifdef _WIN32
                const auto count = ::send(peer, response.data() + sent, static_cast<int>(response.size() - sent), 0);
#else
                const auto count = ::send(peer, response.data() + sent, response.size() - sent, MSG_NOSIGNAL);
#endif
                Require(count > 0);
                sent += static_cast<std::size_t>(count);
            }
            if (keepAlive) while (::recv(peer, bytes, sizeof(bytes), 0) > 0) {}
            NativeClose(peer);
            completed = true;
        });
    }
    ~Server() {
        ReleaseResponse();
#ifdef _WIN32
        ::shutdown(listener, SD_BOTH);
#else
        ::shutdown(listener, SHUT_RDWR);
#endif
        NativeClose(listener);
        worker.join();
    }
    void ReleaseResponse() {
        {
            std::lock_guard lock(mutex);
            releaseResponse = true;
        }
        cv.notify_all();
    }
    std::string Url(const std::string& host = "127.0.0.1") const {
        return "http://" + host + ":" + std::to_string(port) + "/socket-test";
    }
    std::uint16_t Port() const { return port; }
    bool Completed() const { return completed.load(); }
private:
    curl_socket_t listener = CURL_SOCKET_BAD;
    std::uint16_t port = 0;
    std::mutex mutex;
    std::condition_variable cv;
    bool releaseResponse = true;
    std::thread worker;
    std::atomic<bool> completed{false};
};

struct State {
    CURLM* multi = nullptr;
    std::map<int, int> watched;
    std::map<int, int> tags;
    std::set<int> live;
    std::set<int> exposed;
    std::string output;
    std::chrono::steady_clock::time_point timer = std::chrono::steady_clock::time_point::max();
    int opens = 0;
    int closes = 0;
    int options = 0;
    int notifications = 0;
    int timers = 0;
    std::uint16_t redirectPort = 0;
    int family = 2;
};

static size_t APS5_VABI Write(char* bytes, size_t size, size_t count, void* opaque) {
    static_cast<State*>(opaque)->output.append(bytes, size * count);
    return size * count;
}

static int APS5_VABI Open(void* opaque, int purpose, guest_curl_sockaddr* address) {
    auto& state = *static_cast<State*>(opaque);
    Require(purpose == CURLSOCKTYPE_IPCXN);
    Require(address->family == state.family && address->socktype == 1);
    const auto length = state.family == 2 ? 16u : 28u;
    Require(address->addrlen == length && address->addr.length == length && address->addr.family == state.family);
    if (state.family == 28) {
        const auto* bytes = reinterpret_cast<const unsigned char*>(address) + offsetof(guest_curl_sockaddr, addr);
        Require(bytes[23] == 1);
    }
    if (state.redirectPort) {
        const auto port = htons(state.redirectPort);
        std::memcpy(address->addr.data, &port, sizeof(port));
    }
    const int descriptor = socket_nid_postfix(address->family, address->socktype, address->protocol);
    Require(descriptor >= 0 && Describes(descriptor));
    Require(state.live.insert(descriptor).second);
    ++state.opens;
    return descriptor;
}

static int APS5_VABI Close(void* opaque, int descriptor) {
    auto& state = *static_cast<State*>(opaque);
    Require(state.live.erase(descriptor) == 1);
    Require(Describes(descriptor));
    Require(close_nid_postfix(descriptor) == 0);
    Require(!Describes(descriptor));
    ++state.closes;
    return 0;
}

static int APS5_VABI Option(void* opaque, int descriptor, int purpose) {
    auto& state = *static_cast<State*>(opaque);
    Require(purpose == CURLSOCKTYPE_IPCXN && Describes(descriptor));
    Require((fcntl_nid_postfix(descriptor, 3) & 4) == 0);
    const int keepAlive = 1;
    Require(setsockopt_nid_postfix(descriptor, 0xffff, 0x8, &keepAlive, sizeof(keepAlive)) == 0);
    ++state.options;
    return CURL_SOCKOPT_OK;
}

static int APS5_VABI SocketEvent(CURL* easy, int descriptor, int action, void* opaque, void* socketData) {
    auto& state = *static_cast<State*>(opaque);
    Require(easy != nullptr && Describes(descriptor));
    if (action != CURL_POLL_REMOVE) Require((fcntl_nid_postfix(descriptor, 3) & 4) != 0);
    ++state.notifications;
    state.exposed.insert(descriptor);
    if (socketData) Require(socketData == &state.tags.at(descriptor));
    if (action == CURL_POLL_REMOVE) {
        state.watched.erase(descriptor);
    } else {
        if (!socketData) {
            auto [it, added] = state.tags.try_emplace(descriptor, descriptor);
            static_cast<void>(added);
            Require(curl_multi_assign_nid_postfix(state.multi, descriptor, &it->second) == CURLM_OK);
        }
        state.watched[descriptor] = action;
    }
    return 0;
}

static int APS5_VABI Timer(CURLM* multi, std::int64_t milliseconds, void* opaque) {
    auto& state = *static_cast<State*>(opaque);
    Require(multi == state.multi);
    ++state.timers;
    state.timer = milliseconds < 0 ? std::chrono::steady_clock::time_point::max() :
        std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    return 0;
}

static CURL* Easy(const std::string& url, State& state, bool callbacks) {
    auto* easy = curl_easy_init_nid_postfix();
    Require(easy != nullptr);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_URL, url.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_PROXY, "") == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_NOSIGNAL, std::int64_t{1}) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_TIMEOUT_MS, std::int64_t{3000}) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_WRITEFUNCTION, Write) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_WRITEDATA, &state) == CURLE_OK);
    if (callbacks) {
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_OPENSOCKETDATA, &state) == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_OPENSOCKETFUNCTION, Open) == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_SOCKOPTDATA, &state) == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_SOCKOPTFUNCTION, Option) == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_CLOSESOCKETDATA, &state) == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_CLOSESOCKETFUNCTION, Close) == CURLE_OK);
    }
    return easy;
}

static void Done(CURLM* multi, CURL* easy) {
    int queued = -1;
    auto* message = curl_multi_info_read_nid_postfix(multi, &queued);
    Require(message && message->msg == CURLMSG_DONE && message->easy_handle == easy);
    Require(message->data.result == CURLE_OK);
    Require(curl_multi_info_read_nid_postfix(multi, &queued) == nullptr);
}

static void SocketTransfer(bool callbacks) {
    const auto before = Descriptors();
    Server server(callbacks, false, true);
    State state;
    state.multi = curl_multi_init_nid_postfix();
    Require(state.multi != nullptr);
    Require(curl_multi_setopt_nid_postfix(state.multi, CURLMOPT_SOCKETFUNCTION, SocketEvent) == CURLM_OK);
    Require(curl_multi_setopt_nid_postfix(state.multi, CURLMOPT_SOCKETDATA, &state) == CURLM_OK);
    Require(curl_multi_setopt_nid_postfix(state.multi, CURLMOPT_TIMERFUNCTION, Timer) == CURLM_OK);
    Require(curl_multi_setopt_nid_postfix(state.multi, CURLMOPT_TIMERDATA, &state) == CURLM_OK);
    state.redirectPort = callbacks ? server.Port() : 0;
    auto* easy = Easy(callbacks ? "http://127.0.0.1:1/redirected" : server.Url(), state, callbacks);
    Require(curl_multi_add_handle_nid_postfix(state.multi, easy) == CURLM_OK);
    int running = -1;
    Require(curl_multi_socket_action_nid_postfix(state.multi, -1, 0, &running) == CURLM_OK);
    server.ReleaseResponse();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (running) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::vector<GuestPollDescriptor> poll;
        for (const auto& [descriptor, action] : state.watched) {
            short events = 0;
            if (action == CURL_POLL_IN || action == CURL_POLL_INOUT) events |= 1;
            if (action == CURL_POLL_OUT || action == CURL_POLL_INOUT) events |= 4;
            poll.push_back({descriptor, events, 0});
        }
        Require(poll_nid_postfix(poll.data(), static_cast<std::uint32_t>(poll.size()), 10) >= 0);
        for (const auto& socket : poll) {
            int events = 0;
            if (socket.revents & 1) events |= CURL_CSELECT_IN;
            if (socket.revents & 4) events |= CURL_CSELECT_OUT;
            if (socket.revents & (8 | 16 | 32)) events |= CURL_CSELECT_ERR;
            if (events) Require(curl_multi_socket_action_nid_postfix(state.multi, socket.descriptor, events, &running) == CURLM_OK);
        }
        if (std::chrono::steady_clock::now() >= state.timer) {
            state.timer = std::chrono::steady_clock::time_point::max();
            Require(curl_multi_socket_action_nid_postfix(state.multi, -1, 0, &running) == CURLM_OK);
        }
    }
    Done(state.multi, easy);
    Require(state.output == Body);
    Require(state.notifications > 0);
    Require(state.timers > 0);
    Require(curl_multi_remove_handle_nid_postfix(state.multi, easy) == CURLM_OK);
    State replacement;
    if (callbacks) Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_CLOSESOCKETDATA, &replacement) == CURLE_OK);
    curl_easy_cleanup_nid_postfix(easy);
    Require(curl_multi_cleanup_nid_postfix(state.multi) == CURLM_OK);
    Require(replacement.closes == 0);
    if (callbacks) Require(state.opens > 0 && state.opens == state.closes && state.options == state.opens && state.live.empty());
    Require(Descriptors() == before);
}

static bool IsSet(const guest_fd_set& set, int descriptor) {
    return descriptor >= 0 && descriptor < 1024 && (set.fds_bits[descriptor / 64] & (std::uint64_t{1} << (descriptor % 64))) != 0;
}

static void FdsetTransfer() {
    const auto before = Descriptors();
    Server server(false, false, true);
    State state;
    auto* easy = Easy(server.Url("localhost"), state, false);
    auto* multi = curl_multi_init_nid_postfix();
    Require(multi && curl_multi_add_handle_nid_postfix(multi, easy) == CURLM_OK);
    int running = 0;
    int sets = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    do {
        Require(curl_multi_perform_nid_postfix(multi, &running) == CURLM_OK);
        guest_fd_set read{}, write{}, except{};
        int maximum = 99;
        Require(curl_multi_fdset_nid_postfix(multi, &read, &write, &except, &maximum) == CURLM_OK);
        if (maximum >= 0) {
            for (int descriptor = 0; descriptor <= maximum; ++descriptor)
                if (IsSet(read, descriptor) || IsSet(write, descriptor)) Require(Describes(descriptor));
            server.ReleaseResponse();
            KernelTimeval timeout{0, 20000};
            Require(select_nid_postfix(maximum + 1, &read, &write, &except, &timeout) >= 0);
            ++sets;
        }
        Require(std::chrono::steady_clock::now() < deadline);
    } while (running);
    Done(multi, easy);
    Require(state.output == Body && sets > 0);
    Require(curl_multi_remove_handle_nid_postfix(multi, easy) == CURLM_OK);
    curl_easy_cleanup_nid_postfix(easy);
    Require(curl_multi_cleanup_nid_postfix(multi) == CURLM_OK);
    Require(Descriptors() == before);
}

static void ConnectOnly() {
    const auto before = Descriptors();
    Server server;
    State state;
    auto* easy = Easy(server.Url(), state, false);
    int descriptor = 99;
    Require(curl_easy_getinfo_nid_postfix(easy, CURLINFO_ACTIVESOCKET, &descriptor) == CURLE_OK && descriptor == -1);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_CONNECT_ONLY, std::int64_t{1}) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(easy) == CURLE_OK);
    Require(curl_easy_getinfo_nid_postfix(easy, CURLINFO_ACTIVESOCKET, &descriptor) == CURLE_OK && descriptor >= 0);
    Require((fcntl_nid_postfix(descriptor, 3) & 4) != 0);
    char unavailable;
    Require(recv_nid_postfix(descriptor, &unavailable, 1, 0) == -1 && *__error_nid_postfix() == 35);
    std::int64_t legacy = -1;
    Require(curl_easy_getinfo_nid_postfix(easy, CURLINFO_LASTSOCKET, &legacy) == CURLE_OK && legacy == descriptor);
    const std::string request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    Require(send_nid_postfix(descriptor, request.data(), request.size(), 0) == static_cast<std::int64_t>(request.size()));
    const int queue = kqueue_nid_postfix();
    Require(queue >= 0);
    KernelEvent change{};
    change.ident = descriptor;
    change.filter = EVFILT_READ;
    change.flags = EV_ADD | EV_ENABLE;
    Require(kevent_nid_postfix(queue, &change, 1, nullptr, 0, nullptr) == 0);
    KernelEvent event{};
    KernelTimespec timeout{2, 0};
    Require(kevent_nid_postfix(queue, nullptr, 0, &event, 1, &timeout) == 1 && event.ident == static_cast<std::uintptr_t>(descriptor));
    std::string response;
    while (!response.ends_with(Body)) {
        char buffer[512];
        const auto received = recv_nid_postfix(descriptor, buffer, sizeof(buffer), 0);
        Require(received > 0);
        response.append(buffer, static_cast<std::size_t>(received));
    }
    Require(response.starts_with("HTTP/1.1 200 OK\r\n"));
    curl_easy_cleanup_nid_postfix(easy);
    Require(!Describes(descriptor));
    Require(close_nid_postfix(queue) == 0);
    Require(Descriptors() == before);
}

static int APS5_VABI CloseBorrowed(void* opaque, int descriptor) {
    auto& state = *static_cast<State*>(opaque);
    Require(Describes(descriptor));
    Require(close_nid_postfix(descriptor) == 0 && !Describes(descriptor));
    ++state.closes;
    return 0;
}

static void CallbackOwnership() {
    const auto before = Descriptors();
    const auto probe = ::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in6 address{};
    address.sin6_family = AF_INET6;
    address.sin6_addr = in6addr_loopback;
    const bool ipv6 = probe != CURL_SOCKET_BAD && ::bind(probe, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
    if (probe != CURL_SOCKET_BAD) NativeClose(probe);
    if (!ipv6) std::fprintf(stderr, "IPv6 loopback unavailable; callback duplication exercised over IPv4\n");
    {
        Server server(false, ipv6);
        State state;
        state.family = ipv6 ? 28 : 2;
        auto* easy = Easy(server.Url(ipv6 ? "[::1]" : "127.0.0.1"), state, true);
        auto* duplicate = curl_easy_duphandle_nid_postfix(easy);
        Require(duplicate != nullptr);
        curl_easy_cleanup_nid_postfix(easy);
        Require(curl_easy_perform_nid_postfix(duplicate) == CURLE_OK);
        curl_easy_cleanup_nid_postfix(duplicate);
        Require(state.output == Body && state.opens == 1 && state.closes == 1 && state.options == 1 && state.live.empty());
    }
    {
        Server server;
        State state;
        auto* easy = Easy(server.Url(), state, false);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_CLOSESOCKETFUNCTION, CloseBorrowed) == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_CLOSESOCKETDATA, &state) == CURLE_OK);
        Require(curl_easy_perform_nid_postfix(easy) == CURLE_OK);
        curl_easy_cleanup_nid_postfix(easy);
        Require(state.output == Body && state.closes == 1);
    }
    Require(Descriptors() == before);
}

static int APS5_VABI RefuseOpen(void*, int, guest_curl_sockaddr*) { return -1; }
static int APS5_VABI RefuseOption(void*, int, int) { return CURL_SOCKOPT_ERROR; }

static void ErrorsAndExtraDescriptors() {
    const auto before = Descriptors();
    auto* multi = curl_multi_init_nid_postfix();
    int running = 17;
    Require(curl_multi_assign_nid_postfix(multi, 99999, nullptr) == CURLM_BAD_SOCKET);
    Require(curl_multi_socket_action_nid_postfix(multi, 99999, CURL_CSELECT_IN, &running) == CURLM_OK && running == 0);
    Require(curl_multi_socket_nid_postfix(multi, -1, &running) == CURLM_OK && running == 0);
    Require(curl_multi_socket_all_nid_postfix(multi, &running) == CURLM_OK && running == 0);
    Require(curl_multi_socket_action_nid_postfix(nullptr, -1, 0, &running) == CURLM_BAD_HANDLE);
    Require(curl_multi_fdset_nid_postfix(nullptr, nullptr, nullptr, nullptr, nullptr) == CURLM_BAD_HANDLE);
    guest_fd_set boundSet{};
    Require(!CurlSockets::Add_nid_no_patch(&boundSet, -1));
    Require(!CurlSockets::Add_nid_no_patch(&boundSet, 1024));
    Require(!CurlSockets::Add_nid_no_patch(&boundSet, 99999));
    for (const auto word : boundSet.fds_bits) Require(word == 0);
    Require(CurlSockets::Add_nid_no_patch(&boundSet, 42));
    Require(IsSet(boundSet, 42));
    Require(!IsSet(boundSet, -1));
    Require(!IsSet(boundSet, 1024));
    Require(CurlSockets::Add_nid_no_patch(nullptr, 42));
    Require(!CurlSockets::Add_nid_no_patch(nullptr, -1));
    Require(!CurlSockets::Add_nid_no_patch(nullptr, 1024));
    Require(curl_multi_poll_nid_postfix(nullptr, nullptr, 0, 0, nullptr) == CURLM_BAD_HANDLE);
    Require(curl_multi_wait_nid_postfix(multi, nullptr, 0, -1, nullptr) == CURLM_BAD_FUNCTION_ARGUMENT);
    int pair[2];
    Require(socketpair_nid_postfix(1, 1, 0, pair) == 0);
    Require(send_nid_postfix(pair[0], "x", 1, 0) == 1);
    guest_curl_waitfd extra{pair[1], CURL_WAIT_POLLIN, 0};
    int ready = -1;
    Require(curl_multi_wait_nid_postfix(multi, &extra, 1, 100, &ready) == CURLM_OK && ready == 1 && (extra.revents & CURL_WAIT_POLLIN));
    extra.revents = 0;
    Require(curl_multi_poll_nid_postfix(multi, &extra, 1, 100, &ready) == CURLM_OK && ready == 1 && (extra.revents & CURL_WAIT_POLLIN));
    char byte = 0;
    Require(recv_nid_postfix(pair[1], &byte, 1, 0) == 1 && byte == 'x');
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
    guest_curl_waitfd invalid{99999, CURL_WAIT_POLLIN, 99};
    ready = 0;
    Require(curl_multi_wait_nid_postfix(multi, &invalid, 1, 100, &ready) == CURLM_OK && ready == 1 && invalid.revents == 0);
#ifndef _WIN32
    int pipe[2];
    Require(pipe_nid_postfix(pipe) == 0);
    Require(write_nid_postfix(pipe[1], "p", 1) == 1);
    guest_curl_waitfd pipeWait{pipe[0], CURL_WAIT_POLLIN, 0};
    Require(curl_multi_poll_nid_postfix(multi, &pipeWait, 1, 100, &ready) == CURLM_OK && ready == 1 && (pipeWait.revents & CURL_WAIT_POLLIN));
    Require(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0);
#endif
    Require(curl_multi_cleanup_nid_postfix(multi) == CURLM_OK);
    {
        Server server;
        State state;
        auto* easy = Easy(server.Url(), state, false);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_OPENSOCKETFUNCTION, RefuseOpen) == CURLE_OK);
        Require(curl_easy_perform_nid_postfix(easy) == CURLE_COULDNT_CONNECT);
        curl_easy_cleanup_nid_postfix(easy);
    }
    {
        Server server;
        State state;
        auto* easy = Easy(server.Url(), state, false);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_SOCKOPTFUNCTION, RefuseOption) == CURLE_OK);
        Require(curl_easy_perform_nid_postfix(easy) == CURLE_ABORTED_BY_CALLBACK);
        curl_easy_cleanup_nid_postfix(easy);
    }
    Require(Descriptors() == before);
}

static void BorrowedLeaseIdentity() {
    const auto before = Descriptors();
    const auto native = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    const auto temporary = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    Require(native != CURL_SOCKET_BAD && temporary != CURL_SOCKET_BAD);
    std::shared_ptr<void> owner;
    const int descriptor = KernelBorrowSocketNative_nid_no_patch(static_cast<std::uintptr_t>(native), 2, 2, false, &owner);
    Require(descriptor >= 0 && Describes(descriptor));
    Require(KernelReleaseBorrowedSocket_nid_no_patch(descriptor, &owner));
    Require(!Describes(descriptor));
    NativeClose(temporary);
    const int reused = socket_nid_postfix(2, 2, IPPROTO_UDP);
    Require(reused == descriptor);
    Require(!KernelReleaseBorrowedSocket_nid_no_patch(descriptor, &owner));
    Require(Describes(reused));
    Require(close_nid_postfix(reused) == 0);
    owner.reset();
    int type = 0;
#ifdef _WIN32
    int length = sizeof(type);
#else
    socklen_t length = sizeof(type);
#endif
    Require(::getsockopt(native, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&type), &length) == 0 && type == SOCK_DGRAM);
    NativeClose(native);
    Require(Descriptors() == before);
}

#ifndef _WIN32
static void BorrowedUnixReadiness() {
    const auto before = Descriptors();
    int pair[2];
    Require(::socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    std::shared_ptr<void> owner;
    const int descriptor = KernelBorrowSocketNative_nid_no_patch(pair[0], 1, 1, false, &owner);
    Require(descriptor >= 0);
    GuestPollDescriptor poll{descriptor, 1, 0};
    Require(poll_nid_postfix(&poll, 1, 0) == 0 && poll.revents == 0);
    Require(::send(pair[1], "q", 1, MSG_NOSIGNAL) == 1);
    Require(poll_nid_postfix(&poll, 1, 100) == 1 && (poll.revents & 1));
    char byte;
    Require(recv_nid_postfix(descriptor, &byte, 1, 0) == 1 && byte == 'q');
    Require(send_nid_postfix(descriptor, "r", 1, 0) == 1);
    Require(::recv(pair[1], &byte, 1, 0) == 1 && byte == 'r');
    Require(KernelReleaseBorrowedSocket_nid_no_patch(descriptor, &owner));
    owner.reset();
    NativeClose(pair[0]);
    NativeClose(pair[1]);
    Require(Descriptors() == before);
}
#endif

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    Require(curl_global_init_nid_postfix(CURL_GLOBAL_DEFAULT) == CURLE_OK);
    BorrowedLeaseIdentity();
#ifndef _WIN32
    BorrowedUnixReadiness();
#endif
    SocketTransfer(true);
    SocketTransfer(false);
    FdsetTransfer();
    ConnectOnly();
    CallbackOwnership();
    ErrorsAndExtraDescriptors();
    curl_global_cleanup_nid_postfix();
}
