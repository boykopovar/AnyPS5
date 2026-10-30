#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
constexpr int DontWait = 0x80;
constexpr int WaitAll = 0x40;
constexpr int WouldBlock = 35;
constexpr std::size_t ChunkBytes = 16384;

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
            std::fputs("socket transfer progress watchdog expired\n", stderr);
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

class Descriptor {
public:
    explicit Descriptor(int value) : value(value) { Require(value >= 0, "cannot create a TCP socket"); }
    ~Descriptor() { if (value >= 0) close_nid_postfix(value); }
    int Get() const { return value; }
    int Take() { const int result = value; value = -1; return result; }
private:
    int value;
};

class Pair {
public:
    Pair(int sender, int receiver) : descriptors{sender, receiver} {}
    ~Pair() { for (const int descriptor : descriptors) close_nid_postfix(descriptor); }
    int Sender() const { return descriptors[0]; }
    int Receiver() const { return descriptors[1]; }
    void Interrupt() noexcept {
        for (const int descriptor : descriptors) shutdown_nid_postfix(descriptor, 2);
    }
private:
    std::array<int, 2> descriptors;
};

std::unique_ptr<Pair> TcpPair(bool limited = false) {
    Descriptor listener(socket_nid_postfix(2, 1, 0));
    Descriptor client(socket_nid_postfix(2, 1, 0));
    if (limited) {
        const int capacity = 4096;
        Require(setsockopt_nid_postfix(listener.Get(), 0xffff, 0x1002, &capacity, sizeof(capacity)) == 0, "cannot limit the TCP receive buffer");
        Require(setsockopt_nid_postfix(client.Get(), 0xffff, 0x1001, &capacity, sizeof(capacity)) == 0, "cannot limit the TCP send buffer");
    }
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener.Get(), address.data(), address.size()) == 0 && listen_nid_postfix(listener.Get(), 4) == 0, "cannot start the TCP listener");
    std::uint32_t addressBytes = address.size();
    Require(getsockname_nid_postfix(listener.Get(), address.data(), &addressBytes) == 0 && addressBytes == address.size(), "cannot read the TCP listener's address");
    Require(connect_nid_postfix(client.Get(), address.data(), address.size()) == 0, "TCP client did not connect");
    Descriptor accepted(accept_nid_postfix(listener.Get(), nullptr, nullptr));
    return std::make_unique<Pair>(client.Take(), accepted.Take());
}

void SetTimeout(int descriptor, bool send, std::chrono::milliseconds duration) {
    const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    const std::array<std::int64_t, 2> timeout{microseconds / 1000000, microseconds % 1000000};
    Require(setsockopt_nid_postfix(descriptor, 0xffff, send ? 0x1005 : 0x1006, timeout.data(), sizeof(timeout)) == 0, "cannot set the transfer timeout");
}

unsigned char Pattern(std::size_t offset) {
    return static_cast<unsigned char>(1 + offset % 127);
}

std::vector<unsigned char> Payload(std::size_t size) {
    std::vector<unsigned char> result(size);
    for (std::size_t offset = 0; offset < size; ++offset) result[offset] = Pattern(offset);
    return result;
}

class TransferCall {
public:
    TransferCall(Pair& pair, bool send, std::size_t length, int flags) : bytes(send ? Payload(length) : std::vector<unsigned char>(length, 0xe7)), pair(pair), thread([this, send, flags] {
        *__error_nid_postfix() = 0;
        start = Clock::now();
        entered.store(true, std::memory_order_release);
        result = send ? send_nid_postfix(this->pair.Sender(), bytes.data(), bytes.size(), flags | 0x20000) :
            recv_nid_postfix(this->pair.Receiver(), bytes.data(), bytes.size(), flags);
        error = *__error_nid_postfix();
        end = Clock::now();
        finished.store(true, std::memory_order_release);
    }) {}
    ~TransferCall() {
        if (!thread.joinable()) return;
        pair.Interrupt();
        thread.join();
    }
    bool Entered() const { return entered.load(std::memory_order_acquire); }
    bool Finished() const { return finished.load(std::memory_order_acquire); }
    bool Wait(std::chrono::milliseconds duration) const { return Within([this] { return Finished(); }, duration); }
    void Join() { thread.join(); }
    Clock::duration Elapsed() const { return end - start; }
    Clock::time_point End() const { return end; }
    std::vector<unsigned char> bytes;
    std::int64_t result = std::numeric_limits<std::int64_t>::min();
    int error = -1;
private:
    Pair& pair;
    Clock::time_point start{}, end{};
    std::atomic<bool> entered{false};
    std::atomic<bool> finished{false};
    std::thread thread;
};

