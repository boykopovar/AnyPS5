#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using NKWebKit::WebCoreAbi::AsyncFileStream;
using NKWebKit::WebCoreAbi::FileStreamClient;
using NKWebKit::WebCoreAbi::FileStreamClientCallable;
using NKWebKit::WebCoreAbi::FileStreamClientCallableVtable;
using NKWebKit::WebCoreAbi::FileStreamClientFunction;
using NKWebKit::WebCoreAbi::FileStreamClientVtable;
using NKWebKit::WebCoreAbi::FileStreamOperationCallable;
using NKWebKit::WebCoreAbi::FileStreamOperationCallableVtable;
using NKWebKit::WebCoreAbi::FileStreamOperationFunction;
using NKWebKit::WebCoreAbi::OptionalWallTime;
using NKWebKit::WtfAbi::String;
using StreamState = NKWebKit::WebCoreAbi::FileStream;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "async file stream check failed at line %d\n", line);
        std::fflush(stderr);
        std::_Exit(1);
    }
}
#define Require(value) Check((value), __LINE__)

constexpr auto construct = &_ZN7WebCore15AsyncFileStreamC1ERNS_16FileStreamClientE;
constexpr auto destroy = &_ZN7WebCore15AsyncFileStreamD1Ev;
constexpr auto openForRead = &_ZN7WebCore15AsyncFileStream11openForReadERKN3WTF6StringExx;
constexpr auto closeStream = &_ZN7WebCore15AsyncFileStream5closeEv;
constexpr auto perform = &_ZN7WebCore15AsyncFileStream7performEON3WTF8FunctionIFNS2_IFvRNS_16FileStreamClientEEEERNS_10FileStreamEEEE;
constexpr auto readStream = &_ZN7WebCore15AsyncFileStream4readEPci;
constexpr auto getSize = &_ZN7WebCore15AsyncFileStream7getSizeERKN3WTF6StringENS1_8OptionalINS1_8WallTimeEEE;

static const char kContent[] = "0123456789abcdef";
static const char kDataPath[] = "/nkwebkit-afs/data.bin";

template<typename TFunction>
static bool ThrowsInvalidArgument(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static String Guest(const char* path) {
    return _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(path));
}

struct Client {
    FileStreamClient base;
    std::vector<std::string> events;
    bool alwaysOnMainThread = true;
};

static Client& ClientOf(FileStreamClient* self) {
    return *reinterpret_cast<Client*>(self);
}

static void Record(FileStreamClient* self, const std::string& event) {
    Client& client = ClientOf(self);
    if (!_ZN3WTF12isMainThreadEv()) client.alwaysOnMainThread = false;
    client.events.push_back(event);
}

static void APS5_VABI DidOpen(FileStreamClient* self, bool success) { Record(self, std::string("open:") + (success ? "1" : "0")); }
static void APS5_VABI DidGetSize(FileStreamClient* self, long long size) { Record(self, "size:" + std::to_string(size)); }
static void APS5_VABI DidRead(FileStreamClient* self, int bytesRead) { Record(self, "read:" + std::to_string(bytesRead)); }
static void APS5_VABI DidWrite(FileStreamClient* self, int) { Record(self, "write"); }
static void APS5_VABI DidTruncate(FileStreamClient* self, bool) { Record(self, "truncate"); }
static void APS5_VABI ClientDestructor(FileStreamClient* self) { Record(self, "destructor"); }

static const FileStreamClientVtable clientVtable = {DidOpen, DidGetSize, DidRead, DidWrite, DidTruncate, ClientDestructor, ClientDestructor};

static std::atomic<int> liveOperations{0};
static std::atomic<int> liveWorks{0};
static std::atomic<int> operationsDeletedOnMainThread{0};
static std::atomic<int> worksDeletedOffMainThread{0};

struct Work {
    FileStreamClientCallable base;
    std::function<void(FileStreamClient*)> body;
};

struct Operation {
    FileStreamOperationCallable base;
    std::function<Work*(StreamState*)> body;
};

