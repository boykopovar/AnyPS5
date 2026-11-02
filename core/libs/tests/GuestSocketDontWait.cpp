#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
constexpr int DontWait = 0x80;
constexpr int WaitAll = 0x40;
constexpr int NonBlocking = 0x20000000;
constexpr int WouldBlock = 35;
constexpr unsigned char FirstByte = 0x6a;
constexpr unsigned char SecondByte = 0x7b;

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

template<class TPredicate>
bool Within(TPredicate ready, std::chrono::milliseconds duration) {
    const auto deadline = Clock::now() + duration;
    while (!ready()) {
        if (Clock::now() >= deadline) return ready();
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

class Watchdog {
public:
    Watchdog() : thread([this] {
        std::unique_lock lock(mutex);
        if (!changed.wait_for(lock, 12s, [this] { return finished; })) {
            std::fputs("socket dontwait watchdog expired\n", stderr);
            std::fflush(stderr);
            std::_Exit(99);
        }
    }) {}
    ~Watchdog() {
        {
            std::lock_guard lock(mutex);
            finished = true;
        }
        changed.notify_one();
        thread.join();
    }
private:
    std::mutex mutex;
    std::condition_variable changed;
    bool finished = false;
    std::thread thread;
};

class Pair {
public:
    explicit Pair(bool nonblocking = false) {
        Require(socketpair_nid_postfix(1, 1 | (nonblocking ? NonBlocking : 0), 0, descriptors.data()) == 0, "cannot create socketpair");
    }
    Pair(int reader, int peer) : descriptors{reader, peer} {}
    ~Pair() {
        ReleasePeer();
        if (descriptors[0] >= 0) close_nid_postfix(descriptors[0]);
    }
    int Reader() const { return descriptors[0]; }
    int Peer() const { return descriptors[1]; }
    int Flags() const { return fcntl_nid_postfix(Reader(), 3, std::uint64_t{0}); }
    void Send(unsigned char value = FirstByte) {
        Require(send_nid_postfix(descriptors[1], &value, 1, 0x20000) == 1, "cannot queue a byte");
    }
    void CheckReadable(bool expected) const {
        GuestPollDescriptor entry{Reader(), 1, 0};
        const auto result = poll_nid_postfix(&entry, 1, expected ? 2000 : 0);
        Require(expected ? result == 1 && (entry.revents & 1) != 0 : result == 0, "socket readability differs from the queued bytes");
    }
    void ReleasePeer() noexcept {
        if (descriptors[1] < 0) return;
        shutdown_nid_postfix(descriptors[1], 2);
        close_nid_postfix(descriptors[1]);
        descriptors[1] = -1;
    }
private:
    std::array<int, 2> descriptors{-1, -1};
};

class Receive {
public:
    Receive(Pair& pair, int flags, std::size_t length = 2, std::atomic<bool>* gate = nullptr) : pair(pair), gate(gate), thread([this, flags, length] {
        ready.store(true, std::memory_order_release);
        while (this->gate != nullptr && !this->gate->load(std::memory_order_acquire)) std::this_thread::yield();
        *__error_nid_postfix() = 0;
        entered.store(true, std::memory_order_release);
        result = recv_nid_postfix(this->pair.Reader(), bytes.data(), length, flags);
        error = *__error_nid_postfix();
        finished.store(true, std::memory_order_release);
    }) {}
    ~Receive() {
        if (!thread.joinable()) return;
        if (gate != nullptr) gate->store(true, std::memory_order_release);
        pair.ReleasePeer();
        thread.join();
    }
    bool Ready() const { return ready.load(std::memory_order_acquire); }
    bool Entered() const { return entered.load(std::memory_order_acquire); }
    bool Finished() const { return finished.load(std::memory_order_acquire); }
    bool Wait(std::chrono::milliseconds duration) const { return Within([this] { return Finished(); }, duration); }
    void Join() { thread.join(); }
    void Prompt(const char* message) {
        Require(Within([this] { return Entered(); }, 2s), "receiver did not enter its call");
        if (Wait(500ms)) {
            Join();
            return;
        }
        pair.ReleasePeer();
        Require(Wait(2s), "receiver did not stop after peer shutdown");
        Join();
        throw std::runtime_error(message);
    }
    std::array<unsigned char, 2> bytes{0xcc, 0xdd};
    std::int64_t result = std::numeric_limits<std::int64_t>::min();
    int error = -1;
private:
    Pair& pair;
    std::atomic<bool>* gate;
    std::atomic<bool> ready{false};
    std::atomic<bool> entered{false};
    std::atomic<bool> finished{false};
    std::thread thread;
};

class SendCall {
public:
    SendCall(Pair& pair, int flags) : pair(pair), thread([this, flags] {
        *__error_nid_postfix() = 0;
        start = Clock::now();
        entered.store(true, std::memory_order_release);
        result = send_nid_postfix(this->pair.Reader(), &value, 1, flags | 0x20000);
        error = *__error_nid_postfix();
        end = Clock::now();
        finished.store(true, std::memory_order_release);
    }) {}
    ~SendCall() {
        if (!thread.joinable()) return;
        pair.ReleasePeer();
        thread.join();
    }
    bool Entered() const { return entered.load(std::memory_order_acquire); }
    bool Finished() const { return finished.load(std::memory_order_acquire); }
    bool Wait(std::chrono::milliseconds duration) const { return Within([this] { return Finished(); }, duration); }
    void Join() { thread.join(); }
    void Prompt(const char* message) {
        Require(Within([this] { return Entered(); }, 2s), "sender did not enter its call");
        if (Wait(500ms)) {
            Join();
            return;
        }
        pair.ReleasePeer();
        Require(Wait(2s), "sender did not stop after peer shutdown");
        Join();
        throw std::runtime_error(message);
    }
    Clock::duration Elapsed() const { return end - start; }
    std::int64_t result = std::numeric_limits<std::int64_t>::min();
    int error = -1;
private:
    Pair& pair;
    unsigned char value = FirstByte;
    Clock::time_point start{}, end{};
    std::atomic<bool> entered{false};
    std::atomic<bool> finished{false};
    std::thread thread;
};

void AvailableWithWaitAll(Pair& pair) {
    const auto flags = pair.Flags();
    Require(flags >= 0 && (flags & 4) == 0, "WAITALL test socket is not blocking");
    pair.CheckReadable(false);
    pair.Send();
    pair.CheckReadable(true);
    Receive call(pair, DontWait | WaitAll);
    call.Prompt("MSG_DONTWAIT | MSG_WAITALL waited for a second byte despite one available byte");
    Require(call.result == 1 && call.bytes[0] == FirstByte && call.bytes[1] == 0xdd, "DONTWAIT | WAITALL did not return exactly the available byte");
    Require(pair.Flags() == flags, "DONTWAIT | WAITALL changed the socket mode");
}

void AvailableWithWaitAll() {
    Pair pair;
    AvailableWithWaitAll(pair);
}

void OrdinaryWaitAll() {
    Pair pair;
    const auto flags = pair.Flags();
    pair.Send();
    pair.CheckReadable(true);
    Receive call(pair, WaitAll);
    Require(Within([&] { return call.Entered(); }, 2s), "blocking WAITALL did not enter");
    Require(!call.Wait(100ms), "ordinary WAITALL returned before its second byte");
    pair.Send(SecondByte);
    call.Prompt("ordinary WAITALL did not return after both bytes arrived");
    Require(call.result == 2 && call.bytes[0] == FirstByte && call.bytes[1] == SecondByte, "ordinary WAITALL did not retain blocking semantics");
    Require(pair.Flags() == flags, "ordinary WAITALL changed the socket mode");
}

void OrdinaryDontWait(bool available) {
    Pair pair;
    const auto flags = pair.Flags();
    pair.CheckReadable(false);
    if (available) pair.Send();
    pair.CheckReadable(available);
    Receive call(pair, DontWait);
    call.Prompt("MSG_DONTWAIT blocked");
    if (available) Require(call.result == 1 && call.bytes[0] == FirstByte && call.bytes[1] == 0xdd, "DONTWAIT did not return the available byte");
    else Require(call.result == -1 && call.error == WouldBlock && call.bytes[0] == 0xcc && call.bytes[1] == 0xdd, "empty DONTWAIT did not report EAGAIN without modifying the buffer");
    Require(pair.Flags() == flags, "DONTWAIT changed the socket mode");
}

void PeekWithWaitAll() {
    Pair pair;
    const auto flags = pair.Flags();
    pair.Send();
    pair.CheckReadable(true);
    Receive peek(pair, DontWait | WaitAll | 2);
    peek.Prompt("DONTWAIT | PEEK | WAITALL waited for another byte");
    Require(peek.result == 1 && peek.bytes[0] == FirstByte && peek.bytes[1] == 0xdd, "DONTWAIT peek did not return only the available byte");
    pair.CheckReadable(true);
#ifdef _WIN32
    Receive unsupported(pair, WaitAll | 2);
    unsupported.Prompt("blocking PEEK | WAITALL did not preserve its unsupported result");
    Require(unsupported.result == -1 && unsupported.error == 45 && unsupported.bytes[0] == 0xcc && unsupported.bytes[1] == 0xdd, "blocking Windows PEEK | WAITALL did not report EOPNOTSUPP");
    pair.CheckReadable(true);
#endif
    Receive consume(pair, DontWait);
    consume.Prompt("receiving the peeked byte blocked");
    Require(consume.result == 1 && consume.bytes[0] == FirstByte, "PEEK consumed the queued byte");
    pair.CheckReadable(false);
    Require(pair.Flags() == flags, "PEEK changed the socket mode");
}

void AlongsideBlockingRead() {
    Pair pair;
    const auto flags = pair.Flags();
    Receive blocking(pair, 0, 1);
    Require(Within([&] { return blocking.Entered(); }, 2s), "ordinary receiver did not enter");
    Require(!blocking.Wait(100ms), "ordinary receiver did not block on an empty socket");
    Receive immediate(pair, DontWait, 1);
    immediate.Prompt("DONTWAIT waited behind another blocking receiver");
    Require(immediate.result == -1 && immediate.error == WouldBlock, "DONTWAIT beside a blocking read did not return EAGAIN");
    Require(!blocking.Finished(), "DONTWAIT disturbed an ordinary blocking receive");
    pair.Send();
    blocking.Prompt("ordinary receiver did not wake when its byte arrived");
    Require(blocking.result == 1 && blocking.bytes[0] == FirstByte, "ordinary receiver lost its queued byte");
    Require(pair.Flags() == flags, "concurrent DONTWAIT changed the socket mode");
}

void CheckRestoredBlocking(Pair& pair) {
    const auto flags = pair.Flags();
    Require(flags >= 0 && (flags & 4) == 0, "restored socket did not report blocking mode");
    Receive immediate(pair, DontWait, 1);
    immediate.Prompt("DONTWAIT blocked after restoring blocking mode");
    Require(immediate.result == -1 && immediate.error == WouldBlock, "empty restored DONTWAIT did not report EAGAIN");
    Receive blocking(pair, 0, 1);
    Require(Within([&] { return blocking.Entered(); }, 2s), "restored blocking receiver did not enter");
    Require(!blocking.Wait(100ms), "restored ordinary receive returned without a byte");
    pair.Send();
    blocking.Prompt("restored blocking receiver did not wake when its byte arrived");
    Require(blocking.result == 1 && blocking.bytes[0] == FirstByte, "restored blocking receiver lost its byte");
    Require(pair.Flags() == flags, "receiving changed the restored blocking mode");
    AvailableWithWaitAll(pair);
}

void SetMode(Pair& pair, unsigned method, bool enabled) {
    if (method == 0) {
        Require(fcntl_nid_postfix(pair.Reader(), 4, std::uint64_t{enabled ? 4u : 0u}) == 0, "F_SETFL mode change failed");
    } else if (method == 1) {
        const int value = enabled ? 1 : 0;
        Require(setsockopt_nid_postfix(pair.Reader(), 0xffff, 0x1200, &value, sizeof(value)) == 0, "SO_NBIO mode change failed");
    } else {
        std::uint32_t value = enabled ? 1 : 0;
        Require(ioctl_nid_postfix(pair.Reader(), 0x8004667e, &value) == 0, "FIONBIO mode change failed");
    }
}

void ModeChanges() {
    for (unsigned method = 0; method < 3; ++method) {
        Pair pair;
        const auto flags = pair.Flags();
        Require(flags >= 0 && (flags & 4) == 0, "mode-change socket did not start blocking");
        SetMode(pair, method, true);
        Require(pair.Flags() == (flags | 4), "enabling nonblocking mode was not reflected by F_GETFL");
        Receive immediate(pair, 0, 1);
        immediate.Prompt("ordinary receive blocked after enabling nonblocking mode");
        Require(immediate.result == -1 && immediate.error == WouldBlock, "nonblocking ordinary receive did not report EAGAIN");
        SetMode(pair, method, false);
        Require(pair.Flags() == flags, "disabling nonblocking mode did not restore F_GETFL");
        CheckRestoredBlocking(pair);
    }
}

class Descriptor {
public:
    explicit Descriptor(int value) : value(value) { Require(value >= 0, "cannot create an accept test socket"); }
    ~Descriptor() { if (value >= 0) close_nid_postfix(value); }
    int Get() const { return value; }
    int Take() { const int result = value; value = -1; return result; }
private:
    int value;
};

std::unique_ptr<Pair> TcpPair() {
    Descriptor listener(socket_nid_postfix(2, 1, 0));
    Descriptor client(socket_nid_postfix(2, 1, 0));
    const int capacity = 4096;
    Require(setsockopt_nid_postfix(listener.Get(), 0xffff, 0x1002, &capacity, sizeof(capacity)) == 0, "cannot limit the pressure peer's receive buffer");
    Require(setsockopt_nid_postfix(client.Get(), 0xffff, 0x1001, &capacity, sizeof(capacity)) == 0, "cannot limit the pressure sender's send buffer");
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener.Get(), address.data(), address.size()) == 0 && listen_nid_postfix(listener.Get(), 4) == 0, "cannot start the pressure listener");
    std::uint32_t addressBytes = address.size();
    Require(getsockname_nid_postfix(listener.Get(), address.data(), &addressBytes) == 0 && addressBytes == address.size(), "cannot read the pressure listener's address");
    Require(connect_nid_postfix(client.Get(), address.data(), address.size()) == 0, "pressure client did not connect");
    Descriptor accepted(accept_nid_postfix(listener.Get(), nullptr, nullptr));
    return std::make_unique<Pair>(client.Take(), accepted.Take());
}

