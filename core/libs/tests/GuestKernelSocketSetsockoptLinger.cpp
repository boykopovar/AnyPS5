#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int SocketLevel = 0xffff;
constexpr int SoLinger = 0x80;
constexpr int StreamSocket = 1;
constexpr int InternetFamily = 2;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorDomain = 33;
constexpr int ErrorNoProtocolOption = 42;

struct Linger {
    std::int32_t enabled;
    std::int32_t seconds;
};

struct ExtendedLinger {
    Linger linger;
    std::int32_t padding[2];
};

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "assertion failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(condition) Check((condition), __LINE__)

bool SetFails(int descriptor, int level, int option, const void* value, std::uint32_t length, int expectedError) {
    return setsockopt_nid_postfix(descriptor, level, option, value, length) == -1 && *__error_nid_postfix() == expectedError;
}

bool GetFails(int descriptor, int level, int option, void* value, std::uint32_t* length, int expectedError) {
    return getsockopt_nid_postfix(descriptor, level, option, value, length) == -1 && *__error_nid_postfix() == expectedError;
}

void AuditZeroTimeout(int stream) {
    Linger linger{1, 0};
    Require(setsockopt_nid_postfix(stream, SocketLevel, SoLinger, &linger, sizeof(linger)) == 0);
    Linger read{};
    std::uint32_t size = sizeof(read);
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &read, &size) == 0 && size == sizeof(read));
    Require(read.enabled == SoLinger && read.seconds == 0);

    linger = {0, 0};
    Require(setsockopt_nid_postfix(stream, SocketLevel, SoLinger, &linger, sizeof(linger)) == 0);
    size = sizeof(read);
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &read, &size) == 0 && size == sizeof(read));
    Require(read.enabled == 0);
}

void AuditPositiveTimeout(int stream) {
    Linger linger{1, 1};
    Require(setsockopt_nid_postfix(stream, SocketLevel, SoLinger, &linger, sizeof(linger)) == 0);
    Linger read{};
    std::uint32_t size = sizeof(read);
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &read, &size) == 0 && size == sizeof(read));
    Require(read.enabled == SoLinger && read.seconds == 1);

    linger = {1, 5};
    Require(setsockopt_nid_postfix(stream, SocketLevel, SoLinger, &linger, sizeof(linger)) == 0);
    size = sizeof(read);
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &read, &size) == 0 && size == sizeof(read));
    Require(read.enabled == SoLinger && read.seconds == 5);

    linger = {1, 120};
    Require(setsockopt_nid_postfix(stream, SocketLevel, SoLinger, &linger, sizeof(linger)) == 0);
    size = sizeof(read);
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &read, &size) == 0 && size == sizeof(read));
    Require(read.enabled == SoLinger && read.seconds == 120);

    linger = {1, 65535};
    Require(setsockopt_nid_postfix(stream, SocketLevel, SoLinger, &linger, sizeof(linger)) == 0);
    size = sizeof(read);
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &read, &size) == 0 && size == sizeof(read));
    Require(read.enabled == SoLinger && read.seconds == 65535);

    linger = {2, 10};
    Require(setsockopt_nid_postfix(stream, SocketLevel, SoLinger, &linger, sizeof(linger)) == 0);
    size = sizeof(read);
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &read, &size) == 0 && size == sizeof(read));
    Require(read.enabled == SoLinger && read.seconds == 10);

    linger = {SoLinger, 20};
    Require(setsockopt_nid_postfix(stream, SocketLevel, SoLinger, &linger, sizeof(linger)) == 0);
    size = sizeof(read);
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &read, &size) == 0 && size == sizeof(read));
    Require(read.enabled == SoLinger && read.seconds == 20);

    linger = {255, 30};
    Require(setsockopt_nid_postfix(stream, SocketLevel, SoLinger, &linger, sizeof(linger)) == 0);
    size = sizeof(read);
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &read, &size) == 0 && size == sizeof(read));
    Require(read.enabled == SoLinger && read.seconds == 30);

    linger = {-1, 40};
    Require(setsockopt_nid_postfix(stream, SocketLevel, SoLinger, &linger, sizeof(linger)) == 0);
    size = sizeof(read);
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &read, &size) == 0 && size == sizeof(read));
    Require(read.enabled == SoLinger && read.seconds == 40);
}

