#include <algorithm>
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
#include "ComputeProgress.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

extern "C" {
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inheritSched);
int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param);
int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stackSize);
int APS5_VABI scePthreadAttrSetaffinity(PthreadAttr* attr, KernelCpumask mask);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** result);
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
    ItemType type;
    Sequence* sequence;
    JobEntry entry;
    void* argument;
    std::uint32_t count;
    std::uint32_t handedOut;
    std::uint32_t completed;
    bool* passed;
    const void* shader = nullptr;
    std::array<std::uint32_t, 3> groups{};
    const void* indirectArguments = nullptr;
    std::string name;
    bool named = false;
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
    void SubmitCompute(const Item& item);
    void FlushCompute();

    Counters limits;
    Counters submitted;
    Counters completed;
    std::uint32_t priority = 0;
    int error = 0;
    std::uint32_t outstanding = 0;
    std::size_t cursor = 0;
    std::deque<Item> items;
    std::condition_variable passed;
    Label* label = nullptr;
    JobManager::ComputeProgress progress;
    std::uint32_t pendingCompute = 0;
    bool batchStarted = false;
    std::uint32_t* commandBegin = nullptr;
    std::size_t commandWords = 0;
    std::size_t commandUsed = 0;
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
    bool running = false;
    std::uint32_t blockedWaiters = 0;
    std::uint32_t freeNodes = 0;
    std::uint32_t freeSequences = 0;
    std::uint32_t persistentThreads = 0;
    std::array<std::deque<Item*>, 2> runQueues;
    std::vector<Sequence> sequences;
    std::deque<Sequence> computeSequences;
    std::uint32_t freeComputeSequences = 0;
    std::vector<std::uint32_t> computeQueues;
    KernelEqueue computeEvents = 0;
    Pthread computeHandler = nullptr;
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
    commandUsed = 0;
    batchStarted = false;
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
        if (item.type == ItemType::Compute || item.type == ItemType::ComputeIndirect) {
            ++outstanding;
            ++cursor;
            SubmitCompute(item);
            continue;
        }
        FlushCompute();
        if (outstanding != 0) return;
        if (item.type == ItemType::SyncPoint) {
            ++completed.syncPoints;
            ++cursor;
            continue;
        }
        bool* const waiter = item.passed;
        const auto retired = static_cast<std::uint32_t>(cursor);
        items.erase(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(retired + 1));
        cursor = 0;
        commandUsed = 0;
        batchStarted = false;
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

void Sequence::SubmitCompute(const Item& item) {
    const auto* bytes = static_cast<const std::uint8_t*>(item.shader);
    const ShaderRegister* registers = nullptr;
    const std::uint8_t* specials = nullptr;
    std::memcpy(&registers, bytes + 0x20, sizeof(registers));
    std::memcpy(&specials, bytes + 0x28, sizeof(specials));
    const std::uint32_t registerCount = bytes[0x5c];
    std::uint32_t modifier = 0;
    std::memcpy(&modifier, specials + 0x10, sizeof(modifier));
    const auto userData = reinterpret_cast<std::uint64_t>(item.argument);
    const std::array<std::uint32_t, 2> userDataWords{static_cast<std::uint32_t>(userData), static_cast<std::uint32_t>(userData >> 32)};
    const auto capacity = std::min(commandWords - commandUsed, 64 + 3 * registerCount + item.name.size() / 4);
    auto* const words = commandBegin + commandUsed;
    CommandBuffer buffer{words, words + capacity, words, words + capacity, nullptr, nullptr, 0};
    {
        const GuestArena::HostWrite destination(words, capacity * 4);
        bool encoded = sceAgcAcbAcquireMem(&buffer, 0x20380u | (batchStarted ? 0u : 1u), nullptr, 0, 0x190) != nullptr;
        encoded = encoded && sceAgcCbSetShRegistersDirect(&buffer, registers, registerCount) != nullptr;
        encoded = encoded && sceAgcCbSetShRegisterRangeDirect(&buffer, UserDataRegister, userDataWords.data(), 2) != nullptr;
        if (item.named) encoded = encoded && sceAgcAcbPushMarker(&buffer, item.name.c_str(), 0) != nullptr;
        if (item.type == ItemType::Compute) {
            encoded = encoded && sceAgcCbDispatch(&buffer, item.groups[0], item.groups[1], item.groups[2], modifier) != nullptr;
        } else {
            encoded = encoded && sceAgcAcbDispatchIndirect(&buffer, item.indirectArguments, modifier) != nullptr;
        }
        if (item.named) encoded = encoded && sceAgcAcbPopMarker(&buffer) != nullptr;
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
    if (!TakeNode(*manager)) return Latch(SequenceNoNodes);
    items.push_back(std::move(item));
    Advance();
    return 0;
}

int Sequence::DispatchCompute(const void* shader, void* userData, std::int32_t x, std::int32_t y, std::int32_t z, const char* name) {
    if (shader == nullptr) return SequenceNullArgument;
    if (x <= 0 || y <= 0 || z <= 0) return SequenceOutOfRange;
    Item item{ItemType::Compute, this, nullptr, userData, 0, 0, 0, nullptr};
    item.shader = shader;
    item.groups = {static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), static_cast<std::uint32_t>(z)};
    item.named = name != nullptr;
    if (item.named) item.name = name;
    return AppendCompute(std::move(item));
}

