#include "Jsc.hpp"
#include "prx/libSceNKWebKit/include/JscExports.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

extern "C" {
using namespace NKWebKit;

JSType APS5_VABI JSValueGetType(JSContextRef ctx, JSValueRef value) {
    (void)ctx;
    if (!value) return kJSTypeNull;
    if (!value->alive) throw std::invalid_argument("JSValueGetType: released value");
    const int tag = JS_VALUE_GET_NORM_TAG(value->qval);
    switch (tag) {
    case JS_TAG_UNDEFINED:
        return kJSTypeUndefined;
    case JS_TAG_NULL:
        return kJSTypeNull;
    case JS_TAG_BOOL:
        return kJSTypeBoolean;
    case JS_TAG_INT:
    case JS_TAG_FLOAT64:
        return kJSTypeNumber;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        return kJSTypeString;
    case JS_TAG_SYMBOL:
        return kJSTypeSymbol;
    default:
        return kJSTypeObject;
    }
}

JSTypedArrayType APS5_VABI JSValueGetTypedArrayType(JSContextRef ctx, JSValueRef value, JSValueRef* exception) {
    (void)exception;
    if (!ctx || !value || !value->alive || !value->isObjectHandle) return kJSTypedArrayTypeNone;
    if (JS_IsArrayBuffer(value->qval)) return kJSTypedArrayTypeArrayBuffer;
    const int qtype = JS_GetTypedArrayType(value->qval);
    switch (qtype) {
    case JS_TYPED_ARRAY_INT8:
        return kJSTypedArrayTypeInt8Array;
    case JS_TYPED_ARRAY_INT16:
        return kJSTypedArrayTypeInt16Array;
    case JS_TYPED_ARRAY_INT32:
        return kJSTypedArrayTypeInt32Array;
    case JS_TYPED_ARRAY_UINT8:
        return kJSTypedArrayTypeUint8Array;
    case JS_TYPED_ARRAY_UINT8C:
        return kJSTypedArrayTypeUint8ClampedArray;
    case JS_TYPED_ARRAY_UINT16:
        return kJSTypedArrayTypeUint16Array;
    case JS_TYPED_ARRAY_UINT32:
        return kJSTypedArrayTypeUint32Array;
    case JS_TYPED_ARRAY_FLOAT32:
        return kJSTypedArrayTypeFloat32Array;
    case JS_TYPED_ARRAY_FLOAT64:
        return kJSTypedArrayTypeFloat64Array;
    case JS_TYPED_ARRAY_BIG_INT64:
        return kJSTypedArrayTypeBigInt64Array;
    case JS_TYPED_ARRAY_BIG_UINT64:
        return kJSTypedArrayTypeBigUint64Array;
    default:
        return kJSTypedArrayTypeNone;
    }
}

bool APS5_VABI JSValueIsUndefined(JSContextRef ctx, JSValueRef value) {
    return JSValueGetType(ctx, value) == kJSTypeUndefined;
}

bool APS5_VABI JSValueIsNull(JSContextRef ctx, JSValueRef value) {
    return JSValueGetType(ctx, value) == kJSTypeNull;
}

bool APS5_VABI JSValueIsBoolean(JSContextRef ctx, JSValueRef value) {
    return JSValueGetType(ctx, value) == kJSTypeBoolean;
}

bool APS5_VABI JSValueIsNumber(JSContextRef ctx, JSValueRef value) {
    return JSValueGetType(ctx, value) == kJSTypeNumber;
}

bool APS5_VABI JSValueIsString(JSContextRef ctx, JSValueRef value) {
    return JSValueGetType(ctx, value) == kJSTypeString;
}

bool APS5_VABI JSValueIsSymbol(JSContextRef ctx, JSValueRef value) {
    return JSValueGetType(ctx, value) == kJSTypeSymbol;
}

bool APS5_VABI JSValueIsObject(JSContextRef ctx, JSValueRef value) {
    return JSValueGetType(ctx, value) == kJSTypeObject && JS_IsObject(value->qval);
}

bool APS5_VABI JSValueIsObjectOfClass(JSContextRef ctx, JSValueRef value, JSClassRef jsClass) {
    if (!ctx || !value || !jsClass || !value->alive || !value->isObjectHandle) return false;
    CallbackObjectData* data = ctx->group->FindCallbackData(value);
    if (!data || data->isDirectConstructor) return false;
    for (OpaqueJSClass* cls = data->jsClass; cls; cls = cls->parentClass) {
        if (cls == jsClass) return true;
    }
    return false;
}

bool APS5_VABI JSValueIsArray(JSContextRef ctx, JSValueRef value) {
    if (!ctx || !value || !value->alive || !value->isObjectHandle) return false;
    return JS_IsArray(value->qval);
}

bool APS5_VABI JSValueIsDate(JSContextRef ctx, JSValueRef value) {
    (void)ctx;
    if (!value || !value->alive || !value->isObjectHandle) return false;
    return JS_IsDate(value->qval);
}

bool APS5_VABI JSValueIsEqual(JSContextRef ctx, JSValueRef a, JSValueRef b, JSValueRef* exception) {
    if (!ctx || (a && !a->alive) || (b && !b->alive)) return false;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    const int res = JS_IsEqual(mutCtx->qctx, ValueOrNull(a), ValueOrNull(b));
    if (res < 0) {
        CapturePendingException(mutCtx, exception);
        return false;
    }
    return res != 0;
}

bool APS5_VABI JSValueIsStrictEqual(JSContextRef ctx, JSValueRef a, JSValueRef b) {
    if ((a && !a->alive) || (b && !b->alive)) return false;
    if (!ctx && (!a || !b)) return false;
    JSContext* qctx = ctx ? ctx->qctx : (a->ctx ? a->ctx->qctx : (a->group ? a->group->utilityCtx : nullptr));
    return JS_IsStrictEqual(qctx, ValueOrNull(a), ValueOrNull(b));
}

bool APS5_VABI JSValueIsInstanceOfConstructor(JSContextRef ctx, JSValueRef value, JSObjectRef constructor, JSValueRef* exception) {
    if (!ctx || !constructor || (value && !value->alive) || !constructor->alive) return false;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    const CallbackObjectData* data = mutCtx->group->FindCallbackData(constructor);
    if (!data && !JS_IsFunction(mutCtx->qctx, constructor->qval)) return false;
    if (data && data->jsClass && !data->isDirectFunction && !data->isDirectConstructor) {
        JSValue method = JS_GetProperty(mutCtx->qctx, constructor->qval, mutCtx->group->hasInstanceAtom);
        if (JS_IsException(method)) {
            CapturePendingException(mutCtx, exception);
            return false;
        }
        const bool absent = JS_IsUndefined(method) || JS_IsNull(method);
        JS_FreeValue(mutCtx->qctx, method);
        if (absent) return false;
    }
    const int res = JS_IsInstanceOf(mutCtx->qctx, ValueOrNull(value), constructor->qval);
    if (res < 0) {
        CapturePendingException(mutCtx, exception);
        return false;
    }
    return res != 0;
}

JSValueRef APS5_VABI JSValueMakeUndefined(JSContextRef ctx) {
    if (!ctx || !ctx->group) return nullptr;
    return &ctx->group->undefinedHandle;
}

JSValueRef APS5_VABI JSValueMakeNull(JSContextRef ctx) {
    if (!ctx || !ctx->group) return nullptr;
    return &ctx->group->nullHandle;
}

JSValueRef APS5_VABI JSValueMakeBoolean(JSContextRef ctx, bool boolean) {
    if (!ctx || !ctx->group) return nullptr;
    return boolean ? &ctx->group->trueHandle : &ctx->group->falseHandle;
}

JSValueRef APS5_VABI JSValueMakeNumber(JSContextRef ctx, double number) {
    if (!ctx || !ctx->group) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    const double clean = std::isnan(number) ? std::numeric_limits<double>::quiet_NaN() : number;
    return mutCtx->group->ToHandle(mutCtx, JS_NewFloat64(mutCtx->qctx, clean));
}

JSValueRef APS5_VABI JSValueMakeSymbol(JSContextRef ctx, JSStringRef description) {
    if (!ctx || !ctx->group) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    JSValue sym;
    if (!description) {
        sym = JS_NewSymbol(mutCtx->qctx, nullptr, false);
    } else {
        const std::string desc = JsStringToUtf8(description);
        sym = JS_NewSymbol(mutCtx->qctx, desc.c_str(), false);
    }
    if (JS_IsException(sym)) {
        JS_FreeValue(mutCtx->qctx, JS_GetException(mutCtx->qctx));
        return nullptr;
    }
    return mutCtx->group->ToHandle(mutCtx, sym);
}

JSValueRef APS5_VABI JSValueMakeFromJSONString(JSContextRef ctx, JSStringRef string) {
    if (!ctx || !string) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    const std::string utf8 = JsStringToUtf8(string);
    JSValue parsed = JS_ParseJSON(mutCtx->qctx, utf8.c_str(), utf8.size(), "<JSON>");
    if (JS_IsException(parsed)) {
        JS_FreeValue(mutCtx->qctx, JS_GetException(mutCtx->qctx));
        return nullptr;
    }
    return mutCtx->group->ToHandle(mutCtx, parsed);
}

JSStringRef APS5_VABI JSValueCreateJSONString(JSContextRef ctx, JSValueRef value, unsigned indent, JSValueRef* exception) {
    if (!ctx || (value && !value->alive)) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    JSValue space = JS_NewInt32(mutCtx->qctx, static_cast<std::int32_t>(std::min(indent, 10u)));
    JSValue strVal = JS_JSONStringify(mutCtx->qctx, ValueOrNull(value), JS_UNDEFINED, space);
    JS_FreeValue(mutCtx->qctx, space);
    if (exception) *exception = nullptr;
    if (JS_IsException(strVal)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    if (JS_IsUndefined(strVal)) {
        return nullptr;
    }
    GuestJSString* res = JsValueToJsString(mutCtx->qctx, strVal);
    JS_FreeValue(mutCtx->qctx, strVal);
    return res;
}

bool APS5_VABI JSValueToBoolean(JSContextRef ctx, JSValueRef value) {
    if (!ctx || !value || !value->alive) return false;
    const int res = JS_ToBool(ctx->qctx, value->qval);
    return res > 0;
}

double APS5_VABI JSValueToNumber(JSContextRef ctx, JSValueRef value, JSValueRef* exception) {
    if (!ctx || (value && !value->alive)) return std::numeric_limits<double>::quiet_NaN();
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    double num = std::numeric_limits<double>::quiet_NaN();
    if (JS_ToFloat64(mutCtx->qctx, &num, ValueOrNull(value)) < 0) {
        CapturePendingException(mutCtx, exception);
        return std::numeric_limits<double>::quiet_NaN();
    }
    return num;
}

JSStringRef APS5_VABI JSValueToStringCopy(JSContextRef ctx, JSValueRef value, JSValueRef* exception) {
    if (!ctx || (value && !value->alive)) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    JSValue strVal = JS_ToString(mutCtx->qctx, ValueOrNull(value));
    if (JS_IsException(strVal)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    GuestJSString* res = JsValueToJsString(mutCtx->qctx, strVal);
    JS_FreeValue(mutCtx->qctx, strVal);
    return res;
}

JSObjectRef APS5_VABI JSValueToObject(JSContextRef ctx, JSValueRef value, JSValueRef* exception) {
    if (!ctx || (value && !value->alive)) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    if (value && value->isObjectHandle) {
        return const_cast<JSObjectRef>(value);
    }
    JSValue objVal = JS_ToObject(mutCtx->qctx, ValueOrNull(value));
    if (JS_IsException(objVal)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    return mutCtx->group->ToHandle(mutCtx, objVal);
}

void APS5_VABI JSValueProtect(JSContextRef ctx, JSValueRef value) {
    if (!ctx || !value) return;
    auto* mutVal = const_cast<OpaqueJSValue*>(value);
    ++mutVal->protectCount;
}

void APS5_VABI JSValueUnprotect(JSContextRef ctx, JSValueRef value) {
    if (!ctx || !value) return;
    auto* mutVal = const_cast<OpaqueJSValue*>(value);
    if (mutVal->protectCount > 0) {
        --mutVal->protectCount;
    }
}

}
