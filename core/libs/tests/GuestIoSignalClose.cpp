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
#include <cstring>
#include <fstream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::exit(1);
}

template <class TPredicate>
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
    int replacement = -1;
    bool disabled = false;
    bool replace = true;
    bool leave = false;
    bool localJump = false;
    bool changeErrno = false;
    bool socket = false;
    std::intptr_t nativeInput = -1;
    alignas(16) std::array<std::uint64_t, 12> jump{};
    std::atomic<long> nativeId{0};
    std::atomic<bool> entering{false};
    std::atomic<bool> finished{false};
    std::atomic<int> handlers{0};
    int closed = -2;
    int replaced = -2;
    std::int64_t result = -2;
    int error = 0;
    char byte = 0;
    bool maskPreserved = false;
    std::thread::id reader;
    std::thread::id handler;
};

static State* state;

static void APS5_VABI SignalHandler(int) {
    state->handler = std::this_thread::get_id();
    if (state->localJump) {
        alignas(16) std::uint64_t local[12]{};
        if (setjmp_nid_postfix(local) == 0) longjmp_nid_postfix(local, 1);
    }
    if (state->changeErrno) Require(close_nid_postfix(-1) == -1 && *__error_nid_postfix() == 9, "handler changes errno");
    if (state->replace) {
        state->closed = close_nid_postfix(state->input);
        state->replaced = dup2_nid_postfix(state->replacement, state->input);
    }
    state->handlers.fetch_add(1);
    if (state->leave) longjmp_nid_postfix(state->jump.data(), 1);
}

static void APS5_VABI ExceptionHandler(int signal, void*) { SignalHandler(signal); }

static void* APS5_VABI Reader(void* opaque) {
    auto& reading = *static_cast<State*>(opaque);
    reading.reader = std::this_thread::get_id();
    if (reading.disabled) Require(scePthreadSetcancelstate(GUEST_CANCEL_DISABLE, nullptr) == 0, "disable cancellation");
#ifndef _WIN32
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGUSR2);
    Require(pthread_sigmask(SIG_BLOCK, &blocked, nullptr) == 0, "block native interrupt before read");
    sigset_t before;
    Require(pthread_sigmask(SIG_SETMASK, nullptr, &before) == 0, "get native mask before read");
    reading.nativeId.store(::syscall(SYS_gettid));
#endif
    reading.entering.store(true);
    if (reading.leave && setjmp_nid_postfix(reading.jump.data()) != 0) reading.result = -3;
    else reading.result = reading.socket ? recv_nid_postfix(reading.input, &reading.byte, 1, 0) : read_nid_postfix(reading.input, &reading.byte, 1);
    reading.error = *__error_nid_postfix();
#ifndef _WIN32
    sigset_t after;
    Require(pthread_sigmask(SIG_SETMASK, nullptr, &after) == 0, "get native mask after read");
    reading.maskPreserved = true;
    for (int signal = 1; signal < NSIG; ++signal)
        if (sigismember(&before, signal) != sigismember(&after, signal)) reading.maskPreserved = false;
#else
    reading.maskPreserved = true;
#endif
    reading.finished.store(true);
    return reinterpret_cast<void*>(std::uintptr_t{7});
}

#ifndef _WIN32
static bool NativeReadBlocked(const State& reading) {
    const auto native = reading.nativeId.load();
    if (!native) return false;
    std::ifstream input("/proc/self/task/" + std::to_string(native) + "/syscall");
    long call = -1;
    std::uint64_t descriptor = 0;
    input >> call >> std::hex >> descriptor;
    return input && ((call == SYS_read && descriptor == static_cast<unsigned>(reading.input)) ||
        (reading.socket && ((call == SYS_recvfrom && descriptor == static_cast<std::uint64_t>(reading.nativeInput)) || call == SYS_poll)));
}
#endif

