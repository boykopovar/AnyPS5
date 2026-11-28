#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <new>
#include <stdexcept>

using Jsc::JscAbi::ArrayBuffer;
using Jsc::JscAbi::ArrayBufferContents;
using Jsc::JscAbi::SharedArrayBufferContents;
using Jsc::JscAbi::SharedTask;
using Jsc::JscAbi::SharedTaskVtable;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JSC ArrayBufferContents check failed at line %d\n", line);
        std::abort();
    }
}

namespace AbcDriver {

constexpr int Slots = 4;
constexpr int Tasks = 4;
constexpr int Shareds = 2;

struct Hash {
    std::uint64_t value = 1469598103934665603ull;
    void Mix(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            value ^= (v >> (8 * i)) & 0xff;
            value *= 1099511628211ull;
        }
    }
};

inline Hash* events = nullptr;

inline void RunEvent(int task, const void* data) {
    events->Mix(1);
    events->Mix(static_cast<std::uint64_t>(task));
    events->Mix(reinterpret_cast<std::uintptr_t>(data));
}

inline void DeleteEvent(int task) {
    events->Mix(2);
    events->Mix(static_cast<std::uint64_t>(task));
}

struct Snapshot {
    std::uintptr_t data;
    unsigned size;
    int task;
    int shared;
};

template<typename Adapter> std::uint64_t Run(Adapter& adapter, std::uint32_t seed, int steps) {
    Hash hash;
    events = &hash;
    bool alive[Slots] = {};
    std::uint32_t state = seed ? seed : 1;
    auto random = [&](std::uint32_t bound) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<int>(state % bound);
    };
    auto mixState = [&] {
        for (int i = 0; i < Slots; ++i) {
            if (!alive[i]) continue;
            Snapshot s = adapter.Get(i);
            hash.Mix(s.data);
            hash.Mix(s.size);
            hash.Mix(static_cast<std::uint64_t>(s.task));
            hash.Mix(static_cast<std::uint64_t>(s.shared));
        }
        for (int t = 0; t < Tasks; ++t) hash.Mix(adapter.TaskRefs(t));
        for (int s = 0; s < Shareds; ++s) hash.Mix(adapter.SharedRefs(s));
    };
    for (int step = 0; step < steps; ++step) {
        int op = random(11);
        int i = random(Slots);
        int j = random(Slots);
        int variant = random(2);
        hash.Mix(static_cast<std::uint64_t>(op * 64 + i * 8 + j * 2 + variant));
        switch (op) {
        case 0:
            if (!alive[i]) {
                adapter.Construct(i, variant);
                alive[i] = true;
            }
            break;
        case 1:
            if (!alive[i] && alive[j]) {
                adapter.MoveConstruct(i, j, variant);
                alive[i] = true;
            }
            break;
        case 2:
            if (alive[i] && alive[j]) adapter.Assign(i, j);
            break;
        case 3:
            if (alive[i]) adapter.Clear(i);
            break;
        case 4:
            if (alive[i]) {
                adapter.Destruct(i, variant);
                alive[i] = false;
            }
            break;
        case 5:
        case 6:
            if (alive[i]) {
                Snapshot s = adapter.Get(i);
                if (!s.data && s.task < 0 && s.shared < 0) {
                    int task = random(Tasks + 1) - 1;
                    if (task >= 0 && !adapter.TaskRefs(task)) task = -1;
                    int shared = random(Shareds + 1) - 1;
                    if (shared >= 0 && !adapter.SharedRefs(shared)) shared = -1;
                    std::uintptr_t data = 0x10000u * static_cast<std::uintptr_t>(step + 1);
                    adapter.Seed(i, data, static_cast<unsigned>(random(4096)), task, shared);
                }
            }
            break;
        case 7:
            adapter.ReleasePool(random(Tasks));
            break;
        case 8:
            adapter.Revive(random(Tasks));
            break;
        case 9:
            adapter.ReleaseSharedPool(random(Shareds));
            break;
        case 10:
            adapter.ReviveShared(random(Shareds), 0x7000000u + 0x100u * static_cast<std::uintptr_t>(step + 1));
            break;
        }
        mixState();
    }
    for (int i = 0; i < Slots; ++i) {
        if (alive[i]) adapter.Destruct(i, 0);
        alive[i] = false;
    }
    for (int t = 0; t < Tasks; ++t) adapter.ReleasePool(t);
    for (int s = 0; s < Shareds; ++s) adapter.ReleaseSharedPool(s);
    mixState();
    events = nullptr;
    return hash.value;
}

} // namespace AbcDriver

