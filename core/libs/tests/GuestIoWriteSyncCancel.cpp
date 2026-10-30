#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/DescriptorControl.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/SharedMemory.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libc/include/Api.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <csignal>
#include <pthread.h>
#include <unistd.h>
#endif

static void Require(bool condition, const char* text) {
    if (condition) return;
    std::fprintf(stderr, "%s; guest errno %d\n", text, *__error_nid_postfix());
    std::_Exit(1);
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
    int file = -1;
    PthreadKey key = -1;
    std::string api, scenario;
    bool metadata = false;
    unsigned preflight = 0;
    std::atomic<unsigned> calls{0}, cleanups{0}, destructors{0}, signals{0};
    std::atomic<bool> entered{false}, finished{false}, returned{false};
    std::int64_t result = -2;
    int error = 0;
#ifdef _WIN32
    HANDLE input = INVALID_HANDLE_VALUE, output = INVALID_HANDLE_VALUE;
#else
    int input = -1, output = -1;
#endif
};

static State* state = nullptr;

static int Flush(int file) {
    Require(file == state->file, "flush used a different descriptor");
    const auto call = state->calls.fetch_add(1) + 1;
    if (state->preflight != 0 && call != state->preflight) return 0;
    if (state->scenario == "signal" && state->preflight == 0 && call > 1) return 0;
    if (state->scenario == "zero" || state->scenario == "error" || state->scenario == "legacy") return 0;
    state->entered.store(true);
    char token = 0;
#ifdef _WIN32
    DWORD count = 0;
    const bool completed = ReadFile(state->input, &token, 1, &count, nullptr) && count == 1;
#else
    const bool completed = ::read(state->input, &token, 1) == 1;
#endif
    if (!completed) return static_cast<int>(0x80020004u);
    if (state->metadata || state->preflight != 0) return 0;
#ifdef _WIN32
    Require(::_commit(file) == 0, "native file flush failed");
#else
    Require(::fsync(file) == 0, "native file flush failed");
#endif
    return 0;
}

static void Release() {
    const char token = 'x';
#ifdef _WIN32
    DWORD count = 0;
    Require(WriteFile(state->output, &token, 1, &count, nullptr) && count == 1, "release native flush wait");
#else
    Require(::write(state->output, &token, 1) == 1, "release native flush wait");
#endif
}

static void APS5_VABI Cleanup(void* opaque) {
    Require(opaque == state, "cleanup argument ABI");
    Require(state->cleanups.fetch_add(1) == 0 && state->destructors.load() == 0, "cleanup order");
    if (state->scenario != "close") Require(fcntl_nid_postfix(state->file, 4, 0) == 0, "cleanup did not release descriptor admission");
}

static void APS5_VABI Destroy(void* opaque) {
    Require(opaque == state, "TSD argument ABI");
    Require(state->cleanups.load() == 1 && state->destructors.fetch_add(1) == 0, "TSD order");
    state->finished.store(true);
}

static void APS5_VABI Handler(int) { ++state->signals; }
static void APS5_VABI ExceptionHandler(int signal, void*) { Handler(signal); }

static std::int64_t Write(std::size_t size) {
    const char bytes[] = {'a', 'b'};
    const KernelIovec vectors[]{{const_cast<char*>(bytes), size / 2}, {const_cast<char*>(bytes) + size / 2, size - size / 2}};
    if (state->api == "write") return write_nid_postfix(state->file, bytes, size);
    if (state->api == "writev") return writev_nid_postfix(state->file, vectors, 2);
    if (state->api == "pwrite") return pwrite_nid_disambig1_nid_postfix(state->file, bytes, size, 0);
    if (state->api == "pwritev") return pwritev_nid_postfix(state->file, vectors, 2, 0);
    return sceKernelWrite(state->file, bytes, size);
}

static void* APS5_VABI Writer(void* opaque) {
    Require(opaque == state, "writer argument ABI");
    if (state->scenario == "disabled") Require(scePthreadSetcancelstate(GUEST_CANCEL_DISABLE, nullptr) == 0, "disable cancellation");
    Require(scePthreadSetspecific(state->key, state) == 0, "set flush TSD");
    CleanupHandler cleanup{};
    __pthread_cleanup_push_imp_nid_postfix(Cleanup, state, &cleanup);
    state->result = Write(2);
    state->error = *__error_nid_postfix();
    if (state->scenario == "signal" && state->preflight == 0 && state->result == -1 && state->error == 4) {
        state->result = Write(2);
        state->error = *__error_nid_postfix();
    }
    state->returned.store(true);
    if (state->scenario == "cancel") scePthreadTestcancel();
    if (state->scenario == "disabled" || state->api == "sce") {
        Require(state->result == 2, "disabled or SCE flush lost completion");
        Require(scePthreadSetcancelstate(GUEST_CANCEL_ENABLE, nullptr) == 0, "enable cancellation");
        scePthreadTestcancel();
    }
    __pthread_cleanup_pop_imp_nid_postfix(1);
    return reinterpret_cast<void*>(std::uintptr_t{7});
}

