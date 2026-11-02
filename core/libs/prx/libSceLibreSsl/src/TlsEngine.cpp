#include "prx/libSceLibreSsl/include/TlsEngine.hpp"
#include "prx/libkernel/include/Api.hpp"
#ifdef _WIN32
#include <winsock2.h>
#else
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <cerrno>
#endif

#include "prx/libkernel/Socket/include/SocketPoll.hpp"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace LibreSslTls {

namespace {

int HostError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool HostWouldBlock(int error) {
#ifdef _WIN32
    return error == WSAEWOULDBLOCK;
#else
    return error == EAGAIN || error == EWOULDBLOCK;
#endif
}

bool HostInterrupted(int error) {
#ifdef _WIN32
    return error == WSAEINTR;
#else
    return error == EINTR;
#endif
}

bool HostReset(int error) {
#ifdef _WIN32
    return error == WSAECONNRESET || error == WSAECONNABORTED || error == WSAENETRESET;
#else
    return error == ECONNRESET || error == EPIPE;
#endif
}

void SetHostError(int error) {
#ifdef _WIN32
    WSASetLastError(error);
#else
    errno = error;
#endif
}

constexpr int HostBadDescriptor =
#ifdef _WIN32
    WSAEBADF;
#else
    EBADF;
#endif

constexpr int HostWouldBlockError =
#ifdef _WIN32
    WSAEWOULDBLOCK;
#else
    EAGAIN;
#endif

constexpr int GuestSocketLevel = 0xffff;
constexpr int GuestSendTimeout = 0x1005;
constexpr int GuestReceiveTimeout = 0x1006;
constexpr int GuestGetStatusFlags = 3;
constexpr int GuestNonBlockingFlag = 4;

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

bool GuestNonBlocking(int guestFd) {
    const int flags = fcntl_nid_postfix(guestFd, GuestGetStatusFlags);
    return flags >= 0 && (flags & GuestNonBlockingFlag) != 0;
}

int GuestTimeoutMilliseconds(int guestFd, bool send) {
    GuestTimeval time{};
    std::uint32_t length = sizeof(time);
    if (getsockopt_nid_postfix(guestFd, GuestSocketLevel, send ? GuestSendTimeout : GuestReceiveTimeout, &time, &length) != 0 || length != sizeof(time)) return 0;
    if (time.seconds <= 0 && time.microseconds <= 0) return 0;
    const std::int64_t seconds = std::clamp<std::int64_t>(time.seconds, 0, INT_MAX / 1000);
    const std::int64_t microseconds = std::clamp<std::int64_t>(time.microseconds, 0, 999999);
    const std::int64_t milliseconds = seconds * 1000 + (microseconds + 999) / 1000;
    return static_cast<int>(std::clamp<std::int64_t>(milliseconds, 1, INT_MAX));
}

std::int64_t NativeAttempt(std::uintptr_t native, bool send, void* buffer, int chunk, bool wait) {
#ifdef _WIN32
    static_cast<void>(wait);
    return send ? ::send(static_cast<SOCKET>(native), static_cast<const char*>(buffer), chunk, 0)
                : ::recv(static_cast<SOCKET>(native), static_cast<char*>(buffer), chunk, 0);
#else
#ifdef __linux__
    const int noWait = wait ? 0 : MSG_DONTWAIT;
#else
    static_cast<void>(wait);
    const int noWait = 0;
#endif
    return send ? ::send(static_cast<int>(native), buffer, static_cast<std::size_t>(chunk), MSG_NOSIGNAL | noWait)
                : ::recv(static_cast<int>(native), buffer, static_cast<std::size_t>(chunk), noWait);
#endif
}

bool SetHostNonBlocking(std::uintptr_t native, bool enabled) {
#ifdef _WIN32
    u_long value = enabled ? 1 : 0;
    return ioctlsocket(static_cast<SOCKET>(native), FIONBIO, &value) == 0;
#else
    int value = enabled ? 1 : 0;
    return ::ioctl(static_cast<int>(native), FIONBIO, &value) == 0;
#endif
}

std::int64_t AttemptWithoutWaiting(int guestFd, std::uintptr_t native, bool send, void* buffer, int chunk) {
#ifdef __linux__
    static_cast<void>(guestFd);
    return NativeAttempt(native, send, buffer, chunk, false);
#else
    if (!SetHostNonBlocking(native, true)) return -1;
    const std::int64_t moved = NativeAttempt(native, send, buffer, chunk, false);
    const int error = HostError();
    if (!SetHostNonBlocking(native, GuestNonBlocking(guestFd)) && moved < 0) return -1;
    SetHostError(error);
    return moved;
#endif
}

std::int64_t TimedTransfer(int guestFd, std::uintptr_t native, bool send, void* buffer, int chunk, int milliseconds) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    for (;;) {
        const auto left = std::chrono::ceil<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (left <= 0) {
            SetHostError(HostWouldBlockError);
            return -1;
        }
        const int wait = static_cast<int>(std::min<std::int64_t>(left, INT_MAX));
#ifdef _WIN32
        WSAPOLLFD ready{static_cast<SOCKET>(native), static_cast<SHORT>(send ? POLLWRNORM : POLLRDNORM), 0};
        const int waited = WSAPoll(&ready, 1, wait);
#else
        pollfd ready{static_cast<int>(native), static_cast<short>(send ? POLLOUT : POLLIN), 0};
        const int waited = ::poll(&ready, 1, wait);
#endif
        if (waited < 0) return -1;
        if (waited == 0) continue;
        const std::int64_t moved = AttemptWithoutWaiting(guestFd, native, send, buffer, chunk);
        if (moved >= 0 || !HostWouldBlock(HostError())) return moved;
    }
}

