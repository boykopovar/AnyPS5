#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using NKWebKit::WtfAbi::CrossThreadTaskHandler;
using NKWebKit::WtfAbi::VoidCallableWrapper;
using NKWebKit::WtfAbi::VoidCallableWrapperVtable;
using NKWebKit::WtfAbi::VoidFunction;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "cross thread task handler check failed at line %d\n", line);
        std::fflush(stderr);
        std::_Exit(1);
    }
}
#define Require(value) Check((value), __LINE__)

constexpr auto construct = &_ZN3WTF22CrossThreadTaskHandlerC1EPKcNS0_25AutodrainedPoolForRunLoopE;
constexpr auto destroy = &_ZN3WTF22CrossThreadTaskHandlerD1Ev;
constexpr auto postTask = &_ZN3WTF22CrossThreadTaskHandler8postTaskEONS_15CrossThreadTaskE;
constexpr auto postTaskReply = &_ZN3WTF22CrossThreadTaskHandler13postTaskReplyEONS_15CrossThreadTaskE;
constexpr auto killHandler = &_ZN3WTF22CrossThreadTaskHandler4killEv;

static std::atomic<void*> lastFreed{nullptr};

static void* APS5_VABI Allocate_nid_no_patch(std::size_t size) { return std::malloc(size); }
static void APS5_VABI Free_nid_no_patch(void* pointer) {
    lastFreed = pointer;
    std::free(pointer);
}
static void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
static void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
static void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
static void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
static int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

static std::mutex logMutex;
static std::vector<std::string> eventLog;
static std::atomic<int> liveCallables{0};

static void Log(const std::string& event) {
    std::lock_guard lock(logMutex);
    eventLog.push_back(event);
}

static std::vector<std::string> TakeLog() {
    std::lock_guard lock(logMutex);
    std::vector<std::string> taken;
    taken.swap(eventLog);
    return taken;
}

static const char* Where() {
    return _ZN3WTF12isMainThreadEv() ? "main" : "other";
}

struct Callable {
    VoidCallableWrapper base;
    std::string name;
    std::function<void()> body;
    bool quiet = false;
};

static void APS5_VABI CallableCompleteDestructor(VoidCallableWrapper*) { Require(false); }
static void APS5_VABI CallableDeletingDestructor(VoidCallableWrapper* self) {
    auto* callable = reinterpret_cast<Callable*>(self);
    if (!callable->quiet) Log("delete:" + callable->name + ":" + Where());
    --liveCallables;
    delete callable;
}
static void APS5_VABI CallableCall(VoidCallableWrapper* self) {
    auto* callable = reinterpret_cast<Callable*>(self);
    if (!callable->quiet) Log("call:" + callable->name + ":" + Where());
    if (callable->body) callable->body();
}
static const VoidCallableWrapperVtable callableVtable = {CallableCompleteDestructor, CallableDeletingDestructor, CallableCall};

static VoidFunction Make(const std::string& name, std::function<void()> body = {}, bool quiet = false) {
    ++liveCallables;
    return VoidFunction{&(new Callable{{&callableVtable}, name, std::move(body), quiet})->base};
}

static void Wait(const std::atomic<bool>& flag) {
    while (!flag) std::this_thread::yield();
}

static void Settle() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

static void DrainMain() {
    VoidFunction hop = Make("hop", [] {
        VoidFunction stop = Make("stop", [] { WKRunLoopStopMain(); }, true);
        _ZN3WTF16callOnMainThreadEONS_8FunctionIFvvEEE(&stop);
    }, true);
    _ZN3WTF16callOnMainThreadEONS_8FunctionIFvvEEE(&hop);
    WKRunLoopRunMain();
    WKRunLoopInitializeMain();
}

