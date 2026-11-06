#include "prx/libSceLibreSsl/include/TlsEngine.hpp"
#include "prx/libSceLibreSsl/tests/FakeSocket.hpp"
#include "prx/libkernel/include/Api.hpp"
#ifndef _WIN32
#include <cerrno>
#include <csignal>
#include <pthread.h>
#endif

#include <mbedtls/x509.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using LibreSslTls::Connection;
using LibreSslTls::Result;

constexpr char ServerCertificate[] = R"(-----BEGIN CERTIFICATE-----
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

constexpr char ServerKey[] = R"(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg6nsmluo/hg8BqwJE
f0kE9DQCpI3swVzdoKOUIJzLh+ehRANCAATwzuAAIWkQT3sLDBne7AmvgRxARguP
SAAf1h0V3ZLc0KDO5WssIVOUSQBIu/Uy/OoDlRpO/isEDkpDDnV8MZHT
-----END PRIVATE KEY-----
)";

constexpr char OtherKey[] = R"(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEINOl7FIj4W75MeHV89I4EQdxzYV5mPN2ZfC/KTxLzDEsoAoGCCqGSM49
AwEHoUQDQgAE2rFujoZ7mhh48TZDyVCUT1U07QPF3lF8hDtVX8mGvU4DfE9sIIIw
nj2xgGUxohBmLUpf6FU0RcQ0Ifdz1PabkQ==
-----END EC PRIVATE KEY-----
)";

constexpr char TestHost[] = "anyps5-tls.test";
constexpr char WrongHost[] = "other.anyps5-tls.test";
constexpr int GuestSocketLevel = 0xffff;
constexpr int GuestNonBlocking = 0x1200;
constexpr int GuestLingerOption = 0x80;
constexpr int GuestSendBuffer = 0x1001;
constexpr int GuestReceiveBuffer = 0x1002;
constexpr int WaitRounds = 5000;
constexpr std::size_t RecordLimit = 16384;

struct LingerValue {
    std::int32_t enabled;
    std::int32_t seconds;
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

template <typename TException, typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, message);
}

void Settle() { std::this_thread::sleep_for(std::chrono::milliseconds(2)); }

template <typename TCall>
Result Retry(TCall call) {
    for (int round = 0; round < WaitRounds; ++round) {
        const Result result = call();
        if (result != Result::WantRead && result != Result::WantWrite) return result;
        Settle();
    }
    Require(false, "an operation kept asking to wait");
    return Result::Ssl;
}

Result Handshake(Connection& tls) {
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

void WriteAll(Connection& tls, const void* data, std::size_t size) {
    std::size_t transferred = 0;
    Require(Retry([&] { return tls.Write(data, size, transferred); }) == Result::Done, "write did not complete");
    Require(transferred == size, "a completed write did not transfer every byte");
}

void ReadExact(Connection& tls, std::uint8_t* out, std::size_t size) {
    std::size_t total = 0;
    while (total < size) {
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(out + total, size - total, transferred, false); }) == Result::Done, "read did not complete");
        Require(transferred > 0 && transferred <= RecordLimit && transferred <= size - total, "a read returned an impossible count");
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
        Require(fd >= 0, "create the listening socket");
        const auto any = Loopback(0);
        Require(bind_nid_postfix(fd, any.data(), static_cast<std::uint32_t>(any.size())) == 0, "bind the listening socket");
        Require(listen_nid_postfix(fd, 4) == 0, "listen");
        std::array<std::uint8_t, 16> bound{};
        std::uint32_t length = static_cast<std::uint32_t>(bound.size());
        Require(getsockname_nid_postfix(fd, bound.data(), &length) == 0 && length == bound.size(), "read the listening address");
        port = static_cast<std::uint16_t>((bound[2] << 8) | bound[3]);
        Require(port != 0, "the listening port was not assigned");
    }

    ~Listener() { close_nid_postfix(fd); }
};

int ConnectTo(std::uint16_t port) {
    const int fd = socket_nid_postfix(2, 1, 6);
    Require(fd >= 0, "create the client socket");
    const auto address = Loopback(port);
    Require(connect_nid_postfix(fd, address.data(), static_cast<std::uint32_t>(address.size())) == 0, "connect the client socket");
    return fd;
}

void MakeNonBlocking(int fd) {
    const int enabled = 1;
    Require(setsockopt_nid_postfix(fd, GuestSocketLevel, GuestNonBlocking, &enabled, sizeof(enabled)) == 0, "make the socket non-blocking");
}

using Clock = std::chrono::steady_clock;
using FakeSocketSupport::FakeDescriptor;
using FakeSocketSupport::GuestReceiveTimeout;
using FakeSocketSupport::GuestSendTimeout;
using FakeSocketSupport::HostBadDescriptorCode;
using FakeSocketSupport::HostInterruptedCode;
using FakeSocketSupport::HostWouldBlockCode;
using FakeSocketSupport::InstallFake;
using FakeSocketSupport::RemoveFake;

void SetTimeout(int fd, int option, int milliseconds) { FakeSocketSupport::SetSocketTimeout(fd, option, milliseconds); }

long ElapsedMilliseconds(Clock::time_point since) {
    return static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - since).count());
}

struct Gate {
    std::atomic<bool> open{false};

    void Open() { open = true; }

    void Wait(const char* message) const {
        for (int round = 0; round < WaitRounds && !open; ++round) Settle();
        Require(open, message);
    }
};

struct Scenario {
    bool serverIdentity = true;
    bool serverHandshake = true;
    bool serverTrustsCertificate = false;
    int serverVerify = LibreSslTls::SslVerifyNone;
    std::function<void(Connection&, int)> client;
    std::function<void(Connection&, int)> server = [](Connection&, int) {};
    int serverReceiveBuffer = 0;
};

void Run(const Scenario& scenario) {
    Listener listener;
    if (scenario.serverReceiveBuffer)
        Require(setsockopt_nid_postfix(listener.fd, GuestSocketLevel, GuestReceiveBuffer, &scenario.serverReceiveBuffer, sizeof(int)) == 0,
            "shrink the receive buffer the accepted socket inherits");
    std::thread serving([&] {
        try {
            const int fd = accept_nid_postfix(listener.fd, nullptr, nullptr);
            Require(fd >= 0, "accept the client");
            {
                Connection tls(true);
                tls.SetSocket(fd);
                if (scenario.serverIdentity)
                    tls.UseCertificate(reinterpret_cast<const unsigned char*>(ServerCertificate), sizeof(ServerCertificate) - 1,
                        reinterpret_cast<const unsigned char*>(ServerKey), sizeof(ServerKey) - 1);
                if (scenario.serverTrustsCertificate)
                    tls.TrustCertificates(reinterpret_cast<const unsigned char*>(ServerCertificate), sizeof(ServerCertificate) - 1);
                tls.SetVerifyMode(scenario.serverVerify);
                if (scenario.serverHandshake) Require(Handshake(tls) == Result::Done, "the server handshake did not complete");
                scenario.server(tls, fd);
            }
            close_nid_postfix(fd);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "failed: server thread: %s\n", error.what());
            std::abort();
        }
    });
    const int fd = ConnectTo(listener.port);
    {
        Connection tls(false);
        tls.SetSocket(fd);
        scenario.client(tls, fd);
    }
    close_nid_postfix(fd);
    serving.join();
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
    Require(record.size() == expected, "the whole record arrived");
    return record;
}

