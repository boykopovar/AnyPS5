#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

struct MemorySizeQueryParams {
    std::uint32_t version;
    std::uint32_t workers;
    std::uint32_t sequences;
    std::uint32_t invocations;
    std::uint32_t dispatches;
    std::uint32_t syncPoints;
    std::uint32_t waits;
    std::uint32_t computeSequences;
    std::uint64_t computeQueueMask;
};

struct ConfigParams {
    std::uint32_t version;
    std::uint32_t threadFlags;
    std::uint32_t stackSize;
    std::uint32_t priority;
    const char* threadName;
    std::uint32_t spinMicroseconds;
    std::uint32_t computeAffinity;
    std::uint32_t computePriority;
    std::uint32_t reserved;
};

using JobEntry = void (APS5_VABI*)(void*, std::uint32_t);

struct Sequence {
    APS5_VABI virtual int Dispatch(JobEntry, void*, std::uint32_t, const char*) = 0;
    APS5_VABI virtual int SyncPoint() = 0;
    APS5_VABI virtual int Wait() = 0;
    APS5_VABI virtual void CompleteDestructor() = 0;
    APS5_VABI virtual void DeletingDestructor() = 0;
};

struct SequenceFactory {
    APS5_VABI virtual int Create(Sequence**, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, const char*) = 0;
    APS5_VABI virtual int Destroy(Sequence*) = 0;
    APS5_VABI virtual void CompleteDestructor() = 0;
    APS5_VABI virtual void DeletingDestructor() = 0;
    APS5_VABI virtual int CreateCompute(Sequence**, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, const char*) = 0;
    APS5_VABI virtual int DestroyCompute(Sequence*) = 0;
};

extern "C" {
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI pthread_getcpuclockid_nid_postfix(Pthread thread, int* clockId);
int APS5_VABI sceKernelClockGettime(KernelClockid clockId, KernelTimespec* time);
int APS5_VABI sceKernelUsleep_nid_postfix(KernelUseconds microseconds);
void APS5_VABI _ZN3sce3Job10JobManagerC1Ev(void*);
void APS5_VABI _ZN3sce3Job10JobManagerD1Ev(void*);
std::uint64_t APS5_VABI _ZN3sce3Job10JobManager27calculateRequiredMemorySizeEPKNS1_21MemorySizeQueryParamsE(const MemorySizeQueryParams*);
int APS5_VABI _ZN3sce3Job10JobManager10initializeEPvmjjjmPKNS1_12ConfigParamsE(void*, void*, std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint64_t, const ConfigParams*);
int APS5_VABI _ZN3sce3Job10JobManager10initializeEPvmjjPKNS1_12ConfigParamsE(void*, void*, std::uint64_t, std::uint32_t, std::uint32_t, const ConfigParams*);
int APS5_VABI _ZN3sce3Job10JobManager8shutdownEv(void*);
SequenceFactory* APS5_VABI _ZN3sce3Job10JobManager27getSequenceFactoryInterfaceENS1_8PriorityE(void*, std::uint32_t);
SequenceFactory* APS5_VABI _ZN3sce3Job10JobManager34getUnifiedSequenceFactoryInterfaceENS1_8PriorityE(void*, std::uint32_t);
int APS5_VABI _ZN3sce3Job10JobManager24setPersistentThreadCountEj(void*, std::uint32_t);
}

#define Construct _ZN3sce3Job10JobManagerC1Ev
#define Destruct _ZN3sce3Job10JobManagerD1Ev
#define RequiredSize _ZN3sce3Job10JobManager27calculateRequiredMemorySizeEPKNS1_21MemorySizeQueryParamsE
#define InitializeUnified _ZN3sce3Job10JobManager10initializeEPvmjjjmPKNS1_12ConfigParamsE
#define Initialize _ZN3sce3Job10JobManager10initializeEPvmjjPKNS1_12ConfigParamsE
#define Shutdown _ZN3sce3Job10JobManager8shutdownEv
#define Factories _ZN3sce3Job10JobManager27getSequenceFactoryInterfaceENS1_8PriorityE
#define UnifiedFactories _ZN3sce3Job10JobManager34getUnifiedSequenceFactoryInterfaceENS1_8PriorityE
#define SetPersistentThreadCount _ZN3sce3Job10JobManager24setPersistentThreadCountEj

static constexpr int InvalidArgument = static_cast<int>(0x81581000u);
static constexpr int NullMemory = static_cast<int>(0x81581001u);
static constexpr int MemoryTooSmall = static_cast<int>(0x81581002u);
static constexpr int NotInitialized = static_cast<int>(0x81581010u);
static constexpr int AlreadyInitialized = static_cast<int>(0x81581020u);
static constexpr int MisalignedMemory = static_cast<int>(0x81581030u);
static constexpr int OutOfRange = static_cast<int>(0x81580000u);
static constexpr int NullArgument = static_cast<int>(0x81580001u);
static constexpr int NoNodes = static_cast<int>(0x81580002u);
static constexpr int LimitExceeded = static_cast<int>(0x81580003u);
static constexpr int NoneFree = static_cast<int>(0x81580006u);
static constexpr int Invalid = static_cast<int>(0x81580007u);

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "job manager check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