void CheckPayload(const TransferCall& call, std::size_t length) {
    Require(call.result == static_cast<std::int64_t>(length), "receive did not return exactly the delivered payload");
    for (std::size_t offset = 0; offset < call.bytes.size(); ++offset) {
        Require(call.bytes[offset] == (offset < length ? Pattern(offset) : 0xe7), "receive lost payload bytes or wrote beyond its partial result");
    }
}

void ReceiveNoProgress() {
    auto pair = TcpPair();
    SetTimeout(pair->Receiver(), false, 300ms);
    TransferCall call(*pair, false, 5 * ChunkBytes, WaitAll);
    Require(call.Wait(2s), "empty WAITALL ignored its receive timeout");
    call.Join();
    Require(call.result == -1 && call.error == WouldBlock && call.Elapsed() >= 250ms, "WAITALL without progress did not time out with EAGAIN");
    Require(std::all_of(call.bytes.begin(), call.bytes.end(), [](auto value) { return value == 0xe7; }), "empty WAITALL modified its buffer");
}

void ReceiveProgress(bool complete) {
    auto pair = TcpPair();
    SetTimeout(pair->Receiver(), false, 300ms);
    SetTimeout(pair->Sender(), true, 2s);
    const auto payload = Payload(4 * ChunkBytes);
    Require(send_nid_postfix(pair->Sender(), payload.data(), ChunkBytes, 0x20000) == ChunkBytes, "cannot queue the first progress chunk");
    TransferCall call(*pair, false, (complete ? 4 : 5) * ChunkBytes, WaitAll);
    Require(Within([&] { return call.Entered(); }, 2s), "progress receiver did not enter its call");
    Clock::time_point lastChunk;
    for (std::size_t chunk = 1; chunk < 4; ++chunk) {
        Require(!call.Wait(120ms), "WAITALL expired despite regular receive progress");
        lastChunk = Clock::now();
        Require(send_nid_postfix(pair->Sender(), payload.data() + chunk * ChunkBytes, ChunkBytes, 0x20000) == ChunkBytes, "cannot deliver a progress chunk");
    }
    Require(call.Wait(2s), "WAITALL did not finish after completion or an idle timeout");
    call.Join();
    CheckPayload(call, payload.size());
    Require(call.Elapsed() >= 330ms, "WAITALL did not span its original timeout");
    if (!complete) Require(call.End() - lastChunk >= 250ms, "partial WAITALL timed out before a full idle interval after its last progress");
    std::printf("WAITALL %s: %lld bytes, %lld ms total, %lld ms since final chunk\n", complete ? "complete" : "idle partial",
        static_cast<long long>(call.result), static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(call.Elapsed()).count()),
        static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(call.End() - lastChunk).count()));
}

std::size_t FillSendQueue(Pair& pair) {
    Require(fcntl_nid_postfix(pair.Sender(), 4, std::uint64_t{4}) == 0, "cannot make the pressure sender nonblocking");
    const auto deadline = Clock::now() + 2s;
    std::array<unsigned char, 1024> bytes{};
    std::size_t total = 0;
    unsigned stable = 0;
    bool tiny = false;
    while (total < 8u * 1024u * 1024u && Clock::now() < deadline && stable < 5) {
        const auto sent = send_nid_postfix(pair.Sender(), bytes.data(), tiny ? 1 : bytes.size(), DontWait | 0x20000);
        if (sent > 0) {
            total += static_cast<std::size_t>(sent);
            stable = 0;
        } else {
            Require(sent == -1 && *__error_nid_postfix() == WouldBlock, "filling the TCP send queue failed");
            if (tiny) ++stable;
            else tiny = true;
            std::this_thread::sleep_for(20ms);
        }
    }
    Require(total != 0 && stable == 5, "TCP did not reach stable one-byte backpressure");
    Require(fcntl_nid_postfix(pair.Sender(), 4, std::uint64_t{0}) == 0, "cannot restore blocking mode after priming");
    return total;
}

