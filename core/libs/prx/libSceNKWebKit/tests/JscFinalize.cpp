#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>

using namespace NKWebKit;

namespace {

constexpr int BaseClass = 1;
constexpr int DerivedClass = 2;

struct Event {
    int classId;
    JSObjectRef object;
    void* privateData;
};

std::vector<Event> events;
void* replacementData = nullptr;
bool replaceResult = false;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JS finalize check failed at line %u\n", location.line());
    std::abort();
}

void APS5_VABI BaseFinalize(JSObjectRef object) {
    events.push_back({BaseClass, object, JSObjectGetPrivate(object)});
}

void APS5_VABI DerivedFinalize(JSObjectRef object) {
    events.push_back({DerivedClass, object, JSObjectGetPrivate(object)});
}

void APS5_VABI ReplacingFinalize(JSObjectRef object) {
    events.push_back({DerivedClass, object, JSObjectGetPrivate(object)});
    replaceResult = JSObjectSetPrivate(object, replacementData);
}

JSClassRef MakeClass(const char* name, JSObjectFinalizeCallback finalize, JSClassRef parent = nullptr) {
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = name;
    definition.finalize = finalize;
    definition.parentClass = parent;
    JSClassRef jsClass = JSClassCreate(&definition);
    Require(jsClass != nullptr);
    return jsClass;
}

bool Saw(JSObjectRef object, void* privateData) {
    return std::count_if(events.begin(), events.end(), [&](const Event& event) {
        return event.object == object && event.privateData == privateData;
    }) == 1;
}

void TestContextReleaseWithoutCollection() {
    events.clear();
    JSClassRef jsClass = MakeClass("Tracked", &BaseFinalize);
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    int data = 7;
    JSObjectRef object = JSObjectMake(ctx, jsClass, &data);
    Require(object != nullptr);
    Require(JSObjectGetPrivate(object) == &data);
    Require(events.empty());
    JSGlobalContextRelease(ctx);
    Require(events.size() == 1);
    Require(events[0].classId == BaseClass);
    Require(events[0].object == object);
    Require(events[0].privateData == &data);
    JSClassRelease(jsClass);
}

void TestGroupReleaseWithoutCollection() {
    events.clear();
    JSClassRef jsClass = MakeClass("Tracked", &BaseFinalize);
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef ctx = JSGlobalContextCreateInGroup(group, nullptr);
    int data = 9;
    JSObjectRef object = JSObjectMake(ctx, jsClass, &data);
    JSClassRelease(jsClass);
    JSGlobalContextRelease(ctx);
    Require(events.empty());
    JSContextGroupRelease(group);
    Require(events.size() == 1);
    Require(events[0].object == object);
    Require(events[0].privateData == &data);
}

void TestParentChain() {
    events.clear();
    JSClassRef base = MakeClass("Base", &BaseFinalize);
    JSClassRef derived = MakeClass("Derived", &DerivedFinalize, base);
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    int data = 11;
    JSObjectRef object = JSObjectMake(ctx, derived, &data);
    JSGlobalContextRelease(ctx);
    Require(events.size() == 2);
    Require(events[0].classId == DerivedClass && events[1].classId == BaseClass);
    Require(events[0].object == object && events[1].object == object);
    Require(events[0].privateData == &data && events[1].privateData == &data);
    JSClassRelease(derived);
    JSClassRelease(base);
}

void TestSetPrivateDuringFinalize() {
    events.clear();
    JSClassRef base = MakeClass("Base", &BaseFinalize);
    JSClassRef derived = MakeClass("Replacing", &ReplacingFinalize, base);
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    int data = 13;
    int replacement = 17;
    replacementData = &replacement;
    replaceResult = false;
    JSObjectRef object = JSObjectMake(ctx, derived, &data);
    Require(object != nullptr);
    JSGlobalContextRelease(ctx);
    Require(events.size() == 2);
    Require(events[0].classId == DerivedClass && events[0].privateData == &data);
    Require(replaceResult);
    Require(events[1].classId == BaseClass && events[1].privateData == &replacement);
    replacementData = nullptr;
    JSClassRelease(derived);
    JSClassRelease(base);
}

void TestCollectedObjectIsNotFinalizedAgain() {
    events.clear();
    JSClassRef jsClass = MakeClass("Tracked", &BaseFinalize);
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    int collectedData = 19;
    int keptData = 23;
    JSObjectRef collected = JSObjectMake(ctx, jsClass, &collectedData);
    JSObjectRef kept = JSObjectMake(ctx, jsClass, &keptData);
    JSValueProtect(ctx, kept);
    JSGarbageCollect(ctx);
    Require(events.size() == 1);
    Require(events[0].object == collected && events[0].privateData == &collectedData);
    JSGlobalContextRelease(ctx);
    Require(events.size() == 2);
    Require(events[1].object == kept && events[1].privateData == &keptData);
    JSClassRelease(jsClass);
}

void TestSeveralObjects() {
    events.clear();
    JSClassRef jsClass = MakeClass("Tracked", &BaseFinalize);
    JSClassRef plainClass = MakeClass("Plain", nullptr);
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    int first = 1;
    int second = 2;
    int third = 3;
    JSObjectRef firstObject = JSObjectMake(ctx, jsClass, &first);
    JSObjectRef secondObject = JSObjectMake(ctx, jsClass, &second);
    JSObjectRef thirdObject = JSObjectMake(ctx, jsClass, &third);
    JSObjectRef withoutData = JSObjectMake(ctx, jsClass, nullptr);
    JSObjectRef withoutFinalize = JSObjectMake(ctx, plainClass, &first);
    JSObjectRef withoutClass = JSObjectMake(ctx, nullptr, nullptr);
    Require(withoutFinalize != nullptr && withoutClass != nullptr);
    Require(firstObject != secondObject && secondObject != thirdObject && thirdObject != withoutData);
    JSGlobalContextRelease(ctx);
    Require(events.size() == 4);
    Require(Saw(firstObject, &first));
    Require(Saw(secondObject, &second));
    Require(Saw(thirdObject, &third));
    Require(Saw(withoutData, nullptr));
    JSClassRelease(plainClass);
    JSClassRelease(jsClass);
}

void TestGlobalObjectClass() {
    events.clear();
    JSClassRef jsClass = MakeClass("Global", &BaseFinalize);
    JSGlobalContextRef ctx = JSGlobalContextCreate(jsClass);
    int data = 29;
    JSObjectRef global = JSContextGetGlobalObject(ctx);
    Require(JSObjectSetPrivate(global, &data));
    JSGlobalContextRelease(ctx);
    Require(events.size() == 1);
    Require(events[0].object == global && events[0].privateData == &data);
    JSClassRelease(jsClass);
}

}  // namespace

int main() {
    TestContextReleaseWithoutCollection();
    TestGroupReleaseWithoutCollection();
    TestParentChain();
    TestSetPrivateDuringFinalize();
    TestCollectedObjectIsNotFinalizedAgain();
    TestSeveralObjects();
    TestGlobalObjectClass();
    std::puts("libSceNKWebKit JS finalize tests passed");
    return 0;
}