int Sequence::DispatchComputeIndirect(const void* shader, void* userData, const void* indirectArguments, const char* name) {
    if (shader == nullptr || indirectArguments == nullptr || (reinterpret_cast<std::uint64_t>(indirectArguments) & 0x1f) != 0) return SequenceNullArgument;
    Item item{ItemType::ComputeIndirect, this, nullptr, userData, 0, 0, 0, nullptr};
    item.shader = shader;
    item.indirectArguments = indirectArguments;
    item.named = name != nullptr;
    if (item.named) item.name = name;
    return AppendCompute(std::move(item));
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
    const auto remaining = static_cast<std::uint32_t>(std::count_if(items.begin(), items.end(), [](const Item& item) { return item.type != ItemType::Wait; }));
    ReturnNodes(*manager, remaining + 1);
    items.clear();
    commandUsed = 0;
    cursor = 0;
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

template <typename TSequences>
int CreateIn(Manager* manager, TSequences& sequences, std::uint32_t& free, std::uint32_t priority, ISequence** out, const Counters& limits) {
    if (out == nullptr) return SequenceNullArgument;
    if (!ValidLimits(limits.invocations, limits.dispatches, limits.syncPoints, limits.waits)) return SequenceOutOfRange;
    const std::lock_guard guard(manager->lock);
    if (free == 0) return SequenceNoneFree;
    const int index = std::countr_zero(free);
    free &= ~(std::uint32_t{1} << index);
    Sequence& sequence = sequences[static_cast<std::size_t>(index)];
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
            if (&sequences[index] == sequence) break;
        }
        if (index == sequences.size() || (free & (std::uint32_t{1} << index)) != 0) return SequenceInvalid;
        found = &sequences[index];
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
        for (Sequence& sequence : manager.computeSequences) sequence.CollectCompute();
    }
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

struct WorkLayout {
    std::uint64_t commands = 0;
    std::uint64_t commandBytes = 0;
    std::uint64_t labels = 0;
    std::uint32_t nodes = 0;
};

