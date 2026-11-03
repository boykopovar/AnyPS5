#include "Jsc.hpp"
#include "prx/libSceNKWebKit/include/JscExports.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace NKWebKit {

namespace {

bool g_remoteInspectionEnabledByDefault = true;

int InterruptHandler(JSRuntime* rt, void* opaque) {
    auto* group = static_cast<OpaqueJSContextGroup*>(opaque);
    if (!group || !group->hasTimeLimit) return 0;
    const auto now = std::chrono::steady_clock::now();
    if (now < group->deadline) return 0;
    if (group->terminateCallback) {
        OpaqueJSContext* ctx = group->activeContext ? group->activeContext : group->DefaultContext();
        if (!group->terminateCallback(ctx, group->terminateContext)) {
            group->RefreshDeadline();
            return 0;
        }
    }
    group->hasTimeLimit = false;
    return 1;
}

void PromiseRejectionTracker(JSContext* qctx, JSValueConst promise, JSValueConst reason, bool isHandled, void* opaque) {
    (void)opaque;
    auto* ctx = static_cast<OpaqueJSContext*>(JS_GetContextOpaque(qctx));
    if (!ctx) return;
    if (!isHandled) {
        ctx->pendingUnhandledRejections.emplace_back(JS_DupValue(qctx, promise), JS_DupValue(qctx, reason));
    } else {
        for (auto it = ctx->pendingUnhandledRejections.begin(); it != ctx->pendingUnhandledRejections.end(); ++it) {
            if (JS_IsStrictEqual(qctx, it->first, promise)) {
                JS_FreeValue(qctx, it->first);
                JS_FreeValue(qctx, it->second);
                ctx->pendingUnhandledRejections.erase(it);
                break;
            }
        }
    }
}

struct ActiveContextScope {
    OpaqueJSContextGroup* group = nullptr;
    OpaqueJSContext* previous = nullptr;

    ActiveContextScope(OpaqueJSContextGroup* grp, OpaqueJSContext* ctx) : group(grp) {
        if (group) {
            previous = group->activeContext;
            group->activeContext = ctx;
        }
    }
    ~ActiveContextScope() {
        if (group) {
            group->activeContext = previous;
        }
    }
};

struct ConstraintMarker : JSMarker {
    OpaqueJSContextGroup* group = nullptr;
    std::unordered_set<OpaqueJSValue*>* markedSet = nullptr;
    bool changed = false;
};

bool APS5_VABI MarkerIsMarked(JSMarker* baseMarker, JSObjectRef object) {
    auto* marker = static_cast<ConstraintMarker*>(baseMarker);
    if (!marker || !object || !object->alive) return false;
    if (object->protectCount > 0) return true;
    if (marker->group->activeCallbackPins.count(object) > 0) return true;
    for (auto* ctx : marker->group->contexts) {
        if (ctx && ctx->globalObject == object) return true;
    }
    return marker->markedSet && marker->markedSet->count(object) > 0;
}

void APS5_VABI MarkerMark(JSMarker* baseMarker, JSObjectRef object) {
    auto* marker = static_cast<ConstraintMarker*>(baseMarker);
    if (!marker || !object || !object->alive || !marker->markedSet) return;
    if (marker->markedSet->insert(object).second) {
        marker->changed = true;
    }
}

int ParseErrorLineFromStack(const std::string& stack, int fallbackLine) {
    const auto colon2 = stack.rfind(':');
    if (colon2 == std::string::npos || colon2 == 0) return fallbackLine;
    const auto colon1 = stack.rfind(':', colon2 - 1);
    if (colon1 == std::string::npos || colon1 + 1 >= colon2) return fallbackLine;
    int line = 0;
    for (std::size_t i = colon1 + 1; i < colon2; ++i) {
        if (stack[i] < '0' || stack[i] > '9') return fallbackLine;
        line = line * 10 + (stack[i] - '0');
    }
    return line > 0 ? line : fallbackLine;
}

JSScriptRef CreateScriptInternal(OpaqueJSContextGroup* group, JSStringRef url, int startingLineNumber, const char* source, std::size_t length, JSStringRef* errorMessage, int* errorLine) {
    if (!group) return nullptr;
    const int clampedLine = startingLineNumber <= 0 ? 1 : startingLineNumber;
    const std::string urlStr = url ? JsStringToUtf8(url) : std::string();
    std::string sourceCopy(source ? source : "", length);
    JSContext* qctx = group->DefaultContext() ? group->DefaultContext()->qctx : group->utilityCtx;
    JSEvalOptions options{};
    options.version = JS_EVAL_OPTIONS_VERSION;
    options.eval_flags = JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY;
    options.filename = urlStr.c_str();
    options.line_num = clampedLine;
    JSValue compiled = JS_Eval2(qctx, sourceCopy.c_str(), sourceCopy.size(), &options);
    if (JS_IsException(compiled)) {
        JSValue exc = JS_GetException(qctx);
        if (errorMessage) {
            JSValue msgVal = JS_GetPropertyStr(qctx, exc, "message");
            if (JS_IsString(msgVal)) {
                *errorMessage = JsValueToJsString(qctx, msgVal);
            } else {
                *errorMessage = JsValueToJsString(qctx, exc);
            }
            JS_FreeValue(qctx, msgVal);
        }
        if (errorLine) {
            int parsedLine = clampedLine;
            JSValue stackVal = JS_GetPropertyStr(qctx, exc, "stack");
            if (JS_IsString(stackVal)) {
                const char* stackStr = JS_ToCString(qctx, stackVal);
                if (stackStr) {
                    parsedLine = ParseErrorLineFromStack(stackStr, clampedLine);
                    JS_FreeCString(qctx, stackStr);
                }
            }
            JS_FreeValue(qctx, stackVal);
            *errorLine = parsedLine;
        }
        JS_FreeValue(qctx, exc);
        return nullptr;
    }
    JS_FreeValue(qctx, compiled);
    auto* script = new OpaqueJSScript();
    group->refCount.fetch_add(1, std::memory_order_relaxed);
    script->group = group;
    script->url = urlStr;
    script->startingLineNumber = clampedLine;
    script->source = std::move(sourceCopy);
    return script;
}

}  // namespace