void TrustServer(Connection& tls) {
    tls.TrustCertificates(reinterpret_cast<const unsigned char*>(ServerCertificate), sizeof(ServerCertificate) - 1);
}

void ResultCodes() {
    using namespace LibreSslTls;
    Require(ToSslError(Result::Done) == 0, "Done maps to SSL_ERROR_NONE");
    Require(ToSslError(Result::Ssl) == 1, "Ssl maps to SSL_ERROR_SSL");
    Require(ToSslError(Result::WantRead) == 2, "WantRead maps to SSL_ERROR_WANT_READ");
    Require(ToSslError(Result::WantWrite) == 3, "WantWrite maps to SSL_ERROR_WANT_WRITE");
    Require(ToSslError(Result::Syscall) == 5, "Syscall maps to SSL_ERROR_SYSCALL");
    Require(ToSslError(Result::ZeroReturn) == 6, "ZeroReturn maps to SSL_ERROR_ZERO_RETURN");
}

void NotASocket() {
    Connection tls(false);
    Require(tls.Socket() == -1, "a new connection has no socket");
    RequireThrows<std::invalid_argument>([&] { tls.SetSocket(-1); }, "a negative descriptor was accepted");
    RequireThrows<std::invalid_argument>([&] { tls.SetSocket(0); }, "standard input was accepted as a guest socket");
    RequireThrows<std::invalid_argument>([&] { tls.SetSocket(1 << 20); }, "an unopened descriptor was accepted");
    Require(tls.Socket() == -1, "a rejected descriptor was stored");
    std::size_t transferred = 7;
    char byte = 0;
    Require(tls.Handshake() == Result::Ssl, "a handshake without a socket did not fail");
    Require(tls.Read(&byte, 1, transferred, false) == Result::Ssl && transferred == 0, "a read without a socket did not fail");
    Require(tls.Write(&byte, 1, transferred) == Result::Ssl && transferred == 0, "a write without a socket did not fail");
    Require(tls.FailureKind() == LibreSslTls::Failure::Other && tls.FailureCode() == 0, "a write without a socket is an unclassified failure");
    Require(tls.PeerChain() == LibreSslTls::ChainShape::None && tls.PeerServerName().empty(), "nothing was seen from a peer");
    Require(tls.BytesReceived() == 0 && tls.BytesSent() == 0 && tls.LastDirection() == LibreSslTls::Direction::None, "nothing was transferred");
    RequireThrows<std::logic_error>([&] { tls.VerifyFlags(); }, "the verify result was readable before a handshake");
    RequireThrows<std::logic_error>([&] { tls.Version(); }, "the version was readable before a handshake");
    RequireThrows<std::logic_error>([&] { tls.Cipher(); }, "the cipher was readable before a handshake");
    RequireThrows<std::invalid_argument>([&] { tls.SetVerifyMode(0x40); }, "an unknown verify bit was accepted");
    RequireThrows<std::invalid_argument>([&] { tls.Read(nullptr, 4, transferred, false); }, "a null read buffer was accepted");
    RequireThrows<std::invalid_argument>([&] { tls.Write(nullptr, 4, transferred); }, "a null write buffer was accepted");
    RequireThrows<std::invalid_argument>([&] { tls.TrustCertificates(nullptr, 0); }, "an empty trust list was accepted");

    const Listener listener;
    tls.SetSocket(listener.fd);
    Require(tls.Socket() == listener.fd, "a guest socket was not stored");
}

void BadIdentity() {
    Connection tls(true);
    const auto* certificate = reinterpret_cast<const unsigned char*>(ServerCertificate);
    const auto* key = reinterpret_cast<const unsigned char*>(ServerKey);
    const auto* other = reinterpret_cast<const unsigned char*>(OtherKey);
    RequireThrows<std::runtime_error>([&] { tls.UseCertificate(certificate, sizeof(ServerCertificate) - 1, other, sizeof(OtherKey) - 1); },
        "a key that does not match the certificate was accepted");
    RequireThrows<std::runtime_error>([&] { tls.UseCertificate(certificate, sizeof(ServerCertificate) - 1, certificate, sizeof(ServerCertificate) - 1); },
        "a certificate was accepted as a private key");
    RequireThrows<std::runtime_error>([&] { tls.UseCertificate(key, sizeof(ServerKey) - 1, key, sizeof(ServerKey) - 1); },
        "a private key was accepted as a certificate");
    RequireThrows<std::runtime_error>([&] { tls.TrustCertificates(key, sizeof(ServerKey) - 1); }, "a private key was accepted as a trust anchor");
    tls.UseCertificate(certificate, sizeof(ServerCertificate) - 1, key, sizeof(ServerKey) - 1);
}

void UntrustedHandshake() {
    Scenario scenario;
    scenario.client = [](Connection& tls, int) {
        tls.SetHostname(TestHost);
        Require(Handshake(tls) == Result::Done, "a handshake with an untrusted certificate must complete under SSL_VERIFY_NONE");
        Require(tls.HandshakeDone(), "the handshake was not recorded");
        Require(tls.VerifyFlags() == MBEDTLS_X509_BADCERT_NOT_TRUSTED, "an untrusted certificate must record exactly not-trusted");
        Require(tls.PeerChain() == LibreSslTls::ChainShape::SelfSignedLeaf, "a single self-signed certificate is recorded as such");
        Require(tls.FailureKind() == LibreSslTls::Failure::None && tls.BytesReceived() > 0 && tls.BytesSent() > 0, "a completed handshake moved bytes both ways");
        Require(std::strcmp(tls.Version(), "TLSv1.2") == 0, "the negotiated version");
        Require(std::strstr(tls.Cipher(), "ECDHE-ECDSA") != nullptr, "the negotiated cipher");
        Require(tls.Handshake() == Result::Done, "a second handshake call after completion must report done");
        Require(!tls.SentShutdown() && !tls.ReceivedShutdown(), "no shutdown happened yet");
        RequireThrows<std::logic_error>([&] { tls.SetHostname("late.example"); }, "the host name changed after the handshake");
        RequireThrows<std::logic_error>([&] { tls.SetVerifyMode(0); }, "the verify mode changed after the handshake");
        RequireThrows<std::invalid_argument>([&] { tls.SetSocket(2); }, "a descriptor that is not a guest socket was accepted after the handshake");
        RequireThrows<std::logic_error>([&] { TrustServer(tls); }, "trust anchors changed after the handshake");
    };
    Run(scenario);
}