struct Block {
    explicit Block(std::uint64_t size) : bytes(size), memory(::operator new((size + 255) & ~std::uint64_t{255}, std::align_val_t{256})) {}
    ~Block() { ::operator delete(memory, std::align_val_t{256}); }
    std::uint64_t bytes;
    void* memory;
};

struct ManagerObject {
    ManagerObject() { Construct(storage); }
    ~ManagerObject() { Destruct(storage); }
    alignas(64) unsigned char storage[0x100]{};
};

static std::uint64_t SizeFor(std::uint32_t workers, std::uint32_t sequences, std::uint32_t dispatches, std::uint32_t syncPoints, std::uint32_t waits) {
    const MemorySizeQueryParams query{0, workers, sequences, 0, dispatches, syncPoints, waits, 0, 0};
    return RequiredSize(&query);
}

static void MemoryCase() {
    const MemorySizeQueryParams query{0, 4, 2, 100, 8, 4, 2, 0, 0};
    Require(RequiredSize(&query) == 0x17c8);
    const MemorySizeQueryParams unified{1, 4, 2, 100, 8, 4, 2, 0, 0};
    Require(RequiredSize(&unified) == 0x17c8);
    const MemorySizeQueryParams single{0, 1, 1, 0, 0, 0, 0, 0, 0};
    Require(RequiredSize(&single) == 0x1090);
    Require(RequiredSize(nullptr) == 0);
    for (const MemorySizeQueryParams& bad : {
             MemorySizeQueryParams{2, 4, 2, 0, 0, 0, 0, 0, 0}, MemorySizeQueryParams{0, 0, 2, 0, 0, 0, 0, 0, 0},
             MemorySizeQueryParams{0, 14, 2, 0, 0, 0, 0, 0, 0}, MemorySizeQueryParams{0, 4, 0, 0, 0, 0, 0, 0, 0},
             MemorySizeQueryParams{0, 4, 33, 0, 0, 0, 0, 0, 0}, MemorySizeQueryParams{0, 4, 2, 2 * 0x7fff + 1, 0, 0, 0, 0, 0},
             MemorySizeQueryParams{0, 4, 2, 0, 2 * 0x3ff + 1, 0, 0, 0, 0}, MemorySizeQueryParams{0, 4, 2, 0, 0, 2 * 0xff + 1, 0, 0, 0},
             MemorySizeQueryParams{0, 4, 2, 0, 0, 0, 2 * 0x3f + 1, 0, 0}, MemorySizeQueryParams{1, 4, 0, 0, 0, 0, 0, 0, 0},
             MemorySizeQueryParams{1, 4, 2, 0, 0, 0, 0, 0, std::uint64_t{1} << 56}}) {
        Require(RequiredSize(&bad) == 0);
    }
}

static void APS5_VABI Nothing(void*, std::uint32_t) {
}

