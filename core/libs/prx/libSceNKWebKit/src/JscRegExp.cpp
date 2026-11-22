#include "Jsc.hpp"
#include <cstdint>

namespace NKWebKit {

namespace {

enum Slot : std::uint32_t {
    kInput,
    kMatchInput,
    kCaptures,
    kCaptureCount,
    kStart,
    kEnd,
    kMultiline,
};

enum Property : int {
    kLastMatch = 0,
    kLastBackref = 9,
    kLastParen,
    kLeftContextProperty,
    kRightContextProperty,
    kInputProperty,
    kMultilineProperty,
};

enum AccessorData : int {
    kState,
    kSubstring,
    kConstructor,
};

struct LegacyProperty {
    const char* name;
    int property;
    bool writable;
};

constexpr LegacyProperty kProperties[] = {
    {"input", kInputProperty, true},
    {"$_", kInputProperty, true},
    {"multiline", kMultilineProperty, true},
    {"$*", kMultilineProperty, true},
    {"lastMatch", kLastMatch, false},
    {"$&", kLastMatch, false},
    {"lastParen", kLastParen, false},
    {"$+", kLastParen, false},
    {"leftContext", kLeftContextProperty, false},
    {"$`", kLeftContextProperty, false},
    {"rightContext", kRightContextProperty, false},
    {"$'", kRightContextProperty, false},
    {"$1", 1, false},
    {"$2", 2, false},
    {"$3", 3, false},
    {"$4", 4, false},
    {"$5", 5, false},
    {"$6", 6, false},
    {"$7", 7, false},
    {"$8", 8, false},
    {"$9", 9, false},
};

void SetSlot(JSContext* qctx, JSValueConst state, Slot slot, JSValue value) {
    JS_DefinePropertyValueUint32(qctx, state, slot, value, JS_PROP_C_W_E);
}

bool Int64Of(JSContext* qctx, JSValue value, std::int64_t* out) {
    if (JS_IsException(value)) return false;
    const int failed = JS_ToInt64(qctx, out, value);
    JS_FreeValue(qctx, value);
    return failed == 0;
}

bool RecordMatch(JSContext* qctx, JSValueConst state, JSValueConst result) {
    std::int64_t count = 0;
    std::int64_t start = 0;
    std::int64_t matchLength = 0;
    if (JS_GetLength(qctx, result, &count) < 0) return false;
    if (!Int64Of(qctx, JS_GetPropertyStr(qctx, result, "index"), &start)) return false;
    JSValue captures = JS_NewObjectProto(qctx, JS_NULL);
    for (std::int64_t i = 0; i < count; ++i) {
        JSValue capture = JS_GetPropertyUint32(qctx, result, static_cast<std::uint32_t>(i));
        if (i == 0 && !Int64Of(qctx, JS_GetPropertyStr(qctx, capture, "length"), &matchLength)) {
            JS_FreeValue(qctx, capture);
            JS_FreeValue(qctx, captures);
            return false;
        }
        JS_DefinePropertyValueUint32(qctx, captures, static_cast<std::uint32_t>(i), capture, JS_PROP_C_W_E);
    }
    JSValue input = JS_GetPropertyStr(qctx, result, "input");
    SetSlot(qctx, state, kInput, JS_DupValue(qctx, input));
    SetSlot(qctx, state, kMatchInput, input);
    SetSlot(qctx, state, kCaptures, captures);
    SetSlot(qctx, state, kCaptureCount, JS_NewInt64(qctx, count));
    SetSlot(qctx, state, kStart, JS_NewInt64(qctx, start));
    SetSlot(qctx, state, kEnd, JS_NewInt64(qctx, start + matchLength));
    return true;
}

JSValue Capture(JSContext* qctx, JSValueConst state, std::int64_t index) {
    std::int64_t count = 0;
    if (!Int64Of(qctx, JS_GetPropertyUint32(qctx, state, kCaptureCount), &count)) return JS_EXCEPTION;
    if (index < count) {
        JSValue captures = JS_GetPropertyUint32(qctx, state, kCaptures);
        JSValue capture = JS_GetPropertyUint32(qctx, captures, static_cast<std::uint32_t>(index));
        JS_FreeValue(qctx, captures);
        if (!JS_IsUndefined(capture)) return capture;
    }
    return JS_NewString(qctx, "");
}

JSValue LastGroup(JSContext* qctx, JSValueConst state) {
    std::int64_t count = 0;
    if (!Int64Of(qctx, JS_GetPropertyUint32(qctx, state, kCaptureCount), &count)) return JS_EXCEPTION;
    const std::int64_t groups = count - 1;
    return groups > 0 ? Capture(qctx, state, groups) : JS_NewString(qctx, "");
}

JSValue Context(JSContext* qctx, JSValueConst* data, bool left) {
    JSValue input = JS_GetPropertyUint32(qctx, data[kState], kMatchInput);
    JSValue bounds[2] = {JS_NewInt32(qctx, 0), JS_GetPropertyUint32(qctx, data[kState], kStart)};
    if (!left) bounds[0] = JS_GetPropertyUint32(qctx, data[kState], kEnd);
    JSValue context = JS_Call(qctx, data[kSubstring], input, left ? 2 : 1, bounds);
    JS_FreeValue(qctx, bounds[0]);
    JS_FreeValue(qctx, bounds[1]);
    JS_FreeValue(qctx, input);
    return context;
}

bool IsTheConstructor(JSValueConst thisVal, JSValueConst* data) {
    return JS_IsObject(thisVal) && JS_VALUE_GET_PTR(thisVal) == JS_VALUE_GET_PTR(data[kConstructor]);
}

JSValue LegacyGetter(JSContext* qctx, JSValueConst thisVal, int argc, JSValueConst* argv, int magic, JSValueConst* data) {
    (void)argc;
    (void)argv;
    if (!IsTheConstructor(thisVal, data)) {
        if (magic >= 1 && magic <= kLastBackref) return JS_ThrowTypeError(qctx, "RegExp.$N getters require RegExp constructor as |this|");
        if (magic == kInputProperty) return JS_ThrowTypeError(qctx, "RegExp.input getter requires RegExp constructor as |this|");
        const char* name = magic == kMultilineProperty ? "multiline" : magic == kLastMatch ? "lastMatch" : magic == kLastParen ? "lastParen" : magic == kLeftContextProperty ? "leftContext" : "rightContext";
        return JS_ThrowTypeError(qctx, "RegExp.%s getter require RegExp constructor as |this|", name);
    }
    if (magic <= kLastBackref) return Capture(qctx, data[kState], magic);
    if (magic == kLastParen) return LastGroup(qctx, data[kState]);
    if (magic == kLeftContextProperty) return Context(qctx, data, true);
    if (magic == kRightContextProperty) return Context(qctx, data, false);
    return JS_GetPropertyUint32(qctx, data[kState], magic == kInputProperty ? kInput : kMultiline);
}

JSValue LegacySetter(JSContext* qctx, JSValueConst thisVal, int argc, JSValueConst* argv, int magic, JSValueConst* data) {
    JSValueConst value = argc > 0 ? argv[0] : JS_UNDEFINED;
    if (!IsTheConstructor(thisVal, data)) {
        return JS_ThrowTypeError(qctx, "RegExp.%s setters require RegExp constructor as |this|", magic == kInputProperty ? "input" : "multiline");
    }
    if (magic == kMultilineProperty) {
        const int multiline = JS_ToBool(qctx, value);
        if (multiline < 0) return JS_EXCEPTION;
        SetSlot(qctx, data[kState], kMultiline, JS_NewBool(qctx, multiline != 0));
        return JS_UNDEFINED;
    }
    JSValue input = JS_ToString(qctx, value);
    if (JS_IsException(input)) return JS_EXCEPTION;
    SetSlot(qctx, data[kState], kInput, input);
    return JS_UNDEFINED;
}

JSValue RegExpExec(JSContext* qctx, JSValueConst thisVal, int argc, JSValueConst* argv, int magic, JSValueConst* data) {
    (void)magic;
    JSValue result = JS_Call(qctx, data[1], thisVal, argc, argv);
    if (JS_IsObject(result) && !RecordMatch(qctx, data[0], result)) {
        JS_FreeValue(qctx, result);
        return JS_EXCEPTION;
    }
    return result;
}

}  // namespace

void InstallRegExpLegacyProperties(JSContext* qctx) {
    JSValue global = JS_GetGlobalObject(qctx);
    JSValue constructor = JS_GetPropertyStr(qctx, global, "RegExp");
    JSValue prototype = JS_GetPropertyStr(qctx, constructor, "prototype");
    JSValue stringConstructor = JS_GetPropertyStr(qctx, global, "String");
    JSValue stringPrototype = JS_GetPropertyStr(qctx, stringConstructor, "prototype");

    JSValue state = JS_NewObjectProto(qctx, JS_NULL);
    JSValue captures = JS_NewObjectProto(qctx, JS_NULL);
    JS_DefinePropertyValueUint32(qctx, captures, 0, JS_NewString(qctx, ""), JS_PROP_C_W_E);
    SetSlot(qctx, state, kInput, JS_NewString(qctx, ""));
    SetSlot(qctx, state, kMatchInput, JS_NewString(qctx, ""));
    SetSlot(qctx, state, kCaptures, captures);
    SetSlot(qctx, state, kCaptureCount, JS_NewInt32(qctx, 1));
    SetSlot(qctx, state, kStart, JS_NewInt32(qctx, 0));
    SetSlot(qctx, state, kEnd, JS_NewInt32(qctx, 0));
    SetSlot(qctx, state, kMultiline, JS_FALSE);

    JSValue execData[2] = {state, JS_GetPropertyStr(qctx, prototype, "exec")};
    JSValue exec = JS_NewCFunctionData(qctx, &RegExpExec, 1, 0, 2, execData);
    JS_DefinePropertyValueStr(qctx, exec, "name", JS_NewString(qctx, "exec"), JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValueStr(qctx, prototype, "exec", exec, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    JS_FreeValue(qctx, execData[1]);

    JSValue accessorData[3] = {state, JS_GetPropertyStr(qctx, stringPrototype, "substring"), constructor};
    for (const LegacyProperty& property : kProperties) {
        JSValue getter = JS_NewCFunctionData(qctx, &LegacyGetter, 0, property.property, 3, accessorData);
        JSValue setter = property.writable ? JS_NewCFunctionData(qctx, &LegacySetter, 1, property.property, 3, accessorData) : JS_UNDEFINED;
        JSAtom name = JS_NewAtom(qctx, property.name);
        JS_DefinePropertyGetSet(qctx, constructor, name, getter, setter, JS_PROP_CONFIGURABLE);
        JS_FreeAtom(qctx, name);
    }
    JS_FreeValue(qctx, accessorData[1]);

    JS_FreeValue(qctx, state);
    JS_FreeValue(qctx, stringPrototype);
    JS_FreeValue(qctx, stringConstructor);
    JS_FreeValue(qctx, prototype);
    JS_FreeValue(qctx, constructor);
    JS_FreeValue(qctx, global);
}

}  // namespace NKWebKit
