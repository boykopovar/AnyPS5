#include "SceTypes.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

struct GuestXtime {
    std::int64_t sec;
    std::int64_t nsec;
};

struct PadTarget {
    std::atomic<int>* order;
    std::atomic<bool>* destroyed;
    int beforeRelease;
    int afterRelease;
    unsigned exitCode;
};

struct GuestTypeInfo {
    const void* vtable;
    const char* name;
};

struct GuestSystemError {
    const void* vtable;
    const char* message;
    int reserved;
    int code;
    const void* category;
};

using GuestTerminate = void (APS5_VABI*)();

struct TestPad {
    unsigned (APS5_VABI* const* vtable)(TestPad*);
    void* condition;
    void* mutex;
    bool started;
    PadTarget target;
};

extern "C" {
void APS5_VABI _ZNSt4_PadC2Ev_nid_postfix(TestPad* pad);
void APS5_VABI _ZNSt4_PadD2Ev_nid_postfix(TestPad* pad);
void APS5_VABI _ZNSt4_Pad7_LaunchEPP7pthread_nid_postfix(TestPad* pad, Pthread* thread);
void APS5_VABI _ZNSt4_Pad8_ReleaseEv_nid_postfix(TestPad* pad);
int APS5_VABI _Thrd_join_nid_postfix(Pthread thread, int* code);
int APS5_VABI _Thrd_sleep_nid_postfix(const GuestXtime* target, GuestXtime* remaining);
void APS5_VABI _Lock_shared_ptr_spin_lock_nid_postfix(void);
void APS5_VABI _Unlock_shared_ptr_spin_lock_nid_postfix(void);
int APS5_VABI sceAmprWcstombsS(std::size_t* result, char* destination, std::size_t capacity, const std::uint16_t* source, std::size_t limit);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI pthread_detach_nid_postfix(Pthread thread);
int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp);
int APS5_VABI _Cnd_signal_nid_postfix(void** handle);
int APS5_VABI _Cnd_init_with_name_nid_postfix(void** handle, const char* name);
int APS5_VABI _Cnd_init_nid_postfix(void** handle);
void APS5_VABI _Cnd_destroy_nid_postfix(void** handle);
int APS5_VABI _Cnd_wait_nid_postfix(void** condition, void** mutex);
int APS5_VABI _Cnd_broadcast_nid_postfix(void** handle);
int APS5_VABI _Mtx_init_nid_postfix(void** handle, int type);
void APS5_VABI _Mtx_destroy_nid_postfix(void** handle);
int APS5_VABI _Mtx_lock_nid_postfix(void** handle);
int APS5_VABI _Mtx_unlock_nid_postfix(void** handle);
GuestTerminate APS5_VABI _ZSt13set_terminatePFvvE_nid_postfix(GuestTerminate handler);
const GuestTypeInfo* APS5_VABI __cxa_current_exception_type_nid_postfix();
void* APS5_VABI __cxa_current_primary_exception_nid_postfix();
LibcException::Header* APS5_VABI __cxa_init_primary_exception_nid_postfix(void* object, std::type_info* type, void (*destructor)(void*));
[[noreturn]] void APS5_VABI __cxa_call_unexpected_nid_postfix(void* unwindException);
extern const unsigned char _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix[];
}

static constexpr int ThrdSuccess = 0;
static constexpr int ThrdError = 4;
static constexpr int GuestEbusy = 16;
static constexpr int GuestEinval = 22;
static constexpr int GuestErange = 34;
static constexpr int GuestEilseq = 86;
static constexpr std::int64_t NanosPerSecond = 1000000000;
static constexpr auto Failed = static_cast<std::size_t>(-1);

#define Require(value) RequireAt((value), __LINE__)

static void RequireAt(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "requirement failed at line %d\n", line);
    std::abort();
}

static std::int64_t RealtimeNanos() {
    KernelTimespec now{};
    Require(clock_gettime_nid_postfix(0, &now) == 0);
    return now.tv_sec * NanosPerSecond + now.tv_nsec;
}

static GuestXtime XtimeAfter(std::int64_t nanos) {
    const auto target = RealtimeNanos() + nanos;
    return {target / NanosPerSecond, target % NanosPerSecond};
}

