#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "rtc thread check failed at line %d\n", line);
        std::fflush(stderr);
        std::_Exit(1);
    }
}
#define Require(value) Check((value), __LINE__)

template<typename TFunction>
bool ThrowsInvalidArgument(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

constexpr auto construct = &_ZN3rtc6ThreadC1EPNS_12SocketServerE_nid_postfix;
constexpr auto destroy = &_ZN3rtc6ThreadD2Ev_nid_postfix;
constexpr auto postTask = &_ZN3rtc6Thread12PostTaskImplEN4absl12AnyInvocableIFvvOEEERKN6webrtc13TaskQueueBase14PostTaskTraitsERKNS5_8LocationE_nid_postfix;
constexpr auto postDelayedTask =
    &_ZN3rtc6Thread19PostDelayedTaskImplEN4absl12AnyInvocableIFvvOEEEN6webrtc9TimeDeltaERKNS5_13TaskQueueBase21PostDelayedTaskTraitsERKNS5_8LocationE_nid_postfix;
constexpr auto processMessages = &_ZN3rtc6Thread15ProcessMessagesEi_nid_postfix;
constexpr auto getDelay = &_ZN3rtc6Thread8GetDelayEv_nid_postfix;
constexpr auto start = &_ZN3rtc6Thread5StartEv_nid_postfix;
constexpr auto stop = &_ZN3rtc6Thread4StopEv_nid_postfix;
constexpr auto quit = &_ZN3rtc6Thread4QuitEv_nid_postfix;
constexpr auto restart = &_ZN3rtc6Thread7RestartEv_nid_postfix;
constexpr auto isQuitting = &_ZN3rtc6Thread10IsQuittingEv_nid_postfix;
constexpr auto blockingCall = &_ZN3rtc6Thread16BlockingCallImplENS_12FunctionViewIFvvEEERKN6webrtc8LocationE_nid_postfix;
constexpr auto isProcessing = &_ZN3rtc6Thread30IsProcessingMessagesForTestingEv_nid_postfix;
constexpr auto currentThread = &_ZN3rtc6Thread7CurrentEv_nid_postfix;
constexpr auto currentQueue = &_ZN6webrtc13TaskQueueBase7CurrentEv_nid_postfix;

struct SocketServer {
    void* const* vtable;
    void* networkBinder = nullptr;
    WebRtcNullSocketServer inner{};
    std::vector<void*> queues;
    std::atomic<int> waits{0};
    std::atomic<int> wakeUps{0};
    std::atomic<std::int64_t> lastWait{0};
    std::atomic<bool> lastProcessIo{false};
    std::atomic<bool> failWait{false};

    SocketServer();
    ~SocketServer() {
        reinterpret_cast<void (APS5_VABI*)(WebRtcNullSocketServer*)>(inner.vtable[0])(&inner);
    }
};

void APS5_VABI ServerUnexpected(SocketServer*) {
    Require(false);
}

void APS5_VABI ServerSetMessageQueue(SocketServer* server, void* queue) {
    server->queues.push_back(queue);
}

bool APS5_VABI ServerWait(SocketServer* server, std::int64_t maxWaitMicroseconds, bool processIo) {
    ++server->waits;
    server->lastWait = maxWaitMicroseconds;
    server->lastProcessIo = processIo;
    if (server->failWait) return false;
    return reinterpret_cast<bool (APS5_VABI*)(WebRtcNullSocketServer*, std::int64_t, bool)>(server->inner.vtable[4])(&server->inner, maxWaitMicroseconds, processIo);
}

void APS5_VABI ServerWakeUp(SocketServer* server) {
    ++server->wakeUps;
    reinterpret_cast<void (APS5_VABI*)(WebRtcNullSocketServer*)>(server->inner.vtable[5])(&server->inner);
}

void* const serverVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<void*>(&ServerUnexpected),
    reinterpret_cast<void*>(&ServerUnexpected),
    reinterpret_cast<void*>(&ServerUnexpected),
    reinterpret_cast<void*>(&ServerSetMessageQueue),
    reinterpret_cast<void*>(&ServerWait),
    reinterpret_cast<void*>(&ServerWakeUp),
};

