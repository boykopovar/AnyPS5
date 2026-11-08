#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <thread>

namespace {

constexpr int GuestEbadf = 9;
constexpr int GuestEinval = 22;
constexpr int GuestDescriptorLimit = 1024;

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

struct GuestTimespec {
    std::int64_t seconds;
    std::int64_t nanoseconds;
};

struct GuestFdSet {
    std::uint64_t fds_bits[GuestDescriptorLimit / 64]{};
};

static_assert(sizeof(GuestFdSet) == 128);

void GuestFdZero(GuestFdSet* set) {
    std::memset(set, 0, sizeof(GuestFdSet));
}

void GuestFdSetBit(int descriptor, GuestFdSet* set) {
    if (descriptor >= 0 && descriptor < GuestDescriptorLimit) {
        set->fds_bits[descriptor / 64] |= (std::uint64_t{1} << (descriptor % 64));
    }
}

void GuestFdClrBit(int descriptor, GuestFdSet* set) {
    if (descriptor >= 0 && descriptor < GuestDescriptorLimit) {
        set->fds_bits[descriptor / 64] &= ~(std::uint64_t{1} << (descriptor % 64));
    }
}

int GuestFdIsSet(int descriptor, const GuestFdSet* set) {
    if (descriptor >= 0 && descriptor < GuestDescriptorLimit) {
        return ((set->fds_bits[descriptor / 64] & (std::uint64_t{1} << (descriptor % 64))) != 0) ? 1 : 0;
    }
    return 0;
}

#undef FD_ZERO
#undef FD_SET
#undef FD_CLR
#undef FD_ISSET

#define FD_ZERO(set) GuestFdZero(set)
#define FD_SET(fd, set) GuestFdSetBit((fd), (set))
#define FD_CLR(fd, set) GuestFdClrBit((fd), (set))
#define FD_ISSET(fd, set) GuestFdIsSet((fd), (set))

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "GuestKernelSelect failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

void TestFdSetMacros() {
    GuestFdSet set{};
    FD_ZERO(&set);
    for (std::size_t index = 0; index < std::size(set.fds_bits); ++index) {
        Require(set.fds_bits[index] == 0);
    }

    const int testBits[] = {0, 1, 63, 64, 65, 127, 128, 511, 512, 1023};
    for (int bit : testBits) {
        Require(!FD_ISSET(bit, &set));
        FD_SET(bit, &set);
        Require(FD_ISSET(bit, &set));
    }

    Require(!FD_ISSET(2, &set));
    Require(!FD_ISSET(62, &set));
    Require(!FD_ISSET(66, &set));
    Require(!FD_ISSET(510, &set));
    Require(!FD_ISSET(1022, &set));

    Require((set.fds_bits[0] & (std::uint64_t{1} << 0)) != 0);
    Require((set.fds_bits[0] & (std::uint64_t{1} << 1)) != 0);
    Require((set.fds_bits[0] & (std::uint64_t{1} << 63)) != 0);
    Require((set.fds_bits[1] & (std::uint64_t{1} << 0)) != 0);
    Require((set.fds_bits[15] & (std::uint64_t{1} << 63)) != 0);

    for (int bit : testBits) {
        FD_CLR(bit, &set);
        Require(!FD_ISSET(bit, &set));
    }

    for (std::size_t index = 0; index < std::size(set.fds_bits); ++index) {
        Require(set.fds_bits[index] == 0);
    }

    FD_SET(-1, &set);
    FD_SET(-100, &set);
    FD_SET(1024, &set);
    FD_SET(2048, &set);
    Require(!FD_ISSET(-1, &set));
    Require(!FD_ISSET(1024, &set));
    for (std::size_t index = 0; index < std::size(set.fds_bits); ++index) {
        Require(set.fds_bits[index] == 0);
    }

    FD_CLR(-1, &set);
    FD_CLR(1024, &set);
    Require(!FD_ISSET(-1, &set));
    Require(!FD_ISSET(1024, &set));
}

void TestNfdsBounds() {
    const GuestTimeval zeroTv{0, 0};
    const GuestTimespec zeroTs{0, 0};

    Require(select_nid_postfix(-1, nullptr, nullptr, nullptr, &zeroTv) == -1);
    Require(Error() == GuestEinval);
    Require(select_nid_postfix(-64, nullptr, nullptr, nullptr, &zeroTv) == -1);
    Require(Error() == GuestEinval);
    Require(select_nid_postfix(1025, nullptr, nullptr, nullptr, &zeroTv) == -1);
    Require(Error() == GuestEinval);
    Require(select_nid_postfix(2048, nullptr, nullptr, nullptr, &zeroTv) == -1);
    Require(Error() == GuestEinval);

    Require(pselect_nid_postfix(-1, nullptr, nullptr, nullptr, &zeroTs, nullptr) == -1);
    Require(Error() == GuestEinval);
    Require(pselect_nid_postfix(-100, nullptr, nullptr, nullptr, &zeroTs, nullptr) == -1);
    Require(Error() == GuestEinval);
    Require(pselect_nid_postfix(1025, nullptr, nullptr, nullptr, &zeroTs, nullptr) == -1);
    Require(Error() == GuestEinval);
    Require(pselect_nid_postfix(4096, nullptr, nullptr, nullptr, &zeroTs, nullptr) == -1);
    Require(Error() == GuestEinval);

    Require(select_nid_postfix(0, nullptr, nullptr, nullptr, &zeroTv) == 0);
    Require(pselect_nid_postfix(0, nullptr, nullptr, nullptr, &zeroTs, nullptr) == 0);

    GuestFdSet emptyRead{};
    GuestFdSet emptyWrite{};
    GuestFdSet emptyExcept{};
    Require(select_nid_postfix(1024, &emptyRead, &emptyWrite, &emptyExcept, &zeroTv) == 0);
    Require(pselect_nid_postfix(1024, &emptyRead, &emptyWrite, &emptyExcept, &zeroTs, nullptr) == 0);

    const GuestTimeval shortTv{0, 50};
    const GuestTimespec shortTs{0, 50000};
    Require(select_nid_postfix(0, nullptr, nullptr, nullptr, &shortTv) == 0);
    Require(pselect_nid_postfix(0, nullptr, nullptr, nullptr, &shortTs, nullptr) == 0);
}

void TestTimeoutValidation() {
    const GuestTimeval negativeSeconds{-1, 0};
    Require(select_nid_postfix(0, nullptr, nullptr, nullptr, &negativeSeconds) == -1);
    Require(Error() == GuestEinval);

    const GuestTimeval negativeMicros{0, -1};
    Require(select_nid_postfix(0, nullptr, nullptr, nullptr, &negativeMicros) == -1);
    Require(Error() == GuestEinval);

    const GuestTimeval overflowMicros{0, 1000000};
    Require(select_nid_postfix(0, nullptr, nullptr, nullptr, &overflowMicros) == -1);
    Require(Error() == GuestEinval);

    const GuestTimeval largeOverflowMicros{2, 1500000};
    Require(select_nid_postfix(0, nullptr, nullptr, nullptr, &largeOverflowMicros) == -1);
    Require(Error() == GuestEinval);

    const GuestTimespec negativeTsSeconds{-1, 0};
    Require(pselect_nid_postfix(0, nullptr, nullptr, nullptr, &negativeTsSeconds, nullptr) == -1);
    Require(Error() == GuestEinval);

    const GuestTimespec negativeNanos{0, -1};
    Require(pselect_nid_postfix(0, nullptr, nullptr, nullptr, &negativeNanos, nullptr) == -1);
    Require(Error() == GuestEinval);

    const GuestTimespec overflowNanos{0, 1000000000};
    Require(pselect_nid_postfix(0, nullptr, nullptr, nullptr, &overflowNanos, nullptr) == -1);
    Require(Error() == GuestEinval);

    const GuestTimespec largeOverflowNanos{3, 2000000000};
    Require(pselect_nid_postfix(0, nullptr, nullptr, nullptr, &largeOverflowNanos, nullptr) == -1);
    Require(Error() == GuestEinval);
}

void TestInvalidDescriptorEbadf() {
    const GuestTimeval zeroTv{0, 0};
    const GuestTimespec zeroTs{0, 0};

    {
        GuestFdSet readSet{};
        FD_SET(999, &readSet);
        Require(select_nid_postfix(1000, &readSet, nullptr, nullptr, &zeroTv) == -1);
        Require(Error() == GuestEbadf);
        Require(pselect_nid_postfix(1000, &readSet, nullptr, nullptr, &zeroTs, nullptr) == -1);
        Require(Error() == GuestEbadf);
    }

    {
        GuestFdSet writeSet{};
        FD_SET(999, &writeSet);
        Require(select_nid_postfix(1000, nullptr, &writeSet, nullptr, &zeroTv) == -1);
        Require(Error() == GuestEbadf);
        Require(pselect_nid_postfix(1000, nullptr, &writeSet, nullptr, &zeroTs, nullptr) == -1);
        Require(Error() == GuestEbadf);
    }

    {
        GuestFdSet exceptSet{};
        FD_SET(999, &exceptSet);
        Require(select_nid_postfix(1000, nullptr, nullptr, &exceptSet, &zeroTv) == -1);
        Require(Error() == GuestEbadf);
        Require(pselect_nid_postfix(1000, nullptr, nullptr, &exceptSet, &zeroTs, nullptr) == -1);
        Require(Error() == GuestEbadf);
    }

    {
        int pipeFds[2]{-1, -1};
        Require(pipe_nid_postfix(pipeFds) == 0);
        Require(close_nid_postfix(pipeFds[0]) == 0);
        Require(close_nid_postfix(pipeFds[1]) == 0);

        GuestFdSet closedSet{};
        FD_SET(pipeFds[0], &closedSet);
        Require(select_nid_postfix(pipeFds[0] + 1, &closedSet, nullptr, nullptr, &zeroTv) == -1);
        Require(Error() == GuestEbadf);
        Require(pselect_nid_postfix(pipeFds[0] + 1, &closedSet, nullptr, nullptr, &zeroTs, nullptr) == -1);
        Require(Error() == GuestEbadf);
    }

    {
        GuestFdSet outOfScanSet{};
        FD_SET(500, &outOfScanSet);
        Require(select_nid_postfix(0, &outOfScanSet, nullptr, nullptr, &zeroTv) == 0);
        Require(pselect_nid_postfix(0, &outOfScanSet, nullptr, nullptr, &zeroTs, nullptr) == 0);
    }
}

void TestPollingImmediateVsInfiniteWait() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);
    const int reader = pipeFds[0];
    const int writer = pipeFds[1];

    const GuestTimeval zeroTv{0, 0};
    const GuestTimespec zeroTs{0, 0};

    {
        GuestFdSet readSet{};
        FD_SET(reader, &readSet);
        const auto pollStart = std::chrono::steady_clock::now();
        const int ready = select_nid_postfix(reader + 1, &readSet, nullptr, nullptr, &zeroTv);
        const auto pollDuration = std::chrono::steady_clock::now() - pollStart;
        Require(ready == 0);
        Require(!FD_ISSET(reader, &readSet));
        Require(pollDuration < std::chrono::milliseconds(100));
    }

    {
        GuestFdSet readSet{};
        FD_SET(reader, &readSet);
        const auto pollStart = std::chrono::steady_clock::now();
        const int ready = pselect_nid_postfix(reader + 1, &readSet, nullptr, nullptr, &zeroTs, nullptr);
        const auto pollDuration = std::chrono::steady_clock::now() - pollStart;
        Require(ready == 0);
        Require(!FD_ISSET(reader, &readSet));
        Require(pollDuration < std::chrono::milliseconds(100));
    }

    Require(write_nid_postfix(writer, "a", 1) == 1);

    {
        GuestFdSet readSet{};
        FD_SET(reader, &readSet);
        const int ready = select_nid_postfix(reader + 1, &readSet, nullptr, nullptr, &zeroTv);
        Require(ready == 1);
        Require(FD_ISSET(reader, &readSet));
    }

    {
        GuestFdSet readSet{};
        FD_SET(reader, &readSet);
        const int ready = pselect_nid_postfix(reader + 1, &readSet, nullptr, nullptr, &zeroTs, nullptr);
        Require(ready == 1);
        Require(FD_ISSET(reader, &readSet));
    }

    char ch = 0;
    Require(read_nid_postfix(reader, &ch, 1) == 1);
    Require(ch == 'a');

    Require(write_nid_postfix(writer, "b", 1) == 1);

    {
        GuestFdSet readSet{};
        FD_SET(reader, &readSet);
        const int ready = select_nid_postfix(reader + 1, &readSet, nullptr, nullptr, nullptr);
        Require(ready == 1);
        Require(FD_ISSET(reader, &readSet));
        Require(read_nid_postfix(reader, &ch, 1) == 1);
        Require(ch == 'b');
    }

    Require(write_nid_postfix(writer, "c", 1) == 1);

    {
        GuestFdSet readSet{};
        FD_SET(reader, &readSet);
        const int ready = pselect_nid_postfix(reader + 1, &readSet, nullptr, nullptr, nullptr, nullptr);
        Require(ready == 1);
        Require(FD_ISSET(reader, &readSet));
        Require(read_nid_postfix(reader, &ch, 1) == 1);
        Require(ch == 'c');
    }

    {
        GuestFdSet readSet{};
        FD_SET(reader, &readSet);
        std::thread backgroundWriter([writer] {
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
            Require(write_nid_postfix(writer, "d", 1) == 1);
        });
        const auto waitStart = std::chrono::steady_clock::now();
        const int ready = select_nid_postfix(reader + 1, &readSet, nullptr, nullptr, nullptr);
        const auto waitElapsed = std::chrono::steady_clock::now() - waitStart;
        backgroundWriter.join();
        Require(ready == 1);
        Require(FD_ISSET(reader, &readSet));
        Require(waitElapsed >= std::chrono::milliseconds(40));
        Require(read_nid_postfix(reader, &ch, 1) == 1);
        Require(ch == 'd');
    }

    {
        GuestFdSet readSet{};
        FD_SET(reader, &readSet);
        std::thread backgroundWriter([writer] {
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
            Require(write_nid_postfix(writer, "e", 1) == 1);
        });
        const auto waitStart = std::chrono::steady_clock::now();
        const int ready = pselect_nid_postfix(reader + 1, &readSet, nullptr, nullptr, nullptr, nullptr);
        const auto waitElapsed = std::chrono::steady_clock::now() - waitStart;
        backgroundWriter.join();
        Require(ready == 1);
        Require(FD_ISSET(reader, &readSet));
        Require(waitElapsed >= std::chrono::milliseconds(40));
        Require(read_nid_postfix(reader, &ch, 1) == 1);
        Require(ch == 'e');
    }

    Require(close_nid_postfix(reader) == 0);
    Require(close_nid_postfix(writer) == 0);
}

