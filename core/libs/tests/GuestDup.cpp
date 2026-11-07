#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstdint>
#include <initializer_list>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }

constexpr int GuestEbadf = 9;
constexpr int Unix = 1;
constexpr int Stream = 1;
constexpr int DescriptorSetSize = 1024;
constexpr int Unopened = 0x1ffffff0;

static void RequireFailure(int result, int error) {
    Require(result == -1);
    Require(*__error_nid_postfix() == error);
}

static void RequirePipeCarries(int writer, int reader, const char* text) {
    const auto length = static_cast<std::int64_t>(std::strlen(text));
    Require(write_nid_postfix(writer, text, length) == length);
    char buffer[16] = {};
    Require(read_nid_postfix(reader, buffer, sizeof(buffer)) == length);
    Require(std::memcmp(buffer, text, static_cast<std::size_t>(length)) == 0);
}

static void RequireSocketCarries(int writer, int reader, const char* text) {
    const auto length = std::strlen(text);
    Require(send_nid_postfix(writer, text, length, 0) == static_cast<std::int64_t>(length));
    char buffer[16] = {};
    Require(recv_nid_postfix(reader, buffer, sizeof(buffer), 0) == static_cast<std::int64_t>(length));
    Require(std::memcmp(buffer, text, length) == 0);
}

template <typename TCall> static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static void DupOutlivesOriginal() {
    int pipe[2];
    Require(pipe_nid_postfix(pipe) == 0);
    const int duplicate = dup_nid_postfix(pipe[1]);
    Require(duplicate >= 0 && duplicate != pipe[1]);
    Require(close_nid_postfix(pipe[1]) == 0);
    RequirePipeCarries(duplicate, pipe[0], "dup");
    Require(close_nid_postfix(duplicate) == 0);
    char end = 0;
    Require(read_nid_postfix(pipe[0], &end, 1) == 0);
    Require(close_nid_postfix(pipe[0]) == 0);
}

static void Dup2ReplacesTarget() {
    int first[2];
    int second[2];
    Require(pipe_nid_postfix(first) == 0);
    Require(pipe_nid_postfix(second) == 0);
    Require(dup2_nid_postfix(first[1], second[1]) == second[1]);
    RequirePipeCarries(second[1], first[0], "dup2");
    char end = 0;
    Require(read_nid_postfix(second[0], &end, 1) == 0);
    Require(dup2_nid_postfix(first[1], first[1]) == first[1]);
    for (const int descriptor : {first[0], first[1], second[0], second[1]}) Require(close_nid_postfix(descriptor) == 0);
}

#ifdef _WIN32
static bool g_invalidParameter = false;
static void RecordInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, std::uintptr_t) { g_invalidParameter = true; }
#endif
static void BadDescriptors() {
    int pipe[2];
    Require(pipe_nid_postfix(pipe) == 0);
    const int closed = pipe[1];
    Require(close_nid_postfix(closed) == 0);
    RequireFailure(dup_nid_postfix(-1), GuestEbadf);
    RequireFailure(dup_nid_postfix(closed), GuestEbadf);
    RequireFailure(dup2_nid_postfix(closed, pipe[0]), GuestEbadf);
    RequireFailure(dup2_nid_postfix(closed, closed), GuestEbadf);
    RequireFailure(dup2_nid_postfix(pipe[0], -1), GuestEbadf);
    RequireFailure(dup2_nid_postfix(-1, pipe[0]), GuestEbadf);
    RequireFailure(dup_nid_postfix(Unopened), GuestEbadf);
    RequireFailure(dup2_nid_postfix(Unopened, Unopened + 1), GuestEbadf);
#ifdef _WIN32
    const auto previous = _set_invalid_parameter_handler(RecordInvalidParameter);
#endif
    Require(dup2_nid_postfix(pipe[0], closed) == closed);
    Require(close_nid_postfix(closed) == 0);
    RequireFailure(close_nid_postfix(closed), GuestEbadf);
    RequireFailure(close_nid_postfix(-1), GuestEbadf);
#ifdef _WIN32
    _set_invalid_parameter_handler(previous);
    Require(!g_invalidParameter);
#endif
    Require(close_nid_postfix(pipe[0]) == 0);
}

static void SocketDuplicates() {
    int pair[2];
    Require(socketpair_nid_postfix(Unix, Stream, 0, pair) == 0);
    const int duplicate = dup_nid_postfix(pair[0]);
    Require(duplicate >= 0 && duplicate < DescriptorSetSize && duplicate != pair[0] && duplicate != pair[1]);
    Require(close_nid_postfix(pair[0]) == 0);
    RequireSocketCarries(duplicate, pair[1], "socket");
    int pipe[2];
    Require(pipe_nid_postfix(pipe) == 0);
    const int target = pipe[0];
    Require(dup2_nid_postfix(duplicate, target) == target);
    Require(close_nid_postfix(duplicate) == 0);
    RequireSocketCarries(target, pair[1], "moved");
    Require(dup2_nid_postfix(target, target) == target);
    int next[2];
    Require(socketpair_nid_postfix(Unix, Stream, 0, next) == 0);
    Require(next[0] < DescriptorSetSize && next[1] < DescriptorSetSize);
    RequireSocketCarries(target, pair[1], "kept");
    Require(dup2_nid_postfix(next[0], target) == target);
    RequireSocketCarries(target, next[1], "over");
    Require(Throws([&] { dup2_nid_postfix(target, 1); }));
    int other[2];
    Require(pipe_nid_postfix(other) == 0);
    Require(dup2_nid_postfix(other[1], target) == target);
    RequirePipeCarries(target, other[0], "file");
    Require(send_nid_postfix(target, "x", 1, 0) == -1);
    for (const int descriptor : {target, pair[1], next[0], next[1], pipe[1], other[0], other[1]}) Require(close_nid_postfix(descriptor) == 0);
}