static void ValidationCase() {
    Block block(SizeFor(2, 2, 16, 4, 4));
    ManagerObject manager;
    Require(Shutdown(manager.storage) == NotInitialized);
    Require(SetPersistentThreadCount(manager.storage, 0) == NotInitialized);
    Require(Factories(manager.storage, 0) == nullptr && UnifiedFactories(manager.storage, 1) == nullptr);
    Require(Initialize(manager.storage, nullptr, block.bytes, 2, 2, nullptr) == NullMemory);
    Require(Initialize(manager.storage, static_cast<char*>(block.memory) + 16, block.bytes - 16, 2, 2, nullptr) == MisalignedMemory);
    Require(Initialize(manager.storage, block.memory, 0, 2, 2, nullptr) == InvalidArgument);
    Require(Initialize(manager.storage, block.memory, block.bytes, 0, 2, nullptr) == InvalidArgument);
    Require(Initialize(manager.storage, block.memory, block.bytes, 14, 2, nullptr) == InvalidArgument);
    Require(Initialize(manager.storage, block.memory, block.bytes, 2, 0, nullptr) == InvalidArgument);
    Require(Initialize(manager.storage, block.memory, block.bytes, 2, 33, nullptr) == InvalidArgument);
    Require(InitializeUnified(manager.storage, block.memory, block.bytes, 2, 0, 0, 0, nullptr) == InvalidArgument);
    Require(InitializeUnified(manager.storage, block.memory, block.bytes, 2, 2, 0, std::uint64_t{1} << 56, nullptr) == InvalidArgument);
    Require(Initialize(manager.storage, block.memory, 8, 2, 2, nullptr) == MemoryTooSmall);
    Require(Initialize(manager.storage, block.memory, 0x400, 2, 2, nullptr) == MemoryTooSmall);
    ConfigParams config{0, 0x00021fff, 0x10000, 0x2bc, nullptr, 0, 0x1fff, 0x100, 0};
    for (int field = 0; field < 7; ++field) {
        ConfigParams bad = config;
        if (field == 0) bad.version = 2;
        if (field == 1) bad.threadFlags = 0x00001fff;
        if (field == 2) bad.threadFlags = 0x00020001;
        if (field == 3) bad.stackSize = 0x3fff;
        if (field == 4) bad.priority = 0x300;
        if (field == 5) bad.spinMicroseconds = 0x33;
        if (field == 6) bad.reserved = 1;
        Require(Initialize(manager.storage, block.memory, block.bytes, 2, 2, &bad) == InvalidArgument);
        Require(Factories(manager.storage, 0) == nullptr);
    }
    config.threadFlags = 0x00010003;
    config.threadName = "JobManagerTestWorker";
    Require(Initialize(manager.storage, block.memory, block.bytes, 2, 2, &config) == 0);
    std::uint16_t magic = 0;
    std::memcpy(&magic, block.memory, sizeof(magic));
    Require(magic == 0x4d4a);
    Require(Initialize(manager.storage, block.memory, block.bytes, 2, 2, nullptr) == AlreadyInitialized);
    Require(SetPersistentThreadCount(manager.storage, 3) == InvalidArgument);
    Require(SetPersistentThreadCount(manager.storage, 2) == 0);
    SequenceFactory* factory = Factories(manager.storage, 0);
    Require(factory != nullptr && Factories(manager.storage, 1) != nullptr && factory != Factories(manager.storage, 1));
    Require(UnifiedFactories(manager.storage, 0) != nullptr);
    Sequence* sequence = nullptr;
    Require(factory->Create(nullptr, 1, 1, 1, 1, nullptr) == NullArgument);
    Require(factory->Create(&sequence, 0, 0, 0, 0, nullptr) == OutOfRange);
    Require(factory->Create(&sequence, 0x8000, 1, 1, 1, nullptr) == OutOfRange);
    Require(factory->Create(&sequence, 1, 0x400, 1, 1, nullptr) == OutOfRange);
    Require(factory->Create(&sequence, 1, 1, 0x100, 1, nullptr) == OutOfRange);
    Require(factory->Create(&sequence, 1, 1, 1, 0x40, nullptr) == OutOfRange);
    Require(factory->CreateCompute(&sequence, 1, 1, 1, 1, nullptr) == NoneFree && sequence == nullptr);
    Sequence* first = nullptr;
    Sequence* second = nullptr;
    Require(factory->Create(&first, 8, 8, 2, 2, "first") == 0 && first != nullptr);
    Require(factory->Create(&second, 8, 8, 2, 2, "second") == 0 && second != nullptr && second != first);
    Require(factory->Create(&sequence, 8, 8, 2, 2, "third") == NoneFree);
    Require(factory->DestroyCompute(first) == Invalid);
    Require(factory->Destroy(reinterpret_cast<Sequence*>(block.memory)) == Invalid);
    Require(factory->Destroy(first) == 0 && factory->Destroy(first) == Invalid);
    Require(factory->Create(&sequence, 8, 8, 2, 2, "again") == 0 && sequence == first);
    Require(sequence->Dispatch(nullptr, nullptr, 1, nullptr) == NullArgument);
    Require(sequence->Dispatch(Nothing, nullptr, 0, nullptr) == OutOfRange);
    Require(sequence->Dispatch(Nothing, nullptr, 0x8000, nullptr) == OutOfRange);
    Require(factory->Destroy(sequence) == 0 && factory->Destroy(second) == 0);
    Require(Shutdown(manager.storage) == 0);
}

struct Tally {
    std::atomic<std::uint32_t> calls{0};
    std::vector<std::atomic<std::uint8_t>> seen;
    explicit Tally(std::size_t count) : seen(count) {}
};

static void APS5_VABI CountJob(void* argument, std::uint32_t index) {
    auto& tally = *static_cast<Tally*>(argument);
    ++tally.seen[index];
    ++tally.calls;
}

static void DispatchCase() {
    Block block(SizeFor(4, 1, 8, 2, 2));
    ManagerObject manager;
    Require(Initialize(manager.storage, block.memory, block.bytes, 4, 1, nullptr) == 0);
    SequenceFactory* factory = Factories(manager.storage, 0);
    Sequence* sequence = nullptr;
    Require(factory->Create(&sequence, 0x7fff, 8, 2, 2, "dispatch") == 0);
    Tally tally(1000);
    Tally single(1);
    Require(sequence->Dispatch(CountJob, &tally, 1000, "many") == 0);
    Require(sequence->Dispatch(CountJob, &single, 1, "one") == 0);
    Require(sequence->Wait() == 0);
    Require(tally.calls == 1000 && single.calls == 1);
    for (const auto& count : tally.seen) Require(count == 1);
    Require(sequence->Wait() == 0);
    Require(factory->Destroy(sequence) == 0);
    Require(Shutdown(manager.storage) == 0);
}

struct Stage {
    std::atomic<std::uint32_t>* finished;
    std::uint32_t expected;
    std::atomic<std::uint32_t>* early;
};

static void APS5_VABI StageJob(void* argument, std::uint32_t) {
    auto& stage = *static_cast<Stage*>(argument);
    if (stage.finished->load() < stage.expected) ++*stage.early;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    ++*stage.finished;
}

