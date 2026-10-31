#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libkernel/System/include/UserContext.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <thread>
#ifndef _WIN32
#include <csignal>
#include <pthread.h>
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

struct Fibre {
    UserUcontext context{};
    alignas(16) std::uint64_t jump[12]{};
    alignas(16) std::uint64_t outer[12]{};
    std::unique_ptr<std::byte[]> stack;
    bool started = false;
    bool handled = false;
    bool finished = false;
    int descriptors[2]{};
};

struct State {
    Fibre dispatcher;
    std::array<Fibre, 2> fibres;
    Fibre* active = nullptr;
    bool jumps = false;
    bool signalJumps = false;
    bool underscoreJumps = false;
    bool manual = false;
    bool leave = false;
    bool signalLeave = false;
    bool exitHandler = false;
    std::atomic<int> phase{0};
    std::atomic<int> cleanups{0};
    std::atomic<int> destructors{0};
    std::atomic<Pthread> adopted{nullptr};
};

static State* state;

#ifndef _WIN32
static sigset_t NativeMask() {
    sigset_t mask;
    Require(pthread_sigmask(SIG_SETMASK, nullptr, &mask) == 0, "capture native mask");
    return mask;
}

static void CheckMask(const sigset_t& before) {
    const auto after = NativeMask();
    for (int signal = 1; signal < NSIG; ++signal)
        Require(sigismember(&before, signal) == sigismember(&after, signal), "fibre switch lost native signal mask");
}
#endif

static void Switch(Fibre& from, Fibre& to) {
    if (!state->jumps) {
        const auto target = to.context;
        Require(swapcontext_nid_postfix(&from.context, &target) == 0, "swap I/O context");
        return;
    }
    from.started = true;
    if (state->signalJumps) {
        if (sigsetjmp_nid_postfix(from.jump, 1) == 0) {
            if (to.started) {
                alignas(16) std::uint64_t target[12];
                std::memcpy(target, to.jump, sizeof(target));
                siglongjmp_nid_postfix(target, 1);
            }
            setcontext_nid_postfix(&to.context);
            Require(false, "signal context restore returned");
        }
        return;
    }
    if (state->underscoreJumps) {
        if (_setjmp_nid_postfix(from.jump) == 0) {
            if (to.started) {
                alignas(16) std::uint64_t target[12];
                std::memcpy(target, to.jump, sizeof(target));
                _longjmp_nid_postfix(target, 1);
            }
            setcontext_nid_postfix(&to.context);
            Require(false, "underscore context restore returned");
        }
        return;
    }
    if (setjmp_nid_postfix(from.jump) == 0) {
        if (to.started) {
            alignas(16) std::uint64_t target[12];
            std::memcpy(target, to.jump, sizeof(target));
            longjmp_nid_postfix(target, 1);
        }
        setcontext_nid_postfix(&to.context);
        Require(false, "context restore returned");
    }
}

static void APS5_VABI Handler(int, void*) {
    if (state->exitHandler) pthread_exit_nid_postfix(GUEST_CANCELED);
    auto& fibre = *state->active;
#ifndef _WIN32
    const auto mask = NativeMask();
#endif
    Switch(fibre, state->dispatcher);
#ifndef _WIN32
    CheckMask(mask);
#endif
    Require(close_nid_postfix(fibre.descriptors[0]) == 0, "close resumed fibre descriptor");
    fibre.descriptors[0] = -1;
    fibre.handled = true;
    if (state->signalLeave) siglongjmp_nid_postfix(fibre.outer, 1);
    if (state->leave) longjmp_nid_postfix(fibre.outer, 1);
}

static void APS5_VABI FibreBody() {
    auto& fibre = *state->active;
    char byte = 0;
    bool escaped = false;
    if (state->leave) {
        if (state->signalLeave) escaped = sigsetjmp_nid_postfix(fibre.outer, 1) != 0;
        else escaped = setjmp_nid_postfix(fibre.outer) != 0;
    }
    if (!escaped) {
        const auto result = read_nid_postfix(fibre.descriptors[0], &byte, 1);
        Require(!state->leave && result == -1 && *__error_nid_postfix() == 4 && fibre.handled, "suspended handler did not resume safely");
    }
    fibre.finished = true;
    Switch(fibre, state->dispatcher);
    Require(false, "completed fibre resumed");
}

static void* APS5_VABI Scheduler(void*) {
    constexpr std::size_t StackBytes = 128 * 1024;
#ifndef _WIN32
    const auto mask = NativeMask();
#endif
    int index = 0;
    for (auto& fibre : state->fibres) {
        fibre.stack = std::make_unique<std::byte[]>(StackBytes);
        Require(getcontext_nid_postfix(&fibre.context) == 0, "capture fibre template");
        fibre.context.stackPointer = fibre.stack.get();
        fibre.context.stackSize = StackBytes;
        if (state->manual) {
            auto& machine = fibre.context.mcontext;
            machine.rsp = ((reinterpret_cast<std::uintptr_t>(fibre.stack.get()) + StackBytes) & ~std::uintptr_t{15}) - 8;
            *reinterpret_cast<std::uint64_t*>(machine.rsp) = 0;
            machine.rip = reinterpret_cast<std::uintptr_t>(FibreBody);
            machine.rbp = 0;
        } else makecontext_nid_postfix(&fibre.context, FibreBody, 0);
        state->active = &fibre;
        state->phase.store(++index);
        Switch(state->dispatcher, fibre);
#ifndef _WIN32
        CheckMask(mask);
#endif
        Require(!fibre.handled && !fibre.finished, "handler did not suspend");
        alignas(16) std::uint64_t local[12]{};
        if (setjmp_nid_postfix(local) == 0) longjmp_nid_postfix(local, 1);
    }
    for (auto& fibre : state->fibres) {
        state->active = &fibre;
        Switch(state->dispatcher, fibre);
#ifndef _WIN32
        CheckMask(mask);
#endif
        Require(fibre.finished && fibre.handled, "fibre completion lost delivery ownership");
    }
    return nullptr;
}