WorkLayout LayoutFor(std::uint64_t memory, std::uint64_t size, std::uint32_t workers, std::uint32_t sequences, std::uint32_t computeQueues) {
    WorkLayout layout;
    const std::uint64_t end = memory + size;
    std::uint64_t address = AlignUp(memory + StateEnd, 8) + workers * ThreadRecordBytes;
    for (const std::uint64_t bytes : {std::uint64_t{0x100}, std::uint64_t{0x80}, std::uint64_t{0x80}, std::uint64_t{0x80}}) address = AlignUp(address, 64) + bytes;
    address = AlignUp(address, 64) + (sequences + computeQueues) * SequenceBytes;
    if (computeQueues != 0) {
        layout.commandBytes = ComputeCommandBudget();
        layout.commands = AlignUp(address + ComputeContextBytes, 0x100);
        layout.labels = AlignUp(layout.commands + layout.commandBytes * computeQueues + 4 * std::uint64_t{computeQueues}, 8);
        address = layout.labels + 8 * std::uint64_t{computeQueues};
    }
    address = AlignUp(address, 64);
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

void StartCompute(Manager& manager, const WorkLayout& layout, std::uint32_t computeSequences, std::uint64_t computeQueueMask, std::uint32_t priority, std::uint32_t affinity) {
    if (sceKernelCreateEqueue(&manager.computeEvents, "SceJobManagerComputeEventQueue") != 0) throw std::runtime_error("libSceJobManager: sceKernelCreateEqueue failed");
    for (std::uint64_t mask = computeQueueMask; mask != 0; mask &= mask - 1) {
        const std::uint32_t queue = FirstComputeQueue + static_cast<std::uint32_t>(std::countr_zero(mask));
        if (sceAgcDriverAddEqEvent(manager.computeEvents, static_cast<int>(queue), nullptr) != 0) throw std::runtime_error("libSceJobManager: sceAgcDriverAddEqEvent failed");
        manager.computeQueues.push_back(queue);
    }
    manager.computeSequences.resize(computeSequences);
    for (std::uint32_t index = 0; index < computeSequences; ++index) {
        manager.computeSequences[index].compute = true;
        manager.computeSequences[index].queue = manager.computeQueues[index];
        manager.computeSequences[index].SetComputeStorage(reinterpret_cast<std::uint32_t*>(layout.commands + index * layout.commandBytes),
            static_cast<std::size_t>(layout.commandBytes / 4), reinterpret_cast<Label*>(layout.labels + index * sizeof(Label)));
    }
    manager.freeComputeSequences = computeSequences == MaxSequences ? ~std::uint32_t{0} : (std::uint32_t{1} << computeSequences) - 1;
    const KernelSchedParam schedule{static_cast<int>(priority)};
    PthreadAttr attributes = nullptr;
    if (scePthreadAttrInit(&attributes) != 0 || scePthreadAttrSetinheritsched(&attributes, 0) != 0 || scePthreadAttrSetschedparam(&attributes, &schedule) != 0 ||
        scePthreadAttrSetstacksize(&attributes, 0x4000) != 0 || scePthreadAttrSetaffinity(&attributes, affinity) != 0) {
        throw std::runtime_error("libSceJobManager: a compute handler thread attribute was rejected");
    }
    const int created = scePthreadCreate(&manager.computeHandler, &attributes, ComputeHandlerEntry, &manager, "SceJobManagerComputeEventHdlr");
    scePthreadAttrDestroy(&attributes);
    if (created != 0) throw std::runtime_error("libSceJobManager: scePthreadCreate failed for the compute handler");
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
    auto* manager = new Manager;
    manager->running = true;
    manager->freeNodes = layout.nodes - 1;
    manager->freeSequences = sequences == MaxSequences ? ~std::uint32_t{0} : (std::uint32_t{1} << sequences) - 1;
    manager->sequences = std::vector<Sequence>(sequences);
    for (std::uint32_t priority = 0; priority < 2; ++priority) {
        manager->factories[priority].manager = manager;
        manager->factories[priority].priority = priority;
        manager->unifiedFactories[priority].manager = manager;
        manager->unifiedFactories[priority].priority = priority;
    }
    if (computeSequences != 0) StartCompute(*manager, layout, computeSequences, computeQueueMask, handlerPriority, handlerAffinity);
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
    {
        const std::lock_guard guard(manager->lock);
        if (manager->blockedWaiters != 0) throw std::runtime_error("sce::Job::JobManager::shutdown: a thread is still blocked in a sequence wait");
        for (const Sequence& sequence : manager->computeSequences) {
            if (sequence.open && sequence.Busy()) throw std::runtime_error("sce::Job::JobManager::shutdown: a compute sequence still has work in flight");
        }
        manager->running = false;
        manager->work.notify_all();
    }
    for (const Pthread worker : manager->workers) {
        if (scePthreadJoin(worker, nullptr) != 0) throw std::runtime_error("sce::Job::JobManager::shutdown: scePthreadJoin failed");
    }
    if (manager->computeHandler != nullptr) {
        if (sceKernelAddUserEventEdge(manager->computeEvents, 0) != 0 || sceKernelTriggerUserEvent(manager->computeEvents, 0, nullptr) != 0) {
            throw std::runtime_error("sce::Job::JobManager::shutdown: the compute handler could not be signalled");
        }
        if (scePthreadJoin(manager->computeHandler, nullptr) != 0) throw std::runtime_error("sce::Job::JobManager::shutdown: scePthreadJoin failed");
        for (const std::uint32_t queue : manager->computeQueues) static_cast<void>(sceAgcDriverDeleteEqEvent(manager->computeEvents, static_cast<int>(queue)));
        if (sceKernelDeleteEqueue(manager->computeEvents) != 0) throw std::runtime_error("sce::Job::JobManager::shutdown: sceKernelDeleteEqueue failed");
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
