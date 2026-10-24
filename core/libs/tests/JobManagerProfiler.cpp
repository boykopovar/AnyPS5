#include "SceTypes.hpp"
#include "prx/libSceAgcDriver/tests/execution/VulkanTestDevice.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <immintrin.h>
#include <mutex>
#include <thread>
#include <vector>

struct ConfigParams;
using JobEntry = void (APS5_VABI*)(void*, std::uint32_t);

struct ISequence {
    APS5_VABI virtual int Dispatch(JobEntry, void*, std::uint32_t, const char*) = 0;
    APS5_VABI virtual int SyncPoint() = 0;
    APS5_VABI virtual int Wait() = 0;
    APS5_VABI virtual void CompleteDestructor() = 0;
    APS5_VABI virtual void DeletingDestructor() = 0;
    APS5_VABI virtual int DispatchCompute(const void*, void*, std::int32_t, std::int32_t, std::int32_t, const char*) = 0;
};

struct ISequenceFactory {
    APS5_VABI virtual int Create(ISequence**, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, const char*) = 0;
    APS5_VABI virtual int Destroy(ISequence*) = 0;
    APS5_VABI virtual void CompleteDestructor() = 0;
    APS5_VABI virtual void DeletingDestructor() = 0;
    APS5_VABI virtual int CreateCompute(ISequence**, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, const char*) = 0;
    APS5_VABI virtual int DestroyCompute(ISequence*) = 0;
};

extern "C" {
void APS5_VABI _ZN3sce3Job10JobManagerC1Ev(void*);
int APS5_VABI _ZN3sce3Job10JobManager10initializeEPvmjjjmPKNS1_12ConfigParamsE(void*, void*, std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint64_t, const ConfigParams*);
int APS5_VABI _ZN3sce3Job10JobManager8shutdownEv(void*);
ISequenceFactory* APS5_VABI _ZN3sce3Job10JobManager34getUnifiedSequenceFactoryInterfaceENS1_8PriorityE(void*, std::uint32_t);
int APS5_VABI sceAgcDriverSubmitAcb(std::uint32_t, const Packet*);
std::uint32_t* APS5_VABI sceAgcAcbWaitRegMem(CommandBuffer*, std::uint8_t, std::uint8_t, std::uint8_t, const volatile void*, std::uint64_t, std::uint64_t, std::uint32_t);
void AgcDriverShutdown_nid_postfix();
}

namespace {

enum class Kind { Sequence, Job, Dispatch, Body };

struct Event {
    Kind kind;
    const char* name;
    std::uint16_t manager;
    std::uint16_t sequence;
    std::uint16_t dispatch;
    std::uint16_t value;
    std::uint16_t event;
    std::uint64_t entry;
    std::uint64_t argument;
};

std::mutex eventLock;
std::vector<Event> events;

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "job manager profiler check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

void Record(Event event) {
    const std::lock_guard guard(eventLock);
    events.push_back(event);
}

std::vector<Event> Snapshot() {
    const std::lock_guard guard(eventLock);
    return events;
}

void Clear() {
    const std::lock_guard guard(eventLock);
    events.clear();
}

struct Fixture {
    alignas(256) std::array<std::uint8_t, 0x100000> memory{};
    alignas(64) std::array<std::uint8_t, 0x100> object{};
    ISequenceFactory* factory = nullptr;
    std::uint16_t id = 0;

    explicit Fixture(std::uint32_t workers = 1) {
        _ZN3sce3Job10JobManagerC1Ev(object.data());
        Require(_ZN3sce3Job10JobManager10initializeEPvmjjjmPKNS1_12ConfigParamsE(object.data(), memory.data(), memory.size(), workers, 2, 2, 0x122, nullptr) == 0);
        factory = _ZN3sce3Job10JobManager34getUnifiedSequenceFactoryInterfaceENS1_8PriorityE(object.data(), 0);
        id = static_cast<std::uint16_t>(_mm_crc32_u64(0, reinterpret_cast<std::uintptr_t>(memory.data())) >> 16);
        Clear();
    }

    void Stop() {
        Require(_ZN3sce3Job10JobManager8shutdownEv(object.data()) == 0);
    }

