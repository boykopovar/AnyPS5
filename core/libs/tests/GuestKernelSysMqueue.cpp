#include "SceTypes.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel_sys/MqueueWait.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <source_location>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int Enoent = 2;
constexpr int Ebadf = 9;
constexpr int Efault = 14;
constexpr int Eexist = 17;
constexpr int Einval = 22;
constexpr int Eagain = 35;
constexpr int Emsgsize = 40;
constexpr int Etimedout = 60;
constexpr int Enametoolong = 63;
constexpr int ORdonly = 0x0000;
constexpr int OWronly = 0x0001;
constexpr int ORdwr = 0x0002;
constexpr int ONonblock = 0x0004;
constexpr int OCreat = 0x0200;
constexpr int OExcl = 0x0800;

KernelTimespec g_fakeRealtime{};
std::int64_t g_fakeRealtimeStepSeconds = 0;
std::atomic<unsigned> g_fakeRealtimeClockCalls{0};

int FakeRealtimeClock(KernelTimespec* realtime) {
    g_fakeRealtimeClockCalls.fetch_add(1, std::memory_order_relaxed);
    *realtime = g_fakeRealtime;
    g_fakeRealtime.tv_sec += g_fakeRealtimeStepSeconds;
    return 0;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

std::string Name(const char* tag) {
    return std::string("/anyps5-mq-") + tag + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

GuestTimespec FromNow(std::chrono::milliseconds delay) {
    const auto when = std::chrono::system_clock::now() + delay;
    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(when.time_since_epoch()).count();
    return {nanoseconds / 1000000000, nanoseconds % 1000000000};
}

std::string Receive(int queue, unsigned* priority = nullptr) {
    char buffer[16]{};
    const auto length = kmq_timedreceive_nid_postfix(queue, buffer, sizeof(buffer), priority, nullptr);
    Require(length >= 0);
    return std::string(buffer, static_cast<std::size_t>(length));
}

void OpenRules() {
    const auto name = Name("open");
    const GuestMqAttr zero{0, 0, 16, 0, {}};
    const GuestMqAttr many{0, 101, 16, 0, {}};
    const GuestMqAttr empty{0, 2, 0, 0, {}};
    const GuestMqAttr huge{0, 2, 16385, 0, {}};
    Require(kmq_open_nid_postfix(name.c_str(), 3 | OCreat, 0600, nullptr) == -1 && Error() == Einval);
    Require(kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &zero) == -1 && Error() == Einval);
    Require(kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &many) == -1 && Error() == Einval);
    Require(kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &empty) == -1 && Error() == Einval);
    Require(kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &huge) == -1 && Error() == Einval);
    Require(kmq_open_nid_postfix("relative", ORdwr | OCreat, 0600, nullptr) == -1 && Error() == Einval);
    Require(kmq_open_nid_postfix("/", ORdwr | OCreat, 0600, nullptr) == -1 && Error() == Einval);
    Require(kmq_open_nid_postfix("/a/b", ORdwr | OCreat, 0600, nullptr) == -1 && Error() == Einval);
    const std::string longName = "/" + std::string(300, 'q');
    Require(kmq_open_nid_postfix(longName.c_str(), ORdwr | OCreat, 0600, nullptr) == -1 && Error() == Enametoolong);
    Require(kmq_open_nid_postfix(nullptr, ORdwr | OCreat, 0600, nullptr) == -1 && Error() == Efault);
    Require(kmq_open_nid_postfix(name.c_str(), ORdwr, 0600, nullptr) == -1 && Error() == Enoent);
    const int queue = kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, nullptr);
    Require(queue >= 0);
    GuestMqAttr attributes{};
    Require(kmq_setattr_nid_postfix(queue, nullptr, &attributes) == 0);
    Require(attributes.maxmsg == 10 && attributes.msgsize == 1024 && attributes.curmsgs == 0 && attributes.flags == 0);
    Require(kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat | OExcl, 0600, nullptr) == -1 && Error() == Eexist);
    Require(close_nid_postfix(queue) == 0);
    Require(kmq_unlink_nid_postfix(name.c_str()) == 0);
    Require(kmq_unlink_nid_postfix(name.c_str()) == -1 && Error() == Enoent);
    Require(kmq_unlink_nid_postfix("bad") == -1 && Error() == Einval);
}