static void* APS5_VABI ReturnArgument(void* arg) {
    return arg;
}

static void* APS5_VABI WaitForRelease(void* arg) {
    auto* release = static_cast<std::atomic<bool>*>(arg);
    while (!release->load()) std::this_thread::yield();
    return nullptr;
}

struct SpinContext {
    std::atomic<int> inside{0};
    std::atomic<bool> overlapped{false};
    int counter = 0;
};

static constexpr int SpinThreads = 4;
static constexpr int SpinIterations = 20000;

static void* APS5_VABI SpinWorker(void* arg) {
    auto& context = *static_cast<SpinContext*>(arg);
    for (int iteration = 0; iteration < SpinIterations; ++iteration) {
        _Lock_shared_ptr_spin_lock_nid_postfix();
        if (context.inside.fetch_add(1) != 0) context.overlapped = true;
        ++context.counter;
        context.inside.fetch_sub(1);
        _Unlock_shared_ptr_spin_lock_nid_postfix();
    }
    return nullptr;
}

static Pthread Start(PthreadEntry entry, void* arg) {
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, entry, arg, nullptr) == 0);
    return thread;
}

static void TestThreadJoin() {
    int code = 0;
    Require(_Thrd_join_nid_postfix(Start(ReturnArgument, reinterpret_cast<void*>(std::intptr_t{42})), &code) == ThrdSuccess);
    Require(code == 42);
    code = 0;
    Require(_Thrd_join_nid_postfix(Start(ReturnArgument, reinterpret_cast<void*>(std::intptr_t{-7})), &code) == ThrdSuccess);
    Require(code == -7);
    code = 5;
    Require(_Thrd_join_nid_postfix(Start(ReturnArgument, reinterpret_cast<void*>(std::intptr_t{0x100000003})), &code) == ThrdSuccess);
    Require(code == 3);
    Require(_Thrd_join_nid_postfix(Start(ReturnArgument, nullptr), nullptr) == ThrdSuccess);

    static std::atomic<bool> release{false};
    const auto detached = Start(WaitForRelease, &release);
    Require(pthread_detach_nid_postfix(detached) == 0);
    code = 9;
    Require(_Thrd_join_nid_postfix(detached, &code) == ThrdError);
    Require(code == 9);
    release = true;
}

static void TestThreadSleep() {
    const auto target = XtimeAfter(50000000);
    Require(_Thrd_sleep_nid_postfix(&target, nullptr) == 0);
    Require(RealtimeNanos() >= target.sec * NanosPerSecond + target.nsec);

    const auto past = XtimeAfter(-NanosPerSecond);
    const auto before = RealtimeNanos();
    Require(_Thrd_sleep_nid_postfix(&past, nullptr) == 0);
    Require(RealtimeNanos() - before < NanosPerSecond / 2);

    const auto carried = XtimeAfter(30000000);
    const GuestXtime unnormalized{carried.sec - 2, carried.nsec + 2 * NanosPerSecond};
    Require(_Thrd_sleep_nid_postfix(&unnormalized, nullptr) == 0);
    Require(RealtimeNanos() >= carried.sec * NanosPerSecond + carried.nsec);

    const auto borrowed = XtimeAfter(20000000);
    const GuestXtime negative{borrowed.sec + 1, borrowed.nsec - NanosPerSecond};
    Require(_Thrd_sleep_nid_postfix(&negative, nullptr) == 0);
    Require(RealtimeNanos() >= borrowed.sec * NanosPerSecond + borrowed.nsec);

#ifdef _WIN32
    for (const auto duration : {100000LL, 2000000LL}) {
        std::array<std::chrono::nanoseconds, 31> samples{};
        for (auto& sample : samples) {
            const auto start = std::chrono::steady_clock::now();
            const auto shortTarget = XtimeAfter(duration);
            Require(_Thrd_sleep_nid_postfix(&shortTarget, nullptr) == ThrdSuccess);
            sample = std::chrono::steady_clock::now() - start;
            Require(RealtimeNanos() >= shortTarget.sec * NanosPerSecond + shortTarget.nsec);
        }
        std::sort(samples.begin(), samples.end());
        const auto median = samples[samples.size() / 2];
        std::fprintf(stderr, "_Thrd_sleep(%lld ns) median: %lld ns\n", duration, static_cast<long long>(median.count()));
        Require(median < std::chrono::milliseconds(12));
    }
#endif

    bool rejected = false;
    try {
        _Thrd_sleep_nid_postfix(nullptr, nullptr);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected);
}

