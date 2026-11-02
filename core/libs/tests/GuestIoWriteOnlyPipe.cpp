#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include "prx/libc/include/Api.hpp"
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s; guest errno %d, Windows error %lu\n", message, *__error_nid_postfix(), GetLastError());
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
    bool vector = false, disabled = false;
    std::size_t bytes = 1;
    std::atomic<bool> ready{false}, returned{false}, done{false};
    std::atomic<unsigned> cleanups{0};
    std::int64_t result = -2;
    int error = 0;
};

static void APS5_VABI Cleanup(void* opaque) {
    auto& state = *static_cast<State*>(opaque);
    Require(state.cleanups.fetch_add(1) == 0, "repeated pipe cleanup");
    state.done.store(true);
}

static void* APS5_VABI Writer(void* opaque) {
    auto& state = *static_cast<State*>(opaque);
    if (state.disabled) Require(scePthreadSetcancelstate(GUEST_CANCEL_DISABLE, nullptr) == 0, "disable pipe cancellation");
    CleanupHandler cleanup{};
    __pthread_cleanup_push_imp_nid_postfix(Cleanup, &state, &cleanup);
    state.ready.store(true);
    char bytes[] = {'w', 'w'};
    const KernelIovec vectors[]{{bytes, state.bytes / 2}, {bytes + state.bytes / 2, state.bytes - state.bytes / 2}};
    state.result = state.vector ? writev_nid_postfix(state.file, vectors, 2) : write_nid_postfix(state.file, bytes, state.bytes);
    state.error = *__error_nid_postfix();
    state.returned.store(true);
    if (state.disabled) {
        Require(state.result == 1, "disabled pipe writer lost completion");
        Require(scePthreadSetcancelstate(GUEST_CANCEL_ENABLE, nullptr) == 0, "enable pipe cancellation");
        scePthreadTestcancel();
    }
    __pthread_cleanup_pop_imp_nid_postfix(1);
    return reinterpret_cast<void*>(std::uintptr_t{7});
}