void Messages() {
    const auto name = Name("messages");
    const GuestMqAttr small{0, 3, 16, 0, {}};
    const int queue = kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &small);
    Require(queue >= 0);
    Require(kmq_timedsend_nid_postfix(queue, "x", 1, 64, nullptr) == -1 && Error() == Einval);
    Require(kmq_timedsend_nid_postfix(queue, "0123456789abcdefg", 17, 1, nullptr) == -1 && Error() == Emsgsize);
    Require(kmq_timedsend_nid_postfix(queue, "low", 3, 1, nullptr) == 0);
    Require(kmq_timedsend_nid_postfix(queue, "high", 4, 5, nullptr) == 0);
    Require(kmq_timedsend_nid_postfix(queue, "low2", 4, 1, nullptr) == 0);
    GuestMqAttr attributes{};
    const GuestMqAttr nonblocking{ONonblock, 0, 0, 0, {}};
    const GuestMqAttr bad{0x1, 0, 0, 0, {}};
    Require(kmq_setattr_nid_postfix(queue, &bad, nullptr) == -1 && Error() == Einval);
    Require(kmq_setattr_nid_postfix(queue, &nonblocking, &attributes) == 0 && attributes.flags == 0 && attributes.curmsgs == 3);
    Require(kmq_setattr_nid_postfix(queue, nullptr, &attributes) == 0 && attributes.flags == ONonblock);
    Require(kmq_timedsend_nid_postfix(queue, "more", 4, 1, nullptr) == -1 && Error() == Eagain);
    char tiny[8]{};
    Require(kmq_timedreceive_nid_postfix(queue, tiny, sizeof(tiny), nullptr, nullptr) == -1 && Error() == Emsgsize);
    unsigned priority = 99;
    Require(Receive(queue, &priority) == "high" && priority == 5);
    Require(Receive(queue, &priority) == "low" && priority == 1);
    Require(Receive(queue, &priority) == "low2" && priority == 1);
    char buffer[16]{};
    Require(kmq_timedreceive_nid_postfix(queue, buffer, sizeof(buffer), nullptr, nullptr) == -1 && Error() == Eagain);
    Require(close_nid_postfix(queue) == 0 && kmq_unlink_nid_postfix(name.c_str()) == 0);
}

void DeadlineConversion() {
    constexpr std::uint64_t monotonic = 20000000000ULL;
    const std::uint64_t deadline = MqueueWait::MonotonicDeadline(102, 250000000, 100, 500000000, monotonic);
    Require(deadline == monotonic + 1750000000ULL);
    const std::uint64_t monotonicAfterStep = monotonic + 500000000ULL;
    const std::uint64_t deadlineAfterForwardStep = MqueueWait::MonotonicDeadline(102, 250000000, 102, 750000000, monotonicAfterStep);
    const std::uint64_t deadlineAfterBackwardStep = MqueueWait::MonotonicDeadline(102, 250000000, 99, 0, monotonicAfterStep);
    Require(deadline > monotonicAfterStep);
    Require(deadline - monotonicAfterStep == 1250000000ULL);
    Require(deadlineAfterForwardStep == monotonicAfterStep);
    Require(deadlineAfterBackwardStep > deadline);
    Require(MqueueWait::MonotonicDeadline(100, 400000000, 100, 500000000, monotonic) == monotonic);
    Require(MqueueWait::MonotonicDeadline(std::numeric_limits<std::int64_t>::max(), 0, 0, 0,
                                          std::numeric_limits<std::uint64_t>::max() - 500000000ULL) ==
            std::numeric_limits<std::uint64_t>::max());
}