static void APS5_VABI WorkCompleteDestructor(FileStreamClientCallable*) { Require(false); }
static void APS5_VABI WorkDeletingDestructor(FileStreamClientCallable* self) {
    if (!_ZN3WTF12isMainThreadEv()) ++worksDeletedOffMainThread;
    --liveWorks;
    delete reinterpret_cast<Work*>(self);
}
static void APS5_VABI WorkCall(FileStreamClientCallable* self, FileStreamClient* client) {
    Require(_ZN3WTF12isMainThreadEv());
    reinterpret_cast<Work*>(self)->body(client);
}
static const FileStreamClientCallableVtable workVtable = {WorkCompleteDestructor, WorkDeletingDestructor, WorkCall};

static Work* MakeWork(std::function<void(FileStreamClient*)> body) {
    ++liveWorks;
    return new Work{{&workVtable}, std::move(body)};
}

static void APS5_VABI OperationCompleteDestructor(FileStreamOperationCallable*) { Require(false); }
static void APS5_VABI OperationDeletingDestructor(FileStreamOperationCallable* self) {
    if (_ZN3WTF12isMainThreadEv()) ++operationsDeletedOnMainThread;
    --liveOperations;
    delete reinterpret_cast<Operation*>(self);
}
static FileStreamClientFunction APS5_VABI OperationCall(FileStreamOperationCallable* self, StreamState* stream) {
    Require(!_ZN3WTF12isMainThreadEv());
    Work* work = reinterpret_cast<Operation*>(self)->body(stream);
    return FileStreamClientFunction(work ? &work->base : nullptr);
}
static const FileStreamOperationCallableVtable operationVtable = {OperationCompleteDestructor, OperationDeletingDestructor, OperationCall};

static void Perform(AsyncFileStream& stream, std::function<Work*(StreamState*)> body) {
    ++liveOperations;
    auto* operation = new Operation{{&operationVtable}, std::move(body)};
    FileStreamOperationFunction function{&operation->base};
    perform(&stream, &function);
    Require(function.callable == nullptr);
}

static AsyncFileStream barrier{};
static Client barrierClient{{&clientVtable}, {}, true};

static void Drain() {
    Perform(barrier, [](StreamState*) { return MakeWork([](FileStreamClient*) { WKRunLoopStopMain(); }); });
    WKRunLoopRunMain();
    WKRunLoopInitializeMain();
}

struct Snapshot {
    StreamState stream{};
    StreamState* address = nullptr;
    std::thread::id thread;
};

static Snapshot Inspect(AsyncFileStream& stream) {
    Snapshot snapshot;
    Perform(stream, [&snapshot](StreamState* value) {
        snapshot.stream = *value;
        snapshot.address = value;
        snapshot.thread = std::this_thread::get_id();
        return MakeWork([](FileStreamClient*) {});
    });
    Drain();
    return snapshot;
}

struct Stream {
    AsyncFileStream stream{};
    Client client{{&clientVtable}, {}, true};

    Stream() {
        construct(&stream, &client.base);
    }

    ~Stream() {
        if (stream.internals) {
            destroy(&stream);
            Drain();
        }
    }

    const std::string& Last() const {
        Require(!client.events.empty());
        return client.events.back();
    }
};

static std::string OpenStream(Stream& stream, const char* path, long long offset, long long length) {
    {
        String guestPath = Guest(path);
        openForRead(&stream.stream, &guestPath, offset, length);
    }
    Drain();
    return stream.Last();
}

static std::string ReadStream(Stream& stream, char* buffer, int length) {
    readStream(&stream.stream, buffer, length);
    Drain();
    return stream.Last();
}

static std::string SizeOf(Stream& stream, const String& path, const OptionalWallTime& expected) {
    getSize(&stream.stream, &path, &expected);
    Drain();
    return stream.Last();
}