struct GuestTask {
    SharedTask base;
    int id;
    bool deleted;
};

std::uint64_t badDeleteRefs = 0;

void APS5_VABI TaskCompleteDestructor(void*) {
    std::abort();
}

void APS5_VABI TaskDeletingDestructor(void* self) {
    auto* task = static_cast<GuestTask*>(self);
    if (task->base.refCount != 1) ++badDeleteRefs;
    AbcDriver::DeleteEvent(task->id);
    task->deleted = true;
}

void APS5_VABI TaskRun(void* self, void* data) {
    AbcDriver::RunEvent(static_cast<GuestTask*>(self)->id, data);
}

const SharedTaskVtable TaskVtable{TaskCompleteDestructor, TaskDeletingDestructor, TaskRun};

template<typename T> T* LoadPacked(const std::array<std::uint8_t, 6>& storage) {
    std::uintptr_t value = 0;
    std::memcpy(&value, storage.data(), storage.size());
    return reinterpret_cast<T*>(value);
}

void StorePacked(std::array<std::uint8_t, 6>& storage, const void* pointer) {
    const auto value = reinterpret_cast<std::uintptr_t>(pointer);
    Check(!(value >> 48), __LINE__);
    std::memcpy(storage.data(), &value, storage.size());
}

struct Guest {
    ArrayBufferContents slots[AbcDriver::Slots];
    std::array<GuestTask, (AbcDriver::Tasks + AbcDriver::Shareds) * 400> taskStorage{};
    std::size_t nextTask = 0;
    GuestTask* tasks[AbcDriver::Tasks] = {};
    bool pool[AbcDriver::Tasks] = {};
    SharedArrayBufferContents* shared[AbcDriver::Shareds] = {};
    GuestTask* sharedTasks[AbcDriver::Shareds] = {};
    bool sharedPool[AbcDriver::Shareds] = {};

