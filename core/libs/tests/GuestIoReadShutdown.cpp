#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::exit(1);
}

template<class TPredicate>
static bool Observe(TPredicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::yield();
    }
    return true;
}

struct State {
    int input = -1;
    std::string api;
    bool disabled = false;
    bool waitAll = false;
    int expected = 0;
    PthreadKey key = -1;
    std::atomic<int> admitted{0};
    std::atomic<bool> enter{false};
    std::atomic<bool> returned{false};
    std::atomic<bool> release{false};
    std::atomic<int> cleanups{0};
    std::atomic<int> destructors{0};
    std::int64_t result = -2;
    std::uint32_t addressLength = 28;
    std::array<char, 4> bytes{};
};

static State* current = nullptr;

static void BeforeWait(std::size_t progress) noexcept {
    if (progress < static_cast<std::size_t>(current->expected)) return;
    current->admitted.fetch_add(1);
    Require(Observe([] { return current->enter.load(); }), "release the admitted receive");
}

static void Read(State& state) {
    const auto length = state.waitAll ? state.bytes.size() : 1;
    const int flags = state.waitAll ? 0x40 : 0;
    KernelIovec vector{state.bytes.data(), length};
    std::array<unsigned char, 28> address{};
    GuestMsghdr message{};
    message.name = address.data();
    message.nameLength = address.size();
    message.iov = &vector;
    message.iovLength = 1;
    if (state.api == "read") state.result = read_nid_postfix(state.input, state.bytes.data(), length);
    else if (state.api == "alias") state.result = _read_nid_postfix(state.input, state.bytes.data(), length);
    else if (state.api == "readv") state.result = readv_nid_postfix(state.input, &vector, 1);
    else if (state.api == "readv_alias") state.result = _readv_nid_postfix(state.input, &vector, 1);
    else if (state.api == "recv") state.result = recv_nid_postfix(state.input, state.bytes.data(), length, flags);
    else if (state.api == "recvfrom") state.result = recvfrom_nid_postfix(state.input, state.bytes.data(), length, flags, address.data(), &state.addressLength);
    else if (state.api == "recvfrom_alias") state.result = _recvfrom_nid_postfix(state.input, state.bytes.data(), length, flags, address.data(), &state.addressLength);
    else if (state.api == "recvmsg") state.result = recvmsg_nid_postfix(state.input, &message, flags);
    else if (state.api == "recvmsg_alias") state.result = _recvmsg_nid_postfix(state.input, &message, flags);
    else if (state.api == "sce_read") state.result = sceKernelRead(state.input, state.bytes.data(), length);
    else if (state.api == "sce_readv") state.result = sceKernelReadv(state.input, &vector, 1);
    else Require(false, "unknown receive API");
    if (state.api.starts_with("recvmsg")) state.addressLength = message.nameLength;
    state.returned.store(true);
}

static void APS5_VABI Cleanup(void* opaque) {
    auto& state = *static_cast<State*>(opaque);
    Require(state.cleanups.fetch_add(1) == 0 && state.destructors.load() == 0, "cleanup once before TSD");
}

static void APS5_VABI Destroy(void* opaque) {
    auto& state = *static_cast<State*>(opaque);
    Require(state.cleanups.load() == 1 && state.destructors.fetch_add(1) == 0, "TSD once after cleanup");
}

static void* APS5_VABI Reader(void* opaque) {
    auto& state = *static_cast<State*>(opaque);
    if (state.disabled) Require(scePthreadSetcancelstate(GUEST_CANCEL_DISABLE, nullptr) == 0, "disable cancellation");
    Require(scePthreadSetspecific(state.key, &state) == 0, "set TSD");
    CleanupHandler cleanup{};
    __pthread_cleanup_push_imp_nid_postfix(Cleanup, &state, &cleanup);
    Read(state);
    if (state.disabled) {
        Require(Observe([&] { return state.release.load(); }), "release pending cancellation after EOF");
        Require(scePthreadSetcancelstate(GUEST_CANCEL_ENABLE, nullptr) == 0, "enable pending cancellation");
        scePthreadTestcancel();
    }
    __pthread_cleanup_pop_imp_nid_postfix(1);
    return reinterpret_cast<void*>(std::uintptr_t{7});
}

static void Connect(bool datagram, int (&endpoints)[2]) {
    const int listener = socket_nid_postfix(2, datagram ? 2 : 1, 0);
    endpoints[1] = socket_nid_postfix(2, datagram ? 2 : 1, 0);
    Require(listener >= 0 && endpoints[1] >= 0, "create loopback sockets");
    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0, "bind loopback receiver");
    std::uint32_t length = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &length) == 0, "get loopback receiver address");
    if (!datagram) Require(listen_nid_postfix(listener, 1) == 0, "listen on loopback");
    Require(connect_nid_postfix(endpoints[1], address.data(), length) == 0, "connect loopback peer");
    if (datagram) {
        length = address.size();
        Require(getsockname_nid_postfix(endpoints[1], address.data(), &length) == 0, "get datagram peer address");
        Require(connect_nid_postfix(listener, address.data(), length) == 0, "connect datagram receiver");
        endpoints[0] = listener;
    } else {
        endpoints[0] = accept_nid_postfix(listener, nullptr, nullptr);
        Require(endpoints[0] >= 0 && close_nid_postfix(listener) == 0, "accept loopback connection");
    }
}

