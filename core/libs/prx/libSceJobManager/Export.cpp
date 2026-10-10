#include <array>
#include <bit>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inheritSched);
int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param);
int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stackSize);
int APS5_VABI scePthreadAttrSetaffinity(PthreadAttr* attr, KernelCpumask mask);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** result);
}

namespace {

constexpr int ManagerInvalidArgument = static_cast<int>(0x81581000u);
constexpr int ManagerNullMemory = static_cast<int>(0x81581001u);
constexpr int ManagerMemoryTooSmall = static_cast<int>(0x81581002u);
constexpr int ManagerNotInitialized = static_cast<int>(0x81581010u);
constexpr int ManagerAlreadyInitialized = static_cast<int>(0x81581020u);
constexpr int ManagerMisalignedMemory = static_cast<int>(0x81581030u);
constexpr int SequenceOutOfRange = static_cast<int>(0x81580000u);
constexpr int SequenceNullArgument = static_cast<int>(0x81580001u);
constexpr int SequenceNoNodes = static_cast<int>(0x81580002u);
constexpr int SequenceLimitExceeded = static_cast<int>(0x81580003u);
constexpr int SequenceNoneFree = static_cast<int>(0x81580006u);
constexpr int SequenceInvalid = static_cast<int>(0x81580007u);

constexpr std::uint32_t MaxWorkers = 13;
constexpr std::uint32_t MaxSequences = 32;
constexpr std::uint32_t MaxInvocations = 0x7fff;
constexpr std::uint32_t MaxDispatches = 0x3ff;
constexpr std::uint32_t MaxSyncPoints = 0xff;
constexpr std::uint32_t MaxWaits = 0x3f;
constexpr std::uint64_t StateOffset = 0x10;
constexpr std::uint64_t StateEnd = 0x2d8;
constexpr std::uint64_t ThreadRecordBytes = 0x48;
constexpr std::uint64_t SequenceBytes = 0x240;
constexpr std::uint64_t NodeBytes = 72;

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

constexpr ConfigParams DefaultConfig{0, 0x00021fff, 0x00010000, 0x2bc, nullptr, 0, 0x1fff, 0x100, 0};

struct BlockHeader {
    std::uint16_t magic;
    std::uint16_t stateOffset;
    std::uint32_t version;
    std::uint32_t size;
    std::uint32_t reserved;
};

using JobEntry = void (APS5_VABI*)(void* argument, std::uint32_t index);

std::uint64_t AlignUp(std::uint64_t value, std::uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

struct Manager;
class Sequence;

enum class ItemType { Dispatch, SyncPoint, Wait };

struct Item {
    ItemType type;
    Sequence* sequence;
    JobEntry entry;
    void* argument;
    std::uint32_t count;
    std::uint32_t handedOut;
    std::uint32_t completed;
    bool* passed;
};

struct Counters {
    std::uint32_t invocations = 0;
    std::uint32_t dispatches = 0;
    std::uint32_t syncPoints = 0;
    std::uint32_t waits = 0;
};

class ISequence {
public:
    APS5_VABI virtual int Dispatch(JobEntry entry, void* argument, std::uint32_t count, const char* name) = 0;
    APS5_VABI virtual int SyncPoint() = 0;
    APS5_VABI virtual int Wait() = 0;
    APS5_VABI virtual void CompleteDestructor() {}
    APS5_VABI virtual void DeletingDestructor() { throw std::runtime_error("libSceJobManager: a sequence was deleted by its caller"); }
    APS5_VABI virtual int DispatchCompute(const void* shader, void* userData, std::int32_t x, std::int32_t y, std::int32_t z, const char* name) = 0;
    APS5_VABI virtual int DispatchComputeIndirect(const void* shader, void* userData, const void* indirectArguments, const char* name) = 0;
};

class ISequenceFactory {
public:
    APS5_VABI virtual int Create(ISequence** out, std::uint32_t invocations, std::uint32_t dispatches, std::uint32_t syncPoints, std::uint32_t waits, const char* name) = 0;
    APS5_VABI virtual int Destroy(ISequence* sequence) = 0;
    APS5_VABI virtual void CompleteDestructor() {}
    APS5_VABI virtual void DeletingDestructor() { throw std::runtime_error("libSceJobManager: a sequence factory was deleted by its caller"); }
    APS5_VABI virtual int CreateCompute(ISequence** out, std::uint32_t invocations, std::uint32_t dispatches, std::uint32_t syncPoints, std::uint32_t waits, const char* name) = 0;
    APS5_VABI virtual int DestroyCompute(ISequence* sequence) = 0;
};

class Sequence final : public ISequence {
public:
    APS5_VABI int Dispatch(JobEntry entry, void* argument, std::uint32_t count, const char* name) override;
    APS5_VABI int SyncPoint() override;
    APS5_VABI int Wait() override;
    APS5_VABI int DispatchCompute(const void*, void*, std::int32_t, std::int32_t, std::int32_t, const char*) override {
        NotImplemented_nid_no_patch("sce::Job compute dispatch");
        return 0;
    }
    APS5_VABI int DispatchComputeIndirect(const void*, void*, const void*, const char*) override {
        NotImplemented_nid_no_patch("sce::Job indirect compute dispatch");
        return 0;
    }

    bool Open(Manager* owner, const Counters& limit, std::uint32_t runQueue);
    int WaitAndClose(std::uint32_t index);
    void Advance();
    void DispatchCompleted(std::uint32_t invocations);

    Manager* manager = nullptr;
    bool open = false;

private:
    int Latch(int code);
    void RequireOpen() const;
    int WaitLocked(std::unique_lock<std::mutex>& guard);

    Counters limits;
    Counters submitted;
    Counters completed;
    std::uint32_t priority = 0;
    int error = 0;
    std::uint32_t outstanding = 0;
    std::size_t cursor = 0;
    std::deque<Item> items;
    std::mutex waiters;
    std::condition_variable passed;
};

class Factory final : public ISequenceFactory {
public:
    APS5_VABI int Create(ISequence** out, std::uint32_t invocations, std::uint32_t dispatches, std::uint32_t syncPoints, std::uint32_t waits, const char* name) override;
    APS5_VABI int Destroy(ISequence* sequence) override;
    APS5_VABI int CreateCompute(ISequence** out, std::uint32_t invocations, std::uint32_t dispatches, std::uint32_t syncPoints, std::uint32_t waits, const char* name) override;
    APS5_VABI int DestroyCompute(ISequence* sequence) override;

    Manager* manager = nullptr;
    std::uint32_t priority = 0;
};

struct Manager {
    std::mutex lock;
    std::condition_variable work;
    std::condition_variable nodeFreed;
    bool running = false;
    std::uint32_t blockedWaiters = 0;
    std::uint32_t freeNodes = 0;
    std::uint32_t freeSequences = 0;
    std::uint32_t persistentThreads = 0;
    std::array<std::deque<Item*>, 2> runQueues;
    std::vector<Sequence> sequences;
    std::array<Factory, 2> factories;
    std::array<Factory, 2> unifiedFactories;
    std::vector<Pthread> workers;
};

Manager*& Implementation(void* self) {
    return *static_cast<Manager**>(self);
}

bool TakeNode(Manager& manager) {
    if (manager.freeNodes == 0) return false;
    --manager.freeNodes;
    return true;
}

void ReturnNodes(Manager& manager, std::uint32_t count) {
    manager.freeNodes += count;
    manager.nodeFreed.notify_all();
}

int Sequence::Latch(int code) {
    if (error == 0) error = code;
    return code;
}

bool Sequence::Open(Manager* owner, const Counters& limit, std::uint32_t runQueue) {
    manager = owner;
    limits = limit;
    submitted = {};
    completed = {};
    priority = runQueue;
    error = 0;
    outstanding = 0;
    cursor = 0;
    items.clear();
    if (!TakeNode(*owner)) return false;
    open = true;
    return true;
}

void Sequence::RequireOpen() const {
    if (!open) throw std::runtime_error("libSceJobManager: a destroyed sequence was used");
}

void Sequence::Advance() {
    while (cursor < items.size()) {
        Item& item = items[cursor];
        if (item.type == ItemType::Dispatch) {
            ++outstanding;
            ++cursor;
            manager->runQueues[priority].push_back(&item);
            manager->work.notify_all();
            continue;
        }
        if (outstanding != 0) return;
        if (item.type == ItemType::SyncPoint) {
            ++completed.syncPoints;
            ++cursor;
            continue;
        }
        bool* const waiter = item.passed;
        const auto retired = static_cast<std::uint32_t>(cursor) + 1;
        items.erase(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(retired));
        cursor = 0;
        if (error == 0) {
            submitted.invocations -= completed.invocations;
            submitted.dispatches -= completed.dispatches;
            submitted.syncPoints -= completed.syncPoints;
            --submitted.waits;
            completed = {};
        }
        ReturnNodes(*manager, retired);
        *waiter = true;
        passed.notify_all();
    }
}

void Sequence::DispatchCompleted(std::uint32_t invocations) {
    completed.invocations += invocations;
    ++completed.dispatches;
    --outstanding;
    if (outstanding == 0) Advance();
}

int Sequence::Dispatch(JobEntry entry, void* argument, std::uint32_t count, const char*) {
    if (entry == nullptr) return SequenceNullArgument;
    if (count == 0 || count > MaxInvocations) return SequenceOutOfRange;
    const std::lock_guard guard(manager->lock);
    RequireOpen();
    if (error != 0) return SequenceInvalid;
    const Counters before = submitted;
    submitted.invocations += count;
    ++submitted.dispatches;
    if (before.invocations + count > limits.invocations || before.dispatches >= limits.dispatches) return Latch(SequenceLimitExceeded);
    if (!TakeNode(*manager)) return Latch(SequenceNoNodes);
    items.push_back({ItemType::Dispatch, this, entry, argument, count, 0, 0, nullptr});
    Advance();
    return 0;
}

int Sequence::SyncPoint() {
    const std::lock_guard guard(manager->lock);
    RequireOpen();
    if (error != 0) return SequenceInvalid;
    const std::uint32_t before = submitted.syncPoints++;
    if (before >= limits.syncPoints) return Latch(SequenceLimitExceeded);
    if (!TakeNode(*manager)) return Latch(SequenceNoNodes);
    items.push_back({ItemType::SyncPoint, this, nullptr, nullptr, 0, 0, 0, nullptr});
    Advance();
    return 0;
}

int Sequence::Wait() {
    std::unique_lock guard(manager->lock);
    return WaitLocked(guard);
}

int Sequence::WaitAndClose(std::uint32_t index) {
    std::unique_lock guard(manager->lock);
    const int result = WaitLocked(guard);
    ReturnNodes(*manager, static_cast<std::uint32_t>(items.size()) + 1);
    items.clear();
    cursor = 0;
    outstanding = 0;
    open = false;
    manager->freeSequences |= std::uint32_t{1} << index;
    return result;
}

int Sequence::WaitLocked(std::unique_lock<std::mutex>& guard) {
    RequireOpen();
    int result = 0;
    if (error != 0) {
        result = SequenceInvalid;
    } else if (submitted.waits++ >= limits.waits) {
        result = Latch(SequenceLimitExceeded);
    }
    if (manager->freeNodes == 0 && result == 0) result = Latch(SequenceNoNodes);
    ++manager->blockedWaiters;
    manager->nodeFreed.wait(guard, [this] { return manager->freeNodes != 0; });
    --manager->freeNodes;
    guard.unlock();
    const std::lock_guard serial(waiters);
    guard.lock();
    bool done = false;
    items.push_back({ItemType::Wait, this, nullptr, nullptr, 0, 0, 0, &done});
    Advance();
    passed.wait(guard, [&done] { return done; });
    --manager->blockedWaiters;
    return result;
}

bool ValidLimits(std::uint32_t invocations, std::uint32_t dispatches, std::uint32_t syncPoints, std::uint32_t waits) {
    if (invocations > MaxInvocations || dispatches > MaxDispatches || syncPoints > MaxSyncPoints || waits > MaxWaits) return false;
    return invocations != 0 || dispatches != 0 || syncPoints != 0 || waits != 0;
}

int Factory::Create(ISequence** out, std::uint32_t invocations, std::uint32_t dispatches, std::uint32_t syncPoints, std::uint32_t waits, const char*) {
    if (out == nullptr) return SequenceNullArgument;
    if (!ValidLimits(invocations, dispatches, syncPoints, waits)) return SequenceOutOfRange;
    const std::lock_guard guard(manager->lock);
    if (manager->freeSequences == 0) return SequenceNoneFree;
    const int index = std::countr_zero(manager->freeSequences);
    manager->freeSequences &= ~(std::uint32_t{1} << index);
    Sequence& sequence = manager->sequences[static_cast<std::size_t>(index)];
    if (!sequence.Open(manager, {invocations, dispatches, syncPoints, waits}, priority)) return SequenceNoNodes;
    *out = &sequence;
    return 0;
}

int Factory::Destroy(ISequence* sequence) {
    Sequence* found = nullptr;
    std::uint32_t index = 0;
    {
        const std::lock_guard guard(manager->lock);
        for (; index < manager->sequences.size(); ++index) {
            if (&manager->sequences[index] == sequence) break;
        }
        if (index == manager->sequences.size() || (manager->freeSequences & (std::uint32_t{1} << index)) != 0) return SequenceInvalid;
        found = &manager->sequences[index];
    }
    static_cast<void>(found->WaitAndClose(index));
    return 0;
}

int Factory::CreateCompute(ISequence** out, std::uint32_t invocations, std::uint32_t dispatches, std::uint32_t syncPoints, std::uint32_t waits, const char*) {
    if (out == nullptr) return SequenceNullArgument;
    if (!ValidLimits(invocations, dispatches, syncPoints, waits)) return SequenceOutOfRange;
    return SequenceNoneFree;
}

int Factory::DestroyCompute(ISequence*) {
    return SequenceInvalid;
}

bool RunOne(Manager& manager, std::unique_lock<std::mutex>& guard) {
    Item* item = nullptr;
    for (const std::size_t queue : {std::size_t{1}, std::size_t{0}}) {
        auto& pending = manager.runQueues[queue];
        if (pending.empty()) continue;
        item = pending.front();
        if (item->handedOut + 1 == item->count) pending.pop_front();
        break;
    }
    if (item == nullptr) return false;
    const std::uint32_t index = item->handedOut++;
    const JobEntry entry = item->entry;
    void* const argument = item->argument;
    guard.unlock();
    entry(argument, index);
    guard.lock();
    if (++item->completed == item->count) item->sequence->DispatchCompleted(item->count);
    return true;
}

void* APS5_VABI WorkerEntry(void* argument) {
    Manager& manager = *static_cast<Manager*>(argument);
    std::unique_lock guard(manager.lock);
    while (manager.running) {
        if (RunOne(manager, guard)) continue;
        manager.work.wait(guard, [&manager] { return !manager.running || !manager.runQueues[0].empty() || !manager.runQueues[1].empty(); });
    }
    return nullptr;
}

std::uint32_t NodeCount(std::uint64_t memory, std::uint64_t size, std::uint32_t workers, std::uint32_t sequences, bool* fits) {
    const std::uint64_t end = memory + size;
    std::uint64_t address = AlignUp(memory + StateEnd, 8) + workers * ThreadRecordBytes;
    for (const std::uint64_t bytes : {std::uint64_t{0x100}, std::uint64_t{0x80}, std::uint64_t{0x80}, std::uint64_t{0x80}}) address = AlignUp(address, 64) + bytes;
    address = AlignUp(AlignUp(address, 64) + sequences * SequenceBytes, 64);
    *fits = address <= end;
    return *fits ? static_cast<std::uint32_t>(((end - address) / NodeBytes) & 0xffff) : 0;
}

int ValidateWorkers(const ConfigParams& config, std::uint32_t workers) {
    const std::uint32_t mode = config.threadFlags & 0x30000;
    if (config.reserved != 0 || config.priority < 0x100 || config.priority > 0x2ff || config.stackSize < 0x4000) return ManagerInvalidArgument;
    if ((config.threadFlags & 0xfffce000) != 0 || static_cast<std::uint32_t>(std::popcount(config.threadFlags & 0x1fff)) < workers) return ManagerInvalidArgument;
    if ((mode != 0x10000 && mode != 0x20000) || config.spinMicroseconds > 0x32) return ManagerInvalidArgument;
    return 0;
}

void StartWorkers(Manager& manager, const ConfigParams& config, std::uint32_t workers) {
    std::uint32_t remaining = config.threadFlags & 0x1fff;
    for (std::uint32_t index = 0; index < workers; ++index) {
        KernelCpumask affinity = config.threadFlags & 0x1fff;
        if ((config.threadFlags & 0x30000) == 0x10000) {
            affinity = remaining & (~remaining + 1);
            remaining &= remaining - 1;
        }
        const std::string name = config.threadName != nullptr ? std::string(config.threadName).substr(0, 31) : "SceJobManagerWorker" + std::to_string(index);
        const KernelSchedParam schedule{static_cast<int>(config.priority)};
        PthreadAttr attributes = nullptr;
        if (scePthreadAttrInit(&attributes) != 0) throw std::runtime_error("libSceJobManager: scePthreadAttrInit failed");
        if (scePthreadAttrSetinheritsched(&attributes, 0) != 0 || scePthreadAttrSetschedparam(&attributes, &schedule) != 0 ||
            scePthreadAttrSetstacksize(&attributes, config.stackSize) != 0 || scePthreadAttrSetaffinity(&attributes, affinity) != 0) {
            throw std::runtime_error("libSceJobManager: a worker thread attribute was rejected");
        }
        Pthread thread = nullptr;
        const int created = scePthreadCreate(&thread, &attributes, WorkerEntry, &manager, name.c_str());
        scePthreadAttrDestroy(&attributes);
        if (created != 0) throw std::runtime_error("libSceJobManager: scePthreadCreate failed for a worker");
        manager.workers.push_back(thread);
    }
}

int Initialize(void* self, void* memory, std::uint64_t size, std::uint32_t workers, std::uint32_t sequences, std::uint32_t computeSequences, std::uint64_t computeQueueMask, const ConfigParams* config, bool unified) {
    if (Implementation(self) != nullptr) return ManagerAlreadyInitialized;
    if (memory == nullptr) return ManagerNullMemory;
    const auto base = reinterpret_cast<std::uint64_t>(memory);
    if ((base & 0xff) != 0) return ManagerMisalignedMemory;
    if (size == 0 || workers == 0 || workers > MaxWorkers) return ManagerInvalidArgument;
    if (unified) {
        if ((sequences == 0 && computeSequences == 0) || sequences + computeSequences > MaxSequences || (computeQueueMask >> 56) != 0 || static_cast<std::uint32_t>(std::popcount(computeQueueMask)) < computeSequences) return ManagerInvalidArgument;
    } else if (sequences == 0 || sequences > MaxSequences) {
        return ManagerInvalidArgument;
    }
    if (size < StateOffset) return ManagerMemoryTooSmall;
    if (size < StateEnd) throw std::runtime_error("sce::Job::JobManager::initialize: a memory block smaller than 0x2d8 bytes is not supported");
    std::memset(static_cast<std::uint8_t*>(memory) + StateOffset, 0, StateEnd - StateOffset);
    const BlockHeader header{0x4d4a, static_cast<std::uint16_t>(StateOffset), 0x05000001, static_cast<std::uint32_t>(size), 0};
    std::memcpy(memory, &header, sizeof(header));
    if (config != nullptr && config->version > 1) return ManagerInvalidArgument;
    if (computeSequences != 0) NotImplemented_nid_no_patch("sce::Job::JobManager::initialize with compute sequences");
    const ConfigParams& settings = config != nullptr ? *config : DefaultConfig;
    bool fits = false;
    const std::uint32_t nodes = NodeCount(base, size, workers, sequences, &fits);
    if (!fits || nodes == 0) return ManagerMemoryTooSmall;
    if (const int invalid = ValidateWorkers(settings, workers); invalid != 0) return invalid;
    auto* manager = new Manager;
    manager->running = true;
    manager->freeNodes = nodes - 1;
    manager->freeSequences = sequences == MaxSequences ? ~std::uint32_t{0} : (std::uint32_t{1} << sequences) - 1;
    manager->sequences = std::vector<Sequence>(sequences);
    for (std::uint32_t priority = 0; priority < 2; ++priority) {
        manager->factories[priority].manager = manager;
        manager->factories[priority].priority = priority;
        manager->unifiedFactories[priority].manager = manager;
        manager->unifiedFactories[priority].priority = priority;
    }
    Implementation(self) = manager;
    StartWorkers(*manager, settings, workers);
    return 0;
}

ISequenceFactory* FactoryFor(void* self, std::uint32_t priority, bool unified) {
    Manager* manager = Implementation(self);
    if (manager == nullptr) return nullptr;
    priority &= 0xffff;
    if (priority > 1) throw std::runtime_error("sce::Job::JobManager: sequence factory priority " + std::to_string(priority) + " does not exist");
    return unified ? &manager->unifiedFactories[priority] : &manager->factories[priority];
}

}

extern "C" {

void APS5_VABI _ZN3sce3Job10JobManagerC1Ev(void* self) {
    Implementation(self) = nullptr;
}

void APS5_VABI _ZN3sce3Job10JobManagerC2Ev(void* self) {
    Implementation(self) = nullptr;
}

void APS5_VABI _ZN3sce3Job10JobManagerD1Ev(void*) {
}

void APS5_VABI _ZN3sce3Job10JobManagerD2Ev(void*) {
}

std::uint64_t APS5_VABI _ZN3sce3Job10JobManager27calculateRequiredMemorySizeEPKNS1_21MemorySizeQueryParamsE(const MemorySizeQueryParams* params) {
    if (params == nullptr || params->version > 1 || params->workers == 0 || params->workers > MaxWorkers || params->sequences > MaxSequences) return 0;
    const bool unified = params->version == 1;
    const std::uint32_t computeSequences = unified ? params->computeSequences : 0;
    if (unified) {
        if (computeSequences > MaxSequences || params->sequences + computeSequences == 0 || params->sequences + computeSequences > MaxSequences) return 0;
        if ((params->computeQueueMask >> 56) != 0 || static_cast<std::uint32_t>(std::popcount(params->computeQueueMask)) < computeSequences) return 0;
    } else if (params->sequences == 0) {
        return 0;
    }
    const std::uint64_t total = params->sequences + computeSequences;
    if (params->invocations > total * MaxInvocations || params->dispatches > total * MaxDispatches || params->syncPoints > total * MaxSyncPoints || params->waits > total * MaxWaits) return 0;
    if (computeSequences != 0) NotImplemented_nid_no_patch("sce::Job::JobManager::calculateRequiredMemorySize with compute sequences");
    std::uint64_t bytes = (NodeBytes * params->workers + 0x317) & 0xfc0;
    bytes = (bytes + 0x13f) & ~std::uint64_t{0x3f};
    bytes = (bytes + 0x0bf) & ~std::uint64_t{0x3f};
    bytes = (bytes + 0x13f) & ~std::uint64_t{0x3f};
    bytes += params->sequences * SequenceBytes;
    const std::uint32_t nodes = params->dispatches + params->syncPoints + params->waits + static_cast<std::uint32_t>(total) + 1;
    return AlignUp(bytes, 64) + std::uint64_t{nodes} * NodeBytes + 0x800;
}

int APS5_VABI _ZN3sce3Job10JobManager10initializeEPvmjjjmPKNS1_12ConfigParamsE(void* self, void* memory, std::uint64_t memorySize, std::uint32_t workers, std::uint32_t sequences, std::uint32_t computeSequences, std::uint64_t computeQueueMask, const ConfigParams* config) {
    return Initialize(self, memory, memorySize, workers, sequences, computeSequences, computeQueueMask, config, true);
}

int APS5_VABI _ZN3sce3Job10JobManager10initializeEPvmjjPKNS1_12ConfigParamsE(void* self, void* memory, std::uint64_t memorySize, std::uint32_t workers, std::uint32_t sequences, const ConfigParams* config) {
    return Initialize(self, memory, memorySize, workers, sequences, 0, 0, config, false);
}

int APS5_VABI _ZN3sce3Job10JobManager8shutdownEv(void* self) {
    Manager* manager = Implementation(self);
    if (manager == nullptr) return ManagerNotInitialized;
    {
        const std::lock_guard guard(manager->lock);
        if (manager->blockedWaiters != 0) throw std::runtime_error("sce::Job::JobManager::shutdown: a thread is still blocked in a sequence wait");
        manager->running = false;
        manager->work.notify_all();
    }
    for (const Pthread worker : manager->workers) {
        if (scePthreadJoin(worker, nullptr) != 0) throw std::runtime_error("sce::Job::JobManager::shutdown: scePthreadJoin failed");
    }
    Implementation(self) = nullptr;
    delete manager;
    return 0;
}

ISequenceFactory* APS5_VABI _ZN3sce3Job10JobManager27getSequenceFactoryInterfaceENS1_8PriorityE(void* self, std::uint32_t priority) {
    return FactoryFor(self, priority, false);
}

ISequenceFactory* APS5_VABI _ZN3sce3Job10JobManager34getUnifiedSequenceFactoryInterfaceENS1_8PriorityE(void* self, std::uint32_t priority) {
    return FactoryFor(self, priority, true);
}

int APS5_VABI _ZN3sce3Job10JobManager24setPersistentThreadCountEj(void* self, std::uint32_t count) {
    Manager* manager = Implementation(self);
    if (manager == nullptr) return ManagerNotInitialized;
    if (count > manager->workers.size()) return ManagerInvalidArgument;
    const std::lock_guard guard(manager->lock);
    manager->persistentThreads = count;
    return 0;
}

}
