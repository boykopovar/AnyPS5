#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int AfUnix = 1;
constexpr int AfInet = 2;
constexpr int AfInet6 = 28;

constexpr int SockStream = 1;
constexpr int SockDgram = 2;

constexpr int PosixEbadf = 9;
constexpr int PosixEfault = 14;
constexpr int PosixEnotsock = 38;
constexpr int PosixEnotconn = 57;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

std::array<std::uint8_t, 16> MakeLoopbackAddress() {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    return address;
}

void TestErrorPrecedenceAndDescriptorValidation() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    const int sock = socket_nid_postfix(AfInet, SockStream, 0);
    Require(sock >= 0);

    std::array<std::uint8_t, 32> buffer{};
    std::uint32_t len = buffer.size();

    Require(getsockname_nid_postfix(-1, buffer.data(), nullptr) == -1 && *__error_nid_postfix() == PosixEfault);
    Require(_getsockname_nid_postfix(-1, buffer.data(), nullptr) == -1 && *__error_nid_postfix() == PosixEfault);
    Require(getpeername_nid_postfix(-1, buffer.data(), nullptr) == -1 && *__error_nid_postfix() == PosixEfault);
    Require(_getpeername_nid_postfix(-1, buffer.data(), nullptr) == -1 && *__error_nid_postfix() == PosixEfault);

    Require(getsockname_nid_postfix(sock, buffer.data(), nullptr) == -1 && *__error_nid_postfix() == PosixEfault);
    Require(_getsockname_nid_postfix(sock, buffer.data(), nullptr) == -1 && *__error_nid_postfix() == PosixEfault);
    Require(getpeername_nid_postfix(sock, buffer.data(), nullptr) == -1 && *__error_nid_postfix() == PosixEfault);
    Require(_getpeername_nid_postfix(sock, buffer.data(), nullptr) == -1 && *__error_nid_postfix() == PosixEfault);

    Require(getsockname_nid_postfix(pipeFds[0], buffer.data(), nullptr) == -1 && *__error_nid_postfix() == PosixEfault);
    Require(_getsockname_nid_postfix(pipeFds[0], buffer.data(), nullptr) == -1 && *__error_nid_postfix() == PosixEfault);
    Require(getpeername_nid_postfix(pipeFds[0], buffer.data(), nullptr) == -1 && *__error_nid_postfix() == PosixEfault);
    Require(_getpeername_nid_postfix(pipeFds[0], buffer.data(), nullptr) == -1 && *__error_nid_postfix() == PosixEfault);

    len = buffer.size();
    Require(getsockname_nid_postfix(-1, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEbadf);
    Require(_getsockname_nid_postfix(-1, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEbadf);
    Require(getpeername_nid_postfix(-1, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEbadf);
    Require(_getpeername_nid_postfix(-1, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEbadf);

    len = buffer.size();
    Require(getsockname_nid_postfix(99999, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEbadf);
    Require(_getsockname_nid_postfix(99999, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEbadf);
    Require(getpeername_nid_postfix(99999, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEbadf);
    Require(_getpeername_nid_postfix(99999, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEbadf);

    len = buffer.size();
    Require(getsockname_nid_postfix(pipeFds[0], buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEnotsock);
    Require(_getsockname_nid_postfix(pipeFds[0], buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEnotsock);
    Require(getpeername_nid_postfix(pipeFds[0], buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEnotsock);
    Require(_getpeername_nid_postfix(pipeFds[0], buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEnotsock);

    len = buffer.size();
    Require(getsockname_nid_postfix(pipeFds[1], buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEnotsock);
    Require(_getsockname_nid_postfix(pipeFds[1], buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEnotsock);
    Require(getpeername_nid_postfix(pipeFds[1], buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEnotsock);
    Require(_getpeername_nid_postfix(pipeFds[1], buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEnotsock);

    len = 16;
    Require(getsockname_nid_postfix(sock, nullptr, &len) == -1 && *__error_nid_postfix() == PosixEfault);
    Require(_getsockname_nid_postfix(sock, nullptr, &len) == -1 && *__error_nid_postfix() == PosixEfault);

    len = 0;
    Require(getsockname_nid_postfix(sock, nullptr, &len) == 0 && len == 0);
    len = 0;
    Require(_getsockname_nid_postfix(sock, nullptr, &len) == 0 && len == 0);

    Require(close_nid_postfix(sock) == 0);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);

    len = buffer.size();
    Require(getsockname_nid_postfix(sock, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEbadf);
    Require(_getsockname_nid_postfix(sock, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEbadf);
    Require(getpeername_nid_postfix(sock, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEbadf);
    Require(_getpeername_nid_postfix(sock, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEbadf);
}

void TestGetsocknameTruncationIPv4() {
    const int sock = socket_nid_postfix(AfInet, SockStream, 0);
    Require(sock >= 0);

    std::array<std::uint8_t, 64> fullAddress{};
    std::uint32_t fullLen = fullAddress.size();
    Require(getsockname_nid_postfix(sock, fullAddress.data(), &fullLen) == 0);
    Require(fullLen == 16);
    Require(fullAddress[0] == 16);
    Require(fullAddress[1] == AfInet);

    std::array<std::uint8_t, 64> buffer{};
    std::uint32_t len = 0;
    for (std::uint32_t capacity = 0; capacity <= 48; ++capacity) {
        buffer.fill(0xa5);
        len = capacity;

        Require(getsockname_nid_postfix(sock, buffer.data(), &len) == 0);
        const std::uint32_t expectedLen = capacity < 16 ? capacity : 16;
        Require(len == expectedLen);

        for (std::uint32_t i = 0; i < expectedLen; ++i) {
            Require(buffer[i] == fullAddress[i]);
        }
        for (std::uint32_t i = expectedLen; i < buffer.size(); ++i) {
            Require(buffer[i] == 0xa5);
        }
    }

    for (std::uint32_t capacity = 0; capacity <= 32; ++capacity) {
        buffer.fill(0x5a);
        len = capacity;

        Require(_getsockname_nid_postfix(sock, buffer.data(), &len) == 0);
        const std::uint32_t expectedLen = capacity < 16 ? capacity : 16;
        Require(len == expectedLen);

        for (std::uint32_t i = 0; i < expectedLen; ++i) {
            Require(buffer[i] == fullAddress[i]);
        }
        for (std::uint32_t i = expectedLen; i < buffer.size(); ++i) {
            Require(buffer[i] == 0x5a);
        }
    }

    for (const std::uint32_t largeCapacity : {64u, 128u, 256u, 1024u}) {
        buffer.fill(0x77);
        len = largeCapacity;
        Require(getsockname_nid_postfix(sock, buffer.data(), &len) == 0);
        Require(len == 16);
        for (std::uint32_t i = 0; i < 16; ++i) {
            Require(buffer[i] == fullAddress[i]);
        }
        for (std::uint32_t i = 16; i < buffer.size(); ++i) {
            Require(buffer[i] == 0x77);
        }
    }

    len = 0;
    buffer.fill(0x99);
    Require(getsockname_nid_postfix(sock, buffer.data(), &len) == 0);
    Require(len == 0);
    for (const std::uint8_t byte : buffer) {
        Require(byte == 0x99);
    }

    len = 0;
    Require(getsockname_nid_postfix(sock, nullptr, &len) == 0);
    Require(len == 0);

    len = 16;
    Require(getsockname_nid_postfix(sock, nullptr, &len) == -1 && *__error_nid_postfix() == PosixEfault);

    Require(close_nid_postfix(sock) == 0);
}

void TestGetsocknameTruncationIPv6() {
    const int sock = socket_nid_postfix(AfInet6, SockStream, 0);
    if (sock < 0) {
        return;
    }

    std::array<std::uint8_t, 64> fullAddress{};
    std::uint32_t fullLen = fullAddress.size();
    Require(getsockname_nid_postfix(sock, fullAddress.data(), &fullLen) == 0);
    Require(fullLen == 28);
    Require(fullAddress[0] == 28);
    Require(fullAddress[1] == AfInet6);

    std::array<std::uint8_t, 64> buffer{};
    std::uint32_t len = 0;
    for (std::uint32_t capacity = 0; capacity <= 48; ++capacity) {
        buffer.fill(0x3c);
        len = capacity;

        Require(getsockname_nid_postfix(sock, buffer.data(), &len) == 0);
        const std::uint32_t expectedLen = capacity < 28 ? capacity : 28;
        Require(len == expectedLen);

        for (std::uint32_t i = 0; i < expectedLen; ++i) {
            Require(buffer[i] == fullAddress[i]);
        }
        for (std::uint32_t i = expectedLen; i < buffer.size(); ++i) {
            Require(buffer[i] == 0x3c);
        }
    }

    for (std::uint32_t capacity = 0; capacity <= 32; ++capacity) {
        buffer.fill(0xc3);
        len = capacity;

        Require(_getsockname_nid_postfix(sock, buffer.data(), &len) == 0);
        const std::uint32_t expectedLen = capacity < 28 ? capacity : 28;
        Require(len == expectedLen);

        for (std::uint32_t i = 0; i < expectedLen; ++i) {
            Require(buffer[i] == fullAddress[i]);
        }
        for (std::uint32_t i = expectedLen; i < buffer.size(); ++i) {
            Require(buffer[i] == 0xc3);
        }
    }

    for (const std::uint32_t largeCapacity : {64u, 128u, 256u, 1024u}) {
        buffer.fill(0x44);
        len = largeCapacity;
        Require(getsockname_nid_postfix(sock, buffer.data(), &len) == 0);
        Require(len == 28);
        for (std::uint32_t i = 0; i < 28; ++i) {
            Require(buffer[i] == fullAddress[i]);
        }
        for (std::uint32_t i = 28; i < buffer.size(); ++i) {
            Require(buffer[i] == 0x44);
        }
    }

    len = 0;
    Require(getsockname_nid_postfix(sock, nullptr, &len) == 0);
    Require(len == 0);

    len = 28;
    Require(getsockname_nid_postfix(sock, nullptr, &len) == -1 && *__error_nid_postfix() == PosixEfault);

    Require(close_nid_postfix(sock) == 0);
}

void TestGetpeernameTruncationAndConnectionValidation() {
    const int listener = socket_nid_postfix(AfInet, SockStream, 0);
    Require(listener >= 0);

    auto address = MakeLoopbackAddress();
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    Require(listen_nid_postfix(listener, 1) == 0);

    std::uint32_t addressSize = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &addressSize) == 0);
    Require(addressSize == 16);
    Require(address[0] == 16);
    Require(address[1] == AfInet);

    const int client = socket_nid_postfix(AfInet, SockStream, 0);
    Require(client >= 0);

    std::array<std::uint8_t, 32> buffer{};
    std::uint32_t len = buffer.size();

    Require(getpeername_nid_postfix(client, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEnotconn);
    Require(_getpeername_nid_postfix(client, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEnotconn);
    Require(getpeername_nid_postfix(listener, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEnotconn);
    Require(_getpeername_nid_postfix(listener, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEnotconn);

    const int dgram = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(dgram >= 0);
    len = buffer.size();
    Require(getpeername_nid_postfix(dgram, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEnotconn);
    Require(_getpeername_nid_postfix(dgram, buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEnotconn);
    Require(close_nid_postfix(dgram) == 0);

    Require(connect_nid_postfix(client, address.data(), address.size()) == 0);

    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0);

    std::array<std::uint8_t, 32> clientPeerAddr{};
    std::uint32_t clientPeerLen = clientPeerAddr.size();
    Require(getpeername_nid_postfix(client, clientPeerAddr.data(), &clientPeerLen) == 0);
    Require(clientPeerLen == 16);
    Require(clientPeerAddr[0] == 16);
    Require(clientPeerAddr[1] == AfInet);

    for (std::uint32_t capacity = 0; capacity <= 32; ++capacity) {
        buffer.fill(0x6a);
        len = capacity;

        Require(getpeername_nid_postfix(client, buffer.data(), &len) == 0);
        const std::uint32_t expectedLen = capacity < 16 ? capacity : 16;
        Require(len == expectedLen);

        for (std::uint32_t i = 0; i < expectedLen; ++i) {
            Require(buffer[i] == clientPeerAddr[i]);
        }
        for (std::uint32_t i = expectedLen; i < buffer.size(); ++i) {
            Require(buffer[i] == 0x6a);
        }
    }

    for (std::uint32_t capacity = 0; capacity <= 32; ++capacity) {
        buffer.fill(0xa6);
        len = capacity;

        Require(_getpeername_nid_postfix(client, buffer.data(), &len) == 0);
        const std::uint32_t expectedLen = capacity < 16 ? capacity : 16;
        Require(len == expectedLen);

        for (std::uint32_t i = 0; i < expectedLen; ++i) {
            Require(buffer[i] == clientPeerAddr[i]);
        }
        for (std::uint32_t i = expectedLen; i < buffer.size(); ++i) {
            Require(buffer[i] == 0xa6);
        }
    }

    std::array<std::uint8_t, 32> serverPeerAddr{};
    std::uint32_t serverPeerLen = serverPeerAddr.size();
    Require(getpeername_nid_postfix(server, serverPeerAddr.data(), &serverPeerLen) == 0);
    Require(serverPeerLen == 16);
    Require(serverPeerAddr[0] == 16);
    Require(serverPeerAddr[1] == AfInet);

    for (std::uint32_t capacity = 0; capacity <= 32; ++capacity) {
        buffer.fill(0xb4);
        len = capacity;

        Require(getpeername_nid_postfix(server, buffer.data(), &len) == 0);
        const std::uint32_t expectedLen = capacity < 16 ? capacity : 16;
        Require(len == expectedLen);

        for (std::uint32_t i = 0; i < expectedLen; ++i) {
            Require(buffer[i] == serverPeerAddr[i]);
        }
        for (std::uint32_t i = expectedLen; i < buffer.size(); ++i) {
            Require(buffer[i] == 0xb4);
        }
    }

    for (std::uint32_t capacity = 0; capacity <= 32; ++capacity) {
        buffer.fill(0x4b);
        len = capacity;

        Require(_getpeername_nid_postfix(server, buffer.data(), &len) == 0);
        const std::uint32_t expectedLen = capacity < 16 ? capacity : 16;
        Require(len == expectedLen);

        for (std::uint32_t i = 0; i < expectedLen; ++i) {
            Require(buffer[i] == serverPeerAddr[i]);
        }
        for (std::uint32_t i = expectedLen; i < buffer.size(); ++i) {
            Require(buffer[i] == 0x4b);
        }
    }

    for (const std::uint32_t largeCapacity : {64u, 128u, 256u, 1024u}) {
        buffer.fill(0x88);
        len = largeCapacity;
        Require(getpeername_nid_postfix(client, buffer.data(), &len) == 0);
        Require(len == 16);
        for (std::uint32_t i = 0; i < 16; ++i) {
            Require(buffer[i] == clientPeerAddr[i]);
        }
        for (std::uint32_t i = 16; i < buffer.size(); ++i) {
            Require(buffer[i] == 0x88);
        }
    }

    len = 0;
    Require(getpeername_nid_postfix(client, nullptr, &len) == 0);
    Require(len == 0);

    len = 16;
    Require(getpeername_nid_postfix(client, nullptr, &len) == -1 && *__error_nid_postfix() == PosixEfault);

    Require(close_nid_postfix(client) == 0);
    Require(close_nid_postfix(server) == 0);
    Require(close_nid_postfix(listener) == 0);
}

void TestUnixDomainSocketTruncationAndPeerExpiry() {
    int pair[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);

    std::array<std::uint8_t, 32> buffer{};

    for (const int fd : {pair[0], pair[1]}) {
        for (std::uint32_t capacity = 0; capacity <= 32; ++capacity) {
            buffer.fill(0x2d);
            std::uint32_t len = capacity;

            Require(getsockname_nid_postfix(fd, buffer.data(), &len) == 0);
            const std::uint32_t expectedLen = capacity < 16 ? capacity : 16;
            Require(len == expectedLen);

            if (expectedLen > 0) {
                Require(buffer[0] == 16);
            }
            if (expectedLen > 1) {
                Require(buffer[1] == AfUnix);
            }
            for (std::uint32_t i = 2; i < expectedLen; ++i) {
                Require(buffer[i] == 0);
            }
            for (std::uint32_t i = expectedLen; i < buffer.size(); ++i) {
                Require(buffer[i] == 0x2d);
            }
        }
    }

    for (const int fd : {pair[0], pair[1]}) {
        for (std::uint32_t capacity = 0; capacity <= 32; ++capacity) {
            buffer.fill(0x52);
            std::uint32_t len = capacity;

            Require(getpeername_nid_postfix(fd, buffer.data(), &len) == 0);
            const std::uint32_t expectedLen = capacity < 16 ? capacity : 16;
            Require(len == expectedLen);

            if (expectedLen > 0) {
                Require(buffer[0] == 16);
            }
            if (expectedLen > 1) {
                Require(buffer[1] == AfUnix);
            }
            for (std::uint32_t i = 2; i < expectedLen; ++i) {
                Require(buffer[i] == 0);
            }
            for (std::uint32_t i = expectedLen; i < buffer.size(); ++i) {
                Require(buffer[i] == 0x52);
            }
        }
    }

    Require(close_nid_postfix(pair[1]) == 0);

    std::uint32_t len = buffer.size();
    Require(getpeername_nid_postfix(pair[0], buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEnotconn);
    Require(_getpeername_nid_postfix(pair[0], buffer.data(), &len) == -1 && *__error_nid_postfix() == PosixEnotconn);

    len = buffer.size();
    Require(getsockname_nid_postfix(pair[0], buffer.data(), &len) == 0);
    Require(len == 16);
    Require(buffer[0] == 16);
    Require(buffer[1] == AfUnix);

    Require(close_nid_postfix(pair[0]) == 0);

    int dgramPair[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockDgram, 0, dgramPair) == 0);

    len = 32;
    buffer.fill(0);
    Require(getsockname_nid_postfix(dgramPair[0], buffer.data(), &len) == 0);
    Require(len == 16);
    Require(buffer[0] == 16);
    Require(buffer[1] == AfUnix);

    len = 32;
    buffer.fill(0);
    Require(getpeername_nid_postfix(dgramPair[0], buffer.data(), &len) == 0);
    Require(len == 16);
    Require(buffer[0] == 16);
    Require(buffer[1] == AfUnix);

    Require(close_nid_postfix(dgramPair[0]) == 0);
    Require(close_nid_postfix(dgramPair[1]) == 0);
}

void TestUninitializedFamilyAndWildcardFields() {
    const int stream = socket_nid_postfix(AfInet, SockStream, 0);
    const int dgram = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(stream >= 0 && dgram >= 0);

    std::array<std::uint8_t, 32> buffer{};
    std::uint32_t len = buffer.size();

    Require(getsockname_nid_postfix(stream, buffer.data(), &len) == 0);
    Require(len == 16);
    Require(buffer[0] == 16);
    Require(buffer[1] == AfInet);
    for (std::size_t i = 2; i < 16; ++i) {
        Require(buffer[i] == 0);
    }

    len = buffer.size();
    Require(getsockname_nid_postfix(dgram, buffer.data(), &len) == 0);
    Require(len == 16);
    Require(buffer[0] == 16);
    Require(buffer[1] == AfInet);
    for (std::size_t i = 2; i < 16; ++i) {
        Require(buffer[i] == 0);
    }

    Require(close_nid_postfix(stream) == 0);
    Require(close_nid_postfix(dgram) == 0);

    const int v6Stream = socket_nid_postfix(AfInet6, SockStream, 0);
    const int v6Dgram = socket_nid_postfix(AfInet6, SockDgram, 0);
    if (v6Stream >= 0) {
        len = buffer.size();
        Require(getsockname_nid_postfix(v6Stream, buffer.data(), &len) == 0);
        Require(len == 28);
        Require(buffer[0] == 28);
        Require(buffer[1] == AfInet6);
        for (std::size_t i = 2; i < 28; ++i) {
            Require(buffer[i] == 0);
        }
        Require(close_nid_postfix(v6Stream) == 0);
    }

    if (v6Dgram >= 0) {
        len = buffer.size();
        Require(getsockname_nid_postfix(v6Dgram, buffer.data(), &len) == 0);
        Require(len == 28);
        Require(buffer[0] == 28);
        Require(buffer[1] == AfInet6);
        for (std::size_t i = 2; i < 28; ++i) {
            Require(buffer[i] == 0);
        }
        Require(close_nid_postfix(v6Dgram) == 0);
    }
}

} // namespace

int main() {
    TestErrorPrecedenceAndDescriptorValidation();
    TestGetsocknameTruncationIPv4();
    TestGetsocknameTruncationIPv6();
    TestGetpeernameTruncationAndConnectionValidation();
    TestUnixDomainSocketTruncationAndPeerExpiry();
    TestUninitializedFamilyAndWildcardFields();
    return 0;
}
