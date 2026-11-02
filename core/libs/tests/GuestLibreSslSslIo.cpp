#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/SslInternal.hpp"
#include "prx/libSceLibreSsl/include/TlsEngine.hpp"
#include "prx/libSceLibreSsl/tests/FakeSocket.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Socket/include/SocketError.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <mutex>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace SslLayout;
using EngineResult = LibreSslTls::Result;
using Tls = LibreSslTls::Connection;

namespace {

constexpr char SelfCertificate[] = R"(-----BEGIN CERTIFICATE-----
MIIBqDCCAU2gAwIBAgIUcIAL8zsBVkq2q4eVQA9fqDeEVPswCgYIKoZIzj0EAwIw
GjEYMBYGA1UEAwwPYW55cHM1LXRscy50ZXN0MCAXDTI2MTAyOTAzNDIwOVoYDzIx
MjYxMDA1MDM0MjA5WjAaMRgwFgYDVQQDDA9hbnlwczUtdGxzLnRlc3QwWTATBgcq
hkjOPQIBBggqhkjOPQMBBwNCAATwzuAAIWkQT3sLDBne7AmvgRxARguPSAAf1h0V
3ZLc0KDO5WssIVOUSQBIu/Uy/OoDlRpO/isEDkpDDnV8MZHTo28wbTAdBgNVHQ4E
FgQUshT1Jeq3beFegTZ9fNlcvrgdG98wHwYDVR0jBBgwFoAUshT1Jeq3beFegTZ9
fNlcvrgdG98wDwYDVR0TAQH/BAUwAwEB/zAaBgNVHREEEzARgg9hbnlwczUtdGxz
LnRlc3QwCgYIKoZIzj0EAwIDSQAwRgIhAMaxIrTkFMsXU/F0v+LNvSLir6M/Qo3D
cK4Cp/9qnAh9AiEAwfJPCDOVIckpQH/8uYEGHr/SVNPgfbJLu8Bk+7ni6ws=
-----END CERTIFICATE-----
)";

constexpr char SelfKey[] = R"(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg6nsmluo/hg8BqwJE
f0kE9DQCpI3swVzdoKOUIJzLh+ehRANCAATwzuAAIWkQT3sLDBne7AmvgRxARguP
SAAf1h0V3ZLc0KDO5WssIVOUSQBIu/Uy/OoDlRpO/isEDkpDDnV8MZHT
-----END PRIVATE KEY-----
)";

constexpr char AuthorityCertificate[] = R"(-----BEGIN CERTIFICATE-----
MIIBSzCB8qADAgECAgkA+yeZEntfclswCgYIKoZIzj0EAwIwGDEWMBQGA1UEAwwN
YW55cHM1LXRscy1jYTAgFw0yNjEwMjkwNTA4MjJaGA8yMTI2MTAwNTA1MDgyMlow
GDEWMBQGA1UEAwwNYW55cHM1LXRscy1jYTBZMBMGByqGSM49AgEGCCqGSM49AwEH
A0IABH5lltRFsaSEckzx4oQMRtvtvL2jNncTf9KIJTG7a8n99e/IMjKKISh6rBQa
SxgklulgQMZxQotq9MP20zMT7ZujIzAhMA8GA1UdEwEB/wQFMAMBAf8wDgYDVR0P
AQH/BAQDAgEGMAoGCCqGSM49BAMCA0gAMEUCIQDSkm3/JdjK+t7lzIvuzi8lMt8p
R84k3vYpNRD37Ms1CgIgWbON2vkNINUIjObfDnUTW8LSZnT8ClyFQQfAxJYsSmc=
-----END CERTIFICATE-----
)";

constexpr char LeafCertificate[] = R"(-----BEGIN CERTIFICATE-----
MIIBdjCCARygAwIBAgIJAPE6XW11VVeUMAoGCCqGSM49BAMCMBgxFjAUBgNVBAMM
DWFueXBzNS10bHMtY2EwIBcNMjYxMDI5MDUwODIyWhgPMjEyNjEwMDUwNTA4MjJa
MBoxGDAWBgNVBAMMD2FueXBzNS10bHMudGVzdDBZMBMGByqGSM49AgEGCCqGSM49
AwEHA0IABNoHzdJmWfxuAnt7TImT6DimMSxb78GN+btchs7/b+tGBYqSVNR+iXkG
I7bBFPMOktqUAQWueB90h53EYm3JcfqjSzBJMBoGA1UdEQQTMBGCD2FueXBzNS10
bHMudGVzdDAJBgNVHRMEAjAAMAsGA1UdDwQEAwIHgDATBgNVHSUEDDAKBggrBgEF
BQcDATAKBggqhkjOPQQDAgNIADBFAiA9XoJZ6v8EHYmF7r59EcboF438GgXi2HGV
M0IyT1uwZwIhAP15hsx0mdaaeJfNqukvhrqNxU3d7A1k4sy+AmBnlPE0
-----END CERTIFICATE-----
)";

constexpr char LeafKey[] = R"(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgyWPEZXwkuL1pUq+5
8OSHWs76jvrheLi1HAqxwpX7qi6hRANCAATaB83SZln8bgJ7e0yJk+g4pjEsW+/B
jfm7XIbO/2/rRgWKklTUfol5BiO2wRTzDpLalAEFrngfdIedxGJtyXH6
-----END PRIVATE KEY-----
)";

constexpr char Host[] = "anyps5-tls.test";
constexpr int GuestSocketLevel = 0xffff;
constexpr int GuestNonBlocking = 0x1200;
constexpr int GuestLingerOption = 0x80;
constexpr int GuestSendBuffer = 0x1001;
constexpr int GuestReceiveBuffer = 0x1002;
constexpr int WaitRounds = 5000;
constexpr std::size_t RecordLimit = 16384;
constexpr int CtrlMode = 33;
constexpr int CtrlSetMaxSendFragment = 52;
constexpr int CtrlClearMode = 78;
constexpr int CtrlSetTlsExtHostName = 55;
constexpr unsigned int ErrorLibraryMask = 0xff;

std::mutex heapMutex;
std::set<void*> live;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL SSL I/O check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) {
        std::lock_guard<std::mutex> lock(heapMutex);
        live.insert(pointer);
    }
    return pointer;
}

void APS5_VABI Release(void* pointer) {
    if (!pointer) return;
    {
        std::lock_guard<std::mutex> lock(heapMutex);
        Require(live.erase(pointer) == 1);
    }
    std::free(pointer);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t size) {
    void* pointer = std::calloc(count, size);
    if (pointer) {
        std::lock_guard<std::mutex> lock(heapMutex);
        live.insert(pointer);
    }
    return pointer;
}

void* APS5_VABI Reallocate(void* pointer, std::size_t size) {
    if (pointer) {
        std::lock_guard<std::mutex> lock(heapMutex);
        Require(live.erase(pointer) == 1);
    }
    void* moved = std::realloc(pointer, size);
    if (moved) {
        std::lock_guard<std::mutex> lock(heapMutex);
        live.insert(moved);
    }
    return moved;
}

void* APS5_VABI Align(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void InitializeHeap() {
    void* const api[10]{reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release), reinterpret_cast<void*>(AllocateZeroed), reinterpret_cast<void*>(Reallocate),
        reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign), reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
}

template <typename TTest>
void Run(TTest test) {
    const std::set<void*> before = live;
    const std::size_t connections = LibreSslConnectionCount_nid_no_patch();
    ERR_clear_error_nid_postfix();
    test();
    Require(live == before);
    Require(LibreSslConnectionCount_nid_no_patch() == connections);
}