std::int64_t NativeTransfer(int guestFd, std::uintptr_t native, bool send, void* buffer, int chunk) {
    if (!GuestNonBlocking(guestFd)) {
        if (const int milliseconds = GuestTimeoutMilliseconds(guestFd, send)) return TimedTransfer(guestFd, native, send, buffer, chunk, milliseconds);
    }
    return NativeAttempt(native, send, buffer, chunk, true);
}

bool HostTimedOut(int error) {
#ifdef _WIN32
    return error == WSAETIMEDOUT;
#else
    (void)error;
    return false;
#endif
}

[[noreturn]] void Fail(const char* what, int code) {
    char text[160];
    mbedtls_strerror(code, text, sizeof(text));
    throw std::runtime_error(std::string("LibreSSL TLS engine: ") + what + ": " + text);
}

std::string Terminated(const unsigned char* data, std::size_t size, const char* what) {
    if (!data || size == 0) throw std::invalid_argument(std::string("LibreSSL TLS engine: empty ") + what);
    return std::string(reinterpret_cast<const char*>(data), size);
}

}

int ToSslError(Result result) {
    switch (result) {
        case Result::Done: return SslErrorNone;
        case Result::WantRead: return SslErrorWantRead;
        case Result::WantWrite: return SslErrorWantWrite;
        case Result::ZeroReturn: return SslErrorZeroReturn;
        case Result::Syscall: return SslErrorSyscall;
        case Result::Ssl: return SslErrorSsl;
    }
    throw std::invalid_argument("LibreSSL TLS engine: unknown result");
}

struct Connection::State {
    struct PendingWrite {
        bool active = false;
        const unsigned char* at = nullptr;
        std::size_t progress = 0;
        std::size_t fragment = 0;
    };

