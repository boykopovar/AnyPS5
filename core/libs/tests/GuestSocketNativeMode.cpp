#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include <array>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "native socket mode check failed at line %d, errno %d, guest errno %d\n",
            line, errno, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int GetFlags = 3;
constexpr int SetFlags = 4;
constexpr int ReadWrite = 2;
constexpr int NonBlocking = 4;
using Clock = std::chrono::steady_clock;

static bool WouldBlock(int error) {
    return error == EAGAIN || error == EWOULDBLOCK;
}

struct NativeDescriptor {
    int value = -1;

    ~NativeDescriptor() {
        if (value >= 0) ::close(value);
    }
};

struct RetainedSocket {
    std::shared_ptr<void> owner;
    int native = -1;

    KernelSocketPoll::NativeOwner* Transport() const {
        auto* transport = static_cast<KernelSocketPoll::NativeOwner*>(owner.get());
        Require(transport != nullptr && transport->transfer != nullptr);
        return transport;
    }
};

static RetainedSocket Lookup(int descriptor) {
    RetainedSocket result;
    std::uintptr_t native = 0;
    Require(KernelLookupSocketNative_nid_no_patch(descriptor, &result.owner, &native));
    Require(result.owner != nullptr);
    result.native = static_cast<int>(native);
    return result;
}

struct SocketPair {
    std::array<int, 2> descriptors{-1, -1};
    RetainedSocket socket;
    RetainedSocket peer;
    NativeDescriptor alias;

    explicit SocketPair(bool nonblocking) {
        Require(socketpair_nid_postfix(1, 1, 0, descriptors.data()) == 0);
        if (nonblocking) Require(fcntl_nid_postfix(descriptors[0], SetFlags, ReadWrite | NonBlocking) == 0);
        Require(fcntl_nid_postfix(descriptors[0], GetFlags) == (ReadWrite | (nonblocking ? NonBlocking : 0)));
        socket = Lookup(descriptors[0]);
        peer = Lookup(descriptors[1]);
        alias.value = ::dup(socket.native);
        Require(alias.value >= 0 && alias.value != socket.native);
    }

    ~SocketPair() {
        for (const int descriptor : descriptors) {
            if (descriptor >= 0) close_nid_postfix(descriptor);
        }
    }

    void SetNativeNonblocking(bool nonblocking, const char* mode) {
        const int flags = ::fcntl(alias.value, F_GETFL);
        Require(flags >= 0);
        Require(::fcntl(alias.value, F_SETFL, nonblocking ? flags | O_NONBLOCK : flags & ~O_NONBLOCK) == 0);
        const int nativeFlags = ::fcntl(socket.native, F_GETFL);
        const int aliasFlags = ::fcntl(alias.value, F_GETFL);
        const int guestFlags = fcntl_nid_postfix(descriptors[0], GetFlags);
        Require(nativeFlags >= 0 && aliasFlags >= 0 && guestFlags >= 0);
        Require(((nativeFlags & O_NONBLOCK) != 0) == nonblocking);
        Require(((aliasFlags & O_NONBLOCK) != 0) == nonblocking);
        std::fprintf(stderr, "%s: guest flags=0x%x, native flags=0x%x, alias flags=0x%x\n",
            mode, guestFlags, nativeFlags, aliasFlags);
    }

    void CloseGuestEndpoint() {
        const int descriptor = descriptors[0];
        descriptors[0] = -1;
        Require(descriptor >= 0 && close_nid_postfix(descriptor) == 0);
    }
};

struct TransferResult {
    int value = -2;
    int error = 0;
    char byte = '?';
    std::int64_t milliseconds = 0;
    bool supplied = false;
    int suppliedBytes = -2;
    int suppliedError = 0;
    bool released = false;
    int releaseResult = -2;
    int releaseError = 0;
};

static TransferResult TransferBounded(const SocketPair& pair, bool send, bool supplyData = false) {
    auto* transport = pair.socket.Transport();
    std::mutex mutex;
    std::condition_variable changed;
    bool finished = false;
    TransferResult result;
    const auto started = Clock::now();
    std::jthread release([&] {
        std::unique_lock lock(mutex);
        if (supplyData) {
            if (changed.wait_for(lock, std::chrono::milliseconds(100), [&] { return finished; })) return;
            lock.unlock();
            const char byte = 'z';
            result.supplied = true;
            result.suppliedBytes = static_cast<int>(::send(pair.peer.native, &byte, 1, MSG_DONTWAIT | MSG_NOSIGNAL));
            result.suppliedError = errno;
            lock.lock();
        }
        if (changed.wait_for(lock, std::chrono::milliseconds(1500), [&] { return finished; })) return;
        lock.unlock();
        result.released = true;
        result.releaseResult = ::shutdown(pair.peer.native, SHUT_RDWR);
        result.releaseError = errno;
    });
    errno = 0;
    result.value = transport->transfer(transport, send, &result.byte, 1);
    result.error = errno;
    result.milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();
    {
        std::lock_guard lock(mutex);
        finished = true;
    }
    changed.notify_one();
    release.join();
    return result;
}