static void TestConstruction() {
    Client client{{&clientVtable}, {}, true};
    AsyncFileStream stream{};
    construct(&stream, &client.base);
    Require(stream.internals != nullptr);
    const Snapshot snapshot = Inspect(stream);
    Require(snapshot.stream.handle == -1 && snapshot.stream.bytesProcessed == 0 && snapshot.stream.totalBytesToRead == 0);
    Require(client.events.empty());
    destroy(&stream);
    Require(stream.internals == nullptr);
    Drain();
    Require(client.events.empty());

    AsyncFileStream second{};
    _ZN7WebCore15AsyncFileStreamC2ERNS_16FileStreamClientE(&second, &client.base);
    Require(second.internals != nullptr);
    _ZN7WebCore15AsyncFileStreamD2Ev(&second);
    Require(second.internals == nullptr);
    Drain();
}

static void TestOpenAndRead() {
    Stream stream;
    Require(OpenStream(stream, kDataPath, 4, 6) == "open:1");
    Require(stream.client.events.size() == 1 && stream.client.alwaysOnMainThread);
    const Snapshot opened = Inspect(stream.stream);
    Require(opened.stream.handle != -1 && opened.stream.totalBytesToRead == 6 && opened.stream.bytesProcessed == 0);

    char buffer[40] = {};
    Require(ReadStream(stream, buffer, 4) == "read:4" && std::memcmp(buffer, "4567", 4) == 0);
    Require(Inspect(stream.stream).stream.bytesProcessed == 4);
    Require(ReadStream(stream, buffer, 4) == "read:2" && std::memcmp(buffer, "8967", 4) == 0);
    Require(ReadStream(stream, buffer, 4) == "read:0");
    Require(stream.client.events.size() == 4);

    Require(OpenStream(stream, "/nkwebkit-afs/missing.bin", 0, 3) == "open:1");
    const Snapshot again = Inspect(stream.stream);
    Require(again.stream.handle == opened.stream.handle && again.stream.totalBytesToRead == 6 && again.stream.bytesProcessed == 6);
    Require(again.address == opened.address && again.thread == opened.thread && again.thread != std::this_thread::get_id());

    closeStream(&stream.stream);
    Require(Inspect(stream.stream).stream.handle == -1);
    Require(stream.client.events.size() == 5);
    Require(ReadStream(stream, buffer, 4) == "read:-1");
    closeStream(&stream.stream);
    Require(Inspect(stream.stream).stream.handle == -1);

    Require(OpenStream(stream, kDataPath, 0, 100) == "open:1");
    Require(ReadStream(stream, buffer, 32) == "read:16" && std::memcmp(buffer, kContent, 16) == 0);
    Require(ReadStream(stream, buffer, 32) == "read:0");
    Require(Inspect(stream.stream).stream.bytesProcessed == 16);
    closeStream(&stream.stream);

    Require(OpenStream(stream, kDataPath, -5, 3) == "open:1");
    Require(ReadStream(stream, buffer, 8) == "read:3" && std::memcmp(buffer, "012", 3) == 0);
    closeStream(&stream.stream);

    Require(OpenStream(stream, kDataPath, 2, -(1LL << 32) + 5) == "open:1");
    Require(ReadStream(stream, buffer, 8) == "read:5" && std::memcmp(buffer, "23456", 5) == 0);
    Require(ReadStream(stream, buffer, 8) == "read:0");
    closeStream(&stream.stream);

    Require(OpenStream(stream, kDataPath, 0, -1) == "open:1");
    Require(ReadStream(stream, buffer, 8) == "read:0");
    closeStream(&stream.stream);

    Require(OpenStream(stream, kDataPath, 0, 10) == "open:1");
    Require(ReadStream(stream, buffer, 0) == "read:0");
    Require(ReadStream(stream, buffer, -3) == "read:0");
    Require(Inspect(stream.stream).stream.bytesProcessed == 0);
    Require(ReadStream(stream, buffer, 3) == "read:3" && std::memcmp(buffer, "012", 3) == 0);
    closeStream(&stream.stream);

    Require(OpenStream(stream, kDataPath, 100, 10) == "open:1");
    Require(ReadStream(stream, buffer, 4) == "read:0");
    closeStream(&stream.stream);

    Require(OpenStream(stream, kDataPath, 0, 10) == "open:1");
    std::memset(buffer, 0, sizeof(buffer));
    readStream(&stream.stream, buffer, 3);
    closeStream(&stream.stream);
    Drain();
    Require(stream.Last() == "read:3" && std::memcmp(buffer, "012", 3) == 0);
    Require(Inspect(stream.stream).stream.handle == -1);

    Require(OpenStream(stream, "/nkwebkit-afs/missing.bin", 0, 3) == "open:0");
    Require(Inspect(stream.stream).stream.handle == -1);
    Require(ReadStream(stream, buffer, 4) == "read:-1");
    Require(stream.client.alwaysOnMainThread);
}