int main(int argc, char** argv) {
    Require(argc == 3, "expected API and scenario");
    State storage;
    state = &storage;
    state->api = argv[1]; state->scenario = argv[2];
    if (state->scenario.starts_with("preflight")) {
        state->preflight = state->scenario[9] == '1' ? 1 : 2;
        state->scenario.erase(0, 11);
    }
    if (state->scenario.starts_with("metadata_")) {
        state->metadata = true;
        state->scenario.erase(0, 9);
    }
    const auto path = "anyps5-sync-cancel-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    state->file = open_nid_postfix(path.c_str(), 2 | 0x200, 0600);
    Require(state->file >= 0 && (state->preflight != 0 || fcntl_nid_postfix(state->file, 4, 0x80) == 0), "create synchronous file");
    if (state->scenario == "error") {
        Require(close_nid_postfix(state->file) == 0, "close writable file");
        state->file = open_nid_postfix(path.c_str(), 0x80, 0600);
        Require(state->file >= 0, "reopen read-only synchronous file");
    }
    if (state->preflight != 0) File::FileSetSharedMemoryMetadataHookForTest_nid_no_patch(Flush);
    else if (state->metadata) File::FileSetSynchronousWriteMetadataHookForTest_nid_no_patch(Flush);
    else File::FileSetSynchronousWriteHookForTest_nid_no_patch(Flush);
    if (state->scenario == "zero" || state->scenario == "error" || state->scenario == "legacy") {
        const auto result = Write(state->scenario == "error" ? 2 : 0);
        Require(result == (state->scenario == "error" ? -1 : 0), "zero or failed write result");
        Require(state->calls.load() == (state->scenario == "legacy" ? 1u : 0u), "zero-progress flush contract changed");
    } else {
#ifdef _WIN32
        Require(CreatePipe(&state->input, &state->output, nullptr, 4096), "create native flush wait pipe");
#else
        int pipe[2]{};
        Require(::pipe(pipe) == 0, "create native flush wait pipe");
        state->input = pipe[0]; state->output = pipe[1];
#endif
        if (state->scenario == "signal") {
#ifdef _WIN32
            Require(sceKernelInstallExceptionHandler(30, reinterpret_cast<void*>(ExceptionHandler)) == 0, "install flush exception handler");
#else
            GuestSigaction action{};
            action.handler = reinterpret_cast<std::uintptr_t>(&Handler);
            action.flags = state->preflight != 0 ? 0 : 2;
            Require(sigaction_nid_postfix(2, &action, nullptr) == 0, "install restartable handler");
#endif
        }
        Require(scePthreadKeyCreate(&state->key, reinterpret_cast<pthread_key_destructor_func_t>(Destroy)) == 0, "create flush TSD key");
        Pthread writer{};
        Require(scePthreadCreate(&writer, nullptr, Writer, state, "sync-write") == 0, "start synchronous writer");
        Require(Observe([&] { return state->entered.load(); }), "writer did not enter native flush");
        std::atomic<bool> closed{false};
        std::thread closer;
        if (state->scenario == "close") {
            closer = std::thread([&] { Require(close_nid_postfix(state->file) == 0, "close flushing descriptor"); closed.store(true); });
        } else if (state->scenario == "signal") {
#ifdef _WIN32
            Require(sceKernelRaiseException(writer, 30) == 0, "raise flush exception");
#else
            Require(::pthread_kill(writer->hostThread, SIGINT) == 0, "interrupt flushing writer");
#endif
        } else if (state->scenario != "complete") {
            Require(scePthreadCancel(writer) == 0, "cancel flushing writer");
        }
        const bool waiting = state->scenario == "disabled" || state->scenario == "complete" || state->api == "sce";
        bool woke = false;
        if (waiting) Release();
        else {
            woke = Observe([&] { return state->finished.load(); });
            if (!woke) Release();
        }
        Require(Observe([&] { return state->finished.load(); }), "writer did not finish after native flush release");
        void* joined = nullptr;
        Require(scePthreadJoin(writer, &joined) == 0, "join flushing writer");
        if (closer.joinable()) { Require(Observe([&] { return closed.load(); }), "close did not drain flush"); closer.join(); }
        Require(waiting || woke, "cancellation or close did not wake native flush");
        const bool canceled = state->scenario == "cancel" || state->scenario == "disabled" || state->api == "sce";
        Require(joined == (canceled ? GUEST_CANCELED : reinterpret_cast<void*>(std::uintptr_t{7})), "unexpected flush join result");
        Require(state->cleanups.load() == 1 && state->destructors.load() == 1 && state->calls.load() == (state->preflight != 0 ? (waiting ? 2u : state->preflight) : 1u), "flush repeated or cleanup lost");
        Require(std::filesystem::file_size(path) == (state->preflight != 0 && !waiting ? 0u : 2u), "flush interruption changed write progress");
        if (canceled) Require(state->returned.load() == (waiting || state->preflight == 0), "canceled flush completion state");
        else if (state->scenario == "complete" || state->preflight == 0) Require(state->result == 2, "completed flush lost positive result");
        else Require(state->result == -1 && state->error == 4, "interrupted flush did not return EINTR");
        if (state->scenario == "signal") Require(state->signals.load() == 1, "restartable signal lost or write repeated");
        Require(scePthreadKeyDelete(state->key) == 0, "delete flush TSD key");
#ifdef _WIN32
        CloseHandle(state->input); CloseHandle(state->output);
#else
        ::close(state->input); ::close(state->output);
#endif
    }
    File::FileSetSharedMemoryMetadataHookForTest_nid_no_patch(nullptr);
    File::FileSetSynchronousWriteHookForTest_nid_no_patch(nullptr);
    File::FileSetSynchronousWriteMetadataHookForTest_nid_no_patch(nullptr);
    if (state->scenario != "close") Require(close_nid_postfix(state->file) == 0, "close synchronous file");
    std::filesystem::remove(path);
    return 0;
}
