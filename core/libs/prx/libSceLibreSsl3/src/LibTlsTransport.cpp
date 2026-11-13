#include "prx/libSceLibreSsl3/include/LibTls.hpp"

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include "prx/libkernel/include/Api.hpp"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <cerrno>
#include <sys/socket.h>
#endif

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr std::int64_t WantPollIn = -2;
constexpr std::int64_t WantPollOut = -3;
constexpr std::size_t WriteChunkSize = 16 * 1024;
constexpr std::uint32_t TlsProtocol12 = 0x08;
constexpr std::uint32_t TlsProtocol13 = 0x10;

int HostError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool HostInterrupted(int error) {
#ifdef _WIN32
    return error == WSAEINTR;
#else
    return error == EINTR;
#endif
}

bool HostWouldBlock(int error) {
#ifdef _WIN32
    return error == WSAEWOULDBLOCK;
#else
    return error == EAGAIN || error == EWOULDBLOCK;
#endif
}

bool HostReset(int error) {
#ifdef _WIN32
    return error == WSAECONNRESET || error == WSAECONNABORTED || error == WSAENETRESET;
#else
    return error == ECONNRESET || error == EPIPE;
#endif
}

bool HostTimedOut(int error) {
#ifdef _WIN32
    return error == WSAETIMEDOUT;
#else
    (void)error;
    return false;
#endif
}

struct Session {
    std::mutex mutex;
    std::shared_ptr<void> socketOwner;
    std::uintptr_t socket;
    bool verifyName;
    bool connected = false;
    bool handshaken = false;
    bool closed = false;
    int hostError = 0;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context random;
    mbedtls_ssl_config config;
    mbedtls_ssl_context ssl;
    mbedtls_x509_crt authorities;

    Session(std::shared_ptr<void> owner, std::uintptr_t native, bool nameVerification)
        : socketOwner(std::move(owner)), socket(native), verifyName(nameVerification) {
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&random);
        mbedtls_ssl_config_init(&config);
        mbedtls_ssl_init(&ssl);
        mbedtls_x509_crt_init(&authorities);
    }

    ~Session() {
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&config);
        mbedtls_x509_crt_free(&authorities);
        mbedtls_ctr_drbg_free(&random);
        mbedtls_entropy_free(&entropy);
    }

    static int Send(void* argument, const unsigned char* buffer, std::size_t length) {
        auto& session = *static_cast<Session*>(argument);
        const int size = static_cast<int>(std::min<std::size_t>(length, INT_MAX));
        for (;;) {
#ifdef _WIN32
            auto* owner = static_cast<KernelSocketPoll::NativeOwner*>(session.socketOwner.get());
            const int result = owner && owner->transfer
                ? owner->transfer(owner, true, const_cast<unsigned char*>(buffer), size)
                : ::send(static_cast<SOCKET>(session.socket), reinterpret_cast<const char*>(buffer), size, 0);
#else
            const int result = static_cast<int>(::send(static_cast<int>(session.socket), buffer,
                static_cast<std::size_t>(size), MSG_NOSIGNAL));
#endif
            if (result >= 0) return result;
            const int error = HostError();
            if (HostInterrupted(error)) continue;
            if (HostWouldBlock(error) || HostTimedOut(error)) return MBEDTLS_ERR_SSL_WANT_WRITE;
            if (HostReset(error)) return MBEDTLS_ERR_NET_CONN_RESET;
            session.hostError = error;
            return MBEDTLS_ERR_NET_SEND_FAILED;
        }
    }

    static int Receive(void* argument, unsigned char* buffer, std::size_t length) {
        auto& session = *static_cast<Session*>(argument);
        const int size = static_cast<int>(std::min<std::size_t>(length, INT_MAX));
        for (;;) {
#ifdef _WIN32
            auto* owner = static_cast<KernelSocketPoll::NativeOwner*>(session.socketOwner.get());
            const int result = owner && owner->transfer
                ? owner->transfer(owner, false, buffer, size)
                : ::recv(static_cast<SOCKET>(session.socket), reinterpret_cast<char*>(buffer), size, 0);
#else
            const int result = static_cast<int>(::recv(static_cast<int>(session.socket), buffer,
                static_cast<std::size_t>(size), 0));
#endif
            if (result >= 0) return result;
            const int error = HostError();
            if (HostInterrupted(error)) continue;
            if (HostWouldBlock(error) || HostTimedOut(error)) return MBEDTLS_ERR_SSL_WANT_READ;
            if (HostReset(error)) return MBEDTLS_ERR_NET_CONN_RESET;
            session.hostError = error;
            return MBEDTLS_ERR_NET_RECV_FAILED;
        }
    }

    static int Verify(void* argument, mbedtls_x509_crt*, int, std::uint32_t* flags) {
        const auto& session = *static_cast<Session*>(argument);
        if (!session.verifyName) *flags &= ~MBEDTLS_X509_BADCERT_CN_MISMATCH;
        return 0;
    }

    int Configure(const TlsTransportConfig& settings, const char* servername) {
        if (!settings.client) return -1;
        if ((settings.protocols & (TlsProtocol12 | TlsProtocol13)) == 0) return -1;
        int result = mbedtls_ctr_drbg_seed(&random, mbedtls_entropy_func, &entropy, nullptr, 0);
        if (result != 0) return result;
        result = mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
            MBEDTLS_SSL_PRESET_DEFAULT);
        if (result != 0) return result;
        mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &random);
        mbedtls_ssl_conf_verify(&config, Verify, this);
        if ((settings.protocols & TlsProtocol12) != 0) {
            mbedtls_ssl_conf_min_tls_version(&config, MBEDTLS_SSL_VERSION_TLS1_2);
        } else {
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
            mbedtls_ssl_conf_min_tls_version(&config, MBEDTLS_SSL_VERSION_TLS1_3);
#else
            return MBEDTLS_ERR_SSL_BAD_CONFIG;
#endif
        }
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
        if ((settings.protocols & TlsProtocol13) != 0) {
            mbedtls_ssl_conf_max_tls_version(&config, MBEDTLS_SSL_VERSION_TLS1_3);
        } else {
            mbedtls_ssl_conf_max_tls_version(&config, MBEDTLS_SSL_VERSION_TLS1_2);
        }