    Guest() {
        std::memset(slots, 0xa5, sizeof(slots));
        for (int t = 0; t < AbcDriver::Tasks; ++t) Revive(t);
        for (int s = 0; s < AbcDriver::Shareds; ++s) ReviveShared(s, std::uintptr_t{0x7000000} + s * 0x10);
    }
    GuestTask* NewTask(int id) {
        Check(nextTask < taskStorage.size(), __LINE__);
        GuestTask* task = &taskStorage[nextTask++];
        task->base.vptr = &TaskVtable;
        task->base.refCount = 1;
        task->id = id;
        task->deleted = false;
        return task;
    }
    void Construct(int i, int variant) {
        (variant ? _ZN3JSC19ArrayBufferContentsC2Ev : _ZN3JSC19ArrayBufferContentsC1Ev)(&slots[i]);
    }
    void MoveConstruct(int i, int j, int variant) {
        std::memset(&slots[i], 0x5a, sizeof(slots[i]));
        (variant ? _ZN3JSC19ArrayBufferContentsC2EOS0_ : _ZN3JSC19ArrayBufferContentsC1EOS0_)(&slots[i], &slots[j]);
    }
    void Assign(int i, int j) {
        Check(_ZN3JSC19ArrayBufferContentsaSEOS0_(&slots[i], &slots[j]) == &slots[i], __LINE__);
    }
    void Clear(int i) {
        _ZN3JSC19ArrayBufferContents5clearEv(&slots[i]);
    }
    void Destruct(int i, int variant) {
        (variant ? _ZN3JSC19ArrayBufferContentsD2Ev : _ZN3JSC19ArrayBufferContentsD1Ev)(&slots[i]);
        std::memset(&slots[i], 0xa5, sizeof(slots[i]));
    }
    void Seed(int i, std::uintptr_t data, unsigned size, int task, int sh) {
        auto& c = slots[i];
        c.data = reinterpret_cast<void*>(data);
        c.sizeInBytes = size;
        if (task >= 0) {
            ++tasks[task]->base.refCount;
            StorePacked(c.destructor, tasks[task]);
        }
        if (sh >= 0) {
            ++shared[sh]->refCount;
            StorePacked(c.shared, shared[sh]);
        }
    }
    bool SharedAlive(int s) { return sharedTasks[s] && !sharedTasks[s]->deleted; }
    int TaskId(GuestTask* task) {
        for (int k = 0; k < AbcDriver::Tasks; ++k)
            if (!tasks[k]->deleted && tasks[k] == task) return k;
        return task ? 99 : -1;
    }
    int SharedId(SharedArrayBufferContents* p) {
        for (int k = 0; k < AbcDriver::Shareds; ++k)
            if (SharedAlive(k) && shared[k] == p) return k;
        return p ? 99 : -1;
    }
    AbcDriver::Snapshot Get(int i) {
        auto& c = slots[i];
        return {reinterpret_cast<std::uintptr_t>(c.data), c.sizeInBytes, TaskId(LoadPacked<GuestTask>(c.destructor)),
            SharedId(LoadPacked<SharedArrayBufferContents>(c.shared))};
    }
    std::uint64_t TaskRefs(int t) { return tasks[t]->deleted ? 0 : tasks[t]->base.refCount.load(); }
    std::uint64_t SharedRefs(int s) { return SharedAlive(s) ? shared[s]->refCount.load() : 0; }
    void ReleasePool(int t) {
        if (!pool[t]) return;
        pool[t] = false;
        GuestTask* task = tasks[t];
        if (--task->base.refCount) return;
        task->base.refCount = 1;
        TaskDeletingDestructor(task);
    }
    void Revive(int t) {
        if (tasks[t] && !tasks[t]->deleted) return;
        tasks[t] = NewTask(t);
        pool[t] = true;
    }
    void ReleaseSharedPool(int s) {
        if (!sharedPool[s]) return;
        sharedPool[s] = false;
        ArrayBufferContents holder{};
        StorePacked(holder.shared, shared[s]);
        _ZN3JSC19ArrayBufferContentsD1Ev(&holder);
    }
    void ReviveShared(int s, std::uintptr_t data) {
        if (SharedAlive(s)) return;
        void* memory = malloc_nid_postfix(sizeof(SharedArrayBufferContents));
        Check(memory != nullptr, __LINE__);
        auto* contents = new (memory) SharedArrayBufferContents;
        contents->refCount = 1;
        contents->data = reinterpret_cast<void*>(data);
        contents->destructor = {};
        contents->sizeInBytes = 64;
        sharedTasks[s] = NewTask(AbcDriver::Tasks + s);
        StorePacked(contents->destructor, sharedTasks[s]);
        shared[s] = contents;
        sharedPool[s] = true;
    }
};

constexpr std::uint64_t ExpectedHashes[] = {
    0xb610fb09130558beull,
    0x7c5628a3d6d297f9ull,
    0x305e6d0086da32efull,
    0x347d0d16d384b5b1ull,
    0x17ec69d7d213aa25ull,
    0xdcda6f64b966c253ull,
    0x62be771bda0076b8ull,
    0xdb2a08c8bca08b7full,
    0x60673caf99865526ull,
    0x99cf4fb748866803ull,
    0x16283142ac2a9a5bull,
    0x9a7f4fb6e4d29f86ull,
    0x364fd1935c82d31cull,
    0x1755fcc4f6ee7db8ull,
    0xcccc06979204a7c9ull,
    0x66a159175045f5b6ull,
    0x85f001b4452ed3b1ull,
    0x2a970c121762e6c4ull,
    0x11607b20d9278da4ull,
    0xe2b75b9c89a71455ull,
    0x14122bc3c331420eull,
    0xceda928195f9e257ull,
    0x1044a5dda1c9eb12ull,
    0x7810077912ff93e9ull,
};