std::string JsStringToUtf8(const GuestJSString* str) {
    if (!str || str->nullString) return {};
    const std::size_t maxLen = JSStringGetMaximumUTF8CStringSize_nid_postfix(str);
    if (maxLen == 0) return {};
    std::string buffer(maxLen, '\0');
    const std::size_t written = JSStringGetUTF8CString_nid_postfix(str, buffer.data(), buffer.size());
    if (written == 0) return {};
    buffer.resize(written - 1);
    return buffer;
}

GuestJSString* Utf8ToJsString(const std::string& utf8) {
    return JSStringCreateWithUTF8CString_nid_postfix(utf8.c_str());
}

GuestJSString* JsValueToJsString(JSContext* qctx, JSValueConst val) {
    JSValue strVal = JS_IsString(val) ? JS_DupValue(qctx, val) : JS_ToString(qctx, val);
    if (JS_IsException(strVal)) return nullptr;
    std::size_t len = 0;
    const std::uint16_t* chars = JS_ToCStringLenUTF16(qctx, &len, strVal);
    if (!chars) {
        JS_FreeValue(qctx, strVal);
        return nullptr;
    }
    GuestJSString* result = JSStringCreateWithCharacters_nid_postfix(chars, len);
    JS_FreeCStringUTF16(qctx, chars);
    JS_FreeValue(qctx, strVal);
    return result;
}

JSValue JsStringToJsValue(JSContext* qctx, const GuestJSString* str) {
    if (!str || str->nullString) return JS_NewStringLen(qctx, "", 0);
    const std::size_t len = JSStringGetLength_nid_postfix(str);
    const std::uint16_t* chars = JSStringGetCharactersPtr_nid_postfix(str);
    if (!chars || len == 0) return JS_NewStringLen(qctx, "", 0);
    return JS_NewStringUTF16(qctx, chars, len);
}

JSAtom JsStringToAtom(JSContext* qctx, const GuestJSString* str) {
    JSValue val = JsStringToJsValue(qctx, str);
    if (JS_IsException(val)) return JS_ATOM_NULL;
    JSAtom atom = JS_ValueToAtom(qctx, val);
    JS_FreeValue(qctx, val);
    return atom;
}

GuestJSString* AtomToJsString(JSContext* qctx, JSAtom atom) {
    JSValue strVal = JS_AtomToString(qctx, atom);
    if (JS_IsException(strVal)) return nullptr;
    GuestJSString* res = JsValueToJsString(qctx, strVal);
    JS_FreeValue(qctx, strVal);
    return res;
}

std::string AtomToUtf8(JSContext* qctx, JSAtom atom) {
    std::size_t len = 0;
    const char* cstr = JS_AtomToCStringLen(qctx, &len, atom);
    if (!cstr) return {};
    std::string res(cstr, len);
    JS_FreeCString(qctx, cstr);
    return res;
}

void CapturePendingException(OpaqueJSContext* ctx, JSValueRef* exception) {
    if (!ctx || !ctx->qctx) return;
    JSValue exc = JS_GetException(ctx->qctx);
    if (JS_IsNull(exc) || JS_IsUndefined(exc)) {
        JS_FreeValue(ctx->qctx, exc);
        return;
    }
    if (JS_IsUncatchableError(exc)) {
        JS_ClearUncatchableError(ctx->qctx, exc);
    }
    if (exception) {
        *exception = ctx->group->ToHandle(ctx, exc);
    } else {
        JS_FreeValue(ctx->qctx, exc);
    }
}

void SetExceptionValue(OpaqueJSContext* ctx, JSValueRef* exception, JSValue excVal) {
    if (!ctx || !ctx->qctx) return;
    if (exception) {
        *exception = ctx->group->ToHandle(ctx, excVal);
    } else {
        JS_FreeValue(ctx->qctx, excVal);
    }
}

OpaqueJSContextGroup::OpaqueJSContextGroup() {
    rt = JS_NewRuntime();
    JS_SetRuntimeOpaque(rt, this);
    JS_SetMaxStackSize(rt, 0);
    JS_SetGCThreshold(rt, static_cast<std::size_t>(-1));
    JS_SetInterruptHandler(rt, &InterruptHandler, this);
    JS_SetHostPromiseRejectionTracker(rt, &PromiseRejectionTracker, this);

    utilityCtx = JS_NewContext(rt);
    JSValue globalObj = JS_GetGlobalObject(utilityCtx);
    weakRefCtor = JS_GetPropertyStr(utilityCtx, globalObj, "WeakRef");
    JSValue weakRefProto = JS_GetPropertyStr(utilityCtx, weakRefCtor, "prototype");
    weakRefDerefFunc = JS_GetPropertyStr(utilityCtx, weakRefProto, "deref");
    JSValue symbolCtor = JS_GetPropertyStr(utilityCtx, globalObj, "Symbol");
    JSValue toStringTagSym = JS_GetPropertyStr(utilityCtx, symbolCtor, "toStringTag");
    JSValue toPrimitiveSym = JS_GetPropertyStr(utilityCtx, symbolCtor, "toPrimitive");
    JSValue hasInstanceSym = JS_GetPropertyStr(utilityCtx, symbolCtor, "hasInstance");
    toStringTagAtom = JS_ValueToAtom(utilityCtx, toStringTagSym);
    toPrimitiveAtom = JS_ValueToAtom(utilityCtx, toPrimitiveSym);
    hasInstanceAtom = JS_ValueToAtom(utilityCtx, hasInstanceSym);
    JS_FreeValue(utilityCtx, toStringTagSym);
    JS_FreeValue(utilityCtx, toPrimitiveSym);
    JS_FreeValue(utilityCtx, hasInstanceSym);
    JS_FreeValue(utilityCtx, symbolCtor);
    JS_FreeValue(utilityCtx, weakRefProto);
    JS_FreeValue(utilityCtx, globalObj);

    undefinedHandle.group = this;
    undefinedHandle.qval = JS_UNDEFINED;
    nullHandle.group = this;
    nullHandle.qval = JS_NULL;
    trueHandle.group = this;
    trueHandle.qval = JS_TRUE;
    falseHandle.group = this;
    falseHandle.qval = JS_FALSE;

    RegisterCallbackClasses(this);
}