#else
        mbedtls_ssl_conf_max_tls_version(&config, MBEDTLS_SSL_VERSION_TLS1_2);
#endif

        std::vector<unsigned char> ca;
        if (settings.ca && settings.caLength) {
            ca.assign(settings.ca, settings.ca + settings.caLength);
        } else {
            const char* const sandbox = sceKernelGetFsSandboxRandomWord();
            if (!sandbox) return MBEDTLS_ERR_X509_FILE_IO_ERROR;
            const std::string path = std::string("/") + sandbox + "/common/cert/CA_LIST.cer";
            const int descriptor = sceKernelOpen(path.c_str(), 0, 0);
            if (descriptor < 0) return MBEDTLS_ERR_X509_FILE_IO_ERROR;
            std::array<char, 16384> buffer{};
            for (;;) {
                const auto count = sceKernelRead(descriptor, buffer.data(), buffer.size());
                if (count < 0) {
                    sceKernelClose(descriptor);
                    return MBEDTLS_ERR_X509_FILE_IO_ERROR;
                }
                if (count == 0) break;
                ca.insert(ca.end(), buffer.begin(), buffer.begin() + count);
            }
            sceKernelClose(descriptor);
        }
        if (ca.empty() || ca.back() != 0) ca.push_back(0);
        result = mbedtls_x509_crt_parse(&authorities, ca.data(), ca.size());
        if (result < 0 || authorities.raw.p == nullptr) {
            return result < 0 ? result : MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
        }
        mbedtls_ssl_conf_ca_chain(&config, &authorities, nullptr);
        result = mbedtls_ssl_setup(&ssl, &config);
        if (result != 0) return result;
        if (servername) {
            result = mbedtls_ssl_set_hostname(&ssl, servername);
            if (result != 0) return result;
        } else if (verifyName) {
            return MBEDTLS_ERR_X509_BAD_INPUT_DATA;
        } else {
            result = mbedtls_ssl_set_hostname(&ssl, nullptr);
            if (result != 0) return result;
        }
        mbedtls_ssl_set_bio(&ssl, this, Send, Receive, nullptr);
        connected = true;
        return 0;
    }

    std::int64_t Map(int result) const {
        if (result == MBEDTLS_ERR_SSL_WANT_READ) return WantPollIn;
        if (result == MBEDTLS_ERR_SSL_WANT_WRITE) return WantPollOut;
        return -1;
    }

    std::string Error(int result, const char* operation) const {
        char message[256];
        mbedtls_strerror(result, message, sizeof(message));
        std::string error(operation);
        error += ": ";
        error += message;
        if (hostError) error += ", host socket error " + std::to_string(hostError);
        return error;
    }
};

