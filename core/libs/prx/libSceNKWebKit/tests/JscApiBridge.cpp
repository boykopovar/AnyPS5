#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#include "../src/WtfTextAbi.hpp"

using namespace NKWebKit;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JscApiBridge check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct ScopedJsString {
    JSStringRef ref = nullptr;
    explicit ScopedJsString(const char* utf8) : ref(JSStringCreateWithUTF8CString_nid_postfix(utf8)) {}
    explicit ScopedJsString(JSStringRef r) : ref(r) {}
    ~ScopedJsString() {
        if (ref) JSStringRelease_nid_postfix(ref);
    }
    ScopedJsString(const ScopedJsString&) = delete;
    ScopedJsString& operator=(const ScopedJsString&) = delete;
    operator JSStringRef() const { return ref; }
};

JSValueRef Eval(JSContextRef ctx, const char* code) {
    ScopedJsString script(code);
    JSValueRef exception = nullptr;
    JSValueRef result = JSEvaluateScript(ctx, script, nullptr, nullptr, 1, &exception);
    Require(exception == nullptr);
    return result;
}

JSValueRef MakeString(JSContextRef ctx, JSStringRef string) {
    return static_cast<JSValueRef>(JSValueMakeString(ctx, string));
}

bool HasPropertyForKey(JSContextRef ctx, JSObjectRef object, JSValueRef key, JSValueRef* exception) {
    return JSObjectHasPropertyForKey(ctx, object, key, reinterpret_cast<const void**>(exception));
}

using Builder = NKWebKit::WtfAbi::StringBuilder;
using WtfString = NKWebKit::WtfAbi::String;

