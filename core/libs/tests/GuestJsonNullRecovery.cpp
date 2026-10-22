#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct Value { Value* parent; void* auxiliary; std::uintptr_t payload; std::uint32_t reserved; std::int32_t type; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
using Handler = const Value& (APS5_VABI*)(std::int32_t, const Value*, void*);
using MismatchHandler = const Value& (APS5_VABI*)(std::int32_t, const Value*, const Value*, void*);
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
int APS5_VABI _ZN3sce4Json11Initializer27setGlobalNullAccessCallbackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(void*, Handler, void*);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueC1El(Value*, std::int64_t);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
int APS5_VABI _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(Value*, Handler, void*);
int APS5_VABI _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(Value*, MismatchHandler, void*);
bool APS5_VABI _ZNK3sce4Json5ValuecvbEv(const Value*);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value*, const char*, std::size_t);
const Value* APS5_VABI _ZNK3sce4Json5ValueixEPKc(const Value*, const char*);
const std::int64_t* APS5_VABI _ZNK3sce4Json5Value10getIntegerEv(const Value*);
const bool* APS5_VABI _ZNK3sce4Json5Value10getBooleanEv(const Value*);
}
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct Context { int calls = 0; int requested = -1; const Value* parent = nullptr; const Value* replacement; const Value* reenter = nullptr; };
static const Value* mismatchSource = nullptr;
static const Value& APS5_VABI Mismatch(std::int32_t requested, const Value* self, const Value* parent, void* context) {
    mismatchSource = self;
    auto& trace = *static_cast<Context*>(context);
    ++trace.calls;
    trace.requested = requested;
    trace.parent = parent;
    if (trace.reenter)
        Require(*_ZNK3sce4Json5Value10getIntegerEv(trace.reenter) == 0, "Reentrant mismatch recovery must return its typed default");
    return *trace.replacement;
}
static const Value& APS5_VABI Recover(std::int32_t requested, const Value* parent, void* context) {
    auto& trace = *static_cast<Context*>(context);
    ++trace.calls;
    trace.requested = requested;
    trace.parent = parent;
    if (trace.reenter)
        Require(*_ZNK3sce4Json5Value10getIntegerEv(trace.reenter) == 0, "Reentrant null access must return its typed default");
    return *trace.replacement;
}
int main() {
    std::uint8_t initializer = 0, inactive = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Value replacement{}, localReplacement{};
    _ZN3sce4Json5ValueC1El(&replacement, 99);
    _ZN3sce4Json5ValueC1El(&localReplacement, 77);
    Context global{0, -1, nullptr, &replacement, nullptr};
    Context local{0, -1, nullptr, &localReplacement, nullptr};
    Require(static_cast<unsigned>(_ZN3sce4Json11Initializer27setGlobalNullAccessCallbackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(&initializer, Recover, &global)) == 0x80848110,
            "Global callback registration must require an active initializer");
    JsonTest::Allocator allocator;
    Parameter parameter{&allocator, nullptr, 0, 2, {}};
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize null recovery");
    Require(static_cast<unsigned>(_ZN3sce4Json11Initializer27setGlobalNullAccessCallbackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(&inactive, Recover, &global)) == 0x80848110,
            "An inactive object cannot register a module-global callback");
    Require(static_cast<unsigned>(_ZN3sce4Json11Initializer27setGlobalNullAccessCallbackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(&initializer, nullptr, nullptr)) == 0x80848120,
            "Global registration must reject a null callback");
    Require(_ZN3sce4Json11Initializer27setGlobalNullAccessCallbackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(&initializer, Recover, &global) == 0, "Register global null recovery");
    Require(static_cast<unsigned>(_ZN3sce4Json11Initializer27setGlobalNullAccessCallbackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(&initializer, Recover, &local)) == 0x80848112,
            "Duplicate global registration must preserve its original callback and context");
    Value root{};
    _ZN3sce4Json5ValueC1Ev(&root);
    const char text[] = "{\"n\":null,\"s\":\"text\"}";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, text, sizeof(text) - 1) == 0, "Parse null recovery tree");
    const auto* null = _ZNK3sce4Json5ValueixEPKc(&root, "n");
    const auto* string = _ZNK3sce4Json5ValueixEPKc(&root, "s");
    const auto* missing = _ZNK3sce4Json5ValueixEPKc(&root, "missing");
    Require(missing->type == 0 && global.calls == 0, "Missing lookup must not invoke a typed null-access callback");
    Require(*_ZNK3sce4Json5Value10getIntegerEv(null) == 99 && global.calls == 1 && global.requested == 2 && global.parent == &root,
            "A typed null getter must call global recovery with its immediate parent");
    Require(*_ZNK3sce4Json5Value10getIntegerEv(string) == 0 && global.calls == 1,
            "A non-null type mismatch must not use null-access recovery");
    Require(*_ZNK3sce4Json5Value10getIntegerEv(missing) == 99 && global.calls == 2 && global.parent == &root,
            "The parser-owned fallback must retain its root parent");
    Require(_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(&root, Recover, &local) == 0,
            "The value callback setter must take this, callback and context");
    Require(*_ZNK3sce4Json5Value10getIntegerEv(null) == 77 && local.calls == 1 && local.parent == &root && global.calls == 2,
            "Root-local null recovery must take precedence over global recovery");
    local.reenter = null;
    Require(!*_ZNK3sce4Json5Value10getBooleanEv(null) && local.calls == 2 && local.requested == 1,
            "A mismatched callback result and reentrant getter must not recursively dispatch");
    local.reenter = nullptr;
    Require(_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(&root, nullptr, nullptr) == 0,
            "A value-local callback may be unregistered");
    Require(*_ZNK3sce4Json5Value10getIntegerEv(null) == 99 && global.calls == 3 && local.calls == 2,
            "Unregistration must restore global recovery");
    Require(static_cast<unsigned>(_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(const_cast<Value*>(null), Recover, &local)) == 0x80848104,
            "Owned children must reject local null callback registration");
    Context mismatch{0, -1, nullptr, &localReplacement, string};
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&root, Mismatch, &mismatch) == 0,
            "Register ancestor mismatch recovery");
    Require(*_ZNK3sce4Json5Value10getIntegerEv(string) == 77 && mismatch.calls == 1 && mismatchSource == string &&
                mismatch.parent == &root && mismatch.requested == 2,
            "Mismatch recovery must pass the requested type, source and immediate parent");
    Require(!*_ZNK3sce4Json5Value10getBooleanEv(string) && mismatch.calls == 2 && mismatch.requested == 1,
            "A wrong-type recovery result must return the requested default without recursively dispatching");
    Require(!_ZNK3sce4Json5ValuecvbEv(string) && mismatch.calls == 3,
            "Boolean conversion must consult mismatch recovery before ordinary truth conversion");
    Require(!_ZNK3sce4Json5ValuecvbEv(&root) && mismatch.calls == 4 && mismatchSource == &root && !mismatch.parent,
            "A root boolean conversion must pass a null parent to mismatch recovery");
    Require(*_ZNK3sce4Json5Value10getIntegerEv(null) == 77 && global.calls == 3 && mismatch.calls == 5,
            "Root mismatch recovery must take precedence over module-global null recovery");
    Require(_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(&root, Recover, &local) == 0,
            "Restore root-local null recovery");
    Require(*_ZNK3sce4Json5Value10getIntegerEv(null) == 77 && local.calls == 3 && mismatch.calls == 5,
            "Root-local null recovery must take precedence over mismatch recovery");
    _ZN3sce4Json5ValueD1Ev(&root);
    _ZN3sce4Json5ValueD1Ev(&replacement);
    _ZN3sce4Json5ValueD1Ev(&localReplacement);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Terminate null recovery");
    Value after{};
    _ZN3sce4Json5ValueC1Ev(&after);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&after) == 0 && global.calls == 3, "Termination must clear global recovery");
    _ZN3sce4Json5ValueD1Ev(&after);
    return 0;
}