int main(int argc, char** argv) {
    Require(argc == 3, "expected pipe API and scenario");
    const std::string scenario = argv[2];
    State state;
    state.vector = std::string(argv[1]) == "writev";
    state.disabled = scenario == "disabled";
    state.bytes = scenario == "nonblocking_partial" ? 2 : 1;
    const auto name = L"\\\\.\\pipe\\anyps5-minimal-writer-" + std::to_wstring(GetCurrentProcessId());
    const auto server = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, nullptr);
    Require(server != INVALID_HANDLE_VALUE, "create inbound pipe server");
    HANDLE client = CreateFileW(name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    Require(client != INVALID_HANDLE_VALUE, "open write-only client");
    Require(ConnectNamedPipe(server, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED, "connect inbound pipe");
    if (scenario.starts_with("nonblocking")) {
        DWORD mode = PIPE_NOWAIT;
        Require(SetNamedPipeHandleState(client, &mode, nullptr, nullptr), "set nonblocking client");
    }
    const int queryDescriptor = ::_open_osfhandle(reinterpret_cast<std::intptr_t>(client), _O_WRONLY | _O_BINARY);
    Require(queryDescriptor >= 0, "adopt pipe for quota setup");
    std::int64_t available = 0;
    bool eof = false, writable = false;
    Require(GuestSockets::PipeWriteState(queryDescriptor, &available, &eof, &writable) && available > 0 && available < 1024 * 1024, "query finite pipe quota");
    const bool attributes = scenario == "disconnected_attributes" || scenario == "disconnected_wait";
    HANDLE limited = INVALID_HANDLE_VALUE;
    Require(DuplicateHandle(GetCurrentProcess(), client, GetCurrentProcess(), &limited, attributes ? 0 : FILE_WRITE_DATA | SYNCHRONIZE, FALSE, attributes ? DUPLICATE_SAME_ACCESS : 0), "retain pipe client");
    Require(::_close(queryDescriptor) == 0, "release pipe setup descriptor");
    client = limited;
    using Query = LONG (NTAPI*)(HANDLE, void*, void*, ULONG, int);
    const auto query = reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationFile"));
    std::uintptr_t status[2]{};
    ACCESS_MASK access = 0;
    Require(query && query(client, status, &access, sizeof(access), 8) >= 0 && ((access & FILE_READ_ATTRIBUTES) != 0) == attributes, "client attribute rights differ");
    state.file = ::_open_osfhandle(reinterpret_cast<std::intptr_t>(client), _O_WRONLY | _O_BINARY);
    Require(state.file >= 0, "adopt minimal pipe client");
    std::vector<char> fill(static_cast<std::size_t>(available) - (scenario == "nonblocking_partial" ? 1 : 0), 'f');
    if (scenario != "complete" && scenario != "disconnected" && scenario != "disconnected_attributes") {
        DWORD count = 0;
        Require(WriteFile(client, fill.data(), static_cast<DWORD>(fill.size()), &count, nullptr) && count == fill.size(), "fill minimal pipe quota");
    }
    if (scenario == "disconnected" || scenario == "disconnected_attributes") Require(DisconnectNamedPipe(server), "disconnect before write");
    Pthread writer{};
    Require(scePthreadCreate(&writer, nullptr, Writer, &state, "minimal-pipe") == 0, "start minimal pipe writer");
    Require(Observe([&] { return state.ready.load(); }), "minimal writer did not start");
    const bool blocked = scenario == "cancel" || state.disabled || scenario == "close" || scenario == "disconnected_wait";
    if (blocked && !state.disabled) Require(Observe([&] { std::lock_guard lock(writer->cancelLock); return writer->cancelIo != nullptr || state.done.load(); }), "minimal writer did not register");
    std::atomic<bool> closed{false};
    std::thread closer;
    if (scenario == "disconnected_wait") Require(DisconnectNamedPipe(server), "disconnect registered writer");
    else if (scenario == "close") closer = std::thread([&] { Require(close_nid_postfix(state.file) == 0, "close minimal pipe"); closed.store(true); });
    else if (scenario == "cancel" || state.disabled) Require(scePthreadCancel(writer) == 0, "cancel minimal writer");
    bool woke = true;
    if (state.disabled) {
        DWORD count = 0;
        Require(ReadFile(server, fill.data(), static_cast<DWORD>(fill.size()), &count, nullptr) && count == fill.size(), "drain disabled writer quota");
    } else if (blocked) {
        woke = Observe([&] { return state.done.load(); });
        if (!woke) {
            DWORD count = 0;
            Require(ReadFile(server, fill.data(), static_cast<DWORD>(fill.size()), &count, nullptr), "release failed minimal writer");
        }
    }
    Require(Observe([&] { return state.done.load(); }), "minimal writer did not complete");
    void* result = nullptr;
    Require(scePthreadJoin(writer, &result) == 0, "join minimal writer");
    if (closer.joinable()) { Require(Observe([&] { return closed.load(); }), "minimal close did not drain"); closer.join(); }
    Require(woke && state.cleanups.load() == 1, "minimal pipe did not wake safely");
    std::printf("minimal pipe writer result %lld, error %d\n", static_cast<long long>(state.result), state.error);
    const bool canceled = scenario == "cancel" || state.disabled;
    Require(result == (canceled ? GUEST_CANCELED : reinterpret_cast<void*>(std::uintptr_t{7})), "minimal pipe join result");
    if (scenario.starts_with("disconnected")) Require(state.result == -1 && state.error == 32, "disconnected pipe did not report EPIPE");
    else if (scenario == "cancel") Require(!state.returned.load(), "blocked minimal writer returned before cancellation");
    else if (scenario == "close") Require(state.result == -1 && state.error == 4, "minimal pipe close result");
    else if (scenario.starts_with("nonblocking")) {
        Require(state.result == -1 && state.error == 35, "minimal pipe nonblocking result");
        DWORD queued = 0;
        Require(PeekNamedPipe(server, nullptr, 0, nullptr, &queued, nullptr) && queued == fill.size(), "small nonblocking write changed the pipe");
    }
    else {
        Require(state.result == 1, "minimal pipe write lost count");
        char byte = 0; DWORD count = 0;
        Require(ReadFile(server, &byte, 1, &count, nullptr) && count == 1 && byte == 'w', "minimal pipe write lost payload");
    }
    if (scenario != "close") Require(close_nid_postfix(state.file) == 0, "close minimal client");
    CloseHandle(server);
    return 0;
}