    bool server;
    int guestFd = -1;
    bool socketBound = false;
    int hostError = 0;
    int verifyMode = SslVerifyNone;
    std::string hostname;
    bool hostnameSet = false;
    bool hostnameVerifies = true;
    bool haveIdentity = false;
    bool prepared = false;
    bool handshaken = false;
    bool sentShutdown = false;
    bool receivedShutdown = false;
    bool closeNotifyReceived = false;
    bool fatalAlert = false;
    std::uint64_t receivedTotal = 0;
    std::uint64_t sentTotal = 0;
    bool alertPending = false;
    Direction direction = Direction::None;
    Failure failure = Failure::None;
    int failureCode = 0;
    std::vector<bool> chain;
    std::string peerServerName;
    std::vector<unsigned char> held;
    std::size_t heldOffset = 0;
    std::vector<unsigned char> scratch;
    PendingWrite pendingWrite;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_x509_crt trusted;
    mbedtls_x509_crt own;
    mbedtls_pk_context key;
    mbedtls_ssl_config config;
    mbedtls_ssl_context ssl;

    explicit State(bool isServer) : server(isServer) {
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&drbg);
        mbedtls_x509_crt_init(&trusted);
        mbedtls_x509_crt_init(&own);
        mbedtls_pk_init(&key);
        mbedtls_ssl_config_init(&config);
        mbedtls_ssl_init(&ssl);
    }

    ~State() {
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&config);
        mbedtls_pk_free(&key);
        mbedtls_x509_crt_free(&own);
        mbedtls_x509_crt_free(&trusted);
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
    }

    void Seed() {
        if (int result = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, nullptr, 0)) Fail("random generator seeding failed", result);
        if (int result = mbedtls_ssl_config_defaults(&config, server ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT))
            Fail("configuration defaults failed", result);
        mbedtls_ssl_conf_min_tls_version(&config, MBEDTLS_SSL_VERSION_TLS1_2);
        mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &drbg);
    }

    void RequireUnstarted(const char* operation) const {
        if (prepared) throw std::logic_error(std::string("LibreSSL TLS engine: ") + operation + " after the handshake started; the behaviour is not modelled");
    }

    int AuthenticationMode() const {
        if (server) {
            if (!(verifyMode & SslVerifyPeer)) return MBEDTLS_SSL_VERIFY_NONE;
            return (verifyMode & SslVerifyFailIfNoPeerCert) ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_OPTIONAL;
        }
        return verifyMode == SslVerifyNone ? MBEDTLS_SSL_VERIFY_OPTIONAL : MBEDTLS_SSL_VERIFY_REQUIRED;
    }

    void Prepare() {
        if (prepared) return;
        mbedtls_ssl_conf_authmode(&config, AuthenticationMode());
        mbedtls_ssl_conf_verify(&config, Verify, this);
        if (server) mbedtls_ssl_conf_sni(&config, ServerName, this);
        if (trusted.raw.p) mbedtls_ssl_conf_ca_chain(&config, &trusted, nullptr);
        if (haveIdentity) {
            if (int result = mbedtls_ssl_conf_own_cert(&config, &own, &key)) Fail("certificate setup failed", result);
        }
        if (int result = mbedtls_ssl_setup(&ssl, &config)) Fail("connection setup failed", result);
        if (!server) {
            if (int result = mbedtls_ssl_set_hostname(&ssl, hostnameSet ? hostname.c_str() : nullptr)) Fail("host name setup failed", result);
        }
        mbedtls_ssl_set_bio(&ssl, this, Send, Receive, nullptr);
        prepared = true;
    }

    std::int64_t Transfer(bool send, unsigned char* buffer, int chunk) {
        std::shared_ptr<void> owner;
        std::uintptr_t native = 0;
        if (guestFd < 0 || !KernelLookupSocketNative_nid_no_patch(guestFd, &owner, &native)) {
            SetHostError(HostBadDescriptor);
            return -1;
        }
        auto* socket = static_cast<KernelSocketPoll::NativeOwner*>(owner.get());
        if (socket && socket->transfer) return socket->transfer(socket, send, buffer, chunk);
        return NativeTransfer(guestFd, native, send, buffer, chunk);
    }

    static int Send(void* argument, const unsigned char* buffer, std::size_t length) {
        auto& self = *static_cast<State*>(argument);
        const auto chunk = static_cast<int>(std::min<std::size_t>(length, INT_MAX));
        self.hostError = 0;
        const auto sent = self.Transfer(true, const_cast<unsigned char*>(buffer), chunk);
        if (sent > 0) self.sentTotal += static_cast<std::uint64_t>(sent);
        if (sent >= 0) return static_cast<int>(sent);
        self.direction = Direction::Write;
        const int error = HostError();
        self.hostError = error;
        if (HostInterrupted(error) || HostWouldBlock(error) || HostTimedOut(error)) return MBEDTLS_ERR_SSL_WANT_WRITE;
        if (HostReset(error)) return MBEDTLS_ERR_NET_CONN_RESET;
        return MBEDTLS_ERR_NET_SEND_FAILED;
    }

    static int Verify(void* argument, mbedtls_x509_crt* certificate, int depth, std::uint32_t* flags) {
        auto& self = *static_cast<State*>(argument);
        const auto index = static_cast<std::size_t>(depth);
        if (self.chain.size() <= index) self.chain.resize(index + 1, false);
        self.chain[index] = certificate->issuer_raw.len == certificate->subject_raw.len
            && std::memcmp(certificate->issuer_raw.p, certificate->subject_raw.p, certificate->subject_raw.len) == 0;
        if (depth == 0 && !self.hostnameVerifies) *flags &= ~static_cast<std::uint32_t>(MBEDTLS_X509_BADCERT_CN_MISMATCH);
        return 0;
    }

    static int ServerName(void* argument, mbedtls_ssl_context*, const unsigned char* name, std::size_t length) {
        static_cast<State*>(argument)->peerServerName.assign(reinterpret_cast<const char*>(name), length);
        return 0;
    }

    static int Receive(void* argument, unsigned char* buffer, std::size_t length) {
        auto& self = *static_cast<State*>(argument);
        const auto chunk = static_cast<int>(std::min<std::size_t>(length, INT_MAX));
        self.hostError = 0;
        const auto received = self.Transfer(false, buffer, chunk);
        if (received > 0) {
            self.receivedTotal += static_cast<std::uint64_t>(received);
            return static_cast<int>(received);
        }
        self.direction = Direction::Read;
        if (received == 0) return 0;
        const int error = HostError();
        self.hostError = error;
        if (HostInterrupted(error) || HostWouldBlock(error) || HostTimedOut(error)) return MBEDTLS_ERR_SSL_WANT_READ;
        if (HostReset(error)) return MBEDTLS_ERR_NET_CONN_RESET;
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }

    Result Classify(int code) {
        switch (code) {
            case MBEDTLS_ERR_SSL_WANT_READ: return Result::WantRead;
            case MBEDTLS_ERR_SSL_WANT_WRITE: return Result::WantWrite;
            case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
                receivedShutdown = true;
                closeNotifyReceived = true;
                return Result::ZeroReturn;
            case MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE:
                receivedShutdown = true;
                fatalAlert = true;
                return Failed(Failure::Other, code);
            case MBEDTLS_ERR_X509_CERT_VERIFY_FAILED:
            case MBEDTLS_ERR_SSL_CA_CHAIN_REQUIRED: return Failed(Failure::VerifyFailed, code);
            case MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE: return Failed(server ? Failure::NoSharedCipher : Failure::Other, code);
            case 0:
            case MBEDTLS_ERR_SSL_CONN_EOF:
            case MBEDTLS_ERR_NET_CONN_RESET:
            case MBEDTLS_ERR_NET_RECV_FAILED:
            case MBEDTLS_ERR_NET_SEND_FAILED: return Result::Syscall;
            default: return Failed(Failure::Other, code);
        }
    }

    Result Failed(Failure kind, int code) {
        failure = kind;
        failureCode = code;
        return Result::Ssl;
    }

    void BeginOperation() {
        hostError = 0;
        direction = Direction::None;
        failure = Failure::None;
        failureCode = 0;
    }

    Result Establish() {
        if (handshaken) return Result::Done;
        if (!socketBound) return Failed(Failure::Other, 0);
        Prepare();
        const int code = mbedtls_ssl_handshake(&ssl);
        if (code == 0) {
            handshaken = true;
            return Result::Done;
        }
        return Classify(code);
    }

    std::size_t HeldAvailable() const { return held.size() - heldOffset; }

    std::size_t InputCapacity() const {
        const int capacity = mbedtls_ssl_get_max_in_record_payload(&ssl);
        if (capacity <= 0) Fail("input record size query failed", capacity);
        return static_cast<std::size_t>(capacity);
    }

    std::size_t OutputCapacity() const {
        const int capacity = mbedtls_ssl_get_max_out_record_payload(&ssl);
        if (capacity <= 0) Fail("output record size query failed", capacity);
        return static_cast<std::size_t>(capacity);
    }

    void ReleaseHeld() {
        held.clear();
        heldOffset = 0;
    }
};