OpaqueJSContextGroup::~OpaqueJSContextGroup() {
    shuttingDown = true;
    for (auto* handle : allHandles) {
        if (!handle) continue;
        if (!JS_IsUndefined(handle->qweak)) {
            JS_FreeValueRT(rt, handle->qweak);
            handle->qweak = JS_UNDEFINED;
        }
        if (handle->ephemeralPinned) {
            handle->ephemeralPinned = false;
            JS_FreeValueRT(rt, handle->qval);
        }
        handle->alive = false;
    }
    for (auto& [ptr, data] : callbackDataByPtr) {
        if (!data) continue;
        RunClassFinalize(data);
        if (!JS_IsUndefined(data->propStore)) {
            JS_FreeValueRT(rt, data->propStore);
            data->propStore = JS_UNDEFINED;
        }
        for (auto& [k, slot] : data->privateProperties) {
            if (!JS_IsUndefined(slot.strongVal)) {
                JS_FreeValueRT(rt, slot.strongVal);
                slot.strongVal = JS_UNDEFINED;
            }
        }
        data->privateProperties.clear();
    }
    if (toStringTagAtom != JS_ATOM_NULL) JS_FreeAtomRT(rt, toStringTagAtom);
    if (toPrimitiveAtom != JS_ATOM_NULL) JS_FreeAtomRT(rt, toPrimitiveAtom);
    if (hasInstanceAtom != JS_ATOM_NULL) JS_FreeAtomRT(rt, hasInstanceAtom);
    if (!JS_IsUndefined(weakRefDerefFunc)) JS_FreeValueRT(rt, weakRefDerefFunc);
    if (!JS_IsUndefined(weakRefCtor)) JS_FreeValueRT(rt, weakRefCtor);
    if (utilityCtx) {
        JS_FreeContext(utilityCtx);
        utilityCtx = nullptr;
    }
    if (rt) {
        JS_FreeRuntime(rt);
        rt = nullptr;
    }
    for (auto& [ptr, data] : callbackDataByPtr) {
        delete data;
    }
    callbackDataByPtr.clear();
    for (auto* handle : allHandles) {
        delete handle;
    }
    allHandles.clear();
}

OpaqueJSContext* OpaqueJSContextGroup::DefaultContext() const {
    return contexts.empty() ? nullptr : contexts.front();
}

OpaqueJSValue* OpaqueJSContextGroup::ToHandle(OpaqueJSContext* ctx, JSValue v) {
    const int tag = JS_VALUE_GET_NORM_TAG(v);
    if (tag == JS_TAG_UNDEFINED) return &undefinedHandle;
    if (tag == JS_TAG_NULL) return &nullHandle;
    if (tag == JS_TAG_BOOL) return JS_VALUE_GET_BOOL(v) ? &trueHandle : &falseHandle;
    if (tag == JS_TAG_OBJECT) {
        void* ptr = JS_VALUE_GET_PTR(v);
        auto it = objectHandles.find(ptr);
        if (it != objectHandles.end() && it->second && it->second->alive) {
            OpaqueJSValue* existing = it->second;
            if (!existing->ephemeralPinned) {
                existing->qval = v;
                existing->ephemeralPinned = true;
            } else {
                JS_FreeValueRT(rt, v);
            }
            if (!existing->ctx && ctx) existing->ctx = ctx;
            return existing;
        }
        auto* handle = new OpaqueJSValue();
        handle->group = this;
        handle->ctx = ctx ? ctx : DefaultContext();
        handle->qval = v;
        handle->objPtr = ptr;
        handle->ephemeralPinned = true;
        handle->alive = true;
        handle->isObjectHandle = true;
        if (!JS_IsUndefined(weakRefCtor) && utilityCtx) {
            JSValueConst arg = v;
            JSValue w = JS_CallConstructor(utilityCtx, weakRefCtor, 1, &arg);
            if (!JS_IsException(w)) {
                handle->qweak = w;
            } else {
                JS_FreeValue(utilityCtx, JS_GetException(utilityCtx));
            }
        }
        objectHandles[ptr] = handle;
        allHandles.push_back(handle);
        return handle;
    }
    auto* handle = new OpaqueJSValue();
    handle->group = this;
    handle->ctx = ctx ? ctx : DefaultContext();
    handle->qval = v;
    handle->ephemeralPinned = true;
    handle->alive = true;
    handle->isObjectHandle = false;
    allHandles.push_back(handle);
    return handle;
}

OpaqueJSValue* OpaqueJSContextGroup::BorrowHandle(OpaqueJSContext* ctx, JSValueConst v) {
    return ToHandle(ctx, JS_DupValueRT(rt, v));
}

CallbackObjectData* OpaqueJSContextGroup::FindCallbackData(const OpaqueJSValue* handle) const {
    if (!handle || !handle->alive || !handle->isObjectHandle) return nullptr;
    return FindCallbackDataByPtr(handle->objPtr);
}

CallbackObjectData* OpaqueJSContextGroup::FindCallbackDataByPtr(void* ptr) const {
    if (!ptr) return nullptr;
    auto it = callbackDataByPtr.find(ptr);
    return it != callbackDataByPtr.end() ? it->second : nullptr;
}