static void ReportTransfer(const char* mode, const TransferResult& result) {
    std::fprintf(stderr, "%s: transfer=%d errno=%d byte=%d elapsed=%lldms, supplied=%d bytes=%d errno=%d, released=%d shutdown=%d errno=%d\n",
        mode, result.value, result.error, static_cast<unsigned char>(result.byte), static_cast<long long>(result.milliseconds),
        result.supplied, result.suppliedBytes, result.suppliedError, result.released, result.releaseResult, result.releaseError);
}

static void CheckReceiveNonblocking() {
    constexpr auto mode = "receive-nonblocking";
    SocketPair pair{false};
    pair.SetNativeNonblocking(true, mode);
    char byte = '?';
    errno = 0;
    const auto started = Clock::now();
    const auto nativeResult = ::recv(pair.socket.native, &byte, 1, 0);
    const int nativeError = errno;
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();
    std::fprintf(stderr, "%s: native empty read=%lld errno=%d byte=%d elapsed=%lldms\n",
        mode, static_cast<long long>(nativeResult), nativeError, static_cast<unsigned char>(byte), static_cast<long long>(milliseconds));
    Require(nativeResult == -1 && WouldBlock(nativeError) && byte == '?');
    pair.CloseGuestEndpoint();
    const auto result = TransferBounded(pair, false);
    ReportTransfer(mode, result);
    Require(!result.released && result.value == -1 && WouldBlock(result.error) && result.byte == '?');
    Require(result.milliseconds < 1000);
}

static void CheckReceiveBlocking() {
    constexpr auto mode = "receive-blocking";
    SocketPair pair{true};
    pair.SetNativeNonblocking(false, mode);
    pair.CloseGuestEndpoint();
    const auto result = TransferBounded(pair, false, true);
    ReportTransfer(mode, result);
    Require(result.supplied && result.suppliedBytes == 1 && !result.released && result.value == 1 && result.byte == 'z');
    Require(result.milliseconds >= 50 && result.milliseconds < 1500);
}

static void CheckSendNonblocking() {
    constexpr auto mode = "send-nonblocking";
    SocketPair pair{false};
    pair.SetNativeNonblocking(true, mode);
    const int sendBuffer = 4096;
    Require(::setsockopt(pair.socket.native, SOL_SOCKET, SO_SNDBUF, &sendBuffer, sizeof(sendBuffer)) == 0);
    std::array<char, 4096> bytes{};
    std::size_t total = 0;
    bool full = false;
    for (int attempt = 0; attempt < 4096; ++attempt) {
        const auto sent = ::send(pair.socket.native, bytes.data(), bytes.size(), MSG_DONTWAIT | MSG_NOSIGNAL);
        if (sent > 0) {
            total += static_cast<std::size_t>(sent);
            continue;
        }
        Require(sent == -1);
        if (errno == EINTR) continue;
        Require(WouldBlock(errno));
        full = true;
        break;
    }
    Require(full && total != 0);
    full = false;
    const char byte = '?';
    for (int attempt = 0; attempt < 8192; ++attempt) {
        const auto sent = ::send(pair.socket.native, &byte, 1, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (sent == 1) {
            ++total;
            continue;
        }
        Require(sent == -1);
        if (errno == EINTR) continue;
        Require(WouldBlock(errno));
        std::fprintf(stderr, "%s: native one-byte write=%lld errno=%d after filling %llu bytes\n",
            mode, static_cast<long long>(sent), errno, static_cast<unsigned long long>(total));
        full = true;
        break;
    }
    Require(full);
    pair.CloseGuestEndpoint();
    const auto result = TransferBounded(pair, true);
    ReportTransfer(mode, result);
    Require(!result.released && result.value == -1 && WouldBlock(result.error) && result.byte == '?');
    Require(result.milliseconds < 1000);
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::string_view mode = argv[1];
    if (mode == "receive-nonblocking") CheckReceiveNonblocking();
    else if (mode == "receive-blocking") CheckReceiveBlocking();
    else if (mode == "send-nonblocking") CheckSendNonblocking();
    else return 2;
    return 0;
}