static void TestAFailedSeek() {
    constexpr long long farthest = std::numeric_limits<long long>::max();
    const int probe = open_nid_postfix(kDataPath, 0, 0);
    Require(probe >= 0);
    const bool hostRefuses = lseek_nid_postfix(probe, farthest, 0) < 0;
    Require(close_nid_postfix(probe) == 0);
    Stream stream;
    if (!hostRefuses) {
        Require(OpenStream(stream, kDataPath, farthest, 4) == "open:1");
        std::puts("the host seeks to the largest offset, so the failed seek is not exercised");
        return;
    }
    Require(OpenStream(stream, kDataPath, farthest, 4) == "open:0");
    Require(Inspect(stream.stream).stream.handle == -1);
    Require(OpenStream(stream, kDataPath, 2, 4) == "open:1");
    const Snapshot reopened = Inspect(stream.stream);
    Require(reopened.stream.handle != -1 && reopened.stream.totalBytesToRead == 4 && reopened.stream.bytesProcessed == 0);
    char buffer[8] = {};
    Require(ReadStream(stream, buffer, 4) == "read:4" && std::memcmp(buffer, "2345", 4) == 0);
}

static void TestGetSize() {
    Stream stream;
    const String path = Guest(kDataPath);
    FileStat status{};
    Require(stat_nid_postfix(kDataPath, &status) == 0);
    const double modified = static_cast<double>(status.st_mtim.tv_sec);
    Require(modified > 1.0);

    Require(SizeOf(stream, path, OptionalWallTime()) == "size:16");
    Require(SizeOf(stream, path, OptionalWallTime(modified)) == "size:16");
    Require(SizeOf(stream, path, OptionalWallTime(modified + 0.75)) == "size:16");
    Require(SizeOf(stream, path, OptionalWallTime(modified + 1)) == "size:-1");
    Require(SizeOf(stream, path, OptionalWallTime(modified - 0.25)) == "size:-1");
    Require(SizeOf(stream, path, OptionalWallTime(1e30)) == "size:-1");
    Require(SizeOf(stream, path, OptionalWallTime(-1e30)) == "size:-1");
    Require(SizeOf(stream, path, OptionalWallTime(std::numeric_limits<double>::infinity())) == "size:-1");

    OptionalWallTime leftover;
    leftover.value = std::numeric_limits<double>::quiet_NaN();
    Require(SizeOf(stream, path, leftover) == "size:16");

    const String missing = Guest("/nkwebkit-afs/missing.bin");
    Require(SizeOf(stream, missing, OptionalWallTime()) == "size:-1");
    const String empty = Guest("");
    Require(SizeOf(stream, empty, OptionalWallTime()) == "size:-1");
    const String null;
    Require(SizeOf(stream, null, OptionalWallTime()) == "size:-1");
    Require(SizeOf(stream, path, OptionalWallTime()) == "size:16");
    Require(stream.client.events.size() == 13 && stream.client.alwaysOnMainThread);
    Require(Inspect(stream.stream).stream.handle == -1);

    const OptionalWallTime notANumber(std::numeric_limits<double>::quiet_NaN());
    Require(ThrowsInvalidArgument([&] { getSize(&stream.stream, &path, &notANumber); }));
    Drain();
    Require(stream.client.events.size() == 13);
}

