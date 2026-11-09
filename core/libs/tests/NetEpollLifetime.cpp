#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
static int InterceptPoll(WSAPOLLFD* descriptors, ULONG count, INT timeout);
#define WSAPoll InterceptPoll
#else
#include <poll.h>
static int InterceptPoll(pollfd* descriptors, nfds_t count, int timeout);
#define poll InterceptPoll
#endif
#include "../prx/libSceNet/Export.cpp"
#ifdef _WIN32
#undef WSAPoll
#else
#undef poll
#endif
#include <future>
#include <source_location>

static void Require(bool value, const std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "requirement failed at %s:%u\n", location.file_name(), location.line());
    std::abort();
}

struct PollGate {
    std::promise<void> arrived;
    std::future<void> released;
    int result = 0;
};

static std::atomic<PollGate*> pollGate{nullptr};

#ifdef _WIN32
static int InterceptPoll(WSAPOLLFD* descriptors, ULONG count, INT timeout) {
    const int result = ::WSAPoll(descriptors, count, timeout);
#else
static int InterceptPoll(pollfd* descriptors, nfds_t count, int timeout) {
    const int result = ::poll(descriptors, count, timeout);
#endif
    if (auto* gate = pollGate.exchange(nullptr)) {
        gate->result = result;
        gate->arrived.set_value();
        gate->released.wait();
    }
    return result;
}

class ReuseSlot {
public:
#ifndef _WIN32
    ReuseSlot() : descriptor(::open("/dev/null", O_RDONLY | O_CLOEXEC)) { Require(descriptor >= 0); }
    ~ReuseSlot() { if (descriptor >= 0) ::close(descriptor); }
    void Release() { Require(::close(descriptor) == 0); descriptor = -1; }
private:
    int descriptor;
#else
    void Release() {}
#endif
};

static constexpr std::uint64_t OldData = 0x123456789abcdef0;
static constexpr std::uint64_t NewData = 0xfedcba9876543210;

static int Socket() {
    const int descriptor = sceNetSocket("epoll-lifetime", 2, 2, 17);
    Require(descriptor >= 0);
    return descriptor;
}

static std::array<std::uint8_t, 16> Bind(int descriptor) {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(descriptor, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(descriptor, address.data(), &size) == 0 && size == address.size());
    return address;
}

static void Send(int sender, const std::array<std::uint8_t, 16>& address) {
    const char payload[] = "epoll-lifetime";
    Require(sceNetSendto(sender, payload, sizeof(payload), 0, address.data(), address.size()) == sizeof(payload));
}

static void Register(int epoll, int descriptor, int operation, std::uint64_t data, std::uint32_t events = 1) {
    NetEpollEvent event{};
    event.events = events;
    event.ident = 0xaaaaaaaaaaaaaaaa;
    event.reserved = 0x55555555;
    event.data.u64 = data;
    Require(sceNetEpollControl(epoll, operation, descriptor, &event) == 0);
}

static void CheckEvent(const NetEpollEvent& event, int descriptor, std::uint64_t data, std::uint32_t events = 1) {
    Require(event.ident == static_cast<std::uint64_t>(descriptor));
    Require(event.data.u64 == data && event.events == events && event.reserved == 0);
}

static void CheckReuse() {
    ReuseSlot slot;
    const int original = Socket();
    const auto retained = g_socks.at(original).native;
    const int sender = Socket();
    std::array<int, 2> epolls{sceNetEpollCreate("reuse-a", 0), sceNetEpollCreate("reuse-b", 0)};
    for (const int epoll : epolls) {
        Require(epoll >= 0);
        Register(epoll, original, 1, OldData);
    }
    Require(sceNetSocketClose(original) == 0);
    slot.Release();
    const int replacement = Socket();
    Require(replacement == original);
    Send(sender, Bind(replacement));
    for (const int epoll : epolls) {
        NetEpollEvent event{};
        Require(sceNetEpollWait(epoll, &event, 1, 0) == 0);
        Register(epoll, replacement, 1, NewData);
        Require(sceNetEpollWait(epoll, &event, 1, 1000000) == 1);
        CheckEvent(event, replacement, NewData);
        Require(sceNetEpollDestroy(epoll) == 0);
    }
    Require(sceNetSocketClose(replacement) == 0 && sceNetSocketClose(sender) == 0);
}

static void CheckControl() {
    const int descriptor = Socket();
    const int epoll = sceNetEpollCreate("control", 0);
    Require(epoll >= 0);
    Require(sceNetEpollControl(epoll, 3, descriptor, nullptr) == static_cast<int>(0x80410102));
    Require(*sceNetErrnoLoc() == 2);
    Register(epoll, descriptor, 1, OldData);
    Require(sceNetSocketClose(descriptor) == 0);
    NetEpollEvent event{};
    event.events = 1;
    for (const int closed : {descriptor, -1}) {
        for (const int operation : {3, 1, 2}) {
            Require(sceNetEpollControl(epoll, operation, closed, operation == 3 ? nullptr : &event) == static_cast<int>(0x80410109));
            Require(*sceNetErrnoLoc() == 9);
        }
    }
    Require(sceNetEpollDestroy(epoll) == 0);
}

struct WaitResult {
    int count = -1;
    int error = 0;
    NetEpollEvent event{};
};

static void CheckWait(const std::string& operation) {
    ReuseSlot slot;
    int receiver = Socket();
    const int sender = Socket();
    Send(sender, Bind(receiver));
    const int epoll = sceNetEpollCreate("inflight", 0);
    Require(epoll >= 0);
    Register(epoll, receiver, 1, OldData);
    std::promise<void> release;
    PollGate gate{{}, release.get_future()};
    auto arrived = gate.arrived.get_future();
    pollGate.store(&gate);
    auto waiter = std::async(std::launch::async, [epoll] {
        WaitResult result;
        result.count = sceNetEpollWait(epoll, &result.event, 1, 200000);
        result.error = *sceNetErrnoLoc();
        return result;
    });
    Require(arrived.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    Require(gate.result == 1);
    if (operation == "close") {
        const int original = receiver;
        Require(sceNetSocketClose(original) == 0);
        slot.Release();
        receiver = Socket();
        Require(receiver == original);
        Send(sender, Bind(receiver));
        Register(epoll, receiver, 1, NewData);
    } else if (operation == "delete" || operation == "replace") {
        Require(sceNetEpollControl(epoll, 3, receiver, nullptr) == 0);
        if (operation == "replace") Register(epoll, receiver, 1, NewData);
    } else if (operation == "modify") {
        Register(epoll, receiver, 2, NewData, 2);
    } else if (operation == "abort") {
        Require(sceNetEpollAbort(epoll, 0) == 0);
    } else {
        Require(operation == "destroy");
        Require(sceNetEpollDestroy(epoll) == 0);
    }
    release.set_value();
    Require(waiter.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    const auto result = waiter.get();
    if (operation == "delete") {
        Require(result.count == 0);
    } else if (operation == "abort" || operation == "destroy") {
        Require(result.count < 0 && result.error == (operation == "abort" ? 53 : 9));
    } else {
        Require(result.count == 1);
        CheckEvent(result.event, receiver, NewData, operation == "modify" ? 2 : 1);
    }
    if (operation != "destroy") Require(sceNetEpollDestroy(epoll) == 0);
    Require(sceNetSocketClose(receiver) == 0 && sceNetSocketClose(sender) == 0);
}

int main(int argc, char** argv) {
    Require(argc == 2);
    Require(sceNetInit_nid_postfix() == 0);
    for (int iteration = 0; iteration != 3; ++iteration) {
        if (std::string(argv[1]) == "reuse") CheckReuse();
        else if (std::string(argv[1]) == "control") CheckControl();
        else CheckWait(argv[1]);
    }
    std::printf("epoll lifetime %s: 3 rounds passed\n", argv[1]);
}