template <typename TAction>
void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return;
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

unsigned int Library(std::uint64_t code) { return static_cast<unsigned int>(code >> 24) & ErrorLibraryMask; }
unsigned int Function(std::uint64_t code) { return static_cast<unsigned int>(code >> 12) & 0xfff; }
unsigned int Reason(std::uint64_t code) { return static_cast<unsigned int>(code) & 0xfff; }

void RequireQueuedError(unsigned int function, unsigned int reason, std::source_location location = std::source_location::current()) {
    const std::uint64_t code = ERR_get_error_nid_postfix();
    Require(code != 0 && Library(code) == 20 && Function(code) == function && Reason(code) == reason, location);
    Require(ERR_get_error_nid_postfix() == 0, location);
}

int WouldBlockCode() {
#ifdef _WIN32
    return GuestSockets::GuestErrorFromHost(WSAEWOULDBLOCK);
#else
    return GuestSockets::GuestErrorFromHost(EAGAIN);
#endif
}

int ResetCode() {
#ifdef _WIN32
    return GuestSockets::GuestErrorFromHost(WSAECONNRESET);
#else
    return GuestSockets::GuestErrorFromHost(ECONNRESET);
#endif
}

void Settle() { std::this_thread::sleep_for(std::chrono::milliseconds(2)); }

template <typename TCall>
EngineResult Retry(TCall call) {
    for (int round = 0; round < WaitRounds; ++round) {
        const EngineResult result = call();
        if (result != EngineResult::WantRead && result != EngineResult::WantWrite) return result;
        Settle();
    }
    Require(false);
    return EngineResult::Ssl;
}

EngineResult Handshake(Tls& tls) {
    return Retry([&] { return tls.Handshake(); });
}

std::vector<std::uint8_t> Pattern(std::size_t size, std::uint32_t seed) {
    std::vector<std::uint8_t> data(size);
    std::uint32_t state = seed;
    for (auto& byte : data) {
        state = state * 1664525u + 1013904223u;
        byte = static_cast<std::uint8_t>(state >> 24);
    }
    return data;
}

void WriteAll(Tls& tls, const void* data, std::size_t size) {
    std::size_t transferred = 0;
    Require(Retry([&] { return tls.Write(data, size, transferred); }) == EngineResult::Done);
    Require(transferred == size);
}

void ReadExact(Tls& tls, std::uint8_t* out, std::size_t size) {
    std::size_t total = 0;
    while (total < size) {
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(out + total, size - total, transferred, false); }) == EngineResult::Done);
        Require(transferred > 0 && transferred <= RecordLimit && transferred <= size - total);
        total += transferred;
    }
}

std::array<std::uint8_t, 16> Loopback(std::uint16_t port) {
    return {16, 2, static_cast<std::uint8_t>(port >> 8), static_cast<std::uint8_t>(port), 127, 0, 0, 1};
}

struct Listener {
    int fd = -1;
    std::uint16_t port = 0;

    Listener() {
        fd = socket_nid_postfix(2, 1, 6);
        Require(fd >= 0);
        const auto any = Loopback(0);
        Require(bind_nid_postfix(fd, any.data(), static_cast<std::uint32_t>(any.size())) == 0);
        Require(listen_nid_postfix(fd, 4) == 0);
        std::array<std::uint8_t, 16> bound{};
        std::uint32_t length = static_cast<std::uint32_t>(bound.size());
        Require(getsockname_nid_postfix(fd, bound.data(), &length) == 0 && length == bound.size());
        port = static_cast<std::uint16_t>((bound[2] << 8) | bound[3]);
        Require(port != 0);
    }

    ~Listener() { close_nid_postfix(fd); }
};

int ConnectTo(std::uint16_t port) {
    const int fd = socket_nid_postfix(2, 1, 6);
    Require(fd >= 0);
    const auto address = Loopback(port);
    Require(connect_nid_postfix(fd, address.data(), static_cast<std::uint32_t>(address.size())) == 0);
    return fd;
}

void MakeNonBlocking(int fd) {
    const int enabled = 1;
    Require(setsockopt_nid_postfix(fd, GuestSocketLevel, GuestNonBlocking, &enabled, sizeof(enabled)) == 0);
}

void ResetOnClose(int fd) {
    struct Linger {
        std::int32_t enabled;
        std::int32_t seconds;
    } linger{1, 0};
    Require(setsockopt_nid_postfix(fd, GuestSocketLevel, GuestLingerOption, &linger, sizeof(linger)) == 0);
}

using Clock = std::chrono::steady_clock;
using FakeSocketSupport::FakeDescriptor;
using FakeSocketSupport::GuestReceiveTimeout;
using FakeSocketSupport::GuestSendTimeout;
using FakeSocketSupport::InstallFake;
using FakeSocketSupport::RemoveFake;
using FakeSocketSupport::SetSocketTimeout;

constexpr int GuestInterruptedCode = 4;
constexpr int GuestBadDescriptorCode = 9;

long ElapsedMilliseconds(Clock::time_point since) {
    return static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - since).count());
}

struct Gate {
    std::atomic<bool> open{false};

    void Open() { open = true; }

    void Wait() const {
        for (int round = 0; round < WaitRounds && !open; ++round) Settle();
        Require(open);
    }
};

struct Scenario {
    const char* certificate = SelfCertificate;
    const char* key = SelfKey;
    bool handshake = true;
    std::function<void(Tls&, int)> server = [](Tls&, int) {};
    int serverReceiveBuffer = 0;
};

void Serve(const Scenario& scenario, const std::function<void(int)>& client) {
    Listener listener;
    if (scenario.serverReceiveBuffer)
        Require(setsockopt_nid_postfix(listener.fd, GuestSocketLevel, GuestReceiveBuffer, &scenario.serverReceiveBuffer, sizeof(int)) == 0);
    std::thread serving([&] {
        try {
            const int fd = accept_nid_postfix(listener.fd, nullptr, nullptr);
            Require(fd >= 0);
            {
                Tls tls(true);
                tls.SetSocket(fd);
                tls.UseCertificate(reinterpret_cast<const unsigned char*>(scenario.certificate), std::strlen(scenario.certificate),
                    reinterpret_cast<const unsigned char*>(scenario.key), std::strlen(scenario.key));
                if (scenario.handshake) {
                    Require(Handshake(tls) == EngineResult::Done);
                    Require(tls.PeerServerName() == Host);
                }
                scenario.server(tls, fd);
            }
            close_nid_postfix(fd);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "LibreSSL SSL I/O server thread: %s\n", error.what());
            std::abort();
        }
    });
    const int fd = ConnectTo(listener.port);
    client(fd);
    close_nid_postfix(fd);
    serving.join();
}

struct Client {
    LibreSslCtx* ctx;
    LibreSsl* ssl;

    explicit Client(int fd, const LibreSslMethod* method = TLS_client_method_nid_postfix()) {
        ctx = SSL_CTX_new_nid_postfix(method);
        Require(ctx != nullptr);
        ssl = SSL_new_nid_postfix(ctx);
        Require(ssl != nullptr);
        Require(SSL_set_fd_nid_postfix(ssl, fd) == 1);
    }

    ~Client() {
        SSL_free_nid_postfix(ssl);
        SSL_CTX_free_nid_postfix(ctx);
    }

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    void NameHost() {
        char name[sizeof(Host)];
        std::memcpy(name, Host, sizeof(Host));
        Require(SSL_ctrl_nid_postfix(ssl, CtrlSetTlsExtHostName, 0, name) == 1);
        std::memset(name, 0, sizeof(name));
    }