static void TestTheCallerKeepsItsPath() {
    Stream stream;
    Stream gate;
    std::atomic<bool> started{false};
    std::atomic<bool> release{false};
    Perform(gate.stream, [&](StreamState*) {
        started = true;
        while (!release) std::this_thread::yield();
        return MakeWork([](FileStreamClient*) {});
    });
    while (!started) std::this_thread::yield();
    String path = Guest(kDataPath);
    const unsigned before = path.impl->refCount;
    openForRead(&stream.stream, &path, 0, 4);
    const OptionalWallTime none;
    getSize(&stream.stream, &path, &none);
    Require(path.impl->refCount == before);
    release = true;
    Drain();
    Require(path.impl->refCount == before);
    Require(stream.client.events.size() == 2 && stream.client.events[0] == "open:1" && stream.client.events[1] == "size:16");
}

static void TestPerform() {
    Stream stream;
    std::vector<std::string> order;
    std::thread::id firstThread;
    std::thread::id secondThread;
    StreamState* firstStream = nullptr;
    StreamState* secondStream = nullptr;
    FileStreamClient* seenClient = nullptr;
    const int operationsBefore = liveOperations.load();
    const int worksBefore = liveWorks.load();
    Perform(stream.stream, [&](StreamState* value) {
        firstThread = std::this_thread::get_id();
        firstStream = value;
        return MakeWork([&](FileStreamClient* client) {
            seenClient = client;
            order.push_back("first");
        });
    });
    Perform(stream.stream, [&](StreamState* value) {
        secondThread = std::this_thread::get_id();
        secondStream = value;
        return MakeWork([&](FileStreamClient*) { order.push_back("second"); });
    });
    Require(order.empty());
    Drain();
    Require(order.size() == 2 && order[0] == "first" && order[1] == "second");
    Require(seenClient == &stream.client.base);
    Require(firstThread == secondThread && firstThread != std::this_thread::get_id());
    Require(firstStream != nullptr && firstStream == secondStream);
    Require(liveOperations.load() == operationsBefore && liveWorks.load() == worksBefore);
    Require(operationsDeletedOnMainThread.load() == 0 && worksDeletedOffMainThread.load() == 0);
    Require(stream.client.events.empty());

    Perform(stream.stream, [](StreamState*) -> Work* { return nullptr; });
    bool threw = false;
    try {
        Drain();
    } catch (const std::logic_error&) {
        threw = true;
        WKRunLoopRunMain();
        WKRunLoopInitializeMain();
    }
    Require(threw);
    Require(liveOperations.load() == operationsBefore && liveWorks.load() == worksBefore);

    Require(ThrowsInvalidArgument([&] { perform(&stream.stream, nullptr); }));
    FileStreamOperationFunction none{nullptr};
    Require(ThrowsInvalidArgument([&] { perform(&stream.stream, &none); }));
    Drain();
    Require(stream.client.events.empty());
}

