#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

struct Iovec {
    void* base;
    std::uint64_t length;
};

struct Msghdr {
    void* name;
    std::uint32_t nameLength;
    Iovec* iov;
    int iovLength;
    void* control;
    std::uint32_t controlLength;
    int flags;
};

extern "C" {
int APS5_VABI socketpair_nid_postfix(int, int, int, int*);
int APS5_VABI pipe_nid_postfix(int*);
std::int64_t APS5_VABI sendmsg_nid_postfix(int, const Msghdr*, int);
std::int64_t APS5_VABI recvmsg_nid_postfix(int, Msghdr*, int);
std::int64_t APS5_VABI send_nid_postfix(int, const void*, std::uint64_t, int);
std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int);
std::int64_t APS5_VABI read_nid_postfix(int, void*, std::uint64_t);
std::int64_t APS5_VABI write_nid_postfix(int, const char*, std::int64_t);
int APS5_VABI fcntl_nid_postfix(int, int, ...);
int APS5_VABI close_nid_postfix(int);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value) { if (!value) std::abort(); }

constexpr int Unix = 1;
constexpr int Stream = 1;
constexpr int Datagram = 2;
constexpr int SocketLevel = 0xffff;
constexpr int Rights = 1;
constexpr int ControlTruncated = 0x20;
constexpr int ControlCloseOnExec = 0x40000;
constexpr int GetDescriptorFlags = 1;
constexpr std::size_t Header = 16;

static std::size_t Space(std::size_t bytes) { return Header + ((bytes + 7u) & ~std::size_t{7}); }

static std::array<unsigned char, 64> RightsControl(std::initializer_list<int> descriptors, std::size_t& length) {
    std::array<unsigned char, 64> control{};
    const auto bytes = descriptors.size() * sizeof(int);
    const auto recordLength = static_cast<std::uint32_t>(Header + bytes);
    std::memcpy(control.data(), &recordLength, sizeof(recordLength));
    std::memcpy(control.data() + 4, &SocketLevel, sizeof(SocketLevel));
    std::memcpy(control.data() + 8, &Rights, sizeof(Rights));
    std::size_t at = Header;
    for (const int descriptor : descriptors) {
        std::memcpy(control.data() + at, &descriptor, sizeof(descriptor));
        at += sizeof(descriptor);
    }
    length = Space(bytes);
    return control;
}

static std::int64_t SendWith(int socket, const char* text, unsigned char* control, std::size_t controlLength) {
    Iovec vector{const_cast<char*>(text), std::strlen(text)};
    Msghdr message{};
    message.iov = &vector;
    message.iovLength = 1;
    message.control = control;
    message.controlLength = static_cast<std::uint32_t>(controlLength);
    return sendmsg_nid_postfix(socket, &message, 0);
}

struct Received {
    std::int64_t bytes = -1;
    std::array<char, 16> data{};
    std::array<unsigned char, 64> control{};
    std::uint32_t controlLength = 0;
    int flags = 0;
};

static Received ReceiveWith(int socket, std::size_t controlCapacity, int flags = 0) {
    Received result;
    Iovec vector{result.data.data(), result.data.size()};
    Msghdr message{};
    message.iov = &vector;
    message.iovLength = 1;
    message.control = controlCapacity != 0 ? result.control.data() : nullptr;
    message.controlLength = static_cast<std::uint32_t>(controlCapacity);
    result.bytes = recvmsg_nid_postfix(socket, &message, flags);
    result.controlLength = message.controlLength;
    result.flags = message.flags;
    return result;
}

static void RequireRecord(const Received& received, std::size_t count) {
    std::uint32_t length = 0;
    int level = 0;
    int type = 0;
    std::memcpy(&length, received.control.data(), sizeof(length));
    std::memcpy(&level, received.control.data() + 4, sizeof(level));
    std::memcpy(&type, received.control.data() + 8, sizeof(type));
    Require(length == Header + count * sizeof(int) && level == SocketLevel && type == Rights);
}

static int ReceivedDescriptor(const Received& received, std::size_t index) {
    int descriptor = -1;
    std::memcpy(&descriptor, received.control.data() + Header + index * sizeof(int), sizeof(descriptor));
    return descriptor;
}