    ISequence* Create(const char* name, bool compute = false) {
        ISequence* sequence = nullptr;
        if (compute) Require(factory->CreateCompute(&sequence, 16, 8, 8, 4, name) == 0);
        else Require(factory->Create(&sequence, 16, 8, 8, 4, name) == 0);
        return sequence;
    }
};

struct Context {
    std::atomic<bool> entered{false};
    std::atomic<std::uint32_t> calls{0};
    std::atomic<bool> released{true};
};

void APS5_VABI Job(void* argument, std::uint32_t index) {
    Record({Kind::Body, nullptr, 0, 0, 0, static_cast<std::uint16_t>(index), 0, 0, reinterpret_cast<std::uint64_t>(argument)});
    auto& context = *static_cast<Context*>(argument);
    ++context.calls;
    context.entered.store(true);
    while (!context.released.load()) std::this_thread::yield();
}

void Await(const std::atomic<bool>& flag) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!flag.load() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    Require(flag.load());
}

struct Reader {
    std::vector<Event> records = Snapshot();
    std::size_t next = 0;
    std::uint16_t manager;

    explicit Reader(std::uint16_t id) : manager(id) {}

    Event Take(Kind kind) {
        Require(next < records.size());
        const Event result = records[next++];
        Require(result.kind == kind);
        if (kind != Kind::Body) Require(result.manager == manager);
        return result;
    }

    void Sequence(const char* name, std::uint16_t id, std::uint16_t event) {
        const Event result = Take(Kind::Sequence);
        Require(result.name == name && result.sequence == id && result.event == event);
    }

    std::uint16_t Dispatch(const char* name, Context& context, std::uint16_t count, std::uint16_t sequence = 0) {
        std::uint16_t dispatch = 0;
        for (std::uint16_t index = 0; index < count; ++index) {
            const Event begin = Take(Kind::Job);
            if (index == 0) dispatch = begin.dispatch;
            Require(begin.dispatch == dispatch && begin.value == index && begin.event == 16);
            const Event body = Take(Kind::Body);
            Require(body.value == index && body.argument == reinterpret_cast<std::uint64_t>(&context));
            const Event end = Take(Kind::Job);
            Require(end.dispatch == dispatch && end.value == index && end.event == 32);
        }
        const Event result = Take(Kind::Dispatch);
        Require(result.name == name && result.sequence == sequence && result.dispatch == dispatch && result.value == count);
        Require(result.entry == reinterpret_cast<std::uint64_t>(&Job) && result.argument == reinterpret_cast<std::uint64_t>(&context));
        return dispatch;
    }

    void Done() const { Require(next == records.size()); }
};

void EmptyAndIdentityCase() {
    Fixture fixture;
    const char* cpuName = "cpu";
    const char* gpuName = "compute";
    ISequence* cpu0 = fixture.Create(cpuName);
    ISequence* cpu1 = fixture.Create(nullptr);
    ISequence* gpu0 = fixture.Create(gpuName, true);
    ISequence* gpu1 = fixture.Create(nullptr, true);
    ISequence* unused = nullptr;
    Require(fixture.factory->Create(&unused, 1, 1, 0, 1, "overflow") == static_cast<int>(0x81580006u));
    Require(fixture.factory->Create(nullptr, 1, 1, 0, 1, "null") == static_cast<int>(0x81580001u));
    for (ISequence* sequence : {cpu0, cpu1, gpu0, gpu1}) {
        Require(sequence->SyncPoint() == 0);
        Require(sequence->Wait() == 0);
    }
    Require(fixture.factory->Destroy(cpu0) == 0);
    Require(fixture.factory->Destroy(cpu1) == 0);
    Require(fixture.factory->DestroyCompute(gpu0) == 0);
    Require(fixture.factory->DestroyCompute(gpu1) == 0);
    ISequence* reused = fixture.Create(nullptr);
    Require(fixture.factory->Destroy(reused) == 0);
    fixture.Stop();
    Reader reader(fixture.id);
    reader.Sequence(cpuName, 0, 1);
    reader.Sequence(nullptr, 1, 1);
    reader.Sequence(gpuName, 0, 1);
    reader.Sequence(nullptr, 1, 1);
    reader.Sequence(cpuName, 0, 2);
    reader.Sequence(nullptr, 1, 2);
    reader.Sequence(gpuName, 0, 2);
    reader.Sequence(nullptr, 1, 2);
    reader.Sequence(nullptr, 0, 1);
    reader.Sequence(nullptr, 0, 2);
    reader.Done();
}