Connection::Connection(bool server) : state(std::make_unique<State>(server)) { state->Seed(); }

Connection::~Connection() = default;

void Connection::SetSocket(int guestFd) {
    std::shared_ptr<void> owner;
    std::uintptr_t native = 0;
    if (!KernelLookupSocketNative_nid_no_patch(guestFd, &owner, &native))
        throw std::invalid_argument("LibreSSL TLS engine: descriptor " + std::to_string(guestFd) + " is not a guest socket");
    state->guestFd = guestFd;
    state->socketBound = true;
}

void Connection::DetachSocket() {
    state->guestFd = -1;
    state->socketBound = true;
}

int Connection::Socket() const { return state->guestFd; }

void Connection::SetHostname(const char* name) {
    state->RequireUnstarted("SetHostname");
    state->hostnameSet = name != nullptr;
    state->hostname = name ? name : "";
    state->hostnameVerifies = true;
}

void Connection::SetServerName(const char* name) {
    state->RequireUnstarted("SetServerName");
    state->hostnameSet = name != nullptr;
    state->hostname = name ? name : "";
    state->hostnameVerifies = false;
}

void Connection::SetVerifyMode(int sslVerifyMode) {
    state->RequireUnstarted("SetVerifyMode");
    if (sslVerifyMode & ~(SslVerifyPeer | SslVerifyFailIfNoPeerCert | SslVerifyClientOnce))
        throw std::invalid_argument("LibreSSL TLS engine: unknown verify mode bits");
    state->verifyMode = sslVerifyMode;
}