void FillSendQueue(Pair& pair) {
    Require(fcntl_nid_postfix(pair.Reader(), 4, std::uint64_t{4}) == 0, "cannot make the pressure sender nonblocking");
    const auto deadline = Clock::now() + 2s;
    std::array<unsigned char, 1024> bytes{};
    std::size_t total = 0;
    unsigned stable = 0;
    bool tiny = false;
    while (total < 8u * 1024u * 1024u && Clock::now() < deadline && stable < 5) {
        const auto sent = send_nid_postfix(pair.Reader(), bytes.data(), tiny ? 1 : bytes.size(), DontWait | 0x20000);
        if (sent > 0) {
            total += static_cast<std::size_t>(sent);
            stable = 0;
        } else {
            Require(sent == -1 && *__error_nid_postfix() == WouldBlock, "filling the send queue returned an unexpected error");
            if (tiny) ++stable;
            else tiny = true;
            std::this_thread::sleep_for(20ms);
        }
    }
    Require(total != 0 && stable == 5, "the TCP send queue did not reach stable one-byte backpressure");
    Require(fcntl_nid_postfix(pair.Reader(), 4, std::uint64_t{0}) == 0 && (pair.Flags() & 4) == 0, "cannot restore the pressure sender's blocking mode");
}

void SendAfterPeerDrains() {
    auto pair = TcpPair();
    FillSendQueue(*pair);
    const auto flags = pair->Flags();
    SendCall immediate(*pair, DontWait);
    immediate.Prompt("DONTWAIT send blocked on a full queue");
    Require(immediate.result == -1 && immediate.error == WouldBlock, "DONTWAIT send did not report EAGAIN for a full queue");
    SendCall blocking(*pair, 0);
    Require(Within([&] { return blocking.Entered(); }, 2s), "blocking pressure sender did not enter");
    Require(!blocking.Wait(100ms), "blocking send returned before the full peer drained");
    Require(fcntl_nid_postfix(pair->Peer(), 4, std::uint64_t{4}) == 0, "cannot make the draining peer nonblocking");
    std::array<unsigned char, 16384> scratch{};
    std::size_t drained = 0;
    const auto deadline = Clock::now() + 2s;
    while (!blocking.Finished() && Clock::now() < deadline) {
        const auto received = recv_nid_postfix(pair->Peer(), scratch.data(), scratch.size(), DontWait);
        if (received > 0) drained += static_cast<std::size_t>(received);
        else {
            Require(received == -1 && *__error_nid_postfix() == WouldBlock, "draining the full peer failed");
            std::this_thread::sleep_for(1ms);
        }
    }
    blocking.Prompt("blocking send did not wake after its peer drained");
    Require(drained != 0 && blocking.result == 1, "blocking send did not transfer its byte after peer progress");
    Require(pair->Flags() == flags, "send pressure changed the sender's mode");
}

