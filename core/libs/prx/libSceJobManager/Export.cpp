#include <algorithm>
#include <array>
#include <bit>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <immintrin.h>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>

#include "SceTypes.hpp"
#include "ComputeProgress.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

extern "C" {
int APS5_VABI _sceUlobjmgrRegisterObject_nid_postfix(std::uint64_t object, std::int32_t kind, std::uint32_t* id);
int APS5_VABI _sceUlobjmgrUnregisterObject_nid_postfix(std::uint32_t id);
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inheritSched);
int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param);
int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stackSize);
int APS5_VABI scePthreadAttrSetaffinity(PthreadAttr* attr, KernelCpumask mask);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** result);
std::uint64_t APS5_VABI sceKernelGetTscFrequency();
int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name);
int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq);
int APS5_VABI sceKernelWaitEqueue(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo);
int APS5_VABI sceKernelAddUserEventEdge(KernelEqueue eq, int id);
int APS5_VABI sceKernelTriggerUserEvent(KernelEqueue eq, int id, void* udata);
int APS5_VABI sceKernelGetEventFilter(const KernelEvent* ev);
std::uintptr_t APS5_VABI sceKernelGetEventId(const KernelEvent* ev);
int APS5_VABI sceAgcDriverAddEqEvent(KernelEqueue eq, int id, void* udata);
int APS5_VABI sceAgcDriverDeleteEqEvent(KernelEqueue eq, int id);
int APS5_VABI sceAgcDriverSubmitAcb(std::uint32_t queue, const Packet* packet);
std::uint32_t* APS5_VABI sceAgcAcbAcquireMem(CommandBuffer* buf, std::uint32_t gcrControl, const volatile void* base, std::uint64_t sizeBytes, std::uint32_t pollCycles);
std::uint32_t* APS5_VABI sceAgcCbSetShRegistersDirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs);
std::uint32_t* APS5_VABI sceAgcCbSetShRegisterRangeDirect(CommandBuffer* buf, std::uint32_t offset, const std::uint32_t* values, std::uint32_t numValues);
std::uint32_t* APS5_VABI sceAgcAcbPushMarker(CommandBuffer* buf, const char* str, std::uint32_t color);
std::uint32_t* APS5_VABI sceAgcAcbPopMarker(CommandBuffer* buf);
std::uint32_t* APS5_VABI sceAgcCbDispatch(CommandBuffer* buf, std::uint32_t threadGroupX, std::uint32_t threadGroupY, std::uint32_t threadGroupZ, std::uint32_t modifier);
std::uint32_t* APS5_VABI sceAgcAcbDispatchIndirect(CommandBuffer* buf, const volatile void* indirectArguments, std::uint32_t modifier);
std::uint32_t* APS5_VABI sceAgcCbReleaseMem(CommandBuffer* buf, std::uint8_t action, std::uint16_t gcrControl, std::uint8_t dst, std::uint8_t cachePolicy, const volatile Label* address, std::uint8_t dataSelect, std::uint64_t data, std::uint16_t gdsOffset, std::uint16_t gdsSize, std::uint8_t interrupt, std::uint32_t interruptContextId);
std::uint32_t* APS5_VABI sceAgcAcbWaitRegMem(CommandBuffer* buf, std::uint8_t size, std::uint8_t compareFunction, std::uint8_t cachePolicy, const volatile void* address, std::uint64_t reference, std::uint64_t mask, std::uint32_t pollCycles);
std::uint32_t* APS5_VABI sceAgcAcbRewind(CommandBuffer* buf, std::uint32_t initialState);
std::uint32_t* APS5_VABI sceAgcAcbWriteData(CommandBuffer* buf, std::uint8_t dst, std::uint8_t cachePolicy, std::uint64_t address, const void* data, std::uint32_t numDwords, std::uint8_t increment, std::uint8_t writeConfirm);
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
constexpr std::uint64_t ComputeContextBytes = 0x168;
constexpr std::uint32_t FirstComputeQueue = 0x20;
constexpr std::uint32_t UserDataRegister = 0x240;
constexpr int UserEventFilter = -11;

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

std::uint64_t ComputeCommandBudget() {
    std::array<std::uint32_t, 256> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    const ShaderRegister shaderRegister{};
    const std::array<std::uint32_t, 2> userData{};
    bool encoded = sceAgcAcbAcquireMem(&buffer, 0x20380, nullptr, 0, 0x190) != nullptr;
    encoded = encoded && sceAgcCbSetShRegistersDirect(&buffer, &shaderRegister, 1) != nullptr;
    encoded = encoded && sceAgcCbSetShRegisterRangeDirect(&buffer, UserDataRegister, userData.data(), 2) != nullptr;
    encoded = encoded && sceAgcAcbPushMarker(&buffer, "1234567890123456789012345678901", 0) != nullptr;
    encoded = encoded && sceAgcCbDispatch(&buffer, 1, 1, 1, 0) != nullptr;
    encoded = encoded && sceAgcAcbPopMarker(&buffer) != nullptr;
    const auto dispatchBytes = static_cast<std::uint64_t>(buffer.cursor_up - words.data()) * 4;
    buffer.cursor_up = words.data();
    encoded = encoded && sceAgcCbReleaseMem(&buffer, 0x2f, 0, 1, 3, nullptr, 1, 0, 0, 0, 0, 0) != nullptr;
    encoded = encoded && sceAgcAcbWaitRegMem(&buffer, 0, 3, 3, nullptr, 0, ~std::uint64_t{0}, 0x190) != nullptr;
    encoded = encoded && sceAgcAcbRewind(&buffer, 1) != nullptr;
    encoded = encoded && sceAgcCbReleaseMem(&buffer, 0x2f, 0x200, 1, 3, nullptr, 1, 0, 0, 0, 0, 0) != nullptr;
    encoded = encoded && sceAgcAcbWriteData(&buffer, 2, 3, 0, userData.data(), 1, 1, 0) != nullptr;
    const auto syncBytes = static_cast<std::uint64_t>(buffer.cursor_up - words.data()) * 4;
    buffer.cursor_up = words.data();
    encoded = encoded && sceAgcCbReleaseMem(&buffer, 0x2f, 0x200, 1, 3, nullptr, 1, 0, 0, 0, 2, 0) != nullptr;
    const auto waitBytes = static_cast<std::uint64_t>(buffer.cursor_up - words.data()) * 4;
    if (!encoded || dispatchBytes > 255 || syncBytes > 255 || waitBytes > 255) throw std::runtime_error("libSceJobManager: compute command size measurement failed");
    return MaxDispatches * dispatchBytes + MaxSyncPoints * syncBytes + MaxWaits * waitBytes;
}