void TrustedHandshake() {
    Scenario scenario;
    scenario.client = [](Connection& tls, int) {
        TrustServer(tls);
        tls.SetHostname(TestHost);
        Require(Handshake(tls) == Result::Done, "a trusted handshake must complete");
        Require(tls.VerifyFlags() == 0, "a trusted certificate with the matching host name must record no flags");
    };
    Run(scenario);
}

void WrongHostname() {
    Scenario scenario;
    scenario.client = [](Connection& tls, int) {
        TrustServer(tls);
        tls.SetHostname(WrongHost);
        Require(Handshake(tls) == Result::Done, "a host name mismatch must not fail under SSL_VERIFY_NONE");
        Require(tls.VerifyFlags() == MBEDTLS_X509_BADCERT_CN_MISMATCH, "a wrong host name must record exactly the name mismatch");
    };
    Run(scenario);
}

void NoHostname() {
    Scenario scenario;
    scenario.client = [](Connection& tls, int) {
        TrustServer(tls);
        tls.SetHostname(WrongHost);
        tls.SetHostname(nullptr);
        Require(Handshake(tls) == Result::Done, "a handshake without a host name must complete");
        Require(tls.VerifyFlags() == 0, "clearing the host name must skip the name check");
    };
    Run(scenario);
}

void VerifyPeerClient() {
    Gate serverSawAlert;
    Scenario rejected;
    rejected.serverHandshake = false;
    rejected.server = [&](Connection& tls, int) {
        Require(Handshake(tls) == Result::Ssl, "the server must see the client's verify alert as an SSL error");
        Require(tls.ReceivedShutdown(), "a fatal alert marks the connection as shut down");
        serverSawAlert.Open();
    };
    rejected.client = [&](Connection& tls, int) {
        tls.SetVerifyMode(LibreSslTls::SslVerifyPeer);
        Require(Handshake(tls) == Result::Ssl, "SSL_VERIFY_PEER with an untrusted certificate must fail the handshake");
        Require(tls.FailureKind() == LibreSslTls::Failure::VerifyFailed && !tls.FailureText().empty(), "an untrusted certificate is a verify failure");
        Require(tls.VerifyFlags() == MBEDTLS_X509_BADCERT_NOT_TRUSTED, "the verify flags stay readable after a failed handshake");
        Require(tls.PeerChain() == LibreSslTls::ChainShape::SelfSignedLeaf, "the failed chain is a single self-signed certificate");
        Require(!tls.HandshakeDone(), "a failed handshake was recorded as done");
        serverSawAlert.Wait("the server never saw the client verify alert");
    };
    Run(rejected);

    Scenario accepted;
    accepted.client = [](Connection& tls, int) {
        tls.SetVerifyMode(LibreSslTls::SslVerifyPeer);
        TrustServer(tls);
        Require(Handshake(tls) == Result::Done, "SSL_VERIFY_PEER with a trusted certificate must complete");
        Require(tls.VerifyFlags() == 0, "no flags for a trusted certificate");
    };
    Run(accepted);
}

void ClientCertificate() {
    Scenario required;
    required.serverVerify = LibreSslTls::SslVerifyPeer | LibreSslTls::SslVerifyFailIfNoPeerCert;
    required.serverTrustsCertificate = true;
    required.serverHandshake = false;
    required.server = [](Connection& tls, int) {
        Require(Handshake(tls) == Result::Ssl, "a missing client certificate must fail under FAIL_IF_NO_PEER_CERT");
    };
    required.client = [](Connection& tls, int) {
        Require(Handshake(tls) == Result::Syscall, "mbedtls sends no alert for a missing client certificate, so the client sees the stream end");
    };
    Run(required);

    Scenario offered;
    offered.serverVerify = LibreSslTls::SslVerifyPeer | LibreSslTls::SslVerifyFailIfNoPeerCert;
    offered.serverTrustsCertificate = true;
    offered.server = [](Connection& tls, int) {
        Require(tls.VerifyFlags() == 0, "the offered client certificate must verify against the trusted copy");
    };
    offered.client = [](Connection& tls, int) {
        tls.UseCertificate(reinterpret_cast<const unsigned char*>(ServerCertificate), sizeof(ServerCertificate) - 1,
            reinterpret_cast<const unsigned char*>(ServerKey), sizeof(ServerKey) - 1);
        Require(Handshake(tls) == Result::Done, "a client certificate handshake must complete");
    };
    Run(offered);

    Scenario optional;
    optional.serverVerify = LibreSslTls::SslVerifyPeer;
    optional.server = [](Connection& tls, int) {
        Require(tls.VerifyFlags() != 0, "an absent client certificate is not a verified one");
    };
    optional.client = [](Connection& tls, int) {
        Require(Handshake(tls) == Result::Done, "SSL_VERIFY_PEER alone must let a client without a certificate in");
    };
    Run(optional);
}

void ServerWithoutCertificate() {
    Scenario scenario;
    scenario.serverIdentity = false;
    scenario.serverHandshake = false;
    scenario.server = [](Connection& tls, int) {
        Require(Handshake(tls) == Result::Ssl, "a server without a certificate must fail the handshake");
        Require(tls.FailureKind() == LibreSslTls::Failure::NoSharedCipher && tls.FailureCode() != 0, "a server without a certificate shares no cipher");
    };
    scenario.client = [](Connection& tls, int) {
        Require(Handshake(tls) == Result::Ssl, "a fatal alert during the handshake must be an SSL error");
        Require(tls.ReceivedShutdown(), "a fatal alert must set the received-shutdown state");
        Require(tls.FailureKind() == LibreSslTls::Failure::Other && tls.FatalAlertReceived(), "a fatal alert is an unclassified failure");
        std::size_t transferred = 5;
        char byte = 0;
        Require(tls.Read(&byte, 1, transferred, false) == Result::Syscall && transferred == 0,
            "a read after a fatal alert must be a syscall error, not a clean close");
    };
    Run(scenario);
}

void EofDuringHandshake() {
    Scenario scenario;
    scenario.serverHandshake = false;
    scenario.client = [](Connection& tls, int) {
        Require(Handshake(tls) == Result::Syscall, "end of stream during the handshake must be a syscall error");
        Require(!tls.ReceivedShutdown(), "end of stream is not a received shutdown");
    };
    Run(scenario);
}