static void APS5_VABI Cleanup(void*) { state->cleanups.fetch_add(1); }
static void APS5_VABI Destroy(void*) {
    Require(state->cleanups.load() == 1, "adopted cleanup order");
    state->destructors.fetch_add(1);
}

#ifdef __linux__
static std::size_t ResidentBytes() {
    std::ifstream memory("/proc/self/statm");
    std::size_t mapped = 0;
    std::size_t resident = 0;
    Require(static_cast<bool>(memory >> mapped >> resident), "read context resident memory");
    return resident * static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
}

static void ContextChurn() {
    constexpr std::size_t StackBytes = 2048;
    constexpr std::size_t Count = 32768;
    std::vector<std::byte> stacks(StackBytes * Count);
    auto* volatileBytes = reinterpret_cast<volatile std::byte*>(stacks.data());
    for (std::size_t offset = 0; offset < stacks.size(); offset += 1024) volatileBytes[offset] = std::byte{1};
    UserUcontext original{};
    Require(getcontext_nid_postfix(&original) == 0, "capture churn template");
    const auto before = ResidentBytes();
    for (std::size_t index = 0; index < Count; ++index) {
        auto context = original;
        context.stackPointer = stacks.data() + StackBytes * index;
        context.stackSize = StackBytes;
        makecontext_nid_postfix(&context, FibreBody, 0);
        Require(context.mcontext.len == sizeof(UserMcontext), "create churn context");
    }
    Require(ResidentBytes() <= before + 1024 * 1024, "context creation retains stack registry metadata");
}
#endif

int main(int argc, char** argv) {
    Require(argc == 2, "expected adopted, adopted_exit, swap, jump or sigjump");
    State reading;
    state = &reading;
#ifdef __linux__
    if (std::strcmp(argv[1], "churn") == 0) { ContextChurn(); return 0; }
#endif
    reading.exitHandler = std::strcmp(argv[1], "adopted_exit") == 0;
    if (reading.exitHandler || std::strcmp(argv[1], "adopted") == 0) {
        if (reading.exitHandler) Require(sceKernelInstallExceptionHandler(30, reinterpret_cast<void*>(Handler)) == 0, "install adopted exit handler");
        int descriptors[2]{};
        Require(pipe_nid_postfix(descriptors) == 0, "create adopted pipe");
        PthreadKey key = -1;
        Require(scePthreadKeyCreate(&key, reinterpret_cast<pthread_key_destructor_func_t>(Destroy)) == 0, "create adopted key");
        std::thread native([&] {
            auto self = scePthreadSelf();
            Require(scePthreadSetspecific(key, &reading) == 0, "set adopted TSD");
            CleanupHandler cleanup{};
            __pthread_cleanup_push_imp_nid_postfix(Cleanup, nullptr, &cleanup);
            reading.adopted.store(self);
            char byte = 0;
            read_nid_postfix(descriptors[0], &byte, 1);
            Require(false, "adopted read returned");
        });
        Require(Observe([&] {
            auto thread = reading.adopted.load();
            if (!thread) return false;
            std::lock_guard lock(thread->cancelLock);
            return thread->cancelIo != nullptr;
        }), "adopted read did not block");
        if (reading.exitHandler) Require(sceKernelRaiseException(reading.adopted.load(), 30) == 0, "exit adopted reader from handler");
        else Require(pthread_cancel_nid_postfix(reading.adopted.load()) == 0, "cancel adopted reader");
        native.join();
        Require(reading.cleanups.load() == 1 && reading.destructors.load() == 1, "adopted cleanup repeated");
        Require(scePthreadKeyDelete(key) == 0 && close_nid_postfix(descriptors[0]) == 0 && close_nid_postfix(descriptors[1]) == 0, "release adopted resources");
        if (reading.exitHandler) Require(sceKernelRemoveExceptionHandler(30) == 0, "remove adopted exit handler");
        return 0;
    }
    const std::string scenario = argv[1];
    reading.manual = scenario.starts_with("manual_");
    reading.leave = scenario.ends_with("leave");
    reading.signalLeave = scenario.ends_with("sigleave");
    reading.signalJumps = scenario.ends_with("sigjump");
    reading.underscoreJumps = scenario.ends_with("underscorejump");
    reading.jumps = scenario.ends_with("jump");
    for (auto& fibre : reading.fibres) Require(pipe_nid_postfix(fibre.descriptors) == 0, "create fibre pipe");
    Require(sceKernelInstallExceptionHandler(30, reinterpret_cast<void*>(Handler)) == 0, "install fibre handler");
    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, Scheduler, nullptr, nullptr) == 0, "create fibre scheduler");
    for (int phase = 1; phase <= 2; ++phase) {
        Require(Observe([&] {
            if (reading.phase.load() != phase) return false;
            std::lock_guard lock(worker->cancelLock);
            return worker->cancelIo != nullptr;
        }), "fibre read did not block");
        Require(sceKernelRaiseException(worker, 30) == 0, "interrupt fibre read");
    }
    Require(Observe([&] { return worker->_finished.load(); }), "fibre scheduler did not finish");
    Require(scePthreadJoin(worker, nullptr) == 0, "join fibre scheduler");
    Require(sceKernelRemoveExceptionHandler(30) == 0, "remove fibre handler");
    for (auto& fibre : reading.fibres)
        Require(fibre.descriptors[0] == -1 && close_nid_postfix(fibre.descriptors[1]) == 0, "close fibre pipe");
}