    void Connect() {
        NameHost();
        Require(SSL_connect_nid_postfix(ssl) == 1);
    }

    LibreSslBio& Bio() { return *static_cast<LibreSslBio*>(ssl->rbio); }
};

void WriteSsl(LibreSsl* ssl, const std::vector<std::uint8_t>& data) {
    Require(SSL_write_nid_postfix(ssl, data.data(), static_cast<int>(data.size())) == static_cast<int>(data.size()));
}

void ReadSsl(LibreSsl* ssl, std::uint8_t* out, std::size_t size) {
    std::size_t total = 0;
    while (total < size) {
        const int count = SSL_read_nid_postfix(ssl, out + total, static_cast<int>(size - total));
        Require(count > 0 && static_cast<std::size_t>(count) <= RecordLimit && static_cast<std::size_t>(count) <= size - total);
        total += static_cast<std::size_t>(count);
    }
}

void TestHandshakeRecordsVerifyResult() {
    Scenario scenario;
    scenario.server = [](Tls&, int) {};
    Serve(scenario, [](int fd) {
        Client client(fd);
        Require(SSL_get_verify_result_nid_postfix(client.ssl) == 0);
        Require(client.ssl->internal->state == (StateBefore | StateConnect) && client.ssl->internal->handshakeFunc == nullptr);
        client.Connect();
        Require(SSL_get_error_nid_postfix(client.ssl, 1) == SslErrorNone);
        Require(SSL_get_verify_result_nid_postfix(client.ssl) == VerifyErrorDepthZeroSelfSigned);
        Require(client.ssl->internal->state == StateOk && client.ssl->internal->rwstate == RwNothing);
        Require(client.ssl->internal->handshakeFunc != nullptr && client.ssl->server == 0);
        Require(SSL_get_shutdown_nid_postfix(client.ssl) == 0 && SSL_want_nid_postfix(client.ssl) == RwNothing);
        Require(ERR_peek_error_nid_postfix() == 0);
        Require(client.Bio().numRead > 0 && client.Bio().numWrite > 0);
        Require(client.Bio().flags == 0);
        Require(client.ssl->tlsextHostname != nullptr && std::strcmp(client.ssl->tlsextHostname, Host) == 0);
    });
}

void TestChainShapes() {
    Scenario withAuthority;
    const std::string chain = std::string(LeafCertificate) + AuthorityCertificate;
    withAuthority.certificate = chain.c_str();
    withAuthority.key = LeafKey;
    Serve(withAuthority, [](int fd) {
        Client client(fd);
        client.Connect();
        Require(SSL_get_verify_result_nid_postfix(client.ssl) == VerifyErrorSelfSignedInChain);
    });
    Scenario leafOnly;
    leafOnly.certificate = LeafCertificate;
    leafOnly.key = LeafKey;
    Serve(leafOnly, [](int fd) {
        Client client(fd);
        client.Connect();
        Require(SSL_get_verify_result_nid_postfix(client.ssl) == VerifyErrorUnableToGetIssuerLocally);
    });
}

void TestEchoBothWays() {
    const auto payload = Pattern(65536, 7);
    const auto reply = Pattern(65536, 11);
    Scenario scenario;
    scenario.server = [&](Tls& tls, int) {
        std::vector<std::uint8_t> received(payload.size());
        ReadExact(tls, received.data(), received.size());
        Require(received == payload);
        WriteAll(tls, reply.data(), reply.size());
    };
    Serve(scenario, [&](int fd) {
        Client client(fd);
        client.Connect();
        const std::uint64_t written = client.Bio().numWrite;
        WriteSsl(client.ssl, payload);
        Require(client.Bio().numWrite >= written + payload.size());
        std::vector<std::uint8_t> received(reply.size());
        ReadSsl(client.ssl, received.data(), received.size());
        Require(received == reply);
        Require(SSL_pending_nid_postfix(client.ssl) == 0);
        Require(SSL_get_error_nid_postfix(client.ssl, 1) == SslErrorNone);
        std::uint8_t byte = 0;
        Require(SSL_read_nid_postfix(client.ssl, &byte, -1) == -1);
        RequireQueuedError(76, ErrorInternalError);
        Require(SSL_write_nid_postfix(client.ssl, &byte, -1) == -1);
        RequireQueuedError(76, ErrorInternalError);
    });
}

void TestPeekPendingAndPartialRead() {
    const auto data = Pattern(100, 3);
    Scenario scenario;
    scenario.server = [&](Tls& tls, int) {
        WriteAll(tls, data.data(), data.size());
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == EngineResult::Done);
    };
    Serve(scenario, [&](int fd) {
        Client client(fd);
        client.Connect();
        std::vector<std::uint8_t> peeked(16);
        Require(SSL_peek_nid_postfix(client.ssl, peeked.data(), 16) == 16);
        Require(std::equal(peeked.begin(), peeked.end(), data.begin()));
        Require(SSL_pending_nid_postfix(client.ssl) == 100);
        std::vector<std::uint8_t> wide(200);
        Require(SSL_peek_nid_postfix(client.ssl, wide.data(), 200) == 100);
        Require(std::equal(data.begin(), data.end(), wide.begin()));
        std::vector<std::uint8_t> head(16);
        Require(SSL_read_nid_postfix(client.ssl, head.data(), 16) == 16);
        Require(head == peeked);
        Require(SSL_pending_nid_postfix(client.ssl) == 84);
        Require(SSL_read_nid_postfix(client.ssl, head.data(), 0) == 0);
        Require(SSL_get_error_nid_postfix(client.ssl, 0) == SslErrorSyscall);
        Require(SSL_pending_nid_postfix(client.ssl) == 84);
        std::vector<std::uint8_t> tail(84);
        Require(SSL_read_nid_postfix(client.ssl, tail.data(), 84) == 84);
        Require(std::equal(tail.begin(), tail.end(), data.begin() + 16));
        Require(SSL_pending_nid_postfix(client.ssl) == 0);
        const std::uint8_t byte = 1;
        Require(SSL_write_nid_postfix(client.ssl, &byte, 0) == 0);
        Require(SSL_get_error_nid_postfix(client.ssl, 0) == SslErrorSyscall);
        Require(SSL_write_nid_postfix(client.ssl, &byte, 1) == 1);
    });
}