static void TestSpinLock() {
    SpinContext context;
    std::array<Pthread, SpinThreads> threads{};
    for (auto& thread : threads) thread = Start(SpinWorker, &context);
    for (auto thread : threads) Require(_Thrd_join_nid_postfix(thread, nullptr) == ThrdSuccess);
    Require(!context.overlapped);
    Require(context.counter == SpinThreads * SpinIterations);
    _Lock_shared_ptr_spin_lock_nid_postfix();
    _Unlock_shared_ptr_spin_lock_nid_postfix();
}

static void TestWcstombs() {
    const std::uint16_t text[] = {u'a', u'b', u'c', 0};
    const std::uint16_t wide[] = {u'a', 0x100, 0};
    std::size_t result = 0;
    char buffer[8];

    std::memset(buffer, 'x', sizeof(buffer));
    Require(sceAmprWcstombsS(&result, buffer, sizeof(buffer), text, sizeof(buffer)) == 0);
    Require(result == 3 && std::strcmp(buffer, "abc") == 0);

    std::memset(buffer, 'x', sizeof(buffer));
    Require(sceAmprWcstombsS(&result, buffer, sizeof(buffer), text, 2) == 0);
    Require(result == 2 && std::strcmp(buffer, "ab") == 0);

    result = 0;
    Require(sceAmprWcstombsS(&result, nullptr, 0, text, 0) == 0);
    Require(result == 3);

    std::memset(buffer, 'x', sizeof(buffer));
    Require(sceAmprWcstombsS(&result, buffer, 3, text, 3) == GuestErange);
    Require(result == Failed && buffer[0] == '\0');

    result = 0;
    Require(sceAmprWcstombsS(&result, nullptr, 4, text, 4) == GuestEinval);
    Require(result == Failed);

    std::memset(buffer, 'x', sizeof(buffer));
    Require(sceAmprWcstombsS(&result, buffer, sizeof(buffer), nullptr, sizeof(buffer)) == GuestEinval);
    Require(result == Failed && buffer[0] == '\0');

    std::memset(buffer, 'x', sizeof(buffer));
    Require(sceAmprWcstombsS(nullptr, buffer, sizeof(buffer), text, sizeof(buffer)) == GuestEinval);
    Require(buffer[0] == '\0');

    std::memset(buffer, 'x', sizeof(buffer));
    Require(sceAmprWcstombsS(&result, buffer, 0, text, 4) == GuestEinval);
    Require(result == Failed && buffer[0] == 'x');

    std::memset(buffer, 'x', sizeof(buffer));
    Require(sceAmprWcstombsS(&result, buffer, sizeof(buffer), wide, sizeof(buffer)) == 0);
    Require(result == 3 && std::memcmp(buffer, "a\xc4\x80\0x", 5) == 0);

    Require(sceAmprWcstombsS(&result, nullptr, 0, wide, 0) == 0);
    Require(result == 3);
}

template<typename TException, typename TCall>
static void RequireThrows(TCall call) {
    try {
        call();
    } catch (const TException&) {
        return;
    }
    std::abort();
}

struct SignalContext {
    void* condition = nullptr;
    void* mutex = nullptr;
    int waiting = 0;
    int tokens = 0;
    int consumed = 0;
    int wakeups = 0;
};

static void Lock(SignalContext& context) {
    Require(_Mtx_lock_nid_postfix(&context.mutex) == ThrdSuccess);
}

static void Unlock(SignalContext& context) {
    Require(_Mtx_unlock_nid_postfix(&context.mutex) == ThrdSuccess);
}