static void TestDestruction() {
    const int operationsBefore = liveOperations.load();
    const int worksBefore = liveWorks.load();
    {
        Client client{{&clientVtable}, {}, true};
        AsyncFileStream stream{};
        construct(&stream, &client.base);
        std::atomic<bool> started{false};
        std::atomic<bool> release{false};
        std::atomic<bool> firstWorkRan{false};
        std::atomic<bool> secondCalled{false};
        Perform(stream, [&](StreamState*) {
            started = true;
            while (!release) std::this_thread::yield();
            return MakeWork([&](FileStreamClient*) { firstWorkRan = true; });
        });
        Perform(stream, [&](StreamState*) {
            secondCalled = true;
            return MakeWork([](FileStreamClient*) {});
        });
        while (!started) std::this_thread::yield();
        destroy(&stream);
        release = true;
        Drain();
        Require(!firstWorkRan && !secondCalled);
        Require(client.events.empty());
        Require(liveOperations.load() == operationsBefore && liveWorks.load() == worksBefore);
        Require(operationsDeletedOnMainThread.load() == 0 && worksDeletedOffMainThread.load() == 0);
    }
    {
        Client client{{&clientVtable}, {}, true};
        AsyncFileStream stream{};
        construct(&stream, &client.base);
        {
            String path = Guest(kDataPath);
            openForRead(&stream, &path, 0, 10);
        }
        Drain();
        Require(client.events.size() == 1 && client.events[0] == "open:1");
        const int handle = Inspect(stream).stream.handle;
        Require(handle != -1 && lseek_nid_postfix(handle, 0, 1) == 0);
        char buffer[8] = {};
        readStream(&stream, buffer, 4);
        {
            String path = Guest(kDataPath);
            const OptionalWallTime none;
            getSize(&stream, &path, &none);
            openForRead(&stream, &path, 0, 10);
        }
        destroy(&stream);
        Drain();
        Require(client.events.size() == 1);
        Require(lseek_nid_postfix(handle, 0, 1) < 0);

        String path = Guest(kDataPath);
        const OptionalWallTime none;
        FileStreamOperationFunction function{nullptr};
        Require(ThrowsInvalidArgument([&] { openForRead(&stream, &path, 0, 1); }));
        Require(ThrowsInvalidArgument([&] { closeStream(&stream); }));
        Require(ThrowsInvalidArgument([&] { readStream(&stream, buffer, 1); }));
        Require(ThrowsInvalidArgument([&] { getSize(&stream, &path, &none); }));
        Require(ThrowsInvalidArgument([&] { perform(&stream, &function); }));
        Require(ThrowsInvalidArgument([&] { destroy(&stream); }));
        Require(ThrowsInvalidArgument([&] { _ZN7WebCore15AsyncFileStreamD2Ev(&stream); }));
        Drain();
        Require(client.events.size() == 1);
    }
}

static void TestNullArguments() {
    Client client{{&clientVtable}, {}, true};
    AsyncFileStream stream{};
    Require(ThrowsInvalidArgument([&] { construct(nullptr, &client.base); }));
    Require(ThrowsInvalidArgument([&] { construct(&stream, nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore15AsyncFileStreamC2ERNS_16FileStreamClientE(&stream, nullptr); }));
    Require(stream.internals == nullptr);
    construct(&stream, &client.base);
    const String path = Guest(kDataPath);
    const OptionalWallTime none;
    char buffer[4] = {};
    FileStreamOperationFunction function{nullptr};
    Require(ThrowsInvalidArgument([&] { openForRead(nullptr, &path, 0, 1); }));
    Require(ThrowsInvalidArgument([&] { openForRead(&stream, nullptr, 0, 1); }));
    Require(ThrowsInvalidArgument([&] { closeStream(nullptr); }));
    Require(ThrowsInvalidArgument([&] { readStream(nullptr, buffer, 1); }));
    Require(ThrowsInvalidArgument([&] { getSize(nullptr, &path, &none); }));
    Require(ThrowsInvalidArgument([&] { getSize(&stream, nullptr, &none); }));
    Require(ThrowsInvalidArgument([&] { getSize(&stream, &path, nullptr); }));
    Require(ThrowsInvalidArgument([&] { perform(nullptr, &function); }));
    Require(ThrowsInvalidArgument([&] { destroy(nullptr); }));
    Drain();
    Require(client.events.empty());
    Require(Inspect(stream).stream.handle == -1);
    destroy(&stream);
    Drain();
}

struct GuardedPages {
    unsigned char* base = nullptr;
    std::size_t pageSize = 0;

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

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 3);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }
};