void TestNonBlockingHandshakeAndRead() {
    const auto data = Pattern(32, 5);
    Gate gate;
    Scenario scenario;
    scenario.server = [&](Tls& tls, int) {
        gate.Wait();
        WriteAll(tls, data.data(), data.size());
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == EngineResult::Done);
    };
    Serve(scenario, [&](int fd) {
        MakeNonBlocking(fd);
        Client client(fd);
        client.NameHost();
        int waits = 0;
        for (int round = 0;; ++round) {
            Require(round < WaitRounds);
            const int result = SSL_connect_nid_postfix(client.ssl);
            if (result == 1) break;
            Require(result == -1);
            const int error = SSL_get_error_nid_postfix(client.ssl, result);
            Require(error == SslErrorWantRead || error == SslErrorWantWrite);
            Require(SSL_want_nid_postfix(client.ssl) == (error == SslErrorWantRead ? RwReading : RwWriting));
            Require((client.Bio().flags & BioFlagShouldRetry) != 0);
            Require(client.ssl->internal->state != StateOk && (client.ssl->internal->state & StateInit) != 0);
            ++waits;
            Settle();
        }
        Require(waits > 0);
        Require(SSL_get_verify_result_nid_postfix(client.ssl) == VerifyErrorDepthZeroSelfSigned);
        std::uint8_t buffer[64];
        const int result = SSL_read_nid_postfix(client.ssl, buffer, sizeof(buffer));
        Require(result == -1);
        Require(SSL_get_error_nid_postfix(client.ssl, result) == SslErrorWantRead);
        Require(SSL_want_nid_postfix(client.ssl) == RwReading && (client.Bio().flags & (BioFlagRead | BioFlagShouldRetry)) == (BioFlagRead | BioFlagShouldRetry));
        Require(*__error_nid_postfix() == WouldBlockCode());
        Require(ERR_peek_error_nid_postfix() == 0);
        gate.Open();
        int count = -1;
        for (int round = 0; round < WaitRounds && count < 0; ++round) {
            count = SSL_read_nid_postfix(client.ssl, buffer, sizeof(buffer));
            if (count < 0) {
                Require(SSL_get_error_nid_postfix(client.ssl, count) == SslErrorWantRead);
                Settle();
            }
        }
        Require(count == static_cast<int>(data.size()) && std::equal(data.begin(), data.end(), buffer));
        Require(SSL_want_nid_postfix(client.ssl) == RwNothing && client.Bio().flags == 0);
        const std::uint8_t byte = 1;
        Require(SSL_write_nid_postfix(client.ssl, &byte, 1) == 1);
    });
}

void FillSendBuffer(Client& client, const std::vector<std::uint8_t>& chunk, std::size_t& total) {
    for (int round = 0; round < 4096; ++round) {
        const int result = SSL_write_nid_postfix(client.ssl, chunk.data(), static_cast<int>(chunk.size()));
        if (result > 0) {
            Require(result == static_cast<int>(chunk.size()));
            total += chunk.size();
            continue;
        }
        Require(result == -1);
        Require(SSL_get_error_nid_postfix(client.ssl, result) == SslErrorWantWrite);
        Require(SSL_want_nid_postfix(client.ssl) == RwWriting && (client.Bio().flags & (BioFlagWrite | BioFlagShouldRetry)) == (BioFlagWrite | BioFlagShouldRetry));
        return;
    }
    Require(false);
}

void DrainUntilClose(Tls& tls, std::atomic<std::uint64_t>& received) {
    std::vector<std::uint8_t> buffer(RecordLimit);
    for (;;) {
        std::size_t transferred = 0;
        const EngineResult result = Retry([&] { return tls.Read(buffer.data(), buffer.size(), transferred, false); });
        if (result == EngineResult::ZeroReturn) break;
        Require(result == EngineResult::Done);
        received += transferred;
    }
}

void FinishWrites(Client& client) {
    int result = -1;
    for (int round = 0; round < WaitRounds && result != 1; ++round) {
        result = SSL_shutdown_nid_postfix(client.ssl);
        Require(result >= -1 && result <= 1);
        if (result != 1) Settle();
    }
    Require(result == 1);
}

void TestWantWriteRetryRules() {
    const auto chunk = Pattern(65536, 9);
    for (const bool moving : {false, true}) {
        Gate gate;
        std::atomic<std::uint64_t> received{0};
        std::size_t total = 0;
        Scenario scenario;
        scenario.server = [&](Tls& tls, int) {
            gate.Wait();
            DrainUntilClose(tls, received);
            bool complete = false;
            Require(Retry([&] { return tls.Shutdown(complete); }) == EngineResult::Done);
        };
        Serve(scenario, [&](int fd) {
            MakeNonBlocking(fd);
            Client client(fd);
            client.NameHost();
            for (int round = 0; SSL_connect_nid_postfix(client.ssl) != 1; ++round) {
                Require(round < WaitRounds);
                Settle();
            }
            if (moving) Require(SSL_ctrl_nid_postfix(client.ssl, CtrlMode, 2, nullptr) == 2);
            FillSendBuffer(client, chunk, total);
            auto moved = chunk;
            const int elsewhere = SSL_write_nid_postfix(client.ssl, moved.data(), static_cast<int>(moved.size()));
            if (moving) {
                Require(elsewhere == -1 && SSL_get_error_nid_postfix(client.ssl, elsewhere) == SslErrorWantWrite);
            } else {
                Require(elsewhere == -1 && SSL_get_error_nid_postfix(client.ssl, elsewhere) == SslErrorSsl);
                RequireQueuedError(76, ReasonBadWriteRetry);
            }
            const int shorter = SSL_write_nid_postfix(client.ssl, chunk.data(), 10);
            Require(shorter == -1 && SSL_get_error_nid_postfix(client.ssl, shorter) == SslErrorSsl);
            RequireQueuedError(76, ReasonBadWriteRetry);
            gate.Open();
            const std::vector<std::uint8_t>& retry = moving ? moved : chunk;
            int result = -1;
            for (int round = 0; round < WaitRounds && result < 0; ++round) {
                result = SSL_write_nid_postfix(client.ssl, retry.data(), static_cast<int>(retry.size()));
                if (result < 0) {
                    Require(SSL_get_error_nid_postfix(client.ssl, result) == SslErrorWantWrite);
                    Settle();
                }
            }
            Require(result == static_cast<int>(chunk.size()));
            total += chunk.size();
            FinishWrites(client);
            Require(SSL_get_shutdown_nid_postfix(client.ssl) == (ShutdownSent | ShutdownReceived));
            const std::uint64_t sent = client.Bio().numWrite;
            Require(sent > total);
            gate.Open();
        });
        Require(received == total);
    }
}

void TestPartialWriteMode() {
    const auto data = Pattern(40000, 13);
    std::atomic<std::uint64_t> received{0};
    Scenario scenario;
    scenario.server = [&](Tls& tls, int) {
        DrainUntilClose(tls, received);
        bool complete = false;
        Require(Retry([&] { return tls.Shutdown(complete); }) == EngineResult::Done);
    };
    Serve(scenario, [&](int fd) {
        Client client(fd);
        client.Connect();
        Require(SSL_ctrl_nid_postfix(client.ssl, CtrlMode, 1, nullptr) == 1);
        Require(SSL_write_nid_postfix(client.ssl, data.data(), static_cast<int>(data.size())) == 16384);
        Require(SSL_write_nid_postfix(client.ssl, data.data() + 16384, 100) == 100);
        Require(SSL_ctrl_nid_postfix(client.ssl, CtrlSetMaxSendFragment, 4096, nullptr) == 1);
        Require(SSL_write_nid_postfix(client.ssl, data.data(), static_cast<int>(data.size())) == 4096);
        Require(SSL_ctrl_nid_postfix(client.ssl, CtrlClearMode, 1, nullptr) == 0);
        Require(SSL_write_nid_postfix(client.ssl, data.data(), static_cast<int>(data.size())) == static_cast<int>(data.size()));
        Require(SSL_shutdown_nid_postfix(client.ssl) == 0);
        Require(SSL_shutdown_nid_postfix(client.ssl) == 1);
    });
    Require(received == 16384 + 100 + 4096 + 40000);
}