static void OrderingCase() {
    Block block(SizeFor(4, 1, 8, 4, 2));
    ManagerObject manager;
    Require(Initialize(manager.storage, block.memory, block.bytes, 4, 1, nullptr) == 0);
    SequenceFactory* factory = Factories(manager.storage, 1);
    Sequence* sequence = nullptr;
    Require(factory->Create(&sequence, 256, 8, 4, 2, "ordering") == 0);
    std::atomic<std::uint32_t> finished{0};
    std::atomic<std::uint32_t> early{0};
    Stage first{&finished, 0, &early};
    Stage second{&finished, 16, &early};
    Stage third{&finished, 32, &early};
    Require(sequence->Dispatch(StageJob, &first, 8, "a") == 0);
    Require(sequence->Dispatch(StageJob, &first, 8, "b") == 0);
    Require(sequence->SyncPoint() == 0);
    Require(sequence->Dispatch(StageJob, &second, 16, "c") == 0);
    Require(sequence->SyncPoint() == 0);
    Require(sequence->SyncPoint() == 0);
    Require(sequence->Dispatch(StageJob, &third, 4, "d") == 0);
    Require(sequence->Wait() == 0);
    Require(finished == 36 && early == 0);
    Require(factory->Destroy(sequence) == 0);
    Require(Shutdown(manager.storage) == 0);
}

struct Nested {
    Sequence* sequence;
    std::atomic<bool> barrierAppended{false};
    std::atomic<bool> followerAppended{false};
    std::atomic<bool> leaderFinished{false};
    std::atomic<int> followerResult{-1};
    std::atomic<int> followerSawLeaderFinished{-1};
};

static void APS5_VABI FollowerJob(void* argument, std::uint32_t) {
    auto& nested = *static_cast<Nested*>(argument);
    nested.followerSawLeaderFinished = nested.leaderFinished ? 1 : 0;
}

static void APS5_VABI LeaderJob(void* argument, std::uint32_t) {
    auto& nested = *static_cast<Nested*>(argument);
    while (!nested.barrierAppended) std::this_thread::yield();
    nested.followerResult = nested.sequence->Dispatch(FollowerJob, &nested, 1, "follower");
    nested.followerAppended = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    nested.leaderFinished = true;
}

static void RequireParked(int clock);

struct WorkerWait {
    int spareClock = 0;
    Sequence* child;
    std::atomic<bool> publisherEntered{false};
    std::atomic<bool> spareEntered{false};
    std::atomic<bool> publish{false};
    std::atomic<bool> finished{false};
    std::atomic<unsigned> childCalls{0};
};

static void APS5_VABI WorkerWaitChild(void* argument, std::uint32_t) {
    ++static_cast<WorkerWait*>(argument)->childCalls;
}

static void APS5_VABI WorkerWaitPublisher(void* argument, std::uint32_t index) {
    auto& state = *static_cast<WorkerWait*>(argument);
    if (index != 0) {
        Require(pthread_getcpuclockid_nid_postfix(scePthreadSelf(), &state.spareClock) == 0);
        state.spareEntered = true;
        return;
    }
    state.publisherEntered = true;
    while (!state.publish) std::this_thread::yield();
    Require(state.child->Dispatch(WorkerWaitChild, &state, 1, "child") == 0);
    Require(state.child->Wait() == 0);
    state.finished = true;
}

