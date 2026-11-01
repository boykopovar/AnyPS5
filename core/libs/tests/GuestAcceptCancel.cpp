#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::exit(1);
}

template <class TPredicate>
static bool Observe(TPredicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::yield();
    }
    return true;
}

static std::size_t NativeDescriptorCount() {
#ifdef _WIN32
    DWORD count = 0;
    Require(GetProcessHandleCount(GetCurrentProcess(), &count) != 0, "count native handles");
    return count;
#else
    std::size_t count = 0;
    const auto limit = ::sysconf(_SC_OPEN_MAX);
    Require(limit > 0, "native descriptor limit");
    for (int descriptor = 0; descriptor < limit; ++descriptor)
        if (::fcntl(descriptor, F_GETFD) >= 0) ++count;
    return count;
#endif
}

struct State {
    int listener = -1;
    int accepted = -1;
    int error = 0;
    int installFault = 0;
    std::size_t nativeBefore = 0;
    bool countDisposal = false;
    PthreadKey key = -1;
    bool disabled = false;
    bool pending = false;
    bool alias = false;
    bool handoff = false;
    bool jump = false;
    bool flags = false;
    bool handoffPending = false;
    bool handoffReuse = false;
    alignas(16) std::array<std::uint64_t, 12> jumpBuffer{};
    std::array<unsigned char, 16> peer{};
    std::uint32_t peerSize = peer.size();
    int acceptedFlags = -1;
    int acceptedDescriptorFlags = -1;
    std::atomic<bool> observed{false};
    std::atomic<bool> ready{false};
    std::atomic<bool> release{false};
    std::atomic<bool> returned{false};
    std::atomic<int> cleanups{0};
    std::atomic<int> destructors{0};
};

static void APS5_VABI Cleanup(void* opaque) {
    auto& state = *static_cast<State*>(opaque);
    if (state.countDisposal) Require(NativeDescriptorCount() == state.nativeBefore, "accepted native resource survived until cleanup");
    Require(state.cleanups.fetch_add(1) == 0 && state.destructors.load() == 0, "cleanup ordering/count");
    if (state.accepted >= 0) Require(close_nid_postfix(state.accepted) == 0, "close accepted socket");
}

static void APS5_VABI Destroy(void* opaque) {
    auto& state = *static_cast<State*>(opaque);
    if (state.countDisposal) Require(NativeDescriptorCount() == state.nativeBefore, "accepted native resource survived until TSD");
    Require(state.cleanups.load() == 1 && state.destructors.fetch_add(1) == 0, "TSD ordering/count");
}

static thread_local State* accepting;

static void LeaveHandoff() {
    if (accepting->jump) longjmp_nid_postfix(accepting->jumpBuffer.data(), 1);
    if (accepting->handoffReuse) {
        Require(close_nid_postfix(accepting->listener) == 0, "close listener in delivery");
        const int replacement = socket_nid_postfix(2, 1, 0);
        Require(replacement >= 0, "delivery replacement");
        if (replacement != accepting->listener) {
            Require(dup2_nid_postfix(replacement, accepting->listener) == accepting->listener, "delivery descriptor reuse");
            Require(close_nid_postfix(replacement) == 0, "release delivery duplicate");
        }
    }
    Require(pthread_cancel_nid_postfix(pthread_self_nid_postfix()) == 0, "self cancel accepted handoff");
    if (accepting->handoffPending) return;
    scePthreadTestcancel();
    Require(false, "handoff cancellation returned");
}

static void ObserveHandoff() {
    accepting->observed.store(true);
    if (accepting->installFault) {
        GuestSocketAcceptInstallFaultForTest_nid_no_patch(accepting->installFault);
        return;
    }
    Require(ThreadCancel::DeferIoCallback(LeaveHandoff, nullptr), "defer accepted handoff");
}

static void* APS5_VABI Acceptor(void* opaque) {
    auto& state = *static_cast<State*>(opaque);
    if (state.disabled) Require(scePthreadSetcancelstate(GUEST_CANCEL_DISABLE, nullptr) == 0, "disable");
    Require(scePthreadSetspecific(state.key, &state) == 0, "set TSD");
    CleanupHandler cleanup{};
    __pthread_cleanup_push_imp_nid_postfix(Cleanup, &state, &cleanup);
    accepting = &state;
    if (state.handoff) GuestSocketAcceptObserverForTest_nid_no_patch(ObserveHandoff);
    if (state.countDisposal) {
        GuestSockets::CurrentWaker();
        {
            ThreadCancel::IoRestart warmup(false);
            ThreadCancel::IoPoint point(warmup);
        }
        state.nativeBefore = NativeDescriptorCount();
    }
    state.ready.store(true);
    if (state.pending) Require(Observe([&] { return state.release.load(); }), "pending release");
    if (!state.jump || setjmp_nid_postfix(state.jumpBuffer.data()) == 0)
        state.accepted = state.alias ? _accept_nid_postfix(state.listener, nullptr, nullptr) :
            accept_nid_postfix(state.listener, state.peer.data(), &state.peerSize);
    GuestSocketAcceptObserverForTest_nid_no_patch(nullptr);
    if (state.accepted >= 0) {
        state.acceptedFlags = fcntl_nid_postfix(state.accepted, 3);
        state.acceptedDescriptorFlags = fcntl_nid_postfix(state.accepted, 1);
    }
    state.error = *__error_nid_postfix();
    state.returned.store(true);
    if (state.disabled) {
        Require(state.accepted >= 0, "disabled accept lost connection");
        Require(scePthreadSetcancelstate(GUEST_CANCEL_ENABLE, nullptr) == 0, "enable");
        scePthreadTestcancel();
    }
    __pthread_cleanup_pop_imp_nid_postfix(1);
    return reinterpret_cast<void*>(std::uintptr_t{7});
}