void RoundTrip() {
    constexpr std::size_t Size = 100 * 1024;
    const auto upload = Pattern(Size, 1);
    const auto download = Pattern(Size, 2);
    Scenario scenario;
    scenario.server = [&](Connection& tls, int) {
        std::vector<std::uint8_t> received(Size);
        ReadExact(tls, received.data(), received.size());
        Require(received == upload, "the server received different bytes than the client sent");
        WriteAll(tls, download.data(), download.size());
    };
    scenario.client = [&](Connection& tls, int) {
        Require(Handshake(tls) == Result::Done, "round trip handshake");
        WriteAll(tls, upload.data(), upload.size());
        std::vector<std::uint8_t> received(Size);
        ReadExact(tls, received.data(), received.size());
        Require(received == download, "the client received different bytes than the server sent");
        Require(tls.Pending() == 0, "nothing should be pending after reading everything");
    };
    Run(scenario);
}

void PeekAndPending() {
    const char message[] = "hello world, peek test";
    constexpr std::size_t Length = sizeof(message) - 1;
    const auto bulk = Pattern(1000, 3);
    Gate second;
    Scenario scenario;
    scenario.server = [&](Connection& tls, int) {
        WriteAll(tls, message, Length);
        second.Wait("the client never opened the second gate");
        WriteAll(tls, bulk.data(), bulk.size());
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == Result::ZeroReturn, "the server waits for the client's close");
    };
    scenario.client = [&](Connection& tls, int) {
        Require(Handshake(tls) == Result::Done, "peek handshake");
        Require(tls.Pending() == 0, "nothing is pending before any record was read");
        std::array<char, 128> out{};
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(out.data(), 5, transferred, true); }) == Result::Done && transferred == 5 && std::memcmp(out.data(), "hello", 5) == 0,
            "peek of five bytes");
        Require(tls.Pending() == Length, "peek must leave the whole record pending");
        Require(tls.Read(out.data(), out.size(), transferred, true) == Result::Done && transferred == Length && std::memcmp(out.data(), message, Length) == 0,
            "a larger peek returns the rest of the record");
        Require(tls.Pending() == Length, "a second peek must not consume");
        out.fill(0);
        Require(tls.Read(out.data(), 5, transferred, false) == Result::Done && transferred == 5 && std::memcmp(out.data(), "hello", 5) == 0,
            "read after peek returns the peeked bytes");
        Require(tls.Pending() == Length - 5, "the read must consume what it returned");
        Require(tls.Read(out.data(), 4, transferred, true) == Result::Done && transferred == 4 && std::memcmp(out.data(), " wor", 4) == 0,
            "peek continues where the read stopped");
        Require(tls.Read(out.data(), out.size(), transferred, false) == Result::Done && transferred == Length - 5
                && std::memcmp(out.data(), message + 5, Length - 5) == 0,
            "the final read returns only what is left of the record");
        Require(tls.Pending() == 0, "everything was consumed");

        second.Open();
        std::vector<std::uint8_t> chunk(bulk.size());
        Require(Retry([&] { return tls.Read(chunk.data(), 100, transferred, false); }) == Result::Done && transferred == 100
                && std::equal(chunk.begin(), chunk.begin() + 100, bulk.begin()),
            "partial read of a record");
        Require(tls.Pending() == bulk.size() - 100, "pending bytes after a partial read");
        Require(tls.Read(chunk.data() + 100, chunk.size() - 100, transferred, false) == Result::Done && transferred == bulk.size() - 100,
            "a read returns at most the rest of the record");
        Require(std::equal(chunk.begin() + 100, chunk.end(), bulk.begin() + 100), "the rest of the record is intact");
        Require(tls.Pending() == 0, "nothing pending after the record was drained");
        bool complete = false;
        Require(tls.Shutdown(complete) == Result::Done, "close the peek connection");
    };
    Run(scenario);
}

void ZeroLengthOperations() {
    Scenario scenario;
    scenario.server = [](Connection& tls, int) {
        WriteAll(tls, "x", 1);
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == Result::ZeroReturn, "wait for the client's close");
    };
    scenario.client = [](Connection& tls, int) {
        Require(Handshake(tls) == Result::Done, "zero length handshake");
        std::size_t transferred = 9;
        char byte = 0;
        Require(Retry([&] { return tls.Read(&byte, 0, transferred, false); }) == Result::Syscall && transferred == 0,
            "a zero-length read waits for a record and then returns 0, which is a syscall error");
        Require(tls.Pending() == 1, "the record fetched by a zero-length read stays pending");
        Require(tls.Read(&byte, 1, transferred, false) == Result::Done && transferred == 1 && byte == 'x', "the record is still readable");
        transferred = 9;
        Require(tls.Write(&byte, 0, transferred) == Result::Syscall && transferred == 0, "a zero-length write returns 0, which is a syscall error");
        bool complete = false;
        Require(tls.Shutdown(complete) == Result::Done, "close the zero length connection");
    };
    Run(scenario);
}

void NonBlockingRead() {
    Gate gate;
    Scenario scenario;
    scenario.server = [&](Connection& tls, int) {
        gate.Wait("the client never opened the gate");
        WriteAll(tls, "ready", 5);
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == Result::ZeroReturn, "wait for the client's close");
    };
    scenario.client = [&](Connection& tls, int fd) {
        MakeNonBlocking(fd);
        Require(Handshake(tls) == Result::Done, "non-blocking handshake");
        std::array<char, 16> out{};
        std::size_t transferred = 3;
        Require(tls.Read(out.data(), out.size(), transferred, false) == Result::WantRead && transferred == 0,
            "a non-blocking read before any data must want a read");
        Require(tls.Read(out.data(), out.size(), transferred, true) == Result::WantRead, "a non-blocking peek before any data must want a read");
        Require(LibreSslTls::ToSslError(Result::WantRead) == LibreSslTls::SslErrorWantRead, "the result maps to SSL_ERROR_WANT_READ");
        Require(tls.Pending() == 0, "nothing is pending");
        Require(tls.SystemError() != 0, "the would-block errno is visible");
        Require(tls.LastDirection() == LibreSslTls::Direction::Read, "the would-block transfer was a read");
        gate.Open();
        Require(Retry([&] { return tls.Read(out.data(), out.size(), transferred, false); }) == Result::Done && transferred == 5
                && std::memcmp(out.data(), "ready", 5) == 0,
            "the data sent after the gate opened");
        bool complete = false;
        Require(Retry([&] { return tls.Shutdown(complete); }) == Result::Done, "close the non-blocking connection");
    };
    Run(scenario);
}

