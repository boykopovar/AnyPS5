#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace NKWebKit;

namespace {

constexpr const char* KeepAndReplace =
    "globalThis.RealDate = Date; globalThis.RealError = Error; globalThis.RealRegExp = RegExp;"
    "globalThis.Date = function () { this.fake = true; };"
    "globalThis.Error = function () { this.fake = true; };"
    "globalThis.RegExp = function () { this.fake = true; }; 0";

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSC make objects check failed at line %u\n", location.line());
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

void SetGlobal(JSContextRef ctx, const char* name, JSValueRef value) {
    ScopedJsString key(name);
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), key, value, kJSPropertyAttributeNone, nullptr);
}

void RequireRealObjects(JSContextRef ctx, std::source_location location = std::source_location::current()) {
    JSValueRef exception = nullptr;
    JSValueRef day = JSValueMakeNumber(ctx, 86400000.0);
    JSObjectRef date = JSObjectMakeDate(ctx, 1, &day, &exception);
    Require(date != nullptr && exception == nullptr, location);
    Require(JSValueIsDate(ctx, date), location);
    SetGlobal(ctx, "made", date);
    Require(EvalBool(ctx, "made instanceof RealDate && !('fake' in made) && made.getTime() === 86400000"), location);

    JSValueRef errorParts[2] = {Eval(ctx, "'boom'"), Eval(ctx, "({ cause: 7 })")};
    JSObjectRef error = JSObjectMakeError(ctx, 2, errorParts, &exception);
    Require(error != nullptr && exception == nullptr, location);
    SetGlobal(ctx, "made", error);
    Require(EvalBool(ctx, "made instanceof RealError && !('fake' in made) && made.message === 'boom' && made.cause === 7"), location);
    Require(EvalBool(ctx, "Object.prototype.toString.call(made) === '[object Error]'"), location);

    JSValueRef patternParts[2] = {Eval(ctx, "'a+'"), Eval(ctx, "'gi'")};
    JSObjectRef pattern = JSObjectMakeRegExp(ctx, 2, patternParts, &exception);
    Require(pattern != nullptr && exception == nullptr, location);
    SetGlobal(ctx, "made", pattern);
    Require(EvalBool(ctx, "made instanceof RealRegExp && !('fake' in made) && made.source === 'a+' && made.flags === 'gi'"), location);
    Require(EvalBool(ctx, "RealRegExp.prototype.test.call(made, 'xAAy')"), location);
}

void TestUntouchedGlobals() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    Eval(ctx, "globalThis.RealDate = Date; globalThis.RealError = Error; globalThis.RealRegExp = RegExp; 0");
    RequireRealObjects(ctx);

    JSValueRef exception = nullptr;
    JSValueRef fields[3] = {JSValueMakeNumber(ctx, 2020.0), JSValueMakeNumber(ctx, 0.0), JSValueMakeNumber(ctx, 2.0)};
    JSObjectRef date = JSObjectMakeDate(ctx, 3, fields, &exception);
    Require(date != nullptr && exception == nullptr);
    SetGlobal(ctx, "made", date);
    Require(EvalBool(ctx, "made.getFullYear() === 2020 && made.getMonth() === 0 && made.getDate() === 2"));

    Require(JSObjectMakeDate(ctx, 0, nullptr, &exception) != nullptr);
    JSObjectRef emptyError = JSObjectMakeError(ctx, 0, nullptr, &exception);
    Require(emptyError != nullptr && exception == nullptr);
    SetGlobal(ctx, "made", emptyError);
    Require(EvalBool(ctx, "made.message === '' && !Object.prototype.hasOwnProperty.call(made, 'message')"));
    JSObjectRef emptyPattern = JSObjectMakeRegExp(ctx, 0, nullptr, &exception);
    Require(emptyPattern != nullptr && exception == nullptr);
    SetGlobal(ctx, "made", emptyPattern);
    Require(EvalBool(ctx, "made.source === '(?:)'"));

    JSValueRef broken = Eval(ctx, "'('");
    Require(JSObjectMakeRegExp(ctx, 1, &broken, &exception) == nullptr);
    Require(exception != nullptr);
    SetGlobal(ctx, "lastError", exception);
    Require(EvalBool(ctx, "lastError instanceof SyntaxError"));

    JSGlobalContextRelease(ctx);
}