void HalfClosedSendTimeout() {
    auto pair = TcpPair();
    FillSendQueue(*pair);
    const auto flags = pair->Flags();
    const std::array<std::int64_t, 2> timeout{0, 60000};
    Require(setsockopt_nid_postfix(pair->Reader(), 0xffff, 0x1005, timeout.data(), sizeof(timeout)) == 0, "cannot set the pressure send timeout");
    Require(shutdown_nid_postfix(pair->Peer(), 1) == 0, "cannot half-close the pressure peer's write direction");
    GuestPollDescriptor eofReady{pair->Reader(), 1, 0};
    Require(poll_nid_postfix(&eofReady, 1, 2000) == 1 && (eofReady.revents & 0x11) != 0, "the pressure sender did not observe its peer's FIN");
    Receive eof(*pair, DontWait, 1);
    eof.Prompt("receiving the pressure peer's FIN blocked");
    Require(eof.result == 0, "half-closing the peer did not produce read EOF");
    SendCall timed(*pair, 0);
    timed.Prompt("a full-queue send ignored its timeout after read EOF");
    Require(timed.result == -1 && timed.error == WouldBlock, "timed full-queue send after read EOF did not report EAGAIN");
    Require(timed.Elapsed() >= 40ms && timed.Elapsed() < 1500ms, "full-queue send did not honor its receive-independent timeout");
    Require(pair->Flags() == flags, "timed send changed the sender's mode");
}