Builder NewBuilder() {
    Builder builder{WtfString()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    return builder;
}

void ResizeBuilder(Builder& builder, unsigned newSize) {
    _ZN3WTF13StringBuilder6resizeEj(reinterpret_cast<Jsc::WtfAbi::StringBuilder*>(&builder), newSize);
}

bool ResizeThrows(Builder* builder, unsigned newSize) {
    try {
        _ZN3WTF13StringBuilder6resizeEj(reinterpret_cast<Jsc::WtfAbi::StringBuilder*>(builder), newSize);
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void Append8(Builder& builder, const char* text) {
    _ZN3WTF13StringBuilder16appendCharactersEPKhj(&builder, reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

std::u16string Units(const NKWebKit::WtfAbi::StringImpl* impl, unsigned length) {
    std::u16string result;
    for (unsigned i = 0; i < length; ++i) {
        if (impl->hashAndFlags & 4) result += static_cast<const unsigned char*>(impl->data)[i];
        else result += static_cast<const char16_t*>(impl->data)[i];
    }
    return result;
}

std::u16string Contents(const Builder& builder) {
    if (builder.string.impl) return Units(builder.string.impl, static_cast<unsigned>(builder.length));
    Require(builder.buffer != nullptr);
    return Units(builder.buffer, static_cast<unsigned>(builder.length));
}

void FreeBuffer(Builder& builder) {
    NKWebKit::WtfAbi::Release(builder.buffer);
    builder.buffer = nullptr;
}

void TestStringBuilderResize() {
    auto owned = NewBuilder();
    Append8(owned, "hello");
    auto* ownedBuffer = owned.buffer;
    Require(ownedBuffer->refCount == 2);
    ResizeBuilder(owned, 5);
    Require(owned.buffer == ownedBuffer && owned.length == 5);
    ResizeBuilder(owned, 2);
    Require(owned.buffer == ownedBuffer && owned.length == 2 && !owned.string.impl && Contents(owned) == u"he");
    Require(ResizeThrows(&owned, 3));
    Require(owned.length == 2);
    ResizeBuilder(owned, 0);
    Require(owned.length == 0 && owned.buffer == ownedBuffer);
    FreeBuffer(owned);

    auto reified = NewBuilder();
    _ZN3WTF13StringBuilder15reserveCapacityEj(&reified, 4);
    Append8(reified, "wxyz");
    _ZNK3WTF13StringBuilder11reifyStringEv(&reified);
    auto* reifiedBuffer = reified.buffer;
    Require(reified.string.impl == reifiedBuffer && reifiedBuffer->refCount == 4);
    ResizeBuilder(reified, 3);
    Require(!reified.string.impl && reified.buffer == reifiedBuffer && reifiedBuffer->refCount == 2 && Contents(reified) == u"wxy");
    FreeBuffer(reified);

    auto shared = NewBuilder();
    _ZN3WTF13StringBuilder15reserveCapacityEj(&shared, 4);
    Append8(shared, "abcd");
    _ZNK3WTF13StringBuilder11reifyStringEv(&shared);
    WtfString held = shared.string;
    auto* heldImpl = held.impl;
    Require(heldImpl->refCount == 6);
    ResizeBuilder(shared, 1);
    Require(!shared.string.impl && shared.buffer != heldImpl && shared.buffer->refCount == 2);
    Require(heldImpl->refCount == 2 && Units(heldImpl, 4) == u"abcd" && Contents(shared) == u"a");
    Require(shared.buffer->length == 4 && (shared.buffer->hashAndFlags & 4));
    FreeBuffer(shared);

    auto wide = NewBuilder();
    const char16_t wideText[] = {0x3b1, 0x3b2, 0x3b3};
    _ZN3WTF13StringBuilder16appendCharactersEPKDsj(&wide, wideText, 3);
    Require(!wide.is8Bit && wide.buffer && !wide.string.impl && wide.buffer->refCount == 2);
    wide.buffer->refCount += 2;
    WtfString heldWide(wide.buffer);
    auto* heldWideImpl = heldWide.impl;
    const unsigned wideCapacity = wide.buffer->length;
    ResizeBuilder(wide, 2);
    Require(wide.buffer != heldWideImpl && !(wide.buffer->hashAndFlags & 4) && wide.buffer->length == wideCapacity);
    Require(heldWideImpl->refCount == 2 && Contents(wide) == std::u16string(wideText, 2) && Units(heldWideImpl, 3) == std::u16string(wideText, 3));
    FreeBuffer(wide);

    auto fromString = NewBuilder();
    fromString.string = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("hello"), 5);
    fromString.length = 5;
    WtfString original = fromString.string;
    ResizeBuilder(fromString, 3);
    Require(!fromString.buffer && fromString.string.impl && fromString.string.impl != original.impl);
    Require(fromString.string.impl->length == 3 && Contents(fromString) == u"hel" && Units(original.impl, 5) == u"hello");

    auto overflowed = NewBuilder();
    Append8(overflowed, "abc");
    overflowed.overflowed = true;
    ResizeBuilder(overflowed, 1);
    Require(overflowed.length == 3);
    Require(!ResizeThrows(&overflowed, 9));
    overflowed.overflowed = false;
    Require(Contents(overflowed) == u"abc");
    FreeBuffer(overflowed);

    Require(ResizeThrows(nullptr, 0));
}

bool StringValueEquals(JSContextRef ctx, JSValueRef value, const char* expected) {
    if (!JSValueIsString(ctx, value)) return false;
    ScopedJsString copy(JSValueToStringCopy(ctx, value, nullptr));
    return copy.ref && JSStringIsEqualToUTF8CString(copy, expected);
}

bool GlobalCheck(JSContextRef ctx, const char* name, JSValueRef value, const char* predicate) {
    ScopedJsString property(name);
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), property, value, kJSPropertyAttributeNone, nullptr);
    return JSValueToBoolean(ctx, Eval(ctx, predicate));
}

void TestMakeString(JSGlobalContextRef ctx) {
    ScopedJsString text("h\xC3\xA9llo");
    JSValueRef value = MakeString(ctx, text);
    Require(value != nullptr);
    Require(StringValueEquals(ctx, value, "h\xC3\xA9llo"));
    Require(GlobalCheck(ctx, "made", value, "typeof made === 'string' && made.length === 5 && made.charCodeAt(1) === 0xe9"));

    ScopedJsString empty("");
    JSValueRef emptyValue = MakeString(ctx, empty);
    Require(StringValueEquals(ctx, emptyValue, ""));

    JSValueRef nullValue = MakeString(ctx, nullptr);
    Require(nullValue != nullptr);
    Require(StringValueEquals(ctx, nullValue, ""));
    Require(GlobalCheck(ctx, "fromNull", nullValue, "fromNull === ''"));

    JSValueRef again = MakeString(ctx, text);
    Require(JSValueIsStrictEqual(ctx, value, again));

    Require(JSValueMakeString(nullptr, text) == nullptr);
}

void TestHasPropertyForKey(JSGlobalContextRef ctx) {
    JSObjectRef object = JSValueToObject(ctx, Eval(ctx, "globalThis.sym = Symbol('k'); ({ a: 1, 3: 'x', [sym]: 2 })"), nullptr);
    Require(object != nullptr);

    ScopedJsString a("a");
    ScopedJsString b("b");
    ScopedJsString inherited("toString");
    Require(HasPropertyForKey(ctx, object, MakeString(ctx, a), nullptr));
    Require(!HasPropertyForKey(ctx, object, MakeString(ctx, b), nullptr));
    Require(HasPropertyForKey(ctx, object, MakeString(ctx, inherited), nullptr));
    Require(HasPropertyForKey(ctx, object, JSValueMakeNumber(ctx, 3), nullptr));
    Require(!HasPropertyForKey(ctx, object, JSValueMakeNumber(ctx, 4), nullptr));
    Require(HasPropertyForKey(ctx, object, Eval(ctx, "sym"), nullptr));
    Require(!HasPropertyForKey(ctx, object, Eval(ctx, "Symbol('k')"), nullptr));

    JSObjectRef proxy = JSValueToObject(ctx, Eval(ctx, "new Proxy({}, { has(target, key) { return key === 'trap'; } })"), nullptr);
    ScopedJsString trap("trap");
    Require(HasPropertyForKey(ctx, proxy, MakeString(ctx, trap), nullptr));
    Require(!HasPropertyForKey(ctx, proxy, MakeString(ctx, a), nullptr));

    JSValueRef throwingKey = Eval(ctx, "({ toString() { throw new Error('boom'); } })");
    JSValueRef exception = nullptr;
    Require(!HasPropertyForKey(ctx, object, throwingKey, &exception));
    Require(exception != nullptr);
    Require(GlobalCheck(ctx, "caught", exception, "caught instanceof Error && caught.message === 'boom'"));
    Require(!HasPropertyForKey(ctx, object, throwingKey, nullptr));

    JSObjectRef throwingProxy = JSValueToObject(ctx, Eval(ctx, "new Proxy({}, { has() { throw new TypeError('trap'); } })"), nullptr);
    JSValueRef trapException = nullptr;
    Require(!HasPropertyForKey(ctx, throwingProxy, MakeString(ctx, a), &trapException));
    Require(trapException != nullptr);
    Require(GlobalCheck(ctx, "trapCaught", trapException, "trapCaught instanceof TypeError"));

    exception = nullptr;
    Require(HasPropertyForKey(ctx, object, MakeString(ctx, a), &exception));
    Require(exception == nullptr);

    Require(!JSObjectHasPropertyForKey(nullptr, object, MakeString(ctx, a), nullptr));
    Require(!HasPropertyForKey(ctx, nullptr, MakeString(ctx, a), nullptr));
    Require(!HasPropertyForKey(ctx, object, nullptr, nullptr));
}

}

int main() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    Require(ctx != nullptr);
    TestMakeString(ctx);
    TestHasPropertyForKey(ctx);
    TestStringBuilderResize();
    JSGlobalContextRelease(ctx);
    return 0;
}