static void* APS5_VABI ConsumeToken(void* arg) {
    auto& context = *static_cast<SignalContext*>(arg);
    Lock(context);
    ++context.waiting;
    while (context.tokens == 0) {
        Require(_Cnd_wait_nid_postfix(&context.condition, &context.mutex) == ThrdSuccess);
        ++context.wakeups;
    }
    --context.tokens;
    ++context.consumed;
    Unlock(context);
    return nullptr;
}

template<typename TPredicate>
static void WaitUntil(SignalContext& context, TPredicate predicate) {
    for (;;) {
        Lock(context);
        const bool done = predicate();
        Unlock(context);
        if (done) return;
        std::this_thread::yield();
    }
}

static void Signal(SignalContext& context) {
    Lock(context);
    ++context.tokens;
    Require(_Cnd_signal_nid_postfix(&context.condition) == ThrdSuccess);
    Unlock(context);
}

static void TestConditionSignal() {
    SignalContext context;
    context.condition = reinterpret_cast<void*>(std::uintptr_t{1});
    Require(_Cnd_init_with_name_nid_postfix(&context.condition, "AmprSignal") == ThrdSuccess && context.condition != nullptr);
    Require(_Mtx_init_nid_postfix(&context.mutex, 0) == ThrdSuccess);
    Require(_Cnd_signal_nid_postfix(&context.condition) == ThrdSuccess);

    const auto single = Start(ConsumeToken, &context);
    WaitUntil(context, [&] { return context.waiting == 1; });
    Signal(context);
    Require(_Thrd_join_nid_postfix(single, nullptr) == ThrdSuccess);
    Require(context.consumed == 1 && context.tokens == 0);

    context.waiting = 0;
    context.consumed = 0;
    context.wakeups = 0;
    const std::array<Pthread, 2> pair{Start(ConsumeToken, &context), Start(ConsumeToken, &context)};
    WaitUntil(context, [&] { return context.waiting == 2; });
    Signal(context);
    WaitUntil(context, [&] { return context.consumed == 1; });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Lock(context);
    Require(context.consumed == 1 && context.wakeups == 1 && context.tokens == 0);
    Unlock(context);
    Signal(context);
    for (auto thread : pair) Require(_Thrd_join_nid_postfix(thread, nullptr) == ThrdSuccess);
    Require(context.consumed == 2 && context.tokens == 0);
    Require(_Cnd_broadcast_nid_postfix(&context.condition) == ThrdSuccess);

    RequireThrows<std::invalid_argument>([] { _Cnd_signal_nid_postfix(nullptr); });
    void* empty = nullptr;
    RequireThrows<std::invalid_argument>([&] { _Cnd_signal_nid_postfix(&empty); });
    RequireThrows<std::invalid_argument>([&] { _Cnd_signal_nid_postfix(&context.mutex); });
    RequireThrows<std::invalid_argument>([] { _Cnd_init_with_name_nid_postfix(nullptr, "AmprSignal"); });
    _Cnd_destroy_nid_postfix(&context.condition);
    Require(context.condition == nullptr);
    RequireThrows<std::invalid_argument>([&] { _Cnd_signal_nid_postfix(&context.condition); });

    void* unnamed = nullptr;
    Require(_Cnd_init_with_name_nid_postfix(&unnamed, nullptr) == ThrdSuccess && unnamed != nullptr);
    Require(_Cnd_signal_nid_postfix(&unnamed) == ThrdSuccess);
    _Cnd_destroy_nid_postfix(&unnamed);
    void* plain = nullptr;
    Require(_Cnd_init_nid_postfix(&plain) == ThrdSuccess);
    Require(_Cnd_signal_nid_postfix(&plain) == ThrdSuccess);
    _Cnd_destroy_nid_postfix(&plain);
    _Mtx_destroy_nid_postfix(&context.mutex);
}

static unsigned APS5_VABI PadGo(TestPad* pad) {
    const auto local = pad->target;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    local.order->store(local.beforeRelease);
    _ZNSt4_Pad8_ReleaseEv_nid_postfix(pad);
    while (!local.destroyed->load()) std::this_thread::yield();
    local.order->store(local.afterRelease);
    return local.exitCode;
}

static constexpr std::array<unsigned (APS5_VABI*)(TestPad*), 1> PadVtable{PadGo};