void MonotonicWaitIgnoresRealtimeSteps() {
    constexpr std::uint64_t NanosecondsPerSecond = 1000000000ULL;
    constexpr std::uint64_t monotonicStart = 20ULL * NanosecondsPerSecond;
    constexpr std::int64_t realtimeStart = 100LL * static_cast<std::int64_t>(NanosecondsPerSecond) + 500000000LL;
    constexpr std::int64_t absoluteDeadline = 102LL * static_cast<std::int64_t>(NanosecondsPerSecond) + 250000000LL;
    const std::uint64_t deadline = MqueueWait::MonotonicDeadline(102, 250000000, 100, 500000000, monotonicStart);
    Require(deadline == monotonicStart + 1750000000ULL);

    const auto verifyRealtimeStep = [&](std::int64_t stepSeconds) {
        std::uint64_t monotonicNow = monotonicStart;
        std::int64_t realtimeNow = realtimeStart;
        unsigned waitCalls = 0;
        const bool completed = MqueueWait::WaitUntilMonotonic(
            deadline, [] { return false; }, [&] { return monotonicNow; },
            [&](std::uint64_t waitDeadline, const auto& ready) {
                ++waitCalls;
                Require(waitDeadline == deadline);
                monotonicNow += 250000000ULL;
                realtimeNow += stepSeconds * static_cast<std::int64_t>(NanosecondsPerSecond);
                Require(monotonicNow < deadline);
                if (stepSeconds > 0) Require(realtimeNow > absoluteDeadline);
                else Require(realtimeNow < absoluteDeadline);
                monotonicNow = waitDeadline;
                return ready();
            });

        Require(!completed);
        Require(waitCalls == 1);
        Require(monotonicNow == deadline);
        Require(monotonicNow - monotonicStart == 1750000000ULL);
        if (stepSeconds > 0) Require(realtimeNow > absoluteDeadline);
        else Require(realtimeNow < absoluteDeadline);
    };

    verifyRealtimeStep(3600);
    verifyRealtimeStep(-3600);
}

void MonotonicWaitChunksLongDeadlines() {
    constexpr std::uint64_t NanosecondsPerHour = 60ULL * 60 * 1000000000ULL;
    constexpr std::uint64_t MaxWaitChunk = 24ULL * NanosecondsPerHour;
    std::uint64_t now = 100;
    const std::uint64_t deadline = now + 49ULL * NanosecondsPerHour + 1;
    std::array<std::uint64_t, 3> chunks{};
    std::size_t chunkCount = 0;

    const bool completed = MqueueWait::WaitUntilMonotonic(
        deadline, [] { return false; }, [&] { return now; }, [&](std::uint64_t waitDeadline, auto ready) {
            if (chunkCount < chunks.size()) chunks[chunkCount] = waitDeadline - now;
            ++chunkCount;
            now = waitDeadline;
            return ready();
        });

    Require(!completed);
    Require(chunkCount == 3);
    Require(chunks[0] == MaxWaitChunk);
    Require(chunks[1] == MaxWaitChunk);
    Require(chunks[2] == NanosecondsPerHour + 1);
    Require(now == deadline);
}

void ProductionWaitUsesMonotonicClock() {
    const auto name = Name("monotonic-production");
    const GuestMqAttr one{0, 1, 16, 0, {}};
    const int queue = kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &one);
    Require(queue >= 0);

    for (const std::int64_t offset : {2, -2}) {
        SetRealtimeClockHookForTesting(nullptr);
        KernelTimespec realtime{};
        Require(clock_gettime_nid_postfix(0, &realtime) == 0);
        g_fakeRealtime = KernelTimespec{realtime.tv_sec + offset, realtime.tv_nsec};
        g_fakeRealtimeStepSeconds = offset > 0 ? -3600 : 3600;
        const std::int64_t fakeRealtimeBeforeStep = g_fakeRealtime.tv_sec;
        g_fakeRealtimeClockCalls.store(0, std::memory_order_relaxed);
        SetRealtimeClockHookForTesting(FakeRealtimeClock);

        GuestTimespec deadline{g_fakeRealtime.tv_sec, g_fakeRealtime.tv_nsec + 500000000};
        if (deadline.nanoseconds >= 1000000000) {
            ++deadline.seconds;
            deadline.nanoseconds -= 1000000000;
        }

        char buffer[16]{};
        const auto start = std::chrono::steady_clock::now();
        Require(kmq_timedreceive_nid_postfix(queue, buffer, sizeof(buffer), nullptr, &deadline) == -1 && Error() == Etimedout);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        Require(elapsed >= std::chrono::milliseconds(300));
        Require(elapsed < std::chrono::milliseconds(1500));
        Require(g_fakeRealtimeClockCalls.load(std::memory_order_relaxed) == 1);
        Require(g_fakeRealtime.tv_sec == fakeRealtimeBeforeStep + g_fakeRealtimeStepSeconds);
    }
    SetRealtimeClockHookForTesting(nullptr);
    Require(close_nid_postfix(queue) == 0 && kmq_unlink_nid_postfix(name.c_str()) == 0);
}