SocketServer::SocketServer()
    : vtable(serverVtable + 2) {
    _ZN3rtc16NullSocketServerC1Ev_nid_postfix(&inner);
}

struct Record {
    std::atomic<int> runs{0};
    std::atomic<int> moves{0};
    std::atomic<int> disposals{0};
    std::thread::id thread;
    void* currentThreadDuringRun = nullptr;
    void* currentQueueDuringRun = nullptr;
    void* currentQueueDuringDisposal = nullptr;
    std::function<void()> body;
};

void APS5_VABI TaskManager(std::uint8_t operation, WebRtcAnyInvocableState* from, WebRtcAnyInvocableState* to) {
    Record* record = nullptr;
    std::memcpy(&record, from->storage, sizeof(record));
    if (operation == 0) {
        std::memcpy(to->storage, from->storage, sizeof(from->storage));
        ++record->moves;
    } else {
        Require(operation == 1);
        record->currentQueueDuringDisposal = currentQueue();
        ++record->disposals;
    }
}

void APS5_VABI TaskInvoker(WebRtcAnyInvocableState* state) {
    Record* record = nullptr;
    std::memcpy(&record, state->storage, sizeof(record));
    record->thread = std::this_thread::get_id();
    record->currentThreadDuringRun = currentThread();
    record->currentQueueDuringRun = currentQueue();
    ++record->runs;
    if (record->body) record->body();
}

struct GuestTask {
    WebRtcAnyInvocable invocable{};

    explicit GuestTask(Record& record) {
        std::memset(invocable.state.storage, 0, sizeof(invocable.state.storage));
        Record* pointer = &record;
        std::memcpy(invocable.state.storage, &pointer, sizeof(pointer));
        invocable.manager = &TaskManager;
        invocable.invoker = &TaskInvoker;
    }

    ~GuestTask() {
        invocable.manager(1, &invocable.state, &invocable.state);
    }
};

struct TestThread {
    alignas(16) unsigned char bytes[512];
    SocketServer server;
    bool alive = true;

    TestThread() {
        std::memset(bytes, 0xA5, sizeof(bytes));
        construct(bytes, &server);
    }

    ~TestThread() {
        if (alive) destroy(bytes);
    }

    void* get() {
        return bytes;
    }

    void Post(Record& record) {
        GuestTask task(record);
        postTask(get(), &task.invocable, nullptr, nullptr);
    }

    void PostDelayed(Record& record, std::int64_t microseconds) {
        GuestTask task(record);
        postDelayedTask(get(), &task.invocable, microseconds, nullptr, nullptr);
    }
};

std::atomic<int> heapFrees{0};
std::atomic<void*> lastFreed{nullptr};

struct Call {
    std::atomic<int>* calls;
    std::thread::id* thread;
};

void APS5_VABI RecordCall(void* context) {
    auto* call = static_cast<Call*>(context);
    if (call->thread) *call->thread = std::this_thread::get_id();
    if (call->calls) ++*call->calls;
}

void APS5_VABI Increment(void* context) {
    ++*static_cast<int*>(context);
}

void TestConstruction() {
    TestThread thread;
    for (std::size_t index = 8; index < sizeof(thread.bytes); ++index)
        if (index < 0x64 || index >= 0x70) Require(thread.bytes[index] == 0xA5);
    void* vtable = nullptr;
    std::memcpy(&vtable, thread.bytes, sizeof(vtable));
    Require(vtable != nullptr);
    std::uint32_t stopped = 1;
    std::memcpy(&stopped, thread.bytes + 0x64, sizeof(stopped));
    void* server = nullptr;
    std::memcpy(&server, thread.bytes + 0x68, sizeof(server));
    Require(stopped == 0 && server == &thread.server && _ZN3rtc6Thread12socketserverEv_nid_postfix(thread.get()) == &thread.server);
    Require(thread.server.queues == std::vector<void*>{thread.get()});
    Require(!isQuitting(thread.get()) && getDelay(thread.get()) == -1 && isProcessing(thread.get()));
    Require(thread.server.waits.load() == 0 && thread.server.wakeUps.load() == 0);
    destroy(thread.get());
    thread.alive = false;
    Require((thread.server.queues == std::vector<void*>{thread.get(), nullptr}));
    Require(thread.server.wakeUps.load() == 1);
}