std::mutex sessionsMutex;
std::unordered_map<TlsContext*, std::shared_ptr<Session>> sessions;

std::shared_ptr<Session> Find(TlsContext* context) {
    std::lock_guard lock(sessionsMutex);
    const auto found = sessions.find(context);
    return found == sessions.end() ? nullptr : found->second;
}

void SetError(TlsContext* context, Session& session, int result, const char* operation) {
    const auto error = session.Error(result, operation);
    TlsTransportSetError_nid_no_patch(context, error.c_str());
}

std::int64_t Handshake(TlsContext* context, Session& session) {
    if (!session.connected || session.closed) {
        TlsTransportSetError_nid_no_patch(context, "context not connected");
        return -1;
    }
    if (session.handshaken) {
        TlsTransportSetError_nid_no_patch(context, "handshake already completed");
        return -1;
    }
    session.hostError = 0;
    const int result = mbedtls_ssl_handshake(&session.ssl);
    if (result == 0) {
        session.handshaken = true;
        return 0;
    }
    const auto mapped = session.Map(result);
    if (mapped == WantPollIn || mapped == WantPollOut) return mapped;
    SetError(context, session, result, "handshake");
    return -1;
}

}

int TlsTransportClose_nid_no_patch(TlsContext* context) {
    const auto session = Find(context);
    if (!session) return 0;
    std::unique_lock lock(session->mutex);
    if (session->closed) return 0;
    if (!session->connected) return 0;
    session->hostError = 0;
    const int result = mbedtls_ssl_close_notify(&session->ssl);
    if (result == MBEDTLS_ERR_SSL_WANT_READ) return static_cast<int>(WantPollIn);
    if (result == MBEDTLS_ERR_SSL_WANT_WRITE) return static_cast<int>(WantPollOut);
    session->closed = true;
    if (result != 0) {
        SetError(context, *session, result, "shutdown");
        return -1;
    }
    return 0;
}

void TlsTransportFree_nid_no_patch(TlsContext* context) {
    std::lock_guard lock(sessionsMutex);
    sessions.erase(context);
}

int TlsTransportConnect_nid_no_patch(TlsContext* context, int socket, const char* servername) {
    TlsTransportClearError_nid_no_patch(context);
    if (!context || socket < 0) {
        TlsTransportSetError_nid_no_patch(context, "invalid file descriptor");
        return -1;
    }
    TlsTransportConfig settings{};
    if (!TlsTransportGetConfig_nid_no_patch(context, &settings) || !settings.client) {
        TlsTransportSetError_nid_no_patch(context, "not a client context");
        return -1;
    }
    if (Find(context)) {
        TlsTransportSetError_nid_no_patch(context, "context already connected");
        return -1;
    }
    std::string host;
    if (servername && CopyGuestString_nid_no_patch(servername, 1024, host) != 0) {
        TlsTransportSetError_nid_no_patch(context, "invalid server name");
        return -1;
    }
    if (!host.empty() && host.back() == '.') host.pop_back();
    if (settings.verifyName && host.empty()) {
        TlsTransportSetError_nid_no_patch(context, "server name not specified");
        return -1;
    }
    std::shared_ptr<void> owner;
    std::uintptr_t native = 0;
    if (!KernelLookupSocketNative_nid_no_patch(socket, &owner, &native)) {
        TlsTransportSetError_nid_no_patch(context, "invalid socket");
        return -1;
    }
    auto session = std::make_shared<Session>(std::move(owner), native, settings.verifyName);
    const int result = session->Configure(settings, host.empty() ? nullptr : host.c_str());
    if (result != 0) {
        SetError(context, *session, result, "TLS setup");
        return -1;
    }
    {
        std::lock_guard lock(sessionsMutex);
        if (sessions.contains(context)) {
            TlsTransportSetError_nid_no_patch(context, "context already connected");
            return -1;
        }
        sessions.emplace(context, std::move(session));
    }
    return 0;
}

