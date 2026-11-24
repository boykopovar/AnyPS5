#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>

using namespace NKWebKit;

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSC global class release check failed at line %u\n", location.line());
    std::abort();
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

int finalizeCalls = 0;

void APS5_VABI CountFinalize(JSObjectRef object) {
    (void)object;
    ++finalizeCalls;
}

JSValueRef APS5_VABI ReturnFive(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    return JSValueMakeNumber(ctx, 5.0);
}

const JSStaticFunction globalFunctions[] = {
    {"classFunction", &ReturnFive, kJSPropertyAttributeNone},
    {nullptr, nullptr, 0},
};

JSClassRef MakeGlobalClass() {
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = "Global";
    definition.finalize = &CountFinalize;
    definition.staticFunctions = globalFunctions;
    JSClassRef jsClass = JSClassCreate(&definition);
    Require(jsClass != nullptr);
    return jsClass;
}

void FillTheGlobalObject(JSGlobalContextRef ctx) {
    JSObjectRef global = JSContextGetGlobalObject(ctx);
    Require(JSObjectSetPrivate(global, &finalizeCalls));
    ScopedJsString privateName("kept");
    Require(JSObjectSetPrivateProperty(ctx, global, privateName, JSObjectMake(ctx, nullptr, nullptr)));
    ScopedJsString selfName("self");
    Require(JSObjectSetPrivateProperty(ctx, global, selfName, global));
    Require(EvalBool(ctx, "var stored = { owner: this }; classFunction() === 5"));
}

void TestReleaseThenCollect(bool fill, int collections) {
    JSClassRef globalClass = MakeGlobalClass();
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef survivor = JSGlobalContextCreateInGroup(group, nullptr);
    const std::size_t entriesBefore = survivor->group->callbackDataByPtr.size();
    JSGlobalContextRef withClass = JSGlobalContextCreateInGroup(group, globalClass);
    if (fill) FillTheGlobalObject(withClass);
    void* globalPointer = JSContextGetGlobalObject(withClass)->objPtr;
    Require(survivor->group->FindCallbackDataByPtr(globalPointer) != nullptr);

    finalizeCalls = 0;
    JSGlobalContextRelease(withClass);
    Require(finalizeCalls == 1);
    for (int i = 0; i < collections; ++i) JSGarbageCollect(survivor);
    Require(finalizeCalls == 1);
    Require(survivor->group->FindCallbackDataByPtr(globalPointer) == nullptr);
    Require(survivor->group->callbackDataByPtr.size() == entriesBefore);

    for (int i = 0; i < 256; ++i) {
        JSObjectRef plain = JSObjectMake(survivor, nullptr, nullptr);
        Require(JSObjectGetPrivate(plain) == nullptr);
        Require(!JSObjectSetPrivate(plain, &finalizeCalls));
    }
    Require(EvalBool(survivor, "typeof classFunction === 'undefined' && [1, 2, 3].length === 3"));
    JSGarbageCollect(survivor);

    JSGlobalContextRelease(survivor);
    JSContextGroupRelease(group);
    Require(finalizeCalls == 1);
    JSClassRelease(globalClass);
}

int trackedFinalizeCalls = 0;

void APS5_VABI CountTrackedFinalize(JSObjectRef object) {
    (void)object;
    ++trackedFinalizeCalls;
}

void TestAPrivateValueSharedWithAnotherContext() {
    JSClassRef globalClass = MakeGlobalClass();
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.finalize = &CountTrackedFinalize;
    JSClassRef trackedClass = JSClassCreate(&definition);
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef survivor = JSGlobalContextCreateInGroup(group, nullptr);
    JSGlobalContextRef withClass = JSGlobalContextCreateInGroup(group, globalClass);

    JSObjectRef tracked = JSObjectMake(survivor, trackedClass, nullptr);
    ScopedJsString name("tracked");
    Require(JSObjectSetPrivateProperty(withClass, JSContextGetGlobalObject(withClass), name, tracked));
    JSObjectSetProperty(survivor, JSContextGetGlobalObject(survivor), name, tracked, kJSPropertyAttributeNone, nullptr);

    trackedFinalizeCalls = 0;
    JSGlobalContextRelease(withClass);
    JSGarbageCollect(survivor);
    Require(trackedFinalizeCalls == 0);
    Require(EvalBool(survivor, "typeof tracked === 'object' && delete this.tracked"));
    JSGarbageCollect(survivor);
    Require(trackedFinalizeCalls == 1);

    JSGlobalContextRelease(survivor);
    JSContextGroupRelease(group);
    Require(trackedFinalizeCalls == 1);
    JSClassRelease(trackedClass);
    JSClassRelease(globalClass);
}

void TestReleaseWithoutCollect() {
    JSClassRef globalClass = MakeGlobalClass();
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef survivor = JSGlobalContextCreateInGroup(group, nullptr);
    JSGlobalContextRef withClass = JSGlobalContextCreateInGroup(group, globalClass);
    FillTheGlobalObject(withClass);
    finalizeCalls = 0;
    JSGlobalContextRelease(withClass);
    JSGlobalContextRelease(survivor);
    JSContextGroupRelease(group);
    Require(finalizeCalls == 1);
    JSClassRelease(globalClass);
}

