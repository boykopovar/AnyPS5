#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string_view>

constexpr int Unix = 1;
constexpr int Datagram = 2;
constexpr int SocketLevel = 65535;
constexpr int Rights = 1;
constexpr int SetStatusFlags = 4;
constexpr int Nonblocking = 4;
constexpr int Truncated = 0x10;
constexpr int DontWait = 0x80;
constexpr std::uint32_t ControlLength = 24;
constexpr unsigned char Sentinel = 0xa5;

static void RequireDatagramRights(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

static void CloseDatagramRightsDescriptor(int& descriptor) {
    const int result = close_nid_postfix(descriptor);
    descriptor = -1;
    RequireDatagramRights(result == 0, "descriptor close fails");
}

struct DatagramRightsResources {
    int carrier[2] = {-1, -1};
    int pipe[2] = {-1, -1};
    int writer = -1;

    DatagramRightsResources() = default;
    DatagramRightsResources(const DatagramRightsResources&) = delete;
    DatagramRightsResources& operator=(const DatagramRightsResources&) = delete;

    ~DatagramRightsResources() {
        for (const int descriptor : {writer, pipe[1], pipe[0], carrier[1], carrier[0]}) {
            if (descriptor < 0) continue;
            try {
                close_nid_postfix(descriptor);
            } catch (...) {
            }
        }
    }
};

static void PrepareDatagramRights(DatagramRightsResources& resources) {
    RequireDatagramRights(socketpair_nid_postfix(Unix, Datagram, 0, resources.carrier) == 0,
                          "datagram socketpair creation fails");
    RequireDatagramRights(pipe_nid_postfix(resources.pipe) == 0, "pipe creation fails");
    RequireDatagramRights(fcntl_nid_postfix(resources.carrier[1], SetStatusFlags, Nonblocking) == 0,
                          "carrier cannot become nonblocking");
    RequireDatagramRights(fcntl_nid_postfix(resources.pipe[0], SetStatusFlags, Nonblocking) == 0,
                          "pipe reader cannot become nonblocking");
}

static std::array<unsigned char, ControlLength> DatagramRightsControl(int descriptor) {
    std::array<unsigned char, ControlLength> control{};
    const std::uint32_t recordLength = 20;
    std::memcpy(control.data(), &recordLength, sizeof(recordLength));
    std::memcpy(control.data() + 4, &SocketLevel, sizeof(SocketLevel));
    std::memcpy(control.data() + 8, &Rights, sizeof(Rights));
    std::memcpy(control.data() + 16, &descriptor, sizeof(descriptor));
    return control;
}

static void CheckDatagramRightsControl(DatagramRightsResources& resources, const GuestMsghdr& message,
                                      const std::array<unsigned char, 32>& control, int expectedFlags) {
    RequireDatagramRights(message.controlLength == ControlLength, "received control length is not 24");
    std::memcpy(&resources.writer, control.data() + 16, sizeof(resources.writer));
    RequireDatagramRights(message.flags == expectedFlags, "received flags differ from expected flags");
    std::uint32_t recordLength = 0;
    int level = 0;
    int type = 0;
    std::memcpy(&recordLength, control.data(), sizeof(recordLength));
    std::memcpy(&level, control.data() + 4, sizeof(level));
    std::memcpy(&type, control.data() + 8, sizeof(type));
    RequireDatagramRights(recordLength == 20, "rights record length is not 20");
    RequireDatagramRights(level == SocketLevel, "rights record level is not SOL_SOCKET");
    RequireDatagramRights(type == Rights, "rights record type is not SCM_RIGHTS");
    RequireDatagramRights(resources.writer >= 0, "received writer descriptor is negative");
    RequireDatagramRights(std::all_of(control.begin() + ControlLength, control.end(),
                                     [](unsigned char byte) { return byte == Sentinel; }),
                          "receive writes beyond the control capacity");
}

static void CheckDatagramRightsPipe(DatagramRightsResources& resources, std::string_view payload) {
    RequireDatagramRights(write_nid_postfix(resources.writer, payload.data(), payload.size()) ==
                              static_cast<std::int64_t>(payload.size()),
                          "received writer does not write the exact payload length");
    std::array<char, 5> received{};
    RequireDatagramRights(read_nid_postfix(resources.pipe[0], received.data(), received.size()) ==
                              static_cast<std::int64_t>(payload.size()),
                          "pipe does not return the exact payload length");
    RequireDatagramRights(std::memcmp(received.data(), payload.data(), payload.size()) == 0,
                          "pipe payload differs from the written payload");
    CloseDatagramRightsDescriptor(resources.writer);
    RequireDatagramRights(read_nid_postfix(resources.pipe[0], received.data(), received.size()) == 0,
                          "pipe does not reach EOF after the received writer closes");
}

static void CheckEmptyDatagramRights() {
    DatagramRightsResources resources;
    PrepareDatagramRights(resources);
    auto sentControl = DatagramRightsControl(resources.pipe[1]);
    GuestMsghdr sent{};
    sent.control = sentControl.data();
    sent.controlLength = ControlLength;
    RequireDatagramRights(sendmsg_nid_postfix(resources.carrier[0], &sent, 0) == 0,
                          "empty datagram send does not return zero");
    CloseDatagramRightsDescriptor(resources.pipe[1]);

    std::array<unsigned char, 16> payload;
    payload.fill(Sentinel);
    std::array<unsigned char, 32> control;
    control.fill(Sentinel);
    KernelIovec vector{payload.data(), payload.size()};
    GuestMsghdr received{};
    received.iov = &vector;
    received.iovLength = 1;
    received.control = control.data();
    received.controlLength = ControlLength;
    received.flags = -1;
    RequireDatagramRights(recvmsg_nid_postfix(resources.carrier[1], &received, 0) == 0,
                          "empty datagram receive does not return zero");
    CheckDatagramRightsControl(resources, received, control, 0);
    RequireDatagramRights(std::all_of(payload.begin(), payload.end(),
                                     [](unsigned char byte) { return byte == Sentinel; }),
                          "empty datagram receive changes the payload sentinel");
    CheckDatagramRightsPipe(resources, "empty");
}

static void CheckZeroVectorDatagramRights() {
    DatagramRightsResources resources;
    PrepareDatagramRights(resources);
    auto sentControl = DatagramRightsControl(resources.pipe[1]);
    std::array<char, 3> payload{'c', 'u', 't'};
    KernelIovec vector{payload.data(), payload.size()};
    GuestMsghdr sent{};
    sent.iov = &vector;
    sent.iovLength = 1;
    sent.control = sentControl.data();
    sent.controlLength = ControlLength;
    RequireDatagramRights(sendmsg_nid_postfix(resources.carrier[0], &sent, 0) == 3,
                          "payload datagram send does not return three");
    CloseDatagramRightsDescriptor(resources.pipe[1]);

    std::array<unsigned char, 32> control;
    control.fill(Sentinel);
    GuestMsghdr received{};
    received.control = control.data();
    received.controlLength = ControlLength;
    received.flags = -1;
    RequireDatagramRights(recvmsg_nid_postfix(resources.carrier[1], &received, 0) == 0,
                          "zero-vector receive does not return zero");
    CheckDatagramRightsControl(resources, received, control, Truncated);
    CheckDatagramRightsPipe(resources, "zero");

    std::array<char, 16> remaining{};
    KernelIovec remainingVector{remaining.data(), remaining.size()};
    GuestMsghdr consumed{};
    consumed.iov = &remainingVector;
    consumed.iovLength = 1;
    *__error_nid_postfix() = 0;
    RequireDatagramRights(recvmsg_nid_postfix(resources.carrier[1], &consumed, DontWait) == -1,
                          "zero-vector receive does not consume the datagram");
    RequireDatagramRights(*__error_nid_postfix() == 35, "empty carrier does not report guest EAGAIN");
}

int main() {
    const char* test = "empty datagram rights";
    try {
        CheckEmptyDatagramRights();
        test = "zero-vector datagram rights";
        CheckZeroVectorDatagramRights();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: %s\n", test, error.what());
        return 1;
    }
}