static void WorkerWaitCase() {
    Block block(SizeFor(2, 2, 4, 0, 4));
    ManagerObject manager;
    const ConfigParams config{0, 0x21fff, 0x10000, 0x2bc, "WorkerWait", 0, 0, 0, 0};
    Require(Initialize(manager.storage, block.memory, block.bytes, 2, 2, &config) == 0);
    Require(SetPersistentThreadCount(manager.storage, 0) == 0);
    SequenceFactory* factory = Factories(manager.storage, 0);
    Sequence* publisher = nullptr;
    WorkerWait state{};
    Require(factory->Create(&publisher, 2, 1, 0, 1, "publisher") == 0);
    Require(factory->Create(&state.child, 1, 1, 0, 1, "child") == 0);
    Require(publisher->Dispatch(WorkerWaitPublisher, &state, 2, "park spare") == 0);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while ((!state.publisherEntered || !state.spareEntered) && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    Require(state.publisherEntered && state.spareEntered);
    RequireParked(state.spareClock);
    state.publish = true;
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!state.finished && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    Require(state.finished && state.childCalls == 1);
    Require(publisher->Wait() == 0);
    Require(factory->Destroy(publisher) == 0 && factory->Destroy(state.child) == 0);
    Require(Shutdown(manager.storage) == 0);
}

static void NestedCase() {
    Block block(SizeFor(2, 1, 4, 2, 2));
    ManagerObject manager;
    Require(Initialize(manager.storage, block.memory, block.bytes, 2, 1, nullptr) == 0);
    SequenceFactory* factory = Factories(manager.storage, 0);
    Nested nested{};
    Require(factory->Create(&nested.sequence, 16, 4, 2, 2, "nested") == 0);
    Require(nested.sequence->Dispatch(LeaderJob, &nested, 1, "leader") == 0);
    Require(nested.sequence->SyncPoint() == 0);
    nested.barrierAppended = true;
    while (!nested.followerAppended) std::this_thread::yield();
    Require(nested.sequence->Wait() == 0);
    Require(nested.followerResult == 0 && nested.followerSawLeaderFinished == 1);
    nested.sequence->CompleteDestructor();
    Require(nested.sequence->Wait() == 0);
    Require(factory->Destroy(nested.sequence) == 0);
    Require(factory->Create(&nested.sequence, 1, 1, 0, 0, "no waits") == 0);
    Require(nested.sequence->Wait() == LimitExceeded);
    Require(nested.sequence->Dispatch(Nothing, nullptr, 1, nullptr) == Invalid);
    Require(factory->Destroy(nested.sequence) == 0);
    Require(Factories(manager.storage, 0x10000) == factory);
    Require(Shutdown(manager.storage) == 0);
}

static void LimitsCase() {
    Block block(SizeFor(2, 2, 2, 1, 1));
    ManagerObject manager;
    Require(Initialize(manager.storage, block.memory, block.bytes, 2, 2, nullptr) == 0);
    SequenceFactory* factory = Factories(manager.storage, 0);
    Sequence* sequence = nullptr;
    Require(factory->Create(&sequence, 4, 2, 1, 1, "limits") == 0);
    for (int round = 0; round < 3; ++round) {
        Require(sequence->Dispatch(Nothing, nullptr, 2, nullptr) == 0);
        Require(sequence->Dispatch(Nothing, nullptr, 2, nullptr) == 0);
        Require(sequence->SyncPoint() == 0);
        Require(sequence->Wait() == 0);
    }
    Require(sequence->Dispatch(Nothing, nullptr, 2, nullptr) == 0);
    Require(sequence->Dispatch(Nothing, nullptr, 3, nullptr) == LimitExceeded);
    Require(sequence->Dispatch(Nothing, nullptr, 1, nullptr) == Invalid);
    Require(sequence->SyncPoint() == Invalid);
    Require(sequence->Wait() == Invalid);
    Require(factory->Destroy(sequence) == 0);
    Require(factory->Create(&sequence, 4, 2, 1, 1, "dispatches") == 0);
    Require(sequence->Dispatch(Nothing, nullptr, 1, nullptr) == 0 && sequence->Dispatch(Nothing, nullptr, 1, nullptr) == 0);
    Require(sequence->Dispatch(Nothing, nullptr, 1, nullptr) == LimitExceeded);
    Require(factory->Destroy(sequence) == 0);
    Require(factory->Create(&sequence, 4, 2, 1, 1, "sync points") == 0);
    Require(sequence->SyncPoint() == 0 && sequence->SyncPoint() == LimitExceeded);
    Require(factory->Destroy(sequence) == 0);
    Require(factory->Create(&sequence, 4, 2, 1, 1, "waits") == 0);
    Require(sequence->Wait() == 0 && sequence->Wait() == 0);
    Require(factory->Destroy(sequence) == 0);
    Require(Shutdown(manager.storage) == 0);
}

static void NodePoolCase() {
    Block block(SizeFor(2, 2, 2, 1, 1));
    ManagerObject manager;
    Require(Initialize(manager.storage, block.memory, block.bytes, 2, 2, nullptr) == 0);
    SequenceFactory* factory = Factories(manager.storage, 0);
    Sequence* sequence = nullptr;
    Require(factory->Create(&sequence, 0x7fff, 0x3ff, 0xff, 1, "nodes") == 0);
    int result = 0;
    int accepted = 0;
    while (accepted < 0x3ff && (result = sequence->Dispatch(Nothing, nullptr, 1, nullptr)) == 0) ++accepted;
    Require(result == NoNodes && accepted == 33);
    Require(sequence->Dispatch(Nothing, nullptr, 1, nullptr) == Invalid);
    Require(sequence->Wait() == Invalid);
    Require(factory->Destroy(sequence) == 0);
    Require(factory->Create(&sequence, 0x7fff, 0x3ff, 0xff, 10, "recycle") == 0);
    for (int cycle = 0; cycle < 5; ++cycle) {
        for (int index = 0; index < 33; ++index) {
            Require(sequence->Dispatch(Nothing, nullptr, 1, nullptr) == 0);
        }
        Require(sequence->Wait() == 0);
    }
    int finalAccepted = 0;
    int finalResult = 0;
    while (finalAccepted < 0x3ff && (finalResult = sequence->Dispatch(Nothing, nullptr, 1, nullptr)) == 0) ++finalAccepted;
    Require(finalResult == NoNodes && finalAccepted == 33);
    Require(sequence->Wait() == Invalid);
    Require(factory->Destroy(sequence) == 0);
    for (int round = 0; round < 5; ++round) {
        Require(factory->Create(&sequence, 0x7fff, 0x3ff, 0xff, 2, "recycle_round") == 0);
        for (int index = 0; index < 33; ++index) {
            Require(sequence->Dispatch(Nothing, nullptr, 1, nullptr) == 0);
        }
        Require(sequence->Wait() == 0);
        int acceptedInRound = 0;
        int resultInRound = 0;
        while (acceptedInRound < 0x3ff && (resultInRound = sequence->Dispatch(Nothing, nullptr, 1, nullptr)) == 0) ++acceptedInRound;
        Require(resultInRound == NoNodes && acceptedInRound == 33);
        Require(sequence->Wait() == Invalid);
        Require(factory->Destroy(sequence) == 0);
    }
    Require(Shutdown(manager.storage) == 0);
}

struct Order {
    std::atomic<bool> release{false};
    std::atomic<bool> started{false};
    std::atomic<std::uint32_t> next{0};
    std::uint32_t low = 0;
    std::uint32_t high = 0;
};

static void APS5_VABI GateJob(void* argument, std::uint32_t) {
    auto& order = *static_cast<Order*>(argument);
    order.started = true;
    while (!order.release) std::this_thread::yield();
}

static void APS5_VABI LowJob(void* argument, std::uint32_t) {
    auto& order = *static_cast<Order*>(argument);
    order.low = ++order.next;
}

static void APS5_VABI HighJob(void* argument, std::uint32_t) {
    auto& order = *static_cast<Order*>(argument);
    order.high = ++order.next;
}

static void PriorityCase() {
    Block block(SizeFor(1, 3, 8, 2, 4));
    ManagerObject manager;
    Require(Initialize(manager.storage, block.memory, block.bytes, 1, 3, nullptr) == 0);
    Sequence* gate = nullptr;
    Sequence* low = nullptr;
    Sequence* high = nullptr;
    Require(Factories(manager.storage, 0)->Create(&gate, 4, 4, 1, 1, "gate") == 0);
    Require(Factories(manager.storage, 0)->Create(&low, 4, 4, 1, 1, "low") == 0);
    Require(Factories(manager.storage, 1)->Create(&high, 4, 4, 1, 1, "high") == 0);
    Order order;
    Require(gate->Dispatch(GateJob, &order, 1, nullptr) == 0);
    while (!order.started) std::this_thread::yield();
    Require(low->Dispatch(LowJob, &order, 1, nullptr) == 0);
    Require(high->Dispatch(HighJob, &order, 1, nullptr) == 0);
    order.release = true;
    Require(gate->Wait() == 0 && low->Wait() == 0 && high->Wait() == 0);
    Require(order.high == 1 && order.low == 2);
    Require(Factories(manager.storage, 0)->Destroy(gate) == 0 && Factories(manager.storage, 0)->Destroy(low) == 0);
    Require(Factories(manager.storage, 1)->Destroy(high) == 0);
    Require(Shutdown(manager.storage) == 0);
}

static void ShutdownCase() {
    Block block(SizeFor(3, 2, 8, 2, 2));
    ManagerObject manager;
    for (int round = 0; round < 3; ++round) {
        Require(Initialize(manager.storage, block.memory, block.bytes, 3, 2, nullptr) == 0);
        SequenceFactory* factory = UnifiedFactories(manager.storage, 0);
        Sequence* sequence = nullptr;
        Tally tally(64);
        Require(factory->Create(&sequence, 64, 8, 2, 2, "shutdown") == 0);
        Require(sequence->Dispatch(CountJob, &tally, 64, nullptr) == 0 && sequence->Wait() == 0 && tally.calls == 64);
        Require(Shutdown(manager.storage) == 0);
        Require(Shutdown(manager.storage) == NotInitialized);
        Require(Factories(manager.storage, 0) == nullptr);
    }
    {
        Block scopedBlock(SizeFor(2, 2, 8, 2, 2));
        ManagerObject scoped;
        Require(Initialize(scoped.storage, scopedBlock.memory, scopedBlock.bytes, 2, 2, nullptr) == 0);
    }
}

static void APS5_VABI CaptureWorkerClock(void* argument, std::uint32_t) {
    Require(pthread_getcpuclockid_nid_postfix(scePthreadSelf(), static_cast<int*>(argument)) == 0);
}

static std::int64_t CpuNanos(int clock) {
    KernelTimespec time{};
    Require(sceKernelClockGettime(clock, &time) == 0);
    return time.tv_sec * 1000000000LL + time.tv_nsec;
}

static void RequireParked(int clock) {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const auto start = CpuNanos(clock);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    Require(CpuNanos(clock) - start < 20000000);
}

static void WorkerPolicyCase() {
    for (const std::uint32_t mode : {0x10000u, 0x20000u}) {
        for (const std::uint32_t spin : {0u, 50u}) {
            Block block(SizeFor(1, 1, 8, 2, 2));
            ManagerObject manager;
            const ConfigParams config{0, mode | 0x1fff, 0x10000, 0x2bc, "WorkerPolicy", spin, 0, 0, 0};
            Require(Initialize(manager.storage, block.memory, block.bytes, 1, 1, &config) == 0);
            auto* factory = Factories(manager.storage, 0);
            Sequence* sequence = nullptr;
            Require(factory->Create(&sequence, 8, 8, 2, 2, "worker policy") == 0);
            int clock = 0;
            Require(sequence->Dispatch(CaptureWorkerClock, &clock, 1, nullptr) == 0 && sequence->Wait() == 0);
            RequireParked(clock);
            for (int round = 0; round < 2; ++round) {
                Require(SetPersistentThreadCount(manager.storage, 1) == 0);
                RequireParked(clock);
                Require(sequence->Dispatch(Nothing, nullptr, 1, nullptr) == 0 && sequence->Wait() == 0);
                const auto start = CpuNanos(clock);
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                while (CpuNanos(clock) - start < 30000000 && std::chrono::steady_clock::now() < deadline) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                Require(CpuNanos(clock) - start >= 30000000);
                Require(SetPersistentThreadCount(manager.storage, 0) == 0);
                RequireParked(clock);
            }
            Require(SetPersistentThreadCount(manager.storage, 1) == 0);
            Require(sequence->Dispatch(Nothing, nullptr, 1, nullptr) == 0 && sequence->Wait() == 0);
            Require(Shutdown(manager.storage) == 0);
        }
    }
}

static void WorkerWakeCase() {
    Block block(SizeFor(4, 1, 8, 2, 2));
    ManagerObject manager;
    for (const std::uint32_t mode : {0x10000u, 0x20000u}) {
        const ConfigParams config{0, mode | 0x1fff, 0x10000, 0x2bc, "WorkerWake", 50, 0, 0, 0};
        Require(Initialize(manager.storage, block.memory, block.bytes, 4, 1, &config) == 0);
        Sequence* sequence = nullptr;
        auto* factory = Factories(manager.storage, 0);
        Require(factory->Create(&sequence, 1024, 8, 2, 2, "worker wake") == 0);
        for (std::uint32_t round = 0; round < 60; ++round) {
            Require(SetPersistentThreadCount(manager.storage, round % 5) == 0);
            Tally tally(257);
            Require(sequence->Dispatch(CountJob, &tally, 257, nullptr) == 0);
            Require(sequence->SyncPoint() == 0);
            Require(sequence->Dispatch(CountJob, &tally, 257, nullptr) == 0);
            Require(sequence->Wait() == 0 && tally.calls == 514);
            for (const auto& count : tally.seen) Require(count == 2);
        }
        Require(factory->Destroy(sequence) == 0);
        Require(Shutdown(manager.storage) == 0);
    }
}

struct HeldWorker {
    std::atomic<bool> started{false};
    std::atomic<bool> release{false};
};

struct SpinClock {
    int guest = 0;
    std::int64_t reference = 0;
#ifdef _WIN32
    HANDLE thread = nullptr;
    ~SpinClock() { if (thread != nullptr) CloseHandle(thread); }
#endif
};

static std::int64_t SpinCpuUnits(const SpinClock& clock);

static void APS5_VABI CaptureSpinClock(void* argument, std::uint32_t) {
    auto& clock = *static_cast<SpinClock*>(argument);
    CaptureWorkerClock(&clock.guest, 0);
#ifdef _WIN32
    clock.thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentThreadId());
    Require(clock.thread != nullptr);
#endif
    const auto before = SpinCpuUnits(clock);
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(5);
    while (std::chrono::steady_clock::now() < until) {}
    clock.reference = SpinCpuUnits(clock) - before;
    Require(clock.reference > 0);
}

