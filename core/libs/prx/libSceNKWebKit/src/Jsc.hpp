#pragma once

#include "prx/libSceNKWebKit/src/JscApi.hpp"
#include "../../../../../3rdparty/quickjs/quickjs.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace NKWebKit {

struct StaticValueEntry {
    std::string name;
    JSObjectGetPropertyCallback getProperty = nullptr;
    JSObjectSetPropertyCallback setProperty = nullptr;
    JSPropertyAttributes attributes = kJSPropertyAttributeNone;
    JSStringRef propertyNameRef = nullptr;
};

struct StaticFunctionEntry {
    std::string name;
    JSObjectCallAsFunctionCallback callAsFunction = nullptr;
    JSPropertyAttributes attributes = kJSPropertyAttributeNone;
};

struct OpaqueJSClass {
    std::atomic<std::uint32_t> refCount{1};
    JSClassAttributes attributes = kJSClassAttributeNone;
    std::string className;
    OpaqueJSClass* parentClass = nullptr;
    std::vector<StaticValueEntry> staticValues;
    std::vector<StaticFunctionEntry> staticFunctions;
    JSObjectInitializeCallback initialize = nullptr;
    JSObjectFinalizeCallback finalize = nullptr;
    JSObjectHasPropertyCallback hasProperty = nullptr;
    JSObjectGetPropertyCallback getProperty = nullptr;
    JSObjectSetPropertyCallback setProperty = nullptr;
    JSObjectDeletePropertyCallback deleteProperty = nullptr;
    JSObjectGetPropertyNamesCallback getPropertyNames = nullptr;
    JSObjectCallAsFunctionCallback callAsFunction = nullptr;
    JSObjectCallAsConstructorCallback callAsConstructor = nullptr;
    JSObjectHasInstanceCallback hasInstance = nullptr;
    JSObjectConvertToTypeCallback convertToType = nullptr;
    std::unique_ptr<OpaqueJSClass> prototypeClass;

    void DestroyInPlace();
    bool HasCallAsFunction() const;
    bool HasCallAsConstructor() const;
    bool HasConvertToType() const;
    bool HasInstanceCallback() const;
    const StaticValueEntry* FindStaticValue(const std::string& propName) const;
    const StaticFunctionEntry* FindStaticFunction(const std::string& propName) const;
};

struct OpaqueJSValue {
    OpaqueJSContextGroup* group = nullptr;
    OpaqueJSContext* ctx = nullptr;
    JSValue qval = JS_UNDEFINED;
    JSValue qweak = JS_UNDEFINED;
    void* objPtr = nullptr;
    std::uint32_t protectCount = 0;
    bool ephemeralPinned = false;
    bool alive = true;
    bool isObjectHandle = false;
};

struct PrivatePropertySlot {
    OpaqueJSValue* handle = nullptr;
    JSValue strongVal = JS_UNDEFINED;
};

struct CallbackObjectData {
    OpaqueJSContextGroup* group = nullptr;
    OpaqueJSContext* ctx = nullptr;
    OpaqueJSValue* handle = nullptr;
    OpaqueJSClass* jsClass = nullptr;
    void* privateData = nullptr;
    JSValue propStore = JS_UNDEFINED;
    std::unordered_map<std::string, PrivatePropertySlot> privateProperties;
    std::unordered_set<std::string> dynamicEnumeratedNames;
    JSObjectCallAsFunctionCallback directFunctionCallback = nullptr;
    JSObjectCallAsConstructorCallback directConstructorCallback = nullptr;
    bool isDirectFunction = false;
    bool isDirectConstructor = false;
    bool isGlobalInterceptor = false;
    bool finalized = false;
};

struct OpaqueJSContextGroup {
    std::atomic<std::uint32_t> refCount{1};
    JSRuntime* rt = nullptr;
    JSContext* utilityCtx = nullptr;
    JSValue weakRefCtor = JS_UNDEFINED;
    JSValue weakRefDerefFunc = JS_UNDEFINED;
    JSClassID callbackObjectClassId = 0;
    JSClassID callbackCallableClassId = 0;
    JSAtom toStringTagAtom = JS_ATOM_NULL;
    JSAtom toPrimitiveAtom = JS_ATOM_NULL;
    JSAtom hasInstanceAtom = JS_ATOM_NULL;
    OpaqueJSValue undefinedHandle{};
    OpaqueJSValue nullHandle{};
    OpaqueJSValue trueHandle{};
    OpaqueJSValue falseHandle{};
    std::vector<OpaqueJSContext*> contexts;
    std::vector<OpaqueJSValue*> allHandles;
    std::unordered_map<void*, OpaqueJSValue*> objectHandles;
    std::unordered_map<void*, CallbackObjectData*> callbackDataByPtr;
    std::unordered_set<OpaqueJSValue*> activeCallbackPins;
    std::vector<OpaqueJSWeak*> weakRefs;
    std::vector<OpaqueJSWeakObjectMap*> weakMaps;
    std::vector<std::pair<JSHeapFinalizer, void*>> heapFinalizers;
    std::vector<std::pair<JSMarkingConstraint, void*>> markingConstraints;
    std::size_t extraMemorySize = 0;
    bool hasTimeLimit = false;
    double timeLimitSeconds = 0.0;
    std::chrono::steady_clock::time_point deadline{};
    JSShouldTerminateCallback terminateCallback = nullptr;
    void* terminateContext = nullptr;
    OpaqueJSContext* activeContext = nullptr;
    bool inGarbageCollect = false;
    bool shuttingDown = false;