void BlockingAccept() {
    Descriptor listener(socket_nid_postfix(2, 1, 0));
    Descriptor client(socket_nid_postfix(2, 1, 0));
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener.Get(), address.data(), address.size()) == 0 && listen_nid_postfix(listener.Get(), 4) == 0, "cannot start the accept test listener");
    std::uint32_t addressBytes = address.size();
    Require(getsockname_nid_postfix(listener.Get(), address.data(), &addressBytes) == 0 && addressBytes == address.size(), "cannot read the accept test address");
    const std::array<std::int64_t, 2> timeout{2, 0};
    Require(setsockopt_nid_postfix(listener.Get(), 0xffff, 0x1006, timeout.data(), sizeof(timeout)) == 0, "cannot bound the accept test's wait");
    const int flags = fcntl_nid_postfix(listener.Get(), 3, std::uint64_t{0});
    Require(flags >= 0 && (flags & 4) == 0, "accept listener did not start blocking");
    std::atomic<bool> entered{false}, finished{false};
    int accepted = -1;
    std::thread thread([&] {
        entered.store(true, std::memory_order_release);
        accepted = accept_nid_postfix(listener.Get(), nullptr, nullptr);
        finished.store(true, std::memory_order_release);
    });
    struct Cleanup {
        std::thread& thread;
        Descriptor& client;
        const std::array<std::uint8_t, 16>& address;
        std::atomic<bool>& finished;
        int& accepted;
        ~Cleanup() {
            if (thread.joinable()) {
                if (!finished.load(std::memory_order_acquire)) connect_nid_postfix(client.Get(), address.data(), address.size());
                thread.join();
            }
            if (accepted >= 0) close_nid_postfix(accepted);
        }
    } cleanup{thread, client, address, finished, accepted};
    Require(Within([&] { return entered.load(std::memory_order_acquire); }, 2s), "blocking accept did not enter");
    Require(!Within([&] { return finished.load(std::memory_order_acquire); }, 100ms), "blocking accept returned before a client connected");
    Require(connect_nid_postfix(client.Get(), address.data(), address.size()) == 0, "accept test client did not connect");
    Require(Within([&] { return finished.load(std::memory_order_acquire); }, 2s), "blocking accept did not finish after a client connected");
    thread.join();
    Require(accepted >= 0 && fcntl_nid_postfix(accepted, 3, std::uint64_t{0}) == flags, "accepted socket did not inherit logical blocking mode");
    Require(fcntl_nid_postfix(listener.Get(), 3, std::uint64_t{0}) == flags, "blocking accept changed its listener's mode");
    const int reader = accepted;
    accepted = -1;
    Pair pair(reader, client.Take());
    CheckRestoredBlocking(pair);
}