void Connection::TrustCertificates(const unsigned char* pem, std::size_t size) {
    state->RequireUnstarted("TrustCertificates");
    const auto text = Terminated(pem, size, "trusted certificate list");
    if (int result = mbedtls_x509_crt_parse(&state->trusted, reinterpret_cast<const unsigned char*>(text.c_str()), text.size() + 1))
        Fail("trusted certificates rejected", result);
}

void Connection::UseCertificate(const unsigned char* certificatePem, std::size_t certificateSize, const unsigned char* keyPem, std::size_t keySize) {
    state->RequireUnstarted("UseCertificate");
    const auto certificate = Terminated(certificatePem, certificateSize, "certificate");
    auto keyText = Terminated(keyPem, keySize, "private key");
    mbedtls_x509_crt_free(&state->own);
    mbedtls_x509_crt_init(&state->own);
    mbedtls_pk_free(&state->key);
    mbedtls_pk_init(&state->key);
    state->haveIdentity = false;
    if (int result = mbedtls_x509_crt_parse(&state->own, reinterpret_cast<const unsigned char*>(certificate.c_str()), certificate.size() + 1))
        Fail("certificate rejected", result);
    const int parsed = mbedtls_pk_parse_key(&state->key, reinterpret_cast<const unsigned char*>(keyText.c_str()), keyText.size() + 1, nullptr, 0,
        mbedtls_ctr_drbg_random, &state->drbg);
    mbedtls_platform_zeroize(keyText.data(), keyText.size());
    if (parsed) Fail("private key rejected", parsed);
    if (int result = mbedtls_pk_check_pair(&state->own.pk, &state->key, mbedtls_ctr_drbg_random, &state->drbg))
        Fail("certificate and private key do not match", result);
    state->haveIdentity = true;
}