void BatchCase(bool sync, bool wait, bool compute = false) {
    Fixture fixture;
    const char* name = "sequence";
    const char* firstName = "first";
    const char* secondName = "second";
    ISequence* sequence = fixture.Create(name, compute);
    const std::uint16_t id = compute ? 2 : 0;
    Context first;
    Context second;
    first.released.store(false);
    Require(sequence->Dispatch(Job, &first, 1, firstName) == 0);
    Await(first.entered);
    if (sync) Require(sequence->SyncPoint() == 0);
    if (wait) {
        first.released.store(true);
        Require(sequence->Wait() == 0);
    }
    Require(sequence->Dispatch(Job, &second, 4, secondName) == 0);
    first.released.store(true);
    Require(sequence->Wait() == 0);
    if (compute) Require(fixture.factory->DestroyCompute(sequence) == 0);
    else Require(fixture.factory->Destroy(sequence) == 0);
    fixture.Stop();
    Reader reader(fixture.id);
    reader.Sequence(name, 0, 1);
    reader.Sequence(name, id, 3);
    const auto firstId = reader.Dispatch(firstName, first, 1, id);
    if (sync || wait) {
        reader.Sequence(name, id, 4);
        reader.Sequence(name, id, 3);
    }
    const auto secondId = reader.Dispatch(secondName, second, 4, id);
    if (!wait) Require(firstId != secondId);
    reader.Sequence(name, id, 4);
    reader.Sequence(name, 0, 2);
    reader.Done();
}

void ConcurrentCase() {
    Fixture fixture(4);
    ISequence* sequence = fixture.Create(nullptr);
    Context context;
    context.released.store(false);
    Require(sequence->Dispatch(Job, &context, 8, nullptr) == 0);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (context.calls.load() < 4 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    Require(context.calls.load() == 4);
    context.released.store(true);
    Require(sequence->Wait() == 0);
    Require(fixture.factory->Destroy(sequence) == 0);
    fixture.Stop();
    Reader reader(fixture.id);
    reader.Sequence(nullptr, 0, 1);
    reader.Sequence(nullptr, 0, 3);
    std::array<std::uint16_t, 8> states{};
    std::uint16_t dispatch = 0;
    for (std::size_t index = 0; index < states.size() * 3; ++index) {
        Require(reader.next < reader.records.size());
        const Event event = reader.records[reader.next++];
        Require(event.value < states.size());
        auto& state = states[event.value];
        if (event.kind == Kind::Job) {
            if (index == 0) dispatch = event.dispatch;
            Require(event.manager == fixture.id && event.dispatch == dispatch);
            Require((state == 0 && event.event == 16) || (state == 2 && event.event == 32));
        } else {
            Require(event.kind == Kind::Body && state == 1 && event.argument == reinterpret_cast<std::uint64_t>(&context));
        }
        ++state;
    }
    for (const auto state : states) Require(state == 3);
    const Event metadata = reader.Take(Kind::Dispatch);
    Require(metadata.dispatch == dispatch && metadata.value == 8 && metadata.sequence == 0 && metadata.name == nullptr);
    Require(metadata.entry == reinterpret_cast<std::uint64_t>(&Job) && metadata.argument == reinterpret_cast<std::uint64_t>(&context));
    reader.Sequence(nullptr, 0, 4);
    reader.Sequence(nullptr, 0, 2);
    reader.Done();
}

void GpuBarrierCase(bool mixed, bool cpuAfter) {
    Fixture fixture;
    ISequence* sequence = fixture.Create(nullptr, true);
    alignas(256) static const std::array<std::uint32_t, 1> code{0xbf810000u};
    const auto address = reinterpret_cast<std::uint64_t>(code.data());
    const std::array<ShaderRegister, 7> registers{{{0x20c, static_cast<std::uint32_t>(address >> 8)}, {0x20d, static_cast<std::uint32_t>(address >> 40) & 0xff}, {0x207, 1}, {0x208, 1}, {0x209, 1}, {0x212, 0}, {0x213, 0}}};
    const std::array<std::uint32_t, 8> specials{0, 0, 0, 0, 1, 0, 0, 0};
    const ShaderRegister* registerPointer = registers.data();
    const std::uint32_t* specialPointer = specials.data();
    std::array<std::uint8_t, 0x80> shader{};
    std::memcpy(shader.data() + 0x20, &registerPointer, sizeof(registerPointer));
    std::memcpy(shader.data() + 0x28, &specialPointer, sizeof(specialPointer));
    shader[0x5c] = static_cast<std::uint8_t>(registers.size());
    alignas(64) std::uint32_t gate = 0;
    alignas(256) std::array<std::uint32_t, 32> commands{};
    CommandBuffer buffer{commands.data(), commands.data() + commands.size(), commands.data(), commands.data() + commands.size(), nullptr, nullptr, 0};
    Require(sceAgcAcbWaitRegMem(&buffer, 0, 3, 3, &gate, 1, 0xffffffffu, 0x190) != nullptr);
    const Packet packet{commands.data(), static_cast<std::uint32_t>(buffer.cursor_up - commands.data()), 0, {}};
    Require(sceAgcDriverSubmitAcb(0x21, &packet) == 0);
    Context context;
    if (mixed) {
        context.released.store(false);
        Require(sequence->Dispatch(Job, &context, 1, nullptr) == 0);
        Await(context.entered);
    }
    Require(sequence->DispatchCompute(shader.data(), nullptr, 1, 1, 1, nullptr) == 0);
    Require(sequence->SyncPoint() == 0);
    if (cpuAfter) {
        Require(sequence->Dispatch(Job, &context, 1, nullptr) == 0);
        Require(sequence->SyncPoint() == 0);
    }
    Require(sequence->DispatchCompute(shader.data(), nullptr, 1, 1, 1, nullptr) == 0);
    context.released.store(true);
    std::atomic_ref<std::uint32_t>(gate).store(1);
    Require(sequence->Wait() == 0);
    Require(fixture.factory->DestroyCompute(sequence) == 0);
    fixture.Stop();
    Reader reader(fixture.id);
    reader.Sequence(nullptr, 0, 1);
    reader.Sequence(nullptr, 2, 3);
    if (cpuAfter) {
        reader.Sequence(nullptr, 2, 4);
        reader.Sequence(nullptr, 2, 3);
    }
    if (mixed || cpuAfter) {
        reader.Dispatch(nullptr, context, 1, 2);
        reader.Sequence(nullptr, 2, 4);
        reader.Sequence(nullptr, 2, 3);
    }
    reader.Sequence(nullptr, 2, 4);
    reader.Sequence(nullptr, 0, 2);
    reader.Done();
}

void ShutdownCase(bool queued) {
    Fixture fixture;
    ISequence* sequence = fixture.Create(nullptr);
    Context first;
    Context canceled;
    first.released.store(false);
    Require(sequence->Dispatch(Job, &first, 1, nullptr) == 0);
    Await(first.entered);
    if (queued) {
        Require(sequence->SyncPoint() == 0);
        Require(sequence->Dispatch(Job, &canceled, 1, "canceled") == 0);
    }
    std::atomic<bool> stopping{false};
    std::thread stopper([&] {
        stopping.store(true);
        fixture.Stop();
    });
    Await(stopping);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    first.released.store(true);
    stopper.join();
    Require(!canceled.entered.load());
    Reader reader(fixture.id);
    reader.Sequence(nullptr, 0, 1);
    reader.Sequence(nullptr, 0, 3);
    reader.Dispatch(nullptr, first, 1);
    if (queued) {
        reader.Sequence(nullptr, 0, 4);
        reader.Sequence(nullptr, 0, 3);
    }
    reader.Done();
}

}