static void RandomDeviceDuplicates() {
    const int random = open_nid_postfix("/dev/urandom", 0, 0);
    Require(random >= 0);
    const int dupRandom = dup_nid_postfix(random);
    Require(dupRandom >= 0 && dupRandom != random);
    unsigned char randomBuf1[16] = {};
    unsigned char randomBuf2[16] = {};
    Require(read_nid_postfix(random, randomBuf1, sizeof(randomBuf1)) == sizeof(randomBuf1));
    Require(read_nid_postfix(dupRandom, randomBuf2, sizeof(randomBuf2)) == sizeof(randomBuf2));
    Require(std::memcmp(randomBuf1, randomBuf2, sizeof(randomBuf1)) != 0);

    Require(dup2_nid_postfix(random, random) == random);
    Require(read_nid_postfix(random, randomBuf1, sizeof(randomBuf1)) == sizeof(randomBuf1));

    int pipe[2];
    Require(pipe_nid_postfix(pipe) == 0);
    Require(write_nid_postfix(pipe[1], "hello", 5) == 5);
    Require(dup2_nid_postfix(pipe[0], random) == random);
    char textBuf[8] = {};
    Require(read_nid_postfix(random, textBuf, 5) == 5);
    Require(std::memcmp(textBuf, "hello", 5) == 0);

    const int targetHost = pipe[0];
    Require(dup2_nid_postfix(dupRandom, targetHost) == targetHost);
    unsigned char randomBuf3[16] = {};
    Require(read_nid_postfix(targetHost, randomBuf3, sizeof(randomBuf3)) == sizeof(randomBuf3));
    Require(std::memcmp(randomBuf3, randomBuf2, sizeof(randomBuf3)) != 0);

    const int secondRandom = open_nid_postfix("/dev/random", 0, 0);
    Require(secondRandom >= 0);
    Require(dup2_nid_postfix(dupRandom, secondRandom) == secondRandom);
    unsigned char randomBuf4[16] = {};
    Require(read_nid_postfix(secondRandom, randomBuf4, sizeof(randomBuf4)) == sizeof(randomBuf4));
    Require(std::memcmp(randomBuf4, randomBuf3, sizeof(randomBuf4)) != 0);

    const int invalidFd = Unopened - 100;
    RequireFailure(dup2_nid_postfix(invalidFd, secondRandom), GuestEbadf);
    unsigned char randomBuf5[16] = {};
    Require(read_nid_postfix(secondRandom, randomBuf5, sizeof(randomBuf5)) == sizeof(randomBuf5));

    for (const int descriptor : {random, dupRandom, pipe[1], secondRandom}) Require(close_nid_postfix(descriptor) == 0);
}

static void DuplicatesShareStatusFlags() {
    constexpr int GetDescriptorFlags = 1;
    constexpr int SetDescriptorFlags = 2;
    constexpr int GetFlags = 3;
    constexpr int SetFlags = 4;
    constexpr int ReadWrite = 2;
    constexpr int NonBlocking = 4;
    constexpr int GuestEagain = 35;
    int pipe[2];
    int other[2];
    Require(pipe_nid_postfix(pipe) == 0);
    Require(pipe_nid_postfix(other) == 0);
    Require(fcntl_nid_postfix(pipe[0], SetDescriptorFlags, 1) == 0);
    *__error_nid_postfix() = 0;
    Require(fcntl_nid_postfix(pipe[0], 0, INT_MAX) == -1 && *__error_nid_postfix() == 22);
    const int duplicate = dup_nid_postfix(pipe[0]);
    Require(duplicate >= 0);
    Require(fcntl_nid_postfix(duplicate, GetDescriptorFlags) == 0);
    Require(fcntl_nid_postfix(duplicate, SetFlags, NonBlocking) == 0);
    Require(fcntl_nid_postfix(pipe[0], GetFlags) == (ReadWrite | NonBlocking));
    Require(dup2_nid_postfix(pipe[0], other[0]) == other[0]);
    Require(fcntl_nid_postfix(other[0], GetDescriptorFlags) == 0);
    Require(fcntl_nid_postfix(other[0], GetFlags) == (ReadWrite | NonBlocking));
    char buffer[4] = {};
    RequireFailure(static_cast<int>(read_nid_postfix(other[0], buffer, sizeof(buffer))), GuestEagain);
    Require(fcntl_nid_postfix(pipe[0], SetFlags, 0) == 0);
    Require(fcntl_nid_postfix(duplicate, GetFlags) == ReadWrite);
    Require(fcntl_nid_postfix(pipe[0], GetDescriptorFlags) == 1);
    for (const int descriptor : {pipe[0], pipe[1], other[0], other[1], duplicate}) Require(close_nid_postfix(descriptor) == 0);
}

int main() {
    DupOutlivesOriginal();
    Dup2ReplacesTarget();
    BadDescriptors();
    SocketDuplicates();
    RandomDeviceDuplicates();
    DuplicatesShareStatusFlags();
}