Result Connection::Handshake() {
    auto& self = *state;
    self.BeginOperation();
    if (self.handshaken) return Result::Done;
    if (self.receivedShutdown) return self.closeNotifyReceived ? Result::ZeroReturn : Result::Syscall;
    return self.Establish();
}

Result Connection::Read(void* buffer, std::size_t size, std::size_t& transferred, bool peek) {
    transferred = 0;
    if (size > static_cast<std::size_t>(INT_MAX)) throw std::invalid_argument("LibreSSL TLS engine: read length exceeds INT_MAX");
    if (!buffer && size) throw std::invalid_argument("LibreSSL TLS engine: null read buffer");
    auto& self = *state;
    self.BeginOperation();
    if (self.receivedShutdown) return self.closeNotifyReceived ? Result::ZeroReturn : Result::Syscall;
    if (const auto established = self.Establish(); established != Result::Done) return established;
    if (!self.HeldAvailable()) {
        if (!peek && size > 0) {
            const int code = mbedtls_ssl_read(&self.ssl, static_cast<unsigned char*>(buffer), size);
            if (code > 0) {
                transferred = static_cast<std::size_t>(code);
                return Result::Done;
            }
            return self.Classify(code);
        }
        self.held.resize(self.InputCapacity());
        const int code = mbedtls_ssl_read(&self.ssl, self.held.data(), self.held.size());
        if (code <= 0) {
            self.ReleaseHeld();
            return self.Classify(code);
        }
        self.held.resize(static_cast<std::size_t>(code));
        self.heldOffset = 0;
    }
    const auto count = std::min(size, self.HeldAvailable());
    if (count) std::memcpy(buffer, self.held.data() + self.heldOffset, count);
    if (!peek) {
        self.heldOffset += count;
        if (!self.HeldAvailable()) self.ReleaseHeld();
    }
    transferred = count;
    return size == 0 ? Result::Syscall : Result::Done;
}

Result Connection::Write(const void* buffer, std::size_t size, std::size_t& transferred, const WriteOptions& options) {
    transferred = 0;
    if (size > static_cast<std::size_t>(INT_MAX)) throw std::invalid_argument("LibreSSL TLS engine: write length exceeds INT_MAX");
    if (!buffer && size) throw std::invalid_argument("LibreSSL TLS engine: null write buffer");
    if (options.fragment == 0) throw std::invalid_argument("LibreSSL TLS engine: zero write fragment size");
    auto& self = *state;
    self.BeginOperation();
    if (const auto established = self.Establish(); established != Result::Done) return established;
    if (self.sentShutdown) return self.Failed(Failure::Other, 0);
    auto& pending = self.pendingWrite;
    const auto* bytes = static_cast<const unsigned char*>(buffer);
    std::size_t total = pending.active ? pending.progress : 0;
    std::size_t remaining = std::max(size, total) - total;
    for (;;) {
        const std::size_t fragment = std::min(remaining, options.fragment);
        const unsigned char* at = bytes ? bytes + total : nullptr;
        std::size_t length = fragment;
        if (pending.active) {
            if (pending.fragment > fragment || (at != pending.at && !options.movingBuffer)) return self.Failed(Failure::BadWriteRetry, 0);
            length = pending.fragment;
        } else if (fragment == 0) {
            return Result::Syscall;
        }
        const int code = mbedtls_ssl_write(&self.ssl, at, length);
        if (code > 0) {
            pending = {};
            const auto written = static_cast<std::size_t>(code);
            if (written == remaining || options.partial) {
                transferred = total + written;
                return Result::Done;
            }
            remaining -= written;
            total += written;
            continue;
        }
        if (code == MBEDTLS_ERR_SSL_WANT_READ || code == MBEDTLS_ERR_SSL_WANT_WRITE) {
            if (!pending.active) pending = {true, at, total, fragment};
            return self.Classify(code);
        }
        pending = {};
        return self.Classify(code);
    }
}

