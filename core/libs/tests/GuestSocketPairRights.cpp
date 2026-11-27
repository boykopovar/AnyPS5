#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

static void Require(bool value) { if (!value) std::abort(); }

constexpr int Unix = 1;
constexpr int Stream = 1;
constexpr int Datagram = 2;
constexpr int SequencedPacket = 5;
constexpr int SocketLevel = 0xffff;
constexpr int Rights = 1;
constexpr int Peek = 0x2;
constexpr int Truncated = 0x10;
constexpr int ControlTruncated = 0x20;
constexpr int WaitAll = 0x40;
constexpr int DontWait = 0x80;
constexpr int ControlCloseOnExec = 0x40000;
constexpr int GetDescriptorFlags = 1;
constexpr std::uint64_t ReadableBytes = 0x4004667f;
constexpr short In = 0x1;
constexpr std::size_t Header = 16;

static std::size_t Space(std::size_t bytes) { return Header + ((bytes + 7u) & ~std::size_t{7}); }

static std::vector<unsigned char> Control(std::initializer_list<std::initializer_list<int>> groups) {
    std::vector<unsigned char> control;
    for (const auto& group : groups) {
        const auto at = control.size();
        const auto bytes = group.size() * sizeof(int);
        control.resize(at + Space(bytes));
        const auto length = static_cast<std::uint32_t>(Header + bytes);
        std::memcpy(control.data() + at, &length, sizeof(length));
        std::memcpy(control.data() + at + 4, &SocketLevel, sizeof(SocketLevel));
        std::memcpy(control.data() + at + 8, &Rights, sizeof(Rights));
        std::size_t offset = at + Header;
        for (const int descriptor : group) {
            std::memcpy(control.data() + offset, &descriptor, sizeof(descriptor));
            offset += sizeof(descriptor);
        }
    }
    return control;
}

static std::int64_t SendWith(int socket, const std::string& text, std::vector<unsigned char> control, int flags = 0) {
    KernelIovec vector{const_cast<char*>(text.data()), text.size()};
    GuestMsghdr message{};
    message.iov = &vector;
    message.iovLength = 1;
    message.control = control.empty() ? nullptr : control.data();
    message.controlLength = static_cast<std::uint32_t>(control.size());
    return sendmsg_nid_postfix(socket, &message, flags);
}

struct Received {
    std::int64_t bytes = -1;
    std::string data;
    std::vector<unsigned char> control;
    std::uint32_t controlLength = 0;
    int flags = 0;
};

static Received ReceiveWith(int socket, std::size_t dataCapacity, std::size_t controlCapacity, int flags = 0) {
    Received result;
    std::vector<char> data(dataCapacity);
    result.control.assign(controlCapacity, 0);
    KernelIovec vector{data.data(), data.size()};
    GuestMsghdr message{};
    message.iov = dataCapacity != 0 ? &vector : nullptr;
    message.iovLength = dataCapacity != 0 ? 1 : 0;
    message.control = controlCapacity != 0 ? result.control.data() : nullptr;
    message.controlLength = static_cast<std::uint32_t>(controlCapacity);
    result.bytes = recvmsg_nid_postfix(socket, &message, flags);
    if (result.bytes > 0) result.data.assign(data.data(), static_cast<std::size_t>(result.bytes));
    result.controlLength = message.controlLength;
    result.flags = message.flags;
    return result;
}

static std::vector<int> Record(const Received& received, std::size_t at) {
    std::uint32_t length = 0;
    int level = 0;
    int type = 0;
    Require(at + Header <= received.controlLength);
    std::memcpy(&length, received.control.data() + at, sizeof(length));
    std::memcpy(&level, received.control.data() + at + 4, sizeof(level));
    std::memcpy(&type, received.control.data() + at + 8, sizeof(type));
    Require(level == SocketLevel && type == Rights && length >= Header && at + Space(length - Header) <= received.controlLength);
    std::vector<int> descriptors((length - Header) / sizeof(int));
    std::memcpy(descriptors.data(), received.control.data() + at + Header, descriptors.size() * sizeof(int));
    return descriptors;
}