void TestTasksOnTheCallingThread() {
    TestThread thread;
    Record first;
    Record second;
    Record third;
    std::vector<int> order;
    first.body = [&] { order.push_back(1); };
    second.body = [&] { order.push_back(2); };
    third.body = [&] { order.push_back(3); };
    thread.Post(first);
    Require(first.moves.load() == 1 && first.disposals.load() == 0 && first.runs.load() == 0);
    Require(thread.server.wakeUps.load() == 1 && getDelay(thread.get()) == 0);
    thread.Post(second);
    thread.Post(third);
    Require(thread.server.wakeUps.load() == 3);
    Require(processMessages(thread.get(), 60));
    Require((order == std::vector<int>{1, 2, 3}));
    for (Record* record : {&first, &second, &third}) {
        Require(record->runs.load() == 1 && record->disposals.load() == 1);
        Require(record->thread == std::this_thread::get_id());
    }
    Require(getDelay(thread.get()) == -1 && thread.server.lastProcessIo.load());
    const auto before = std::chrono::steady_clock::now();
    Require(processMessages(thread.get(), 40));
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before).count();
    Require(elapsed >= 30 && elapsed < 2000);

    Record moved;
    {
        GuestTask task(moved);
        postTask(thread.get(), &task.invocable, nullptr, nullptr);
        Require(task.invocable.invoker == nullptr && task.invocable.manager != &TaskManager);
    }
    Require(moved.disposals.load() == 0 && processMessages(thread.get(), 0) && moved.runs.load() == 1 && moved.disposals.load() == 1);

    Record slow;
    slow.body = [] { std::this_thread::sleep_for(std::chrono::milliseconds(40)); };
    thread.Post(slow);
    const int waits = thread.server.waits.load();
    Require(processMessages(thread.get(), 10) && slow.runs.load() == 1 && thread.server.waits.load() == waits);
}

void TestAWakeUpWithoutATask() {
    TestThread thread;
    std::thread waker([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ServerWakeUp(&thread.server);
    });
    Require(processMessages(thread.get(), 1000));
    waker.join();
    Require(thread.server.waits.load() == 2 && thread.server.lastWait.load() > 0 && thread.server.lastWait.load() <= 950000);
}

