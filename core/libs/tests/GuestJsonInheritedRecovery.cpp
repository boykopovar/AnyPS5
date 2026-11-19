#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include <limits>
#include <bit>
#include <string>

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
static_assert(sizeof(Value) == 32 && sizeof(Parameter) == 40);
using ReadCallback = int (APS5_VABI*)(char&, void*);
using FloatCallback = Value* (APS5_VABI*)(Value*, std::int32_t, double, const Value*, void*, bool*);
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct RecoveryAllocationTrace {
    std::map<void*, std::size_t> live;
    std::vector<std::size_t> requests;
    int budget = -1;
    bool once = false;
    unsigned notifications = 0;
    std::map<void*, std::size_t> ids;
    std::size_t nextId = 0;
    void (*onNotify)(RecoveryAllocationTrace&) = nullptr;
    void* notificationContext = nullptr;
    bool notifying = false;
};
static void* APS5_VABI AllocateRecovery(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<RecoveryAllocationTrace*>(context); trace.requests.push_back(bytes);
    if (trace.budget == 0) { if (trace.once) trace.budget = -1; return nullptr; }
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(bytes);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes); trace.live.emplace(memory, bytes); trace.ids[memory] = ++trace.nextId; return memory;
}
static void APS5_VABI FreeRecovery(void*, void* memory, void* context) {
    if (!memory) return;
    auto& trace = *static_cast<RecoveryAllocationTrace*>(context);
    Require(trace.ids.count(memory) == 1, "Freed recovery storage must identify a currently live allocation generation");
    trace.ids.erase(memory);
    Require(trace.live.erase(memory) == 1, "Recovery storage must release through its owning allocator"); std::free(memory);
}
static void APS5_VABI NotifyRecovery(void*, int error, std::size_t bytes, void* context) {
    auto& trace = *static_cast<RecoveryAllocationTrace*>(context);
    Require(static_cast<unsigned>(error) == 0x80848102 && trace.requests.back() == bytes, "Recovery notification must identify the failed allocation"); ++trace.notifications;
    if (trace.onNotify && !trace.notifying) {
        trace.notifying = true;
        trace.onNotify(trace);
        trace.notifying = false;
    }
}
static void* const RecoveryVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateRecovery), reinterpret_cast<void*>(FreeRecovery), reinterpret_cast<void*>(NotifyRecovery)};
struct RecoveryAllocator { void* const* vtable = RecoveryVtable; };
struct RecoveryContext { const Value* replacement; bool object; unsigned calls = 0; int function = -1; bool errorPointer = false; };
static Value* APS5_VABI RecoverAllocated(Value* result, std::int32_t function, double number, const Value* parent, void* context, bool* error) {
    auto& trace = *static_cast<RecoveryContext*>(context);
    ++trace.calls; trace.function = function; trace.errorPointer = error != nullptr;
    Require(number == std::numeric_limits<double>::infinity() && !parent, "Recovery must receive the original non-finite value and immediate parent");
    if (trace.object) _ZN3sce4Json5ValueC1ERKS1_(result, trace.replacement);
    else _ZN3sce4Json5ValueC1EPKc(result, "recovery");
    if (error) *error = false;
    return result;
}
static std::string Contents(Value& value) {
    if (value.type == 0) return "null";
    if (value.type == 4) { Require(value.payload == std::bit_cast<std::uint64_t>(std::numeric_limits<double>::infinity()), "Serialization recovery must preserve the source real payload bits"); return "inf"; }
    if (value.type == 5) return _ZNK3sce4Json6String5c_strEv(reinterpret_cast<const String*>(value.payload));
    String output{}; _ZN3sce4Json6StringC1EPKc(&output, "");
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &output) == 0, "Partial callback-return objects must remain serializable");
    std::string result = _ZNK3sce4Json6String5c_strEv(&output);
    _ZN3sce4Json6StringD1Ev(&output);
    return result;
}
struct ReadContext {
    RecoveryAllocationTrace* allocations;
    bool fail;
    Value* assignment;
    Value* format;
    Value* nested;
    int nestedStatus = 0;
    int formatStatus = 0;
    String* output;
    unsigned reads = 0;
    unsigned nestedReads = 0;
    unsigned hooks = 0;
};
static int APS5_VABI ReadTrue(char& character, void* data) {
    auto& position = *static_cast<unsigned*>(data);
    if (position >= 4) return static_cast<int>(0x80848101);
    character = "true"[position++];
    return 0;
}
static int APS5_VABI ReadNested(char& character, void* data) {
    auto& context = *static_cast<ReadContext*>(data);
    if (!context.reads++) {
        if (context.fail) {
            String failed{};
            context.allocations->budget = 0; context.allocations->once = true;
            _ZN3sce4Json6StringC1EPKc(&failed, "x");
            context.allocations->budget = -1; context.allocations->once = false;
            _ZN3sce4Json6StringD1Ev(&failed);
        }
        context.nestedStatus = _ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(context.nested, ReadTrue, &context.nestedReads);
        _ZN3sce4Json5Value3setEd(context.assignment, std::numeric_limits<double>::infinity());
        context.formatStatus = _ZN3sce4Json5Value9serializeERNS0_6StringE(context.format, context.output);
    }
    if (context.reads > 4) return static_cast<int>(0x80848101);
    character = "true"[context.reads - 1];
    return 0;
}
static void RunNotification(RecoveryAllocationTrace& allocations) {
    auto& context = *reinterpret_cast<ReadContext*>(allocations.notificationContext);
    ++context.hooks;
    if (context.fail) { allocations.budget = 0; allocations.once = true; }
    context.nestedStatus = _ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(context.nested, ReadTrue, &context.nestedReads);
    allocations.budget = -1; allocations.once = false;
    _ZN3sce4Json5Value3setEd(context.assignment, std::numeric_limits<double>::infinity());
    context.formatStatus = _ZN3sce4Json5Value9serializeERNS0_6StringE(context.format, context.output);
}
struct Expected {
    const char* document;
    bool fail;
    int type;
    const char* assignment;
    const char* output;
    std::size_t requests;
    std::uint64_t order;
    bool notification = false;
};
static const Expected Cases[]{
    {"\"recovery\"", 0, 5, "recovery", "p\"recovery\"", 22, UINT64_C(0x512fbd7009730792)},
    {"\"recovery\"", 1, 5, "recovery", "p\"recovery\"", 21, UINT64_C(0xb7c45554dbd6d058)},
    {"{\"k\":\"recovery\"}", 0, 7, "{\"k\":\"recovery\"}", "p{\"k\":\"recovery\"}", 143, UINT64_C(0x105ecd3d3b0befa3)},
    {"{\"k\":\"recovery\"}", 1, 7, "{\"k\":\"\"}", "p{\"k\":\"\"}", 137, UINT64_C(0x1c0f8b9f8e134cd2)},
    {"[[\"recovery\"],{\"k\":1}]", 0, 6, "[[\"recovery\"],{\"k\":1}]", "p[[\"recovery\"],{\"k\":1}]", 254, UINT64_C(0xd1da135cb823d2ad)},
    {"[[\"recovery\"],{\"k\":1}]", 1, 6, "[[],{}]", "p[[],{}]", 136, UINT64_C(0xe4d4124493072e0a)},
    {"\"recovery\"", 0, 5, "recovery", "p\"recovery\"", 21, UINT64_C(0x1e6ba634008a1e40), true},
    {"\"recovery\"", 1, 5, "recovery", "p\"recovery\"", 20, UINT64_C(0x4f5af7c5c1ffdcb2), true},
    {"{\"k\":\"recovery\"}", 0, 7, "{\"k\":\"recovery\"}", "p{\"k\":\"recovery\"}", 142, UINT64_C(0x1b1dd3ba293b3ae5), true},
    {"{\"k\":\"recovery\"}", 1, 7, "{\"k\":\"\"}", "p{\"k\":\"\"}", 136, UINT64_C(0x2c958fa5f20dbd84), true},
    {"[[\"recovery\"],{\"k\":1}]", 0, 6, "[[\"recovery\"],{\"k\":1}]", "p[[\"recovery\"],{\"k\":1}]", 253, UINT64_C(0x1e7b03d5c679de77), true},
    {"[[\"recovery\"],{\"k\":1}]", 1, 6, "[[],{}]", "p[[],{}]", 135, UINT64_C(0x2d2754b3359f2448), true},
};
int main() {
    RecoveryAllocationTrace allocations; RecoveryAllocator allocator;
    Parameter parameter{&allocator, &allocations, 8192, 2, {}};
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0, "Initialize inherited callback recovery contracts");
    const auto baseline = allocations.live;
    const auto baselineIds = allocations.ids;
    for (const auto& expected : Cases) {
        Value replacement{}, assignment{}, format{}, nested{}, outer{};
        _ZN3sce4Json5ValueC1Ev(&replacement);
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&replacement, expected.document, std::strlen(expected.document)) == 0, "Construct independent replacement template");
        _ZN3sce4Json5ValueC1Ed(&assignment, 0.125);
        _ZN3sce4Json5ValueC1Ed(&format, std::numeric_limits<double>::infinity());
        _ZN3sce4Json5ValueC1Ed(&nested, 0.125); _ZN3sce4Json5ValueC1Ed(&outer, 0.125);
        RecoveryContext assignCallback{&replacement, true}, formatCallback{&replacement, true};
        Require(_ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(&assignment, RecoverAllocated, &assignCallback) == 0 &&
                _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(&format, RecoverAllocated, &formatCallback) == 0, "Register assignment and formatting recovery");
        void* assignmentAuxiliary = assignment.auxiliary;
        void* formatAuxiliary = format.auxiliary;
        const auto assignmentId = allocations.ids.at(assignmentAuxiliary), formatId = allocations.ids.at(formatAuxiliary);
        String output{}; _ZN3sce4Json6StringC1EPKc(&output, "p");
        ReadContext context{&allocations, expected.fail, &assignment, &format, &nested, 0, 0, &output};
        const auto first = allocations.requests.size(); const auto notifications = allocations.notifications;
        int status;
        if (expected.notification) {
            allocations.onNotify = RunNotification; allocations.notificationContext = &context;
            allocations.budget = 0; allocations.once = true;
            status = _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&outer, "true", 4);
            allocations.onNotify = nullptr; allocations.notificationContext = nullptr;
            allocations.budget = -1; allocations.once = false;
        } else status = _ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(&outer, ReadNested, &context);
        const auto end = allocations.requests.size();
        Require(status == (expected.fail || expected.notification ? static_cast<int>(0x80848102) : 0) && outer.type == 1 && (outer.payload & 0xff) == 1 && context.reads == (expected.notification ? 0u : 4u) && context.hooks == (expected.notification ? 1u : 0u),
                "A failure raised by the reader must preserve literal completion while reporting the outer operation failure");
        Require(context.nestedStatus == (expected.fail ? static_cast<int>(0x80848102) : 0) && nested.type == (expected.fail && !expected.notification ? 4 : 1) && context.nestedReads == (expected.fail && !expected.notification ? 0u : 4u), "A nested parse must observe the inherited failure without consuming the outer failure state");
        Require(expected.fail && !expected.notification ? nested.payload == std::bit_cast<std::uint64_t>(0.125) : (nested.payload & 0xff) == 1, "Refused nested parsing must preserve its original scalar bits");
        Require(assignment.type == expected.type && Contents(assignment) == expected.assignment && context.formatStatus == 0 &&
                std::strcmp(_ZNK3sce4Json6String5c_strEv(&output), expected.output) == 0, "Inherited failures must preserve reference callback-return partial collections and serialization");
        std::uint64_t order = UINT64_C(14695981039346656037);
        for (auto i = first; i < end; ++i) { order ^= allocations.requests[i]; order *= UINT64_C(1099511628211); }
        Require(end - first == expected.requests && order == expected.order && allocations.notifications - notifications == unsigned(expected.fail) + unsigned(expected.notification), "Nested callback recovery must preserve exact allocation request order and notifications");
        Require(assignCallback.calls == 1 && assignCallback.function == 1 && !assignCallback.errorPointer && formatCallback.calls == 1 && formatCallback.function == 0 && formatCallback.errorPointer, "Nested assignment and serialization must preserve their distinct callback ABIs");
        Require(assignment.auxiliary == assignmentAuxiliary && format.auxiliary == formatAuxiliary && allocations.ids.at(assignmentAuxiliary) == assignmentId && allocations.ids.at(formatAuxiliary) == formatId, "Inherited failures must preserve destination metadata identities and generations");
        Require(Contents(replacement) == (replacement.type == 5 ? "recovery" : expected.document), "Nested recovery must preserve its independent source template");
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&nested, "true", 4) == 0 && nested.type == 1 && (nested.payload & 0xff) == 1, "Outermost completion must clear the inherited failure and permit the next operation");
        _ZN3sce4Json6StringD1Ev(&output);
        for (auto* value : {&replacement, &assignment, &format, &nested, &outer}) _ZN3sce4Json5ValueD1Ev(value);
        Require(allocations.live == baseline && allocations.ids == baselineIds, "Nested operations must release all temporary ownership without replacing module resources");
    }
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && allocations.live.empty() && allocations.ids.empty(), "Release inherited recovery resources");
    return 0;
}