void ServerNameOnly() {
    Scenario sniOnly;
    sniOnly.server = [](Connection& tls, int) { Require(tls.PeerServerName() == WrongHost, "the server must see the name the client set"); };
    sniOnly.client = [](Connection& tls, int) {
        TrustServer(tls);
        tls.SetVerifyMode(LibreSslTls::SslVerifyPeer);
        tls.SetServerName(WrongHost);
        Require(Handshake(tls) == Result::Done, "a name that does not match the certificate must not fail when it is only sent as SNI");
        Require(tls.VerifyFlags() == 0, "an SNI-only name must not leave a name mismatch flag");
    };
    Run(sniOnly);

    Scenario verified;
    verified.serverHandshake = false;
    verified.server = [](Connection& tls, int) { Require(Handshake(tls) != Result::Done, "the server must not complete a handshake the client rejected"); };
    verified.client = [](Connection& tls, int) {
        TrustServer(tls);
        tls.SetVerifyMode(LibreSslTls::SslVerifyPeer);
        tls.SetHostname(WrongHost);
        Require(Handshake(tls) == Result::Ssl && tls.FailureKind() == LibreSslTls::Failure::VerifyFailed, "a verified name mismatch must fail the handshake");
        Require(tls.VerifyFlags() == MBEDTLS_X509_BADCERT_CN_MISMATCH, "the mismatch is the only recorded flag");
    };
    Run(verified);
}

void WriteOptions() {
    constexpr std::size_t Expected = 16384 + 1000 + 5000;
    const auto payload = Pattern(40000, 6);
    Scenario scenario;
    scenario.server = [&](Connection& tls, int) {
        std::vector<std::uint8_t> sink(RecordLimit);
        std::size_t total = 0;
        for (;;) {
            std::size_t transferred = 0;
            const Result result = Retry([&] { return tls.Read(sink.data(), sink.size(), transferred, false); });
            if (result == Result::ZeroReturn) break;
            Require(result == Result::Done, "the server read failed");
            total += transferred;
        }
        Require(total == Expected, "the server received a different number of bytes than the writes reported");
    };
    scenario.client = [&](Connection& tls, int) {
        Require(Handshake(tls) == Result::Done, "write options handshake");
        std::size_t transferred = 0;
        LibreSslTls::WriteOptions partial;
        partial.partial = true;
        Require(tls.Write(payload.data(), payload.size(), transferred, partial) == Result::Done && transferred == 16384, "a partial write returns after one default fragment");
        partial.fragment = 1000;
        Require(tls.Write(payload.data(), payload.size(), transferred, partial) == Result::Done && transferred == 1000, "a partial write honours the fragment size");
        LibreSslTls::WriteOptions whole;
        whole.fragment = 1000;
        Require(tls.Write(payload.data(), 5000, transferred, whole) == Result::Done && transferred == 5000, "a complete write loops over every fragment");
        LibreSslTls::WriteOptions empty;
        empty.fragment = 0;
        RequireThrows<std::invalid_argument>([&] { tls.Write(payload.data(), 1, transferred, empty); }, "a zero fragment size was accepted");
        bool complete = false;
        Require(Retry([&] { return tls.Shutdown(complete); }) == Result::Done, "close the write options connection");
    };
    Run(scenario);
}

void WriteBackPressure() {
    constexpr std::size_t Size = 16 * 1024 * 1024;
    const auto payload = Pattern(Size, 4);
    Scenario scenario;
    scenario.server = [&](Connection& tls, int) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::vector<std::uint8_t> received(Size);
        ReadExact(tls, received.data(), received.size());
        Require(received == payload, "back-pressured write delivered different bytes");
        WriteAll(tls, "K", 1);
    };
    scenario.client = [&](Connection& tls, int fd) {
        MakeNonBlocking(fd);
        Require(Handshake(tls) == Result::Done, "back-pressure handshake");
        std::size_t transferred = 0;
        bool waited = false;
        bool misused = false;
        Result result;
        int rounds = 0;
        while ((result = tls.Write(payload.data(), payload.size(), transferred)) != Result::Done) {
            Require(result == Result::WantWrite && transferred == 0, "a stalled write must want a write and report nothing transferred");
            waited = true;
            if (!misused) {
                misused = true;
                std::vector<std::uint8_t> moved(payload);
                Require(tls.Write(moved.data(), moved.size(), transferred) == Result::Ssl && transferred == 0,
                    "retrying a stalled write with another buffer is a bad write retry");
                Require(tls.FailureKind() == LibreSslTls::Failure::BadWriteRetry, "a bad write retry is classified");
                Require(tls.Write(payload.data(), 10, transferred) == Result::Ssl && tls.FailureKind() == LibreSslTls::Failure::BadWriteRetry,
                    "retrying a stalled write with a shorter length is a bad write retry");
                LibreSslTls::WriteOptions moving;
                moving.movingBuffer = true;
                Require(tls.Write(moved.data(), moved.size(), transferred, moving) != Result::Ssl, "a moving write buffer is accepted when the option is set");
            }
            Require(++rounds < WaitRounds, "the stalled write never finished");
            Settle();
        }
        Require(waited, "a 16 MiB write into a sleeping peer never stalled; back-pressure was not exercised");
        Require(transferred == payload.size(), "a completed write transferred every byte");
        char ack = 0;
        Require(Retry([&] { return tls.Read(&ack, 1, transferred, false); }) == Result::Done && ack == 'K', "the server's acknowledgement");
    };
    Run(scenario);
}

void Shutdown() {
    Scenario scenario;
    scenario.server = [](Connection& tls, int) {
        std::uint8_t byte = 0;
        std::size_t transferred = 5;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == Result::ZeroReturn && transferred == 0,
            "a peer close_notify must read as a clean zero return");
        Require(tls.ReceivedShutdown() && !tls.SentShutdown(), "the server only received the shutdown");
        Require(tls.Read(&byte, 1, transferred, false) == Result::ZeroReturn, "reading again after close_notify stays a zero return");
        Require(tls.Read(&byte, 1, transferred, true) == Result::ZeroReturn, "peeking after close_notify is a zero return");
        WriteAll(tls, "bye", 3);
        bool complete = false;
        Require(Retry([&] { return tls.Shutdown(complete); }) == Result::Done && complete, "answering a close_notify completes the shutdown");
        Require(tls.SentShutdown(), "the server sent its close_notify");
        transferred = 5;
        Require(tls.Write("x", 1, transferred) == Result::Ssl && transferred == 0, "writing after our own close_notify is an SSL error");
    };
    scenario.client = [](Connection& tls, int) {
        Require(Handshake(tls) == Result::Done, "shutdown handshake");
        bool complete = true;
        Require(Retry([&] { return tls.Shutdown(complete); }) == Result::Done && !complete, "the first shutdown sends close_notify and has not heard back");
        Require(tls.SentShutdown() && !tls.ReceivedShutdown(), "only our close_notify has been exchanged");
        std::size_t transferred = 5;
        Require(tls.Write("x", 1, transferred) == Result::Ssl && transferred == 0, "writing after our own close_notify is an SSL error");
        std::array<char, 8> out{};
        Require(Retry([&] { return tls.Read(out.data(), out.size(), transferred, false); }) == Result::Done && transferred == 3
                && std::memcmp(out.data(), "bye", 3) == 0,
            "data sent before the peer's close_notify is still readable after our shutdown");
        Require(Retry([&] { return tls.Read(out.data(), out.size(), transferred, false); }) == Result::ZeroReturn, "the peer's close_notify ends the stream cleanly");
        Require(tls.Shutdown(complete) == Result::Done && complete, "the second shutdown reports both close_notify alerts");
    };
    Run(scenario);
}