struct Probe {
    int kept = -1;
    int passed = -1;
    Probe() {
        int pair[2] = {-1, -1};
        Require(socketpair_nid_postfix(Unix, Stream, 0, pair) == 0);
        kept = pair[0];
        passed = pair[1];
    }
    void DropPassed() {
        Require(close_nid_postfix(passed) == 0);
        passed = -1;
    }
    bool Open() const {
        GuestPollDescriptor polled{kept, In, 0};
        return poll_nid_postfix(&polled, 1, 100) == 0;
    }
    bool Ended() const {
        GuestPollDescriptor polled{kept, In, 0};
        if (poll_nid_postfix(&polled, 1, 2000) != 1) return false;
        char byte = 0;
        return recv_nid_postfix(kept, &byte, 1, DontWait) == 0;
    }
    bool Reaches(int other) const {
        char byte = 0;
        return send_nid_postfix(other, "p", 1, 0) == 1 && recv_nid_postfix(kept, &byte, 1, 0) == 1 && byte == 'p';
    }
    ~Probe() {
        if (passed >= 0) close_nid_postfix(passed);
        close_nid_postfix(kept);
    }
};

static void WaitQueued(int socket, int bytes) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    int queued = 0;
    while (ioctl_nid_postfix(socket, ReadableBytes, &queued) == 0 && queued < bytes) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(queued == bytes);
}

static void Pair(int type, int (&pair)[2]) {
    Require(socketpair_nid_postfix(Unix, type, 0, pair) == 0);
}

static void ClosePair(const int (&pair)[2]) {
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
}

static void CheckLowestNumber() {
    int pair[2];
    Pair(Stream, pair);
    int pipe[2] = {-1, -1};
    Require(pipe_nid_postfix(pipe) == 0);
    const int original = pipe[1];
    Require(SendWith(pair[0], "pipe", Control({{original}})) == 4);
    Require(close_nid_postfix(original) == 0);
    const auto received = ReceiveWith(pair[1], 16, Space(sizeof(int)));
    Require(received.bytes == 4 && received.data == "pipe" && received.flags == 0 && received.controlLength == Space(sizeof(int)));
    const auto descriptors = Record(received, 0);
    Require(descriptors.size() == 1 && descriptors[0] == original);
    Require(fcntl_nid_postfix(descriptors[0], GetDescriptorFlags) == 0);
    Require(write_nid_postfix(descriptors[0], "moved", 5) == 5);
    char text[8] = {};
    Require(read_nid_postfix(pipe[0], text, sizeof(text)) == 5 && std::memcmp(text, "moved", 5) == 0);
    Require(close_nid_postfix(descriptors[0]) == 0);
    Require(read_nid_postfix(pipe[0], text, sizeof(text)) == 0);
    const int lowest = dup_nid_postfix(pair[0]);
    Require(lowest >= 0 && close_nid_postfix(lowest) == 0);
    Require(SendWith(pair[0], "kept", Control({{pipe[0]}})) == 4);
    const auto again = ReceiveWith(pair[1], 16, Space(sizeof(int)));
    Require(again.bytes == 4 && again.data == "kept" && Record(again, 0) == std::vector<int>{lowest});
    Require(close_nid_postfix(lowest) == 0);
    Require(close_nid_postfix(pipe[0]) == 0);
    ClosePair(pair);
}

static void CheckPassedSocket(int type) {
    int pair[2];
    Pair(type, pair);
    Probe probe;
    Require(SendWith(pair[0], "one", Control({{probe.passed}})) == 3);
    probe.DropPassed();
    Require(probe.Open());
    const auto received = ReceiveWith(pair[1], 16, Space(sizeof(int)), ControlCloseOnExec);
    Require(received.bytes == 3 && received.data == "one" && received.controlLength == Space(sizeof(int)));
    const auto descriptors = Record(received, 0);
    Require(descriptors.size() == 1);
    Require(fcntl_nid_postfix(descriptors[0], GetDescriptorFlags) == 1);
    Require(probe.Reaches(descriptors[0]));
    Require(close_nid_postfix(descriptors[0]) == 0);
    Require(probe.Ended());
    ClosePair(pair);
}