void TestReplacedGlobals() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    Eval(ctx, KeepAndReplace);
    Require(EvalBool(ctx, "new Date().fake === true && new Error().fake === true && new RegExp().fake === true"));
    RequireRealObjects(ctx);

    Eval(ctx, "globalThis.Date = 5; globalThis.Error = null; globalThis.RegExp = {}; 0");
    RequireRealObjects(ctx);

    Eval(ctx, "delete globalThis.Date; delete globalThis.Error; delete globalThis.RegExp; 0");
    Require(EvalBool(ctx, "typeof Date === 'undefined' && typeof Error === 'undefined' && typeof RegExp === 'undefined'"));
    RequireRealObjects(ctx);

    JSGlobalContextRelease(ctx);
}

void TestEachContextUsesItsOwnConstructors() {
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef first = JSGlobalContextCreateInGroup(group, nullptr);
    JSGlobalContextRef second = JSGlobalContextCreateInGroup(group, nullptr);
    Eval(first, KeepAndReplace);
    Eval(second, "globalThis.RealDate = Date; globalThis.RealError = Error; globalThis.RealRegExp = RegExp; 0");
    RequireRealObjects(first);
    RequireRealObjects(second);

    JSValueRef day = JSValueMakeNumber(second, 0.0);
    JSObjectRef date = JSObjectMakeDate(second, 1, &day, nullptr);
    Require(date != nullptr);
    SetGlobal(first, "foreign", date);
    Require(EvalBool(first, "!(foreign instanceof RealDate) && Object.prototype.toString.call(foreign) === '[object Date]'"));

    JSGlobalContextRelease(second);
    JSGlobalContextRelease(first);
    JSContextGroupRelease(group);
}

void APS5_VABI ReplaceOnInitialize(JSContextRef ctx, JSObjectRef object) {
    (void)object;
    Eval(ctx, KeepAndReplace);
}

void TestGlobalClassInitializeRunsAfterTheCapture() {
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.initialize = &ReplaceOnInitialize;
    JSClassRef globalClass = JSClassCreate(&definition);
    Require(globalClass != nullptr);
    JSGlobalContextRef ctx = JSGlobalContextCreate(globalClass);
    Require(EvalBool(ctx, "new Date().fake === true"));
    RequireRealObjects(ctx);
    JSGlobalContextRelease(ctx);
    JSClassRelease(globalClass);
}

struct GuardedPages {
    std::size_t pageSize = 0;
    unsigned char* base = nullptr;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 3);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }
};

template<typename TCall>
bool ThrowsNotImplemented(TCall&& call) {
    const auto before = NotImplementedCalls_nid_no_patch();
    try {
        call();
    } catch (const std::exception&) {
        return NotImplementedCalls_nid_no_patch() > before;
    }
    return false;
}