void TestPeerCloseNotify() {
    const auto data = Pattern(3, 21);
    Scenario scenario;
    scenario.server = [&](Tls& tls, int) {
        WriteAll(tls, data.data(), data.size());
        bool complete = false;
        Require(Retry([&] { return tls.Shutdown(complete); }) == EngineResult::Done);
        Require(!complete);
    };
    Serve(scenario, [&](int fd) {
        Client client(fd);
        client.Connect();
        std::uint8_t buffer[16];
        Require(SSL_read_nid_postfix(client.ssl, buffer, sizeof(buffer)) == 3 && std::equal(data.begin(), data.end(), buffer));
        Require(SSL_get_shutdown_nid_postfix(client.ssl) == 0);
        Require(SSL_read_nid_postfix(client.ssl, buffer, sizeof(buffer)) == 0);
        Require(SSL_get_error_nid_postfix(client.ssl, 0) == SslErrorZeroReturn);
        Require(SSL_get_shutdown_nid_postfix(client.ssl) == ShutdownReceived);
        Require(SSL_read_nid_postfix(client.ssl, buffer, sizeof(buffer)) == 0);
        Require(SSL_get_error_nid_postfix(client.ssl, 0) == SslErrorZeroReturn);
        Require(SSL_peek_nid_postfix(client.ssl, buffer, sizeof(buffer)) == 0);
        Require(SSL_want_nid_postfix(client.ssl) == RwNothing);
    });
}

void TestShutdownSequence() {
    Scenario scenario;
    scenario.server = [&](Tls& tls, int) {
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == EngineResult::ZeroReturn);
        bool complete = false;
        Require(Retry([&] { return tls.Shutdown(complete); }) == EngineResult::Done);
        Require(complete);
    };
    Serve(scenario, [&](int fd) {
        Client client(fd);
        client.Connect();
        Require(SSL_shutdown_nid_postfix(client.ssl) == 0);
        Require(SSL_get_shutdown_nid_postfix(client.ssl) == ShutdownSent);
        const std::uint8_t byte = 1;
        Require(SSL_write_nid_postfix(client.ssl, &byte, 1) == -1);
        Require(SSL_get_error_nid_postfix(client.ssl, -1) == SslErrorSsl);
        RequireQueuedError(76, ReasonProtocolIsShutdown);
        Require(SSL_shutdown_nid_postfix(client.ssl) == 1);
        Require(SSL_get_shutdown_nid_postfix(client.ssl) == (ShutdownSent | ShutdownReceived));
        std::uint8_t buffer[4];
        Require(SSL_read_nid_postfix(client.ssl, buffer, sizeof(buffer)) == 0);
        Require(SSL_get_error_nid_postfix(client.ssl, 0) == SslErrorZeroReturn);
    });
}

void TestQuietShutdown() {
    Scenario scenario;
    scenario.server = [](Tls& tls, int) {
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == EngineResult::Syscall);
        Require(!tls.ReceivedShutdown());
    };
    Serve(scenario, [](int fd) {
        Client client(fd);
        client.Connect();
        Write<int>(client.ssl->internal, InternalQuietShutdown, 1);
        Require(SSL_shutdown_nid_postfix(client.ssl) == 1);
        Require(SSL_get_shutdown_nid_postfix(client.ssl) == (ShutdownSent | ShutdownReceived));
        std::uint8_t byte = 0;
        Require(SSL_write_nid_postfix(client.ssl, &byte, 1) == -1);
        RequireQueuedError(76, ReasonProtocolIsShutdown);
    });
}

void TestAbruptClose() {
    Scenario plain;
    Serve(plain, [](int fd) {
        Client client(fd);
        client.Connect();
        std::uint8_t buffer[8];
        Require(SSL_read_nid_postfix(client.ssl, buffer, sizeof(buffer)) == 0);
        Require(SSL_get_error_nid_postfix(client.ssl, 0) == SslErrorSyscall);
        Require(*__error_nid_postfix() == 0);
        Require(SSL_get_shutdown_nid_postfix(client.ssl) == 0);
        Require(SSL_want_nid_postfix(client.ssl) == RwReading && client.Bio().flags == 0);
    });
    Gate connected;
    Scenario reset;
    reset.server = [&](Tls&, int fd) {
        connected.Wait();
        ResetOnClose(fd);
    };
    Serve(reset, [&](int fd) {
        Client client(fd);
        client.Connect();
        connected.Open();
        std::uint8_t buffer[8];
        const int result = SSL_read_nid_postfix(client.ssl, buffer, sizeof(buffer));
        Require(result == -1);
        Require(SSL_get_error_nid_postfix(client.ssl, result) == SslErrorSyscall);
        Require(*__error_nid_postfix() == ResetCode());
        Require(SSL_get_shutdown_nid_postfix(client.ssl) == 0);
    });
}

void TestVerifyPeerRejectsUntrustedCertificate() {
    Scenario scenario;
    scenario.handshake = false;
    scenario.server = [](Tls& tls, int) { Require(Handshake(tls) != EngineResult::Done); };
    Serve(scenario, [](int fd) {
        Client client(fd);
        client.NameHost();
        SSL_set_verify_nid_postfix(client.ssl, 1, nullptr);
        Require(SSL_connect_nid_postfix(client.ssl) == -1);
        Require(SSL_get_error_nid_postfix(client.ssl, -1) == SslErrorSsl);
        RequireQueuedError(7, ReasonCertificateVerifyFailed);
        Require(SSL_get_verify_result_nid_postfix(client.ssl) == VerifyErrorDepthZeroSelfSigned);
        Require(client.ssl->internal->state == StateConnectCertB && SSL_want_nid_postfix(client.ssl) == RwNothing);
        Require(SSL_shutdown_nid_postfix(client.ssl) == 1);
    });
}