static void CheckStreamRecords() {
    int pair[2];
    Pair(Stream, pair);
    Probe first;
    Probe second;
    Require(send_nid_postfix(pair[0], "ab", 2, 0) == 2);
    Require(SendWith(pair[0], "cd", Control({{first.passed}})) == 2);
    Require(send_nid_postfix(pair[0], "ef", 2, 0) == 2);
    Require(SendWith(pair[0], "gh", Control({{second.passed}})) == 2);
    first.DropPassed();
    second.DropPassed();
    WaitQueued(pair[1], 8);
    char plain[16] = {};
    Require(recv_nid_postfix(pair[1], plain, sizeof(plain), 0) == 2 && std::memcmp(plain, "ab", 2) == 0);
    const auto merged = ReceiveWith(pair[1], 16, Space(sizeof(int)));
    Require(merged.bytes == 4 && merged.data == "cdef" && merged.flags == 0);
    const auto firstPassed = Record(merged, 0);
    Require(firstPassed.size() == 1 && first.Reaches(firstPassed[0]));
    const auto last = ReceiveWith(pair[1], 16, Space(sizeof(int)));
    Require(last.bytes == 2 && last.data == "gh");
    const auto secondPassed = Record(last, 0);
    Require(secondPassed.size() == 1 && second.Reaches(secondPassed[0]));
    Require(close_nid_postfix(firstPassed[0]) == 0 && close_nid_postfix(secondPassed[0]) == 0);
    Require(first.Ended() && second.Ended());
    ClosePair(pair);
}

static void CheckPlainReceiveDiscards() {
    int pair[2];
    Pair(Stream, pair);
    Probe probe;
    Require(SendWith(pair[0], "zz", Control({{probe.passed}})) == 2);
    probe.DropPassed();
    WaitQueued(pair[1], 2);
    char text[8] = {};
    Require(read_nid_postfix(pair[1], text, sizeof(text)) == 2 && std::memcmp(text, "zz", 2) == 0);
    Require(probe.Ended());
    ClosePair(pair);
}

static void CheckTruncatedControl() {
    int pair[2];
    Pair(Stream, pair);
    Probe whole;
    Require(SendWith(pair[0], "cut", Control({{whole.passed}})) == 3);
    whole.DropPassed();
    WaitQueued(pair[1], 3);
    const auto cut = ReceiveWith(pair[1], 16, Header);
    Require(cut.bytes == 3 && cut.data == "cut" && (cut.flags & ControlTruncated) != 0 && cut.controlLength == 0);
    Require(whole.Ended());

    Probe kept;
    Probe dropped;
    Require(SendWith(pair[0], "two", Control({{kept.passed}, {dropped.passed}})) == 3);
    kept.DropPassed();
    dropped.DropPassed();
    WaitQueued(pair[1], 3);
    const auto first = ReceiveWith(pair[1], 16, Space(sizeof(int)));
    Require(first.bytes == 3 && (first.flags & ControlTruncated) != 0 && first.controlLength == Space(sizeof(int)));
    const auto descriptors = Record(first, 0);
    Require(descriptors.size() == 1 && kept.Reaches(descriptors[0]));
    Require(dropped.Ended());
    Require(close_nid_postfix(descriptors[0]) == 0);
    Require(kept.Ended());
    ClosePair(pair);
}

static void CheckSeparateMessages() {
    int pair[2];
    Pair(Stream, pair);
    Probe first;
    Probe second;
    Probe third;
    Require(SendWith(pair[0], "three", Control({{first.passed}, {second.passed, third.passed}})) == 5);
    first.DropPassed();
    second.DropPassed();
    third.DropPassed();
    const auto received = ReceiveWith(pair[1], 16, Space(sizeof(int)) + Space(2 * sizeof(int)));
    Require(received.bytes == 5 && received.flags == 0 && received.controlLength == Space(sizeof(int)) + Space(2 * sizeof(int)));
    const auto one = Record(received, 0);
    const auto two = Record(received, Space(sizeof(int)));
    Require(one.size() == 1 && two.size() == 2);
    Require(first.Reaches(one[0]) && second.Reaches(two[0]) && third.Reaches(two[1]));
    for (const int descriptor : {one[0], two[0], two[1]}) Require(close_nid_postfix(descriptor) == 0);
    Require(first.Ended() && second.Ended() && third.Ended());
    ClosePair(pair);
}

