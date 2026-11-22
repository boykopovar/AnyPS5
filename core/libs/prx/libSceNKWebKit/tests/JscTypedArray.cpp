#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <string>

using namespace NKWebKit;

namespace {

constexpr const char* BufferMessage = "JSObjectMakeTypedArrayWithArrayBuffer expects buffer to be an Array Buffer object";

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSC typed array check failed at line %u\n", location.line());
    std::abort();
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct ScopedJsString {
    JSStringRef ref = nullptr;
    explicit ScopedJsString(const char* utf8) : ref(JSStringCreateWithUTF8CString_nid_postfix(utf8)) {}
    ScopedJsString(const ScopedJsString&) = delete;
    ~ScopedJsString() {
        if (ref) JSStringRelease_nid_postfix(ref);
    }
    operator JSStringRef() const { return ref; }
};

JSValueRef Eval(JSContextRef ctx, const char* code, std::source_location location = std::source_location::current()) {
    ScopedJsString script(code);
    JSValueRef exception = nullptr;
    JSValueRef value = JSEvaluateScript(ctx, script, nullptr, nullptr, 1, &exception);
    Require(exception == nullptr && value != nullptr, location);
    return value;
}

bool EvalBool(JSContextRef ctx, const char* code, std::source_location location = std::source_location::current()) {
    return JSValueToBoolean(ctx, Eval(ctx, code, location));
}

JSObjectRef EvalObject(JSContextRef ctx, const char* code, std::source_location location = std::source_location::current()) {
    JSObjectRef object = JSValueToObject(ctx, Eval(ctx, code, location), nullptr);
    Require(object != nullptr, location);
    return object;
}

void SetGlobal(JSContextRef ctx, const char* name, JSValueRef value) {
    ScopedJsString key(name);
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), key, value, kJSPropertyAttributeNone, nullptr);
}

bool IsTypeErrorWithMessage(JSContextRef ctx, JSValueRef error, const char* message) {
    if (!error) return false;
    SetGlobal(ctx, "lastError", error);
    const std::string check = std::string("lastError instanceof TypeError && lastError.message === '") + message + "'";
    return EvalBool(ctx, check.c_str());
}

void RequireView(JSContextRef ctx, JSObjectRef view, std::size_t length, std::size_t byteLength, std::size_t byteOffset,
    std::source_location location = std::source_location::current()) {
    Require(view != nullptr, location);
    Require(JSObjectGetTypedArrayLength(ctx, view, nullptr) == length, location);
    Require(JSObjectGetTypedArrayByteLength(ctx, view, nullptr) == byteLength, location);
    Require(JSObjectGetTypedArrayByteOffset(ctx, view, nullptr) == byteOffset, location);
    Require(JSObjectGetTypedArrayLength(nullptr, view, nullptr) == length, location);
    Require(JSObjectGetTypedArrayByteLength(nullptr, view, nullptr) == byteLength, location);
    Require(JSObjectGetTypedArrayByteOffset(nullptr, view, nullptr) == byteOffset, location);
}

void TestWholeBufferViewRoundsTheLengthDown() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSObjectRef ten = EvalObject(ctx, "globalThis.ten = new ArrayBuffer(10); ten");
    JSValueRef exception = nullptr;
    JSObjectRef int32 = JSObjectMakeTypedArrayWithArrayBuffer(ctx, kJSTypedArrayTypeInt32Array, ten, &exception);
    Require(exception == nullptr);
    RequireView(ctx, int32, 2, 8, 0);
    Require(JSValueGetTypedArrayType(ctx, int32, nullptr) == kJSTypedArrayTypeInt32Array);
    SetGlobal(ctx, "made", int32);
    Require(EvalBool(ctx, "made instanceof Int32Array && made.buffer === ten && made.length === 2"));

    JSObjectRef uint16 = JSObjectMakeTypedArrayWithArrayBuffer(ctx, kJSTypedArrayTypeUint16Array, ten, &exception);
    Require(exception == nullptr);
    RequireView(ctx, uint16, 5, 10, 0);

    JSObjectRef float64 = JSObjectMakeTypedArrayWithArrayBuffer(ctx, kJSTypedArrayTypeFloat64Array, ten, &exception);
    Require(exception == nullptr);
    RequireView(ctx, float64, 1, 8, 0);

    JSObjectRef seven = EvalObject(ctx, "new ArrayBuffer(7)");
    JSObjectRef empty = JSObjectMakeTypedArrayWithArrayBuffer(ctx, kJSTypedArrayTypeFloat64Array, seven, &exception);
    Require(exception == nullptr);
    RequireView(ctx, empty, 0, 0, 0);

    JSObjectRef sixteen = EvalObject(ctx, "new ArrayBuffer(16)");
    JSObjectRef uint32 = JSObjectMakeTypedArrayWithArrayBuffer(ctx, kJSTypedArrayTypeUint32Array, sixteen, &exception);
    Require(exception == nullptr);
    RequireView(ctx, uint32, 4, 16, 0);

    Require(JSObjectMakeTypedArrayWithArrayBuffer(ctx, kJSTypedArrayTypeNone, ten, &exception) == nullptr);
    Require(JSObjectMakeTypedArrayWithArrayBuffer(ctx, kJSTypedArrayTypeArrayBuffer, ten, &exception) == nullptr);
    Require(exception == nullptr);

    JSObjectRef detached = EvalObject(ctx, "var gone = new ArrayBuffer(8); gone.transfer(); gone");
    Require(JSObjectMakeTypedArrayWithArrayBuffer(ctx, kJSTypedArrayTypeUint8Array, detached, &exception) == nullptr);
    Require(exception != nullptr);
    SetGlobal(ctx, "lastError", exception);
    Require(EvalBool(ctx, "lastError instanceof TypeError"));

    JSObjectRef resizable = EvalObject(ctx, "new ArrayBuffer(8, { maxByteLength: 16 })");
    Require(Throws([&] { JSObjectMakeTypedArrayWithArrayBuffer(ctx, kJSTypedArrayTypeUint8Array, resizable, nullptr); }));
    exception = nullptr;
    JSObjectRef partial = JSObjectMakeTypedArrayWithArrayBufferAndOffset(ctx, kJSTypedArrayTypeUint8Array, resizable, 2, 4, &exception);
    Require(exception == nullptr);
    RequireView(ctx, partial, 4, 4, 2);

    JSGlobalContextRelease(ctx);
}

