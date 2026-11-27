#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceJsc/include/Api.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace NKWebKit;

namespace {

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "JscValue guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(condition) Check((condition), __LINE__)

struct GuardedPages {
    unsigned char* base{nullptr};
    std::size_t pageSize{0};

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
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
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
        Require(VirtualFree(base, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(base, pageSize * 3) == 0);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }
};

template <typename Action>
bool ThrowsNotImplemented(Action&& action, const char* symbol) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(symbol) + " not implemented";
    }
    return false;
}

JSValueRef Eval(JSGlobalContextRef ctx, const char* source) {
    JSStringRef script = JSStringCreateWithUTF8CString_nid_postfix(source);
    JSValueRef exception = nullptr;
    JSValueRef result = JSEvaluateScript(ctx, script, nullptr, nullptr, 1, &exception);
    JSStringRelease_nid_postfix(script);
    Require(exception == nullptr && result != nullptr);
    return result;
}

}  // namespace

int main() {
    GuardedPages pages;
    auto* readOnly = reinterpret_cast<JSValueRef*>(pages.ReadOnly());
    auto* guard = reinterpret_cast<JSValueRef*>(pages.Guard());
    auto* dangling = reinterpret_cast<JSValueRef*>(0x10);
    unsigned char* splitBytes = pages.ReadOnly() - 4;
    auto* split = reinterpret_cast<JSValueRef*>(splitBytes);

    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    Require(ctx != nullptr);

    JSValueRef numberVal = JSValueMakeNumber(ctx, 42.0);
    JSValueRef nullVal = JSValueMakeNull(ctx);
    JSValueRef symbolVal = JSValueMakeSymbol(ctx, nullptr);
    JSValueRef cyclicObj = Eval(ctx, "(() => { const o = {}; o.self = o; return o; })()");
    JSValueRef throwingEqObj = Eval(ctx, "({ valueOf() { throw new TypeError('boom'); } })");
    JSObjectRef throwingCtor = JSValueToObject(ctx, Eval(ctx, "Object.defineProperty(function F() {}, Symbol.hasInstance, { value() { throw new TypeError('boom'); } })"), nullptr);
    Require(throwingCtor != nullptr);

    std::memset(splitBytes, 0x5A, 4);
    Require(ThrowsNotImplemented([&] { JSValueCreateJSONString(ctx, numberVal, 0, readOnly); }, "JSValueCreateJSONString"));
    Require(ThrowsNotImplemented([&] { JSValueCreateJSONString(ctx, numberVal, 0, guard); }, "JSValueCreateJSONString"));
    Require(ThrowsNotImplemented([&] { JSValueCreateJSONString(ctx, numberVal, 0, dangling); }, "JSValueCreateJSONString"));
    Require(ThrowsNotImplemented([&] { JSValueCreateJSONString(ctx, numberVal, 0, split); }, "JSValueCreateJSONString"));
    Require(ThrowsNotImplemented([&] { JSValueCreateJSONString(ctx, cyclicObj, 0, readOnly); }, "JSValueCreateJSONString"));
    Require(ThrowsNotImplemented([&] { JSValueCreateJSONString(ctx, cyclicObj, 0, split); }, "JSValueCreateJSONString"));
    for (std::size_t i = 0; i < 4; ++i) Require(splitBytes[i] == 0x5A);

    Require(ThrowsNotImplemented([&] { JSValueToNumber(ctx, symbolVal, readOnly); }, "JSValueToNumber"));
    Require(ThrowsNotImplemented([&] { JSValueToNumber(ctx, symbolVal, guard); }, "JSValueToNumber"));
    Require(ThrowsNotImplemented([&] { JSValueToNumber(ctx, symbolVal, split); }, "JSValueToNumber"));
    for (std::size_t i = 0; i < 4; ++i) Require(splitBytes[i] == 0x5A);

    Require(ThrowsNotImplemented([&] { JSValueToStringCopy(ctx, symbolVal, readOnly); }, "JSValueToStringCopy"));
    Require(ThrowsNotImplemented([&] { JSValueToStringCopy(ctx, symbolVal, guard); }, "JSValueToStringCopy"));
    Require(ThrowsNotImplemented([&] { JSValueToStringCopy(ctx, symbolVal, split); }, "JSValueToStringCopy"));
    for (std::size_t i = 0; i < 4; ++i) Require(splitBytes[i] == 0x5A);

    Require(ThrowsNotImplemented([&] { JSValueToObject(ctx, nullVal, readOnly); }, "JSValueToObject"));
    Require(ThrowsNotImplemented([&] { JSValueToObject(ctx, nullVal, guard); }, "JSValueToObject"));
    Require(ThrowsNotImplemented([&] { JSValueToObject(ctx, nullVal, split); }, "JSValueToObject"));
    for (std::size_t i = 0; i < 4; ++i) Require(splitBytes[i] == 0x5A);

    Require(ThrowsNotImplemented([&] { JSValueIsEqual(ctx, throwingEqObj, numberVal, readOnly); }, "JSValueIsEqual"));
    Require(ThrowsNotImplemented([&] { JSValueIsEqual(ctx, throwingEqObj, numberVal, guard); }, "JSValueIsEqual"));
    Require(ThrowsNotImplemented([&] { JSValueIsEqual(ctx, throwingEqObj, numberVal, split); }, "JSValueIsEqual"));
    for (std::size_t i = 0; i < 4; ++i) Require(splitBytes[i] == 0x5A);

    Require(ThrowsNotImplemented([&] { JSValueIsInstanceOfConstructor(ctx, numberVal, throwingCtor, readOnly); }, "JSValueIsInstanceOfConstructor"));
    Require(ThrowsNotImplemented([&] { JSValueIsInstanceOfConstructor(ctx, numberVal, throwingCtor, guard); }, "JSValueIsInstanceOfConstructor"));
    Require(ThrowsNotImplemented([&] { JSValueIsInstanceOfConstructor(ctx, numberVal, throwingCtor, split); }, "JSValueIsInstanceOfConstructor"));
    for (std::size_t i = 0; i < 4; ++i) Require(splitBytes[i] == 0x5A);

    JSValueRef exception = numberVal;
    JSStringRef json = JSValueCreateJSONString(ctx, numberVal, 0, &exception);
    Require(json != nullptr && exception == nullptr);
    JSStringRelease_nid_postfix(json);

    exception = nullptr;
    Require(JSValueCreateJSONString(ctx, cyclicObj, 0, &exception) == nullptr && exception != nullptr);

    exception = nullptr;
    JSValueToNumber(ctx, symbolVal, &exception);
    Require(exception != nullptr);

    exception = nullptr;
    Require(JSValueToStringCopy(ctx, symbolVal, &exception) == nullptr && exception != nullptr);

    exception = nullptr;
    Require(JSValueToObject(ctx, nullVal, &exception) == nullptr && exception != nullptr);

    exception = nullptr;
    Require(!JSValueIsEqual(ctx, throwingEqObj, numberVal, &exception) && exception != nullptr);

    exception = nullptr;
    Require(!JSValueIsInstanceOfConstructor(ctx, numberVal, throwingCtor, &exception) && exception != nullptr);

    JSGlobalContextRelease(ctx);
    return 0;
}