void TestGuestPointers() {
    GuardedPages pages;
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    Require(ThrowsNotImplemented([&] {
        JSClassCreate(reinterpret_cast<const JSClassDefinition*>(pages.Guard()));
    }));

    JSClassDefinition badName = kJSClassDefinitionEmpty;
    badName.className = reinterpret_cast<const char*>(pages.Guard());
    Require(ThrowsNotImplemented([&] { JSClassCreate(&badName); }));

    JSClassDefinition badValues = kJSClassDefinitionEmpty;
    badValues.staticValues = reinterpret_cast<const JSStaticValue*>(pages.Guard());
    Require(ThrowsNotImplemented([&] { JSClassCreate(&badValues); }));

    JSStaticValue badValueNameEntry[]{
        {reinterpret_cast<const char*>(pages.Guard()), nullptr, nullptr, 0},
        {nullptr, nullptr, nullptr, 0},
    };
    JSClassDefinition badValueName = kJSClassDefinitionEmpty;
    badValueName.staticValues = badValueNameEntry;
    Require(ThrowsNotImplemented([&] { JSClassCreate(&badValueName); }));

    JSClassDefinition badFuncs = kJSClassDefinitionEmpty;
    badFuncs.staticFunctions = reinterpret_cast<const JSStaticFunction*>(pages.Guard());
    Require(ThrowsNotImplemented([&] { JSClassCreate(&badFuncs); }));

    JSStaticFunction badFuncNameEntry[]{
        {reinterpret_cast<const char*>(pages.Guard()), nullptr, 0},
        {nullptr, nullptr, 0},
    };
    JSClassDefinition badFuncName = kJSClassDefinitionEmpty;
    badFuncName.staticFunctions = badFuncNameEntry;
    Require(ThrowsNotImplemented([&] { JSClassCreate(&badFuncName); }));

    Require(ThrowsNotImplemented([&] {
        _ZN13OpaqueJSClassD1Ev(reinterpret_cast<OpaqueJSClass*>(pages.Guard()));
    }));
    Require(ThrowsNotImplemented([&] {
        _ZN13OpaqueJSClassD2Ev(reinterpret_cast<OpaqueJSClass*>(pages.ReadOnly()));
    }));

    ScopedJsString body("return 1;");
    Require(ThrowsNotImplemented([&] {
        JSObjectMakeFunction(ctx, nullptr, 1, reinterpret_cast<const JSStringRef*>(pages.Guard()), body, nullptr, 1, nullptr);
    }));

    JSObjectRef resolveSlot = reinterpret_cast<JSObjectRef>(std::uintptr_t{0x1234});
    JSObjectRef rejectSlot = reinterpret_cast<JSObjectRef>(std::uintptr_t{0x5678});
    auto* readOnlyObjPtr = reinterpret_cast<JSObjectRef*>(pages.ReadOnly());
    auto* guardObjPtr = reinterpret_cast<JSObjectRef*>(pages.Guard());
    Require(ThrowsNotImplemented([&] {
        JSObjectMakeDeferredPromise(ctx, readOnlyObjPtr, &rejectSlot, nullptr);
    }));
    Require(rejectSlot == reinterpret_cast<JSObjectRef>(std::uintptr_t{0x5678}));
    Require(ThrowsNotImplemented([&] {
        JSObjectMakeDeferredPromise(ctx, &resolveSlot, readOnlyObjPtr, nullptr);
    }));
    Require(resolveSlot == reinterpret_cast<JSObjectRef>(std::uintptr_t{0x1234}));
    Require(ThrowsNotImplemented([&] {
        JSObjectMakeDeferredPromise(ctx, guardObjPtr, nullptr, nullptr);
    }));

    auto* guardArgs = reinterpret_cast<const JSValueRef*>(pages.Guard());
    Require(ThrowsNotImplemented([&] { JSObjectMakeArray(ctx, 1, guardArgs, nullptr); }));
    Require(ThrowsNotImplemented([&] { JSObjectMakeDate(ctx, 1, guardArgs, nullptr); }));
    Require(ThrowsNotImplemented([&] { JSObjectMakeError(ctx, 1, guardArgs, nullptr); }));
    Require(ThrowsNotImplemented([&] { JSObjectMakeRegExp(ctx, 1, guardArgs, nullptr); }));

    JSObjectRef fn = JSValueToObject(ctx, Eval(ctx, "(function(x) { return x; })"), nullptr);
    JSObjectRef ctor = JSValueToObject(ctx, Eval(ctx, "(function C(x) { this.x = x; })"), nullptr);
    Require(ThrowsNotImplemented([&] { JSObjectCallAsFunction(ctx, fn, nullptr, 1, guardArgs, nullptr); }));
    Require(ThrowsNotImplemented([&] { JSObjectCallAsConstructor(ctx, ctor, 1, guardArgs, nullptr); }));

    auto* readOnlyExc = reinterpret_cast<JSValueRef*>(pages.ReadOnly());
    JSValueRef broken = Eval(ctx, "'('");
    Require(ThrowsNotImplemented([&] { JSObjectMakeRegExp(ctx, 1, &broken, readOnlyExc); }));
    Require(ThrowsNotImplemented([&] {
        JSObjectMakeTypedArrayWithBytesNoCopy(ctx, kJSTypedArrayTypeUint8Array, nullptr, 4, nullptr, nullptr, readOnlyExc);
    }));
    JSObjectRef throwingObj = JSValueToObject(ctx, Eval(ctx, "({ get boom() { throw new Error('x'); } })"), nullptr);
    ScopedJsString boomKey("boom");
    Require(ThrowsNotImplemented([&] { JSObjectGetProperty(ctx, throwingObj, boomKey, readOnlyExc); }));

    JSGlobalContextRelease(ctx);
}

}  // namespace

int main() {
    TestUntouchedGlobals();
    TestReplacedGlobals();
    TestEachContextUsesItsOwnConstructors();
    TestGlobalClassInitializeRunsAfterTheCapture();
    TestGuestPointers();
    std::puts("libSceNKWebKit JS make objects tests passed");
    return 0;
}
