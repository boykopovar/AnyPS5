#include "Jsc.hpp"
#include <string>
#include <vector>

namespace NKWebKit {

namespace {

constexpr int hiddenFlags = JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE;

struct StackEntry {
    std::string functionName;
    std::string fileName;
    bool hasFunction = false;
    bool hasFileName = false;
    bool isNative = false;
    int line = 0;
    int column = 0;
};

JSValue CallSiteMethod(JSContext* qctx, JSValueConst site, const char* method) {
    JSValue function = JS_GetPropertyStr(qctx, site, method);
    if (JS_IsException(function)) return function;
    JSValue result = JS_Call(qctx, function, site, 0, nullptr);
    JS_FreeValue(qctx, function);
    return result;
}

bool ReadText(JSContext* qctx, JSValueConst site, const char* method, std::string& text, bool& present) {
    JSValue value = CallSiteMethod(qctx, site, method);
    if (JS_IsException(value)) return false;
    present = JS_IsString(value);
    if (present) {
        std::size_t length = 0;
        const char* utf8 = JS_ToCStringLen(qctx, &length, value);
        if (!utf8) {
            JS_FreeValue(qctx, value);
            return false;
        }
        text.assign(utf8, length);
        JS_FreeCString(qctx, utf8);
    }
    JS_FreeValue(qctx, value);
    return true;
}

bool ReadNumber(JSContext* qctx, JSValueConst site, const char* method, int& number) {
    JSValue value = CallSiteMethod(qctx, site, method);
    if (JS_IsException(value)) return false;
    std::int32_t converted = 0;
    const bool ok = JS_ToInt32(qctx, &converted, value) == 0;
    JS_FreeValue(qctx, value);
    number = converted;
    return ok;
}

bool ReadEntry(JSContext* qctx, JSValueConst site, StackEntry& entry) {
    JSValue function = CallSiteMethod(qctx, site, "getFunction");
    if (JS_IsException(function)) return false;
    entry.hasFunction = JS_IsObject(function);
    JS_FreeValue(qctx, function);
    JSValue native = CallSiteMethod(qctx, site, "isNative");
    if (JS_IsException(native)) return false;
    entry.isNative = JS_ToBool(qctx, native) > 0;
    JS_FreeValue(qctx, native);
    bool named = false;
    return ReadText(qctx, site, "getFunctionName", entry.functionName, named)
        && ReadText(qctx, site, "getFileName", entry.fileName, entry.hasFileName)
        && ReadNumber(qctx, site, "getLineNumber", entry.line)
        && ReadNumber(qctx, site, "getColumnNumber", entry.column);
}

bool IsEvaluatedText(const StackEntry& entry) {
    return entry.fileName == "<input>";
}

std::string SourceUrl(const StackEntry& entry) {
    if (entry.isNative) return "[native code]";
    return IsEvaluatedText(entry) ? std::string() : entry.fileName;
}

std::string FrameName(const StackEntry& entry) {
    if (entry.isNative || entry.functionName != "<eval>") return entry.functionName;
    return IsEvaluatedText(entry) ? "eval code" : "global code";
}

std::string FrameText(const StackEntry& entry) {
    const std::string url = SourceUrl(entry);
    std::string text = FrameName(entry) + "@" + url;
    if (url.empty() || entry.isNative) return text;
    return text + ":" + std::to_string(entry.line) + ":" + std::to_string(entry.column);
}

bool Define(JSContext* qctx, JSValueConst error, const char* name, JSValue value, int flags) {
    return JS_DefinePropertyValueStr(qctx, error, name, value, flags) >= 0;
}

JSValue PrepareErrorInfo(JSContext* qctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    (void)thisVal;
    if (argc < 2 || !JS_IsObject(argv[0]) || !JS_IsObject(argv[1])) return JS_UNDEFINED;
    JSValueConst error = argv[0];
    std::int64_t count = 0;
    if (JS_GetLength(qctx, argv[1], &count) < 0) return JS_EXCEPTION;

    std::vector<StackEntry> frames;
    StackEntry parsePosition;
    bool hasParsePosition = false;
    for (std::int64_t index = 0; index < count; ++index) {
        JSValue site = JS_GetPropertyInt64(qctx, argv[1], index);
        if (JS_IsException(site)) return JS_EXCEPTION;
        StackEntry entry;
        const bool read = ReadEntry(qctx, site, entry);
        JS_FreeValue(qctx, site);
        if (!read) return JS_EXCEPTION;
        if (index == 0 && !entry.hasFunction && !entry.isNative) {
            parsePosition = entry;
            hasParsePosition = true;
            continue;
        }
        frames.push_back(std::move(entry));
    }

    JSValue stack = JS_UNDEFINED;
    if (!frames.empty()) {
        int line = 0;
        int column = 0;
        std::string sourceUrl;
        for (const StackEntry& frame : frames) {
            if (frame.isNative) continue;
            line = frame.line;
            column = frame.column;
            sourceUrl = SourceUrl(frame);
            break;
        }
        std::string text;
        for (const StackEntry& frame : frames) {
            if (!text.empty()) text += "\n";
            text += FrameText(frame);
        }
        stack = JS_NewStringLen(qctx, text.data(), text.size());
        if (JS_IsException(stack)) return JS_EXCEPTION;
        bool defined = Define(qctx, error, "line", JS_NewInt32(qctx, line), hiddenFlags)
            && Define(qctx, error, "column", JS_NewInt32(qctx, column), hiddenFlags);
        if (defined && !sourceUrl.empty()) defined = Define(qctx, error, "sourceURL", JS_NewStringLen(qctx, sourceUrl.data(), sourceUrl.size()), hiddenFlags);
        if (defined) defined = Define(qctx, error, "stack", JS_DupValue(qctx, stack), hiddenFlags);
        if (!defined) {
            JS_FreeValue(qctx, stack);
            return JS_EXCEPTION;
        }
    }
    if (hasParsePosition) {
        bool defined = Define(qctx, error, "line", JS_NewInt32(qctx, parsePosition.line), JS_PROP_C_W_E);
        const std::string sourceUrl = SourceUrl(parsePosition);
        if (defined && !sourceUrl.empty()) defined = Define(qctx, error, "sourceURL", JS_NewStringLen(qctx, sourceUrl.data(), sourceUrl.size()), JS_PROP_C_W_E);
        if (!defined) {
            JS_FreeValue(qctx, stack);
            return JS_EXCEPTION;
        }
    }
    return stack;
}

}  // namespace

void InstallErrorInfo(JSContext* qctx, JSValueConst global) {
    JSValue errorConstructor = JS_GetPropertyStr(qctx, global, "Error");
    JS_SetPropertyStr(qctx, errorConstructor, "prepareStackTrace", JS_NewCFunction(qctx, &PrepareErrorInfo, "", 2));
    JSAtom name = JS_NewAtom(qctx, "prepareStackTrace");
    JS_DeleteProperty(qctx, errorConstructor, name, 0);
    JS_FreeAtom(qctx, name);
    JS_SetPropertyStr(qctx, errorConstructor, "stackTraceLimit", JS_NewInt32(qctx, 100));
    JS_FreeValue(qctx, errorConstructor);
}

}  // namespace NKWebKit
