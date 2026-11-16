#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorNotSocket = 38;
constexpr int ErrorNoProtocolOption = 42;

constexpr int SolSocket = 0xffff;
constexpr int SoAcceptConn = 0x02;
constexpr int SoSndBuf = 0x1001;
constexpr int SoRcvBuf = 0x1002;
constexpr int SoError = 0x1007;
constexpr int SoType = 0x1008;

constexpr int FamilyInet = 2;
constexpr int FamilyInet6 = 28;
constexpr int FamilyUnix = 1;
constexpr int SocketStream = 1;
constexpr int SocketDatagram = 2;
#if !defined(_WIN32) && !defined(__APPLE__)
constexpr int SocketSeqpacket = 5;
#endif

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

bool Fails(int result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

void TestBufferDefaultsAndQueries() {
    const int stream4 = socket_nid_postfix(FamilyInet, SocketStream, 0);
    const int dgram4 = socket_nid_postfix(FamilyInet, SocketDatagram, 0);
    const int stream6 = socket_nid_postfix(FamilyInet6, SocketStream, 0);
    const int dgram6 = socket_nid_postfix(FamilyInet6, SocketDatagram, 0);
    Require(stream4 >= 0 && dgram4 >= 0 && stream6 >= 0 && dgram6 >= 0);

    int svStream[2]{-1, -1};
    Require(socketpair_nid_postfix(FamilyUnix, SocketStream, 0, svStream) == 0);
    int svDgram[2]{-1, -1};
    Require(socketpair_nid_postfix(FamilyUnix, SocketDatagram, 0, svDgram) == 0);
#if !defined(_WIN32) && !defined(__APPLE__)
    int svSeq[2]{-1, -1};
    Require(socketpair_nid_postfix(FamilyUnix, SocketSeqpacket, 0, svSeq) == 0);
#endif

    struct BufferExpectation {
        int fd;
        int expectedSnd;
        int expectedRcv;
        int expectedType;
    };

    const BufferExpectation expectations[] = {
        {stream4, 32768, 65536, SocketStream},
        {dgram4, 9216, 42080, SocketDatagram},
        {stream6, 32768, 65536, SocketStream},
        {dgram6, 9216, 42080, SocketDatagram},
        {svStream[0], 8192, 8192, SocketStream},
        {svStream[1], 8192, 8192, SocketStream},
        {svDgram[0], 2048, 4096, SocketDatagram},
        {svDgram[1], 2048, 4096, SocketDatagram},
#if !defined(_WIN32) && !defined(__APPLE__)
        {svSeq[0], 8192, 8192, SocketSeqpacket},
        {svSeq[1], 8192, 8192, SocketSeqpacket}
#endif
    };

    for (const auto& exp : expectations) {
        int snd = 0;
        std::uint32_t len = sizeof(snd);
        Require(getsockopt_nid_postfix(exp.fd, SolSocket, SoSndBuf, &snd, &len) == 0 && len == 4 && snd == exp.expectedSnd);

        int rcv = 0;
        len = sizeof(rcv);
        Require(getsockopt_nid_postfix(exp.fd, SolSocket, SoRcvBuf, &rcv, &len) == 0 && len == 4 && rcv == exp.expectedRcv);

        int typeVal = 0;
        len = sizeof(typeVal);
        Require(getsockopt_nid_postfix(exp.fd, SolSocket, SoType, &typeVal, &len) == 0 && len == 4 && typeVal == exp.expectedType);

        int altType = 0;
        len = sizeof(altType);
        Require(_getsockopt_nid_postfix(exp.fd, SolSocket, SoType, &altType, &len) == 0 && len == 4 && altType == exp.expectedType);
    }

    Require(close_nid_postfix(stream4) == 0);
    Require(close_nid_postfix(dgram4) == 0);
    Require(close_nid_postfix(stream6) == 0);
    Require(close_nid_postfix(dgram6) == 0);
    Require(close_nid_postfix(svStream[0]) == 0 && close_nid_postfix(svStream[1]) == 0);
    Require(close_nid_postfix(svDgram[0]) == 0 && close_nid_postfix(svDgram[1]) == 0);
#if !defined(_WIN32) && !defined(__APPLE__)
    Require(close_nid_postfix(svSeq[0]) == 0 && close_nid_postfix(svSeq[1]) == 0);
#endif
}

void TestReadOnlyOptionRejection() {
    const int stream = socket_nid_postfix(FamilyInet, SocketStream, 0);
    const int dgram = socket_nid_postfix(FamilyInet, SocketDatagram, 0);
    Require(stream >= 0 && dgram >= 0);

    const int readOnlyOptions[] = {SoType, SoError, SoAcceptConn};
    const int dummyVal = 1;

    for (const int opt : readOnlyOptions) {
        Require(Fails(setsockopt_nid_postfix(stream, SolSocket, opt, &dummyVal, sizeof(dummyVal)), ErrorNoProtocolOption));
        Require(Fails(setsockopt_nid_postfix(dgram, SolSocket, opt, &dummyVal, sizeof(dummyVal)), ErrorNoProtocolOption));
        Require(Fails(_setsockopt_nid_postfix(stream, SolSocket, opt, &dummyVal, sizeof(dummyVal)), ErrorNoProtocolOption));
        Require(Fails(_setsockopt_nid_postfix(dgram, SolSocket, opt, &dummyVal, sizeof(dummyVal)), ErrorNoProtocolOption));

        Require(Fails(setsockopt_nid_postfix(stream, SolSocket, opt, nullptr, sizeof(dummyVal)), ErrorFault));
        Require(Fails(setsockopt_nid_postfix(stream, SolSocket, opt, &dummyVal, static_cast<std::uint32_t>(-1)), ErrorInvalid));
        Require(Fails(setsockopt_nid_postfix(-1, SolSocket, opt, &dummyVal, sizeof(dummyVal)), ErrorBadDescriptor));
    }

    int typeVal = 0;
    std::uint32_t len = sizeof(typeVal);
    Require(getsockopt_nid_postfix(stream, SolSocket, SoType, &typeVal, &len) == 0 && typeVal == SocketStream);
    len = sizeof(typeVal);
    Require(getsockopt_nid_postfix(dgram, SolSocket, SoType, &typeVal, &len) == 0 && typeVal == SocketDatagram);

    Require(close_nid_postfix(stream) == 0);
    Require(close_nid_postfix(dgram) == 0);
}

void TestDescriptorValidationAndPrecedence() {
    const int val = 4096;
    for (const int opt : {SoSndBuf, SoRcvBuf, SoType}) {
        Require(Fails(setsockopt_nid_postfix(-1, SolSocket, opt, nullptr, 4), ErrorFault));
        Require(Fails(setsockopt_nid_postfix(-1, SolSocket, opt, &val, static_cast<std::uint32_t>(-1)), ErrorInvalid));
        Require(Fails(setsockopt_nid_postfix(-1, SolSocket, opt, &val, 0x80000000u), ErrorInvalid));

        Require(Fails(setsockopt_nid_postfix(-1, SolSocket, opt, &val, sizeof(val)), ErrorBadDescriptor));
        Require(Fails(setsockopt_nid_postfix(-100, SolSocket, opt, &val, sizeof(val)), ErrorBadDescriptor));
        Require(Fails(setsockopt_nid_postfix(1024, SolSocket, opt, &val, sizeof(val)), ErrorBadDescriptor));

        int out = 0;
        Require(Fails(getsockopt_nid_postfix(-1, SolSocket, opt, &out, nullptr), ErrorFault));
        std::uint32_t len = sizeof(out);
        Require(Fails(getsockopt_nid_postfix(-1, SolSocket, opt, &out, &len), ErrorBadDescriptor));
        Require(Fails(getsockopt_nid_postfix(-100, SolSocket, opt, &out, &len), ErrorBadDescriptor));
        Require(Fails(getsockopt_nid_postfix(1024, SolSocket, opt, &out, &len), ErrorBadDescriptor));
    }

    const int closedSocket = socket_nid_postfix(FamilyInet, SocketStream, 0);
    Require(closedSocket >= 0);
    Require(close_nid_postfix(closedSocket) == 0);

    for (const int opt : {SoSndBuf, SoRcvBuf, SoType}) {
        Require(Fails(setsockopt_nid_postfix(closedSocket, SolSocket, opt, &val, sizeof(val)), ErrorBadDescriptor));
        int out = 0;
        std::uint32_t len = sizeof(out);
        Require(Fails(getsockopt_nid_postfix(closedSocket, SolSocket, opt, &out, &len), ErrorBadDescriptor));
    }

    int pipefds[2]{-1, -1};
    Require(pipe_nid_postfix(pipefds) == 0);
    Require(pipefds[0] >= 0 && pipefds[1] >= 0);

    for (const int pipefd : pipefds) {
        for (const int opt : {SoSndBuf, SoRcvBuf, SoType}) {
            Require(Fails(setsockopt_nid_postfix(pipefd, SolSocket, opt, &val, sizeof(val)), ErrorNotSocket));
            int out = 0;
            std::uint32_t len = sizeof(out);
            Require(Fails(getsockopt_nid_postfix(pipefd, SolSocket, opt, &out, &len), ErrorNotSocket));
        }
    }

    Require(close_nid_postfix(pipefds[0]) == 0);
    Require(close_nid_postfix(pipefds[1]) == 0);

    for (const int pipefd : pipefds) {
        for (const int opt : {SoSndBuf, SoRcvBuf, SoType}) {
            Require(Fails(setsockopt_nid_postfix(pipefd, SolSocket, opt, &val, sizeof(val)), ErrorBadDescriptor));
            int out = 0;
            std::uint32_t len = sizeof(out);
            Require(Fails(getsockopt_nid_postfix(pipefd, SolSocket, opt, &out, &len), ErrorBadDescriptor));
        }
    }

    const int liveSocket = socket_nid_postfix(FamilyInet, SocketStream, 0);
    Require(liveSocket >= 0);

    for (const int badLevel : {-1, 1, 2, 3, 5, 7, 40, 42, 99999}) {
        Require(Fails(setsockopt_nid_postfix(liveSocket, badLevel, SoSndBuf, &val, sizeof(val)), ErrorInvalid));
        int out = 0;
        std::uint32_t len = sizeof(out);
        Require(Fails(getsockopt_nid_postfix(liveSocket, badLevel, SoSndBuf, &out, &len), ErrorInvalid));
    }

    std::uint32_t len0 = 0;
    int canary = 0x12345678;
    Require(getsockopt_nid_postfix(liveSocket, SolSocket, SoType, &canary, &len0) == 0 && len0 == 0 && canary == 0x12345678);

    std::uint32_t len1 = 1;
    std::uint8_t byteCanary = 0xaa;
    Require(getsockopt_nid_postfix(liveSocket, SolSocket, SoType, &byteCanary, &len1) == 0 && len1 == 1 && byteCanary == 0x01);

    std::uint32_t len2 = 2;
    std::uint16_t wordCanary = 0xaaaa;
    Require(getsockopt_nid_postfix(liveSocket, SolSocket, SoType, &wordCanary, &len2) == 0 && len2 == 2 && wordCanary == 0x0001);

    std::uint32_t len8 = 8;
    int fullOutput = 0;
    Require(getsockopt_nid_postfix(liveSocket, SolSocket, SoType, &fullOutput, &len8) == 0 && len8 == 4 && fullOutput == SocketStream);

    std::uint32_t lenNull = 4;
    Require(getsockopt_nid_postfix(liveSocket, SolSocket, SoType, nullptr, &lenNull) == 0 && lenNull == 0);

    Require(close_nid_postfix(liveSocket) == 0);
}

} // namespace

int main() {
    TestBufferDefaultsAndQueries();
    TestReadOnlyOptionRejection();
    TestDescriptorValidationAndPrecedence();
    return 0;
}
