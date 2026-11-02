#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <initializer_list>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

static void Require(bool value) { if (!value) std::abort(); }

constexpr int Unix = 1;
constexpr int Stream = 1;
constexpr int Datagram = 2;
constexpr int SocketLevel = 0xffff;
constexpr int Rights = 1;
constexpr int ControlTruncated = 0x20;
constexpr int ControlCloseOnExec = 0x40000;
constexpr int Peek = 0x2;
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
    KernelIovec vector{const_cast<char*>(text), std::strlen(text)};
    GuestMsghdr message{};
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
    KernelIovec vector{result.data.data(), result.data.size()};
    GuestMsghdr message{};
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

    int closing[2] = {-1, -1};
    Require(pipe_nid_postfix(closing) == 0);
    control = RightsControl({closing[1]}, length);
    Require(SendWith(pair[0], "exec", control.data(), length) == 4);
    Require(close_nid_postfix(closing[1]) == 0);
    const auto marked = ReceiveWith(pair[1], Space(sizeof(int)), ControlCloseOnExec);
    Require(marked.bytes == 4 && marked.controlLength == Space(sizeof(int)));
    RequireRecord(marked, 1);
    const int markedWriter = ReceivedDescriptor(marked, 0);
    Require(fcntl_nid_postfix(markedWriter, GetDescriptorFlags) == 1);
    Require(close_nid_postfix(markedWriter) == 0 && close_nid_postfix(closing[0]) == 0);
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
    Require(received.bytes == 3 && received.flags == ControlCloseOnExec && received.controlLength == Space(2 * sizeof(int)));
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

static std::size_t OpenHostDescriptors() {
    std::size_t open = 0;
    for (int descriptor = 0; descriptor < 4096; ++descriptor) {
        if (::fcntl(descriptor, F_GETFD) != -1) ++open;
    }
    return open;
}

static void CheckTruncatedControl(const int (&pair)[2]) {
    int spare[2] = {-1, -1};
    Require(pipe_nid_postfix(spare) == 0);
    std::size_t length = 0;
    auto control = RightsControl({spare[0]}, length);
    const auto before = OpenHostDescriptors();
    Require(SendWith(pair[0], "cut", control.data(), length) == 3);
    const auto received = ReceiveWith(pair[1], Header);
    Require(received.bytes == 3 && (received.flags & ControlTruncated) != 0 && received.controlLength == 0);
    Require(OpenHostDescriptors() == before);
    std::size_t twoLength = 0;
    auto two = RightsControl({spare[0], spare[1]}, twoLength);
    Require(SendWith(pair[0], "two", two.data(), twoLength) == 3);
    const auto oneRoom = ReceiveWith(pair[1], Header + sizeof(int));
    Require(oneRoom.bytes == 3 && (oneRoom.flags & ControlTruncated) != 0 && oneRoom.controlLength == 0);
    Require(OpenHostDescriptors() == before);
    Require(SendWith(pair[0], "none", control.data(), length) == 4);
    const auto plain = ReceiveWith(pair[1], 0);
    Require(plain.bytes == 4 && plain.flags == 0 && plain.controlLength == 0);
    Require(close_nid_postfix(spare[0]) == 0 && close_nid_postfix(spare[1]) == 0);
}

