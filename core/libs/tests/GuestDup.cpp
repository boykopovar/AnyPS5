#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
int APS5_VABI dup_nid_postfix(int);
int APS5_VABI dup2_nid_postfix(int, int);
int APS5_VABI pipe_nid_postfix(int*);
int APS5_VABI close_nid_postfix(int);
std::int64_t APS5_VABI read_nid_postfix(int, void*, std::uint64_t);
std::int64_t APS5_VABI write_nid_postfix(int, const char*, std::int64_t);
int APS5_VABI socketpair_nid_postfix(int, int, int, int*);
std::int64_t APS5_VABI send_nid_postfix(int, const void*, std::uint64_t, int);
std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value) { if (!value) std::abort(); }

constexpr int GuestEbadf = 9;
constexpr int Unix = 1;
constexpr int Stream = 1;
constexpr int FirstSocket = 0x10000000;

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
    Require(duplicate >= 0 && duplicate < FirstSocket && duplicate != pipe[1]);
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
    RequireFailure(dup_nid_postfix(FirstSocket + 0x0ffffff0), GuestEbadf);
    RequireFailure(dup2_nid_postfix(FirstSocket + 0x0ffffff0, FirstSocket + 0x0ffffff1), GuestEbadf);
    Require(close_nid_postfix(pipe[0]) == 0);
}

static void SocketDuplicates() {
    int pair[2];
    Require(socketpair_nid_postfix(Unix, Stream, 0, pair) == 0);
    const int duplicate = dup_nid_postfix(pair[0]);
    Require(duplicate >= FirstSocket && duplicate != pair[0] && duplicate != pair[1]);
    Require(close_nid_postfix(pair[0]) == 0);
    RequireSocketCarries(duplicate, pair[1], "socket");
    const int target = duplicate + 0x1000;
    Require(dup2_nid_postfix(duplicate, target) == target);
    Require(close_nid_postfix(duplicate) == 0);
    RequireSocketCarries(target, pair[1], "moved");
    Require(dup2_nid_postfix(target, target) == target);
    int next[2];
    Require(socketpair_nid_postfix(Unix, Stream, 0, next) == 0);
    Require(next[0] > target && next[1] > target);
    RequireSocketCarries(target, pair[1], "kept");
    Require(dup2_nid_postfix(next[0], target) == target);
    RequireSocketCarries(target, next[1], "over");
    Require(Throws([&] { dup2_nid_postfix(target, 1); }));
    int pipe[2];
    Require(pipe_nid_postfix(pipe) == 0);
    Require(Throws([&] { dup2_nid_postfix(pipe[0], target); }));
    for (const int descriptor : {target, pair[1], next[0], next[1], pipe[0], pipe[1]}) Require(close_nid_postfix(descriptor) == 0);
}

int main() {
    DupOutlivesOriginal();
    Dup2ReplacesTarget();
    BadDescriptors();
    SocketDuplicates();
}