void TestUninitializedAndInvalidCalls() {
    LibreSslCtx* ctx = SSL_CTX_new_nid_postfix(TLS_client_method_nid_postfix());
    LibreSsl* ssl = SSL_new_nid_postfix(ctx);
    std::uint8_t buffer[8] = {};
    Require(SSL_read_nid_postfix(ssl, buffer, sizeof(buffer)) == -1);
    Require(SSL_get_error_nid_postfix(ssl, -1) == SslErrorSsl);
    RequireQueuedError(78, ReasonUninitialized);
    Require(SSL_peek_nid_postfix(ssl, buffer, sizeof(buffer)) == -1);
    RequireQueuedError(78, ReasonUninitialized);
    Require(SSL_write_nid_postfix(ssl, buffer, sizeof(buffer)) == -1);
    RequireQueuedError(78, ReasonUninitialized);
    Require(SSL_shutdown_nid_postfix(ssl) == -1);
    RequireQueuedError(78, ReasonUninitialized);
    Require(SSL_pending_nid_postfix(ssl) == 0);
    Require(SSL_get_error_nid_postfix(ssl, 1) == SslErrorNone);
    Require(SSL_get_error_nid_postfix(ssl, -1) == SslErrorSyscall);
    Require(SSL_get_error_nid_postfix(ssl, 0) == SslErrorSyscall);

    ssl->internal->rwstate = RwX509Lookup;
    Require(SSL_get_error_nid_postfix(ssl, -1) == SslErrorWantX509Lookup);
    ssl->internal->rwstate = RwNothing;
    LibreSslBio bio{};
    void* const savedRead = ssl->rbio;
    void* const savedWrite = ssl->wbio;
    ssl->rbio = &bio;
    ssl->wbio = &bio;
    ssl->internal->rwstate = RwReading;
    bio.flags = BioFlagWrite;
    Require(SSL_get_error_nid_postfix(ssl, -1) == SslErrorWantWrite);
    bio.flags = BioFlagIoSpecial;
    bio.retryReason = BioRetryConnect;
    Require(SSL_get_error_nid_postfix(ssl, -1) == SslErrorWantConnect);
    bio.retryReason = BioRetryAccept;
    Require(SSL_get_error_nid_postfix(ssl, -1) == SslErrorWantAccept);
    bio.retryReason = 1;
    Require(SSL_get_error_nid_postfix(ssl, -1) == SslErrorSyscall);
    bio.flags = 0;
    ssl->internal->rwstate = RwWriting;
    Require(SSL_get_error_nid_postfix(ssl, -1) == SslErrorSyscall);
    bio.flags = BioFlagRead;
    Require(SSL_get_error_nid_postfix(ssl, -1) == SslErrorWantRead);
    bio.flags = BioFlagIoSpecial;
    bio.retryReason = BioRetryAccept;
    Require(SSL_get_error_nid_postfix(ssl, -1) == SslErrorWantAccept);
    bio.flags = BioFlagRead;
    Require(SSL_get_error_nid_postfix(ssl, 0) == SslErrorSyscall);
    Require(SSL_get_error_nid_postfix(ssl, 3) == SslErrorNone);
    ssl->internal->rwstate = RwNothing;
    ssl->rbio = savedRead;
    ssl->wbio = savedWrite;

    LibreSslPutError_nid_no_patch(ErrorLibrarySys, 1, 2);
    Require(SSL_get_error_nid_postfix(ssl, -1) == SslErrorSyscall);
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(ErrorLibrarySsl, 1, 2);
    Require(SSL_get_error_nid_postfix(ssl, 0) == SslErrorSsl);
    Require(SSL_get_error_nid_postfix(ssl, 1) == SslErrorNone);
    ERR_clear_error_nid_postfix();

    RequireThrows([&] { SSL_connect_nid_postfix(nullptr); });
    RequireThrows([&] { SSL_accept_nid_postfix(nullptr); });
    RequireThrows([&] { SSL_read_nid_postfix(nullptr, buffer, 1); });
    RequireThrows([&] { SSL_peek_nid_postfix(nullptr, buffer, 1); });
    RequireThrows([&] { SSL_write_nid_postfix(nullptr, buffer, 1); });
    RequireThrows([&] { SSL_shutdown_nid_postfix(nullptr); });
    RequireThrows([&] { SSL_pending_nid_postfix(nullptr); });
    RequireThrows([&] { SSL_get_error_nid_postfix(nullptr, 0); });
    Require(SSL_get_error_nid_postfix(nullptr, 1) == SslErrorNone);
    RequireThrows([&] { SSL_ctrl_nid_postfix(nullptr, CtrlSetTlsExtHostName, 0, nullptr); });

    LibreSslCtx* serverContext = SSL_CTX_new_nid_postfix(SSLv23_server_method_nid_postfix());
    LibreSsl* serverSsl = SSL_new_nid_postfix(serverContext);
    Require(SSL_connect_nid_postfix(serverSsl) == 0);
    RequireQueuedError(78, ErrorShouldNotHaveBeenCalled);
    Require(serverSsl->server == 0 && serverSsl->internal->handshakeFunc != nullptr);
    Require(SSL_accept_nid_postfix(ssl) == 0);
    RequireQueuedError(80, ErrorShouldNotHaveBeenCalled);
    Require(ssl->server == 1 && ssl->internal->state == (StateAccept | StateBefore));
    SSL_free_nid_postfix(serverSsl);
    SSL_CTX_free_nid_postfix(serverContext);

    SSL_free_nid_postfix(ssl);
    SSL_CTX_free_nid_postfix(ctx);
}

void TestHostnameControl() {
    LibreSslCtx* ctx = SSL_CTX_new_nid_postfix(TLS_client_method_nid_postfix());
    LibreSsl* ssl = SSL_new_nid_postfix(ctx);
    Require(ssl->tlsextHostname == nullptr);
    char name[] = "first.example";
    Require(SSL_ctrl_nid_postfix(ssl, CtrlSetTlsExtHostName, 0, name) == 1);
    Require(ssl->tlsextHostname != nullptr && ssl->tlsextHostname != name && std::strcmp(ssl->tlsextHostname, "first.example") == 0);
    name[0] = 'x';
    Require(std::strcmp(ssl->tlsextHostname, "first.example") == 0);
    char second[] = "second.example";
    Require(SSL_ctrl_nid_postfix(ssl, CtrlSetTlsExtHostName, 0, second) == 1);
    Require(std::strcmp(ssl->tlsextHostname, "second.example") == 0);
    std::string longest(255, 'a');
    Require(SSL_ctrl_nid_postfix(ssl, CtrlSetTlsExtHostName, 0, longest.data()) == 1);
    Require(std::strlen(ssl->tlsextHostname) == 255);
    std::string tooLong(256, 'a');
    Require(SSL_ctrl_nid_postfix(ssl, CtrlSetTlsExtHostName, 0, tooLong.data()) == 0);
    Require(ssl->tlsextHostname == nullptr);
    RequireQueuedError(78, ReasonServerNameInvalid);
    Require(SSL_ctrl_nid_postfix(ssl, CtrlSetTlsExtHostName, 0, second) == 1);
    Require(SSL_ctrl_nid_postfix(ssl, CtrlSetTlsExtHostName, 0, nullptr) == 1);
    Require(ssl->tlsextHostname == nullptr);
    Require(SSL_ctrl_nid_postfix(ssl, CtrlSetTlsExtHostName, 0, second) == 1);
    Require(SSL_ctrl_nid_postfix(ssl, CtrlSetTlsExtHostName, 1, second) == 0);
    Require(ssl->tlsextHostname != nullptr && std::strcmp(ssl->tlsextHostname, "second.example") == 0);
    RequireQueuedError(78, ReasonServerNameTypeInvalid);
    SSL_free_nid_postfix(ssl);
    SSL_CTX_free_nid_postfix(ctx);
}

void RunAccepting(const std::function<void(Tls&, Gate&)>& engineClient, const std::function<void(int, Gate&)>& accepting) {
    Listener listener;
    Gate gate;
    std::thread connecting([&] {
        try {
            const int fd = ConnectTo(listener.port);
            {
                Tls tls(false);
                tls.SetSocket(fd);
                engineClient(tls, gate);
            }
            close_nid_postfix(fd);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "LibreSSL SSL I/O client thread: %s\n", error.what());
            std::abort();
        }
    });
    const int fd = accept_nid_postfix(listener.fd, nullptr, nullptr);
    Require(fd >= 0);
    accepting(fd, gate);
    close_nid_postfix(fd);
    connecting.join();
}

void TestAcceptWithoutCertificateFailsLikeLibreSsl() {
    RunAccepting(
        [](Tls& tls, Gate& gate) {
            gate.Wait();
            Require(Handshake(tls) != EngineResult::Done);
            Require(!tls.HandshakeDone());
        },
        [](int fd, Gate& gate) {
            MakeNonBlocking(fd);
            LibreSslCtx* ctx = SSL_CTX_new_nid_postfix(SSLv23_server_method_nid_postfix());
            LibreSsl* ssl = SSL_new_nid_postfix(ctx);
            Require(SSL_set_fd_nid_postfix(ssl, fd) == 1);
            Require(SSL_accept_nid_postfix(ssl) == -1);
            Require(SSL_get_error_nid_postfix(ssl, -1) == SslErrorWantRead);
            Require(ssl->server == 1 && ssl->internal->state == StateAcceptHelloA && SSL_want_nid_postfix(ssl) == RwReading);
            gate.Open();
            int result = -1;
            for (int round = 0; round < WaitRounds; ++round) {
                result = SSL_accept_nid_postfix(ssl);
                if (result != -1 || SSL_get_error_nid_postfix(ssl, result) != SslErrorWantRead) break;
                Settle();
            }
            Require(result == -1 && SSL_get_error_nid_postfix(ssl, result) == SslErrorSsl);
            RequireQueuedError(39, ReasonNoSharedCipher);
            Require(ssl->internal->state == StateAcceptHelloC && SSL_want_nid_postfix(ssl) == RwNothing);
            SSL_free_nid_postfix(ssl);
            SSL_CTX_free_nid_postfix(ctx);
        });
}