void TestDelayedTasks() {
    TestThread thread;
    Record late;
    Record early;
    Record sameAsEarly;
    Record now;
    std::vector<int> order;
    late.body = [&] { order.push_back(3); };
    early.body = [&] { order.push_back(1); };
    sameAsEarly.body = [&] { order.push_back(2); };
    now.body = [&] { order.push_back(0); };
    thread.PostDelayed(late, 60000);
    thread.PostDelayed(early, 20000);
    thread.PostDelayed(sameAsEarly, 20000);
    Require(late.moves.load() >= 1 && thread.server.wakeUps.load() == 3);
    const int delay = getDelay(thread.get());
    Require(delay > 0 && delay <= 20);
    thread.Post(now);
    Require(getDelay(thread.get()) == 0);
    Require(processMessages(thread.get(), 150));
    Require((order == std::vector<int>{0, 1, 2, 3}));
    Require(late.disposals.load() == 1 && early.disposals.load() == 1 && getDelay(thread.get()) == -1);
    Require(thread.server.lastWait.load() <= 130000);

    std::array<Record, 24> ties;
    std::vector<int> tieOrder;
    for (std::size_t index = 0; index < ties.size(); ++index) {
        ties[index].body = [&tieOrder, index] { tieOrder.push_back(static_cast<int>(index)); };
        thread.PostDelayed(ties[index], 15000);
    }
    Require(processMessages(thread.get(), 60) && tieOrder.size() == ties.size());
    for (std::size_t index = 0; index < ties.size(); ++index) Require(tieOrder[index] == static_cast<int>(index));

    bool waited = false;
    for (int attempt = 0; attempt < 8 && !waited; ++attempt) {
        Record soon;
        thread.PostDelayed(soon, 1);
        Require(processMessages(thread.get(), 0));
        waited = soon.runs.load() == 0;
        Require(processMessages(thread.get(), 5) && soon.runs.load() == 1);
    }
    Require(waited);

    Record rounded;
    const auto before = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point ranAt;
    rounded.body = [&] { ranAt = std::chrono::steady_clock::now(); };
    thread.PostDelayed(rounded, 9001);
    const int roundedDelay = getDelay(thread.get());
    Require(roundedDelay >= 8 && roundedDelay <= 10);
    Require(processMessages(thread.get(), 80));
    Require(rounded.runs.load() == 1);
    Require(std::chrono::duration_cast<std::chrono::milliseconds>(ranAt - before).count() >= 9);

    Record negative;
    thread.PostDelayed(negative, -5000000);
    Require(getDelay(thread.get()) == 0);
    Require(processMessages(thread.get(), 20) && negative.runs.load() == 1);
    for (const std::int64_t delay : {INT64_MAX, INT64_MAX - 998, INT64_MIN, std::int64_t{2147483647001}, std::int64_t{-2147483649999}}) {
        Record refused;
        Require(ThrowsInvalidArgument([&] { thread.PostDelayed(refused, delay); }));
        Require(refused.moves.load() == 0 && refused.disposals.load() == 1);
    }
    Require(getDelay(thread.get()) == -1);
    Record farthest;
    thread.PostDelayed(farthest, std::int64_t{2147483647000});
    Require(farthest.moves.load() >= 1 && getDelay(thread.get()) > 2147483000);
    Record earliest;
    thread.PostDelayed(earliest, std::int64_t{-2147483649998});
    Require(getDelay(thread.get()) == 0 && processMessages(thread.get(), 0) && earliest.runs.load() == 1 && farthest.runs.load() == 0);
}

void TestARunningThread() {
    TestThread thread;
    Require(start(thread.get()));
    Require(!start(thread.get()));
    Record record;
    std::atomic<bool> done{false};
    record.body = [&] { done = true; };
    thread.Post(record);
    while (!done) std::this_thread::yield();
    Require(record.thread != std::this_thread::get_id());
    Require(record.currentThreadDuringRun == thread.get() && record.currentQueueDuringRun == thread.get());
    Require(currentThread() == nullptr && isProcessing(thread.get()));

    std::atomic<int> calls{0};
    std::thread::id callThread;
    Call call{&calls, &callThread};
    blockingCall(thread.get(), &call, &RecordCall, nullptr);
    Require(calls.load() == 1 && callThread == record.thread);

    Record nested;
    std::atomic<bool> nestedDone{false};
    std::thread::id innerThread;
    Call inner{nullptr, &innerThread};
    nested.body = [&] {
        blockingCall(thread.get(), &inner, &RecordCall, nullptr);
        nestedDone = true;
    };
    thread.Post(nested);
    while (!nestedDone) std::this_thread::yield();
    Require(innerThread == record.thread);

    while (thread.server.lastWait.load() != INT64_MAX) std::this_thread::yield();
    Record later;
    std::atomic<bool> laterDone{false};
    later.body = [&] { laterDone = true; };
    thread.PostDelayed(later, 10000);
    while (!laterDone) std::this_thread::yield();
    Require(later.thread == record.thread);

    stop(thread.get());
    Require(isQuitting(thread.get()) && !isProcessing(thread.get()));
    Record dropped;
    thread.Post(dropped);
    Require(dropped.moves.load() == 0 && dropped.runs.load() == 0 && dropped.disposals.load() == 1);
    Record droppedLater;
    thread.PostDelayed(droppedLater, 1000);
    Require(droppedLater.moves.load() == 0 && droppedLater.disposals.load() == 1 && getDelay(thread.get()) == -1);
    int skipped = 0;
    blockingCall(thread.get(), &skipped, &Increment, nullptr);
    Require(skipped == 0 && !processMessages(thread.get(), 0));

    restart(thread.get());
    Require(!isQuitting(thread.get()) && start(thread.get()));
    Record again;
    std::atomic<bool> againDone{false};
    again.body = [&] { againDone = true; };
    thread.Post(again);
    while (!againDone) std::this_thread::yield();
    Require(again.currentThreadDuringRun == thread.get() && again.currentQueueDuringRun == thread.get());
    Require(again.thread != std::this_thread::get_id());
    stop(thread.get());
}