int TlsTransportHandshake_nid_no_patch(TlsContext* context) {
    TlsTransportClearError_nid_no_patch(context);
    const auto session = Find(context);
    if (!session) {
        TlsTransportSetError_nid_no_patch(context, "context not connected");
        return -1;
    }
    std::unique_lock lock(session->mutex);
    return static_cast<int>(Handshake(context, *session));
}

std::int64_t TlsTransportRead_nid_no_patch(TlsContext* context, void* buffer, std::size_t length) {
    TlsTransportClearError_nid_no_patch(context);
    if (length > static_cast<std::size_t>(INT_MAX) || (length && !buffer)) {
        TlsTransportSetError_nid_no_patch(context, "invalid read buffer");
        return -1;
    }
    const auto session = Find(context);
    if (!session) {
        TlsTransportSetError_nid_no_patch(context, "context not connected");
        return -1;
    }
    std::unique_lock lock(session->mutex);
    if (session->closed) {
        TlsTransportSetError_nid_no_patch(context, "connection is closed");
        return -1;
    }
    if (!session->handshaken) {
        const auto result = Handshake(context, *session);
        if (result != 0) return result;
    }
    if (length == 0) return 0;
    GuestArena::HostWrite destination(buffer, length);
    if (!destination.Open()) {
        TlsTransportSetError_nid_no_patch(context, "invalid read buffer");
        return -1;
    }
    session->hostError = 0;
    const int result = mbedtls_ssl_read(&session->ssl, static_cast<unsigned char*>(buffer), length);
    if (result > 0) return result;
    if (result == 0 || result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return 0;
    const auto mapped = session->Map(result);
    if (mapped == WantPollIn || mapped == WantPollOut) return mapped;
    SetError(context, *session, result, "read");
    return -1;
}

std::int64_t TlsTransportWrite_nid_no_patch(TlsContext* context, const void* buffer, std::size_t length) {
    TlsTransportClearError_nid_no_patch(context);
    if (length > static_cast<std::size_t>(INT_MAX) || (length && !buffer)) {
        TlsTransportSetError_nid_no_patch(context, "invalid write buffer");
        return -1;
    }
    const auto session = Find(context);
    if (!session) {
        TlsTransportSetError_nid_no_patch(context, "context not connected");
        return -1;
    }
    std::unique_lock lock(session->mutex);
    if (session->closed) {
        TlsTransportSetError_nid_no_patch(context, "connection is closed");
        return -1;
    }
    if (!session->handshaken) {
        const auto result = Handshake(context, *session);
        if (result != 0) return result;
    }
    if (length == 0) return 0;
    const std::size_t chunk = std::min(length, WriteChunkSize);
    std::vector<unsigned char> input(chunk);
    if (!GuestCopyIn_nid_no_patch(input.data(), buffer, chunk)) {
        TlsTransportSetError_nid_no_patch(context, "invalid write buffer");
        return -1;
    }
    session->hostError = 0;
    const int result = mbedtls_ssl_write(&session->ssl, input.data(), chunk);
    if (result >= 0) return result;
    const auto mapped = session->Map(result);
    if (mapped == WantPollIn || mapped == WantPollOut) return mapped;
    SetError(context, *session, result, "write");
    return -1;
}