struct Manager;
class Sequence;

enum class ItemType { Dispatch, SyncPoint, Wait, Compute, ComputeIndirect };

struct Item {
    Item* next = nullptr;
    Item* runnableNext = nullptr;
    Sequence* sequence;
    struct Cpu {
        JobEntry entry;
        void* argument;
        std::uint32_t count;
        std::uint32_t handedOut;
        std::uint32_t completed;
    };
    struct Compute {
        const void* shader;
        void* userData;
        union {
            std::array<std::uint32_t, 3> groups;
            const void* indirectArguments;
        };
    };
    union {
        Cpu cpu;
        Compute gpu;
        bool* passed;
    };
    const char* name = nullptr;
    ItemType type;
};

static_assert(sizeof(Item) == NodeBytes);

struct RunQueue {
    Item* first = nullptr;
    Item* last = nullptr;

    void Push(Item* item) {
        item->runnableNext = nullptr;
        if (last != nullptr) last->runnableNext = item;
        else first = item;
        last = item;
    }

    void Pop() {
        first = first->runnableNext;
        if (first == nullptr) last = nullptr;
    }
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
    APS5_VABI int DispatchCompute(const void* shader, void* userData, std::int32_t x, std::int32_t y, std::int32_t z, const char* name) override;
    APS5_VABI int DispatchComputeIndirect(const void* shader, void* userData, const void* indirectArguments, const char* name) override;

    bool Open(Manager* owner, const Counters& limit, std::uint32_t runQueue);
    int WaitAndClose(std::uint32_t index);
    void Advance();
    void DispatchCompleted(std::uint32_t invocations);
    void CollectCompute();
    void SetComputeStorage(std::uint32_t* commands, std::size_t words, Label* completion);
    void CancelWaits();
    bool Busy() const { return outstanding != 0; }

    Manager* manager = nullptr;
    bool open = false;
    bool compute = false;
    std::uint32_t queue = 0;

private:
    int Latch(int code);
    void RequireOpen() const;
    int WaitLocked(std::unique_lock<std::mutex>& guard);
    int AppendCompute(Item item);
    void Append(Item* item);
    void RemoveWait(Item* item);
    void SubmitCompute(const Item& item);
    void FlushCompute();

    Counters limits;
    Counters submitted;
    Counters completed;
    std::uint32_t priority = 0;
    int error = 0;
    std::uint32_t outstanding = 0;
    Item* reserved = nullptr;
    Item* first = nullptr;
    Item* last = nullptr;
    Item* cursor = nullptr;
    std::condition_variable passed;
    Label* label = nullptr;
    JobManager::ComputeProgress progress;
    std::uint32_t pendingCompute = 0;
    bool batchStarted = false;
    std::uint32_t* commandBegin = nullptr;
    std::size_t commandWords = 0;
    std::size_t commandUsed = 0;
};

static_assert(sizeof(Sequence) < SequenceBytes);

struct alignas(64) SequenceSlot {
    Sequence value;
    std::array<std::byte, SequenceBytes - sizeof(Sequence)> padding;
};

static_assert(sizeof(SequenceSlot) == SequenceBytes);

struct WorkerSlot {
    Pthread thread = nullptr;
    std::array<std::byte, ThreadRecordBytes - sizeof(Pthread)> padding;
};

static_assert(sizeof(WorkerSlot) == ThreadRecordBytes);

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
    std::uint32_t objectId = 0;
    std::mutex lock;
    std::condition_variable work;
    std::condition_variable quiesced;
    bool running = false;
    std::uint32_t blockedWaiters = 0;
    Item* freeNodes = nullptr;
    std::uint32_t freeSequences = 0;
    std::uint32_t persistentThreads = 0;
    std::uint32_t workerCount = 0;
    std::uint32_t sleepingWorkers = 0;
    std::uint32_t wakeTokens = 0;
    std::uint64_t pendingInvocations = 0;
    std::uint64_t spinTicks = 0;
    std::array<RunQueue, 2> runQueues;
    std::span<SequenceSlot> sequences;
    std::span<SequenceSlot> computeSequences;
    std::uint32_t freeComputeSequences = 0;
    std::uint64_t computeQueues = 0;
    KernelEqueue computeEvents = 0;
    Pthread computeHandler = nullptr;
    std::array<Factory, 2> factories;
    std::array<Factory, 2> unifiedFactories;
    std::span<WorkerSlot> workers;
    std::uint32_t activeWorkers = 0;
    void* stateBegin = nullptr;
    std::size_t stateBytes = 0;
    std::span<Item> nodes;
};

static_assert(sizeof(Manager) <= StateEnd - StateOffset);

thread_local Manager* currentWorker = nullptr;

