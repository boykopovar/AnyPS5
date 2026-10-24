#include "SceTypes.hpp"
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

static void APS5_VABI ShutdownGateJob(void* argument, std::uint32_t index) {
    GateJob(argument, index);
    ++static_cast<Order*>(argument)->next;
}

static void ShutdownActiveCase(bool wait, bool queue, bool destroy, std::uint32_t count = 1) {
    for (int round = 0; round < 3; ++round) {
        Block block(SizeFor(1, 1, 4, 2, 2));
        ManagerObject manager;
        Require(Initialize(manager.storage, block.memory, block.bytes, 1, 1, nullptr) == 0);
        SequenceFactory* factory = Factories(manager.storage, 0);
        Sequence* sequence = nullptr;
        Require(factory->Create(&sequence, 4, 4, 2, 2, "shutdown active") == 0);
        Order order;
        Require(sequence->Dispatch(ShutdownGateJob, &order, count, nullptr) == 0);
        while (!order.started) std::this_thread::yield();
        if (queue) {
            Require(sequence->SyncPoint() == 0);
            Require(sequence->Dispatch(LowJob, &order, 1, nullptr) == 0);
        }
        std::atomic<bool> entered{false};
        std::atomic<bool> returned{false};
        std::thread waiter;
        if (wait) {
            waiter = std::thread([&] {
                entered = true;
                Require(destroy ? factory->Destroy(sequence) == 0 : sequence->Wait() == 0);
                returned = true;
            });
            while (!entered) std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            Require(!returned);
        }
        std::atomic<bool> stopping{false};
        std::atomic<bool> stopped{false};
        std::thread shutdown([&] {
            stopping = true;
            Require(Shutdown(manager.storage) == 0);
            stopped = true;
        });
        while (!stopping) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        Require(!stopped);
        order.release = true;
        shutdown.join();
        if (waiter.joinable()) waiter.join();
        Require(stopped && (!wait || returned));
        Require(order.next == 1);
        Require(Factories(manager.storage, 0) == nullptr);
        Require(Initialize(manager.storage, block.memory, block.bytes, 1, 1, nullptr) == 0);
        Require(Shutdown(manager.storage) == 0);
    }
}

struct ShutdownCrossWait {
    Sequence* sequence;
    std::atomic<bool> entered{false};
    std::atomic<bool> returned{false};
};

static void APS5_VABI ShutdownWaitOther(void* argument, std::uint32_t) {
    auto& state = *static_cast<ShutdownCrossWait*>(argument);
    state.entered = true;
    Require(state.sequence->Wait() == 0);
    state.returned = true;
}

static void ShutdownCrossWaitCase() {
    Block block(SizeFor(2, 2, 4, 2, 2));
    ManagerObject manager;
    Require(Initialize(manager.storage, block.memory, block.bytes, 2, 2, nullptr) == 0);
    auto* factory = Factories(manager.storage, 0);
    Sequence* producer = nullptr;
    Sequence* consumer = nullptr;
    Require(factory->Create(&producer, 1, 1, 1, 1, nullptr) == 0);
    Require(factory->Create(&consumer, 1, 1, 1, 1, nullptr) == 0);
    Order order;
    Require(producer->Dispatch(ShutdownGateJob, &order, 1, nullptr) == 0);
    while (!order.started) std::this_thread::yield();
    ShutdownCrossWait state{producer};
    Require(consumer->Dispatch(ShutdownWaitOther, &state, 1, nullptr) == 0);
    while (!state.entered) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(!state.returned);
    std::atomic<bool> stopping{false};
    std::atomic<bool> stopped{false};
    std::thread shutdown([&] {
        stopping = true;
        Require(Shutdown(manager.storage) == 0);
        stopped = true;
    });
    while (!stopping) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(!stopped);
    order.release = true;
    shutdown.join();
    Require(state.returned && stopped && order.next == 1);
}

int RunJobManagerCase(const char* caseName) {
    const std::string name = caseName;
    if (name == "memory") MemoryCase();
    else if (name == "validation") ValidationCase();
    else if (name == "dispatch") DispatchCase();
    else if (name == "ordering") OrderingCase();
    else if (name == "limits") LimitsCase();
    else if (name == "nested") NestedCase();
    else if (name == "priority") PriorityCase();
    else if (name == "shutdown") ShutdownCase();
    else if (name == "shutdown_running") ShutdownActiveCase(false, false, false);
    else if (name == "shutdown_waiter") ShutdownActiveCase(true, false, false);
    else if (name == "shutdown_queued") ShutdownActiveCase(true, true, false);
    else if (name == "shutdown_destroy") ShutdownActiveCase(true, true, true);
    else if (name == "shutdown_partial") ShutdownActiveCase(true, false, false, 4);
    else if (name == "shutdown_cross_wait") ShutdownCrossWaitCase();
    else return 2;
    return 0;
}

#ifndef ANYPS5_JOB_MANAGER_REFERENCE
int main(int argc, char** argv) {
    return RunJobManagerCase(argc > 1 ? argv[1] : "");
}
#endif