static std::int64_t SpinCpuUnits(const SpinClock& clock) {
#ifdef _WIN32
    ULONG64 cycles = 0;
    Require(QueryThreadCycleTime(clock.thread, &cycles));
    return static_cast<std::int64_t>(cycles);
#else
    return CpuNanos(clock.guest);
#endif
}

static void APS5_VABI HoldWorker(void* argument, std::uint32_t) {
    auto& worker = *static_cast<HeldWorker*>(argument);
    worker.started = true;
    worker.started.notify_one();
    worker.release.wait(false);
}

struct SpinSample {
    std::int64_t consumed;
    std::int64_t reference;
};

static SpinSample MeasureWorkerSpin(std::uint32_t spin, std::uint32_t rounds, bool pending) {
    Block block(SizeFor(2, 2, 4, 2, 2));
    ManagerObject manager;
    const ConfigParams config{0, 0x10003, 0x10000, 0x2bc, "WorkerSpin", spin, 0, 0, 0};
    Require(Initialize(manager.storage, block.memory, block.bytes, 2, 2, &config) == 0);
    auto* factory = Factories(manager.storage, 0);
    Sequence* held = nullptr;
    Sequence* pulse = nullptr;
    Require(factory->Create(&held, 4, 4, 2, 2, "held worker") == 0);
    Require(factory->Create(&pulse, 4, 4, 2, 2, "idle worker") == 0);
    HeldWorker worker;
    Require(held->Dispatch(HoldWorker, &worker, 1, nullptr) == 0);
    worker.started.wait(false);
    if (pending) {
        Require(held->SyncPoint() == 0);
        Require(held->Dispatch(Nothing, nullptr, 1, nullptr) == 0);
    }
    SpinClock clock;
    Require(pulse->Dispatch(CaptureSpinClock, &clock, 1, nullptr) == 0 && pulse->Wait() == 0);
    RequireParked(clock.guest);
    const auto before = SpinCpuUnits(clock);
    for (std::uint32_t round = 0; round < rounds; ++round) {
        Require(pulse->Dispatch(Nothing, nullptr, 1, nullptr) == 0 && pulse->Wait() == 0);
        Require(sceKernelUsleep_nid_postfix(1000) == 0);
    }
    const auto consumed = SpinCpuUnits(clock) - before;
    RequireParked(clock.guest);
    Require(!worker.release);
    worker.release = true;
    worker.release.notify_one();
    Require(held->Wait() == 0);
    Require(factory->Destroy(held) == 0 && factory->Destroy(pulse) == 0);
    Require(Shutdown(manager.storage) == 0);
    return {consumed, clock.reference};
}

