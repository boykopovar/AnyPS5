#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int GuestAfInet = 2;
constexpr int GuestAfInet6 = 28;
constexpr int GuestSockStream = 1;
constexpr int GuestSockDgram = 2;

constexpr int SocketLevel = 0xffff;
constexpr int OptionLinger = 0x80;
constexpr int OptionSendTimeout = 0x1005;
constexpr int OptionReceiveTimeout = 0x1006;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorDomain = 33;
constexpr int ErrorNotSocket = 38;

struct GuestLinger {
    std::int32_t enabled;
    std::int32_t seconds;
};

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void AuditLinger(int descriptor) {
    GuestLinger lingerValue{};
    std::uint32_t length = sizeof(lingerValue);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &lingerValue, &length) == 0);
    Require(length == sizeof(lingerValue));
    Require(lingerValue.enabled == 0);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &lingerValue, 0) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &lingerValue, 4) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &lingerValue, 7) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    lingerValue.enabled = 1;
    lingerValue.seconds = -1;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &lingerValue, sizeof(lingerValue)) == -1);
    Require(*__error_nid_postfix() == ErrorDomain);

    lingerValue.seconds = -100;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &lingerValue, sizeof(lingerValue)) == -1);
    Require(*__error_nid_postfix() == ErrorDomain);

    lingerValue.seconds = 65536;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &lingerValue, sizeof(lingerValue)) == -1);
    Require(*__error_nid_postfix() == ErrorDomain);

    lingerValue.seconds = 100000;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &lingerValue, sizeof(lingerValue)) == -1);
    Require(*__error_nid_postfix() == ErrorDomain);

    lingerValue.enabled = 1;
    lingerValue.seconds = 5;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &lingerValue, sizeof(lingerValue)) == 0);

    GuestLinger readback{};
    length = sizeof(readback);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &readback, &length) == 0);
    Require(length == sizeof(readback));
    Require(readback.enabled == OptionLinger);
    Require(readback.seconds == 5);

    lingerValue.enabled = -1;
    lingerValue.seconds = 12;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &lingerValue, sizeof(lingerValue)) == 0);

    readback = {};
    length = sizeof(readback);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &readback, &length) == 0);
    Require(length == sizeof(readback));
    Require(readback.enabled == OptionLinger);
    Require(readback.seconds == 12);

    lingerValue.enabled = 1;
    lingerValue.seconds = 65535;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &lingerValue, sizeof(lingerValue)) == 0);

    readback = {};
    length = sizeof(readback);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &readback, &length) == 0);
    Require(length == sizeof(readback));
    Require(readback.enabled == OptionLinger);
    Require(readback.seconds == 65535);

    lingerValue.enabled = 0;
    lingerValue.seconds = 0;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &lingerValue, sizeof(lingerValue)) == 0);

    readback = {};
    length = sizeof(readback);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &readback, &length) == 0);
    Require(length == sizeof(readback));
    Require(readback.enabled == 0);

    lingerValue.enabled = 1;
    lingerValue.seconds = 10;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &lingerValue, sizeof(lingerValue)) == 0);

    readback = {};
    length = 0;
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &readback, &length) == 0);
    Require(length == 0);
    Require(readback.enabled == 0 && readback.seconds == 0);

    std::uint32_t truncatedEnabled = 0;
    length = sizeof(truncatedEnabled);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &truncatedEnabled, &length) == 0);
    Require(length == sizeof(truncatedEnabled));
    Require(truncatedEnabled == OptionLinger);

    std::array<std::uint8_t, 32> largeBuffer{};
    length = largeBuffer.size();
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, largeBuffer.data(), &length) == 0);
    Require(length == sizeof(GuestLinger));

    lingerValue = {0, 0};
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, OptionLinger, &lingerValue, sizeof(lingerValue)) == 0);
}