void TestBlockingSocketWaitsForData() {
    Gate release;
    Scenario scenario;
    scenario.server = [&](Tls& tls, int) {
        release.Wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        WriteAll(tls, "late", 4);
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == EngineResult::ZeroReturn);
    };
    Serve(scenario, [&](int fd) {
        Client client(fd);
        client.Connect();
        release.Open();
        std::uint8_t buffer[8];
        const auto start = Clock::now();
        Require(SSL_read_nid_postfix(client.ssl, buffer, sizeof(buffer)) == 4 && std::memcmp(buffer, "late", 4) == 0);
        Require(ElapsedMilliseconds(start) >= 150);
        Require(SSL_want_nid_postfix(client.ssl) == RwNothing && client.Bio().flags == 0);
        Require(SSL_shutdown_nid_postfix(client.ssl) == 0);
    });
}

void TestNonBlockingSocketIgnoresTimeouts() {
    Gate release;
    Scenario scenario;
    scenario.server = [&](Tls& tls, int) {
        release.Wait();
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == EngineResult::Done);
    };
    Serve(scenario, [&](int fd) {
        MakeNonBlocking(fd);
        SetSocketTimeout(fd, GuestReceiveTimeout, 3000);
        SetSocketTimeout(fd, GuestSendTimeout, 3000);
        Client client(fd);
        client.NameHost();
        for (int round = 0; SSL_connect_nid_postfix(client.ssl) != 1; ++round) {
            Require(round < WaitRounds);
            Settle();
        }
        std::uint8_t buffer[8];
        const auto start = Clock::now();
        Require(SSL_read_nid_postfix(client.ssl, buffer, sizeof(buffer)) == -1);
        Require(SSL_get_error_nid_postfix(client.ssl, -1) == SslErrorWantRead && *__error_nid_postfix() == WouldBlockCode());
        Require(ElapsedMilliseconds(start) < 1500);
        release.Open();
        const std::uint8_t byte = 1;
        Require(SSL_write_nid_postfix(client.ssl, &byte, 1) == 1);
    });
}

void TestReceiveTimeout() {
    Gate release;
    Scenario scenario;
    scenario.server = [&](Tls& tls, int) {
        release.Wait();
        WriteAll(tls, "after", 5);
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == EngineResult::ZeroReturn);
    };
    Serve(scenario, [&](int fd) {
        Client client(fd);
        client.Connect();
        SetSocketTimeout(fd, GuestReceiveTimeout, 250);
        std::uint8_t buffer[8];
        const auto start = Clock::now();
        Require(SSL_read_nid_postfix(client.ssl, buffer, sizeof(buffer)) == -1);
        const long waited = ElapsedMilliseconds(start);
        Require(waited >= 200 && waited < 5000);
        Require(SSL_get_error_nid_postfix(client.ssl, -1) == SslErrorWantRead);
        Require(*__error_nid_postfix() == WouldBlockCode());
        Require(SSL_want_nid_postfix(client.ssl) == RwReading && (client.Bio().flags & (BioFlagRead | BioFlagShouldRetry)) == (BioFlagRead | BioFlagShouldRetry));
        Require(ERR_peek_error_nid_postfix() == 0);
        release.Open();
        int count = -1;
        for (int round = 0; round < WaitRounds && count < 0; ++round) {
            count = SSL_read_nid_postfix(client.ssl, buffer, sizeof(buffer));
            if (count < 0) Require(SSL_get_error_nid_postfix(client.ssl, count) == SslErrorWantRead);
        }
        Require(count == 5 && std::memcmp(buffer, "after", 5) == 0);
        Require(SSL_shutdown_nid_postfix(client.ssl) == 0);
    });
}

void TestSendTimeout() {
    Gate release;
    Scenario scenario;
    scenario.server = [&](Tls&, int) { release.Wait(); };
    Serve(scenario, [&](int fd) {
        Client client(fd);
        client.Connect();
        if (FakeSocketSupport::SendTimeoutIsUnreachable(fd)) {
            release.Open();
            return;
        }
        SetSocketTimeout(fd, GuestSendTimeout, 250);
        const auto chunk = Pattern(100, 9);
        int result = 100;
        long waited = 0;
        for (int round = 0; round < 400000 && result == 100; ++round) {
            const auto start = Clock::now();
            result = SSL_write_nid_postfix(client.ssl, chunk.data(), static_cast<int>(chunk.size()));
            waited = ElapsedMilliseconds(start);
        }
        Require(result == -1);
        Require(waited >= 200 && waited < 5000);
        Require(SSL_get_error_nid_postfix(client.ssl, -1) == SslErrorWantWrite);
        Require(*__error_nid_postfix() == WouldBlockCode());
        Require(SSL_want_nid_postfix(client.ssl) == RwWriting && (client.Bio().flags & (BioFlagWrite | BioFlagShouldRetry)) == (BioFlagWrite | BioFlagShouldRetry));
        release.Open();
    });
}

void TestLargeRecordSendTimeout() {
    Gate release;
    Scenario scenario;
    scenario.server = [&](Tls&, int) { release.Wait(); };
    scenario.serverReceiveBuffer = 4096;
    Serve(scenario, [&](int fd) {
        Client client(fd);
        client.Connect();
        if (FakeSocketSupport::SendTimeoutIsUnreachable(fd)) {
            release.Open();
            return;
        }
        const int small = 4096;
        Require(setsockopt_nid_postfix(fd, GuestSocketLevel, GuestSendBuffer, &small, sizeof(small)) == 0);
        SetSocketTimeout(fd, GuestSendTimeout, 250);
        const auto record = Pattern(RecordLimit, 11);
        const int size = static_cast<int>(record.size());
        int result = size;
        for (int round = 0; round < 4096 && result == size; ++round) {
            const auto start = Clock::now();
            result = SSL_write_nid_postfix(client.ssl, record.data(), size);
            Require(ElapsedMilliseconds(start) < 5000);
        }
        Require(result == -1);
        Require(SSL_get_error_nid_postfix(client.ssl, -1) == SslErrorWantWrite);
        Require(*__error_nid_postfix() == WouldBlockCode());
        release.Open();
    });
}

std::vector<std::uint8_t> ReceiveRecord(int fd) {
    std::vector<std::uint8_t> record;
    std::size_t expected = 5;
    std::array<std::uint8_t, 64> piece{};
    for (int round = 0; round < WaitRounds && record.size() < expected; ++round) {
        const std::int64_t received = recv_nid_postfix(fd, piece.data(), std::min(piece.size(), expected - record.size()), 0);
        if (received > 0)
            record.insert(record.end(), piece.data(), piece.data() + received);
        else
            Settle();
        if (record.size() >= 5) expected = 5 + (static_cast<std::size_t>(record[3]) << 8 | record[4]);
    }
    Require(record.size() == expected);
    return record;
}