static void CheckFaultedReceive(const int (&pair)[2]) {
    int spare[2] = {-1, -1};
    Require(pipe_nid_postfix(spare) == 0);
    std::size_t length = 0;
    auto control = RightsControl({spare[0], spare[1]}, length);
    void* page = ::mmap(nullptr, 4096, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
    const auto before = OpenHostDescriptors();
    Require(SendWith(pair[0], "ro", control.data(), length) == 2);
    std::array<unsigned char, 64> received{};
    KernelIovec vector{page, 16};
    GuestMsghdr message{};
    message.iov = &vector;
    message.iovLength = 1;
    message.control = received.data();
    message.controlLength = static_cast<std::uint32_t>(received.size());
    Require(recvmsg_nid_postfix(pair[1], &message, 0) == -1 && *__error_nid_postfix() == 14);
    Require(OpenHostDescriptors() == before);
    Require(::munmap(page, 4096) == 0);
    Require(close_nid_postfix(spare[0]) == 0 && close_nid_postfix(spare[1]) == 0);
}

static void CheckFaultedHeader(const int (&pair)[2]) {
    int spare[2] = {-1, -1};
    Require(pipe_nid_postfix(spare) == 0);
    std::size_t length = 0;
    auto control = RightsControl({spare[0], spare[1]}, length);
    auto* page = static_cast<unsigned char*>(::mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(page != MAP_FAILED);
    std::array<char, 16> data{};
    std::array<unsigned char, 64> received{};
    auto* vector = reinterpret_cast<KernelIovec*>(page + 64);
    *vector = KernelIovec{data.data(), data.size()};
    auto* message = reinterpret_cast<GuestMsghdr*>(page);
    *message = GuestMsghdr{};
    message->iov = vector;
    message->iovLength = 1;
    message->control = received.data();
    message->controlLength = static_cast<std::uint32_t>(received.size());
    Require(::mprotect(page, 4096, PROT_READ) == 0);
    const auto before = OpenHostDescriptors();
    Require(SendWith(pair[0], "hdr", control.data(), length) == 3);
    Require(recvmsg_nid_postfix(pair[1], message, 0) == -1 && *__error_nid_postfix() == 14);
    Require(std::memcmp(data.data(), "hdr", 3) == 0);
    Require(OpenHostDescriptors() == before);
    Require(::munmap(page, 4096) == 0);
    Require(close_nid_postfix(spare[0]) == 0 && close_nid_postfix(spare[1]) == 0);
}

static void CheckFailedInstallation(const int (&pair)[2], bool routeFirst) {
    int local[2] = {-1, -1};
    Require(socketpair_nid_postfix(Unix, Stream, 0, local) == 0);
    const int route = ::socket(AF_ROUTE, SOCK_RAW, 0);
    Require(route >= 0);
    const auto before = OpenHostDescriptors();
    std::size_t length = 0;
    auto control = routeFirst ? RightsControl({route, local[0]}, length) : RightsControl({local[0], route}, length);
    Require(SendWith(pair[0], "own", control.data(), length) == 3);
    Received result;
    try {
        result = ReceiveWith(pair[1], Space(2 * sizeof(int)));
    } catch (const std::exception&) {
    }
    if (result.bytes == 3 && result.controlLength == Space(2 * sizeof(int))) {
        Require(close_nid_postfix(ReceivedDescriptor(result, 0)) == 0 && close_nid_postfix(ReceivedDescriptor(result, 1)) == 0);
    }
    Require(OpenHostDescriptors() == before);
    Require(::close(route) == 0);
    Require(close_nid_postfix(local[0]) == 0 && close_nid_postfix(local[1]) == 0);
}

static void CheckPlainPeek(const int (&pair)[2]) {
    Require(SendWith(pair[0], "peek", nullptr, 0) == 4);
    const auto peeked = ReceiveWith(pair[1], Space(sizeof(int)), Peek);
    Require(peeked.bytes == 4 && std::memcmp(peeked.data.data(), "peek", 4) == 0 && peeked.controlLength == 0);
    const auto read = ReceiveWith(pair[1], Space(sizeof(int)));
    Require(read.bytes == 4 && std::memcmp(read.data.data(), "peek", 4) == 0 && read.controlLength == 0);
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

static void CheckRefusedKqueue(const int (&pair)[2]) {
    const int queue = kqueue_nid_postfix();
    Require(queue >= 0);
    std::size_t length = 0;
    auto control = RightsControl({queue}, length);
    Require(SendWith(pair[0], "kq", control.data(), length) == -1 && *__error_nid_postfix() == 45);
    Require(SendWith(pair[0], "next", nullptr, 0) == 4);
    const auto received = ReceiveWith(pair[1], Space(sizeof(int)));
    Require(received.bytes == 4 && std::memcmp(received.data.data(), "next", 4) == 0 && received.flags == 0 && received.controlLength == 0);
    Require(close_nid_postfix(queue) == 0);
}

int main() {
    int pair[2] = {-1, -1};
    Require(socketpair_nid_postfix(Unix, Stream, 0, pair) == 0);
    CheckPassedPipe(pair);
    CheckPassedSockets(pair);
    CheckTruncatedControl(pair);
    CheckFaultedReceive(pair);
    CheckFaultedHeader(pair);
    CheckFailedInstallation(pair, false);
    CheckFailedInstallation(pair, true);
    CheckPlainPeek(pair);
    CheckRefusedControl(pair);
    CheckRefusedKqueue(pair);
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
}