void AuditTimeouts(int descriptor, int option) {
    GuestTimeval timeval{};
    std::uint32_t length = sizeof(timeval);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(descriptor, SocketLevel, option, &timeval, &length) == 0);
    Require(length == sizeof(timeval));
    Require(timeval.seconds == 0 && timeval.microseconds == 0);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, option, &timeval, 0) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, option, &timeval, 8) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, option, &timeval, 15) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    timeval.seconds = -1;
    timeval.microseconds = 0;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, option, &timeval, sizeof(timeval)) == -1);
    Require(*__error_nid_postfix() == ErrorDomain);

    timeval.seconds = 0;
    timeval.microseconds = -1;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, option, &timeval, sizeof(timeval)) == -1);
    Require(*__error_nid_postfix() == ErrorDomain);

    timeval.seconds = 0;
    timeval.microseconds = 1000000;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, option, &timeval, sizeof(timeval)) == -1);
    Require(*__error_nid_postfix() == ErrorDomain);

    timeval.seconds = 0;
    timeval.microseconds = 2500000;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, option, &timeval, sizeof(timeval)) == -1);
    Require(*__error_nid_postfix() == ErrorDomain);

    timeval.seconds = 3;
    timeval.microseconds = 0;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, option, &timeval, sizeof(timeval)) == 0);

    GuestTimeval readback{};
    length = sizeof(readback);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(descriptor, SocketLevel, option, &readback, &length) == 0);
    Require(length == sizeof(readback));
    Require(readback.seconds == 3);
    Require(readback.microseconds == 0);

    timeval.seconds = 0;
    timeval.microseconds = 500000;
    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, option, &timeval, sizeof(timeval)) == 0);

    readback = {};
    length = sizeof(readback);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(descriptor, SocketLevel, option, &readback, &length) == 0);
    Require(length == sizeof(readback));
    Require(readback.seconds == 0);
    Require(readback.microseconds >= 499000 && readback.microseconds <= 501000);

    readback = {};
    length = 0;
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(descriptor, SocketLevel, option, &readback, &length) == 0);
    Require(length == 0);
    Require(readback.seconds == 0 && readback.microseconds == 0);

    std::int64_t truncatedSeconds = 0;
    length = sizeof(truncatedSeconds);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(descriptor, SocketLevel, option, &truncatedSeconds, &length) == 0);
    Require(length == sizeof(truncatedSeconds));
    Require(truncatedSeconds == 0);

    std::array<std::uint8_t, 32> largeBuffer{};
    length = largeBuffer.size();
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(descriptor, SocketLevel, option, largeBuffer.data(), &length) == 0);
    Require(length == sizeof(GuestTimeval));

    timeval = {0, 0};
    Require(setsockopt_nid_postfix(descriptor, SocketLevel, option, &timeval, sizeof(timeval)) == 0);
}

} // namespace