void CompetingReaders(bool nonblocking) {
    constexpr std::size_t count = 8;
    for (unsigned round = 0; round < 64; ++round) {
        Pair pair(nonblocking);
        const auto flags = pair.Flags();
        Require(flags >= 0 && ((flags & 4) != 0) == nonblocking, "race socket has the wrong mode");
        pair.CheckReadable(false);
        pair.Send();
        pair.CheckReadable(true);
        std::atomic<bool> gate{false};
        std::array<std::unique_ptr<Receive>, count> readers;
        for (auto& reader : readers) reader = std::make_unique<Receive>(pair, DontWait, 1, &gate);
        const auto all = [&](auto state) {
            for (const auto& reader : readers) if (!(reader.get()->*state)()) return false;
            return true;
        };
        Require(Within([&] { return all(&Receive::Ready); }, 2s), "competing receivers did not reach their gate");
        gate.store(true, std::memory_order_release);
        Require(Within([&] { return all(&Receive::Entered); }, 2s), "competing receivers did not enter");
        const bool prompt = Within([&] { return all(&Receive::Finished); }, 500ms);
        if (!prompt) pair.ReleasePeer();
        Require(Within([&] { return all(&Receive::Finished); }, 2s), "competing receivers did not stop after peer shutdown");
        for (auto& reader : readers) reader->Join();
        Require(prompt, "MSG_DONTWAIT blocked after another receiver consumed the ready byte");
        unsigned received = 0, again = 0;
        for (const auto& reader : readers) {
            received += reader->result == 1 && reader->bytes[0] == FirstByte;
            again += reader->result == -1 && reader->error == WouldBlock;
            Require(reader->bytes[1] == 0xdd, "one-byte receive wrote beyond its destination");
        }
        Require(received == 1 && again == count - 1, "competing DONTWAIT receivers did not consume exactly one byte");
        Require(pair.Flags() == flags, "competing DONTWAIT receives changed the socket mode");
    }
}

}

int main() {
    Watchdog watchdog;
    try {
        AvailableWithWaitAll();
        OrdinaryWaitAll();
        OrdinaryDontWait(false);
        OrdinaryDontWait(true);
        PeekWithWaitAll();
        AlongsideBlockingRead();
        ModeChanges();
        BlockingAccept();
        SendAfterPeerDrains();
        HalfClosedSendTimeout();
        CompetingReaders(true);
        CompetingReaders(false);
        std::puts("socket DONTWAIT preserves per-call and blocking receive semantics");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "socket DONTWAIT: %s\n", error.what());
        return 1;
    }
}