extern "C" {

int APS5_VABI sceRazorCpuJobManagerSequence(const char* name, std::uint16_t manager, std::uint16_t sequence, std::uint16_t event) {
    Record({Kind::Sequence, name, manager, sequence, 0, 0, event, 0, 0});
    return -1;
}

int APS5_VABI sceRazorCpuJobManagerJob(std::uint16_t manager, std::uint16_t dispatch, std::uint16_t index, std::uint16_t event) {
    Record({Kind::Job, nullptr, manager, 0, dispatch, index, event, 0, 0});
    return -1;
}

int APS5_VABI sceRazorCpuJobManagerDispatch(const char* name, std::uint16_t manager, std::uint16_t sequence, std::uint16_t dispatch, std::uint16_t count, std::uint64_t entry, std::uint64_t argument) {
    Record({Kind::Dispatch, name, manager, sequence, dispatch, count, 0, entry, argument});
    return -1;
}

}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "gpu") == 0) {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        device.reset();
        GpuBarrierCase(false, false);
        GpuBarrierCase(false, true);
        GpuBarrierCase(true, false);
        AgcDriverShutdown_nid_postfix();
        std::puts("job manager GPU profiler tests passed");
        return 0;
    }
    if (argc != 1) return 2;
    EmptyAndIdentityCase();
    BatchCase(false, false);
    BatchCase(true, false);
    BatchCase(false, true);
    BatchCase(true, false, true);
    ConcurrentCase();
    ShutdownCase(false);
    ShutdownCase(true);
    std::puts("job manager profiler tests passed");
}