void AuditNegativeTimeoutBounds(int stream) {
    Linger linger{1, -1};
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, sizeof(linger), ErrorDomain));

    linger = {1, -2};
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, sizeof(linger), ErrorDomain));

    linger = {1, -32768};
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, sizeof(linger), ErrorDomain));

    linger = {1, -2147483647 - 1};
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, sizeof(linger), ErrorDomain));

    linger = {0, -1};
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, sizeof(linger), ErrorDomain));

    linger = {0, -100};
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, sizeof(linger), ErrorDomain));

    linger = {1, 65536};
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, sizeof(linger), ErrorDomain));

    linger = {1, 100000};
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, sizeof(linger), ErrorDomain));

    linger = {1, 2147483647};
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, sizeof(linger), ErrorDomain));

    linger = {0, 65536};
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, sizeof(linger), ErrorDomain));

    Linger read{};
    std::uint32_t size = sizeof(read);
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &read, &size) == 0 && size == sizeof(read));
    Require(read.enabled == SoLinger && read.seconds == 40);
}

void AuditInvalidOptlen(int stream) {
    Linger linger{1, 15};
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, 0, ErrorInvalid));
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, 1, ErrorInvalid));
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, 2, ErrorInvalid));
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, 3, ErrorInvalid));
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, 4, ErrorInvalid));
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, 7, ErrorInvalid));

    Require(SetFails(stream, SocketLevel, SoLinger, &linger, 0x80000000u, ErrorInvalid));
    Require(SetFails(stream, SocketLevel, SoLinger, &linger, 0xffffffffu, ErrorInvalid));

    Linger read{};
    std::uint32_t size = sizeof(read);
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &read, &size) == 0 && size == sizeof(read));
    Require(read.enabled == SoLinger && read.seconds == 40);

    ExtendedLinger extended{{1, 50}, {111, 222}};
    Require(setsockopt_nid_postfix(stream, SocketLevel, SoLinger, &extended, sizeof(extended)) == 0);
    size = sizeof(read);
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &read, &size) == 0 && size == sizeof(read));
    Require(read.enabled == SoLinger && read.seconds == 50);

    std::array<std::uint8_t, 12> buffer{};
    Linger validLinger{1, 25};
    std::memcpy(buffer.data(), &validLinger, sizeof(validLinger));
    Require(setsockopt_nid_postfix(stream, SocketLevel, SoLinger, buffer.data(), buffer.size()) == 0);
    size = sizeof(read);
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &read, &size) == 0 && size == sizeof(read));
    Require(read.enabled == SoLinger && read.seconds == 25);
}

void AuditPrecedenceAndOptval(int stream) {
    Require(SetFails(stream, SocketLevel, SoLinger, nullptr, sizeof(Linger), ErrorFault));
    Require(SetFails(-1, SocketLevel, SoLinger, nullptr, sizeof(Linger), ErrorBadDescriptor));
    Require(SetFails(9999, SocketLevel, SoLinger, nullptr, sizeof(Linger), ErrorBadDescriptor));

    Linger linger{1, 10};
    Require(SetFails(-1, SocketLevel, SoLinger, &linger, sizeof(linger), ErrorBadDescriptor));
    Require(SetFails(9999, SocketLevel, SoLinger, &linger, sizeof(linger), ErrorBadDescriptor));
    Require(SetFails(stream, 9999, SoLinger, &linger, sizeof(linger), ErrorInvalid));
    Require(SetFails(stream, SocketLevel, 0x7777, &linger, sizeof(linger), ErrorNoProtocolOption));

    std::int32_t partial = -1;
    std::uint32_t size = sizeof(partial);
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &partial, &size) == 0 && size == sizeof(partial));
    Require(partial == SoLinger);

    size = 0;
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &partial, &size) == 0 && size == 0);

    std::array<std::uint8_t, 32> largeBuffer{};
    size = largeBuffer.size();
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, largeBuffer.data(), &size) == 0 && size == sizeof(Linger));
    Linger read{};
    std::memcpy(&read, largeBuffer.data(), sizeof(read));
    Require(read.enabled == SoLinger && read.seconds == 25);

    Require(GetFails(stream, SocketLevel, SoLinger, &read, nullptr, ErrorFault));
}

}

int main() {
    const int stream = socket_nid_postfix(InternetFamily, StreamSocket, 0);
    Require(stream >= 0);

    Linger initial{};
    std::uint32_t size = sizeof(initial);
    Require(getsockopt_nid_postfix(stream, SocketLevel, SoLinger, &initial, &size) == 0 && size == sizeof(initial));
    Require(initial.enabled == 0);

    AuditZeroTimeout(stream);
    AuditPositiveTimeout(stream);
    AuditNegativeTimeoutBounds(stream);
    AuditInvalidOptlen(stream);
    AuditPrecedenceAndOptval(stream);

    Require(close_nid_postfix(stream) == 0);
    return 0;
}
