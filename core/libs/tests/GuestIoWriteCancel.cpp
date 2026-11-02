#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <csignal>
#include <pthread.h>
#include <unistd.h>
#endif

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s, guest errno %d\n", message, *__error_nid_postfix());
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
    int output = -1;
    int input = -1;
    int replacement = -1;
    std::atomic<unsigned> signals{0};
    PthreadKey key = -1;
    std::string api;
    std::string scenario;
    bool socket = false;
    bool regular = false;
    bool datagram = false;
    std::vector<char> bytes;
    std::int64_t result = -2;
    int error = 0;
    std::atomic<bool> ready{false}, release{false}, returned{false};
    std::atomic<int> cleanups{0}, destructors{0};
};

static State* signalState = nullptr;
static void APS5_VABI SignalHandler(int) {
    Require(close_nid_postfix(signalState->output) == 0, "signal handler closes writer");
    Require(dup2_nid_postfix(signalState->replacement, signalState->output) == signalState->output, "signal handler replaces writer");
    ++signalState->signals;
}
static void APS5_VABI ExceptionHandler(int signal, void*) { SignalHandler(signal); }

static void APS5_VABI Cleanup(void* opaque) {
    auto& state = *static_cast<State*>(opaque);
    Require(state.cleanups.fetch_add(1) == 0 && state.destructors.load() == 0, "repeated or reordered cleanup");
}
static void APS5_VABI Destroy(void* opaque) {
    auto& state = *static_cast<State*>(opaque);
    Require(state.cleanups.load() == 1 && state.destructors.fetch_add(1) == 0, "repeated or reordered TSD");
}

static void* APS5_VABI Writer(void* opaque) {
    auto& state = *static_cast<State*>(opaque);
    const bool disabled = state.scenario == "disabled";
    if (disabled) Require(scePthreadSetcancelstate(GUEST_CANCEL_DISABLE, nullptr) == 0, "disable writer cancellation");
    Require(scePthreadSetspecific(state.key, &state) == 0, "set writer TSD");
    CleanupHandler cleanup{};
    __pthread_cleanup_push_imp_nid_postfix(Cleanup, &state, &cleanup);
    state.ready.store(true);
    if (state.scenario.starts_with("pending")) Require(Observe([&] { return state.release.load(); }), "release pending write");
    const auto size = state.bytes.size();
    KernelIovec vectors[2]{{state.bytes.data(), size / 2}, {size == 0 ? nullptr : state.bytes.data() + size / 2, size - size / 2}};
    GuestMsghdr message{};
    message.iov = vectors;
    message.iovLength = 2;
    const int flags = state.scenario == "dontwait" ? 0x20080 : 0x20000;
    if (state.api == "write") state.result = write_nid_postfix(state.output, state.bytes.data(), size);
    else if (state.api == "alias") state.result = _write_nid_postfix(state.output, state.bytes.data(), size);
    else if (state.api == "writev") state.result = writev_nid_postfix(state.output, vectors, 2);
    else if (state.api == "writev_alias") state.result = _writev_nid_postfix(state.output, vectors, 2);
    else if (state.api == "pwrite") state.result = pwrite_nid_disambig1_nid_postfix(state.output, state.bytes.data(), size, 1);
    else if (state.api == "pwritev") state.result = pwritev_nid_postfix(state.output, vectors, 2, 1);
    else if (state.api == "send") state.result = send_nid_postfix(state.output, state.bytes.data(), size, flags);
    else if (state.api == "sendto") state.result = sendto_nid_postfix(state.output, state.bytes.data(), size, flags, nullptr, 0);
    else if (state.api == "sendto_alias") state.result = _sendto_nid_postfix(state.output, state.bytes.data(), size, flags, nullptr, 0);
    else if (state.api == "sendmsg") state.result = sendmsg_nid_postfix(state.output, &message, flags);
    else if (state.api == "sendmsg_alias") state.result = _sendmsg_nid_postfix(state.output, &message, flags);
    else if (state.api == "sce_write") state.result = sceKernelWrite(state.output, state.bytes.data(), size);
    else if (state.api == "sce_writev") state.result = sceKernelWritev(state.output, vectors, 2);
    else Require(false, "unknown write API");
    state.error = *__error_nid_postfix();
    state.returned.store(true);
    if (disabled || state.api.starts_with("sce_")) {
        Require(state.result == static_cast<std::int64_t>(size), "disabled/SCE write lost data");
        Require(scePthreadSetcancelstate(GUEST_CANCEL_ENABLE, nullptr) == 0, "enable writer cancellation");
        scePthreadTestcancel();
    }
    __pthread_cleanup_pop_imp_nid_postfix(1);
    return reinterpret_cast<void*>(std::uintptr_t{7});
}