void TestDestroyingARunningThread() {
    Record record;
    std::atomic<bool> started{false};
    std::atomic<bool> finished{false};
    record.body = [&] {
        started = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        finished = true;
    };
    TestThread thread;
    Require(start(thread.get()));
    thread.Post(record);
    while (!started) std::this_thread::yield();
    destroy(thread.get());
    thread.alive = false;
    Require(finished.load() && record.disposals.load() == 1);
}

void TestTheVirtualTable() {
    void* const* table = WebRtcThreadVtable_nid_no_patch();
    Require(table[-2] == nullptr && table[-1] == nullptr);
    using Void = void (APS5_VABI*)(void*);
    using Bool = bool (APS5_VABI*)(void*);
    using Int = int (APS5_VABI*)(void*);
    using PostTask = void (APS5_VABI*)(void*, WebRtcAnyInvocable*, const void*, const void*);
    using PostDelayedTask = void (APS5_VABI*)(void*, WebRtcAnyInvocable*, std::int64_t, const void*, const void*);
    using BlockingCall = void (APS5_VABI*)(void*, void*, void (APS5_VABI*)(void*), const void*);
    TestThread thread;
    void* self = thread.get();
    Require(reinterpret_cast<Bool>(table[8])(self) && reinterpret_cast<Int>(table[9])(self) == -1);
    Record delayed;
    {
        GuestTask task(delayed);
        reinterpret_cast<PostDelayedTask>(table[2])(self, &task.invocable, 3600000000, nullptr, nullptr);
    }
    Require(delayed.moves.load() >= 1 && reinterpret_cast<Int>(table[9])(self) > 3000000);
    Record record;
    {
        GuestTask task(record);
        reinterpret_cast<PostTask>(table[1])(self, &task.invocable, nullptr, nullptr);
    }
    Require(record.moves.load() == 1 && reinterpret_cast<Int>(table[9])(self) == 0);
    reinterpret_cast<Void>(table[5])(self);
    Require(isQuitting(self) && reinterpret_cast<Bool>(table[6])(self) && !reinterpret_cast<Bool>(table[8])(self));
    reinterpret_cast<Void>(table[11])(self);
    Require(record.runs.load() == 1 && delayed.runs.load() == 0);
    reinterpret_cast<Void>(table[7])(self);
    Require(!isQuitting(self) && !reinterpret_cast<Bool>(table[6])(self));
    WebRtcThreadManager* manager = _ZN3rtc13ThreadManager8InstanceEv_nid_postfix();
    _ZN3rtc13ThreadManager16SetCurrentThreadEPNS_6ThreadE_nid_postfix(manager, self);
    int calls = 0;
    reinterpret_cast<BlockingCall>(table[12])(self, &calls, &Increment, nullptr);
    _ZN3rtc13ThreadManager16SetCurrentThreadEPNS_6ThreadE_nid_postfix(manager, nullptr);
    Require(calls == 1);
    reinterpret_cast<Void>(table[10])(self);
    Require(isQuitting(self) && delayed.disposals.load() == 0);
    reinterpret_cast<Void>(table[3])(self);
    thread.alive = false;
    Require((thread.server.queues == std::vector<void*>{self, nullptr}) && delayed.disposals.load() == 1);

    SocketServer server;
    void* memory = _Znwm_nid_postfix(512);
    construct(memory, &server);
    int frees = heapFrees.load();
    reinterpret_cast<Void>(table[4])(memory);
    Require(heapFrees.load() == frees + 1 && lastFreed.load() == memory && (server.queues == std::vector<void*>{memory, nullptr}));
    memory = _Znwm_nid_postfix(512);
    construct(memory, &server);
    frees = heapFrees.load();
    reinterpret_cast<Void>(table[0])(memory);
    Require(heapFrees.load() == frees + 1 && lastFreed.load() == memory && server.queues.size() == 4);
}

