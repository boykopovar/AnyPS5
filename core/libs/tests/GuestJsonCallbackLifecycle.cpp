#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>

struct Value { Value* parent; void* auxiliary; std::uint64_t payload; std::uint32_t reserved; std::int32_t type; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
using FloatHandler = Value* (APS5_VABI*)(Value*, std::int32_t, double, const Value*, void*, bool*);
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueC1El(Value*, std::int64_t);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
void APS5_VABI _ZN3sce4Json5Value3setEd(Value*, double);
int APS5_VABI _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(Value*, FloatHandler, void*);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value*, const char*, std::size_t);
void* APS5_VABI _ZN3sce4Json6MallocEm(std::size_t);
}
static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
enum class NoticeAction { None, Terminate, RejectTermination };
struct LifecycleTrace {
    std::map<void*, std::size_t> live;
    std::size_t generation = 0;
    bool failNext = false;
    std::uint8_t* owner = nullptr;
    NoticeAction noticeAction = NoticeAction::None;
    unsigned notices = 0;
    unsigned refused = 0;
};
static void* APS5_VABI AllocateLifecycle(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<LifecycleTrace*>(context);
    if (trace.failNext) { trace.failNext = false; return nullptr; }
    void* memory = std::malloc(bytes);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes);
    Require(trace.live.emplace(memory, ++trace.generation).second, "Lifecycle resources must have independently live allocation generations");
    return memory;
}
static void APS5_VABI FreeLifecycle(void*, void* memory, void* context) {
    if (!memory) return;
    Require(static_cast<LifecycleTrace*>(context)->live.erase(memory) == 1, "Lifecycle cleanup must release each allocation through its owner exactly once");
    std::free(memory);
}
static void RejectOwnerTermination(LifecycleTrace& trace) {
    const auto before = trace.live;
    bool rejected = false;
    try { _ZN3sce4Json11Initializer9terminateEv(trace.owner); }
    catch (const std::runtime_error& error) {
        Require(std::strstr(error.what(), "terminate"), "Unsafe callback termination must use the explicit unsupported boundary");
        rejected = true;
    }
    Require(rejected, "Termination inside an active callback operation must be explicitly rejected before teardown");
    Require(*trace.owner == 1 && trace.live == before, "Rejected callback termination must preserve the entire module allocation generation and active byte");
    ++trace.refused;
}
static void APS5_VABI NotifyLifecycle(void*, int error, std::size_t bytes, void* context) {
    auto& trace = *static_cast<LifecycleTrace*>(context);
    Require(static_cast<unsigned>(error) == 0x80848102 && bytes == (trace.noticeAction == NoticeAction::Terminate ? 8u : 72u), "Lifecycle notifier must receive the actual failed allocation size and module error");
    ++trace.notices;
    if (trace.noticeAction == NoticeAction::Terminate) {
        try {
            Require(_ZN3sce4Json11Initializer9terminateEv(trace.owner) == 0 && *trace.owner == 0 && trace.live.empty(), "A standalone Malloc notifier must retain reference-supported module termination");
        } catch (const std::runtime_error&) { Require(false, "Reference-supported standalone notifier termination must not be rejected"); }
    } else if (trace.noticeAction == NoticeAction::RejectTermination) RejectOwnerTermination(trace);
    else Require(false, "Healthy lifecycle operations must not unexpectedly fail allocation");
}
static void* const LifecycleVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateLifecycle), reinterpret_cast<void*>(FreeLifecycle), reinterpret_cast<void*>(NotifyLifecycle)};
struct LifecycleAllocator { void* const* vtable = LifecycleVtable; };
struct RecoveryContext { LifecycleTrace* trace; bool terminateOwner; unsigned calls = 0; };
static Value* APS5_VABI RecoverLifecycle(Value* result, std::int32_t function, double number, const Value* parent, void* context, bool* error) {
    auto& state = *static_cast<RecoveryContext*>(context);
    Require(function == 1 && number == std::numeric_limits<double>::infinity() && !parent && !error, "Lifecycle recovery must preserve its assignment callback ABI");
    ++state.calls;
    std::uint8_t inactive = 0;
    Require(static_cast<unsigned>(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&inactive, nullptr)) == 0x80848111 && inactive == 0, "Recovery reentry must reject a competing initializer before parameter validation");
    Require(static_cast<unsigned>(_ZN3sce4Json11Initializer9terminateEv(&inactive)) == 0x80848110 && *state.trace->owner == 1, "Inactive termination must preserve active recovery and module ownership");
    if (state.terminateOwner) RejectOwnerTermination(*state.trace);
    _ZN3sce4Json5ValueC1El(result, 7);
    return result;
}
static void CheckRecovery(bool terminateOwner) {
    LifecycleTrace trace; LifecycleAllocator allocator; Parameter parameter{&allocator, &trace, 8192, 2, {}};
    std::uint8_t initializer = 0; trace.owner = &initializer;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize recovery lifecycle contracts");
    Value value{}; _ZN3sce4Json5ValueC1Ev(&value);
    RecoveryContext context{&trace, terminateOwner};
    Require(_ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(&value, RecoverLifecycle, &context) == 0, "Register lifecycle recovery");
    const auto baseline = trace.live;
    void* auxiliary = value.auxiliary;
    for (unsigned attempt = 1; attempt <= 2; ++attempt) {
        _ZN3sce4Json5Value3setEd(&value, std::numeric_limits<double>::infinity());
        Require(value.type == 2 && value.payload == 7 && value.auxiliary == auxiliary && context.calls == attempt && trace.refused == (terminateOwner ? attempt : 0) && initializer == 1 && trace.live == baseline,
                "Handled callback lifecycle operations must preserve recovery metadata, repeatability and complete module ownership");
    }
    _ZN3sce4Json5ValueD1Ev(&value);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && initializer == 0 && trace.live.empty(), "Ordinary termination must work after recovery callback reentry");
}
static void CheckNotice(bool activeOperation) {
    LifecycleTrace trace; LifecycleAllocator allocator; Parameter parameter{&allocator, &trace, 8192, 2, {}};
    std::uint8_t initializer = 0; trace.owner = &initializer;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize notifier lifecycle contracts");
    const auto baseline = trace.live;
    trace.noticeAction = activeOperation ? NoticeAction::RejectTermination : NoticeAction::Terminate;
    trace.failNext = true;
    if (activeOperation) {
        Value value{}; _ZN3sce4Json5ValueC1Ev(&value);
        Require(static_cast<unsigned>(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, "true", 4)) == 0x80848102 && value.type == 1 && (value.payload & 0xff) == 1 && !value.auxiliary && initializer == 1 && trace.live == baseline,
                "Rejected notifier termination must retain normal parser out-of-memory behavior and module ownership");
        Require(trace.refused == 1 && trace.notices == 1, "Active notifier termination must be rejected exactly once at the actual failure boundary");
        trace.noticeAction = NoticeAction::None;
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, "true", 4) == 0 && value.auxiliary && initializer == 1, "Parser retry must work after a caught notifier lifecycle error");
        _ZN3sce4Json5ValueD1Ev(&value);
        Require(trace.live == baseline && _ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Ordinary teardown must reclaim resources after notifier refusal and retry");
    } else {
        Require(!_ZN3sce4Json6MallocEm(8) && initializer == 0 && trace.live.empty() && trace.notices == 1 && !trace.refused,
                "Standalone allocation failure must preserve successful notifier termination and its null allocation result");
    }
    Require(initializer == 0 && trace.live.empty(), "Notifier lifecycle cases must fully release the original module generation");
    const auto generation = trace.generation;
    trace.noticeAction = NoticeAction::None;
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0 && initializer == 1 && trace.live.size() == 12, "The same initializer must admit a fresh module generation after notifier-driven termination or refusal");
    for (const auto& [memory, identity] : trace.live) Require(memory && identity > generation, "Lifecycle retry must never retain stale allocation identities");
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && initializer == 0 && trace.live.empty(), "Release retried notifier lifecycle generation");
}
int main() {
    CheckRecovery(false);
    CheckRecovery(true);
    CheckNotice(false);
    CheckNotice(true);
    return 0;
}