void TestBufferArgumentMessage() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    JSObjectRef plain = EvalObject(ctx, "({})");
    JSObjectRef view = EvalObject(ctx, "new Uint8Array(4)");

    for (JSObjectRef notBuffer : {plain, view}) {
        JSValueRef exception = nullptr;
        Require(JSObjectMakeTypedArrayWithArrayBuffer(ctx, kJSTypedArrayTypeUint8Array, notBuffer, &exception) == nullptr);
        Require(IsTypeErrorWithMessage(ctx, exception, BufferMessage));
        exception = nullptr;
        Require(JSObjectMakeTypedArrayWithArrayBufferAndOffset(ctx, kJSTypedArrayTypeUint8Array, notBuffer, 0, 1, &exception) == nullptr);
        Require(IsTypeErrorWithMessage(ctx, exception, BufferMessage));
    }

    JSGlobalContextRelease(ctx);
}

void TestArrayBufferGettersDeclineQuietly() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSObjectRef buffer = EvalObject(ctx, "new ArrayBuffer(12)");
    JSValueRef exception = nullptr;
    Require(JSObjectGetArrayBufferBytesPtr(ctx, buffer, &exception) != nullptr);
    Require(JSObjectGetArrayBufferByteLength(ctx, buffer, &exception) == 12);
    Require(JSObjectGetArrayBufferByteLength(nullptr, buffer, &exception) == 12);
    Require(exception == nullptr);

    JSObjectRef plain = EvalObject(ctx, "({ byteLength: 5 })");
    JSObjectRef view = EvalObject(ctx, "new Uint8Array(4)");
    JSObjectRef function = EvalObject(ctx, "(function () {})");
    for (JSObjectRef other : {plain, view, function}) {
        Require(JSObjectGetArrayBufferBytesPtr(ctx, other, &exception) == nullptr);
        Require(exception == nullptr);
        Require(JSObjectGetArrayBufferByteLength(ctx, other, &exception) == 0);
        Require(exception == nullptr);
    }

    JSObjectRef detached = EvalObject(ctx, "var gone = new ArrayBuffer(8); gone.transfer(); gone");
    Require(JSObjectGetArrayBufferBytesPtr(ctx, detached, &exception) == nullptr);
    Require(exception == nullptr);
    Require(JSObjectGetArrayBufferByteLength(ctx, detached, &exception) == 0);
    Require(exception == nullptr);
    Require(EvalBool(ctx, "1 + 1 === 2"));

    Require(Throws([&] { JSObjectGetArrayBufferByteLength(ctx, nullptr, nullptr); }));

    JSGlobalContextRelease(ctx);
}