static void WorkerSpinCase() {
    constexpr std::uint32_t rounds = 512;
    const auto parked = MeasureWorkerSpin(0, rounds, true);
    const auto spinning = MeasureWorkerSpin(50, rounds, true);
    const auto noPending = MeasureWorkerSpin(50, rounds, false);
#ifdef _WIN32
    constexpr const char* units = "cycles";
#else
    constexpr const char* units = "ns";
#endif
    std::fprintf(stderr, "idle worker CPU over %u pulses (%s): spin=0 %lld, spin=50 %lld, no pending work %lld, 5ms active reference %lld\n", rounds, units,
        static_cast<long long>(parked.consumed), static_cast<long long>(spinning.consumed), static_cast<long long>(noPending.consumed), static_cast<long long>(spinning.reference));
    const auto interval = spinning.reference * rounds / 100;
    Require(spinning.consumed > parked.consumed + interval / 3);
    Require(spinning.consumed < parked.consumed + interval * 5);
    Require(noPending.consumed < parked.consumed + interval / 3);
}

struct WorkerClocks {
    std::array<int, 4> clocks{};
    std::atomic<unsigned int> arrived{0};
    std::atomic<bool> release{false};
};

static void APS5_VABI CaptureWorkerClocks(void* argument, std::uint32_t index) {
    auto& workers = *static_cast<WorkerClocks*>(argument);
    CaptureWorkerClock(&workers.clocks[index], 0);
    ++workers.arrived;
    while (!workers.release) std::this_thread::yield();
}