void ShutdownBeforeHandshake() {
    Scenario scenario;
    scenario.serverHandshake = false;
    scenario.client = [](Connection& tls, int) {
        bool complete = false;
        Require(tls.Shutdown(complete) == Result::Done && complete, "SSL_shutdown during init reports success without doing anything");
        Require(!tls.SentShutdown() && !tls.ReceivedShutdown(), "nothing was sent");
    };
    Run(scenario);
}

void AbruptClose() {
    Scenario eof;
    eof.server = [](Connection&, int) {};
    eof.client = [](Connection& tls, int) {
        Require(Handshake(tls) == Result::Done, "abrupt close handshake");
        std::array<char, 8> out{};
        std::size_t transferred = 5;
        Require(Retry([&] { return tls.Read(out.data(), out.size(), transferred, false); }) == Result::Syscall && transferred == 0,
            "end of stream without close_notify must be a syscall error");
        Require(!tls.ReceivedShutdown(), "an unexpected end of stream is not a received shutdown");
        Require(tls.SystemError() == 0, "an orderly end of stream leaves errno clear");
        Require(tls.LastDirection() == LibreSslTls::Direction::Read, "the failing transfer was a read");
        Require(tls.Read(out.data(), out.size(), transferred, false) == Result::Syscall, "reading again stays a syscall error");
        Result result = Result::Done;
        for (int attempt = 0; attempt < 200 && result == Result::Done; ++attempt) {
            result = tls.Write(out.data(), out.size(), transferred);
            Settle();
        }
        Require(result == Result::Syscall, "writing to a closed peer must end in a syscall error");
    };
    Run(eof);

    Gate clientReady;
    Scenario reset;
    reset.server = [&](Connection&, int fd) {
        clientReady.Wait("the client never finished its handshake");
        const LingerValue linger{1, 0};
        Require(setsockopt_nid_postfix(fd, GuestSocketLevel, GuestLingerOption, &linger, sizeof(linger)) == 0, "enable the zero linger");
    };
    reset.client = [&](Connection& tls, int) {
        Require(Handshake(tls) == Result::Done, "reset handshake");
        clientReady.Open();
        std::array<char, 8> out{};
        std::size_t transferred = 5;
        Require(Retry([&] { return tls.Read(out.data(), out.size(), transferred, false); }) == Result::Syscall && transferred == 0,
            "a reset connection must be a syscall error");
#ifndef _WIN32
        Require(tls.SystemError() == ECONNRESET, "the reset errno is exposed for SSL_ERROR_SYSCALL");
#endif
    };
    Run(reset);
}

void BlockingTransport() {
    Gate release;
    Scenario scenario;
    scenario.server = [&](Connection& tls, int) {
        release.Wait("the client never opened the gate");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        WriteAll(tls, "late", 4);
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == Result::ZeroReturn, "BlockingTransport: wait for the client's close");
    };
    scenario.client = [&](Connection& tls, int) {
        Require(tls.Handshake() == Result::Done, "a blocking socket finishes the handshake in one call");
        release.Open();
        std::array<char, 8> out{};
        std::size_t transferred = 0;
        const auto start = Clock::now();
        Require(tls.Read(out.data(), out.size(), transferred, false) == Result::Done && transferred == 4 && std::memcmp(out.data(), "late", 4) == 0,
            "a blocking read returns the data once it arrives");
        Require(ElapsedMilliseconds(start) >= 150, "a blocking read waited for the peer instead of returning at once");
        bool complete = false;
        Require(Retry([&] { return tls.Shutdown(complete); }) == Result::Done, "close the blocking connection");
    };
    Run(scenario);
}

void NonBlockingTransport() {
    Gate release;
    Scenario scenario;
    scenario.server = [&](Connection& tls, int) {
        release.Wait("the client never opened the gate");
        WriteAll(tls, "now", 3);
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == Result::ZeroReturn, "NonBlockingTransport: wait for the client's close");
    };
    scenario.client = [&](Connection& tls, int fd) {
        MakeNonBlocking(fd);
        SetTimeout(fd, GuestReceiveTimeout, 3000);
        SetTimeout(fd, GuestSendTimeout, 3000);
        Require(Handshake(tls) == Result::Done, "non-blocking handshake");
        std::array<char, 8> out{};
        std::size_t transferred = 0;
        const auto start = Clock::now();
        Require(tls.Read(out.data(), out.size(), transferred, false) == Result::WantRead && transferred == 0, "a non-blocking read wants a read");
        Require(ElapsedMilliseconds(start) < 1500, "a non-blocking read must ignore SO_RCVTIMEO and return at once");
        Require(tls.SystemError() == HostWouldBlockCode, "the would-block errno is reported");
        release.Open();
        Require(Retry([&] { return tls.Read(out.data(), out.size(), transferred, false); }) == Result::Done && transferred == 3, "the data after the gate");
        bool complete = false;
        Require(Retry([&] { return tls.Shutdown(complete); }) == Result::Done, "close the non-blocking connection");
    };
    Run(scenario);
}

void ReceiveTimeout() {
    Gate release;
    Scenario scenario;
    scenario.server = [&](Connection& tls, int) {
        release.Wait("the client never opened the gate");
        WriteAll(tls, "after", 5);
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == Result::ZeroReturn, "ReceiveTimeout: wait for the client's close");
    };
    scenario.client = [&](Connection& tls, int fd) {
        Require(Handshake(tls) == Result::Done, "timeout handshake");
        SetTimeout(fd, GuestReceiveTimeout, 250);
        std::array<char, 8> out{};
        std::size_t transferred = 3;
        const auto start = Clock::now();
        Require(tls.Read(out.data(), out.size(), transferred, false) == Result::WantRead && transferred == 0, "an expired receive timeout wants a read");
        const long waited = ElapsedMilliseconds(start);
        Require(waited >= 200 && waited < 5000, "the read waited for the receive timeout");
        Require(tls.SystemError() == HostWouldBlockCode, "an expired receive timeout is reported as would-block");
        Require(tls.LastDirection() == LibreSslTls::Direction::Read, "the timed out transfer was a read");
        release.Open();
        Require(Retry([&] { return tls.Read(out.data(), out.size(), transferred, false); }) == Result::Done && transferred == 5, "the read resumes after a timeout");
        bool complete = false;
        Require(Retry([&] { return tls.Shutdown(complete); }) == Result::Done, "close the connection");
    };
    Run(scenario);
}