void TestInterruptedTransfer() {
    Gate release;
    Scenario scenario;
    scenario.server = [&](Tls& tls, int) {
        release.Wait();
        WriteAll(tls, "wake", 4);
        std::array<std::uint8_t, 4> ping{};
        ReadExact(tls, ping.data(), ping.size());
        Require(std::memcmp(ping.data(), "ping", 4) == 0);
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == EngineResult::ZeroReturn);
    };
    Serve(scenario, [&](int fd) {
        const auto fake = InstallFake(fd);
        Client client(fd);
        Require(SSL_set_fd_nid_postfix(client.ssl, FakeDescriptor) == 1 && SSL_get_fd_nid_postfix(client.ssl) == FakeDescriptor);
        client.NameHost();
        fake->failNext = FakeSocketSupport::HostInterruptedCode;
        Require(SSL_connect_nid_postfix(client.ssl) == -1);
        Require(SSL_get_error_nid_postfix(client.ssl, -1) == SslErrorWantWrite && *__error_nid_postfix() == GuestInterruptedCode);
        Require(SSL_want_nid_postfix(client.ssl) == RwWriting && (client.Bio().flags & BioFlagShouldRetry) != 0);
        Require(SSL_connect_nid_postfix(client.ssl) == 1);
        Require(fake->calls > 0 && fake->sendCalls > 0);

        release.Open();
        std::uint8_t buffer[8];
        for (int round = 0; round < WaitRounds; ++round) {
            fake->failNext = FakeSocketSupport::HostInterruptedCode;
            const int result = SSL_read_nid_postfix(client.ssl, buffer, sizeof(buffer));
            Require(result == -1);
            Require(SSL_get_error_nid_postfix(client.ssl, -1) == SslErrorWantRead && *__error_nid_postfix() == GuestInterruptedCode);
            Require(SSL_want_nid_postfix(client.ssl) == RwReading && (client.Bio().flags & BioFlagShouldRetry) != 0);
            Require(ERR_peek_error_nid_postfix() == 0);
            fake->failNext = 0;
            const int count = SSL_read_nid_postfix(client.ssl, buffer, sizeof(buffer));
            if (count == 4) break;
            Require(count == -1 && SSL_get_error_nid_postfix(client.ssl, count) == SslErrorWantRead);
            Settle();
        }
        Require(std::memcmp(buffer, "wake", 4) == 0);

        fake->failNext = FakeSocketSupport::HostInterruptedCode;
        Require(SSL_write_nid_postfix(client.ssl, "ping", 4) == -1);
        Require(SSL_get_error_nid_postfix(client.ssl, -1) == SslErrorWantWrite && *__error_nid_postfix() == GuestInterruptedCode);
        Require(SSL_want_nid_postfix(client.ssl) == RwWriting && (client.Bio().flags & BioFlagShouldRetry) != 0);
        Require(SSL_write_nid_postfix(client.ssl, "ping", 4) == 4);
        Require(SSL_shutdown_nid_postfix(client.ssl) == 0);
        Require(SSL_set_fd_nid_postfix(client.ssl, fd) == 1);
        RemoveFake();
    });
}

void TestClosedDescriptor() {
    Gate closed;
    Scenario scenario;
    scenario.server = [&](Tls& tls, int) {
        closed.Wait();
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == EngineResult::Syscall && !tls.ReceivedShutdown());
    };
    Serve(scenario, [&](int fd) {
        Client client(fd);
        client.Connect();
        Require(close_nid_postfix(fd) == 0);
        closed.Open();
        std::uint8_t buffer[8];
        Require(SSL_read_nid_postfix(client.ssl, buffer, sizeof(buffer)) == -1);
        Require(SSL_get_error_nid_postfix(client.ssl, -1) == SslErrorSyscall && *__error_nid_postfix() == GuestBadDescriptorCode);
        Require(SSL_want_nid_postfix(client.ssl) == RwReading && client.Bio().flags == 0);
        Require(SSL_write_nid_postfix(client.ssl, "x", 1) == -1);
        Require(SSL_get_error_nid_postfix(client.ssl, -1) == SslErrorSyscall && *__error_nid_postfix() == GuestBadDescriptorCode);
        Require(SSL_want_nid_postfix(client.ssl) == RwWriting);
    });
}

void TestSetFdFollowsTheNewSocket() {
    Gate release;
    Gate checked;
    Scenario scenario;
    scenario.server = [&](Tls& tls, int fd) {
        release.Wait();
        MakeNonBlocking(fd);
        std::uint8_t byte = 0;
        std::size_t transferred = 5;
        Require(tls.Read(&byte, 1, transferred, false) == EngineResult::WantRead && transferred == 0);
        checked.Open();
    };
    Serve(scenario, [&](int first) {
        Listener other;
        const int second = ConnectTo(other.port);
        const int peer = accept_nid_postfix(other.fd, nullptr, nullptr);
        Require(peer >= 0);
        MakeNonBlocking(peer);
        Client client(first);
        client.Connect();
        Require(SSL_set_fd_nid_postfix(client.ssl, second) == 1);
        Require(SSL_get_fd_nid_postfix(client.ssl) == second);
        Require(SSL_write_nid_postfix(client.ssl, "hello", 5) == 5);
        const auto wire = ReceiveRecord(peer);
        Require(wire.size() > 5 && wire[0] == 0x17 && wire[1] == 0x03 && wire[2] == 0x03);
        Require(client.Bio().numWrite == wire.size());
        release.Open();
        checked.Wait();

        Require(SSL_set_fd_nid_postfix(client.ssl, -1) == 1 && SSL_get_fd_nid_postfix(client.ssl) == -1);
        std::uint8_t buffer[8];
        Require(SSL_read_nid_postfix(client.ssl, buffer, sizeof(buffer)) == -1);
        Require(SSL_get_error_nid_postfix(client.ssl, -1) == SslErrorSyscall && *__error_nid_postfix() == GuestBadDescriptorCode);
        Require(SSL_set_fd_nid_postfix(client.ssl, first) == 1);
        close_nid_postfix(second);
        close_nid_postfix(peer);
    });
}
}

int main(int argc, char** argv) {
    const std::string group = argc > 1 ? argv[1] : "all";
    const bool all = group == "all";
    const bool handshake = all || group == "handshake";
    const bool data = all || group == "data";
    const bool shutdown = all || group == "shutdown";
    const bool transport = all || group == "transport";
    if (!handshake && !data && !shutdown && !transport) {
        std::fprintf(stderr, "unknown test group %s\n", group.c_str());
        return 2;
    }
    InitializeHeap();
    if (handshake) {
        Run(TestHandshakeRecordsVerifyResult);
        Run(TestChainShapes);
        Run(TestVerifyPeerRejectsUntrustedCertificate);
        Run(TestUninitializedAndInvalidCalls);
        Run(TestHostnameControl);
        Run(TestAcceptWithoutCertificateFailsLikeLibreSsl);
    }
    if (data) {
        Run(TestEchoBothWays);
        Run(TestPeekPendingAndPartialRead);
        Run(TestNonBlockingHandshakeAndRead);
        Run(TestWantWriteRetryRules);
        Run(TestPartialWriteMode);
    }
    if (shutdown) {
        Run(TestPeerCloseNotify);
        Run(TestShutdownSequence);
        Run(TestAbruptClose);
        Run(TestQuietShutdown);
    }
    if (transport) {
        Run(TestBlockingSocketWaitsForData);
        Run(TestNonBlockingSocketIgnoresTimeouts);
        Run(TestReceiveTimeout);
        Run(TestSendTimeout);
        Run(TestLargeRecordSendTimeout);
        Run(TestInterruptedTransfer);
        Run(TestClosedDescriptor);
        Run(TestSetFdFollowsTheNewSocket);
    }
    std::puts("LibreSSL SSL I/O tests passed");
    return 0;
}
