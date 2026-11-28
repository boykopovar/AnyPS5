#include "Jsc.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace NKWebKit {

namespace {

constexpr double kMsPerDay = 86400000.0;
constexpr double kMaxTime = 8.64e15;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr const char* kNarrowNoBreakSpace = "\xE2\x80\xAF";

struct DateIntrinsics {
    JSValue constructor = JS_UNDEFINED;
    JSValue setFullYear = JS_UNDEFINED;
    JSValue setHours = JS_UNDEFINED;
    JSValue getTime = JS_UNDEFINED;
};

double TimeClip(double time) {
    if (!(std::fabs(time) <= kMaxTime)) return kNaN;
    return std::trunc(time) + 0.0;
}

bool NumberOf(JSContext* qctx, JSValue value, double* out) {
    if (JS_IsException(value)) return false;
    const int failed = JS_ToFloat64(qctx, out, value);
    JS_FreeValue(qctx, value);
    return failed == 0;
}

bool LocalWallTimeToUtc(JSContext* qctx, const DateIntrinsics& intrinsics, double wall, double* utc) {
    const double fraction = wall - std::trunc(wall);
    wall = std::trunc(wall);
    if (!(std::fabs(wall) <= kMaxTime + 2.0 * kMsPerDay)) {
        *utc = kNaN;
        return true;
    }
    const std::int64_t wallMs = static_cast<std::int64_t>(wall);
    const std::int64_t msPerDay = static_cast<std::int64_t>(kMsPerDay);
    const std::int64_t days = wallMs >= 0 ? wallMs / msPerDay : -((-wallMs + msPerDay - 1) / msPerDay);
    const std::int64_t inDay = wallMs - days * msPerDay;

    const std::int64_t shifted = days + 719468;
    const std::int64_t era = (shifted >= 0 ? shifted : shifted - 146096) / 146097;
    const std::int64_t dayOfEra = shifted - era * 146097;
    const std::int64_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    const std::int64_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const std::int64_t marchMonth = (5 * dayOfYear + 2) / 153;
    const std::int64_t day = dayOfYear - (153 * marchMonth + 2) / 5 + 1;
    const std::int64_t month = marchMonth < 10 ? marchMonth + 2 : marchMonth - 10;
    const std::int64_t year = yearOfEra + era * 400 + (month <= 1 ? 1 : 0);

    JSValue fields[7] = {
        JS_NewFloat64(qctx, static_cast<double>(year)),
        JS_NewFloat64(qctx, static_cast<double>(month)),
        JS_NewFloat64(qctx, static_cast<double>(day)),
        JS_NewFloat64(qctx, static_cast<double>(inDay / 3600000)),
        JS_NewFloat64(qctx, static_cast<double>(inDay / 60000 % 60)),
        JS_NewFloat64(qctx, static_cast<double>(inDay / 1000 % 60)),
        JS_NewFloat64(qctx, static_cast<double>(inDay % 1000)),
    };
    JSValue time = JS_UNDEFINED;
    JSValue date = JS_UNDEFINED;
    if (year < 0 || year > 99) {
        date = JS_CallConstructor(qctx, intrinsics.constructor, 7, fields);
        if (JS_IsException(date)) return false;
        time = JS_Call(qctx, intrinsics.getTime, date, 0, nullptr);
    } else {
        JSValue noon[4] = {JS_NewFloat64(qctx, 2000.0), JS_NewFloat64(qctx, 0.0), JS_NewFloat64(qctx, 1.0), JS_NewFloat64(qctx, 12.0)};
        date = JS_CallConstructor(qctx, intrinsics.constructor, 4, noon);
        if (JS_IsException(date)) return false;
        JSValue dated = JS_Call(qctx, intrinsics.setFullYear, date, 3, fields);
        if (JS_IsException(dated)) {
            JS_FreeValue(qctx, date);
            return false;
        }
        JS_FreeValue(qctx, dated);
        time = JS_Call(qctx, intrinsics.setHours, date, 4, fields + 3);
    }
    JS_FreeValue(qctx, date);
    if (!NumberOf(qctx, time, utc)) return false;
    *utc += fraction;
    return true;
}

bool ParseDateString(JSContext* qctx, const DateIntrinsics& intrinsics, JSValueConst string, double* out) {
    std::size_t length = 0;
    const char* utf8 = JS_ToCStringLen(qctx, &length, string);
    if (!utf8) return false;
    std::string text(utf8, length);
    JS_FreeCString(qctx, utf8);
    for (std::size_t at = text.find(kNarrowNoBreakSpace); at != std::string::npos; at = text.find(kNarrowNoBreakSpace, at + 1)) {
        text.replace(at, 3, " ");
    }

    bool isLocalTime = false;
    double value = _ZN3WTF40parseES5DateFromNullTerminatedCharactersEPKcRb(text.c_str(), isLocalTime);
    if (std::isnan(value)) {
        value = _ZN3WTF37parseDateFromNullTerminatedCharactersEPKcRb(text.c_str(), &isLocalTime);
    }
    if (isLocalTime && std::isfinite(value) && !LocalWallTimeToUtc(qctx, intrinsics, value, &value)) return false;
    *out = TimeClip(value);
    return true;
}

JSValue ToPrimitiveWithoutHint(JSContext* qctx, const OpaqueJSContextGroup* group, JSValueConst value) {
    if (!JS_IsObject(value)) return JS_DupValue(qctx, value);
    JSValue exotic = JS_GetProperty(qctx, value, group->toPrimitiveAtom);
    if (JS_IsException(exotic)) return JS_EXCEPTION;
    if (!JS_IsUndefined(exotic) && !JS_IsNull(exotic)) {
        if (!JS_IsFunction(qctx, exotic)) {
            JS_FreeValue(qctx, exotic);
            return JS_ThrowTypeError(qctx, "Symbol.toPrimitive is not a function, undefined, or null");
        }
        JSValue hint = JS_NewString(qctx, "default");
        JSValue result = JS_Call(qctx, exotic, value, 1, &hint);
        JS_FreeValue(qctx, hint);
        JS_FreeValue(qctx, exotic);
        if (JS_IsObject(result)) {
            JS_FreeValue(qctx, result);
            return JS_ThrowTypeError(qctx, "Symbol.toPrimitive returned an object");
        }
        return result;
    }
    for (const char* name : {"valueOf", "toString"}) {
        JSValue method = JS_GetPropertyStr(qctx, value, name);
        if (JS_IsException(method)) return JS_EXCEPTION;
        if (!JS_IsFunction(qctx, method)) {
            JS_FreeValue(qctx, method);
            continue;
        }
        JSValue result = JS_Call(qctx, method, value, 0, nullptr);
        JS_FreeValue(qctx, method);
        if (!JS_IsObject(result)) return result;
        JS_FreeValue(qctx, result);
    }
    return JS_ThrowTypeError(qctx, "No default value");
}

JSValue DateConstructorCall(JSContext* qctx, JSValueConst funcObj, JSValueConst thisVal, int argc, JSValueConst* argv, int flags) {
    auto* group = static_cast<OpaqueJSContextGroup*>(JS_GetRuntimeOpaque(JS_GetRuntime(qctx)));
    const auto* intrinsics = static_cast<const DateIntrinsics*>(JS_GetOpaque(funcObj, group->dateConstructorClassId));
    if (!(flags & JS_CALL_FLAG_CONSTRUCTOR)) return JS_Call(qctx, intrinsics->constructor, JS_UNDEFINED, 0, nullptr);
    if (argc != 1 || JS_IsDate(argv[0])) return JS_CallConstructor2(qctx, intrinsics->constructor, thisVal, argc, argv);

    JSValue argument = ToPrimitiveWithoutHint(qctx, group, argv[0]);
    if (JS_IsException(argument)) return JS_EXCEPTION;
    if (JS_IsString(argument)) {
        double time = kNaN;
        const bool parsed = ParseDateString(qctx, *intrinsics, argument, &time);
        JS_FreeValue(qctx, argument);
        if (!parsed) return JS_EXCEPTION;
        argument = JS_NewFloat64(qctx, time);
    }
    JSValue made = JS_CallConstructor2(qctx, intrinsics->constructor, thisVal, 1, &argument);
    JS_FreeValue(qctx, argument);
    return made;
}

JSValue DateParse(JSContext* qctx, JSValueConst thisVal, int argc, JSValueConst* argv, int magic, JSValueConst* data) {
    (void)thisVal;
    (void)magic;
    JSValue string = JS_ToString(qctx, argc > 0 ? argv[0] : JS_UNDEFINED);
    if (JS_IsException(string)) return JS_EXCEPTION;
    const DateIntrinsics intrinsics{data[0], data[1], data[2], data[3]};
    double time = kNaN;
    const bool parsed = ParseDateString(qctx, intrinsics, string, &time);
    JS_FreeValue(qctx, string);
    return parsed ? JS_NewFloat64(qctx, time) : JS_EXCEPTION;
}

void DateConstructorFinalizer(JSRuntime* rt, JSValueConst value) {
    auto* group = static_cast<OpaqueJSContextGroup*>(JS_GetRuntimeOpaque(rt));
    auto* intrinsics = static_cast<DateIntrinsics*>(JS_GetOpaque(value, group->dateConstructorClassId));
    if (!intrinsics) return;
    JS_FreeValueRT(rt, intrinsics->constructor);
    JS_FreeValueRT(rt, intrinsics->setFullYear);
    JS_FreeValueRT(rt, intrinsics->setHours);
    JS_FreeValueRT(rt, intrinsics->getTime);
    delete intrinsics;
}

void DateConstructorGcMark(JSRuntime* rt, JSValueConst value, JS_MarkFunc* markFunc) {
    auto* group = static_cast<OpaqueJSContextGroup*>(JS_GetRuntimeOpaque(rt));
    const auto* intrinsics = static_cast<const DateIntrinsics*>(JS_GetOpaque(value, group->dateConstructorClassId));
    if (!intrinsics) return;
    JS_MarkValue(rt, intrinsics->constructor, markFunc);
    JS_MarkValue(rt, intrinsics->setFullYear, markFunc);
    JS_MarkValue(rt, intrinsics->setHours, markFunc);
    JS_MarkValue(rt, intrinsics->getTime, markFunc);
}

}  // namespace