void TestPendingTasksAreReleased() {
    Record pending;
    Record delayed;
    {
        TestThread thread;
        thread.Post(pending);
        thread.PostDelayed(delayed, 5000000);
        Require(pending.disposals.load() == 0 && delayed.disposals.load() == 0);
        void* self = thread.get();
        destroy(self);
        thread.alive = false;
        Require(pending.runs.load() == 0 && pending.disposals.load() == 1 && pending.currentQueueDuringDisposal == self);
        Require(delayed.runs.load() == 0 && delayed.disposals.load() == 1 && delayed.currentQueueDuringDisposal == self);
        Require(currentQueue() == nullptr);
        Require(ThrowsInvalidArgument([&] { getDelay(self); }));
    }
}

void TestDelete() {
    auto* server = new SocketServer;
    void* memory = _Znwm_nid_postfix(512);
    construct(memory, server);
    Require(start(memory));
    Record record;
    std::atomic<bool> done{false};
    record.body = [&] { done = true; };
    {
        GuestTask task(record);
        postTask(memory, &task.invocable, nullptr, nullptr);
    }
    while (!done) std::this_thread::yield();
    const int frees = heapFrees.load();
    _ZN3rtc6Thread6DeleteEv_nid_postfix(memory);
    Require(heapFrees.load() == frees + 1 && lastFreed.load() == memory);
    Require((server->queues == std::vector<void*>{memory, nullptr}));
    Require(ThrowsInvalidArgument([&] { getDelay(memory); }));
    delete server;
}

void TestTheSocketServerEndsTheLoop() {
    TestThread thread;
    thread.server.failWait = true;
    Require(processMessages(thread.get(), 1000));
    Require(thread.server.waits.load() == 1 && thread.server.lastWait.load() <= 1000000 && thread.server.lastWait.load() > 900000);
    Record record;
    thread.Post(record);
    Require(processMessages(thread.get(), 1000) && record.runs.load() == 1 && thread.server.waits.load() == 2);
}

void TestTheCurrentThread() {
    TestThread thread;
    WebRtcThreadManager* manager = _ZN3rtc13ThreadManager8InstanceEv_nid_postfix();
    Require(currentThread() == nullptr && currentQueue() == nullptr);
    _ZN3rtc13ThreadManager16SetCurrentThreadEPNS_6ThreadE_nid_postfix(manager, thread.get());
    Require(currentThread() == thread.get() && currentQueue() == thread.get());
    Require(_ZNK3rtc6Thread9IsCurrentEv_nid_postfix(thread.get()));
    int inlineCalls = 0;
    blockingCall(thread.get(), &inlineCalls, &Increment, nullptr);
    Require(inlineCalls == 1 && thread.server.wakeUps.load() == 0);
    _ZN3rtc13ThreadManager16SetCurrentThreadEPNS_6ThreadE_nid_postfix(manager, nullptr);
    Require(currentThread() == nullptr && currentQueue() == nullptr);
    _ZN3rtc13ThreadManager16SetCurrentThreadEPNS_6ThreadE_nid_postfix(manager, nullptr);
    Require(currentThread() == nullptr);

    _ZN3rtc13ThreadManager16SetCurrentThreadEPNS_6ThreadE_nid_postfix(manager, thread.get());
    _ZN3rtc13ThreadManager16SetCurrentThreadEPNS_6ThreadE_nid_postfix(manager, thread.get());
    Require(currentThread() == thread.get() && currentQueue() == nullptr);
    _ZN3rtc13ThreadManager16SetCurrentThreadEPNS_6ThreadE_nid_postfix(manager, nullptr);
    Require(currentThread() == nullptr && currentQueue() == thread.get());
    WebRtcCurrentTaskQueueSetter reset{};
    _ZN6webrtc13TaskQueueBase22CurrentTaskQueueSetterC1EPS0__nid_postfix(&reset, nullptr);
    Require(currentQueue() == nullptr);
}

