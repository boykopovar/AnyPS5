#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "terminal check failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int Now = 0;
constexpr int Drain = 1;
constexpr int Flush = 2;
constexpr int Soft = 0x10;
constexpr int NotTerminal = 25;
constexpr int BadDescriptor = 9;
constexpr int InvalidArgument = 22;
constexpr int BadAddress = 14;
constexpr int AddressNotAvailable = 49;
constexpr int OperationNotSupported = 45;

static bool Fails(int result, int error) { return result == -1 && *__error_nid_postfix() == error; }

static void RequireControlFails(int descriptor, int error) {
    *__error_nid_postfix() = 0;
    Require(Fails(tcsendbreak_nid_postfix(descriptor, 0), error));
    *__error_nid_postfix() = 0;
    Require(Fails(tcsendbreak_nid_postfix(descriptor, 5), error));
    *__error_nid_postfix() = 0;
    Require(Fails(tcdrain_nid_postfix(descriptor), error));
    for (const int queue : {1, 2, 3}) {
        *__error_nid_postfix() = 0;
        Require(Fails(tcflush_nid_postfix(descriptor, queue), error));
    }
    for (const int action : {1, 2, 3, 4}) {
        *__error_nid_postfix() = 0;
        Require(Fails(tcflow_nid_postfix(descriptor, action), error));
    }
}

static void RequireProcessGroupAndSessionFail(int descriptor, int error) {
    *__error_nid_postfix() = 0;
    Require(Fails(tcgetpgrp_nid_postfix(descriptor), error));
    for (const int group : {0, 1}) {
        *__error_nid_postfix() = 0;
        Require(Fails(tcsetpgrp_nid_postfix(descriptor, group), error));
    }
    *__error_nid_postfix() = 0;
    Require(Fails(tcgetsid_nid_postfix(descriptor), error));
}

template <typename TCall> static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static void RequireNotTerminal(int descriptor) {
    unsigned char attributes[44];
    std::memset(attributes, 0xaa, sizeof(attributes));
    unsigned char untouched[44];
    std::memset(untouched, 0xaa, sizeof(untouched));
    *__error_nid_postfix() = 0;
    Require(Fails(tcgetattr_nid_postfix(descriptor, attributes), NotTerminal));
    Require(std::memcmp(attributes, untouched, sizeof(attributes)) == 0);
    for (const int action : {Now, Drain, Flush, Now | Soft, Flush | Soft}) {
        *__error_nid_postfix() = 0;
        Require(Fails(tcsetattr_nid_postfix(descriptor, action, attributes), NotTerminal));
    }
    RequireControlFails(descriptor, NotTerminal);
}

int main() {
    const auto name = "anyps5-terminal-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const int file = open_nid_postfix(name.c_str(), 0x202, 0644);
    Require(file >= 0);
    RequireNotTerminal(file);
    RequireProcessGroupAndSessionFail(file, NotTerminal);
    for (const int stream : {0, 1, 2}) {
        RequireNotTerminal(stream);
        RequireProcessGroupAndSessionFail(stream, NotTerminal);
    }

    int ends[2] = {-1, -1};
    Require(pipe_nid_postfix(ends) == 0);
    for (const int end : ends) {
        RequireNotTerminal(end);
        Require(Throws([end] { tcgetpgrp_nid_postfix(end); }));
        Require(Throws([end] { tcsetpgrp_nid_postfix(end, 0); }));
        *__error_nid_postfix() = 0;
        Require(Fails(tcgetsid_nid_postfix(end), NotTerminal));
    }

    const int socket = socket_nid_postfix(2, 2, 0);
    Require(socket >= 0);
    unsigned char untouched[44];
    std::memset(untouched, 0xaa, sizeof(untouched));
    unsigned char probe[44];
    std::memset(probe, 0xaa, sizeof(probe));
    Require(Fails(tcgetattr_nid_postfix(socket, probe), AddressNotAvailable));
    Require(std::memcmp(probe, untouched, sizeof(probe)) == 0);
    Require(Fails(tcsetattr_nid_postfix(socket, Flush, probe), AddressNotAvailable));
    RequireControlFails(socket, AddressNotAvailable);
    RequireProcessGroupAndSessionFail(socket, AddressNotAvailable);

    int pair[2] = {-1, -1};
    Require(socketpair_nid_postfix(1, 1, 0, pair) == 0);
    for (const int end : pair) {
        Require(Fails(tcgetattr_nid_postfix(end, probe), OperationNotSupported));
        RequireControlFails(end, OperationNotSupported);
        RequireProcessGroupAndSessionFail(end, OperationNotSupported);
    }
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);

    const auto directoryName = name + "-directory";
    Require(mkdir_nid_postfix(directoryName.c_str(), 0755) == 0);
    const int directory = open_nid_postfix(directoryName.c_str(), 0x00020000, 0);
    Require(directory >= 0);
    RequireNotTerminal(directory);
    RequireProcessGroupAndSessionFail(directory, NotTerminal);
    Require(close_nid_postfix(directory) == 0);
    RequireControlFails(directory, BadDescriptor);
    std::filesystem::remove(directoryName);

    unsigned char attributes[44]{};
    Require(Fails(tcsetattr_nid_postfix(file, 3, attributes), InvalidArgument));
    Require(Fails(tcsetattr_nid_postfix(file, -1, attributes), InvalidArgument));
    Require(Fails(tcsetattr_nid_postfix(file, Soft | 3, attributes), InvalidArgument));
    Require(Fails(tcsetattr_nid_postfix(-1, 3, attributes), InvalidArgument));
    Require(Fails(tcsetattr_nid_postfix(file, Now, nullptr), BadAddress));
    Require(Fails(tcsetattr_nid_postfix(-1, Now, nullptr), BadAddress));
    Require(Fails(tcgetattr_nid_postfix(-1, attributes), BadDescriptor));
    Require(Fails(tcsetattr_nid_postfix(-1, Now, attributes), BadDescriptor));
    for (const int descriptor : {file, -1}) {
        for (const int queue : {0, 4, -1}) {
            *__error_nid_postfix() = 0;
            Require(Fails(tcflush_nid_postfix(descriptor, queue), InvalidArgument));
        }
        for (const int action : {0, 5, -1}) {
            *__error_nid_postfix() = 0;
            Require(Fails(tcflow_nid_postfix(descriptor, action), InvalidArgument));
        }
    }
    RequireControlFails(-1, BadDescriptor);
    RequireProcessGroupAndSessionFail(-1, BadDescriptor);

    Require(close_nid_postfix(socket) == 0);
    Require(Fails(tcgetattr_nid_postfix(socket, attributes), BadDescriptor));
    Require(Fails(tcsetattr_nid_postfix(socket, Now, attributes), BadDescriptor));
    RequireControlFails(socket, BadDescriptor);
    RequireProcessGroupAndSessionFail(socket, BadDescriptor);
    Require(close_nid_postfix(ends[0]) == 0 && close_nid_postfix(ends[1]) == 0);
    RequireControlFails(ends[0], BadDescriptor);
    RequireProcessGroupAndSessionFail(ends[0], BadDescriptor);
    Require(close_nid_postfix(file) == 0);
    Require(Fails(tcgetattr_nid_postfix(file, attributes), BadDescriptor));
    Require(Fails(tcsetattr_nid_postfix(file, Flush, attributes), BadDescriptor));
    RequireControlFails(file, BadDescriptor);
    RequireProcessGroupAndSessionFail(file, BadDescriptor);
    std::filesystem::remove(name);
    return 0;
}