template<typename TFunction>
static bool ThrowsInvalidArgument(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

struct Handler {
    alignas(16) unsigned char bytes[256];

    Handler() {
        std::memset(bytes, 0xA5, sizeof(bytes));
        construct(get(), "nkwebkit test", 0);
    }

    ~Handler() {
        if (get()->state) destroy(get());
        Require(Untouched());
    }

    CrossThreadTaskHandler* get() {
        return reinterpret_cast<CrossThreadTaskHandler*>(bytes);
    }

    bool Untouched() const {
        for (std::size_t index = 16; index < sizeof(bytes); ++index)
            if ((index < 112 || index >= 120) && bytes[index] != 0xA5) return false;
        return true;
    }

    void Post(const std::string& name, std::function<void()> body = {}) {
        VoidFunction task = Make(name, std::move(body));
        postTask(get(), &task);
        Require(task.callableWrapper == nullptr);
    }

    void Reply(const std::string& name, std::function<void()> body = {}) {
        VoidFunction task = Make(name, std::move(body));
        postTaskReply(get(), &task);
        Require(task.callableWrapper == nullptr);
    }

    void OnCompletion(const std::string& name, std::function<void()> body = {}) {
        VoidFunction callback = Make(name, std::move(body));
        Require(get()->completionCallback == nullptr);
        get()->completionCallback = callback.callableWrapper;
    }

    void Sync() {
        std::atomic<bool> done{false};
        Post("sync", [&done] { done = true; });
        Wait(done);
        while (true) {
            {
                std::lock_guard lock(logMutex);
                bool deleted = false;
                for (const std::string& event : eventLog)
                    if (event == "delete:sync:other") deleted = true;
                if (deleted) break;
            }
            std::this_thread::yield();
        }
        std::lock_guard lock(logMutex);
        std::vector<std::string> kept;
        for (const std::string& event : eventLog)
            if (event != "call:sync:other" && event != "delete:sync:other") kept.push_back(event);
        eventLog.swap(kept);
    }
};

static bool Same(const std::vector<std::string>& actual, std::initializer_list<const char*> expected) {
    if (actual.size() != expected.size()) return false;
    std::size_t index = 0;
    for (const char* event : expected)
        if (actual[index++] != event) return false;
    return true;
}

static void TestConstruction() {
    Handler handler;
    Require(handler.get()->vtable != nullptr && handler.get()->state != nullptr);
    Require(handler.Untouched());
    handler.get()->vtable->completeDestructor(handler.get());
    Require(handler.get()->state == nullptr);

    Handler second;
    destroy(second.get());
    _ZN3WTF22CrossThreadTaskHandlerC2EPKcNS0_25AutodrainedPoolForRunLoopE(second.get(), nullptr, 1);
    Require(second.get()->state != nullptr);
    second.Sync();
    _ZN3WTF22CrossThreadTaskHandlerD2Ev(second.get());
    Require(second.get()->state == nullptr);
    Settle();
    TakeLog();
    Require(liveCallables.load() == 0);
}

static void TestTasks() {
    Handler handler;
    Handler other;
    std::thread::id first;
    std::thread::id second;
    std::thread::id third;
    std::atomic<bool> started{false};
    std::atomic<bool> release{false};
    handler.Post("gate", [&] {
        started = true;
        while (!release) std::this_thread::yield();
    });
    Wait(started);
    handler.Post("a", [&] { first = std::this_thread::get_id(); });
    handler.Post("b", [&] { second = std::this_thread::get_id(); });
    release = true;
    handler.Sync();
    other.Post("c", [&] { third = std::this_thread::get_id(); });
    other.Sync();
    Require(Same(TakeLog(), {"call:gate:other", "delete:gate:other", "call:a:other", "delete:a:other", "call:b:other", "delete:b:other", "call:c:other", "delete:c:other"}));
    Require(first == second && first != std::this_thread::get_id() && third != first && third != std::this_thread::get_id());
    Require(liveCallables.load() == 0);
}

static void TestReplies() {
    Handler handler;
    handler.Reply("r1");
    handler.Reply("r2");
    Require(TakeLog().empty());
    DrainMain();
    Require(Same(TakeLog(), {"call:r1:main", "delete:r1:main", "call:r2:main", "delete:r2:main"}));

    handler.Reply("r3");
    DrainMain();
    Require(Same(TakeLog(), {"call:r3:main", "delete:r3:main"}));

    std::atomic<bool> posted{false};
    handler.Post("t", [&] {
        handler.Reply("r4", [&] { handler.Reply("r5"); });
        posted = true;
    });
    Wait(posted);
    handler.Sync();
    DrainMain();
    Require(Same(TakeLog(), {"call:t:other", "delete:t:other", "call:r4:main", "delete:r4:main", "call:r5:main", "delete:r5:main"}));

    killHandler(handler.get());
    handler.Reply("r6");
    DrainMain();
    Require(Same(TakeLog(), {"call:r6:main", "delete:r6:main"}));
    Require(liveCallables.load() == 0);
}

static void TestKill() {
    {
        Handler handler;
        std::atomic<bool> started{false};
        std::atomic<bool> release{false};
        std::atomic<bool> completed{false};
        handler.OnCompletion("done", [&] { completed = true; });
        handler.Post("gate", [&] {
            started = true;
            while (!release) std::this_thread::yield();
        });
        handler.Post("pending");
        Wait(started);
        killHandler(handler.get());
        handler.Post("late");
        release = true;
        Wait(completed);
        Settle();
        Require(Same(TakeLog(), {"call:gate:other", "delete:gate:other", "call:done:other"}));
        Require(liveCallables.load() == 3);
        destroy(handler.get());
        Require(Same(TakeLog(), {"delete:done:main", "delete:pending:main", "delete:late:main"}));
        Require(liveCallables.load() == 0);
    }
    {
        Handler handler;
        handler.Sync();
        killHandler(handler.get());
        handler.Post("never");
        Settle();
        Require(TakeLog().empty() && liveCallables.load() == 1);
        killHandler(handler.get());
    }
    Require(Same(TakeLog(), {"delete:never:main"}));
}

static void TestTheEmptyTask() {
    Handler handler;
    std::atomic<bool> completed{false};
    handler.OnCompletion("done", [&] { completed = true; });
    handler.Post("before");
    VoidFunction empty{nullptr};
    postTask(handler.get(), &empty);
    handler.Post("after");
    Wait(completed);
    Settle();
    Require(Same(TakeLog(), {"call:before:other", "delete:before:other", "call:done:other"}));
    destroy(handler.get());
    Require(Same(TakeLog(), {"delete:done:main", "delete:after:main"}));
}

static void TestTheCompletionCallback() {
    {
        Handler handler;
        Require(handler.get()->completionCallback == nullptr);
        std::atomic<bool> completed{false};
        handler.OnCompletion("done", [&] { completed = true; });
        handler.Sync();
        killHandler(handler.get());
        Wait(completed);
        Settle();
        Require(Same(TakeLog(), {"call:done:other"}));
        Require(handler.get()->completionCallback != nullptr);
    }
    Require(Same(TakeLog(), {"delete:done:main"}));
    {
        Handler handler;
        handler.Sync();
        killHandler(handler.get());
        Settle();
        Require(TakeLog().empty());
    }
    Require(TakeLog().empty() && liveCallables.load() == 0);
}

static void TestDestruction() {
    Handler handler;
    std::atomic<bool> started{false};
    std::atomic<bool> release{false};
    handler.OnCompletion("done");
    handler.Post("gate", [&] {
        started = true;
        while (!release) std::this_thread::yield();
    });
    handler.Post("t1");
    handler.Post("t2");
    handler.Reply("r1");
    handler.Reply("r2");
    Wait(started);
    destroy(handler.get());
    Require(handler.get()->state == nullptr && handler.get()->completionCallback == nullptr);
    Require(Same(TakeLog(), {"call:gate:other", "delete:done:main", "delete:r1:main", "delete:r2:main", "delete:t1:main", "delete:t2:main"}));
    release = true;
    while (liveCallables.load() != 0) std::this_thread::yield();
    Settle();
    Require(Same(TakeLog(), {"delete:gate:other"}));
    DrainMain();
    Require(TakeLog().empty());

    VoidFunction function{nullptr};
    Require(ThrowsInvalidArgument([&] { destroy(handler.get()); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF22CrossThreadTaskHandlerD2Ev(handler.get()); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF22CrossThreadTaskHandlerD0Ev(handler.get()); }));
    Require(ThrowsInvalidArgument([&] { postTask(handler.get(), &function); }));
    Require(ThrowsInvalidArgument([&] { postTaskReply(handler.get(), &function); }));
    Require(ThrowsInvalidArgument([&] { killHandler(handler.get()); }));
}

static void TestTheDeletingDestructor() {
    auto* handler = static_cast<CrossThreadTaskHandler*>(_Znwm_nid_postfix(128));
    construct(handler, "nkwebkit test", 0);
    {
        VoidFunction task = Make("queued");
        killHandler(handler);
        postTask(handler, &task);
    }
    Settle();
    Require(TakeLog().empty());
    Require(lastFreed.load() != handler);
    handler->vtable->deletingDestructor(handler);
    Require(Same(TakeLog(), {"delete:queued:main"}));
    Require(lastFreed.load() == handler);
}

static void TestNullArguments() {
    Handler handler;
    VoidFunction function{nullptr};
    VoidFunction task = Make("kept");
    Require(ThrowsInvalidArgument([&] { construct(nullptr, "name", 0); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF22CrossThreadTaskHandlerC2EPKcNS0_25AutodrainedPoolForRunLoopE(nullptr, "name", 0); }));
    Require(ThrowsInvalidArgument([&] { destroy(nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF22CrossThreadTaskHandlerD0Ev(nullptr); }));
    Require(ThrowsInvalidArgument([&] { postTask(nullptr, &task); }));
    Require(ThrowsInvalidArgument([&] { postTask(handler.get(), nullptr); }));
    Require(ThrowsInvalidArgument([&] { postTaskReply(nullptr, &task); }));
    Require(ThrowsInvalidArgument([&] { postTaskReply(handler.get(), nullptr); }));
    Require(ThrowsInvalidArgument([&] { postTaskReply(handler.get(), &function); }));
    Require(ThrowsInvalidArgument([&] { killHandler(nullptr); }));
    Require(task.callableWrapper != nullptr);
    handler.Sync();
    DrainMain();
    Require(TakeLog().empty());
    task.callableWrapper->vtable->deletingDestructor(task.callableWrapper);
    TakeLog();
    Require(liveCallables.load() == 0);
}

struct GuardedPages {
    unsigned char* base{nullptr};
    std::size_t pageSize{0};

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        Require(VirtualFree(base, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(base, pageSize * 3) == 0);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }
};

template<typename TFunction>
static bool ThrowsNotImplemented(TFunction&& function, const char* symbol) {
    try {
        function();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(symbol) + " not implemented";
    }
    return false;
}

static void TestGuestPointers() {
    GuardedPages pages;
    auto* readOnly = reinterpret_cast<CrossThreadTaskHandler*>(pages.ReadOnly());
    auto* guard = reinterpret_cast<CrossThreadTaskHandler*>(pages.Guard());
    auto* dangling = reinterpret_cast<CrossThreadTaskHandler*>(0x10);
    unsigned char* splitWritable = pages.ReadOnly() - 16;
    std::memset(splitWritable, 0x5A, 16);

    Require(ThrowsNotImplemented([&] { construct(readOnly, "name", 0); }, "_ZN3WTF22CrossThreadTaskHandlerC1EPKcNS0_25AutodrainedPoolForRunLoopE"));
    Require(ThrowsNotImplemented([&] { _ZN3WTF22CrossThreadTaskHandlerC2EPKcNS0_25AutodrainedPoolForRunLoopE(guard, "name", 0); }, "_ZN3WTF22CrossThreadTaskHandlerC2EPKcNS0_25AutodrainedPoolForRunLoopE"));
    Require(ThrowsNotImplemented([&] { construct(dangling, "name", 0); }, "_ZN3WTF22CrossThreadTaskHandlerC1EPKcNS0_25AutodrainedPoolForRunLoopE"));
    Require(ThrowsNotImplemented([&] { construct(reinterpret_cast<CrossThreadTaskHandler*>(splitWritable), "name", 0); }, "_ZN3WTF22CrossThreadTaskHandlerC1EPKcNS0_25AutodrainedPoolForRunLoopE"));
    for (std::size_t i = 0; i < 16; ++i) Require(splitWritable[i] == 0x5A);

    Require(ThrowsNotImplemented([&] { destroy(readOnly); }, "_ZN3WTF22CrossThreadTaskHandlerD1Ev"));
    Require(ThrowsNotImplemented([&] { _ZN3WTF22CrossThreadTaskHandlerD2Ev(guard); }, "_ZN3WTF22CrossThreadTaskHandlerD2Ev"));
    Require(ThrowsNotImplemented([&] { _ZN3WTF22CrossThreadTaskHandlerD0Ev(dangling); }, "_ZN3WTF22CrossThreadTaskHandlerD0Ev"));
    Require(ThrowsNotImplemented([&] { killHandler(guard); }, "_ZN3WTF22CrossThreadTaskHandler4killEv"));
    Require(ThrowsNotImplemented([&] { killHandler(dangling); }, "_ZN3WTF22CrossThreadTaskHandler4killEv"));

    CrossThreadTaskHandler forged{};
    forged.state = reinterpret_cast<NKWebKit::WtfAbi::CrossThreadTaskHandlerState*>(pages.Guard());
    forged.completionCallback = nullptr;
    VoidFunction emptyTask{nullptr};
    Require(ThrowsNotImplemented([&] { destroy(&forged); }, "_ZN3WTF22CrossThreadTaskHandlerD1Ev"));
    Require(ThrowsNotImplemented([&] { killHandler(&forged); }, "_ZN3WTF22CrossThreadTaskHandler4killEv"));
    Require(ThrowsNotImplemented([&] { postTask(&forged, &emptyTask); }, "_ZN3WTF22CrossThreadTaskHandler8postTaskEONS_15CrossThreadTaskE"));
    Require(ThrowsNotImplemented([&] { postTaskReply(&forged, &emptyTask); }, "_ZN3WTF22CrossThreadTaskHandler13postTaskReplyEONS_15CrossThreadTaskE"));

    Handler handler;
    auto* readOnlyTask = reinterpret_cast<VoidFunction*>(pages.ReadOnly());
    auto* guardTask = reinterpret_cast<VoidFunction*>(pages.Guard());
    Require(ThrowsNotImplemented([&] { postTask(guard, &emptyTask); }, "_ZN3WTF22CrossThreadTaskHandler8postTaskEONS_15CrossThreadTaskE"));
    Require(ThrowsNotImplemented([&] { postTaskReply(guard, &emptyTask); }, "_ZN3WTF22CrossThreadTaskHandler13postTaskReplyEONS_15CrossThreadTaskE"));
    Require(ThrowsNotImplemented([&] { postTask(handler.get(), readOnlyTask); }, "_ZN3WTF22CrossThreadTaskHandler8postTaskEONS_15CrossThreadTaskE"));
    Require(ThrowsNotImplemented([&] { postTask(handler.get(), guardTask); }, "_ZN3WTF22CrossThreadTaskHandler8postTaskEONS_15CrossThreadTaskE"));
    Require(ThrowsNotImplemented([&] { postTaskReply(handler.get(), readOnlyTask); }, "_ZN3WTF22CrossThreadTaskHandler13postTaskReplyEONS_15CrossThreadTaskE"));
    Require(ThrowsNotImplemented([&] { postTaskReply(handler.get(), guardTask); }, "_ZN3WTF22CrossThreadTaskHandler13postTaskReplyEONS_15CrossThreadTaskE"));

    VoidFunction badCallableTask{reinterpret_cast<VoidCallableWrapper*>(pages.Guard())};
    Require(ThrowsNotImplemented([&] { postTask(handler.get(), &badCallableTask); }, "_ZN3WTF22CrossThreadTaskHandler8postTaskEONS_15CrossThreadTaskE"));
    Require(badCallableTask.callableWrapper == reinterpret_cast<VoidCallableWrapper*>(pages.Guard()));
    Require(ThrowsNotImplemented([&] { postTaskReply(handler.get(), &badCallableTask); }, "_ZN3WTF22CrossThreadTaskHandler13postTaskReplyEONS_15CrossThreadTaskE"));
    Require(badCallableTask.callableWrapper == reinterpret_cast<VoidCallableWrapper*>(pages.Guard()));

    VoidCallableWrapper badVtableWrapper{reinterpret_cast<const VoidCallableWrapperVtable*>(pages.Guard())};
    VoidFunction badVtableTask{&badVtableWrapper};
    Require(ThrowsNotImplemented([&] { postTask(handler.get(), &badVtableTask); }, "_ZN3WTF22CrossThreadTaskHandler8postTaskEONS_15CrossThreadTaskE"));
    Require(badVtableTask.callableWrapper == &badVtableWrapper);
    Require(ThrowsNotImplemented([&] { postTaskReply(handler.get(), &badVtableTask); }, "_ZN3WTF22CrossThreadTaskHandler13postTaskReplyEONS_15CrossThreadTaskE"));
    Require(badVtableTask.callableWrapper == &badVtableWrapper);

    handler.get()->completionCallback = reinterpret_cast<VoidCallableWrapper*>(pages.Guard());
    Require(ThrowsNotImplemented([&] { destroy(handler.get()); }, "_ZN3WTF22CrossThreadTaskHandlerD1Ev"));
    Require(handler.get()->state != nullptr);
    handler.get()->completionCallback = nullptr;

    CrossThreadTaskHandler mismatchedOwner{};
    mismatchedOwner.state = handler.get()->state;
    mismatchedOwner.completionCallback = nullptr;
    Require(ThrowsNotImplemented([&] { killHandler(&mismatchedOwner); }, "_ZN3WTF22CrossThreadTaskHandler4killEv"));
    Require(ThrowsNotImplemented([&] { destroy(&mismatchedOwner); }, "_ZN3WTF22CrossThreadTaskHandlerD1Ev"));
    Require(ThrowsNotImplemented([&] { postTask(&mismatchedOwner, &emptyTask); }, "_ZN3WTF22CrossThreadTaskHandler8postTaskEONS_15CrossThreadTaskE"));
    Require(ThrowsNotImplemented([&] { postTaskReply(&mismatchedOwner, &emptyTask); }, "_ZN3WTF22CrossThreadTaskHandler13postTaskReplyEONS_15CrossThreadTaskE"));

    handler.Sync();
    DrainMain();
    Require(TakeLog().empty());
    Require(liveCallables.load() == 0);

    for (int round = 0; round < 16; ++round) {
        Handler racing;
        std::atomic<bool> go{false};
        std::atomic<int> destroySucceeded{0};
        std::atomic<int> destroyRejected{0};
        std::thread tDestroy([&] {
            while (!go) std::this_thread::yield();
            try {
                destroy(racing.get());
                ++destroySucceeded;
            } catch (const std::invalid_argument&) {
                ++destroyRejected;
            } catch (const std::runtime_error&) {
                ++destroyRejected;
            }
        });
        std::thread tPost([&] {
            while (!go) std::this_thread::yield();
            VoidFunction job = Make("race", {}, true);
            try {
                postTask(racing.get(), &job);
            } catch (const std::invalid_argument&) {
            } catch (const std::runtime_error&) {
            }
            if (job.callableWrapper) job.callableWrapper->vtable->deletingDestructor(job.callableWrapper);
        });
        go = true;
        try {
            destroy(racing.get());
            ++destroySucceeded;
        } catch (const std::invalid_argument&) {
            ++destroyRejected;
        } catch (const std::runtime_error&) {
            ++destroyRejected;
        }
        tDestroy.join();
        tPost.join();
        Require(destroySucceeded.load() == 1 && destroyRejected.load() == 1);
    }
    Settle();
    DrainMain();
    TakeLog();
    Require(liveCallables.load() == 0);
}

int main() {
    std::thread([] {
        std::this_thread::sleep_for(std::chrono::seconds(20));
        std::fprintf(stderr, "cross thread task handler test timed out\n");
        std::fflush(stderr);
        std::_Exit(3);
    }).detach();
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    _ZN3WTF20initializeMainThreadEv();
    WKRunLoopInitializeMain();
    TestConstruction();
    TestTasks();
    TestReplies();
    TestKill();
    TestTheEmptyTask();
    TestTheCompletionCallback();
    TestDestruction();
    TestTheDeletingDestructor();
    TestNullArguments();
    TestGuestPointers();
    std::puts("libSceNKWebKit cross thread task handler tests passed");
    return 0;
}