static std::size_t Fill(State& state) {
    Require(fcntl_nid_postfix(state.output, 4, 6) == 0, "set fill nonblocking");
    std::array<char, 4096> bytes{};
    bytes.fill('f');
    std::size_t filled = 0;
    while (filled < 16 * 1024 * 1024) {
        const auto sent = write_nid_postfix(state.output, bytes.data(), bytes.size());
        if (sent < 0) {
            Require(*__error_nid_postfix() == 35, "fill did not stop with EAGAIN");
            if (state.scenario != "nonblocking") Require(fcntl_nid_postfix(state.output, 4, 2) == 0, "restore fill blocking");
            return filled;
        }
        Require(sent > 0, "fill made no progress");
        filled += static_cast<std::size_t>(sent);
    }
    Require(false, "transport did not fill within its bounded budget");
    return 0;
}

static void Drain(State& state, std::size_t count, char expected) {
    std::array<char, 16384> buffer{};
    while (count != 0) {
        int available = 0;
        Require(Observe([&] {
            Require(ioctl_nid_postfix(state.input, 0x4004667f, &available) == 0, "query input bytes");
            return available > 0;
        }), "peer did not receive expected bytes");
        const auto amount = std::min({count, buffer.size(), static_cast<std::size_t>(available)});
        const auto received = read_nid_postfix(state.input, buffer.data(), amount);
        Require(received > 0, "drain did not progress");
        Require(std::all_of(buffer.begin(), buffer.begin() + received, [&](char byte) { return byte == expected; }), "write lost byte order or ownership");
        count -= static_cast<std::size_t>(received);
    }
}

static void Setup(State& state) {
    if (state.datagram) {
        int descriptors[2]{};
        Require(socketpair_nid_postfix(1, 2, 0, descriptors) == 0, "create write datagram pair");
        state.input = descriptors[0]; state.output = descriptors[1];
        return;
    }
    if (!state.socket) {
        int descriptors[2]{};
        Require(pipe_nid_postfix(descriptors) == 0, "create blocked write pipe");
        state.input = descriptors[0]; state.output = descriptors[1];
        return;
    }
    const int listener = socket_nid_postfix(2, 1, 0);
    Require(listener >= 0, "create write listener");
    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0 && listen_nid_postfix(listener, 4) == 0, "listen write loopback");
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &size) == 0, "get write listener address");
    state.output = socket_nid_postfix(2, 1, 0);
    const int capacity = 4096;
    Require(state.output >= 0 && setsockopt_nid_postfix(state.output, 0xffff, 0x1001, &capacity, sizeof(capacity)) == 0, "bound socket send buffer");
    Require(connect_nid_postfix(state.output, address.data(), address.size()) == 0, "connect write loopback");
    state.input = accept_nid_postfix(listener, nullptr, nullptr);
    Require(state.input >= 0 && setsockopt_nid_postfix(state.input, 0xffff, 0x1002, &capacity, sizeof(capacity)) == 0, "bound socket receive buffer");
    Require(close_nid_postfix(listener) == 0, "close write listener");
}