Result Connection::Shutdown(bool& complete) {
    state->BeginOperation();
    complete = false;
    auto& self = *state;
    if (!self.handshaken) {
        complete = true;
        return Result::Done;
    }
    if (!self.sentShutdown) {
        self.sentShutdown = true;
        self.alertPending = true;
    }
    if (self.alertPending) {
        const int code = mbedtls_ssl_close_notify(&self.ssl);
        if (code != 0) return self.Classify(code);
        self.alertPending = false;
    } else if (!self.receivedShutdown) {
        self.ReleaseHeld();
        self.scratch.resize(self.InputCapacity());
        const int code = mbedtls_ssl_read(&self.ssl, self.scratch.data(), self.scratch.size());
        if (code == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || code == MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE) {
            self.Classify(code);
        } else if (code > 0) {
            return Result::Syscall;
        } else {
            return self.Classify(code);
        }
    }
    complete = self.sentShutdown && self.receivedShutdown && !self.alertPending;
    return Result::Done;
}

std::size_t Connection::Pending() const {
    const auto& self = *state;
    return self.HeldAvailable() + (self.handshaken ? mbedtls_ssl_get_bytes_avail(&self.ssl) : 0);
}

bool Connection::HandshakeDone() const { return state->handshaken; }

bool Connection::SentShutdown() const { return state->sentShutdown; }

bool Connection::ReceivedShutdown() const { return state->receivedShutdown; }

bool Connection::FatalAlertReceived() const { return state->fatalAlert; }

std::uint64_t Connection::BytesReceived() const { return state->receivedTotal; }

std::uint64_t Connection::BytesSent() const { return state->sentTotal; }

int Connection::SystemError() const { return state->hostError; }

Direction Connection::LastDirection() const { return state->direction; }

Failure Connection::FailureKind() const { return state->failure; }

int Connection::FailureCode() const { return state->failureCode; }

std::string Connection::FailureText() const {
    char text[160];
    mbedtls_strerror(state->failureCode, text, sizeof(text));
    return text;
}

ChainShape Connection::PeerChain() const {
    const auto& chain = state->chain;
    if (chain.empty()) return ChainShape::None;
    if (chain.size() == 1) return chain.front() ? ChainShape::SelfSignedLeaf : ChainShape::Incomplete;
    return chain.back() ? ChainShape::EndsInSelfSigned : ChainShape::Incomplete;
}

const std::string& Connection::PeerServerName() const { return state->peerServerName; }

std::uint32_t Connection::VerifyFlags() const {
    if (!state->handshaken && state->failure != Failure::VerifyFailed) throw std::logic_error("LibreSSL TLS engine: the verify result is read before the handshake finished");
    return mbedtls_ssl_get_verify_result(&state->ssl);
}

const char* Connection::Version() const {
    if (!state->handshaken) throw std::logic_error("LibreSSL TLS engine: the version is read before the handshake finished");
    return mbedtls_ssl_get_version(&state->ssl);
}

const char* Connection::Cipher() const {
    if (!state->handshaken) throw std::logic_error("LibreSSL TLS engine: the cipher is read before the handshake finished");
    return mbedtls_ssl_get_ciphersuite(&state->ssl);
}

}