void WakeWorkers(Manager& manager, std::uint32_t invocations, bool publisherCanRun = true) {
    const std::uint32_t available = invocations - (publisherCanRun && currentWorker == &manager ? 1u : 0u);
    manager.wakeTokens += std::min(available, manager.sleepingWorkers - manager.wakeTokens);
    if (manager.wakeTokens != 0) manager.work.notify_all();
}

Manager*& Implementation(void* self) {
    return *static_cast<Manager**>(self);
}

Item* TakeNode(Manager& manager) {
    Item* item = manager.freeNodes;
    if (item != nullptr) {
        manager.freeNodes = item->next;
        *item = {};
    }
    return item;
}

void ReturnNode(Manager& manager, Item* item) {
    item->next = manager.freeNodes;
    manager.freeNodes = item;
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
    cursor = nullptr;
    first = nullptr;
    last = nullptr;
    commandUsed = 0;
    batchStarted = false;
    reserved = TakeNode(*owner);
    if (reserved == nullptr) return false;
    open = true;
    return true;
}

void Sequence::RequireOpen() const {
    if (!open) throw std::runtime_error("libSceJobManager: a destroyed sequence was used");
}

void Sequence::Append(Item* item) {
    item->next = nullptr;
    if (last != nullptr) last->next = item;
    else first = item;
    last = item;
    if (cursor == nullptr) cursor = item;
}

void Sequence::RemoveWait(Item* item) {
    Item* previous = nullptr;
    for (Item* current = first; current != nullptr; current = current->next) {
        if (current == item) {
            if (previous != nullptr) previous->next = item->next;
            else first = item->next;
            if (last == item) last = previous;
            if (cursor == item) cursor = item->next;
            return;
        }
        previous = current;
    }
}