void OpaqueJSContextGroup::NotifyObjectFinalized(void* ptr, OpaqueJSValue* handle) {
    if (handle) {
        handle->alive = false;
        handle->ephemeralPinned = false;
        handle->qval = JS_UNDEFINED;
        if (!JS_IsUndefined(handle->qweak) && !shuttingDown) {
            JS_FreeValueRT(rt, handle->qweak);
            handle->qweak = JS_UNDEFINED;
        }
        for (auto* weak : weakRefs) {
            if (weak && weak->object == handle) {
                weak->object = nullptr;
            }
        }
        for (auto* map : weakMaps) {
            if (!map) continue;
            for (auto it = map->entries.begin(); it != map->entries.end();) {
                if (it->second == handle) {
                    it = map->entries.erase(it);
                } else {
                    ++it;
                }
            }
        }
        for (auto* ctx : contexts) {
            if (!ctx) continue;
            for (auto it = ctx->classPrototypes.begin(); it != ctx->classPrototypes.end();) {
                if (it->second == handle) {
                    it = ctx->classPrototypes.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
    if (ptr) {
        objectHandles.erase(ptr);
    }
}

void OpaqueJSContextGroup::CollectGarbage() {
    if (inGarbageCollect || shuttingDown) return;
    inGarbageCollect = true;

    std::unordered_set<OpaqueJSValue*> constraintMarked;
    if (!markingConstraints.empty()) {
        ConstraintMarker marker{};
        marker.IsMarked = &MarkerIsMarked;
        marker.Mark = &MarkerMark;
        marker.group = this;
        marker.markedSet = &constraintMarked;
        do {
            marker.changed = false;
            const auto constraintsCopy = markingConstraints;
            for (const auto& [fn, userData] : constraintsCopy) {
                if (fn) fn(&marker, userData);
            }
        } while (marker.changed);
    }

    std::vector<OpaqueJSValue*> candidates;
    candidates.reserve(objectHandles.size());
    for (const auto& [ptr, handle] : objectHandles) {
        if (!handle || !handle->alive || !handle->isObjectHandle || !handle->ephemeralPinned) continue;
        if (handle->protectCount > 0) continue;
        if (activeCallbackPins.count(handle) > 0) continue;
        if (constraintMarked.count(handle) > 0) continue;
        bool isGlobal = false;
        for (auto* ctx : contexts) {
            if (ctx && (ctx->globalObject == handle || ctx->unhandledRejectionCallback == handle)) {
                isGlobal = true;
                break;
            }
        }
        if (isGlobal) continue;
        candidates.push_back(handle);
    }

    for (auto* handle : candidates) {
        if (!handle->alive || !handle->ephemeralPinned) continue;
        handle->ephemeralPinned = false;
        JS_FreeValueRT(rt, handle->qval);
    }

    JS_RunGC(rt);

    for (auto* handle : candidates) {
        if (!handle->alive) continue;
        if (JS_IsUndefined(handle->qweak)) {
            NotifyObjectFinalized(handle->objPtr, handle);
            continue;
        }
        JSValue derefVal = JS_Call(utilityCtx, weakRefDerefFunc, handle->qweak, 0, nullptr);
        if (JS_IsException(derefVal) || JS_IsUndefined(derefVal)) {
            if (JS_IsException(derefVal)) {
                JS_FreeValue(utilityCtx, JS_GetException(utilityCtx));
            }
            NotifyObjectFinalized(handle->objPtr, handle);
        } else {
            handle->qval = derefVal;
            handle->ephemeralPinned = true;
        }
    }

    const auto finalizersCopy = heapFinalizers;
    for (const auto& [fn, userData] : finalizersCopy) {
        if (fn) fn(this, userData);
    }

    inGarbageCollect = false;
}

void OpaqueJSContextGroup::DrainMicrotasks(OpaqueJSContext* ctx) {
    JSContext* jobCtx = nullptr;
    while (JS_IsJobPending(rt)) {
        if (JS_ExecutePendingJob(rt, &jobCtx) < 0) {
            if (jobCtx) {
                JS_FreeValue(jobCtx, JS_GetException(jobCtx));
            }
        }
    }
    if (ctx && !ctx->pendingUnhandledRejections.empty()) {
        auto rejections = std::move(ctx->pendingUnhandledRejections);
        ctx->pendingUnhandledRejections.clear();
        for (auto& [promiseVal, reasonVal] : rejections) {
            if (ctx->unhandledRejectionCallback && ctx->unhandledRejectionCallback->alive) {
                JSValueConst args[2] = {promiseVal, reasonVal};
                JSValue res = JS_Call(ctx->qctx, ctx->unhandledRejectionCallback->qval, ctx->globalObject->qval, 2, args);
                JS_FreeValue(ctx->qctx, res);
                if (JS_HasException(ctx->qctx)) {
                    JS_FreeValue(ctx->qctx, JS_GetException(ctx->qctx));
                }
            }
            JS_FreeValue(ctx->qctx, promiseVal);
            JS_FreeValue(ctx->qctx, reasonVal);
        }
    }
}

void OpaqueJSContextGroup::RefreshDeadline() {
    if (timeLimitSeconds <= 0.0) {
        deadline = std::chrono::steady_clock::now();
    } else {
        deadline = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(timeLimitSeconds));
    }
}

OpaqueJSContext::OpaqueJSContext(OpaqueJSContextGroup* grp, OpaqueJSClass* globalObjectClass)
    : group(grp), remoteInspectionEnabled(g_remoteInspectionEnabledByDefault) {
    group->refCount.fetch_add(1, std::memory_order_relaxed);
    qctx = JS_NewContext(group->rt);
    JS_SetContextOpaque(qctx, this);
    group->contexts.push_back(this);

    JSValue rawGlobal = JS_GetGlobalObject(qctx);
    JSValue objCtor = JS_GetPropertyStr(qctx, rawGlobal, "Object");
    JSValue objProto = JS_GetPropertyStr(qctx, objCtor, "prototype");
    JS_FreeValue(qctx, objCtor);
    JS_SetClassProto(qctx, group->callbackObjectClassId, JS_DupValue(qctx, objProto));
    JS_SetClassProto(qctx, group->callbackCallableClassId, objProto);
    globalObject = group->ToHandle(this, rawGlobal);

    if (globalObjectClass) {
        auto* data = new CallbackObjectData();
        data->group = group;
        data->ctx = this;
        data->handle = globalObject;
        globalObjectClass->refCount.fetch_add(1, std::memory_order_relaxed);
        data->jsClass = globalObjectClass;
        data->propStore = JS_NewObjectProto(qctx, JS_NULL);
        group->callbackDataByPtr[globalObject->objPtr] = data;

        globalExoticInterceptor = JS_NewObjectClass(qctx, group->callbackObjectClassId);
        void* interceptorPtr = JS_VALUE_GET_PTR(globalExoticInterceptor);
        auto* interceptorData = new CallbackObjectData();
        interceptorData->group = group;
        interceptorData->ctx = this;
        interceptorData->handle = globalObject;
        globalObjectClass->refCount.fetch_add(1, std::memory_order_relaxed);
        interceptorData->jsClass = globalObjectClass;
        interceptorData->isGlobalInterceptor = true;
        JS_SetOpaque(globalExoticInterceptor, interceptorData);
        group->callbackDataByPtr[interceptorPtr] = interceptorData;

        OpaqueJSValue* classProto = GetOrCreateClassPrototype(this, globalObjectClass);
        if (classProto && classProto->alive) {
            JS_SetPrototype(qctx, globalExoticInterceptor, classProto->qval);
        }
        JS_SetPrototype(qctx, globalObject->qval, globalExoticInterceptor);

        RunClassInitialize(this, globalObject, globalObjectClass);
    }
}

OpaqueJSContext::~OpaqueJSContext() {
    for (auto& [promiseVal, reasonVal] : pendingUnhandledRejections) {
        JS_FreeValue(qctx, promiseVal);
        JS_FreeValue(qctx, reasonVal);
    }
    pendingUnhandledRejections.clear();

    if (globalObject) {
        if (auto* data = group->FindCallbackData(globalObject)) {
            RunClassFinalize(data);
        }
    }
    if (!JS_IsUndefined(globalExoticInterceptor)) {
        void* interceptorPtr = JS_VALUE_GET_PTR(globalExoticInterceptor);
        if (auto* interceptorData = group->FindCallbackDataByPtr(interceptorPtr)) {
            if (interceptorData->jsClass) {
                if (interceptorData->jsClass->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    interceptorData->jsClass->DestroyInPlace();
                    delete interceptorData->jsClass;
                }
                interceptorData->jsClass = nullptr;
            }
            interceptorData->finalized = true;
        }
        JS_FreeValue(qctx, globalExoticInterceptor);
        globalExoticInterceptor = JS_UNDEFINED;
    }

    auto it = std::find(group->contexts.begin(), group->contexts.end(), this);
    if (it != group->contexts.end()) {
        group->contexts.erase(it);
    }
    if (qctx) {
        JS_FreeContext(qctx);
        qctx = nullptr;
    }
    if (group->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete group;
    }
}

OpaqueJSPropertyNameArray::~OpaqueJSPropertyNameArray() {
    for (auto* str : names) {
        if (str) JSStringRelease_nid_postfix(str);
    }
}

OpaqueJSPropertyNameAccumulator::~OpaqueJSPropertyNameAccumulator() {
    for (auto* str : names) {
        if (str) JSStringRelease_nid_postfix(str);
    }
}

void OpaqueJSPropertyNameAccumulator::AddName(JSStringRef name) {
    if (!name) return;
    const std::string utf8 = JsStringToUtf8(name);
    if (seen.insert(utf8).second) {
        names.push_back(JSStringRetain_nid_postfix(name));
    }
}

OpaqueJSScript::~OpaqueJSScript() {
    if (group) {
        if (group->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete group;
        }
    }
}

OpaqueJSWeakObjectMap::~OpaqueJSWeakObjectMap() {
    if (destroyedCallback) {
        destroyedCallback(this, privateData);
    }
    if (group) {
        auto it = std::find(group->weakMaps.begin(), group->weakMaps.end(), this);
        if (it != group->weakMaps.end()) {
            group->weakMaps.erase(it);
        }
    }
}

}  // namespace NKWebKit

extern "C" {
using namespace NKWebKit;

JSContextGroupRef APS5_VABI JSContextGroupCreate() {
    return new OpaqueJSContextGroup();
}

JSContextGroupRef APS5_VABI JSContextGroupRetain(JSContextGroupRef group) {
    if (!group) return nullptr;
    const_cast<OpaqueJSContextGroup*>(group)->refCount.fetch_add(1, std::memory_order_relaxed);
    return group;
}

void APS5_VABI JSContextGroupRelease(JSContextGroupRef group) {
    if (!group) return;
    auto* mutGroup = const_cast<OpaqueJSContextGroup*>(group);
    if (mutGroup->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete mutGroup;
    }
}

void APS5_VABI JSContextGroupSetExecutionTimeLimit(JSContextGroupRef group, double limit, JSShouldTerminateCallback callback, void* callbackContext) {
    if (!group) return;
    auto* mutGroup = const_cast<OpaqueJSContextGroup*>(group);
    mutGroup->timeLimitSeconds = limit;
    mutGroup->terminateCallback = callback;
    mutGroup->terminateContext = callbackContext;
    mutGroup->hasTimeLimit = true;
    mutGroup->RefreshDeadline();
}

void APS5_VABI JSContextGroupClearExecutionTimeLimit(JSContextGroupRef group) {
    if (!group) return;
    auto* mutGroup = const_cast<OpaqueJSContextGroup*>(group);
    mutGroup->hasTimeLimit = false;
    mutGroup->terminateCallback = nullptr;
    mutGroup->terminateContext = nullptr;
}

void APS5_VABI JSContextGroupAddHeapFinalizer(JSContextGroupRef group, JSHeapFinalizer finalizer, void* userData) {
    if (!group || !finalizer) return;
    auto* mutGroup = const_cast<OpaqueJSContextGroup*>(group);
    const std::pair<JSHeapFinalizer, void*> entry{finalizer, userData};
    if (std::find(mutGroup->heapFinalizers.begin(), mutGroup->heapFinalizers.end(), entry) == mutGroup->heapFinalizers.end()) {
        mutGroup->heapFinalizers.push_back(entry);
    }
}

void APS5_VABI JSContextGroupRemoveHeapFinalizer(JSContextGroupRef group, JSHeapFinalizer finalizer, void* userData) {
    if (!group || !finalizer) return;
    auto* mutGroup = const_cast<OpaqueJSContextGroup*>(group);
    const std::pair<JSHeapFinalizer, void*> entry{finalizer, userData};
    auto it = std::find(mutGroup->heapFinalizers.begin(), mutGroup->heapFinalizers.end(), entry);
    if (it != mutGroup->heapFinalizers.end()) {
        mutGroup->heapFinalizers.erase(it);
    }
}

void APS5_VABI JSContextGroupAddMarkingConstraint(JSContextGroupRef group, JSMarkingConstraint constraintFunction, void* userData) {
    if (!group || !constraintFunction) return;
    auto* mutGroup = const_cast<OpaqueJSContextGroup*>(group);
    mutGroup->markingConstraints.emplace_back(constraintFunction, userData);
}

void APS5_VABI JSGarbageCollect(JSContextRef ctx) {
    if (!ctx || !ctx->group) return;
    ctx->group->CollectGarbage();
}

void APS5_VABI JSSynchronousGarbageCollectForDebugging(JSContextRef ctx) {
    if (!ctx || !ctx->group) return;
    ctx->group->CollectGarbage();
}

void APS5_VABI JSSynchronousEdenCollectForDebugging(JSContextRef ctx) {
    if (!ctx || !ctx->group) return;
    ctx->group->CollectGarbage();
}

void APS5_VABI JSReportExtraMemoryCost(JSContextRef ctx, std::size_t size) {
    if (!ctx || !ctx->group) return;
    ctx->group->extraMemorySize += size;
}

void APS5_VABI JSDisableGCTimer() {
}

JSObjectRef APS5_VABI JSGetMemoryUsageStatistics(JSContextRef ctx) {
    if (!ctx || !ctx->group) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    OpaqueJSContextGroup* group = mutCtx->group;
    JSMemoryUsage usage{};
    JS_ComputeMemoryUsage(group->rt, &usage);

    std::size_t protectedObjects = 0;
    std::size_t protectedGlobals = 0;
    std::unordered_map<std::string, std::size_t> typeCounts;

    for (const auto& [ptr, handle] : group->objectHandles) {
        if (!handle || !handle->alive) continue;
        if (handle->protectCount > 0) {
            ++protectedObjects;
        }
        std::string className = "Object";
        if (auto* cbData = group->FindCallbackData(handle)) {
            if (cbData->jsClass && !cbData->jsClass->className.empty()) {
                className = cbData->jsClass->className;
            }
        } else if (JS_IsArray(handle->qval)) {
            className = "Array";
        } else if (JS_IsFunction(mutCtx->qctx, handle->qval)) {
            className = "Function";
        }
        ++typeCounts[className];
    }
    for (auto* c : group->contexts) {
        if (c && c->globalObject && c->globalObject->protectCount > 0) {
            ++protectedGlobals;
        }
    }

    JSValue statsObj = JS_NewObject(mutCtx->qctx);
    JS_DefinePropertyValueStr(mutCtx->qctx, statsObj, "heapSize", JS_NewFloat64(mutCtx->qctx, static_cast<double>(usage.memory_used_size)), JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(mutCtx->qctx, statsObj, "heapCapacity", JS_NewFloat64(mutCtx->qctx, static_cast<double>(usage.malloc_size)), JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(mutCtx->qctx, statsObj, "extraMemorySize", JS_NewFloat64(mutCtx->qctx, static_cast<double>(group->extraMemorySize)), JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(mutCtx->qctx, statsObj, "objectCount", JS_NewFloat64(mutCtx->qctx, static_cast<double>(usage.obj_count)), JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(mutCtx->qctx, statsObj, "protectedObjectCount", JS_NewFloat64(mutCtx->qctx, static_cast<double>(protectedObjects)), JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(mutCtx->qctx, statsObj, "globalObjectCount", JS_NewFloat64(mutCtx->qctx, static_cast<double>(group->contexts.size())), JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(mutCtx->qctx, statsObj, "protectedGlobalObjectCount", JS_NewFloat64(mutCtx->qctx, static_cast<double>(protectedGlobals)), JS_PROP_C_W_E);

    JSValue typeCountsObj = JS_NewObject(mutCtx->qctx);
    for (const auto& [typeName, count] : typeCounts) {
        JS_DefinePropertyValueStr(mutCtx->qctx, typeCountsObj, typeName.c_str(), JS_NewFloat64(mutCtx->qctx, static_cast<double>(count)), JS_PROP_C_W_E);
    }
    JS_DefinePropertyValueStr(mutCtx->qctx, statsObj, "objectTypeCounts", typeCountsObj, JS_PROP_C_W_E);

    return group->ToHandle(mutCtx, statsObj);
}

JSGlobalContextRef APS5_VABI JSGlobalContextCreateInGroup(JSContextGroupRef group, JSClassRef globalObjectClass) {
    OpaqueJSContextGroup* mutGroup = const_cast<OpaqueJSContextGroup*>(group);
    bool createdGroup = false;
    if (!mutGroup) {
        mutGroup = new OpaqueJSContextGroup();
        createdGroup = true;
    }
    auto* ctx = new OpaqueJSContext(mutGroup, globalObjectClass);
    if (createdGroup) {
        JSContextGroupRelease(mutGroup);
    }
    return ctx;
}

JSGlobalContextRef APS5_VABI JSGlobalContextCreate(JSClassRef globalObjectClass) {
    return JSGlobalContextCreateInGroup(nullptr, globalObjectClass);
}

JSGlobalContextRef APS5_VABI JSGlobalContextRetain(JSGlobalContextRef ctx) {
    if (!ctx) return nullptr;
    ctx->refCount.fetch_add(1, std::memory_order_relaxed);
    return ctx;
}

void APS5_VABI JSGlobalContextRelease(JSGlobalContextRef ctx) {
    if (!ctx) return;
    if (ctx->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete ctx;
    }
}

JSObjectRef APS5_VABI JSContextGetGlobalObject(JSContextRef ctx) {
    if (!ctx) return nullptr;
    return ctx->globalObject;
}

JSContextGroupRef APS5_VABI JSContextGetGroup(JSContextRef ctx) {
    if (!ctx) return nullptr;
    return ctx->group;
}

JSGlobalContextRef APS5_VABI JSContextGetGlobalContext(JSContextRef ctx) {
    if (!ctx) return nullptr;
    return const_cast<JSGlobalContextRef>(ctx);
}

JSStringRef APS5_VABI JSGlobalContextCopyName(JSGlobalContextRef ctx) {
    if (!ctx || !ctx->hasName) return nullptr;
    return Utf8ToJsString(ctx->name);
}

void APS5_VABI JSGlobalContextSetName(JSGlobalContextRef ctx, JSStringRef name) {
    if (!ctx) return;
    if (name && !name->nullString) {
        ctx->hasName = true;
        ctx->name = JsStringToUtf8(name);
    } else {
        ctx->hasName = false;
        ctx->name.clear();
    }
}

bool APS5_VABI JSGlobalContextGetRemoteInspectionEnabled(JSGlobalContextRef ctx) {
    if (!ctx) return false;
    return ctx->remoteInspectionEnabled;
}

void APS5_VABI JSGlobalContextSetRemoteInspectionEnabled(JSGlobalContextRef ctx, bool enabled) {
    if (!ctx) return;
    ctx->remoteInspectionEnabled = enabled;
}

bool APS5_VABI JSGlobalContextGetIncludesNativeCallStackWhenReportingExceptions(JSGlobalContextRef ctx) {
    if (!ctx) return false;
    return ctx->includesNativeCallStack;
}

void APS5_VABI JSGlobalContextSetIncludesNativeCallStackWhenReportingExceptions(JSGlobalContextRef ctx, bool includesNativeCallStack) {
    if (!ctx) return;
    ctx->includesNativeCallStack = includesNativeCallStack;
}

void APS5_VABI JSGlobalContextSetUnhandledRejectionCallback(JSGlobalContextRef ctx, JSObjectRef function, JSValueRef* exception) {
    if (!ctx) return;
    if (!IsObjectCallable(ctx, function)) {
        if (exception) {
            JSValue err = JS_NewTypeError(ctx->qctx, "Unhandled rejection callback must be a function");
            *exception = ctx->group->ToHandle(ctx, err);
        }
        return;
    }
    ctx->unhandledRejectionCallback = function;
}

bool APS5_VABI JSRemoteInspectorGetInspectionEnabledByDefault() {
    return g_remoteInspectionEnabledByDefault;
}

void APS5_VABI JSRemoteInspectorSetInspectionEnabledByDefault(bool enabledByDefault) {
    g_remoteInspectionEnabledByDefault = enabledByDefault;
}

JSValueRef APS5_VABI JSEvaluateScript(JSContextRef ctx, JSStringRef script, JSObjectRef thisObject, JSStringRef sourceURL, int startingLineNumber, JSValueRef* exception) {
    if (!ctx || !script) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    ActiveContextScope activeScope(mutCtx->group, mutCtx);
    if (mutCtx->group->hasTimeLimit) {
        mutCtx->group->RefreshDeadline();
    }

    const std::string code = JsStringToUtf8(script);
    const std::string url = sourceURL ? JsStringToUtf8(sourceURL) : std::string();
    const int clampedLine = startingLineNumber <= 0 ? 1 : startingLineNumber;

    JSEvalOptions options{};
    options.version = JS_EVAL_OPTIONS_VERSION;
    options.eval_flags = JS_EVAL_TYPE_GLOBAL;
    options.filename = url.c_str();
    options.line_num = clampedLine;

    JSValue result;
    if (thisObject && thisObject->alive && thisObject != mutCtx->globalObject) {
        result = JS_EvalThis2(mutCtx->qctx, thisObject->qval, code.c_str(), code.size(), &options);
    } else {
        result = JS_Eval2(mutCtx->qctx, code.c_str(), code.size(), &options);
    }

    if (JS_IsException(result)) {
        CapturePendingException(mutCtx, exception);
        mutCtx->group->DrainMicrotasks(mutCtx);
        return nullptr;
    }

    OpaqueJSValue* resultHandle = mutCtx->group->ToHandle(mutCtx, result);
    mutCtx->group->DrainMicrotasks(mutCtx);
    return resultHandle;
}

bool APS5_VABI JSCheckScriptSyntax(JSContextRef ctx, JSStringRef script, JSStringRef sourceURL, int startingLineNumber, JSValueRef* exception) {
    if (!ctx || !script) return false;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    const std::string code = JsStringToUtf8(script);
    const std::string url = sourceURL ? JsStringToUtf8(sourceURL) : std::string();
    const int clampedLine = startingLineNumber <= 0 ? 1 : startingLineNumber;

    JSEvalOptions options{};
    options.version = JS_EVAL_OPTIONS_VERSION;
    options.eval_flags = JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY;
    options.filename = url.c_str();
    options.line_num = clampedLine;

    JSValue compiled = JS_Eval2(mutCtx->qctx, code.c_str(), code.size(), &options);
    if (JS_IsException(compiled)) {
        CapturePendingException(mutCtx, exception);
        return false;
    }
    JS_FreeValue(mutCtx->qctx, compiled);
    return true;
}

JSScriptRef APS5_VABI JSScriptCreateReferencingImmortalASCIIText(JSContextGroupRef group, JSStringRef url, int startingLineNumber, const char* source, std::size_t length, JSStringRef* errorMessage, int* errorLine) {
    if (!group || (!source && length > 0)) return nullptr;
    for (std::size_t i = 0; i < length; ++i) {
        if (static_cast<unsigned char>(source[i]) & 0x80u) {
            return nullptr;
        }
    }
    return CreateScriptInternal(const_cast<OpaqueJSContextGroup*>(group), url, startingLineNumber, source, length, errorMessage, errorLine);
}

JSScriptRef APS5_VABI JSScriptCreateFromString(JSContextGroupRef group, JSStringRef url, int startingLineNumber, JSStringRef source, JSStringRef* errorMessage, int* errorLine) {
    if (!group || !source) return nullptr;
    const std::string utf8 = JsStringToUtf8(source);
    return CreateScriptInternal(const_cast<OpaqueJSContextGroup*>(group), url, startingLineNumber, utf8.data(), utf8.size(), errorMessage, errorLine);
}

JSValueRef APS5_VABI JSScriptEvaluate(JSContextRef ctx, JSScriptRef script, JSValueRef thisValueRef, JSValueRef* exception) {
    if (!ctx || !script) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    if (script->group != mutCtx->group) return nullptr;
    if (thisValueRef && (!thisValueRef->alive || !thisValueRef->isObjectHandle)) {
        return nullptr;
    }

    ActiveContextScope activeScope(mutCtx->group, mutCtx);
    if (mutCtx->group->hasTimeLimit) {
        mutCtx->group->RefreshDeadline();
    }

    JSEvalOptions options{};
    options.version = JS_EVAL_OPTIONS_VERSION;
    options.eval_flags = JS_EVAL_TYPE_GLOBAL;
    options.filename = script->url.c_str();
    options.line_num = script->startingLineNumber;

    JSValue result;
    if (thisValueRef && thisValueRef != mutCtx->globalObject) {
        result = JS_EvalThis2(mutCtx->qctx, thisValueRef->qval, script->source.c_str(), script->source.size(), &options);
    } else {
        result = JS_Eval2(mutCtx->qctx, script->source.c_str(), script->source.size(), &options);
    }

    if (JS_IsException(result)) {
        CapturePendingException(mutCtx, exception);
        mutCtx->group->DrainMicrotasks(mutCtx);
        return nullptr;
    }

    OpaqueJSValue* resultHandle = mutCtx->group->ToHandle(mutCtx, result);
    mutCtx->group->DrainMicrotasks(mutCtx);
    return resultHandle;
}

void APS5_VABI JSScriptRetain(JSScriptRef script) {
    if (!script) return;
    script->refCount.fetch_add(1, std::memory_order_relaxed);
}

void APS5_VABI JSScriptRelease(JSScriptRef script) {
    if (!script) return;
    if (script->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete script;
    }
}

JSWeakRef APS5_VABI JSWeakCreate(JSContextGroupRef group, JSObjectRef object) {
    if (!group || !object) return nullptr;
    auto* mutGroup = const_cast<OpaqueJSContextGroup*>(group);
    auto* weak = new OpaqueJSWeak();
    weak->group = mutGroup;
    weak->object = object->alive ? object : nullptr;
    mutGroup->weakRefs.push_back(weak);
    return weak;
}

void APS5_VABI JSWeakRetain(JSWeakRef weak) {
    if (!weak) return;
    weak->refCount.fetch_add(1, std::memory_order_relaxed);
}

void APS5_VABI JSWeakRelease(JSWeakRef weak) {
    if (!weak) return;
    if (weak->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        if (weak->group && !weak->group->shuttingDown) {
            auto& vec = weak->group->weakRefs;
            auto it = std::find(vec.begin(), vec.end(), weak);
            if (it != vec.end()) {
                vec.erase(it);
            }
        }
        delete weak;
    }
}

JSObjectRef APS5_VABI JSWeakGetObject(JSWeakRef weak) {
    if (!weak || !weak->object || !weak->object->alive) return nullptr;
    return weak->object;
}

void APS5_VABI JSWeakObjectMapSet(JSContextRef ctx, JSWeakObjectMapRef map, void* key, JSObjectRef object) {
    if (!ctx || !map) return;
    if (!object || !object->alive) {
        map->entries.erase(key);
        return;
    }
    map->entries[key] = object;
}

JSObjectRef APS5_VABI JSWeakObjectMapGet(JSContextRef ctx, JSWeakObjectMapRef map, void* key) {
    if (!ctx || !map) return nullptr;
    auto it = map->entries.find(key);
    if (it == map->entries.end()) return nullptr;
    if (!it->second || !it->second->alive) {
        map->entries.erase(it);
        return nullptr;
    }
    return it->second;
}

bool APS5_VABI JSWeakObjectMapRemove(JSContextRef ctx, JSWeakObjectMapRef map, void* key) {
    if (!ctx || !map) return false;
    auto it = map->entries.find(key);
    if (it == map->entries.end()) return false;
    const bool wasAlive = it->second && it->second->alive;
    map->entries.erase(it);
    return wasAlive;
}

GuestJSString* APS5_VABI JSStringCreateWithCharactersNoCopy(const std::uint16_t* chars, std::size_t numChars) {
    if (!chars && numChars > 0) {
        throw std::invalid_argument("JSStringCreateWithCharactersNoCopy: null characters");
    }
    if (!GuestJSStringValidLength<char16_t>(numChars)) {
        throw std::length_error("JSStringCreateWithCharactersNoCopy: length above the string limit");
    }
    auto string = std::make_unique<GuestJSString>();
    if (chars && numChars > 0) {
        string->characters.assign(chars, chars + numChars);
    }
    string->characters.push_back(0);
    return string.release();
}

void APS5_VABI _ZN14OpaqueJSStringD1Ev(GuestJSString* self) {
    if (!self) return;
    std::destroy_at(&self->characters);
}

void APS5_VABI _ZN14OpaqueJSStringD2Ev(GuestJSString* self) {
    if (!self) return;
    std::destroy_at(&self->characters);
}

}
