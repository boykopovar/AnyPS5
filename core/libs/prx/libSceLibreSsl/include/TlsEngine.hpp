#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_TLSENGINE_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_TLSENGINE_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace LibreSslTls {

enum class Result { Done, WantRead, WantWrite, ZeroReturn, Syscall, Ssl };

inline constexpr int SslErrorNone = 0;
inline constexpr int SslErrorSsl = 1;
inline constexpr int SslErrorWantRead = 2;
inline constexpr int SslErrorWantWrite = 3;
inline constexpr int SslErrorSyscall = 5;
inline constexpr int SslErrorZeroReturn = 6;

inline constexpr int SslVerifyNone = 0x00;
inline constexpr int SslVerifyPeer = 0x01;
inline constexpr int SslVerifyFailIfNoPeerCert = 0x02;
inline constexpr int SslVerifyClientOnce = 0x04;

enum class Failure { None, VerifyFailed, BadWriteRetry, NoSharedCipher, Other };

enum class Direction { None, Read, Write };

enum class ChainShape { None, SelfSignedLeaf, EndsInSelfSigned, Incomplete };

struct WriteOptions {
    std::size_t fragment = 16384;
    bool partial = false;
    bool movingBuffer = false;
};

int ToSslError(Result result);

class Connection {
public:
    explicit Connection(bool server);
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(Connection&&) = delete;

    void SetSocket(int guestFd);
    void DetachSocket();
    int Socket() const;
    void SetHostname(const char* name);
    void SetServerName(const char* name);
    void SetVerifyMode(int sslVerifyMode);
    void TrustCertificates(const unsigned char* pem, std::size_t size);
    void UseCertificate(const unsigned char* certificatePem, std::size_t certificateSize, const unsigned char* keyPem, std::size_t keySize);

    Result Handshake();
    Result Read(void* buffer, std::size_t size, std::size_t& transferred, bool peek);
    Result Write(const void* buffer, std::size_t size, std::size_t& transferred, const WriteOptions& options = {});
    Result Shutdown(bool& complete);
    std::size_t Pending() const;

    bool HandshakeDone() const;
    bool SentShutdown() const;
    bool ReceivedShutdown() const;
    bool FatalAlertReceived() const;
    std::uint64_t BytesReceived() const;
    std::uint64_t BytesSent() const;
    int SystemError() const;
    Direction LastDirection() const;
    Failure FailureKind() const;
    int FailureCode() const;
    std::string FailureText() const;
    ChainShape PeerChain() const;
    const std::string& PeerServerName() const;
    std::uint32_t VerifyFlags() const;
    const char* Version() const;
    const char* Cipher() const;

private:
    struct State;
    std::unique_ptr<State> state;
};

}

#endif