void Sequence::Advance() {
    while (cursor != nullptr) {
        Item& item = *cursor;
        if (item.type == ItemType::Dispatch) {
            if (!manager->running) return;
            ++outstanding;
            cursor = item.next;
            manager->runQueues[priority].Push(&item);
            manager->pendingInvocations -= item.cpu.count;
            WakeWorkers(*manager, item.cpu.count);
            continue;
        }
        if (item.type == ItemType::Compute || item.type == ItemType::ComputeIndirect) {
            if (!manager->running) return;
            ++outstanding;
            cursor = item.next;
            SubmitCompute(item);
            continue;
        }
        FlushCompute();
        if (outstanding != 0) return;
        if (item.type == ItemType::SyncPoint) {
            ++completed.syncPoints;
            cursor = item.next;
            continue;
        }
        bool* const waiter = item.passed;
        while (first != &item) {
            Item* retired = first;
            first = first->next;
            ReturnNode(*manager, retired);
        }
        first = item.next;
        if (last == &item) last = nullptr;
        cursor = first;
        commandUsed = 0;
        batchStarted = false;
        if (error == 0) {
            submitted.invocations -= completed.invocations;
            submitted.dispatches -= completed.dispatches;
            submitted.syncPoints -= completed.syncPoints;
            --submitted.waits;
            completed = {};
        }
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

void Sequence::CancelWaits() {
    for (Item* item = first; item != nullptr; item = item->next) {
        if (item->type == ItemType::Wait) *item->passed = true;
    }
    passed.notify_all();
}

int Sequence::Dispatch(JobEntry entry, void* argument, std::uint32_t count, const char* name) {
    if (entry == nullptr) return SequenceNullArgument;
    if (count == 0 || count > MaxInvocations) return SequenceOutOfRange;
    const std::lock_guard guard(manager->lock);
    RequireOpen();
    if (error != 0) return SequenceInvalid;
    const Counters before = submitted;
    submitted.invocations += count;
    ++submitted.dispatches;
    if (before.invocations + count > limits.invocations || before.dispatches >= limits.dispatches) return Latch(SequenceLimitExceeded);
    Item* item = TakeNode(*manager);
    if (item == nullptr) return Latch(SequenceNoNodes);
    item->type = ItemType::Dispatch;
    item->sequence = this;
    item->name = name;
    item->cpu = {entry, argument, count, 0, 0};
    Append(item);
    manager->pendingInvocations += count;
    Advance();
    return 0;
}

void Sequence::SubmitCompute(const Item& item) {
    const auto* bytes = static_cast<const std::uint8_t*>(item.gpu.shader);
    const ShaderRegister* registers = nullptr;
    const std::uint8_t* specials = nullptr;
    std::memcpy(&registers, bytes + 0x20, sizeof(registers));
    std::memcpy(&specials, bytes + 0x28, sizeof(specials));
    const std::uint32_t registerCount = bytes[0x5c];
    std::uint32_t modifier = 0;
    std::memcpy(&modifier, specials + 0x10, sizeof(modifier));
    const auto userData = reinterpret_cast<std::uint64_t>(item.gpu.userData);
    const std::array<std::uint32_t, 2> userDataWords{static_cast<std::uint32_t>(userData), static_cast<std::uint32_t>(userData >> 32)};
    const auto capacity = std::min(commandWords - commandUsed, 64 + 3 * registerCount + (item.name != nullptr ? std::strlen(item.name) / 4 : 0));
    auto* const words = commandBegin + commandUsed;
    CommandBuffer buffer{words, words + capacity, words, words + capacity, nullptr, nullptr, 0};
    {
        const GuestArena::HostWrite destination(words, capacity * 4);
        bool encoded = sceAgcAcbAcquireMem(&buffer, 0x20380u | (batchStarted ? 0u : 1u), nullptr, 0, 0x190) != nullptr;
        encoded = encoded && sceAgcCbSetShRegistersDirect(&buffer, registers, registerCount) != nullptr;
        encoded = encoded && sceAgcCbSetShRegisterRangeDirect(&buffer, UserDataRegister, userDataWords.data(), 2) != nullptr;
        if (item.name != nullptr) encoded = encoded && sceAgcAcbPushMarker(&buffer, item.name, 0) != nullptr;
        if (item.type == ItemType::Compute) {
            encoded = encoded && sceAgcCbDispatch(&buffer, item.gpu.groups[0], item.gpu.groups[1], item.gpu.groups[2], modifier) != nullptr;
        } else {
            encoded = encoded && sceAgcAcbDispatchIndirect(&buffer, item.gpu.indirectArguments, modifier) != nullptr;
        }
        if (item.name != nullptr) encoded = encoded && sceAgcAcbPopMarker(&buffer) != nullptr;
        if (!encoded) throw std::runtime_error("libSceJobManager: a compute dispatch did not fit its command buffer");
    }
    batchStarted = true;
    ++pendingCompute;
    const Packet packet{words, static_cast<std::uint32_t>(buffer.cursor_up - words), 0, {}};
    commandUsed += packet.dw_num;
    if (sceAgcDriverSubmitAcb(queue, &packet) != 0) throw std::runtime_error("libSceJobManager: sceAgcDriverSubmitAcb failed");
}

void Sequence::SetComputeStorage(std::uint32_t* commands, std::size_t words, Label* completion) {
    commandBegin = commands;
    commandWords = words;
    label = completion;
    const GuestArena::HostWrite destination(label, sizeof(*label));
    label->value = 0;
}

void Sequence::FlushCompute() {
    if (pendingCompute == 0) return;
    if (commandWords - commandUsed < 8) throw std::runtime_error("libSceJobManager: a compute completion did not fit its command buffer");
    auto* const words = commandBegin + commandUsed;
    CommandBuffer buffer{words, words + 8, words, words + 8, nullptr, nullptr, 0};
    {
        const GuestArena::HostWrite destination(words, 32);
        if (sceAgcCbReleaseMem(&buffer, 0x2f, 0x200, 1, 3, label, 1, progress.Publish(pendingCompute), 0, 0, 2, 0) == nullptr) {
            throw std::runtime_error("libSceJobManager: a compute completion could not be encoded");
        }
    }
    pendingCompute = 0;
    const Packet packet{words, static_cast<std::uint32_t>(buffer.cursor_up - words), 0, {}};
    commandUsed += packet.dw_num;
    if (sceAgcDriverSubmitAcb(queue, &packet) != 0) throw std::runtime_error("libSceJobManager: sceAgcDriverSubmitAcb failed for a completion");
}

void Sequence::CollectCompute() {
    const auto finished = progress.Collect(label->value);
    if (finished == 0) return;
    completed.dispatches += finished;
    outstanding -= finished;
    if (outstanding == 0) Advance();
}

int Sequence::AppendCompute(Item item) {
    const std::lock_guard guard(manager->lock);
    RequireOpen();
    if (!compute) throw std::runtime_error("libSceJobManager: a compute dispatch was made on a CPU sequence");
    if (error != 0) return SequenceInvalid;
    if (submitted.dispatches++ >= limits.dispatches) return Latch(SequenceLimitExceeded);
    Item* stored = TakeNode(*manager);
    if (stored == nullptr) return Latch(SequenceNoNodes);
    *stored = item;
    Append(stored);
    Advance();
    return 0;
}

int Sequence::DispatchCompute(const void* shader, void* userData, std::int32_t x, std::int32_t y, std::int32_t z, const char* name) {
    if (shader == nullptr) return SequenceNullArgument;
    if (x <= 0 || y <= 0 || z <= 0) return SequenceOutOfRange;
    Item item{};
    item.type = ItemType::Compute;
    item.sequence = this;
    item.gpu.shader = shader;
    item.gpu.userData = userData;
    item.gpu.groups = {static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), static_cast<std::uint32_t>(z)};
    item.name = name;
    return AppendCompute(item);
}

int Sequence::DispatchComputeIndirect(const void* shader, void* userData, const void* indirectArguments, const char* name) {
    if (shader == nullptr || indirectArguments == nullptr || (reinterpret_cast<std::uint64_t>(indirectArguments) & 0x1f) != 0) return SequenceNullArgument;
    Item item{};
    item.type = ItemType::ComputeIndirect;
    item.sequence = this;
    item.gpu.shader = shader;
    item.gpu.userData = userData;
    item.gpu.indirectArguments = indirectArguments;
    item.name = name;
    return AppendCompute(item);
}

int Sequence::SyncPoint() {
    const std::lock_guard guard(manager->lock);
    RequireOpen();
    if (error != 0) return SequenceInvalid;
    const std::uint32_t before = submitted.syncPoints++;
    if (before >= limits.syncPoints) return Latch(SequenceLimitExceeded);
    Item* item = TakeNode(*manager);
    if (item == nullptr) return Latch(SequenceNoNodes);
    item->type = ItemType::SyncPoint;
    item->sequence = this;
    Append(item);
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
    while (first != nullptr) {
        Item* item = first;
        first = item->next;
        if (item->type != ItemType::Wait) ReturnNode(*manager, item);
    }
    ReturnNode(*manager, reserved);
    reserved = nullptr;
    last = nullptr;
    commandUsed = 0;
    cursor = nullptr;
    outstanding = 0;
    open = false;
    (compute ? manager->freeComputeSequences : manager->freeSequences) |= std::uint32_t{1} << index;
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
    ++manager->blockedWaiters;
    bool done = false;
    Item waiter{};
    waiter.type = ItemType::Wait;
    waiter.sequence = this;
    waiter.passed = &done;
    Append(&waiter);
    Advance();
    if (!done && currentWorker == manager) {
        std::uint64_t runnable = 0;
        for (const auto& queue : manager->runQueues) {
            for (const Item* item = queue.first; item != nullptr; item = item->runnableNext) runnable += item->cpu.count - item->cpu.handedOut;
        }
        WakeWorkers(*manager, static_cast<std::uint32_t>(std::min(runnable, std::uint64_t{manager->workerCount})), false);
    }
    passed.wait(guard, [&done] { return done; });
    RemoveWait(&waiter);
    --manager->blockedWaiters;
    manager->quiesced.notify_all();
    return result;
}

bool ValidLimits(std::uint32_t invocations, std::uint32_t dispatches, std::uint32_t syncPoints, std::uint32_t waits) {
    if (invocations > MaxInvocations || dispatches > MaxDispatches || syncPoints > MaxSyncPoints || waits > MaxWaits) return false;
    return invocations != 0 || dispatches != 0 || syncPoints != 0 || waits != 0;
}

template <typename TSequences>
int CreateIn(Manager* manager, TSequences& sequences, std::uint32_t& free, std::uint32_t priority, ISequence** out, const Counters& limits) {
    if (out == nullptr) return SequenceNullArgument;
    if (!ValidLimits(limits.invocations, limits.dispatches, limits.syncPoints, limits.waits)) return SequenceOutOfRange;
    const std::lock_guard guard(manager->lock);
    if (free == 0) return SequenceNoneFree;
    const int index = std::countr_zero(free);
    free &= ~(std::uint32_t{1} << index);
    Sequence& sequence = sequences[static_cast<std::size_t>(index)].value;
    if (!sequence.Open(manager, limits, priority)) return SequenceNoNodes;
    *out = &sequence;
    return 0;
}

template <typename TSequences>
int DestroyIn(Manager* manager, TSequences& sequences, const std::uint32_t& free, ISequence* sequence) {
    Sequence* found = nullptr;
    std::uint32_t index = 0;
    {
        const std::lock_guard guard(manager->lock);
        for (; index < sequences.size(); ++index) {
            if (&sequences[index].value == sequence) break;
        }
        if (index == sequences.size() || (free & (std::uint32_t{1} << index)) != 0) return SequenceInvalid;
        found = &sequences[index].value;
    }
    static_cast<void>(found->WaitAndClose(index));
    return 0;
}

int Factory::Create(ISequence** out, std::uint32_t invocations, std::uint32_t dispatches, std::uint32_t syncPoints, std::uint32_t waits, const char*) {
    return CreateIn(manager, manager->sequences, manager->freeSequences, priority, out, {invocations, dispatches, syncPoints, waits});
}

int Factory::Destroy(ISequence* sequence) {
    return DestroyIn(manager, manager->sequences, manager->freeSequences, sequence);
}

int Factory::CreateCompute(ISequence** out, std::uint32_t invocations, std::uint32_t dispatches, std::uint32_t syncPoints, std::uint32_t waits, const char*) {
    return CreateIn(manager, manager->computeSequences, manager->freeComputeSequences, priority, out, {invocations, dispatches, syncPoints, waits});
}

int Factory::DestroyCompute(ISequence* sequence) {
    return DestroyIn(manager, manager->computeSequences, manager->freeComputeSequences, sequence);
}

void* APS5_VABI ComputeHandlerEntry(void* argument) {
    Manager& manager = *static_cast<Manager*>(argument);
    for (;;) {
        KernelEvent event{};
        int count = 0;
        if (sceKernelWaitEqueue(manager.computeEvents, &event, 1, &count, nullptr) != 0) throw std::runtime_error("libSceJobManager: sceKernelWaitEqueue failed in the compute handler");
        if (count <= 0) continue;
        if (sceKernelGetEventFilter(&event) == UserEventFilter && sceKernelGetEventId(&event) == 0) return nullptr;
        const std::lock_guard guard(manager.lock);
        for (SequenceSlot& slot : manager.computeSequences) slot.value.CollectCompute();
    }
}

bool RunOne(Manager& manager, std::unique_lock<std::mutex>& guard) {
    Item* item = nullptr;
    for (const std::size_t queue : {std::size_t{1}, std::size_t{0}}) {
        auto& pending = manager.runQueues[queue];
        if (pending.first == nullptr) continue;
        item = pending.first;
        if (item->cpu.handedOut + 1 == item->cpu.count) pending.Pop();
        break;
    }
    if (item == nullptr) return false;
    const std::uint32_t index = item->cpu.handedOut++;
    const JobEntry entry = item->cpu.entry;
    void* const argument = item->cpu.argument;
    guard.unlock();
    entry(argument, index);
    guard.lock();
    if (++item->cpu.completed == item->cpu.count) item->sequence->DispatchCompleted(item->cpu.count);
    return true;
}

void* APS5_VABI WorkerEntry(void* argument) {
    Manager& manager = *static_cast<Manager*>(argument);
    currentWorker = &manager;
    std::uint64_t lastWork = __rdtsc();
    std::unique_lock guard(manager.lock);
    while (manager.running) {
        if (RunOne(manager, guard)) {
            lastWork = __rdtsc();
            continue;
        }
        const bool spinning = manager.pendingInvocations != 0 && __rdtsc() - lastWork < manager.spinTicks;
        if (spinning || manager.sleepingWorkers >= manager.workerCount - manager.persistentThreads) {
            guard.unlock();
            _mm_pause();
            guard.lock();
            continue;
        }
        ++manager.sleepingWorkers;
        manager.work.wait(guard, [&manager] { return !manager.running || manager.wakeTokens != 0; });
        if (manager.wakeTokens != 0) --manager.wakeTokens;
        --manager.sleepingWorkers;
    }
    currentWorker = nullptr;
    return nullptr;
}

struct WorkLayout {
    std::uint64_t workers = 0;
    std::uint64_t sequences = 0;
    std::uint64_t stateEnd = 0;
    std::uint64_t pool = 0;
    std::uint64_t commands = 0;
    std::uint64_t commandBytes = 0;
    std::uint64_t labels = 0;
    std::uint32_t nodes = 0;
};

WorkLayout LayoutFor(std::uint64_t memory, std::uint64_t size, std::uint32_t workers, std::uint32_t sequences, std::uint32_t computeQueues) {
    WorkLayout layout;
    const std::uint64_t end = memory + size;
    layout.workers = AlignUp(memory + StateEnd, 8);
    std::uint64_t address = layout.workers + workers * ThreadRecordBytes;
    for (const std::uint64_t bytes : {std::uint64_t{0x100}, std::uint64_t{0x80}, std::uint64_t{0x80}, std::uint64_t{0x80}}) address = AlignUp(address, 64) + bytes;
    layout.sequences = AlignUp(address, 64);
    address = layout.sequences + (sequences + computeQueues) * SequenceBytes;
    layout.stateEnd = address;
    if (computeQueues != 0) {
        layout.commandBytes = ComputeCommandBudget();
        layout.commands = AlignUp(address + ComputeContextBytes, 0x100);
        layout.labels = AlignUp(layout.commands + layout.commandBytes * computeQueues + 4 * std::uint64_t{computeQueues}, 8);
        address = layout.labels + 8 * std::uint64_t{computeQueues};
    }
    address = AlignUp(address, 64);
    layout.pool = address;
    if (address <= end) layout.nodes = static_cast<std::uint32_t>(((end - address) / NodeBytes) & 0xffff);
    return layout;
}

int ValidateWorkers(const ConfigParams& config, std::uint32_t workers) {
    const std::uint32_t mode = config.threadFlags & 0x30000;
    if (config.reserved != 0 || config.priority < 0x100 || config.priority > 0x2ff || config.stackSize < 0x4000) return ManagerInvalidArgument;
    if ((config.threadFlags & 0xfffce000) != 0 || static_cast<std::uint32_t>(std::popcount(config.threadFlags & 0x1fff)) < workers) return ManagerInvalidArgument;
    if ((mode != 0x10000 && mode != 0x20000) || config.spinMicroseconds > 0x32) return ManagerInvalidArgument;
    return 0;
}

class ThreadAttributes {
public:
    ThreadAttributes() {
        if (scePthreadAttrInit(&value) != 0) throw std::runtime_error("libSceJobManager: scePthreadAttrInit failed");
    }
    ~ThreadAttributes() { static_cast<void>(scePthreadAttrDestroy(&value)); }
    ThreadAttributes(const ThreadAttributes&) = delete;
    ThreadAttributes& operator=(const ThreadAttributes&) = delete;
    PthreadAttr value = nullptr;
};

void StartWorkers(Manager& manager, const ConfigParams& config, std::uint32_t workers) {
    std::uint32_t remaining = config.threadFlags & 0x1fff;
    for (std::uint32_t index = 0; index < workers; ++index) {
        KernelCpumask affinity = config.threadFlags & 0x1fff;
        if ((config.threadFlags & 0x30000) == 0x10000) {
            affinity = remaining & (~remaining + 1);
            remaining &= remaining - 1;
        }
        std::array<char, 32> name{};
        if (config.threadName != nullptr) std::snprintf(name.data(), name.size(), "%.31s", config.threadName);
        else std::snprintf(name.data(), name.size(), "SceJobManagerWorker%u", index);
        const KernelSchedParam schedule{static_cast<int>(config.priority)};
        ThreadAttributes attributes;
        if (scePthreadAttrSetinheritsched(&attributes.value, 0) != 0 || scePthreadAttrSetschedparam(&attributes.value, &schedule) != 0 ||
            scePthreadAttrSetstacksize(&attributes.value, config.stackSize) != 0 || scePthreadAttrSetaffinity(&attributes.value, affinity) != 0) {
            throw std::runtime_error("libSceJobManager: a worker thread attribute was rejected");
        }
        Pthread thread = nullptr;
        const int created = scePthreadCreate(&thread, &attributes.value, WorkerEntry, &manager, name.data());
        if (created != 0) throw std::runtime_error("libSceJobManager: scePthreadCreate failed for a worker");
        manager.workers[manager.activeWorkers++].thread = thread;
    }
}

void StartCompute(Manager& manager, const WorkLayout& layout, std::uint32_t computeSequences, std::uint64_t computeQueueMask, std::uint32_t priority, std::uint32_t affinity) {
    if (sceKernelCreateEqueue(&manager.computeEvents, "SceJobManagerComputeEventQueue") != 0) throw std::runtime_error("libSceJobManager: sceKernelCreateEqueue failed");
    for (std::uint64_t mask = computeQueueMask; mask != 0; mask &= mask - 1) {
        const std::uint32_t queue = FirstComputeQueue + static_cast<std::uint32_t>(std::countr_zero(mask));
        if (sceAgcDriverAddEqEvent(manager.computeEvents, static_cast<int>(queue), nullptr) != 0) throw std::runtime_error("libSceJobManager: sceAgcDriverAddEqEvent failed");
        manager.computeQueues |= std::uint64_t{1} << (queue - FirstComputeQueue);
    }
    std::uint64_t availableQueues = computeQueueMask;
    for (std::uint32_t index = 0; index < computeSequences; ++index) {
        Sequence& sequence = manager.computeSequences[index].value;
        sequence.compute = true;
        sequence.queue = FirstComputeQueue + static_cast<std::uint32_t>(std::countr_zero(availableQueues));
        availableQueues &= availableQueues - 1;
        sequence.SetComputeStorage(reinterpret_cast<std::uint32_t*>(layout.commands + index * layout.commandBytes),
            static_cast<std::size_t>(layout.commandBytes / 4), reinterpret_cast<Label*>(layout.labels + index * sizeof(Label)));
    }
    manager.freeComputeSequences = computeSequences == MaxSequences ? ~std::uint32_t{0} : (std::uint32_t{1} << computeSequences) - 1;
    const KernelSchedParam schedule{static_cast<int>(priority)};
    ThreadAttributes attributes;
    if (scePthreadAttrSetinheritsched(&attributes.value, 0) != 0 || scePthreadAttrSetschedparam(&attributes.value, &schedule) != 0 ||
        scePthreadAttrSetstacksize(&attributes.value, 0x4000) != 0 || scePthreadAttrSetaffinity(&attributes.value, affinity) != 0) {
        throw std::runtime_error("libSceJobManager: a compute handler thread attribute was rejected");
    }
    const int created = scePthreadCreate(&manager.computeHandler, &attributes.value, ComputeHandlerEntry, &manager, "SceJobManagerComputeEventHdlr");
    if (created != 0) throw std::runtime_error("libSceJobManager: scePthreadCreate failed for the compute handler");
}

void StopManager(Manager& manager) {
    {
        const std::lock_guard guard(manager.lock);
        manager.running = false;
        manager.work.notify_all();
    }
    while (manager.activeWorkers != 0) {
        if (scePthreadJoin(manager.workers[manager.activeWorkers - 1].thread, nullptr) != 0) throw std::runtime_error("sce::Job::JobManager::shutdown: scePthreadJoin failed");
        manager.workers[--manager.activeWorkers].thread = nullptr;
    }
    if (manager.computeHandler != nullptr) {
        if (sceKernelAddUserEventEdge(manager.computeEvents, 0) != 0 || sceKernelTriggerUserEvent(manager.computeEvents, 0, nullptr) != 0) {
            throw std::runtime_error("sce::Job::JobManager::shutdown: the compute handler could not be signalled");
        }
        if (scePthreadJoin(manager.computeHandler, nullptr) != 0) throw std::runtime_error("sce::Job::JobManager::shutdown: scePthreadJoin failed");
        manager.computeHandler = nullptr;
    }
    if (manager.computeEvents != 0) {
        for (std::uint64_t mask = manager.computeQueues; mask != 0; mask &= mask - 1) {
            const auto queue = FirstComputeQueue + static_cast<std::uint32_t>(std::countr_zero(mask));
            static_cast<void>(sceAgcDriverDeleteEqEvent(manager.computeEvents, static_cast<int>(queue)));
        }
        if (sceKernelDeleteEqueue(manager.computeEvents) != 0) throw std::runtime_error("sce::Job::JobManager::shutdown: sceKernelDeleteEqueue failed");
        manager.computeEvents = 0;
        manager.computeQueues = 0;
    }
    std::unique_lock guard(manager.lock);
    for (SequenceSlot& slot : manager.sequences) slot.value.CancelWaits();
    for (SequenceSlot& slot : manager.computeSequences) slot.value.CancelWaits();
    manager.quiesced.wait(guard, [&manager] { return manager.blockedWaiters == 0; });
}

void DestroyManager(Manager* manager) {
    const GuestArena::HostWrite memory(manager->stateBegin, manager->stateBytes);
    for (SequenceSlot& slot : manager->sequences) std::destroy_at(&slot);
    for (SequenceSlot& slot : manager->computeSequences) std::destroy_at(&slot);
    std::destroy_at(manager);
}

int Initialize(void* self, void* memory, std::uint64_t size, std::uint32_t workers, std::uint32_t sequences, std::uint32_t computeSequences, std::uint64_t computeQueueMask, const ConfigParams* config, bool unified) {
    if (Implementation(self) != nullptr) return ManagerAlreadyInitialized;
    if (memory == nullptr) return ManagerNullMemory;
    const auto base = reinterpret_cast<std::uint64_t>(memory);
    if ((base & 0xff) != 0) return ManagerMisalignedMemory;
    if (size == 0 || workers == 0 || workers > MaxWorkers) return ManagerInvalidArgument;
    if (unified) {
        if ((sequences == 0 && computeSequences == 0) || std::uint64_t{sequences} + computeSequences > MaxSequences || (computeQueueMask >> 56) != 0 || static_cast<std::uint32_t>(std::popcount(computeQueueMask)) < computeSequences) return ManagerInvalidArgument;
    } else if (sequences == 0 || sequences > MaxSequences) {
        return ManagerInvalidArgument;
    }
    if (size < StateOffset) return ManagerMemoryTooSmall;
    if (size < StateEnd) throw std::runtime_error("sce::Job::JobManager::initialize: a memory block smaller than 0x2d8 bytes is not supported");
    {
        const GuestArena::HostWrite destination(memory, StateEnd);
        std::memset(static_cast<std::uint8_t*>(memory) + StateOffset, 0, StateEnd - StateOffset);
        const BlockHeader header{0x4d4a, static_cast<std::uint16_t>(StateOffset), 0x05000001, static_cast<std::uint32_t>(size), 0};
        std::memcpy(memory, &header, sizeof(header));
    }
    if (config != nullptr && config->version > 1) return ManagerInvalidArgument;
    const ConfigParams& settings = config != nullptr ? *config : DefaultConfig;
    const auto computeQueues = computeSequences != 0 ? static_cast<std::uint32_t>(std::popcount(computeQueueMask)) : 0;
    const auto layout = LayoutFor(base, size, workers, sequences, computeQueues);
    if (layout.nodes == 0) return ManagerMemoryTooSmall;
    if (const int invalid = ValidateWorkers(settings, workers); invalid != 0) return invalid;
    const bool ownHandlerSettings = (settings.version & 1) != 0;
    const std::uint32_t handlerPriority = ownHandlerSettings ? settings.computePriority : 0x100;
    const std::uint32_t handlerAffinity = ownHandlerSettings ? settings.computeAffinity : 0x1fff;
    if (computeSequences != 0 && (handlerPriority < 0x100 || handlerPriority > 0x2ff || handlerAffinity == 0 || handlerAffinity > 0x1fff)) return ManagerInvalidArgument;
    const GuestArena::HostWrite stateMemory(memory, static_cast<std::size_t>(layout.stateEnd - base));
    const GuestArena::HostWrite nodeMemory(reinterpret_cast<void*>(layout.pool), layout.nodes * sizeof(Item));
    Manager* manager = std::construct_at(reinterpret_cast<Manager*>(base + StateOffset));
    manager->stateBegin = memory;
    manager->stateBytes = static_cast<std::size_t>(layout.stateEnd - base);
    manager->nodes = {reinterpret_cast<Item*>(layout.pool), layout.nodes};
    std::unique_ptr<Manager, decltype(&DestroyManager)> owner(manager, DestroyManager);
    manager->running = true;
    manager->workerCount = workers;
    manager->spinTicks = sceKernelGetTscFrequency() / 1000000 * settings.spinMicroseconds;
    for (std::uint32_t index = 0; index < layout.nodes; ++index) std::construct_at(&manager->nodes[index]);
    for (std::uint32_t index = layout.nodes; index > 1; --index) ReturnNode(*manager, &manager->nodes[index - 1]);
    manager->freeSequences = sequences == MaxSequences ? ~std::uint32_t{0} : (std::uint32_t{1} << sequences) - 1;
    manager->workers = {reinterpret_cast<WorkerSlot*>(layout.workers), workers};
    for (WorkerSlot& worker : manager->workers) std::construct_at(&worker);
    auto* cpuSlots = reinterpret_cast<SequenceSlot*>(layout.sequences);
    auto* computeSlots = cpuSlots + sequences;
    for (std::uint32_t index = 0; index < sequences; ++index) {
        std::construct_at(cpuSlots + index);
        manager->sequences = {cpuSlots, index + 1};
    }
    for (std::uint32_t index = 0; index < computeSequences; ++index) {
        std::construct_at(computeSlots + index);
        manager->computeSequences = {computeSlots, index + 1};
    }
    for (std::uint32_t priority = 0; priority < 2; ++priority) {
        manager->factories[priority].manager = manager;
        manager->factories[priority].priority = priority;
        manager->unifiedFactories[priority].manager = manager;
        manager->unifiedFactories[priority].priority = priority;
    }
    try {
        if (computeSequences != 0) StartCompute(*manager, layout, computeSequences, computeQueueMask, handlerPriority, handlerAffinity);
        StartWorkers(*manager, settings, workers);
    } catch (...) {
        try {
            StopManager(*manager);
        } catch (...) {
            Implementation(self) = owner.release();
            throw;
        }
        throw;
    }
    Implementation(self) = owner.release();
    return _sceUlobjmgrRegisterObject_nid_postfix(base, 0x21, &manager->objectId);
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

int APS5_VABI _ZN3sce3Job10JobManager8shutdownEv(void* self);

void APS5_VABI _ZN3sce3Job10JobManagerD1Ev(void* self) {
    if (Implementation(self) != nullptr) {
        static_cast<void>(_ZN3sce3Job10JobManager8shutdownEv(self));
    }
}

void APS5_VABI _ZN3sce3Job10JobManagerD2Ev(void* self) {
    _ZN3sce3Job10JobManagerD1Ev(self);
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
    std::uint64_t bytes = (NodeBytes * params->workers + 0x317) & 0xfc0;
    bytes = (bytes + 0x13f) & ~std::uint64_t{0x3f};
    bytes = (bytes + 0x0bf) & ~std::uint64_t{0x3f};
    bytes = (bytes + 0x13f) & ~std::uint64_t{0x3f};
    if (computeSequences != 0) {
        const auto queues = static_cast<std::uint64_t>(std::popcount(params->computeQueueMask));
        bytes = (bytes + (params->sequences + queues) * SequenceBytes + 0x267) & ~std::uint64_t{0xff};
        bytes = (bytes + ComputeCommandBudget() * queues + 3) & ~std::uint64_t{3};
        bytes = (bytes + 4 * queues + 7) & ~std::uint64_t{7};
        bytes += 8 * queues;
    } else {
        bytes += params->sequences * SequenceBytes;
    }
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
    if (currentWorker == manager) throw std::runtime_error("sce::Job::JobManager::shutdown: a worker cannot join itself");
    {
        const std::lock_guard guard(manager->lock);
        for (const SequenceSlot& slot : manager->computeSequences) {
            const Sequence& sequence = slot.value;
            if (sequence.open && sequence.Busy()) throw std::runtime_error("sce::Job::JobManager::shutdown: a compute sequence still has work in flight");
        }
        manager->running = false;
    }
    StopManager(*manager);
    static_cast<void>(_sceUlobjmgrUnregisterObject_nid_postfix(manager->objectId));
    Implementation(self) = nullptr;
    DestroyManager(manager);
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
    if (count > manager->workerCount) return ManagerInvalidArgument;
    const std::lock_guard guard(manager->lock);
    manager->persistentThreads = count;
    return 0;
}

}