static void CheckWaitAllStops() {
    int pair[2];
    Pair(Stream, pair);
    Probe probe;
    std::int64_t waited = -1;
    char text[8] = {};
    std::thread reader([&] { waited = recv_nid_postfix(pair[1], text, 6, WaitAll); });
    Require(send_nid_postfix(pair[0], "abc", 3, 0) == 3);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(SendWith(pair[0], "def", Control({{probe.passed}})) == 3);
    probe.DropPassed();
    reader.join();
    Require(waited == 3 && std::memcmp(text, "abc", 3) == 0);
    const auto rest = ReceiveWith(pair[1], 16, Space(sizeof(int)));
    Require(rest.bytes == 3 && rest.data == "def");
    const auto descriptors = Record(rest, 0);
    Require(descriptors.size() == 1 && probe.Reaches(descriptors[0]));
    Require(close_nid_postfix(descriptors[0]) == 0);
    Require(probe.Ended());
    ClosePair(pair);
}

static void CheckBlockedReceiver() {
    int pair[2];
    Pair(Stream, pair);
    Probe probe;
    Received received;
    std::thread reader([&] { received = ReceiveWith(pair[1], 16, Space(sizeof(int))); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(SendWith(pair[0], "late", Control({{probe.passed}})) == 4);
    probe.DropPassed();
    reader.join();
    Require(received.bytes == 4 && received.data == "late");
    const auto descriptors = Record(received, 0);
    Require(descriptors.size() == 1 && probe.Reaches(descriptors[0]));
    Require(close_nid_postfix(descriptors[0]) == 0);
    Require(probe.Ended());
    ClosePair(pair);
}

static void CheckEmptyReceive() {
    int pair[2];
    Pair(Stream, pair);
    Probe probe;
    Require(SendWith(pair[0], "data", Control({{probe.passed}})) == 4);
    probe.DropPassed();
    WaitQueued(pair[1], 4);
    const auto empty = ReceiveWith(pair[1], 0, Space(sizeof(int)));
    Require(empty.bytes == 0 && empty.controlLength == Space(sizeof(int)));
    const auto descriptors = Record(empty, 0);
    Require(descriptors.size() == 1 && probe.Reaches(descriptors[0]));
    const auto data = ReceiveWith(pair[1], 16, Space(sizeof(int)));
    Require(data.bytes == 4 && data.data == "data" && data.controlLength == 0);
    Require(close_nid_postfix(descriptors[0]) == 0);
    Require(probe.Ended());
    ClosePair(pair);
}

static void CheckPeek() {
    int pair[2];
    Pair(Stream, pair);
    Probe probe;
    Require(send_nid_postfix(pair[0], "a", 1, 0) == 1);
    Require(SendWith(pair[0], "pk", Control({{probe.passed}})) == 2);
    probe.DropPassed();
    WaitQueued(pair[1], 3);
    char text[8] = {};
    Require(recv_nid_postfix(pair[1], text, sizeof(text), Peek) == 1);
    Require(recv_nid_postfix(pair[1], text, sizeof(text), 0) == 1 && text[0] == 'a');
    const auto peeked = ReceiveWith(pair[1], 16, Space(sizeof(int)), Peek);
    Require(peeked.bytes == 2 && peeked.data == "pk" && peeked.controlLength == 0);
    const auto taken = ReceiveWith(pair[1], 16, Space(sizeof(int)));
    Require(taken.bytes == 2 && taken.data == "pk");
    const auto descriptors = Record(taken, 0);
    Require(descriptors.size() == 1 && probe.Reaches(descriptors[0]));
    Require(close_nid_postfix(descriptors[0]) == 0);
    Require(probe.Ended());
    ClosePair(pair);
}

static void CheckClosedReceiver(int type) {
    int pair[2];
    Pair(type, pair);
    Probe probe;
    Require(SendWith(pair[0], "gone", Control({{probe.passed}})) == 4);
    probe.DropPassed();
    Require(probe.Open());
    Require(close_nid_postfix(pair[1]) == 0);
    Require(probe.Ended());
    Require(close_nid_postfix(pair[0]) == 0);
}

static void CheckReplacedReceiver() {
    int pair[2];
    Pair(Stream, pair);
    int spare[2];
    Pair(Stream, spare);
    Probe probe;
    Require(SendWith(pair[0], "over", Control({{probe.passed}})) == 4);
    probe.DropPassed();
    Require(dup2_nid_postfix(spare[0], pair[1]) == pair[1]);
    Require(probe.Ended());
    ClosePair(pair);
    ClosePair(spare);
}

static void CheckShutReceiver() {
    int pair[2];
    Pair(Stream, pair);
    Probe probe;
    Require(SendWith(pair[0], "shut", Control({{probe.passed}})) == 4);
    probe.DropPassed();
    Require(shutdown_nid_postfix(pair[1], 0) == 0);
    Require(probe.Ended());
    ClosePair(pair);
}

static void CheckRefusedSend() {
    int pair[2];
    Pair(Stream, pair);
    Probe probe;
    Require(close_nid_postfix(pair[1]) == 0);
    *__error_nid_postfix() = 0;
    Require(SendWith(pair[0], "late", Control({{probe.passed}})) == -1 && *__error_nid_postfix() == 32);
    probe.DropPassed();
    Require(probe.Ended());
    Require(close_nid_postfix(pair[0]) == 0);

    Pair(Stream, pair);
    Probe shut;
    Require(shutdown_nid_postfix(pair[0], 1) == 0);
    *__error_nid_postfix() = 0;
    Require(SendWith(pair[0], "late", Control({{shut.passed}})) == -1 && *__error_nid_postfix() == 32);
    shut.DropPassed();
    Require(shut.Ended());
    const auto empty = ReceiveWith(pair[1], 0, Space(sizeof(int)));
    Require(empty.bytes == 0 && empty.controlLength == 0);
    ClosePair(pair);
}

static void CheckSequencedRecords() {
    int pair[2];
    Pair(SequencedPacket, pair);
    Probe first;
    Probe second;
    Probe third;
    Require(SendWith(pair[0], "r1", Control({{first.passed}})) == 2);
    Require(send_nid_postfix(pair[0], "r2", 2, 0) == 2);
    Require(SendWith(pair[0], "r3", Control({{second.passed}})) == 2);
    Require(SendWith(pair[0], "a long record", Control({{third.passed}})) == 13);
    first.DropPassed();
    second.DropPassed();
    third.DropPassed();
    const auto one = ReceiveWith(pair[1], 16, Space(sizeof(int)));
    Require(one.bytes == 2 && one.data == "r1" && one.flags == 0);
    const auto passed = Record(one, 0);
    Require(passed.size() == 1 && first.Reaches(passed[0]));
    const auto peeked = ReceiveWith(pair[1], 16, Space(sizeof(int)), Peek);
    Require(peeked.bytes == 2 && peeked.data == "r2" && peeked.controlLength == 0);
    const auto two = ReceiveWith(pair[1], 16, Space(sizeof(int)));
    Require(two.bytes == 2 && two.data == "r2" && two.controlLength == 0);
    char text[8] = {};
    Require(recv_nid_postfix(pair[1], text, sizeof(text), 0) == 2 && std::memcmp(text, "r3", 2) == 0);
    Require(second.Ended());
    const auto cut = ReceiveWith(pair[1], 4, Space(sizeof(int)));
    Require(cut.bytes == 4 && cut.data == "a lo" && (cut.flags & Truncated) != 0);
    const auto last = Record(cut, 0);
    Require(last.size() == 1 && third.Reaches(last[0]));
    Require(close_nid_postfix(passed[0]) == 0 && close_nid_postfix(last[0]) == 0);
    Require(first.Ended() && third.Ended());
    ClosePair(pair);
}

static void CheckDatagramRefused() {
    int pair[2];
    Pair(Datagram, pair);
    Probe probe;
    bool refused = false;
    try {
        SendWith(pair[0], "d", Control({{probe.passed}}));
    } catch (const std::runtime_error&) {
        refused = true;
    }
    Require(refused);
    char text[4] = {};
    Require(recv_nid_postfix(pair[1], text, sizeof(text), DontWait) == -1 && *__error_nid_postfix() == 35);
    ClosePair(pair);
}

int main() {
    CheckLowestNumber();
    CheckPassedSocket(Stream);
    CheckPassedSocket(SequencedPacket);
    CheckStreamRecords();
    CheckPlainReceiveDiscards();
    CheckTruncatedControl();
    CheckSeparateMessages();
    CheckWaitAllStops();
    CheckBlockedReceiver();
    CheckEmptyReceive();
    CheckPeek();
    CheckClosedReceiver(Stream);
    CheckClosedReceiver(SequencedPacket);
    CheckReplacedReceiver();
    CheckShutReceiver();
    CheckRefusedSend();
    CheckSequencedRecords();
    CheckDatagramRefused();
}
