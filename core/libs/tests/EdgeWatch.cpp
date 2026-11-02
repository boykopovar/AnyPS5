#include "prx/libkernel/Equeue/include/EdgeWatch.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <future>
#include <poll.h>
#include <stdexcept>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

extern "C" [[noreturn]] void NotImplemented_nid_no_patch(const char* name) {
    throw std::runtime_error(name);
}

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Pair {
    explicit Pair(bool socket) {
        Require((socket ? ::socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors.data()) : ::pipe(descriptors.data())) == 0, "cannot create descriptor pair");
    }
    ~Pair() {
        for (int descriptor : descriptors) if (descriptor >= 0) ::close(descriptor);
    }
    std::array<int, 2> descriptors{-1, -1};
};

bool Wait(int descriptor, int milliseconds) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    for (;;) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (remaining < 0) return false;
        pollfd entry{descriptor, POLLIN, 0};
        const int result = ::poll(&entry, 1, static_cast<int>(remaining + 1));
        if (result < 0 && errno == EINTR) continue;
        Require(result >= 0, "poll failed");
        Require((entry.revents & (POLLERR | POLLNVAL | POLLHUP)) == 0, "watch descriptor failed");
        return (entry.revents & POLLIN) != 0;
    }
}

void Edge(const std::shared_ptr<EdgeWatch::Watch>& watch) {
    Require(Wait(watch->Descriptor(), 5000), "edge did not wake the watch descriptor");
    Require(watch->TakeEdge(), "readable watch did not yield an edge");
}

void Quiet(const std::shared_ptr<EdgeWatch::Watch>& watch) {
    Require(!watch->TakeEdge(), "edge repeated without new activity");
    Require(!Wait(watch->Descriptor(), 50), "idle watch descriptor remained readable");
}

void Send(int descriptor, std::size_t count) {
    std::array<char, 8> bytes{};
    Require(::write(descriptor, bytes.data(), count) == static_cast<ssize_t>(count), "write failed");
}

void Read(int descriptor, std::size_t count) {
    std::array<char, 8> bytes{};
    Require(::read(descriptor, bytes.data(), count) == static_cast<ssize_t>(count), "read failed");
}

void ReadEdges(bool socket) {
    Pair pair(socket);
    auto watch = EdgeWatch::Watch::Open(pair.descriptors[0], false);
    Quiet(watch);
    Send(pair.descriptors[1], 2);
    Edge(watch);
    Quiet(watch);
    Send(pair.descriptors[1], 1);
    Edge(watch);
    Quiet(watch);
    Read(pair.descriptors[0], 3);
    Quiet(watch);
    std::promise<void> entered;
    auto started = entered.get_future();
    auto waiter = std::async(std::launch::async, [&] {
        entered.set_value();
        return Wait(watch->Descriptor(), 5000);
    });
    started.wait();
    const bool blocked = waiter.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout;
    Send(pair.descriptors[1], 1);
    const bool woke = waiter.get();
    Require(blocked, "blocking wait returned before new activity");
    Require(woke, "blocking wait did not wake after new activity");
    Require(watch->TakeEdge(), "blocking wait lost the edge");
    Require(::close(pair.descriptors[1]) == 0, "close writer failed");
    pair.descriptors[1] = -1;
    Edge(watch);
    Quiet(watch);
    Read(pair.descriptors[0], 1);
    watch->TakeEdge();
    char byte;
    Require(::read(pair.descriptors[0], &byte, 1) == 0, "read did not report EOF");
    watch->TakeEdge();
    Quiet(watch);
}

void WriteEdges() {
    Pair pair(true);
    Require(::fcntl(pair.descriptors[1], F_SETFL, O_NONBLOCK) == 0, "cannot make socket nonblocking");
    auto watch = EdgeWatch::Watch::Open(pair.descriptors[1], true);
    Edge(watch);
    Quiet(watch);
    std::array<char, 8192> bytes{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    ssize_t result;
    do {
        result = ::write(pair.descriptors[1], bytes.data(), bytes.size());
        Require(std::chrono::steady_clock::now() < deadline, "socket did not fill");
    } while (result >= 0 || errno == EINTR);
    Require(errno == EAGAIN || errno == EWOULDBLOCK, "socket fill failed");
    while (watch->TakeEdge()) Require(std::chrono::steady_clock::now() < deadline, "write edge did not clear");
    Quiet(watch);
    Require(::fcntl(pair.descriptors[0], F_SETFL, O_NONBLOCK) == 0, "cannot make reader nonblocking");
    do {
        result = ::read(pair.descriptors[0], bytes.data(), bytes.size());
        Require(std::chrono::steady_clock::now() < deadline, "socket did not drain");
    } while (result > 0 || (result < 0 && errno == EINTR));
    Require(result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK), "socket drain failed");
    Edge(watch);
    Quiet(watch);
}

void Lifecycle() {
    Pair pair(false);
    int descriptor;
    {
        auto watch = EdgeWatch::Watch::Open(pair.descriptors[0], false);
        descriptor = watch->Descriptor();
        const int flags = ::fcntl(descriptor, F_GETFD);
        Require(flags >= 0 && (flags & FD_CLOEXEC) != 0, "watch lacks close-on-exec");
    }
    Require(::fcntl(descriptor, F_GETFD) == -1 && errno == EBADF, "watch descriptor leaked");
    Send(pair.descriptors[1], 1);
    Read(pair.descriptors[0], 1);
}

void Errors() {
    bool threw = false;
    try {
        EdgeWatch::Watch::Open(-1, false);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "invalid descriptor was accepted");
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "expected test mode");
        const std::string_view mode(argv[1]);
        if (mode == "socket") ReadEdges(true);
        else if (mode == "pipe") ReadEdges(false);
        else if (mode == "write") WriteEdges();
        else if (mode == "lifecycle") Lifecycle();
        else if (mode == "errors") Errors();
        else throw std::runtime_error("unknown test mode");
        std::printf("PASS: %s\n", argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