template <typename TException, typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (const TException&) {
        return true;
    }
    return false;
}

static void ConstructPad(TestPad& pad, std::atomic<int>& order, std::atomic<bool>& destroyed, int index, unsigned exitCode) {
    std::memset(&pad, 0xCD, sizeof(pad));
    pad.vtable = PadVtable.data();
    _ZNSt4_PadC2Ev_nid_postfix(&pad);
    Require(pad.vtable == PadVtable.data());
    Require(pad.condition && pad.mutex && !pad.started);
    pad.target = {&order, &destroyed, 2 * index + 1, 2 * index + 2, exitCode};
}

static void TestPadLaunch() {
    std::atomic<int> order{0};
    std::atomic<bool> destroyed{false};
    TestPad pad;
    ConstructPad(pad, order, destroyed, 0, 0);
    Pthread thread = nullptr;
    _ZNSt4_Pad7_LaunchEPP7pthread_nid_postfix(&pad, &thread);
    Require(thread != nullptr);
    Require(pad.started);
    Require(order.load() == 1);
    _ZNSt4_PadD2Ev_nid_postfix(&pad);
    Require(!pad.condition && !pad.mutex);
    Require(order.load() == 1);
    destroyed = true;
    int code = -1;
    Require(_Thrd_join_nid_postfix(thread, &code) == ThrdSuccess);
    Require(code == 0);
    Require(order.load() == 2);
}

static void TestPadLaunchMany() {
    constexpr int Count = 4;
    std::array<std::atomic<int>, Count> orders{};
    std::atomic<bool> destroyed{false};
    std::array<TestPad, Count> pads{};
    std::array<Pthread, Count> threads{};
    for (int index = 0; index < Count; ++index) {
        ConstructPad(pads[index], orders[index], destroyed, index, 100u + index);
        _ZNSt4_Pad7_LaunchEPP7pthread_nid_postfix(&pads[index], &threads[index]);
        Require(orders[index].load() == 2 * index + 1);
        _ZNSt4_PadD2Ev_nid_postfix(&pads[index]);
    }
    destroyed = true;
    for (int index = 0; index < Count; ++index) {
        int code = 0;
        Require(_Thrd_join_nid_postfix(threads[index], &code) == ThrdSuccess);
        Require(code == 100 + index);
        Require(orders[index].load() == 2 * index + 2);
    }
}

static unsigned APS5_VABI PadGoImmediate(TestPad* pad) {
    const auto local = pad->target;
    local.order->store(local.beforeRelease);
    _ZNSt4_Pad8_ReleaseEv_nid_postfix(pad);
    while (!local.destroyed->load()) std::this_thread::yield();
    local.order->store(local.afterRelease);
    return local.exitCode;
}

static void TestPadConcurrentLaunch() {
    constexpr int Count = 4;
    constexpr std::array<unsigned (APS5_VABI*)(TestPad*), 1> immediateVtable{PadGoImmediate};
    std::atomic<int> ready{0};
    std::array<std::thread, Count> parents;
    for (int index = 0; index < Count; ++index) {
        parents[index] = std::thread([&, index] {
            std::atomic<int> order{0};
            std::atomic<bool> destroyed{false};
            TestPad pad;
            ConstructPad(pad, order, destroyed, index, 0x80000000u + index);
            pad.vtable = immediateVtable.data();
            ready.fetch_add(1);
            while (ready.load() != Count) std::this_thread::yield();
            Pthread thread = nullptr;
            _ZNSt4_Pad7_LaunchEPP7pthread_nid_postfix(&pad, &thread);
            Require(thread != nullptr && pad.started && order.load() == 2 * index + 1);
            _ZNSt4_PadD2Ev_nid_postfix(&pad);
            std::memset(&pad, 0xcd, sizeof(pad));
            destroyed = true;
            int code = 0;
            Require(_Thrd_join_nid_postfix(thread, &code) == ThrdSuccess);
            Require(static_cast<unsigned>(code) == 0x80000000u + index);
            Require(order.load() == 2 * index + 2);
        });
    }
    for (auto& parent : parents) parent.join();
}