void TestDataViewAnswersTheViewGetters() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSObjectRef buffer = EvalObject(ctx, "globalThis.store = new ArrayBuffer(16); store");
    JSObjectRef dataView = EvalObject(ctx, "globalThis.dv = new DataView(store, 4, 8); dv");
    void* bytes = JSObjectGetArrayBufferBytesPtr(ctx, buffer, nullptr);
    Require(bytes != nullptr);

    JSValueRef exception = nullptr;
    Require(JSValueGetTypedArrayType(ctx, dataView, &exception) == kJSTypedArrayTypeNone);
    RequireView(ctx, dataView, 8, 8, 4);
    Require(JSObjectGetTypedArrayBytesPtr(ctx, dataView, &exception) == bytes);
    JSObjectRef viewBuffer = JSObjectGetTypedArrayBuffer(ctx, dataView, &exception);
    Require(viewBuffer != nullptr && JSValueIsStrictEqual(ctx, viewBuffer, buffer));
    Require(exception == nullptr);

    JSObjectRef whole = EvalObject(ctx, "new DataView(store)");
    RequireView(ctx, whole, 16, 16, 0);

    Eval(ctx,
        "Object.defineProperty(DataView.prototype, 'byteLength', { get() { return 99; } });"
        "Object.defineProperty(DataView.prototype, 'byteOffset', { get() { return 98; } });"
        "Object.defineProperty(DataView.prototype, 'buffer', { get() { return new ArrayBuffer(1); } });"
        "Object.defineProperty(dv, 'byteLength', { value: 97 }); 0");
    Require(EvalBool(ctx, "dv.byteLength === 97 && dv.byteOffset === 98"));
    RequireView(ctx, dataView, 8, 8, 4);
    Require(JSObjectGetTypedArrayBytesPtr(ctx, dataView, &exception) == bytes);
    viewBuffer = JSObjectGetTypedArrayBuffer(ctx, dataView, &exception);
    Require(viewBuffer != nullptr && JSValueIsStrictEqual(ctx, viewBuffer, buffer));
    Require(exception == nullptr);

    JSObjectRef typed = EvalObject(ctx, "globalThis.typed = new Uint16Array(store, 2, 3); typed");
    Eval(ctx, "Object.defineProperty(Object.getPrototypeOf(Uint8Array.prototype), 'buffer', { get() { return new ArrayBuffer(1); } }); 0");
    Require(EvalBool(ctx, "typed.buffer !== store"));
    RequireView(ctx, typed, 3, 6, 2);
    Require(JSObjectGetTypedArrayBytesPtr(ctx, typed, &exception) == bytes);
    viewBuffer = JSObjectGetTypedArrayBuffer(ctx, typed, &exception);
    Require(viewBuffer != nullptr && JSValueIsStrictEqual(ctx, viewBuffer, buffer));
    Require(exception == nullptr);

    JSObjectRef plain = EvalObject(ctx, "({ byteLength: 5, byteOffset: 1, length: 5 })");
    for (JSObjectRef other : {plain, buffer}) {
        RequireView(ctx, other, 0, 0, 0);
        Require(JSObjectGetTypedArrayBytesPtr(ctx, other, &exception) == nullptr);
        Require(JSObjectGetTypedArrayBuffer(ctx, other, &exception) == nullptr);
        Require(exception == nullptr);
    }

    JSGlobalContextRelease(ctx);
}

void TestDetachedBuffersGiveNoException() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSObjectRef buffer = EvalObject(ctx, "globalThis.store = new ArrayBuffer(16); store");
    JSObjectRef typed = EvalObject(ctx, "new Uint32Array(store, 4, 2)");
    JSObjectRef dataView = EvalObject(ctx, "new DataView(store, 4, 8)");
    RequireView(ctx, typed, 2, 8, 4);
    RequireView(ctx, dataView, 8, 8, 4);
    Eval(ctx, "store.transfer(); 0");

    for (JSObjectRef view : {typed, dataView}) {
        JSValueRef exception = nullptr;
        RequireView(ctx, view, 0, 0, 0);
        Require(JSObjectGetTypedArrayBytesPtr(ctx, view, &exception) == nullptr);
        Require(exception == nullptr);
        JSObjectRef viewBuffer = JSObjectGetTypedArrayBuffer(ctx, view, &exception);
        Require(exception == nullptr);
        Require(viewBuffer != nullptr && JSValueIsStrictEqual(ctx, viewBuffer, buffer));
        Require(JSValueGetTypedArrayType(ctx, viewBuffer, nullptr) == kJSTypedArrayTypeArrayBuffer);
        Require(JSObjectGetArrayBufferByteLength(ctx, viewBuffer, nullptr) == 0);
    }
    Require(EvalBool(ctx, "1 + 1 === 2"));

    JSGlobalContextRelease(ctx);
}

}  // namespace

int main() {
    TestWholeBufferViewRoundsTheLengthDown();
    TestBufferArgumentMessage();
    TestArrayBufferGettersDeclineQuietly();
    TestDataViewAnswersTheViewGetters();
    TestDetachedBuffersGiveNoException();
    std::puts("libSceNKWebKit JS typed array tests passed");
    return 0;
}