static void WorkerCountCase() {
    Block block(SizeFor(4, 1, 8, 2, 2));
    ManagerObject manager;
    const ConfigParams config{0, 0x1000f, 0x10000, 0x2bc, "WorkerCount", 0, 0, 0, 0};
    Require(Initialize(manager.storage, block.memory, block.bytes, 4, 1, &config) == 0);
    Sequence* sequence = nullptr;
    Require(Factories(manager.storage, 0)->Create(&sequence, 8, 8, 2, 2, "worker count") == 0);
    WorkerClocks workers;
    Require(sequence->Dispatch(CaptureWorkerClocks, &workers, 4, nullptr) == 0);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (workers.arrived != 4 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    Require(workers.arrived == 4);
    workers.release = true;
    Require(sequence->Wait() == 0);
    for (const std::uint32_t persistent : {2u, 4u}) {
        Require(SetPersistentThreadCount(manager.storage, 0) == 0);
        for (const int clock : workers.clocks) RequireParked(clock);
        Require(SetPersistentThreadCount(manager.storage, persistent) == 0);
        for (std::uint32_t job = 0; job < persistent; ++job) {
            Require(sequence->Dispatch(Nothing, nullptr, 1, nullptr) == 0 && sequence->Wait() == 0);
        }
        std::array<std::int64_t, 4> starts{};
        for (std::size_t index = 0; index < starts.size(); ++index) starts[index] = CpuNanos(workers.clocks[index]);
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        unsigned int active = 0;
        do {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            active = 0;
            for (std::size_t index = 0; index < starts.size(); ++index) {
                if (CpuNanos(workers.clocks[index]) - starts[index] >= 30000000) ++active;
            }
        } while (active < persistent && std::chrono::steady_clock::now() < until);
        Require(active == persistent);
    }
    Require(Shutdown(manager.storage) == 0);
}

int RunJobManagerCase(const char* caseName) {
    const std::string name = caseName;
    if (name == "memory") MemoryCase();
    else if (name == "validation") ValidationCase();
    else if (name == "dispatch") DispatchCase();
    else if (name == "ordering") OrderingCase();
    else if (name == "limits") LimitsCase();
    else if (name == "node_pool") NodePoolCase();
    else if (name == "nested") NestedCase();
    else if (name == "priority") PriorityCase();
    else if (name == "shutdown") ShutdownCase();
    else if (name == "worker_policy") WorkerPolicyCase();
    else if (name == "worker_wait") WorkerWaitCase();
    else if (name == "worker_wake") WorkerWakeCase();
    else if (name == "worker_spin") WorkerSpinCase();
    else if (name == "worker_count") WorkerCountCase();
    else return 2;
    return 0;
}

#ifndef ANYPS5_JOB_MANAGER_REFERENCE
int main(int argc, char** argv) {
    return RunJobManagerCase(argc > 1 ? argv[1] : "");
}
#endif