void Waiting() {
    const auto name = Name("waiting");
    const GuestMqAttr one{0, 1, 16, 0, {}};
    const int queue = kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &one);
    Require(queue >= 0);
    char buffer[16]{};
    const GuestTimespec invalid{0, 1000000000};
    Require(kmq_timedreceive_nid_postfix(queue, buffer, sizeof(buffer), nullptr, &invalid) == -1 && Error() == Einval);
    const auto deadline = FromNow(std::chrono::milliseconds(150));
    const auto start = std::chrono::steady_clock::now();
    Require(kmq_timedreceive_nid_postfix(queue, buffer, sizeof(buffer), nullptr, &deadline) == -1 && Error() == Etimedout);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(100));
    Require(kmq_timedsend_nid_postfix(queue, "a", 1, 0, &invalid) == 0);
    Require(kmq_timedsend_nid_postfix(queue, "b", 1, 0, &invalid) == -1 && Error() == Einval);
    Require(kmq_timedreceive_nid_postfix(queue, buffer, sizeof(buffer), nullptr, &invalid) == 1 && buffer[0] == 'a');
    std::atomic<std::int64_t> received{-2};
    std::thread reader([&] {
        char local[16]{};
        received = kmq_timedreceive_nid_postfix(queue, local, sizeof(local), nullptr, nullptr);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    Require(received == -2);
    Require(kmq_timedsend_nid_postfix(queue, "wake", 4, 3, nullptr) == 0);
    reader.join();
    Require(received == 4);
    Require(close_nid_postfix(queue) == 0 && kmq_unlink_nid_postfix(name.c_str()) == 0);
}

void Access() {
    const auto name = Name("access");
    const int both = kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, nullptr);
    const int reader = kmq_open_nid_postfix(name.c_str(), ORdonly, 0600, nullptr);
    const int writer = kmq_open_nid_postfix(name.c_str(), OWronly | ONonblock, 0600, nullptr);
    Require(both >= 0 && reader >= 0 && writer >= 0);
    char buffer[1024]{};
    Require(kmq_timedsend_nid_postfix(reader, "x", 1, 0, nullptr) == -1 && Error() == Ebadf);
    Require(kmq_timedreceive_nid_postfix(writer, buffer, sizeof(buffer), nullptr, nullptr) == -1 && Error() == Ebadf);
    Require(kmq_timedsend_nid_postfix(writer, "shared", 6, 0, nullptr) == 0);
    Require(kmq_unlink_nid_postfix(name.c_str()) == 0);
    Require(kmq_open_nid_postfix(name.c_str(), ORdwr, 0600, nullptr) == -1 && Error() == Enoent);
    Require(kmq_timedreceive_nid_postfix(reader, buffer, sizeof(buffer), nullptr, nullptr) == 6);
    Require(close_nid_postfix(both) == 0 && close_nid_postfix(reader) == 0);
    Require(kmq_timedreceive_nid_postfix(reader, buffer, sizeof(buffer), nullptr, nullptr) == -1 && Error() == Ebadf);
    const auto path = "anyps5-mq-stale-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin";
    { std::ofstream stream(path); stream << "x"; }
    const int plain = open_nid_postfix(path.c_str(), 0, 0);
    Require(plain >= 0);
    Require(kmq_timedsend_nid_postfix(plain, "x", 1, 0, nullptr) == -1 && Error() == Ebadf);
    Require(kmq_setattr_nid_postfix(plain, nullptr, nullptr) == -1 && Error() == Ebadf);
    Require(close_nid_postfix(plain) == 0 && close_nid_postfix(writer) == 0);
    std::filesystem::remove(path);
}