struct Overrides {
    std::atomic<int> isQuitting{0};
    std::atomic<int> restart{0};
    std::atomic<int> postTask{0};
    std::atomic<int> stop{0};
    std::atomic<int> run{0};
    std::atomic<int> deleting{0};
};

Overrides overrides;

bool APS5_VABI OverrideIsQuitting(void* thread) {
    ++overrides.isQuitting;
    return isQuitting(thread);
}

void APS5_VABI OverrideRestart(void* thread) {
    ++overrides.restart;
    restart(thread);
}

void APS5_VABI OverridePostTask(void* thread, WebRtcAnyInvocable* task, const void* traits, const void* location) {
    ++overrides.postTask;
    Require(traits != nullptr && location != nullptr);
    postTask(thread, task, traits, location);
}

void APS5_VABI OverrideStop(void* thread) {
    ++overrides.stop;
    stop(thread);
}

void APS5_VABI OverrideRun(void* thread) {
    ++overrides.run;
    _ZN3rtc6Thread3RunEv_nid_postfix(thread);
}

void APS5_VABI OverrideDeleting(void* thread) {
    ++overrides.deleting;
    destroy(thread);
    void* const* vtable = nullptr;
    std::memcpy(&vtable, thread, sizeof(vtable));
    Require(vtable == WebRtcThreadVtable_nid_no_patch());
    _ZdlPv_nid_postfix(thread);
}

void TestVirtualDispatch() {
    std::array<void*, 15> table{};
    std::memcpy(table.data(), WebRtcThreadVtable_nid_no_patch() - 2, sizeof(table));
    table[2 + 1] = reinterpret_cast<void*>(&OverridePostTask);
    table[2 + 4] = reinterpret_cast<void*>(&OverrideDeleting);
    table[2 + 6] = reinterpret_cast<void*>(&OverrideIsQuitting);
    table[2 + 7] = reinterpret_cast<void*>(&OverrideRestart);
    table[2 + 10] = reinterpret_cast<void*>(&OverrideStop);
    table[2 + 11] = reinterpret_cast<void*>(&OverrideRun);
    auto* server = new SocketServer;
    void* memory = _Znwm_nid_postfix(512);
    construct(memory, server);
    void* const* derived = table.data() + 2;
    std::memcpy(memory, &derived, sizeof(derived));

    Require(getDelay(memory) == -1 && overrides.isQuitting.load() == 0);
    Record record;
    std::atomic<bool> done{false};
    record.body = [&] { done = true; };
    {
        GuestTask task(record);
        postTask(memory, &task.invocable, nullptr, nullptr);
    }
    Require(overrides.isQuitting.load() == 1 && overrides.postTask.load() == 0);
    {
        Record delayed;
        GuestTask task(delayed);
        postDelayedTask(memory, &task.invocable, 3600000000, nullptr, nullptr);
        Require(overrides.isQuitting.load() == 2 && delayed.moves.load() >= 1);
    }
    Require(start(memory));
    Require(overrides.restart.load() == 1);
    while (!done) std::this_thread::yield();
    Require(overrides.run.load() == 1);
    int calls = 0;
    blockingCall(memory, &calls, &Increment, nullptr);
    Require(calls == 1 && overrides.postTask.load() == 1);
    const int before = overrides.isQuitting.load();
    Require(isProcessing(memory) && overrides.isQuitting.load() > before);
    Require(overrides.stop.load() == 0 && overrides.deleting.load() == 0);
    quit(memory);
    blockingCall(memory, &calls, &Increment, nullptr);
    Require(calls == 1 && overrides.postTask.load() == 1);
    _ZN3rtc6Thread6DeleteEv_nid_postfix(memory);
    Require(overrides.stop.load() == 1 && overrides.deleting.load() == 1 && overrides.restart.load() == 1);
    Require((server->queues == std::vector<void*>{memory, nullptr}));
    delete server;
}