int main(int argc, char** argv) {
    Require(argc == 2, "expected scenario");
    const std::string scenario = argv[1];
    State state;
    state.disabled = scenario == "disabled";
    state.pending = scenario == "pending";
    state.alias = scenario == "alias";
    state.handoff = scenario.starts_with("handoff_");
    state.handoffPending = scenario == "handoff_pending";
    state.handoffReuse = scenario == "handoff_reuse";
    state.jump = scenario == "handoff_jump";
    state.flags = scenario == "flags";
    state.installFault = scenario == "install_reserve_failure" ? 1 : scenario == "install_map_failure" ? 2 : 0;
    state.countDisposal = state.installFault || scenario == "handoff_cancel" || scenario == "handoff_jump";
    state.handoff = state.handoff || state.installFault;
    state.listener = socket_nid_postfix(2, 1, 0);
    Require(state.listener >= 0, "create listener");
    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(state.listener, address.data(), address.size()) == 0, "bind");
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(state.listener, address.data(), &size) == 0, "address");
    Require(listen_nid_postfix(state.listener, 4) == 0, "listen");
    const int client = socket_nid_postfix(2, 1, 0);
    Require(client >= 0, "client");
    if (scenario == "nonblocking" || state.flags) Require(fcntl_nid_postfix(state.listener, 4, 6) == 0, "nonblocking");
    Require(fcntl_nid_postfix(state.listener, 2, 1) == 0, "listener close-on-exec");
    if (state.flags) Require(connect_nid_postfix(client, address.data(), address.size()) == 0, "queue connection");
    Require(scePthreadKeyCreate(&state.key, reinterpret_cast<pthread_key_destructor_func_t>(Destroy)) == 0, "TSD key");
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Acceptor, &state, nullptr) == 0, "create acceptor");
    Require(Observe([&] { return state.ready.load(); }), "acceptor admission");
    if (!state.pending && !state.disabled && scenario != "nonblocking" && !state.flags) {
        Require(Observe([&] {
            std::lock_guard lock(thread->cancelLock);
            return thread->cancelIo != nullptr;
        }), "accept wait registration");
    }
    const bool cancel = scenario == "cancel" || scenario == "alias" || state.disabled || state.pending;
    if (cancel) Require(pthread_cancel_nid_postfix(thread) == 0, "cancel");
    state.release.store(true);
    if (scenario == "complete" || state.disabled || state.handoff) Require(connect_nid_postfix(client, address.data(), address.size()) == 0, "complete accept");
    int replacement[2]{-1, -1};
    if (scenario == "reuse") Require(pipe_nid_postfix(replacement) == 0, "replacement pipe");
    if (scenario == "close" || scenario == "reuse") Require(close_nid_postfix(state.listener) == 0, "close waiting listener");
    if (scenario == "reuse") {
        if (replacement[0] != state.listener) Require(dup2_nid_postfix(replacement[0], state.listener) == state.listener, "reuse listener slot");
        Require(write_nid_postfix(replacement[1], "r", 1) == 1, "seed replacement");
    }
    Require(Observe([&] { return state.destructors.load() == 1; }), "accept did not finish");
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == 0, "join");
    Require(result == ((cancel || (state.handoff && !state.installFault && !state.jump && !state.handoffPending)) ? GUEST_CANCELED : reinterpret_cast<void*>(std::uintptr_t{7})), "thread result");
    Require(state.cleanups.load() == 1 && state.destructors.load() == 1, "cleanup counts");
    if (scenario == "nonblocking") Require(state.accepted == -1 && state.error == 35, "nonblocking errno");
    if (scenario == "complete" || state.flags) {
        Require(state.accepted >= 0 && state.peerSize == 16 && state.peer[0] == 16 && state.peer[1] == 2, "completed accept peer");
        Require(state.acceptedDescriptorFlags == 0, "accepted close-on-exec inherited");
        Require((state.acceptedFlags & 4) == (state.flags ? 4 : 0), "accepted nonblocking flags");
    }
    if (state.installFault) Require(state.accepted == -1 && state.error == (state.installFault == 1 ? 24 : 12), "accepted installation failure errno");
    if (state.handoff) {
        Require(state.observed.load() && (state.handoffPending ? state.accepted >= 0 : state.accepted == -1), "accepted ownership handoff not reached");
        if (state.handoffReuse) Require(fcntl_nid_postfix(state.listener, 3) >= 0, "disposal closed replacement");
        char byte = 0;
        Require(Observe([&] { return recv_nid_postfix(client, &byte, 1, 0x80) == 0; }), "unpublished accepted socket leaked");
    }
    if (scenario == "reuse") {
        char byte = 0;
        Require(read_nid_postfix(state.listener, &byte, 1) == 1 && byte == 'r', "retired accept damaged replacement");
        if (replacement[0] != state.listener) Require(close_nid_postfix(replacement[0]) == 0, "close replacement read end");
        Require(close_nid_postfix(replacement[1]) == 0, "close replacement write end");
    }
    if (scenario == "close" || scenario == "reuse") Require(state.accepted == -1 && state.error == 4, "closed operation errno");
    if (scenario == "cancel" || scenario == "alias" || state.pending) {
        Require(connect_nid_postfix(client, address.data(), address.size()) == 0, "listener survives cancellation");
        const int accepted = accept_nid_postfix(state.listener, nullptr, nullptr);
        Require(accepted >= 0 && close_nid_postfix(accepted) == 0, "accept after cancellation");
    }
    if (scenario != "close") Require(close_nid_postfix(state.listener) == 0, "close listener");
    Require(close_nid_postfix(client) == 0 && scePthreadKeyDelete(state.key) == 0, "release resources");
}
