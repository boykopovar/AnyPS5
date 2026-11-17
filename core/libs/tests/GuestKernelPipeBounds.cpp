#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "Testing.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <stdexcept>

namespace {

constexpr int guestEperm = 1;
constexpr int guestEbadf = 9;
constexpr int guestEfault = 14;
constexpr int guestEinval = 22;
constexpr int guestEpipe = 32;
constexpr int guestEagain = 35;

constexpr int getDescriptorFlags = 1;
constexpr int setDescriptorFlags = 2;
constexpr int getFlags = 3;
constexpr int setFlags = 4;
constexpr int nonBlocking = 4;
constexpr int readWrite = 2;

constexpr std::uint64_t closeOnExecRequest = 0x20006601;
constexpr std::uint64_t keepOnExecRequest = 0x20006602;
constexpr std::uint64_t nonblockingRequest = 0x8004667e;
constexpr std::uint64_t readableRequest = 0x4004667f;

void RequireFailure(int result, int error) {
    Require(result == -1);
    Require(*__error_nid_postfix() == error);
}

void AuditPipeParameterValidation() {
    *__error_nid_postfix() = 0;
    Require(pipe_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == guestEfault);

    auto* unmappedLow = reinterpret_cast<int*>(std::uintptr_t{0x10});
    *__error_nid_postfix() = 0;
    Require(pipe_nid_postfix(unmappedLow) == -1);
    Require(*__error_nid_postfix() == guestEfault);

    auto* unmappedHigh = reinterpret_cast<int*>(~std::uintptr_t{0xfff});
    *__error_nid_postfix() = 0;
    Require(pipe_nid_postfix(unmappedHigh) == -1);
    Require(*__error_nid_postfix() == guestEfault);

    *__error_nid_postfix() = 0x5a5a;
    int validPair[2] = {-1, -1};
    Require(pipe_nid_postfix(validPair) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);
    Require(validPair[0] >= 0);
    Require(validPair[1] >= 0);
    Require(validPair[0] != validPair[1]);
    Require(close_nid_postfix(validPair[0]) == 0);
    Require(close_nid_postfix(validPair[1]) == 0);
}

void AuditDescriptorAllocationAndRecycling() {
    int pairA[2] = {-1, -1};
    int pairB[2] = {-1, -1};
    Require(pipe_nid_postfix(pairA) == 0);
    Require(pipe_nid_postfix(pairB) == 0);
    Require(pairA[0] != pairB[0] && pairA[0] != pairB[1]);
    Require(pairA[1] != pairB[0] && pairA[1] != pairB[1]);

    Require(close_nid_postfix(pairA[0]) == 0);
    Require(close_nid_postfix(pairA[1]) == 0);

    int pairC[2] = {-1, -1};
    Require(pipe_nid_postfix(pairC) == 0);
    Require(pairC[0] >= 0 && pairC[1] >= 0);
    Require(pairC[0] != pairC[1]);

    Require(close_nid_postfix(pairB[0]) == 0);
    Require(close_nid_postfix(pairB[1]) == 0);
    Require(close_nid_postfix(pairC[0]) == 0);
    Require(close_nid_postfix(pairC[1]) == 0);

    RequireFailure(close_nid_postfix(-1), guestEbadf);
    RequireFailure(close_nid_postfix(99999), guestEbadf);
    RequireFailure(close_nid_postfix(pairA[0]), guestEbadf);
}

void AuditPipePayloadAndEofBounds() {
    int pipe[2] = {-1, -1};
    Require(pipe_nid_postfix(pipe) == 0);

    char zeroBuf[16] = {};
    Require(read_nid_postfix(pipe[0], zeroBuf, 0) == 0);
    Require(write_nid_postfix(pipe[1], zeroBuf, 0) == 0);

    const char sample[] = "AnyPS5 libkernel pipe test payload 1234567890";
    const auto len = static_cast<std::int64_t>(std::strlen(sample));
    Require(write_nid_postfix(pipe[1], sample, len) == len);

    char readBuf[128] = {};
    Require(read_nid_postfix(pipe[0], readBuf, sizeof(readBuf)) == len);
    Require(std::memcmp(sample, readBuf, static_cast<std::size_t>(len)) == 0);

    Require(close_nid_postfix(pipe[1]) == 0);

    char eofByte = 'x';
    Require(read_nid_postfix(pipe[0], &eofByte, 1) == 0);
    Require(read_nid_postfix(pipe[0], &eofByte, 1) == 0);
    Require(close_nid_postfix(pipe[0]) == 0);
}

void AuditBrokenPipeEpipeBounds() {
    int broken[2] = {-1, -1};
    Require(pipe_nid_postfix(broken) == 0);
    Require(close_nid_postfix(broken[0]) == 0);

    *__error_nid_postfix() = 0;
    char dummy = 'z';
    Require(write_nid_postfix(broken[1], &dummy, 1) == -1);
    Require(*__error_nid_postfix() == guestEpipe);
    Require(close_nid_postfix(broken[1]) == 0);
}

void AuditNonblockingReadWriteBounds() {
    int nbPipe[2] = {-1, -1};
    Require(pipe_nid_postfix(nbPipe) == 0);

    Require(fcntl_nid_postfix(nbPipe[0], getFlags) == readWrite);
    Require(fcntl_nid_postfix(nbPipe[1], getFlags) == readWrite);

    Require(fcntl_nid_postfix(nbPipe[0], setFlags, readWrite | nonBlocking) == 0);
    Require(fcntl_nid_postfix(nbPipe[0], getFlags) == (readWrite | nonBlocking));

    char nbBuf[32] = {};
    *__error_nid_postfix() = 0;
    Require(read_nid_postfix(nbPipe[0], nbBuf, sizeof(nbBuf)) == -1);
    Require(*__error_nid_postfix() == guestEagain);

    Require(write_nid_postfix(nbPipe[1], "hello", 5) == 5);
    int available = -1;
    Require(ioctl_nid_postfix(nbPipe[0], readableRequest, &available) == 0);
    Require(available == 5);

    RequireFailure(ioctl_nid_postfix(nbPipe[0], readableRequest, nullptr), guestEfault);
    RequireFailure(ioctl_nid_postfix(nbPipe[0], readableRequest, reinterpret_cast<void*>(std::uintptr_t{0x10})), guestEfault);

    Require(read_nid_postfix(nbPipe[0], nbBuf, 5) == 5);
    Require(std::memcmp(nbBuf, "hello", 5) == 0);

    *__error_nid_postfix() = 0;
    Require(read_nid_postfix(nbPipe[0], nbBuf, sizeof(nbBuf)) == -1);
    Require(*__error_nid_postfix() == guestEagain);

    int disableNb = 0;
    Require(ioctl_nid_postfix(nbPipe[0], nonblockingRequest, &disableNb) == 0);
    Require(fcntl_nid_postfix(nbPipe[0], getFlags) == readWrite);

    int enableNb = 1;
    Require(ioctl_nid_postfix(nbPipe[0], nonblockingRequest, &enableNb) == 0);
    Require(fcntl_nid_postfix(nbPipe[0], getFlags) == (readWrite | nonBlocking));

    Require(fcntl_nid_postfix(nbPipe[1], setFlags, nonBlocking) == 0);
    char fillBlock[1024] = {};
    std::int64_t totalWritten = 0;
    std::int64_t writeResult = 0;
    while (totalWritten < (1 << 22) && (writeResult = write_nid_postfix(nbPipe[1], fillBlock, sizeof(fillBlock))) > 0) {
        totalWritten += writeResult;
    }
    Require(totalWritten > 0);
    RequireFailure(static_cast<int>(writeResult), guestEagain);

    Require(close_nid_postfix(nbPipe[0]) == 0);
    Require(close_nid_postfix(nbPipe[1]) == 0);
}

void AuditCloseOnExecDescriptorFlags() {
    int cloexecPipe[2] = {-1, -1};
    Require(pipe_nid_postfix(cloexecPipe) == 0);

    Require(fcntl_nid_postfix(cloexecPipe[0], getDescriptorFlags) == 0);
    Require(fcntl_nid_postfix(cloexecPipe[1], getDescriptorFlags) == 0);

    Require(fcntl_nid_postfix(cloexecPipe[0], setDescriptorFlags, 1) == 0);
    Require(fcntl_nid_postfix(cloexecPipe[0], getDescriptorFlags) == 1);

    Require(ioctl_nid_postfix(cloexecPipe[1], closeOnExecRequest, nullptr) == 0);
    Require(fcntl_nid_postfix(cloexecPipe[1], getDescriptorFlags) == 1);

    Require(ioctl_nid_postfix(cloexecPipe[1], keepOnExecRequest, nullptr) == 0);
    Require(fcntl_nid_postfix(cloexecPipe[1], getDescriptorFlags) == 0);

    Require(close_nid_postfix(cloexecPipe[0]) == 0);
    Require(close_nid_postfix(cloexecPipe[1]) == 0);
}

} // namespace

int main() {
    AuditPipeParameterValidation();
    AuditDescriptorAllocationAndRecycling();
    AuditPipePayloadAndEofBounds();
    AuditBrokenPipeEpipeBounds();
    AuditNonblockingReadWriteBounds();
    AuditCloseOnExecDescriptorFlags();
    return 0;
}