void NullBuffer() {
    const auto name = Name("nullbuffer");
    const GuestMqAttr small{0, 3, 16, 0, {}};
    const int queue = kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &small);
    Require(queue >= 0);
    Require(kmq_timedsend_nid_postfix(queue, "keep", 4, 2, nullptr) == 0);
    Require(kmq_timedreceive_nid_postfix(queue, nullptr, 16, nullptr, nullptr) == -1 && Error() == Efault);
    unsigned priority = 0;
    Require(Receive(queue, &priority) == "keep" && priority == 2);
    Require(close_nid_postfix(queue) == 0 && kmq_unlink_nid_postfix(name.c_str()) == 0);
}

void FarDeadline() {
    const auto name = Name("fardeadline");
    const GuestMqAttr one{0, 1, 16, 0, {}};
    const int queue = kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &one);
    Require(queue >= 0);
    const GuestTimespec forever{INT64_MAX, 0};
    std::atomic<std::int64_t> received{-2};
    std::thread reader([&] {
        char local[16]{};
        received = kmq_timedreceive_nid_postfix(queue, local, sizeof(local), nullptr, &forever);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    Require(received == -2);
    Require(kmq_timedsend_nid_postfix(queue, "late", 4, 0, nullptr) == 0);
    reader.join();
    Require(received == 4);
    Require(close_nid_postfix(queue) == 0 && kmq_unlink_nid_postfix(name.c_str()) == 0);
}

void ConcurrentSetattr() {
    const auto name = Name("setattr");
    const GuestMqAttr one{0, 1, 16, 0, {}};
    const int queue = kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &one);
    Require(queue >= 0);
    std::atomic<bool> stop{false};
    std::thread toggler([&] {
        const GuestMqAttr on{ONonblock, 0, 0, 0, {}};
        const GuestMqAttr off{0, 0, 0, 0, {}};
        while (!stop) {
            kmq_setattr_nid_postfix(queue, &on, nullptr);
            kmq_setattr_nid_postfix(queue, &off, nullptr);
        }
    });
    char buffer[16]{};
    for (int i = 0; i < 200; ++i) {
        const auto deadline = FromNow(std::chrono::milliseconds(1));
        const auto result = kmq_timedreceive_nid_postfix(queue, buffer, sizeof(buffer), nullptr, &deadline);
        Require(result == -1 && (Error() == Etimedout || Error() == Eagain));
    }
    stop = true;
    toggler.join();
    const GuestMqAttr off{0, 0, 0, 0, {}};
    Require(kmq_setattr_nid_postfix(queue, &off, nullptr) == 0);
    Require(kmq_timedsend_nid_postfix(queue, "ok", 2, 0, nullptr) == 0);
    unsigned priority = 7;
    Require(Receive(queue, &priority) == "ok" && priority == 0);
    Require(close_nid_postfix(queue) == 0 && kmq_unlink_nid_postfix(name.c_str()) == 0);
}

class UnreadablePage {
public:
    UnreadablePage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(memory != nullptr);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0);
        size = static_cast<std::size_t>(pageSize);
        memory = static_cast<char*>(mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
#endif
    }

    ~UnreadablePage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, size);
#endif
    }

    UnreadablePage(const UnreadablePage&) = delete;
    UnreadablePage& operator=(const UnreadablePage&) = delete;

    void* Address() const { return memory; }

private:
    char* memory = nullptr;
    std::size_t size = 0;
};