    OpaqueJSContextGroup();
    ~OpaqueJSContextGroup();

    OpaqueJSContext* DefaultContext() const;
    OpaqueJSValue* ToHandle(OpaqueJSContext* ctx, JSValue v);
    OpaqueJSValue* BorrowHandle(OpaqueJSContext* ctx, JSValueConst v);
    CallbackObjectData* FindCallbackData(const OpaqueJSValue* handle) const;
    CallbackObjectData* FindCallbackDataByPtr(void* ptr) const;
    void NotifyObjectFinalized(void* ptr, OpaqueJSValue* handle);
    void CollectGarbage();
    void DrainMicrotasks(OpaqueJSContext* ctx);
    void RefreshDeadline();
};

struct OpaqueJSContext {
    std::atomic<std::uint32_t> refCount{1};
    OpaqueJSContextGroup* group = nullptr;
    JSContext* qctx = nullptr;
    OpaqueJSValue* globalObject = nullptr;
    JSValue globalExoticInterceptor = JS_UNDEFINED;
    std::unordered_map<OpaqueJSClass*, OpaqueJSValue*> classPrototypes;
    OpaqueJSValue* unhandledRejectionCallback = nullptr;
    std::vector<std::pair<JSValue, JSValue>> pendingUnhandledRejections;
    std::string name;
    bool hasName = false;
    bool remoteInspectionEnabled = true;
    bool includesNativeCallStack = true;

    OpaqueJSContext(OpaqueJSContextGroup* grp, OpaqueJSClass* globalObjectClass);
    ~OpaqueJSContext();
};

struct OpaqueJSPropertyNameArray {
    std::atomic<std::uint32_t> refCount{1};
    std::vector<JSStringRef> names;

    ~OpaqueJSPropertyNameArray();
};

struct OpaqueJSPropertyNameAccumulator {
    std::vector<JSStringRef> names;
    std::unordered_set<std::string> seen;

    ~OpaqueJSPropertyNameAccumulator();
    void AddName(JSStringRef name);
};

struct OpaqueJSScript {
    std::atomic<std::uint32_t> refCount{1};
    OpaqueJSContextGroup* group = nullptr;
    std::string url;
    int startingLineNumber = 1;
    std::string source;

    ~OpaqueJSScript();
};

struct OpaqueJSWeak {
    std::atomic<std::uint32_t> refCount{1};
    OpaqueJSContextGroup* group = nullptr;
    OpaqueJSValue* object = nullptr;
};

struct OpaqueJSWeakObjectMap {
    OpaqueJSContextGroup* group = nullptr;
    void* privateData = nullptr;
    JSWeakMapDestroyedCallback destroyedCallback = nullptr;
    std::unordered_map<void*, OpaqueJSValue*> entries;

    ~OpaqueJSWeakObjectMap();
};

struct CallbackPinScope {
    OpaqueJSContextGroup* group = nullptr;
    std::vector<OpaqueJSValue*> newlyPinned;

    explicit CallbackPinScope(OpaqueJSContextGroup* grp) : group(grp) {}
    void Pin(const OpaqueJSValue* val) {
        if (!group || !val) return;
        auto* mutVal = const_cast<OpaqueJSValue*>(val);
        if (group->activeCallbackPins.insert(mutVal).second) {
            newlyPinned.push_back(mutVal);
        }
    }
    ~CallbackPinScope() {
        if (!group) return;
        for (auto* val : newlyPinned) {
            group->activeCallbackPins.erase(val);
        }
    }
};

std::string JsStringToUtf8(const GuestJSString* str);
GuestJSString* Utf8ToJsString(const std::string& utf8);
GuestJSString* JsValueToJsString(JSContext* qctx, JSValueConst val);
JSValue JsStringToJsValue(JSContext* qctx, const GuestJSString* str);
JSAtom JsStringToAtom(JSContext* qctx, const GuestJSString* str);
GuestJSString* AtomToJsString(JSContext* qctx, JSAtom atom);
std::string AtomToUtf8(JSContext* qctx, JSAtom atom);

void CapturePendingException(OpaqueJSContext* ctx, JSValueRef* exception);
void SetExceptionValue(OpaqueJSContext* ctx, JSValueRef* exception, JSValue excVal);
void RegisterCallbackClasses(OpaqueJSContextGroup* group);
void RunClassInitialize(OpaqueJSContext* ctx, OpaqueJSValue* objHandle, OpaqueJSClass* jsClass);
void RunClassFinalize(CallbackObjectData* data);
OpaqueJSValue* GetOrCreateClassPrototype(OpaqueJSContext* ctx, OpaqueJSClass* jsClass);
OpaqueJSValue* MakeCallbackObject(OpaqueJSContext* ctx, OpaqueJSClass* jsClass, void* data);
bool IsObjectCallable(const OpaqueJSContext* ctx, OpaqueJSValue* object);

}  // namespace NKWebKit
