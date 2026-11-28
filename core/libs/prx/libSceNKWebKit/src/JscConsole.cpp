#include "Jsc.hpp"
#include <array>

namespace NKWebKit {

namespace {

struct ConsoleFunction {
    const char* name;
    bool convertsFirstArgument;
};

constexpr std::array<ConsoleFunction, 26> consoleFunctions {{
    {"debug", false},
    {"error", false},
    {"log", false},
    {"info", false},
    {"warn", false},
    {"clear", false},
    {"dir", false},
    {"dirxml", false},
    {"table", false},
    {"trace", false},
    {"assert", false},
    {"count", true},
    {"countReset", true},
    {"profile", true},
    {"profileEnd", true},
    {"time", true},
    {"timeLog", true},
    {"timeEnd", true},
    {"timeStamp", false},
    {"takeHeapSnapshot", true},
    {"group", false},
    {"groupCollapsed", false},
    {"groupEnd", false},
    {"record", false},
    {"recordEnd", false},
    {"screenshot", false},
}};

JSValue CallConsoleFunction(JSContext* qctx, JSValueConst thisVal, int argc, JSValueConst* argv, int convertsFirstArgument) {
    (void)thisVal;
    if (!convertsFirstArgument || argc < 1 || JS_IsUndefined(argv[0])) return JS_UNDEFINED;
    JSValue text = JS_ToString(qctx, argv[0]);
    if (JS_IsException(text)) return JS_EXCEPTION;
    JS_FreeValue(qctx, text);
    return JS_UNDEFINED;
}

}  // namespace

void DefineConsoleObject(OpaqueJSContext* ctx, JSValueConst global) {
    JSContext* qctx = ctx->qctx;
    JSValue prototype = JS_NewObject(qctx);
    JSValue console = JS_NewObjectProto(qctx, prototype);
    JS_FreeValue(qctx, prototype);
    for (const ConsoleFunction& function : consoleFunctions) {
        JSValue callable = JS_NewCFunctionMagic(qctx, &CallConsoleFunction, function.name, 0, JS_CFUNC_generic_magic, function.convertsFirstArgument ? 1 : 0);
        JS_DefinePropertyValueStr(qctx, console, function.name, callable, JS_PROP_C_W_E);
    }
    JS_DefinePropertyValue(qctx, console, ctx->group->toStringTagAtom, JS_NewString(qctx, "console"), JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValueStr(qctx, global, "console", console, JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);
}

}  // namespace NKWebKit