void TestTwoContextsWithAClass() {
    JSClassRef globalClass = MakeGlobalClass();
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef first = JSGlobalContextCreateInGroup(group, globalClass);
    JSGlobalContextRef second = JSGlobalContextCreateInGroup(group, globalClass);
    FillTheGlobalObject(first);
    FillTheGlobalObject(second);
    finalizeCalls = 0;
    JSGlobalContextRelease(first);
    Require(finalizeCalls == 1);
    JSGarbageCollect(second);
    Require(EvalBool(second, "classFunction() === 5 && stored.owner === this"));
    ScopedJsString privateName("kept");
    JSValueRef kept = JSObjectGetPrivateProperty(second, JSContextGetGlobalObject(second), privateName);
    Require(kept != nullptr && JSValueIsObject(second, kept));
    Require(JSObjectGetPrivate(JSContextGetGlobalObject(second)) == &finalizeCalls);
    JSGlobalContextRelease(second);
    JSContextGroupRelease(group);
    Require(finalizeCalls == 2);
    JSClassRelease(globalClass);
}

void RequirePlainObjects(JSGlobalContextRef survivor) {
    for (int i = 0; i < 256; ++i) {
        JSObjectRef plain = JSObjectMake(survivor, nullptr, nullptr);
        Require(JSObjectGetPrivate(plain) == nullptr);
        Require(!JSObjectSetPrivate(plain, &finalizeCalls));
    }
}

void TestAKeptPrototypeKeepsTheGlobalObject() {
    JSClassRef globalClass = MakeGlobalClass();
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef survivor = JSGlobalContextCreateInGroup(group, nullptr);
    const std::size_t entriesBefore = survivor->group->callbackDataByPtr.size();
    JSGlobalContextRef withClass = JSGlobalContextCreateInGroup(group, globalClass);
    JSObjectRef globalHandle = JSContextGetGlobalObject(withClass);
    void* globalPointer = globalHandle->objPtr;
    JSValueRef prototype = Eval(withClass, "Object.getPrototypeOf(this)");
    Require(JSValueIsObject(withClass, prototype));
    ScopedJsString name("keptPrototype");
    JSObjectSetProperty(survivor, JSContextGetGlobalObject(survivor), name, prototype, kJSPropertyAttributeNone, nullptr);
    const std::size_t entriesWithClass = survivor->group->callbackDataByPtr.size();
    Require(entriesWithClass >= entriesBefore + 2);

    finalizeCalls = 0;
    JSGlobalContextRelease(withClass);
    JSGarbageCollect(survivor);
    Require(finalizeCalls == 1);
    Require(survivor->group->FindCallbackDataByPtr(globalPointer) != nullptr);
    Require(survivor->group->callbackDataByPtr.size() == entriesWithClass);
    Require(globalHandle->alive);
    RequirePlainObjects(survivor);
    Require(EvalBool(survivor, "typeof keptPrototype === 'object' && delete this.keptPrototype"));
    JSGarbageCollect(survivor);
    JSGarbageCollect(survivor);
    Require(survivor->group->FindCallbackDataByPtr(globalPointer) == nullptr);
    Require(survivor->group->callbackDataByPtr.size() == entriesBefore);
    RequirePlainObjects(survivor);

    JSGlobalContextRelease(survivor);
    JSContextGroupRelease(group);
    Require(finalizeCalls == 1);
    JSClassRelease(globalClass);
}

void TestTheGlobalObjectOutlivesItsPrototype() {
    JSClassRef globalClass = MakeGlobalClass();
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef survivor = JSGlobalContextCreateInGroup(group, nullptr);
    const std::size_t entriesBefore = survivor->group->callbackDataByPtr.size();
    JSGlobalContextRef withClass = JSGlobalContextCreateInGroup(group, globalClass);
    FillTheGlobalObject(withClass);
    JSObjectRef globalHandle = JSContextGetGlobalObject(withClass);
    void* globalPointer = globalHandle->objPtr;
    Require(EvalBool(withClass, "Object.setPrototypeOf(this, null) === this"));
    const std::size_t entriesWithClass = survivor->group->callbackDataByPtr.size();

    finalizeCalls = 0;
    JSGlobalContextRelease(withClass);
    Require(finalizeCalls == 1);
    Require(survivor->group->FindCallbackDataByPtr(globalPointer) != nullptr);
    Require(survivor->group->callbackDataByPtr.size() == entriesWithClass - 1);
    Require(globalHandle->alive && globalHandle->ephemeralPinned);
    RequirePlainObjects(survivor);
    JSGarbageCollect(survivor);
    Require(survivor->group->FindCallbackDataByPtr(globalPointer) == nullptr);
    Require(survivor->group->callbackDataByPtr.size() == entriesBefore);
    RequirePlainObjects(survivor);

    JSGlobalContextRelease(survivor);
    JSContextGroupRelease(group);
    Require(finalizeCalls == 1);
    JSClassRelease(globalClass);
}

}  // namespace

int main() {
    TestAKeptPrototypeKeepsTheGlobalObject();
    TestTheGlobalObjectOutlivesItsPrototype();
    TestReleaseThenCollect(false, 1);
    TestReleaseThenCollect(true, 1);
    TestReleaseThenCollect(true, 3);
    TestAPrivateValueSharedWithAnotherContext();
    TestReleaseWithoutCollect();
    TestTwoContextsWithAClass();
    std::puts("libSceNKWebKit JS global class release tests passed");
    return 0;
}