void TestInvalidArguments() {
    TestThread thread;
    SocketServer server;
    unsigned char other[512] = {};
    Record record;
    GuestTask task(record);
    Require(ThrowsInvalidArgument([&] { construct(nullptr, &server); }));
    Require(ThrowsInvalidArgument([&] { construct(other, nullptr); }));
    Require(server.queues.empty());
    Require(ThrowsInvalidArgument([&] { destroy(nullptr); }));
    Require(ThrowsInvalidArgument([&] { destroy(other); }));
    Require(ThrowsInvalidArgument([&] { postTask(other, &task.invocable, nullptr, nullptr); }));
    Require(ThrowsInvalidArgument([&] { postTask(thread.get(), nullptr, nullptr, nullptr); }));
    Require(ThrowsInvalidArgument([&] { postDelayedTask(thread.get(), nullptr, 0, nullptr, nullptr); }));
    Require(ThrowsInvalidArgument([&] { processMessages(other, 0); }));
    Require(ThrowsInvalidArgument([&] { start(other); }));
    Require(ThrowsInvalidArgument([&] { stop(nullptr); }));
    Require(ThrowsInvalidArgument([&] { isProcessing(other); }));
    int calls = 0;
    Require(ThrowsInvalidArgument([&] { blockingCall(nullptr, &calls, &Increment, nullptr); }));
    Require(ThrowsInvalidArgument([&] { blockingCall(other, &calls, &Increment, nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN3rtc6Thread6DeleteEv_nid_postfix(nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN3rtc6Thread6DeleteEv_nid_postfix(other); }));
    Require(ThrowsInvalidArgument([&] { _ZN3rtc6Thread6DoInitEv_nid_postfix(other); }));
    Require(ThrowsInvalidArgument([&] { _ZN3rtc6Thread9DoDestroyEv_nid_postfix(other); }));
    Require(calls == 0);
    _ZN3rtc6Thread6DoInitEv_nid_postfix(thread.get());
    Require(thread.server.queues == std::vector<void*>{thread.get()});
    _ZN3rtc6Thread9DoDestroyEv_nid_postfix(thread.get());
    _ZN3rtc6Thread9DoDestroyEv_nid_postfix(thread.get());
    Require((thread.server.queues == std::vector<void*>{thread.get(), nullptr}));
    Require(ThrowsInvalidArgument([&] { _ZN3rtc13ThreadManager16SetCurrentThreadEPNS_6ThreadE_nid_postfix(nullptr, thread.get()); }));
    Require(record.moves.load() == 0);
}

void* APS5_VABI TestHeapAllocate(std::size_t size) { return std::malloc(size == 0 ? 1 : size); }
void APS5_VABI TestHeapFree(void* pointer) {
    lastFreed = pointer;
    ++heapFrees;
    std::free(pointer);
}
void* APS5_VABI TestHeapCallocate(std::size_t count, std::size_t size) {
    return std::calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
}
void* APS5_VABI TestHeapReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size == 0 ? 1 : size); }
void* APS5_VABI UnexpectedAlign(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

}  // namespace

int main() {
    std::thread([] {
        std::this_thread::sleep_for(std::chrono::seconds(40));
        std::fprintf(stderr, "rtc thread test timed out\n");
        std::fflush(stderr);
        std::_Exit(3);
    }).detach();
    std::array<void*, 10> heapApi{
        reinterpret_cast<void*>(TestHeapAllocate),
        reinterpret_cast<void*>(TestHeapFree),
        reinterpret_cast<void*>(TestHeapCallocate),
        reinterpret_cast<void*>(TestHeapReallocate),
        reinterpret_cast<void*>(UnexpectedAlign),
        reinterpret_cast<void*>(UnexpectedRealign),
        reinterpret_cast<void*>(UnexpectedPosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    TestConstruction();
    TestTheVirtualTable();
    TestTasksOnTheCallingThread();
    TestAWakeUpWithoutATask();
    TestDelayedTasks();
    TestARunningThread();
    TestDestroyingARunningThread();
    TestPendingTasksAreReleased();
    TestDelete();
    TestTheSocketServerEndsTheLoop();
    TestTheCurrentThread();
    TestVirtualDispatch();
    TestInvalidArguments();
    std::puts("libwebrtc rtc thread tests passed");
    return 0;
}