void TestTranscripts() {
    for (std::uint32_t seed = 1; seed <= std::size(ExpectedHashes); ++seed) {
        auto* guest = new Guest;
        const std::uint64_t hash = AbcDriver::Run(*guest, seed, 600);
        if (hash != ExpectedHashes[seed - 1]) std::fprintf(stderr, "seed %u: 0x%016llx\n", seed, static_cast<unsigned long long>(hash));
        Check(hash == ExpectedHashes[seed - 1], __LINE__);
        delete guest;
    }
    Check(badDeleteRefs == 0, __LINE__);
}

template<typename Function> bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestNullObjects() {
    ArrayBufferContents contents{};
    Check(Throws([] { _ZN3JSC19ArrayBufferContentsC1Ev(nullptr); }), __LINE__);
    Check(Throws([] { _ZN3JSC19ArrayBufferContentsC2Ev(nullptr); }), __LINE__);
    Check(Throws([] { _ZN3JSC19ArrayBufferContentsD1Ev(nullptr); }), __LINE__);
    Check(Throws([] { _ZN3JSC19ArrayBufferContentsD2Ev(nullptr); }), __LINE__);
    Check(Throws([] { _ZN3JSC19ArrayBufferContents5clearEv(nullptr); }), __LINE__);
    Check(Throws([&] { _ZN3JSC19ArrayBufferContentsaSEOS0_(&contents, nullptr); }), __LINE__);
    Check(Throws([&] { _ZN3JSC19ArrayBufferContentsaSEOS0_(nullptr, &contents); }), __LINE__);
    Check(Throws([&] { _ZN3JSC19ArrayBufferContentsC1EOS0_(&contents, nullptr); }), __LINE__);
    Check(Throws([&] { _ZN3JSC19ArrayBufferContentsC2EOS0_(&contents, nullptr); }), __LINE__);
}

bool SameText(const char* actual, const char* expected) {
    return actual && std::strcmp(actual, expected) == 0;
}

void TestTransferErrors() {
    const char* shared = "Cannot transfer a SharedArrayBuffer";
    const char* wasm = "Cannot transfer a WebAssembly.Memory";
    const char* api = "Cannot transfer an ArrayBuffer whose backing store has been accessed by the JavaScriptCore C API";
    SharedArrayBufferContents sharedContents{};
    ArrayBuffer buffer{};
    buffer.locked = true;
    Check(SameText(_ZN3JSC23errorMesasgeForTransferEPNS_11ArrayBufferE(&buffer), api), __LINE__);
    buffer.isWasmMemory = true;
    Check(SameText(_ZN3JSC23errorMesasgeForTransferEPNS_11ArrayBufferE(&buffer), wasm), __LINE__);
    StorePacked(buffer.contents.shared, &sharedContents);
    Check(SameText(_ZN3JSC23errorMesasgeForTransferEPNS_11ArrayBufferE(&buffer), shared), __LINE__);
    buffer.isWasmMemory = false;
    Check(SameText(_ZN3JSC23errorMesasgeForTransferEPNS_11ArrayBufferE(&buffer), shared), __LINE__);
    StorePacked(buffer.contents.shared, nullptr);
    buffer.pinCount = 0xffffffffu;
    buffer.contents.data = &sharedContents;
    buffer.contents.sizeInBytes = 0xffffffffu;
    Check(SameText(_ZN3JSC23errorMesasgeForTransferEPNS_11ArrayBufferE(&buffer), api), __LINE__);
    buffer.locked = false;
    Check(SameText(_ZN3JSC23errorMesasgeForTransferEPNS_11ArrayBufferE(&buffer), api), __LINE__);
    Check(Throws([] { _ZN3JSC23errorMesasgeForTransferEPNS_11ArrayBufferE(nullptr); }), __LINE__);
    Check(Throws([] { _ZN3JSC23errorMesasgeForTransferEPNS_11ArrayBufferE(reinterpret_cast<ArrayBuffer*>(0x10)); }), __LINE__);
}

} // namespace

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    TestTranscripts();
    TestNullObjects();
    TestTransferErrors();
    std::puts("JSC ArrayBufferContents tests passed");
    return 0;
}