int main(int argc, char** argv) {
    Require(argc == 4, "network, API and scenario required");
    State state;
    state.api = argv[2];
    const std::string scenario = argv[3];
    state.disabled = scenario == "disabled";
    state.waitAll = scenario == "waitall";
    const bool native = scenario == "native";
    const bool cancel = scenario == "cancel";
    const bool parallel = scenario == "parallel";
    const bool datagram = std::string(argv[1]) == "datagram";
    int endpoints[2]{};
    Connect(datagram, endpoints);
    state.input = endpoints[0];
    int alias = -1;
    if (scenario == "alias") {
        alias = dup_nid_postfix(state.input);
        Require(alias >= 0, "duplicate receiver");
        state.input = alias;
    }
    if (scenario == "timeout") {
        const std::array<std::int64_t, 2> timeout{0, 200000};
        Require(setsockopt_nid_postfix(state.input, 0xffff, 0x1006, timeout.data(), sizeof(timeout)) == 0, "set receive timeout");
    }
#ifdef _WIN32
    if (state.waitAll) {
        state.expected = 1;
        Require(write_nid_postfix(endpoints[1], "x", 1) == 1, "seed known partial progress");
    }
#endif
    current = &state;
    GuestSocketReadWaitObserverForTest_nid_no_patch(BeforeWait);
    Require(scePthreadKeyCreate(&state.key, reinterpret_cast<pthread_key_destructor_func_t>(Destroy)) == 0, "create TSD key");
    Pthread reader = nullptr;
    Pthread second = nullptr;
    State secondary;
    std::thread host;
    if (native) host = std::thread([&] { Read(state); });
    else Require(scePthreadCreate(&reader, nullptr, Reader, &state, nullptr) == 0, "create guest receiver");
    if (parallel) {
        secondary.input = state.input;
        secondary.api = state.api;
        secondary.key = state.key;
        Require(scePthreadCreate(&second, nullptr, Reader, &secondary, nullptr) == 0, "create second receiver");
    }
    Require(Observe([&] { return state.admitted.load() >= (parallel ? 2 : 1); }), "receive did not reach the wait boundary");
    std::thread requester;
    if (cancel) requester = std::thread([&] { Require(pthread_cancel_nid_postfix(reader) == 0, "cancel blocked receiver"); });
    else {
        if (state.disabled) Require(pthread_cancel_nid_postfix(reader) == 0, "queue disabled cancellation");
        Require(shutdown_nid_postfix(endpoints[0], 0) == 0, "shut reading through the original descriptor");
    }
    state.enter.store(true);
    const auto completed = [&] {
        const bool first = native ? state.returned.load() : reader->_finished.load() || (state.disabled && state.returned.load());
        return first && (!second || second->_finished.load());
    };
    const bool prompt = Observe(completed);
    if (!prompt) {
#ifdef _WIN32
        CancelIoEx(reinterpret_cast<HANDLE>(GuestSockets::NativeDescriptor(state.input)), nullptr);
#endif
        if (!datagram) shutdown_nid_postfix(endpoints[1], 1);
        if (reader && !state.disabled) pthread_cancel_nid_postfix(reader);
        if (second) pthread_cancel_nid_postfix(second);
        Require(Observe(completed), "failure cleanup did not retire the receive");
    }
    if (!cancel) Require(state.result == state.expected, "shutdown lost EOF or known positive progress");
    if (state.expected != 0) Require(state.bytes[0] == 'x', "shutdown changed transferred data");
    if (!cancel && state.result == 0 && state.api.starts_with("recvfrom")) Require(state.addressLength == 0, "shutdown recvfrom must report an empty address");
    if (!cancel && state.result == 0 && state.api.starts_with("recvmsg")) Require(state.addressLength == 0, "shutdown recvmsg must report an empty address");
    state.release.store(true);
    if (requester.joinable()) requester.join();
    if (native) host.join();
    else {
        Require(Observe([&] { return reader->_finished.load(); }), "receiver did not retire after failure cleanup");
        void* result = nullptr;
        Require(scePthreadJoin(reader, &result) == 0, "join receiver");
        Require(result == (cancel || state.disabled ? GUEST_CANCELED : reinterpret_cast<void*>(std::uintptr_t{7})), "shutdown and cancellation outcomes were confused");
        Require(state.cleanups.load() == 1 && state.destructors.load() == 1, "cleanup and TSD counts");
    }
    if (second) {
        void* result = nullptr;
        Require(scePthreadJoin(second, &result) == 0, "join second receiver");
        Require(result == reinterpret_cast<void*>(std::uintptr_t{7}) && secondary.result == 0, "shutdown did not finish both receivers");
        Require(secondary.cleanups.load() == 1 && secondary.destructors.load() == 1, "second receiver cleanup and TSD counts");
    }
    Require(prompt, "shutdown needed unrelated peer activity or cancellation");
    GuestSocketReadWaitObserverForTest_nid_no_patch(nullptr);
    Require(scePthreadKeyDelete(state.key) == 0, "delete TSD key");
    if (alias >= 0) Require(close_nid_postfix(alias) == 0, "close receiver alias");
    Require(close_nid_postfix(endpoints[0]) == 0 && close_nid_postfix(endpoints[1]) == 0, "close endpoints");
}