void LargeSend() {
    auto pair = TcpPair(true);
    const auto priming = FillSendQueue(*pair);
    constexpr std::size_t length = 1024 * 1024;
    TransferCall immediate(*pair, true, length, DontWait);
    Require(immediate.Wait(500ms), "large DONTWAIT send blocked on a full queue");
    immediate.Join();
    Require(immediate.result == -1 && immediate.error == WouldBlock, "large DONTWAIT send did not report EAGAIN on a full queue");
    SetTimeout(pair->Sender(), true, 200ms);
    TransferCall timed(*pair, true, length, 0);
    Require(timed.Wait(2s), "large send ignored its timeout without peer progress");
    timed.Join();
    Require(timed.result == -1 && timed.error == WouldBlock && timed.Elapsed() >= 150ms, "large send without progress did not time out with EAGAIN");
    SetTimeout(pair->Sender(), true, 0ms);
    TransferCall blocking(*pair, true, length, 0);
    Require(Within([&] { return blocking.Entered(); }, 2s), "large blocking sender did not enter its call");
    Require(!blocking.Wait(100ms), "large blocking send returned before its peer drained");
    std::array<unsigned char, ChunkBytes> scratch{};
    std::size_t received = 0;
    const auto deadline = Clock::now() + 3s;
    while (received < priming + length && Clock::now() < deadline) {
        const auto count = recv_nid_postfix(pair->Receiver(), scratch.data(), std::min(scratch.size(), priming + length - received), DontWait);
        if (count > 0) {
            for (std::size_t index = 0; index < static_cast<std::size_t>(count); ++index) {
                const auto offset = received + index;
                Require(scratch[index] == (offset < priming ? 0 : Pattern(offset - priming)), "TCP did not retain the priming and large payload byte sequence");
            }
            received += static_cast<std::size_t>(count);
        } else {
            Require(count == -1 && *__error_nid_postfix() == WouldBlock, "draining the large TCP send failed");
            std::this_thread::sleep_for(1ms);
        }
    }
    Require(received == priming + length && blocking.Wait(2s), "large blocking send did not finish after its peer drained");
    blocking.Join();
    Require(blocking.result == length, "large blocking send did not account for every delivered payload byte");
    std::printf("large send: %zu priming bytes followed by %zu payload bytes\n", priming, length);
}

}

int main(int argc, char** argv) {
    Watchdog watchdog;
    try {
        const char* mode = argc == 2 ? argv[1] : "all";
        Require(argc <= 2, "expected at most one transfer progress mode");
        if (std::strcmp(mode, "all") == 0 || std::strcmp(mode, "send") == 0) LargeSend();
        if (std::strcmp(mode, "all") == 0 || std::strcmp(mode, "receive-empty") == 0) ReceiveNoProgress();
        if (std::strcmp(mode, "all") == 0 || std::strcmp(mode, "receive-partial") == 0) ReceiveProgress(false);
        if (std::strcmp(mode, "all") == 0 || std::strcmp(mode, "receive-complete") == 0) ReceiveProgress(true);
        Require(std::strcmp(mode, "all") == 0 || std::strcmp(mode, "send") == 0 || std::strcmp(mode, "receive-empty") == 0 ||
            std::strcmp(mode, "receive-partial") == 0 || std::strcmp(mode, "receive-complete") == 0, "unknown transfer progress mode");
        std::puts("socket transfer progress preserves payloads and inactivity timeouts");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "socket transfer progress: %s\n", error.what());
        return 1;
    }
}