template<typename TFunction>
static void RequireNotImplemented(TFunction&& function) {
    const auto before = NotImplementedCalls_nid_no_patch();
    bool threw = false;
    try {
        function();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(NotImplementedCalls_nid_no_patch() == before + 1);
}

static void TestGuestPointers() {
    GuardedPages pages;
    auto* guardStream = reinterpret_cast<AsyncFileStream*>(pages.Guard());
    auto* roStream = reinterpret_cast<AsyncFileStream*>(pages.ReadOnly());
    auto* guardClient = reinterpret_cast<FileStreamClient*>(pages.Guard());
    const auto* guardPath = reinterpret_cast<const String*>(pages.Guard());
    auto* guardOp = reinterpret_cast<FileStreamOperationFunction*>(pages.Guard());
    auto* roOp = reinterpret_cast<FileStreamOperationFunction*>(pages.ReadOnly());
    const auto* guardTime = reinterpret_cast<const OptionalWallTime*>(pages.Guard());

    Client client{{&clientVtable}, {}, true};
    AsyncFileStream stream{};
    RequireNotImplemented([&] { construct(guardStream, &client.base); });
    RequireNotImplemented([&] { construct(roStream, &client.base); });
    RequireNotImplemented([&] { construct(&stream, guardClient); });
    Require(stream.internals == nullptr);

    FileStreamClient badClient{reinterpret_cast<const FileStreamClientVtable*>(pages.Guard())};
    RequireNotImplemented([&] { construct(&stream, &badClient); });
    Require(stream.internals == nullptr);

    construct(&stream, &client.base);
    const String path = Guest(kDataPath);
    const OptionalWallTime none;
    char buffer[8] = {};

    RequireNotImplemented([&] { destroy(guardStream); });
    RequireNotImplemented([&] { destroy(roStream); });
    RequireNotImplemented([&] { openForRead(guardStream, &path, 0, 4); });
    RequireNotImplemented([&] { openForRead(&stream, guardPath, 0, 4); });
    RequireNotImplemented([&] { closeStream(guardStream); });
    RequireNotImplemented([&] { readStream(guardStream, buffer, 4); });
    RequireNotImplemented([&] { readStream(&stream, reinterpret_cast<char*>(pages.Guard()), 4); });
    RequireNotImplemented([&] { readStream(&stream, reinterpret_cast<char*>(pages.ReadOnly()), 4); });
    RequireNotImplemented([&] { getSize(guardStream, &path, &none); });
    RequireNotImplemented([&] { getSize(&stream, guardPath, &none); });
    RequireNotImplemented([&] { getSize(&stream, &path, guardTime); });
    RequireNotImplemented([&] { perform(guardStream, roOp); });
    RequireNotImplemented([&] { perform(&stream, guardOp); });
    RequireNotImplemented([&] { perform(&stream, roOp); });

    FileStreamOperationFunction badCallable{reinterpret_cast<FileStreamOperationCallable*>(pages.Guard())};
    RequireNotImplemented([&] { perform(&stream, &badCallable); });
    Require(badCallable.callable == reinterpret_cast<FileStreamOperationCallable*>(pages.Guard()));

    NKWebKit::WtfAbi::StringImpl badPathImpl{2, 8, pages.Guard(), 4};
    String badPathStr;
    badPathStr.impl = &badPathImpl;
    RequireNotImplemented([&] { openForRead(&stream, &badPathStr, 0, 4); });
    RequireNotImplemented([&] { getSize(&stream, &badPathStr, &none); });
    badPathStr.impl = nullptr;

    Drain();
    Require(client.events.empty());
    destroy(&stream);
    Drain();
}

int main() {
    std::thread([] {
        std::this_thread::sleep_for(std::chrono::seconds(60));
        std::fprintf(stderr, "async file stream test timed out\n");
        std::fflush(stderr);
        std::_Exit(3);
    }).detach();
    const auto root = std::filesystem::current_path() / ("nkwebkit-afs-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto hostPath = root.u8string();
    AddPathAlias_nid_no_patch("/nkwebkit-afs", reinterpret_cast<const char*>(hostPath.c_str()));
    {
        std::ofstream file(root / "data.bin", std::ios::binary);
        file << kContent;
    }
    _ZN3WTF20initializeMainThreadEv();
    WKRunLoopInitializeMain();
    construct(&barrier, &barrierClient.base);
    TestConstruction();
    TestOpenAndRead();
    TestAFailedSeek();
    TestGetSize();
    TestTheCallerKeepsItsPath();
    TestPerform();
    TestDestruction();
    TestNullArguments();
    TestGuestPointers();
    Require(barrierClient.events.empty());
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::puts("libSceNKWebKit async file stream tests passed");
    return 0;
}