int main() {
    GuestLinger lingerParam{1, 5};
    GuestTimeval timeParam{2, 0};
    std::uint32_t optLength = sizeof(lingerParam);

    const int v4Stream = socket_nid_postfix(GuestAfInet, GuestSockStream, 0);
    Require(v4Stream >= 0);

    int pipeDescriptors[2]{-1, -1};
    Require(pipe_nid_postfix(pipeDescriptors) == 0);

    const int closedDescriptor = socket_nid_postfix(GuestAfInet, GuestSockStream, 0);
    Require(closedDescriptor >= 0);
    Require(close_nid_postfix(closedDescriptor) == 0);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(-1, SocketLevel, OptionLinger, nullptr, 8) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(9999, SocketLevel, OptionLinger, nullptr, 8) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(closedDescriptor, SocketLevel, OptionLinger, nullptr, 8) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(pipeDescriptors[0], SocketLevel, OptionLinger, nullptr, 8) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Stream, SocketLevel, OptionLinger, nullptr, 8) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(-1, SocketLevel, OptionLinger, &lingerParam, static_cast<std::uint32_t>(-1)) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(9999, SocketLevel, OptionLinger, &lingerParam, static_cast<std::uint32_t>(-1)) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(closedDescriptor, SocketLevel, OptionLinger, &lingerParam, static_cast<std::uint32_t>(-1)) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(pipeDescriptors[0], SocketLevel, OptionLinger, &lingerParam, static_cast<std::uint32_t>(-1)) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Stream, SocketLevel, OptionLinger, &lingerParam, static_cast<std::uint32_t>(-1)) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(-1, SocketLevel, OptionLinger, &lingerParam, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(9999, SocketLevel, OptionLinger, &lingerParam, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(closedDescriptor, SocketLevel, OptionLinger, &lingerParam, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(pipeDescriptors[0], SocketLevel, OptionLinger, &lingerParam, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(v4Stream, SocketLevel, OptionLinger, &lingerParam, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(-1, SocketLevel, OptionLinger, &lingerParam, sizeof(lingerParam)) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(-100, SocketLevel, OptionLinger, &lingerParam, sizeof(lingerParam)) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(9999, SocketLevel, OptionLinger, &lingerParam, sizeof(lingerParam)) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(closedDescriptor, SocketLevel, OptionLinger, &lingerParam, sizeof(lingerParam)) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(pipeDescriptors[0], SocketLevel, OptionLinger, &lingerParam, sizeof(lingerParam)) == -1);
    Require(*__error_nid_postfix() == ErrorNotSocket);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(pipeDescriptors[1], SocketLevel, OptionLinger, &lingerParam, sizeof(lingerParam)) == -1);
    Require(*__error_nid_postfix() == ErrorNotSocket);

    optLength = sizeof(lingerParam);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(-1, SocketLevel, OptionLinger, &lingerParam, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    optLength = sizeof(lingerParam);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(-100, SocketLevel, OptionLinger, &lingerParam, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    optLength = sizeof(lingerParam);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(9999, SocketLevel, OptionLinger, &lingerParam, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    optLength = sizeof(lingerParam);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(closedDescriptor, SocketLevel, OptionLinger, &lingerParam, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    optLength = sizeof(lingerParam);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(pipeDescriptors[0], SocketLevel, OptionLinger, &lingerParam, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorNotSocket);

    optLength = sizeof(lingerParam);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(pipeDescriptors[1], SocketLevel, OptionLinger, &lingerParam, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorNotSocket);

    Require(close_nid_postfix(pipeDescriptors[0]) == 0);
    Require(close_nid_postfix(pipeDescriptors[1]) == 0);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Stream, 1234, OptionLinger, &lingerParam, sizeof(lingerParam)) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    optLength = sizeof(lingerParam);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(v4Stream, 1234, OptionLinger, &lingerParam, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    AuditLinger(v4Stream);
    AuditTimeouts(v4Stream, OptionSendTimeout);
    AuditTimeouts(v4Stream, OptionReceiveTimeout);

    const int v4Dgram = socket_nid_postfix(GuestAfInet, GuestSockDgram, 0);
    Require(v4Dgram >= 0);
    AuditLinger(v4Dgram);
    AuditTimeouts(v4Dgram, OptionSendTimeout);
    AuditTimeouts(v4Dgram, OptionReceiveTimeout);

    const int v6Stream = socket_nid_postfix(GuestAfInet6, GuestSockStream, 0);
    Require(v6Stream >= 0);
    AuditLinger(v6Stream);
    AuditTimeouts(v6Stream, OptionSendTimeout);
    AuditTimeouts(v6Stream, OptionReceiveTimeout);

    const int v6Dgram = socket_nid_postfix(GuestAfInet6, GuestSockDgram, 0);
    Require(v6Dgram >= 0);
    AuditLinger(v6Dgram);
    AuditTimeouts(v6Dgram, OptionSendTimeout);
    AuditTimeouts(v6Dgram, OptionReceiveTimeout);

    *__error_nid_postfix() = 0;
    Require(_setsockopt_nid_postfix(-1, SocketLevel, OptionLinger, nullptr, 8) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    optLength = sizeof(lingerParam);
    *__error_nid_postfix() = 0;
    Require(_getsockopt_nid_postfix(-1, SocketLevel, OptionLinger, &lingerParam, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    GuestLinger twinLinger{1, 7};
    Require(_setsockopt_nid_postfix(v4Stream, SocketLevel, OptionLinger, &twinLinger, sizeof(twinLinger)) == 0);
    GuestLinger twinReadback{};
    optLength = sizeof(twinReadback);
    Require(_getsockopt_nid_postfix(v4Stream, SocketLevel, OptionLinger, &twinReadback, &optLength) == 0);
    Require(optLength == sizeof(twinReadback));
    Require(twinReadback.enabled == OptionLinger);
    Require(twinReadback.seconds == 7);

    GuestTimeval twinTime{5, 0};
    Require(_setsockopt_nid_postfix(v4Stream, SocketLevel, OptionSendTimeout, &twinTime, sizeof(twinTime)) == 0);
    GuestTimeval twinTimeReadback{};
    optLength = sizeof(twinTimeReadback);
    Require(_getsockopt_nid_postfix(v4Stream, SocketLevel, OptionSendTimeout, &twinTimeReadback, &optLength) == 0);
    Require(optLength == sizeof(twinTimeReadback));
    Require(twinTimeReadback.seconds == 5);
    Require(twinTimeReadback.microseconds == 0);

    Require(close_nid_postfix(v4Stream) == 0);
    Require(close_nid_postfix(v4Dgram) == 0);
    Require(close_nid_postfix(v6Stream) == 0);
    Require(close_nid_postfix(v6Dgram) == 0);

    *__error_nid_postfix() = 0;
    Require(setsockopt_nid_postfix(v4Stream, SocketLevel, OptionLinger, &lingerParam, sizeof(lingerParam)) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    optLength = sizeof(lingerParam);
    *__error_nid_postfix() = 0;
    Require(getsockopt_nid_postfix(v4Stream, SocketLevel, OptionLinger, &lingerParam, &optLength) == -1);
    Require(*__error_nid_postfix() == ErrorBadDescriptor);

    return 0;
}