void SendTimeout() {
    Gate release;
    Scenario scenario;
    scenario.server = [&](Connection&, int) { release.Wait("the client never opened the gate"); };
    scenario.client = [&](Connection& tls, int fd) {
        Require(Handshake(tls) == Result::Done, "send timeout handshake");
        if (FakeSocketSupport::SendTimeoutIsUnreachable(fd)) {
            release.Open();
            return;
        }
        SetTimeout(fd, GuestSendTimeout, 250);
        const auto chunk = Pattern(100, 9);
        std::size_t transferred = 0;
        long waited = 0;
        Result result = Result::Done;
        for (int round = 0; round < 400000 && result == Result::Done; ++round) {
            const auto start = Clock::now();
            result = tls.Write(chunk.data(), chunk.size(), transferred);
            waited = ElapsedMilliseconds(start);
        }
        Require(result == Result::WantWrite && transferred == 0, "a full send buffer with an expired send timeout wants a write");
        Require(waited >= 200 && waited < 5000, "the write waited for the send timeout");
        Require(tls.SystemError() == HostWouldBlockCode, "an expired send timeout is reported as would-block");
        Require(tls.LastDirection() == LibreSslTls::Direction::Write, "the timed out transfer was a write");
        release.Open();
    };
    Run(scenario);
}

void LargeRecordSendTimeout() {
    Gate release;
    Scenario scenario;
    scenario.server = [&](Connection&, int) { release.Wait("the client never opened the gate"); };
    scenario.serverReceiveBuffer = 4096;
    scenario.client = [&](Connection& tls, int fd) {
        Require(Handshake(tls) == Result::Done, "large record handshake");
        if (FakeSocketSupport::SendTimeoutIsUnreachable(fd)) {
            release.Open();
            return;
        }
        const int small = 4096;
        Require(setsockopt_nid_postfix(fd, GuestSocketLevel, GuestSendBuffer, &small, sizeof(small)) == 0, "shrink the send buffer");
        SetTimeout(fd, GuestSendTimeout, 250);
        const auto record = Pattern(RecordLimit, 11);
        std::size_t transferred = 0;
        Result result = Result::Done;
        for (int round = 0; round < 4096 && result == Result::Done; ++round) {
            const auto start = Clock::now();
            result = tls.Write(record.data(), record.size(), transferred);
            Require(ElapsedMilliseconds(start) < 5000, "a record larger than the free send space blocked past the send timeout");
        }
        Require(result == Result::WantWrite && transferred == 0, "a peer that stops reading leaves a large record wanting a write");
        Require(tls.SystemError() == HostWouldBlockCode, "the expired send timeout is reported as would-block");
        release.Open();
    };
    Run(scenario);
}

#ifndef _WIN32
void IgnoreSignal(int) {}

void SignalInterruptsBlockingRead() {
    struct sigaction action {};
    action.sa_handler = IgnoreSignal;
    sigemptyset(&action.sa_mask);
    struct sigaction previous {};
    Require(sigaction(SIGUSR2, &action, &previous) == 0, "install the signal handler without SA_RESTART");
    Gate release;
    Scenario scenario;
    scenario.server = [&](Connection& tls, int) {
        release.Wait("the client never opened the gate");
        WriteAll(tls, "after", 5);
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == Result::ZeroReturn, "SignalInterruptsBlockingRead: wait for the client's close");
    };
    scenario.client = [&](Connection& tls, int fd) {
        Require(Handshake(tls) == Result::Done, "signal handshake");
        std::array<char, 8> out{};
        std::size_t transferred = 0;
        if (!FakeSocketSupport::OwnerProvidesTransfer(fd)) {
            const pthread_t reader = pthread_self();
            std::thread sender([reader] {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                pthread_kill(reader, SIGUSR2);
            });
            const auto start = Clock::now();
            const Result result = tls.Read(out.data(), out.size(), transferred, false);
            const long waited = ElapsedMilliseconds(start);
            sender.join();
            Require(result == Result::WantRead && transferred == 0, "a signal interrupting a blocked read wants a read");
            Require(waited >= 150 && waited < 5000, "the read returned when the signal arrived");
            Require(tls.SystemError() == EINTR && tls.LastDirection() == LibreSslTls::Direction::Read, "the interruption is visible as errno EINTR");
        }
        release.Open();
        Require(Retry([&] { return tls.Read(out.data(), out.size(), transferred, false); }) == Result::Done && transferred == 5, "the read resumes after the interruption");
        bool complete = false;
        Require(Retry([&] { return tls.Shutdown(complete); }) == Result::Done, "close the connection");
    };
    Run(scenario);
    Require(sigaction(SIGUSR2, &previous, nullptr) == 0, "restore the signal handler");
}
#endif