void TestWriteAndExceptSets() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);
    const int writer = pipeFds[1];

    const GuestTimeval zeroTv{0, 0};
    const GuestTimespec zeroTs{0, 0};

    GuestFdSet writeSet{};
    FD_SET(writer, &writeSet);
    const int readyWrite = select_nid_postfix(writer + 1, nullptr, &writeSet, nullptr, &zeroTv);
    Require(readyWrite == 1);
    Require(FD_ISSET(writer, &writeSet));

    FD_ZERO(&writeSet);
    FD_SET(writer, &writeSet);
    const int readyPwrite = pselect_nid_postfix(writer + 1, nullptr, &writeSet, nullptr, &zeroTs, nullptr);
    Require(readyPwrite == 1);
    Require(FD_ISSET(writer, &writeSet));

    GuestFdSet exceptSet{};
    FD_SET(writer, &exceptSet);
    const int readyExcept = select_nid_postfix(writer + 1, nullptr, nullptr, &exceptSet, &zeroTv);
    Require(readyExcept == 0);
    Require(!FD_ISSET(writer, &exceptSet));

    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void TestFdSetBitClearingBoundary() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);
    const int reader = pipeFds[0];

    GuestFdSet readSet{};
    FD_SET(reader, &readSet);
    readSet.fds_bits[1] = 0xAAAAAAAAAAAAAAAAULL;

    const GuestTimeval zeroTv{0, 0};
    const int ready = select_nid_postfix(reader + 1, &readSet, nullptr, nullptr, &zeroTv);
    Require(ready == 0);
    Require(!FD_ISSET(reader, &readSet));
    Require(readSet.fds_bits[1] == 0xAAAAAAAAAAAAAAAAULL);

    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

} // namespace

int main() {
    TestFdSetMacros();
    TestNfdsBounds();
    TestTimeoutValidation();
    TestInvalidDescriptorEbadf();
    TestPollingImmediateVsInfiniteWait();
    TestWriteAndExceptSets();
    TestFdSetBitClearingBoundary();
    std::puts("GuestKernelSelect passed");
    return 0;
}