static void RunCase(int argc, char** argv) {
    Require(argc == 3 || argc == 4, "expected signal source, cancellation mode and optional action");
    const std::string source = argv[1];
    const bool exception = source.starts_with("exception");
    const bool unregistered = std::strcmp(argv[2], "unregistered") == 0;
    State reading;
    state = &reading;
    reading.disabled = std::strcmp(argv[2], "disabled") == 0;
    reading.socket = source.ends_with("_socket");
    const bool starting = argc == 4 && std::strcmp(argv[3], "starting") == 0;
    reading.changeErrno = argc == 4 && std::strcmp(argv[3], "resume_errno") == 0;
    const bool resume = starting || reading.changeErrno || (argc == 4 && std::strcmp(argv[3], "resume") == 0);
    const bool interrupt = argc == 4 && std::strcmp(argv[3], "interrupt") == 0;
    const bool timeout = argc == 4 && std::strcmp(argv[3], "timeout") == 0;
    reading.leave = argc == 4 && std::strcmp(argv[3], "jump") == 0;
    reading.localJump = argc == 4 && std::strcmp(argv[3], "local_jump") == 0;
    reading.replace = !resume && !interrupt && !timeout;
    int original[2]{};
    int replacement[2]{};
    Require((reading.socket ? socketpair_nid_postfix(1, 1, 0, original) : pipe_nid_postfix(original)) == 0 &&
        pipe_nid_postfix(replacement) == 0, "create signal transports");
    reading.input = original[0];
    reading.replacement = replacement[0];
    if (reading.socket) reading.nativeInput = GuestSockets::NativeDescriptor(reading.input);
    if (timeout) {
        const std::array<std::int64_t, 2> duration{2, 0};
        Require(setsockopt_nid_postfix(reading.input, 0xffff, 0x1006, duration.data(), sizeof(duration)) == 0, "set interrupted receive timeout");
    }
    Require(write_nid_postfix(replacement[1], "n", 1) == 1, "seed replacement pipe");
    const int guestSignal = source.starts_with("fault_signal") ? 11 : 2;
    GuestHandler previous = nullptr;
    if (exception) Require(sceKernelInstallExceptionHandler(30, reinterpret_cast<void*>(ExceptionHandler)) == 0, "install exception handler");
    else if (interrupt) {
        const GuestSigaction action{reinterpret_cast<std::uintptr_t>(SignalHandler), 0, {}};
        Require(sigaction_nid_postfix(guestSignal, &action, nullptr) == 0, "install non-restarting handler");
    }
    else previous = signal_nid_postfix(guestSignal, SignalHandler);
    Pthread thread = nullptr;
    std::thread native;
    if (unregistered) native = std::thread([&] { Reader(&reading); });
    else Require(scePthreadCreate(&thread, nullptr, Reader, &reading, nullptr) == 0, "create signal reader");
    if (starting) Require(Observe([&] { return reading.entering.load(); }), "reader did not start");
    else {
#ifdef _WIN32
    Require(Observe([&] {
        BOOL pending = FALSE;
        return GetThreadIOPendingFlag(thread->nativeHandle, &pending) && pending;
    }), "read did not enter native syscall");
#else
    Require(Observe([&] { return NativeReadBlocked(reading); }), "read did not enter native syscall");
#endif
    }
    std::atomic<bool> requested{false};
    std::thread sender([&] {
        if (exception) Require(sceKernelRaiseException(thread, 30) == 0, "raise exception during read");
#ifndef _WIN32
        else Require(pthread_kill(unregistered ? native.native_handle() : thread->hostThread,
            guestSignal == 11 ? SIGSEGV : SIGINT) == 0, "raise native signal during read");
#endif
        requested.store(true);
    });
    if (resume) {
        Require(Observe([&] { return reading.handlers.load() == 1 && requested.load(); }), "restart handler was not delivered");
        Require(!reading.finished.load(), "restarting signal ended read");
        Require(write_nid_postfix(original[1], "x", 1) == 1, "complete restarted read");
    }
    Require(Observe([&] { return reading.finished.load() && requested.load(); }), "signal handler close did not release interrupted read");
    sender.join();
    if (native.joinable()) native.join();
    else {
        void* result = nullptr;
        Require(scePthreadJoin(thread, &result) == 0 && result == reinterpret_cast<void*>(std::uintptr_t{7}), "join signal reader");
    }
    Require(reading.handlers.load() == 1 && reading.handler == reading.reader, "wrong signal delivery");
    if (reading.replace) Require(reading.closed == 0 && reading.replaced == reading.input, "signal close/reuse failed");
    if ((!resume && !reading.leave && (reading.result != -1 || reading.error != 4 || reading.byte != 0)) ||
        (resume && (reading.result != 1 || reading.byte != 'x')))
        std::fprintf(stderr, "signal receive result %lld, errno %d, byte %d\n", static_cast<long long>(reading.result), reading.error, reading.byte);
    Require(reading.leave ? reading.result == -3 && reading.byte == 0 :
        resume ? reading.result == 1 && reading.byte == 'x' : reading.result == -1 && reading.error == 4 && reading.byte == 0,
        "wrong interrupted or restarted read result");
    Require(reading.maskPreserved, "I/O signal mask was not restored");
    char byte = 0;
    if (reading.replace) Require(read_nid_postfix(reading.input, &byte, 1) == 1 && byte == 'n', "interrupted read consumed reused descriptor");
    else {
        Require(write_nid_postfix(original[1], "p", 1) == 1, "seed original descriptor after signal");
        Require(read_nid_postfix(reading.input, &byte, 1) == 1 && byte == 'p', "signal lost original descriptor");
    }
    Require(close_nid_postfix(reading.input) == 0 && close_nid_postfix(original[1]) == 0 &&
        close_nid_postfix(replacement[0]) == 0 && close_nid_postfix(replacement[1]) == 0, "close signal pipes");
    if (exception) Require(sceKernelRemoveExceptionHandler(30) == 0, "remove exception handler");
    else signal_nid_postfix(guestSignal, previous);
}

int main(int argc, char** argv) {
    const int rounds = argc == 4 && std::strcmp(argv[3], "starting") == 0 ? 32 : 1;
    for (int round = 0; round < rounds; ++round) RunCase(argc, argv);
}