void ConsultsNativeOwnerTransfer() {
    Gate release;
    Scenario scenario;
    scenario.server = [&](Connection& tls, int) {
        release.Wait("the client never opened the gate");
        WriteAll(tls, "one", 3);
        release.open = false;
        release.Wait("the client never reopened the gate");
        WriteAll(tls, "two", 3);
        std::array<std::uint8_t, 4> ping{};
        ReadExact(tls, ping.data(), ping.size());
        Require(std::memcmp(ping.data(), "ping", 4) == 0, "the interrupted write delivered its bytes once");
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == Result::ZeroReturn, "ConsultsNativeOwnerTransfer: wait for the client's close");
    };
    scenario.client = [&](Connection& tls, int fd) {
        const auto fake = InstallFake(fd);
        tls.SetSocket(FakeDescriptor);
        Require(Handshake(tls) == Result::Done, "handshake through the owner's transfer");
        Require(fake->calls > 0 && fake->sendCalls > 0, "the engine moved the handshake through NativeOwner::transfer, not the raw socket");
        std::array<char, 8> out{};
        std::size_t transferred = 0;
        release.Open();
        Require(Retry([&] { return tls.Read(out.data(), out.size(), transferred, false); }) == Result::Done && transferred == 3, "the first read");
        const int callsAfterFirstRead = fake->calls;
        release.open = false;

        fake->failNext = HostInterruptedCode;
        transferred = 1;
        Require(tls.Read(out.data(), out.size(), transferred, false) == Result::WantRead && transferred == 0, "an interrupted receive wants a read, as a retryable BIO error does");
        Require(tls.SystemError() == HostInterruptedCode && tls.LastDirection() == LibreSslTls::Direction::Read, "the interruption is visible as errno EINTR");
        Require(fake->calls > callsAfterFirstRead, "the interrupted read went through the owner's transfer");
        fake->failNext = HostWouldBlockCode;
        Require(tls.Read(out.data(), out.size(), transferred, false) == Result::WantRead && tls.SystemError() == HostWouldBlockCode, "a timeout from the transfer wants a read");

        fake->failNext = HostInterruptedCode;
        const std::array<char, 4> payload{'p', 'i', 'n', 'g'};
        Require(tls.Write(payload.data(), payload.size(), transferred) == Result::WantWrite && transferred == 0, "an interrupted send wants a write");
        Require(tls.SystemError() == HostInterruptedCode && tls.LastDirection() == LibreSslTls::Direction::Write, "the interrupted send is visible as errno EINTR");
        Require(Retry([&] { return tls.Write(payload.data(), payload.size(), transferred); }) == Result::Done && transferred == payload.size(), "the interrupted write completes when retried");

        release.Open();
        Require(Retry([&] { return tls.Read(out.data(), out.size(), transferred, false); }) == Result::Done && transferred == 3 && std::memcmp(out.data(), "two", 3) == 0,
            "the read completes when retried");
        bool complete = false;
        Require(Retry([&] { return tls.Shutdown(complete); }) == Result::Done, "close the connection");
        tls.DetachSocket();
        RemoveFake();
    };
    Run(scenario);
}

void ClosedDescriptor() {
    Gate closed;
    Scenario scenario;
    scenario.server = [&](Connection& tls, int) {
        closed.Wait("the client never closed its socket");
        std::uint8_t byte = 0;
        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Read(&byte, 1, transferred, false); }) == Result::Syscall && !tls.ReceivedShutdown(),
            "closing the guest descriptor must close the connection, not leave the engine holding it open");
    };
    scenario.client = [&](Connection& tls, int fd) {
        Require(Handshake(tls) == Result::Done, "close handshake");
        Require(close_nid_postfix(fd) == 0, "close the guest descriptor under the connection");
        closed.Open();
        std::array<char, 8> out{};
        std::size_t transferred = 3;
        Require(tls.Read(out.data(), out.size(), transferred, false) == Result::Syscall && transferred == 0, "a read on a closed descriptor is a syscall error");
        Require(tls.SystemError() == HostBadDescriptorCode, "a read on a closed descriptor reports EBADF");
        Require(tls.Write(out.data(), 1, transferred) == Result::Syscall && tls.SystemError() == HostBadDescriptorCode, "a write on a closed descriptor reports EBADF");
        Require(tls.LastDirection() == LibreSslTls::Direction::Write, "the failing transfer was a write");
    };
    Run(scenario);
}

void ReassignedSocket() {
    Gate release;
    Gate checked;
    Scenario scenario;
    scenario.server = [&](Connection& tls, int fd) {
        release.Wait("the client never moved to the second socket");
        MakeNonBlocking(fd);
        std::uint8_t byte = 0;
        std::size_t transferred = 5;
        Require(tls.Read(&byte, 1, transferred, false) == Result::WantRead && transferred == 0, "nothing reached the first socket after the switch");
        checked.Open();
    };
    scenario.client = [&](Connection& tls, int first) {
        Listener other;
        const int second = ConnectTo(other.port);
        const int peer = accept_nid_postfix(other.fd, nullptr, nullptr);
        Require(peer >= 0, "accept the second connection");
        MakeNonBlocking(peer);
        Require(Handshake(tls) == Result::Done, "handshake on the first socket");
        Require(tls.Socket() == first, "the connection reports the first socket");
        const std::uint64_t sentBefore = tls.BytesSent();

        RequireThrows<std::invalid_argument>([&] { tls.SetSocket(1 << 20); }, "switching to a descriptor that is not a guest socket was accepted");
        Require(tls.Socket() == first, "a rejected switch kept the first socket");
        tls.SetSocket(second);
        Require(tls.Socket() == second, "the connection reports the second socket");

        std::size_t transferred = 0;
        Require(Retry([&] { return tls.Write("hello", 5, transferred); }) == Result::Done && transferred == 5, "write after switching sockets");
        const auto wire = ReceiveRecord(peer);
        Require(wire.size() > 5 && wire[0] == 0x17 && wire[1] == 0x03 && wire[2] == 0x03, "the record went out on the second socket");
        Require(tls.BytesSent() == sentBefore + wire.size(), "the byte counter follows the second socket");
        release.Open();
        checked.Wait("the server never checked the first socket");

        tls.DetachSocket();
        Require(tls.Socket() == -1, "a detached connection has no socket");
        std::array<char, 8> out{};
        Require(tls.Read(out.data(), out.size(), transferred, false) == Result::Syscall && tls.SystemError() == HostBadDescriptorCode, "a detached connection reads EBADF");
        close_nid_postfix(second);
        close_nid_postfix(peer);
    };
    Run(scenario);
}

}

int main(int argc, char** argv) {
    const std::string group = argc > 1 ? argv[1] : "all";
    const bool all = group == "all";
    const bool handshake = all || group == "handshake";
    const bool data = all || group == "data";
    const bool transport = all || group == "transport";
    if (!handshake && !data && !transport) {
        std::fprintf(stderr, "unknown test group %s\n", group.c_str());
        return 2;
    }
    try {
        if (handshake) {
            ResultCodes();
            NotASocket();
            BadIdentity();
            UntrustedHandshake();
            TrustedHandshake();
            WrongHostname();
            NoHostname();
            VerifyPeerClient();
            ClientCertificate();
            ServerWithoutCertificate();
            EofDuringHandshake();
            ServerNameOnly();
        }
        if (data) {
            RoundTrip();
            PeekAndPending();
            ZeroLengthOperations();
            WriteOptions();
            Shutdown();
            ShutdownBeforeHandshake();
            AbruptClose();
        }
        if (transport) {
            NonBlockingRead();
            WriteBackPressure();
            BlockingTransport();
            NonBlockingTransport();
            ReceiveTimeout();
            SendTimeout();
            LargeRecordSendTimeout();
#ifndef _WIN32
            SignalInterruptsBlockingRead();
#endif
            ConsultsNativeOwnerTransfer();
            ClosedDescriptor();
            ReassignedSocket();
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "failed: %s\n", error.what());
        return 1;
    }
    std::puts("libressl tls engine ok");
    return 0;
}