struct ForeignDestroy {
    TestPad* pad;
    bool rejected;
};

static void* APS5_VABI DestroyPad(void* arg) {
    auto& context = *static_cast<ForeignDestroy*>(arg);
    context.rejected = Throws<std::logic_error>([&] { _ZNSt4_PadD2Ev_nid_postfix(context.pad); });
    return nullptr;
}

#ifndef _WIN32
static void APS5_VABI ExitWithSystemErrorCode() {
    const auto* type = __cxa_current_exception_type_nid_postfix();
    const auto* error = static_cast<const GuestSystemError*>(__cxa_current_primary_exception_nid_postfix());
    _exit(type && error && std::strcmp(type->name, "St12system_error") == 0 ? error->code : 1);
}

template <typename TCall>
static int TerminateExitCode(GuestTerminate handler, TCall call) {
    const pid_t child = fork();
    Require(child >= 0);
    if (child == 0) {
        _ZSt13set_terminatePFvvE_nid_postfix(handler);
        call();
        _exit(2);
    }
    int status = 0;
    Require(waitpid(child, &status, 0) == child);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static const GuestTypeInfo UnexpectedType{_ZTVN10__cxxabiv117__class_type_infoE_nid_postfix + 16, "10Unexpected"};

static void APS5_VABI ExitWithCaughtType() {
    const auto* type = __cxa_current_exception_type_nid_postfix();
    _exit(type == &UnexpectedType ? 42 : type == nullptr ? 43 : 1);
}

static void TestCallUnexpected() {
    Require(TerminateExitCode(ExitWithCaughtType, [] {
        void* object = __cxa_allocate_exception_nid_postfix(sizeof(int));
        auto* type = reinterpret_cast<std::type_info*>(const_cast<GuestTypeInfo*>(&UnexpectedType));
        LibcException::Header* header = __cxa_init_primary_exception_nid_postfix(object, type, nullptr);
        __cxa_call_unexpected_nid_postfix(&header->unwind);
    }) == 42);
    Require(TerminateExitCode(ExitWithCaughtType, [] { __cxa_call_unexpected_nid_postfix(nullptr); }) == 43);
}
#endif

static void TestPadErrors() {
    Require(Throws<std::invalid_argument>([] { _ZNSt4_PadC2Ev_nid_postfix(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZNSt4_PadD2Ev_nid_postfix(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZNSt4_Pad8_ReleaseEv_nid_postfix(nullptr); }));
    Pthread thread = nullptr;
    Require(Throws<std::invalid_argument>([&] { _ZNSt4_Pad7_LaunchEPP7pthread_nid_postfix(nullptr, &thread); }));

    std::atomic<int> order{0};
    std::atomic<bool> destroyed{true};
    TestPad pad;
    ConstructPad(pad, order, destroyed, 0, 0);
    Require(Throws<std::invalid_argument>([&] { _ZNSt4_Pad7_LaunchEPP7pthread_nid_postfix(&pad, nullptr); }));
    Require(!pad.started && order.load() == 0);
#ifndef _WIN32
    Require(TerminateExitCode(ExitWithSystemErrorCode, [&] { _ZNSt4_Pad8_ReleaseEv_nid_postfix(&pad); }) == GuestEbusy);
#endif
    Require(!pad.started);

    ForeignDestroy context{&pad, false};
    Require(_Thrd_join_nid_postfix(Start(DestroyPad, &context), nullptr) == ThrdSuccess);
    Require(context.rejected);
    Require(pad.condition && pad.mutex);
    _ZNSt4_PadD2Ev_nid_postfix(&pad);
    Require(!pad.condition && !pad.mutex);
    Require(Throws<std::invalid_argument>([&] { _ZNSt4_PadD2Ev_nid_postfix(&pad); }));
}

int main() {
    TestPadLaunch();
    TestPadLaunchMany();
    TestPadConcurrentLaunch();
    TestPadErrors();
    TestThreadJoin();
    TestThreadSleep();
    TestSpinLock();
    TestWcstombs();
    TestConditionSignal();
#ifndef _WIN32
    TestCallUnexpected();
#endif
    return 0;
}
