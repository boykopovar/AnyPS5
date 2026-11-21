#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"
#include "prx/libc/include/JumpBuffer.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <limits>
#include <optional>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s, guest errno %d\n", message, *__error_nid_postfix());
    std::exit(1);
}

static void HostThreads() {
    for (unsigned round = 0; round < 96; ++round) {
        const auto transfer = [] {
            int descriptors[2]{};
            Require(pipe_nid_postfix(descriptors) == 0, "create host-thread pipe");
            Require(write_nid_postfix(descriptors[1], "host", 4) == 4, "host-thread write");
            std::array<char, 4> bytes{};
            Require(read_nid_postfix(descriptors[0], bytes.data(), bytes.size()) == 4 && std::memcmp(bytes.data(), "host", 4) == 0, "host-thread read");
            Require(close_nid_postfix(descriptors[0]) == 0 && close_nid_postfix(descriptors[1]) == 0, "close host-thread pipe");
        };
#ifdef _WIN32
        if ((round & 1) != 0) {
            auto entry = transfer;
            const auto worker = CreateThread(nullptr, 0, +[](void* opaque) -> DWORD { (*static_cast<decltype(entry)*>(opaque))(); return 0; }, &entry, 0, nullptr);
            Require(worker && WaitForSingleObject(worker, 5000) == WAIT_OBJECT_0, "join native host thread");
            CloseHandle(worker);
            continue;
        }
#endif
        std::thread worker(transfer);
        worker.join();
    }
}

#ifdef _WIN32
template<class TPredicate>
static bool Observe(TPredicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::yield();
    }
    return true;
}
class FlsExhaustion {
public:
    FlsExhaustion() {
        for (unsigned count = 0; count < 65536; ++count) {
            const auto slot = FlsAlloc(nullptr);
            if (slot == FLS_OUT_OF_INDEXES) return;
            slots.push_back(slot);
        }
        Release();
        Require(false, "FLS exhaustion exceeded its bounded budget");
    }
    ~FlsExhaustion() { Release(); }
    void Release() { for (const auto slot : slots) FlsFree(slot); slots.clear(); }
private:
    std::vector<DWORD> slots;
};
static void EmptyRetirement(bool withRestart) {
    std::atomic<bool> ready{false}, release{false}, failed{false};
    std::thread worker([&] {
        std::optional<ThreadCancel::IoRestart> restart;
        if (withRestart) restart.emplace(false);
        ready.store(true);
        Require(Observe([&] { return release.load(); }), "release empty retirement");
        try {
            ThreadCancel::RetireIoStack(std::numeric_limits<std::uintptr_t>::max());
            const auto target = JumpBuffer::GuestContextCreate_nid_no_patch(1);
            ThreadCancel::RetireIoStack(0, target.identity);
            if (JumpBuffer::GuestContextCurrent_nid_no_patch() != target.identity) failed.store(true);
        }
        catch (...) { failed.store(true); }
    });
    Require(Observe([&] { return ready.load(); }), "start empty retirement");
    FlsExhaustion exhausted;
    release.store(true);
    worker.join();
    exhausted.Release();
    Require(!failed.load(), "empty I/O retirement allocated unavailable FLS storage");
}
struct ExitState {
    PthreadKey key = -1;
    bool explicitExit = true;
    std::atomic<bool> ready{false}, release{false};
    std::atomic<unsigned> cleanups{0}, destructors{0};
};
static void APS5_VABI ExitCleanup(void* opaque) { ++static_cast<ExitState*>(opaque)->cleanups; }
static void APS5_VABI ExitDestroy(void* opaque) {
    auto& state = *static_cast<ExitState*>(opaque);
    Require(state.cleanups.load() == 1, "no-I/O exit TSD preceded cleanup");
    ++state.destructors;
}
static void* APS5_VABI ExitWorker(void* opaque) {
    auto& state = *static_cast<ExitState*>(opaque);
    Require(scePthreadSetspecific(state.key, &state) == 0, "set no-I/O exit TSD");
    CleanupHandler cleanup{};
    __pthread_cleanup_push_imp_nid_postfix(ExitCleanup, &state, &cleanup);
    state.ready.store(true);
    Require(Observe([&] { return state.release.load(); }), "release no-I/O exit");
    if (state.explicitExit) scePthreadExit(reinterpret_cast<void*>(std::uintptr_t{7}));
    __pthread_cleanup_pop_imp_nid_postfix(1);
    return reinterpret_cast<void*>(std::uintptr_t{7});
}
static void EmptyGuestExit(bool explicitExit) {
    ExitState state;
    state.explicitExit = explicitExit;
    Require(scePthreadKeyCreate(&state.key, reinterpret_cast<pthread_key_destructor_func_t>(ExitDestroy)) == 0, "create no-I/O exit key");
    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, ExitWorker, &state, nullptr) == 0, "create no-I/O exit worker");
    Require(Observe([&] { return state.ready.load(); }), "start no-I/O exit worker");
    FlsExhaustion exhausted;
    state.release.store(true);
    const bool finished = Observe([&] { return worker->_finished.load(); });
    exhausted.Release();
    Require(finished, "no-I/O guest exit required freeing FLS slots");
    void* result = nullptr;
    Require(scePthreadJoin(worker, &result) == 0 && result == reinterpret_cast<void*>(std::uintptr_t{7}), "join no-I/O exit worker");
    Require(state.cleanups.load() == 1 && state.destructors.load() == 1, "no-I/O exit cleanup/TSD count");
    Require(scePthreadKeyDelete(state.key) == 0, "delete no-I/O exit key");
}
#endif
int main(int argc, char** argv) {
#ifdef _WIN32
    if (argc == 2 && std::strcmp(argv[1], "retire_restart") == 0) { EmptyRetirement(true); return 0; }
    if (argc == 2 && std::strcmp(argv[1], "retire_empty") == 0) { EmptyRetirement(false); return 0; }
    if (argc == 2 && std::strcmp(argv[1], "return_empty") == 0) { EmptyGuestExit(false); return 0; }
    if (argc == 2 && std::strcmp(argv[1], "exit_empty") == 0) { EmptyGuestExit(true); return 0; }
#else
    static_cast<void>(argc);
    static_cast<void>(argv);
#endif
    HostThreads();
}
