#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

struct Value { Value* parent; void* auxiliary; std::uint64_t payload; std::uint32_t reserved; std::int32_t type; };
struct String { void* storage; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
using Mismatch = const Value& (APS5_VABI*)(std::int32_t, const Value*, const Value*, void*);
using Null = const Value& (APS5_VABI*)(std::int32_t, const Value*, void*);
using Special = Value* (APS5_VABI*)(Value*, std::int32_t, double, const Value*, void*, bool*);
using Element = void (APS5_VABI*)(int, const String*, const std::size_t*, const Value*, const Value*, void*);
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
int APS5_VABI _ZN3sce4Json11Initializer28setGlobalTypeMismatchHandlerEPFRKNS0_5ValueENS0_9ValueTypeEPS3_S6_PvES7_(void*, Mismatch, void*);
int APS5_VABI _ZN3sce4Json11Initializer28setGlobalSpecialFloatHandlerEPFKNS0_5ValueENS0_12FunctionTypeEdPS3_PvPbES6_(void*, Special, void*);
int APS5_VABI _ZN3sce4Json11Initializer36setGlobalElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKNS0_5ValueES9_PvESA_(void*, Element, void*);
int APS5_VABI _ZN3sce4Json11Initializer27setGlobalNullAccessCallbackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(void*, Null, void*);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueC1El(Value*, std::int64_t);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
void APS5_VABI _ZN3sce4Json5Value3setEd(Value*, double);
const std::int64_t* APS5_VABI _ZNK3sce4Json5Value10getIntegerEv(const Value*);
bool APS5_VABI _ZNK3sce4Json5ValuecvbEv(const Value*);
const Value* APS5_VABI _ZNK3sce4Json5ValueixEm(const Value*, std::size_t);
int APS5_VABI _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(Value*, Mismatch, void*);
int APS5_VABI _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(Value*, Special, void*);
int APS5_VABI _ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(Value*, Element, void*);
}
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct CallbackTrace { int calls = 0; const Value* source = nullptr; const Value* parent = nullptr; const Value* replacement = nullptr; bool reenter = false; int function = -1; };
static const Value& APS5_VABI RecoverMismatch(std::int32_t requested, const Value* source, const Value* parent, void* context) {
    auto& trace = *static_cast<CallbackTrace*>(context);
    ++trace.calls; trace.source = source; trace.parent = parent;
    if (trace.reenter) Require(*_ZNK3sce4Json5Value10getIntegerEv(source) == 0, "Global mismatch reentry must use the typed default");
    Require(requested == 1 || requested == 2, "Preserve requested type");
    return *trace.replacement;
}
static const Value& APS5_VABI RecoverNull(std::int32_t, const Value*, void* context) {
    auto& trace = *static_cast<CallbackTrace*>(context); ++trace.calls; return *trace.replacement;
}
static Value* APS5_VABI RecoverFloat(Value* result, std::int32_t function, double, const Value* parent, void* context, bool* error) {
    auto& trace = *static_cast<CallbackTrace*>(context);
    ++trace.calls; trace.function = function; trace.parent = parent;
    Require(function == 1 && !error, "Assignment passes its function kind and no error output");
    _ZN3sce4Json5ValueC1El(result, trace.calls + 100);
    return result;
}
static void APS5_VABI RecoverElement(int error, const String* key, const std::size_t* index, const Value* parent, const Value* source, void* context) {
    auto& trace = *static_cast<CallbackTrace*>(context);
    ++trace.calls; trace.source = source; trace.parent = parent;
    Require(static_cast<unsigned>(error) == 0x80848118 && !key && index && *index == 9, "Global element recovery receives lookup details");
    if (trace.reenter) _ZNK3sce4Json5ValueixEm(source, 9);
}
int main() {
    JsonTest::Allocator allocator;
    Parameter parameter{&allocator, nullptr, 16384, 2, {}};
    std::uint8_t initializer = 0, inactive = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Value root{}, replacement{}, falseValue{};
    _ZN3sce4Json5ValueC1Ev(&root);
    _ZN3sce4Json5ValueC1El(&replacement, 77);
    _ZN3sce4Json5ValueC1Ev(&falseValue);
    CallbackTrace mismatch{0, nullptr, nullptr, &replacement, true};
    CallbackTrace local{0, nullptr, nullptr, &falseValue, false};
    CallbackTrace null{0, nullptr, nullptr, &replacement, false};
    CallbackTrace special{}, element{}; element.reenter = true;
    auto registerMismatch = [&](void* self, Mismatch callback, void* context) { return _ZN3sce4Json11Initializer28setGlobalTypeMismatchHandlerEPFRKNS0_5ValueENS0_9ValueTypeEPS3_S6_PvES7_(self, callback, context); };
    auto registerSpecial = [&](void* self, Special callback, void* context) { return _ZN3sce4Json11Initializer28setGlobalSpecialFloatHandlerEPFKNS0_5ValueENS0_12FunctionTypeEdPS3_PvPbES6_(self, callback, context); };
    auto registerElement = [&](void* self, Element callback, void* context) { return _ZN3sce4Json11Initializer36setGlobalElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKNS0_5ValueES9_PvESA_(self, callback, context); };
    Require(static_cast<unsigned>(registerMismatch(&initializer, RecoverMismatch, &mismatch)) == 0x80848110 &&
            static_cast<unsigned>(registerSpecial(&initializer, RecoverFloat, &special)) == 0x80848110 &&
            static_cast<unsigned>(registerElement(&initializer, RecoverElement, &element)) == 0x80848110, "Global handlers require initialization");
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize global recovery");
    Require(static_cast<unsigned>(registerMismatch(&inactive, RecoverMismatch, &mismatch)) == 0x80848110 &&
            static_cast<unsigned>(registerSpecial(&inactive, RecoverFloat, &special)) == 0x80848110 &&
            static_cast<unsigned>(registerElement(&inactive, RecoverElement, &element)) == 0x80848110, "Inactive initializer cannot register handlers");
    Require(static_cast<unsigned>(registerMismatch(&initializer, nullptr, nullptr)) == 0x80848120 &&
            static_cast<unsigned>(registerSpecial(&initializer, nullptr, nullptr)) == 0x80848120 &&
            static_cast<unsigned>(registerElement(&initializer, nullptr, nullptr)) == 0x80848120, "Global handlers reject null callbacks");
    Require(registerMismatch(&initializer, RecoverMismatch, &mismatch) == 0 && registerSpecial(&initializer, RecoverFloat, &special) == 0 && registerElement(&initializer, RecoverElement, &element) == 0, "Register all global handlers");
    Require(static_cast<unsigned>(registerMismatch(&initializer, RecoverMismatch, &local)) == 0x80848112 &&
            static_cast<unsigned>(registerSpecial(&initializer, RecoverFloat, &local)) == 0x80848112 &&
            static_cast<unsigned>(registerElement(&initializer, RecoverElement, &local)) == 0x80848112, "Duplicate registration preserves original contexts");
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&root) == 77 && mismatch.calls == 1 && mismatch.source == &root && !mismatch.parent, "Global mismatch recovers an unowned null value");
    Require(_ZN3sce4Json11Initializer27setGlobalNullAccessCallbackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(&initializer, RecoverNull, &null) == 0, "Register global null callback");
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&root) == 77 && null.calls == 1 && mismatch.calls == 1, "Global null callback precedes global mismatch for null getters");
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&root, RecoverMismatch, &local) == 0, "Register local mismatch");
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&root) == 0 && local.calls == 1 && null.calls == 1 && mismatch.calls == 1, "Local mismatch overrides both global callbacks");
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&root, nullptr, nullptr) == 0, "Unregister local mismatch");
    _ZN3sce4Json5Value3setEd(&root, std::numeric_limits<double>::infinity());
    Require(special.calls == 1 && * _ZNK3sce4Json5Value10getIntegerEv(&root) == 101, "Global float handler supplies assignment replacement");
    Require(_ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(&root, RecoverFloat, &local) == 0, "Register local float handler");
    _ZN3sce4Json5Value3setEd(&root, std::numeric_limits<double>::infinity());
    Require(special.calls == 1 && local.calls == 2, "Local float handler overrides global recovery");
    const auto* fallback = _ZNK3sce4Json5ValueixEm(&root, 9);
    Require(element.calls == 1 && element.source == &root && !element.parent && fallback->type == 0, "Global element handler receives failed access and suppresses reentry");
    _ZNK3sce4Json5ValueixEm(fallback, 9);
    Require(element.calls == 1, "Fallback access must not recursively invoke global element recovery");
    Require(_ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(&root, RecoverElement, &local) == 0, "Register local element handler");
    _ZNK3sce4Json5ValueixEm(&root, 9);
    Require(element.calls == 1 && local.calls == 3, "Local element handler overrides global recovery");
    _ZN3sce4Json5ValueD1Ev(&root);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Terminate global recovery");
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Reinitialize after global recovery");
    Require(registerMismatch(&initializer, RecoverMismatch, &mismatch) == 0 && registerSpecial(&initializer, RecoverFloat, &special) == 0 && registerElement(&initializer, RecoverElement, &element) == 0, "Termination clears all global registration slots");
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Terminate second initialization");
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(&initializer, &parameter) == 0, "Legacy initialization");
    Require(static_cast<unsigned>(registerSpecial(&initializer, nullptr, nullptr)) == 0x80848113 && static_cast<unsigned>(registerSpecial(&initializer, RecoverFloat, &special)) == 0x80848113, "Legacy initialization rejects global float registration before callback validation");
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Terminate legacy initialization");
    _ZN3sce4Json5ValueD1Ev(&replacement);
    _ZN3sce4Json5ValueD1Ev(&falseValue);
    return 0;
}