static void Run(const char* transport, const char* api, const char* scenario) {
    State state;
    state.api = api; state.scenario = scenario; state.socket = std::strcmp(transport, "loopback") == 0;
    state.datagram = std::strcmp(transport, "datagram") == 0;
    state.socket = state.socket || state.datagram;
    state.regular = std::strcmp(transport, "regular") == 0;
    const bool partial = state.scenario == "partial";
    const bool disabled = state.scenario == "disabled";
    const bool sce = state.api.starts_with("sce_");
    const bool pending = state.scenario.starts_with("pending");
    const bool zero = state.scenario == "zero" || state.scenario == "pending_zero";
    const auto expectedCount = zero ? 0 : 2;
    const bool append = state.scenario == "append";
    const bool race = state.scenario == "race";
    const bool peerClose = state.scenario == "peer_close";
    const bool aliasClose = state.scenario == "alias_close";
    const bool completion = state.scenario == "complete" || state.scenario == "zero" || append || disabled || sce;
    const bool wouldBlock = state.scenario == "nonblocking" || state.scenario == "dontwait" || state.scenario == "timeout";
    const bool close = state.scenario == "close";
    const bool reuse = state.scenario == "reuse";
    const bool signal = state.scenario == "signal_close";
    std::FILE* file = nullptr;
    if (state.regular) {
        file = std::tmpfile(); Require(file != nullptr, "create write file");
        Require(std::fwrite("xyz", 1, 3, file) == 3 && std::fflush(file) == 0 && std::fseek(file, 0, SEEK_SET) == 0, "seed write file");
#ifdef _WIN32
        state.output = ::_fileno(file);
#else
        state.output = ::fileno(file);
#endif
    } else Setup(state);
    state.bytes.assign(zero ? 0 : partial ? 8 * 1024 * 1024 : 2, 'w');
    auto filled = state.regular || state.datagram || zero || partial || pending ? 0 : Fill(state);
    if (state.scenario == "atomic") { Require(filled > 0, "empty atomic-write fill"); Drain(state, 1, 'f'); --filled; }
    const int alias = state.regular ? -1 : dup_nid_postfix(state.output);
    if (!state.regular) Require(alias >= 0, "duplicate writer");
    if (state.scenario == "timeout") {
        const std::array<std::int64_t, 2> timeout{0, 60000};
        Require(setsockopt_nid_postfix(state.output, 0xffff, 0x1005, timeout.data(), sizeof(timeout)) == 0, "set send timeout");
    }
    if (append) Require(fcntl_nid_postfix(state.output, 4, 10) == 0, "set append writes");
    std::optional<GuestDescriptorAccess> exclusion;
    if (state.scenario == "access") exclusion.emplace(state.output, GuestDescriptorAccess::Mode::Update);
#ifdef _WIN32
    std::optional<File::PositionedTransferLock> positioned;
    if (state.scenario == "position") positioned.emplace(File::LockPositionedTransfer(state.output));
#endif
    Require(scePthreadKeyCreate(&state.key, reinterpret_cast<pthread_key_destructor_func_t>(Destroy)) == 0, "create write TSD key");
    Pthread writer = nullptr;
    Require(scePthreadCreate(&writer, nullptr, Writer, &state, nullptr) == 0, "create blocked writer");
    Require(Observe([&] { return state.ready.load(); }), "writer did not start");
    if (!state.regular && !partial && !disabled && !sce && !wouldBlock && !pending) Require(Observe([&] {
        std::lock_guard lock(writer->cancelLock); return writer->cancelIo != nullptr || writer->_finished.load();
    }), "writer did not register its blocked operation");
    if (partial) Require(Observe([&] {
        int available = 0; Require(ioctl_nid_postfix(state.input, 0x4004667f, &available) == 0, "query partial write"); return available > 0;
    }), "partial write did not transfer initial bytes");
    int replacement[2]{-1, -1};
    GuestSigaction previous{};
    if (signal) {
        signalState = &state;
        Require(pipe_nid_postfix(replacement) == 0, "create signal replacement pipe");
        state.replacement = replacement[1];
#ifdef _WIN32
        Require(sceKernelInstallExceptionHandler(30, reinterpret_cast<void*>(ExceptionHandler)) == 0, "install write exception handler");
#else
        const GuestSigaction action{reinterpret_cast<std::uintptr_t>(SignalHandler), 0, {}};
        Require(sigaction_nid_postfix(2, &action, &previous) == 0, "install write signal handler");
#endif
    }
    std::atomic<bool> changed{false};
    std::thread changer;
    if (reuse) {
        Require(pipe_nid_postfix(replacement) == 0, "create replacement write pipe");
        changer = std::thread([&] { Require(dup2_nid_postfix(replacement[1], state.output) == state.output, "replace blocked writer"); changed.store(true); });
    }
    if (close) changer = std::thread([&] { Require(close_nid_postfix(state.output) == 0, "close blocked writer"); changed.store(true); });
    if (aliasClose) Require(close_nid_postfix(alias) == 0, "close independent writer alias");
    if (peerClose) Require(close_nid_postfix(state.input) == 0, "close write peer");
    std::thread drainer;
    if (race) drainer = std::thread([&] { Drain(state, filled, 'f'); });
    bool prompt = true;
    if (signal) {
#ifdef _WIN32
        Require(sceKernelRaiseException(writer, 30) == 0, "raise exception during write");
#else
        Require(pthread_kill(writer->hostThread, SIGINT) == 0, "raise native signal during write");
#endif
    } else if (!completion && !wouldBlock && !close && !peerClose) {
        std::atomic<bool> requested{false};
        std::thread cancel([&] { Require(pthread_cancel_nid_postfix(writer) == 0, "cancel blocked writer"); requested.store(true); });
        prompt = Observe([&] { return requested.load(); });
        if (!prompt && !state.regular) Drain(state, partial ? 65536 : filled, partial ? 'w' : 'f');
        state.release.store(true);
        cancel.join();
    } else if (disabled || sce) Require(pthread_cancel_nid_postfix(writer) == 0, "pend disabled/SCE write cancellation");
    if (completion && !state.regular) Drain(state, filled, 'f');
    Require(Observe([&] { return writer->_finished.load(); }), "writer did not exit");
    void* result = nullptr;
    Require(scePthreadJoin(writer, &result) == 0, "join writer");
    exclusion.reset();
#ifdef _WIN32
    positioned.reset();
#endif
    if (drainer.joinable()) drainer.join();
    if (changer.joinable()) { Require(Observe([&] { return changed.load(); }), "descriptor change did not finish"); changer.join(); }
    Require(prompt, "cancellation required external write completion");
    Require(state.cleanups.load() == 1 && state.destructors.load() == 1, "write cleanup/TSD did not run exactly once");
    if (completion) {
        Require(state.result == expectedCount && state.returned.load(), "completed write lost its result");
        Require(result == (disabled || sce ? GUEST_CANCELED : reinterpret_cast<void*>(std::uintptr_t{7})), "completion/disabled result wrong");
        if (state.datagram) {
            Require(write_nid_postfix(state.output, "t", 1) == 1, "send following datagram");
            Require(fcntl_nid_postfix(state.input, 4, 6) == 0, "set datagram receive nonblocking");
            std::array<char, 8> record{};
            Require(recv_nid_postfix(state.input, record.data(), record.size(), 0) == expectedCount, "write changed a datagram boundary");
            if (!zero) Require(record[0] == 'w' && record[1] == 'w', "datagram payload changed");
            Require(recv_nid_postfix(state.input, record.data(), record.size(), 0) == 1 && record[0] == 't', "write consumed following datagram");
        } else if (!state.regular) Drain(state, expectedCount, 'w');
    } else if (signal) Require(state.signals.load() == 1 && result == reinterpret_cast<void*>(std::uintptr_t{7}) && state.returned.load() &&
        ((state.result == -1 && state.error == 4) || (state.socket && state.result > 0 && state.result <= 2)), "write signal close lost result or retained a guard");
    else if (wouldBlock) Require(state.returned.load() && (state.result > 0 || (state.result == -1 && state.error == 35)), "would-block write result wrong");
    else if (close) Require(state.returned.load() && ((state.result == -1 && state.error == 4) || (state.socket && state.result > 0 && state.result <= 2)), "close did not preserve completion or interrupt with EINTR");
    else if (peerClose) Require(state.returned.load() && ((state.result == -1 && (state.error == 32 || state.error == 54 || state.error == 57)) || (state.socket && state.result > 0 && state.result <= 2)), "peer close returned wrong write error");
    else if (race && result == reinterpret_cast<void*>(std::uintptr_t{7})) {
        Require(state.returned.load() && state.result > 0 && state.result <= 2, "racing write lost progress");
        Drain(state, static_cast<std::size_t>(state.result), 'w');
    }
    else if (!partial) Require((result == GUEST_CANCELED && !state.returned.load()) ||
        (!pending && state.socket && result == reinterpret_cast<void*>(std::uintptr_t{7}) && state.returned.load() && state.result > 0 && state.result <= 2), "blocked write neither canceled nor preserved completed bytes");
    if (state.socket && !completion && !partial && !peerClose && !race && state.result > 0) {
        Drain(state, filled, 'f');
        Drain(state, static_cast<std::size_t>(state.result), 'w');
    }
    if (state.regular) {
        const bool positionedWrite = state.api.starts_with("pwrite");
        const auto expectedOffset = !completion || zero || positionedWrite ? 0 : append ? 5 : 2;
        Require(lseek_nid_postfix(state.output, 0, 1) == expectedOffset, "write changed the wrong shared offset");
        Require(std::fseek(file, 0, SEEK_SET) == 0, "rewind write file");
        std::array<char, 3> contents{};
        Require(std::fread(contents.data(), 1, contents.size(), file) == contents.size(), "read write-file result");
        const char* expected = append || zero ? "xyz" : completion ? (positionedWrite ? "xww" : "wwz") : "xyz";
        Require(std::memcmp(contents.data(), expected, contents.size()) == 0, "write file modified unexpectedly");
        if (append) Require(std::fgetc(file) == 'w' && std::fgetc(file) == 'w' && std::fgetc(file) == EOF, "append write overwrote existing bytes");
        Require(std::fclose(file) == 0, "close write file");
    } else {
        if (reuse || signal) {
            Require(write_nid_postfix(state.output, "r", 1) == 1, "write reused descriptor");
            char byte = 0; Require(read_nid_postfix(replacement[0], &byte, 1) == 1 && byte == 'r', "old writer acted on replacement");
            Require(close_nid_postfix(replacement[0]) == 0 && close_nid_postfix(replacement[1]) == 0, "close replacement pipe");
        }
        if (!close) Require(close_nid_postfix(state.output) == 0, "close original writer");
        if (!aliasClose) Require(close_nid_postfix(alias) == 0, "close duplicate writer");
        if (partial) {
            Require(result == reinterpret_cast<void*>(std::uintptr_t{7}) && state.result > 0 && state.result < static_cast<std::int64_t>(state.bytes.size()), "partial write progress was canceled or lost");
            Drain(state, static_cast<std::size_t>(state.result), 'w');
            char extra = 0; Require(read_nid_postfix(state.input, &extra, 1) == 0, "write reported fewer bytes than it transferred");
        }
        if (state.scenario == "atomic") {
            Drain(state, filled, 'f');
            char extra = 0; Require(read_nid_postfix(state.input, &extra, 1) == 0, "atomic pipe write exposed a prefix");
        }
        if (race) { char extra = 0; Require(read_nid_postfix(state.input, &extra, 1) == 0, "racing cancellation hid transferred bytes"); }
        if (!peerClose) Require(close_nid_postfix(state.input) == 0, "close read peer");
    }
    if (signal) {
#ifdef _WIN32
        Require(sceKernelRemoveExceptionHandler(30) == 0, "remove write exception handler");
#else
        Require(sigaction_nid_postfix(2, &previous, nullptr) == 0, "restore write signal handler");
#endif
        signalState = nullptr;
    }
    Require(scePthreadKeyDelete(state.key) == 0, "delete writer TSD key");
}

int main(int argc, char** argv) {
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    Require(argc == 4, "expected write transport, API and scenario");
    const int rounds = std::strcmp(argv[3], "race") == 0 ? 8 : 1;
    for (int round = 0; round < rounds; ++round) Run(argv[1], argv[2], argv[3]);
}
