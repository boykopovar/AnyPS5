#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

using namespace NKWebKit;

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSWeak check failed at line %u\n", location.line());
    std::abort();
}

std::uint32_t GroupReferences(JSContextGroupRef group) {
    return group->refCount.load();
}

std::uint32_t WeakReferences(JSWeakRef weak) {
    return weak->refCount.load();
}

bool Tracked(JSContextGroupRef group, JSWeakRef weak) {
    const auto& weakRefs = group->weakRefs;
    return std::find(weakRefs.begin(), weakRefs.end(), weak) != weakRefs.end();
}

double EvaluateNumber(JSContextRef ctx, const char* code) {
    JSStringRef script = JSStringCreateWithUTF8CString_nid_postfix(code);
    JSValueRef exception = nullptr;
    JSValueRef result = JSEvaluateScript(ctx, script, nullptr, nullptr, 1, &exception);
    JSStringRelease_nid_postfix(script);
    Require(result != nullptr && exception == nullptr);
    return JSValueToNumber(ctx, result, nullptr);
}

void TestRetainAndReleaseCountTheWeak() {
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef ctx = JSGlobalContextCreateInGroup(group, nullptr);
    const std::uint32_t groupReferences = GroupReferences(group);
    Require(groupReferences == 2);

    JSObjectRef object = JSObjectMake(ctx, nullptr, nullptr);
    Require(object != nullptr);
    JSValueProtect(ctx, object);

    JSWeakRef weak = JSWeakCreate(group, object);
    Require(weak != nullptr);
    Require(WeakReferences(weak) == 1);
    Require(Tracked(group, weak));
    Require(GroupReferences(group) == groupReferences);
    Require(JSWeakGetObject(weak) == object);

    JSWeakRetain(group, weak);
    Require(WeakReferences(weak) == 2);
    Require(GroupReferences(group) == groupReferences);
    Require(Tracked(group, weak));
    Require(JSWeakGetObject(weak) == object);

    JSWeakRelease(group, weak);
    Require(WeakReferences(weak) == 1);
    Require(GroupReferences(group) == groupReferences);
    Require(Tracked(group, weak));
    Require(JSWeakGetObject(weak) == object);

    JSWeakRelease(group, weak);
    Require(!Tracked(group, weak));
    Require(group->weakRefs.empty());
    Require(GroupReferences(group) == groupReferences);

    Require(EvaluateNumber(ctx, "20 + 22") == 42.0);
    JSValueUnprotect(ctx, object);
    JSGlobalContextRelease(ctx);
    Require(GroupReferences(group) == 1);
    JSContextGroupRelease(group);
}

void TestGroupHeldOnlyByItsContext() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    JSContextGroupRef group = JSContextGetGroup(ctx);
    Require(group != nullptr);
    Require(GroupReferences(group) == 1);

    JSObjectRef object = JSObjectMake(ctx, nullptr, nullptr);
    JSValueProtect(ctx, object);
    JSWeakRef first = JSWeakCreate(group, object);
    JSWeakRef second = JSWeakCreate(group, object);
    Require(first != nullptr && second != nullptr && first != second);
    Require(group->weakRefs.size() == 2);

    JSWeakRetain(group, first);
    Require(WeakReferences(first) == 2);
    Require(WeakReferences(second) == 1);
    Require(GroupReferences(group) == 1);

    JSWeakRelease(group, second);
    Require(GroupReferences(group) == 1);
    Require(!Tracked(group, second));
    Require(Tracked(group, first));
    Require(WeakReferences(first) == 2);
    Require(JSWeakGetObject(first) == object);

    JSWeakRelease(group, first);
    JSWeakRelease(group, first);
    Require(group->weakRefs.empty());
    Require(GroupReferences(group) == 1);

    Require(EvaluateNumber(ctx, "6 * 7") == 42.0);
    Require(JSContextGetGroup(ctx) == group);
    JSValueUnprotect(ctx, object);
    JSGlobalContextRelease(ctx);
}

void TestNullArguments() {
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef ctx = JSGlobalContextCreateInGroup(group, nullptr);
    const std::uint32_t groupReferences = GroupReferences(group);
    JSObjectRef object = JSObjectMake(ctx, nullptr, nullptr);
    JSValueProtect(ctx, object);
    JSWeakRef weak = JSWeakCreate(group, object);
    Require(weak != nullptr);

    JSWeakRetain(nullptr, weak);
    Require(WeakReferences(weak) == 1);
    JSWeakRetain(group, nullptr);
    Require(GroupReferences(group) == groupReferences);

    JSWeakRelease(nullptr, weak);
    Require(WeakReferences(weak) == 1);
    Require(Tracked(group, weak));
    JSWeakRelease(group, nullptr);
    Require(GroupReferences(group) == groupReferences);
    Require(Tracked(group, weak));
    Require(JSWeakGetObject(weak) == object);

    JSWeakRelease(group, weak);
    Require(group->weakRefs.empty());
    Require(GroupReferences(group) == groupReferences);

    JSValueUnprotect(ctx, object);
    JSGlobalContextRelease(ctx);
    JSContextGroupRelease(group);
}

void TestCollectedObject() {
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef ctx = JSGlobalContextCreateInGroup(group, nullptr);
    const std::uint32_t groupReferences = GroupReferences(group);
    JSObjectRef object = JSObjectMake(ctx, nullptr, nullptr);
    JSWeakRef weak = JSWeakCreate(group, object);
    JSWeakRetain(group, weak);
    Require(JSWeakGetObject(weak) == object);

    JSGarbageCollect(ctx);
    Require(JSWeakGetObject(weak) == nullptr);
    Require(WeakReferences(weak) == 2);
    Require(Tracked(group, weak));

    JSWeakRelease(group, weak);
    Require(Tracked(group, weak));
    JSWeakRelease(group, weak);
    Require(group->weakRefs.empty());
    Require(GroupReferences(group) == groupReferences);

    JSGlobalContextRelease(ctx);
    JSContextGroupRelease(group);
}

}  // namespace

int main() {
    TestRetainAndReleaseCountTheWeak();
    TestGroupHeldOnlyByItsContext();
    TestNullArguments();
    TestCollectedObject();
    std::puts("libSceNKWebKit JSWeak tests passed");
    return 0;
}