void InstallDateConstructor(OpaqueJSContextGroup* group, JSContext* qctx) {
    if (!group->dateConstructorClassId) {
        JS_NewClassID(group->rt, &group->dateConstructorClassId);
        JSClassDef definition{};
        definition.class_name = "Function";
        definition.finalizer = &DateConstructorFinalizer;
        definition.gc_mark = &DateConstructorGcMark;
        definition.call = &DateConstructorCall;
        JS_NewClass(group->rt, group->dateConstructorClassId, &definition);
    }

    JSValue global = JS_GetGlobalObject(qctx);
    JSValue native = JS_GetPropertyStr(qctx, global, "Date");
    JSValue prototype = JS_GetPropertyStr(qctx, native, "prototype");
    auto* intrinsics = new DateIntrinsics{JS_DupValue(qctx, native), JS_GetPropertyStr(qctx, prototype, "setFullYear"), JS_GetPropertyStr(qctx, prototype, "setHours"), JS_GetPropertyStr(qctx, prototype, "getTime")};

    JSValue functionPrototype = JS_GetFunctionProto(qctx);
    JSValue constructor = JS_NewObjectProtoClass(qctx, functionPrototype, group->dateConstructorClassId);
    JS_FreeValue(qctx, functionPrototype);
    JS_SetOpaque(constructor, intrinsics);
    JS_SetConstructorBit(qctx, constructor, true);

    JSValue parseData[4] = {intrinsics->constructor, intrinsics->setFullYear, intrinsics->setHours, intrinsics->getTime};
    JSValue parse = JS_NewCFunctionData(qctx, &DateParse, 1, 0, 4, parseData);
    JS_DefinePropertyValueStr(qctx, parse, "name", JS_NewString(qctx, "parse"), JS_PROP_CONFIGURABLE);

    JS_DefinePropertyValueStr(qctx, constructor, "length", JS_NewInt32(qctx, 7), JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValueStr(qctx, constructor, "name", JS_NewString(qctx, "Date"), JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValueStr(qctx, constructor, "prototype", JS_DupValue(qctx, prototype), 0);
    JS_DefinePropertyValueStr(qctx, constructor, "parse", parse, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValueStr(qctx, constructor, "UTC", JS_GetPropertyStr(qctx, native, "UTC"), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValueStr(qctx, constructor, "now", JS_GetPropertyStr(qctx, native, "now"), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValueStr(qctx, prototype, "constructor", JS_DupValue(qctx, constructor), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValueStr(qctx, global, "Date", constructor, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

    JS_FreeValue(qctx, prototype);
    JS_FreeValue(qctx, native);
    JS_FreeValue(qctx, global);
}

}  // namespace NKWebKit
