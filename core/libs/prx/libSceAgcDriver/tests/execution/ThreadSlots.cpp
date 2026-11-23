#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {

using AgcDriver::DriverDetail::DrawFinisher;
using AgcDriver::DriverDetail::DrawPipeline;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TValue, typename TTag>
void SlotStaysOnItsThread(TValue value, const char* what) {
    using Slot = HostThreadSlot<TValue, TTag>;
    Require(Slot::Get() == TValue{}, what);
    Slot::Set(value);
    TValue seenElsewhere = value;
    TValue seenAfterExit = value;
    std::thread other([&] {
        seenElsewhere = Slot::Get();
        Slot::Set(value);
    });
    other.join();
    std::thread later([&] { seenAfterExit = Slot::Get(); });
    later.join();
    Require(seenElsewhere == TValue{}, what);
    Require(seenAfterExit == TValue{}, what);
    Require(Slot::Get() == value, what);
    Slot::Set(TValue{});
    Require(Slot::Get() == TValue{}, what);
}

struct FlagsTag {};
struct BoolTag {};
struct CounterTag {};
struct TimeTag {};
struct PointerTag {};
struct ReusedTag {};

void SlotKindsStayOnTheirThreads() {
    int object = 0;
    SlotStaysOnItsThread<std::uint8_t, FlagsTag>(0x05u, "a flag byte slot is per thread");
    SlotStaysOnItsThread<bool, BoolTag>(true, "a bool slot is per thread");
    SlotStaysOnItsThread<std::uint32_t, CounterTag>(0xfffffffeu, "a 32-bit counter slot is per thread");
    SlotStaysOnItsThread<std::chrono::steady_clock::time_point, TimeTag>(std::chrono::steady_clock::time_point{} + std::chrono::nanoseconds(123456789), "a time point slot is per thread");
    SlotStaysOnItsThread<int*, PointerTag>(&object, "a pointer slot is per thread");
}

void FreedIndexStartsAtZeroOnEveryThread() {
#ifdef _WIN32
    const DWORD index = TlsAlloc();
    Require(index != TLS_OUT_OF_INDEXES, "a thread slot index is available");
    std::promise<void> heldValue;
    std::promise<void> reused;
    std::promise<std::uint64_t> seen;
    auto reusedFuture = reused.get_future();
    std::thread bystander([&] {
        TlsSetValue(index, reinterpret_cast<void*>(std::uintptr_t{0x77}));
        heldValue.set_value();
        reusedFuture.wait();
        seen.set_value(HostThreadSlot<std::uint64_t, ReusedTag>::Get());
    });
    heldValue.get_future().wait();
    TlsFree(index);
    HostThreadSlot<std::uint64_t, ReusedTag>::Set(0x1234u);
    reused.set_value();
    Require(seen.get_future().get() == 0, "a slot allocated after a free is zero on a thread that held a value in the freed index");
    bystander.join();
    Require(HostThreadSlot<std::uint64_t, ReusedTag>::Get() == 0x1234u, "a slot allocated after a free keeps its own value on the allocating thread");
    HostThreadSlot<std::uint64_t, ReusedTag>::Set(0);
#endif
}

struct Counted {
    inline static std::atomic<int> alive{0};
    std::array<std::uint64_t, 4096> words{};
    std::uint64_t last = 0;
    Counted() { alive.fetch_add(1); }
    ~Counted() { alive.fetch_sub(1); }
};

struct CountedTag {};