void BadPointers() {
    UnreadablePage page;
    const void* const bad[] = {reinterpret_cast<const void*>(0x10), page.Address()};
    const auto name = Name("badpointers");
    const GuestMqAttr small{0, 3, 16, 0, {}};
    const int queue = kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &small);
    Require(queue >= 0);
    char buffer[16]{};
    unsigned priority = 0;
    for (const void* pointer : bad) {
        Require(kmq_timedsend_nid_postfix(queue, "bad", 3, 1, nullptr) == 0);
        Require(kmq_timedreceive_nid_postfix(queue, const_cast<char*>(static_cast<const char*>(pointer)), sizeof(buffer), &priority, nullptr) == -1 && Error() == Efault);
    }
    for (const void* pointer : bad) {
        Require(kmq_timedsend_nid_postfix(queue, "bad", 3, 1, nullptr) == 0);
        Require(kmq_timedreceive_nid_postfix(queue, buffer, sizeof(buffer), const_cast<unsigned*>(static_cast<const unsigned*>(pointer)), nullptr) == -1 && Error() == Efault);
    }
    for (const void* pointer : bad) {
        const auto badTime = static_cast<const GuestTimespec*>(pointer);
        Require(kmq_timedsend_nid_postfix(queue, static_cast<const char*>(pointer), 3, 1, nullptr) == -1 && Error() == Efault);
        Require(kmq_timedsend_nid_postfix(queue, "bad", 3, 1, badTime) == -1 && Error() == Efault);
        Require(kmq_timedsend_nid_postfix(queue, "bad", 3, 64, badTime) == -1 && Error() == Efault);
        Require(kmq_timedsend_nid_postfix(queue, "0123456789abcdefg", 17, 1, badTime) == -1 && Error() == Efault);
        Require(kmq_timedsend_nid_postfix(queue, static_cast<const char*>(pointer), 3, 64, nullptr) == -1 && Error() == Einval);
        Require(kmq_timedsend_nid_postfix(queue, static_cast<const char*>(pointer), 17, 1, nullptr) == -1 && Error() == Emsgsize);
        Require(kmq_timedsend_nid_postfix(queue, "ok", 2, 1, nullptr) == 0);
        Require(kmq_timedreceive_nid_postfix(queue, buffer, sizeof(buffer), nullptr, badTime) == -1 && Error() == Efault);
        Require(kmq_timedreceive_nid_postfix(queue, buffer, 8, nullptr, badTime) == -1 && Error() == Efault);
        Require(kmq_timedreceive_nid_postfix(queue, static_cast<char*>(const_cast<void*>(pointer)), 8, nullptr, nullptr) == -1 && Error() == Emsgsize);
        Require(Receive(queue) == "ok");
    }
    Require(kmq_timedsend_nid_postfix(queue, nullptr, 0, 1, nullptr) == 0);
    Require(Receive(queue).empty());
    for (const void* pointer : bad) {
        const auto badAttr = static_cast<const GuestMqAttr*>(pointer);
        auto badOut = const_cast<GuestMqAttr*>(badAttr);
        const auto createBad = Name("badcreate");
        Require(kmq_open_nid_postfix(createBad.c_str(), ORdwr | OCreat, 0600, badAttr) == -1 && Error() == Efault);
        const int opened = kmq_open_nid_postfix(name.c_str(), ORdwr, 0600, badAttr);
        Require(opened >= 0 && close_nid_postfix(opened) == 0);
        Require(kmq_setattr_nid_postfix(queue, badAttr, nullptr) == -1 && Error() == Efault);
        Require(kmq_setattr_nid_postfix(queue, nullptr, badOut) == -1 && Error() == Efault);
        Require(kmq_setattr_nid_postfix(queue, badAttr, badOut) == -1 && Error() == Efault);
    }
    const GuestMqAttr on{ONonblock, 0, 0, 0, {}};
    Require(kmq_setattr_nid_postfix(queue, &on, const_cast<GuestMqAttr*>(static_cast<const GuestMqAttr*>(bad[0]))) == -1 && Error() == Efault);
    GuestMqAttr check{};
    Require(kmq_setattr_nid_postfix(queue, nullptr, &check) == 0);
    Require((check.flags & ONonblock) == 0);
    Require(close_nid_postfix(queue) == 0 && kmq_unlink_nid_postfix(name.c_str()) == 0);
}

}

int main() {
    OpenRules();
    BadPointers();
    Messages();
    DeadlineConversion();
    MonotonicWaitIgnoresRealtimeSteps();
    MonotonicWaitChunksLongDeadlines();
    ProductionWaitUsesMonotonicClock();
    Waiting();
    Access();
    NullBuffer();
    FarDeadline();
    ConcurrentSetattr();
    std::puts("message queues passed");
    return 0;
}