static void CheckPassedPipe(const int (&pair)[2]) {
    int pipe[2] = {-1, -1};
    Require(pipe_nid_postfix(pipe) == 0);
    std::size_t length = 0;
    auto control = RightsControl({pipe[1]}, length);
    Require(SendWith(pair[0], "pipe", control.data(), length) == 4);
    Require(close_nid_postfix(pipe[1]) == 0);
    const auto received = ReceiveWith(pair[1], Space(sizeof(int)));
    Require(received.bytes == 4 && std::memcmp(received.data.data(), "pipe", 4) == 0);
    Require(received.flags == 0 && received.controlLength == Space(sizeof(int)));
    RequireRecord(received, 1);
    const int writer = ReceivedDescriptor(received, 0);
    Require(writer >= 0 && writer != pipe[0]);
    Require(fcntl_nid_postfix(writer, GetDescriptorFlags) == 0);
    Require(write_nid_postfix(writer, "moved", 5) == 5);
    char text[8] = {};
    Require(read_nid_postfix(pipe[0], text, sizeof(text)) == 5 && std::memcmp(text, "moved", 5) == 0);
    Require(close_nid_postfix(writer) == 0);
    Require(read_nid_postfix(pipe[0], text, sizeof(text)) == 0);
    Require(close_nid_postfix(pipe[0]) == 0);
}

static void CheckPassedSockets(const int (&pair)[2]) {
    int first[2] = {-1, -1};
    int second[2] = {-1, -1};
    Require(socketpair_nid_postfix(Unix, Stream, 0, first) == 0);
    Require(socketpair_nid_postfix(Unix, Datagram, 0, second) == 0);
    std::size_t length = 0;
    auto control = RightsControl({first[0], second[0]}, length);
    Require(SendWith(pair[0], "two", control.data(), length) == 3);
    Require(close_nid_postfix(first[0]) == 0 && close_nid_postfix(second[0]) == 0);
    const auto received = ReceiveWith(pair[1], Space(2 * sizeof(int)), ControlCloseOnExec);
    Require(received.bytes == 3 && received.flags == 0 && received.controlLength == Space(2 * sizeof(int)));
    RequireRecord(received, 2);
    const int stream = ReceivedDescriptor(received, 0);
    const int datagram = ReceivedDescriptor(received, 1);
    Require(fcntl_nid_postfix(stream, GetDescriptorFlags) == 1 && fcntl_nid_postfix(datagram, GetDescriptorFlags) == 1);
    char text[8] = {};
    Require(send_nid_postfix(stream, "over", 4, 0) == 4);
    Require(recv_nid_postfix(first[1], text, sizeof(text), 0) == 4 && std::memcmp(text, "over", 4) == 0);
    Require(send_nid_postfix(second[1], "back", 4, 0) == 4);
    Require(recv_nid_postfix(datagram, text, sizeof(text), 0) == 4 && std::memcmp(text, "back", 4) == 0);
    Require(close_nid_postfix(stream) == 0 && close_nid_postfix(datagram) == 0);
    Require(close_nid_postfix(first[1]) == 0 && close_nid_postfix(second[1]) == 0);
}

static void CheckTruncatedControl(const int (&pair)[2]) {
    int spare[2] = {-1, -1};
    Require(pipe_nid_postfix(spare) == 0);
    std::size_t length = 0;
    auto control = RightsControl({spare[0]}, length);
    Require(SendWith(pair[0], "cut", control.data(), length) == 3);
    const auto received = ReceiveWith(pair[1], Header);
    Require(received.bytes == 3 && (received.flags & ControlTruncated) != 0 && received.controlLength == Header);
    Require(SendWith(pair[0], "none", control.data(), length) == 4);
    const auto plain = ReceiveWith(pair[1], 0);
    Require(plain.bytes == 4 && plain.flags == 0 && plain.controlLength == 0);
    Require(close_nid_postfix(spare[0]) == 0 && close_nid_postfix(spare[1]) == 0);
}

static void CheckRefusedControl(const int (&pair)[2]) {
    int closed[2] = {-1, -1};
    Require(pipe_nid_postfix(closed) == 0);
    Require(close_nid_postfix(closed[0]) == 0 && close_nid_postfix(closed[1]) == 0);
    std::size_t length = 0;
    auto control = RightsControl({closed[0]}, length);
    Require(SendWith(pair[0], "bad", control.data(), length) == -1 && *__error_nid_postfix() == 9);
    int level = 6;
    std::memcpy(control.data() + 4, &level, sizeof(level));
    Require(SendWith(pair[0], "bad", control.data(), length) == -1 && *__error_nid_postfix() == 22);
    auto oversized = RightsControl({0}, length);
    const std::uint32_t tooLong = 64;
    std::memcpy(oversized.data(), &tooLong, sizeof(tooLong));
    Require(SendWith(pair[0], "bad", oversized.data(), length) == -1 && *__error_nid_postfix() == 22);
}

int main() {
    int pair[2] = {-1, -1};
    Require(socketpair_nid_postfix(Unix, Stream, 0, pair) == 0);
    CheckPassedPipe(pair);
    CheckPassedSockets(pair);
    CheckTruncatedControl(pair);
    CheckRefusedControl(pair);
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
}