void ScratchObjectsAreOwnedByTheirThread() {
    auto& mine = AgcDriver::ThreadScratch<Counted, CountedTag>();
    Require(&mine == &AgcDriver::ThreadScratch<Counted, CountedTag>(), "a thread gets the same scratch object every time");
    Require(mine.words[17] == 0 && mine.last == 0, "a scratch object starts zeroed");
    mine.words[17] = 5;
    mine.last = 9;
    const Counted* other = nullptr;
    std::uint64_t otherWord = 1;
    std::uint64_t otherLast = 1;
    std::thread([&] {
        auto& theirs = AgcDriver::ThreadScratch<Counted, CountedTag>();
        other = &theirs;
        otherWord = theirs.words[17];
        otherLast = theirs.last;
        theirs.words[17] = 7;
    }).join();
    Require(other != nullptr && other != &mine, "another thread gets its own scratch object");
    Require(otherWord == 0 && otherLast == 0, "another thread's scratch object starts zeroed");
    Require(mine.words[17] == 5 && mine.last == 9, "another thread leaves this thread's scratch object alone");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (Counted::alive.load() != 1 && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(Counted::alive.load() == 1, "a thread's scratch object is destroyed when the thread exits");
    std::uint64_t fresh = 1;
    std::thread([&] { fresh = AgcDriver::ThreadScratch<Counted, CountedTag>().words[17]; }).join();
    Require(fresh == 0, "a thread started after another exited gets a fresh scratch object");
}

void ActiveStaysOnItsThread() {
    Require(!DrawPipeline::Active(), "a thread starts with no active draw pipeline");
    DrawPipeline::Active() = true;
    bool seenElsewhere = true;
    bool seenAfterExit = true;
    std::thread other([&] {
        seenElsewhere = DrawPipeline::Active();
        DrawPipeline::Active() = true;
    });
    other.join();
    std::thread later([&] { seenAfterExit = DrawPipeline::Active(); });
    later.join();
    Require(!seenElsewhere, "another thread does not see this thread's active draw pipeline");
    Require(!seenAfterExit, "a thread started after another exited starts with no active draw pipeline");
    Require(DrawPipeline::Active(), "another thread leaves this thread's active draw pipeline alone");
    Require(!DrawFinisher::OnFinisherThread(), "an active draw pipeline does not make a thread the finisher");
    DrawPipeline::Active() = false;
    Require(!DrawPipeline::Active(), "an active draw pipeline can be cleared");
}

void FinisherFlagsStayOnTheirThreads() {
    auto& finisher = DrawFinisher::Get();
    DrawPipeline::Active() = true;
    bool onFinisher = false;
    bool activeOnFinisher = true;
    finisher.Enqueue([&] {
        onFinisher = DrawFinisher::OnFinisherThread();
        activeOnFinisher = DrawPipeline::Active();
    });
    finisher.Drain();
    Require(onFinisher, "the finisher thread knows it is the finisher");
    Require(!activeOnFinisher, "the finisher thread does not inherit the enqueuing thread's active draw pipeline");
    Require(!DrawFinisher::OnFinisherThread(), "the enqueuing thread is not the finisher");
    Require(DrawPipeline::Active(), "the finisher leaves the enqueuing thread's active draw pipeline alone");
    DrawPipeline::Active() = false;
}

void ProducerFlagStaysOnTheEnqueuingThread() {
    auto& finisher = DrawFinisher::Get();
    std::promise<void> release;
    const auto gate = release.get_future().share();
    DrawFinisher::Pending held;
    held.key = 0x5eed1234u;
    held.unknown = false;
    finisher.Enqueue([gate] { gate.wait(); }, held);
    const bool producerSees = finisher.PendingKey(0x5eed1234u, 0);
    bool otherSees = true;
    std::thread other([&] { otherSees = finisher.PendingKey(0x5eed1234u, 0); });
    other.join();
    release.set_value();
    finisher.Drain();
    Require(producerSees, "the thread that enqueued sees its draw held by the finisher");
    Require(!otherSees, "a thread that never enqueued does not see the finisher's held draw");
}

void PerformanceContextStaysOnItsThread() {
    AgcDriver::FrameTiming outer(1, true);
    AgcDriver::FrameTiming inner(2, true);
    AgcDriver::FrameTiming theirs(3, true);
    Require(AgcDriver::PerformanceContext::Current() == nullptr, "a thread starts with no timing context");
    {
        const AgcDriver::PerformanceContext context(&outer);
        Require(AgcDriver::PerformanceContext::Current() == &outer, "a timing context is the thread's current one");
        AgcDriver::FrameTiming* seenElsewhere = &outer;
        AgcDriver::FrameTiming* seenAfterExit = &outer;
        std::thread other([&] {
            seenElsewhere = AgcDriver::PerformanceContext::Current();
            const AgcDriver::PerformanceContext own(&theirs);
        });
        other.join();
        std::thread later([&] { seenAfterExit = AgcDriver::PerformanceContext::Current(); });
        later.join();
        Require(seenElsewhere == nullptr, "another thread does not see this thread's timing context");
        Require(seenAfterExit == nullptr, "a thread started after another exited has no timing context");
        Require(AgcDriver::PerformanceContext::Current() == &outer, "another thread's timing context leaves this thread's alone");
        {
            const AgcDriver::PerformanceContext nested(&inner);
            Require(AgcDriver::PerformanceContext::Current() == &inner, "a nested timing context replaces the current one");
        }
        Require(AgcDriver::PerformanceContext::Current() == &outer, "the end of a nested timing context restores the outer one");
    }
    Require(AgcDriver::PerformanceContext::Current() == nullptr, "the end of a timing context restores none");
}

}

int main() {
    try {
        SlotKindsStayOnTheirThreads();
        FreedIndexStartsAtZeroOnEveryThread();
        ScratchObjectsAreOwnedByTheirThread();
        ActiveStaysOnItsThread();
        FinisherFlagsStayOnTheirThreads();
        ProducerFlagStaysOnTheEnqueuingThread();
        PerformanceContextStaysOnItsThread();
        std::cout << "thread slot tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
