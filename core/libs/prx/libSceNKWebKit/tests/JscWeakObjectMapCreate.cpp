#include "../src/Jsc.hpp"
#include "prx/libSceJsc/include/ApiBridgeExports.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

using namespace NKWebKit;

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSWeakObjectMapCreate check failed at line %u\n", location.line());
    std::abort();
}

struct DestroyedRecord {
    unsigned calls = 0;
    void* map = nullptr;
    void* privateData = nullptr;
};

DestroyedRecord destroyed;

void APS5_VABI OnDestroyed(void* map, void* privateData) {
    ++destroyed.calls;
    destroyed.map = map;
    destroyed.privateData = privateData;
}

bool Tracked(JSContextGroupRef group, const void* map) {
    const auto& maps = group->weakMaps;
    return std::find(maps.begin(), maps.end(), map) != maps.end();
}

void TestCreateBindsTheMapToTheGroup() {
    destroyed = {};
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef ctx = JSGlobalContextCreateInGroup(group, nullptr);
    int privateData = 7;

    auto* map = static_cast<OpaqueJSWeakObjectMap*>(
        JSWeakObjectMapCreate(ctx, &privateData, reinterpret_cast<Jsc::WeakMapDestroyedCallback>(&OnDestroyed)));
    Require(map != nullptr);
    Require(map->group == group);
    Require(map->privateData == &privateData);
    Require(map->destroyedCallback == reinterpret_cast<JSWeakMapDestroyedCallback>(&OnDestroyed));
    Require(map->entries.empty());
    Require(Tracked(group, map));
    Require(destroyed.calls == 0);

    JSGlobalContextRelease(ctx);
    JSContextGroupRelease(group);
    Require(destroyed.calls == 1);
    Require(destroyed.map == map);
    Require(destroyed.privateData == &privateData);
}

void TestEntriesOfAFreshMapAreWeak() {
    destroyed = {};
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef ctx = JSGlobalContextCreateInGroup(group, nullptr);

    auto* map = static_cast<OpaqueJSWeakObjectMap*>(
        JSWeakObjectMapCreate(ctx, nullptr, reinterpret_cast<Jsc::WeakMapDestroyedCallback>(&OnDestroyed)));
    Require(map != nullptr);

    int key = 0;
    int otherKey = 1;
    JSObjectRef object = JSObjectMake(ctx, nullptr, nullptr);
    Require(object != nullptr);
    JSWeakObjectMapSet(ctx, map, &key, object);
    Require(JSWeakObjectMapGet(ctx, map, &key) == object);
    Require(map->entries.size() == 1);

    Require(JSWeakObjectMapRemove(ctx, map, &key));
    Require(map->entries.empty());
    Require(JSWeakObjectMapGet(ctx, map, &key) == nullptr);

    JSWeakObjectMapSet(ctx, map, &key, object);
    JSWeakObjectMapSet(ctx, map, &otherKey, object);
    Require(map->entries.size() == 2);
    JSGarbageCollect(ctx);
    Require(!object->alive);
    Require(map->entries.empty());
    Require(JSWeakObjectMapGet(ctx, map, &key) == nullptr);
    Require(!JSWeakObjectMapRemove(ctx, map, &key));

    JSGlobalContextRelease(ctx);
    JSContextGroupRelease(group);
    Require(destroyed.calls == 1);
}

void TestMapStaysAliveWhileAContextRetainsItsGroup() {
    destroyed = {};
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef creator = JSGlobalContextCreateInGroup(group, nullptr);
    JSGlobalContextRef survivor = JSGlobalContextCreateInGroup(group, nullptr);
    auto* map = static_cast<OpaqueJSWeakObjectMap*>(
        JSWeakObjectMapCreate(creator, nullptr, reinterpret_cast<Jsc::WeakMapDestroyedCallback>(&OnDestroyed)));
    Require(map != nullptr);

    JSContextGroupRelease(group);
    JSGlobalContextRelease(creator);
    Require(destroyed.calls == 0);

    int key = 0;
    JSObjectRef object = JSObjectMake(survivor, nullptr, nullptr);
    Require(object != nullptr);
    JSWeakObjectMapSet(survivor, map, &key, object);
    Require(JSWeakObjectMapGet(survivor, map, &key) == object);
    Require(JSWeakObjectMapRemove(survivor, map, &key));
    Require(JSWeakObjectMapGet(survivor, map, &key) == nullptr);

    JSGlobalContextRelease(survivor);
    Require(destroyed.calls == 1);
}

void TestEveryMapIsDestroyedOnce() {
    destroyed = {};
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef ctx = JSGlobalContextCreateInGroup(group, nullptr);

    auto* first = static_cast<OpaqueJSWeakObjectMap*>(
        JSWeakObjectMapCreate(ctx, nullptr, reinterpret_cast<Jsc::WeakMapDestroyedCallback>(&OnDestroyed)));
    auto* second = static_cast<OpaqueJSWeakObjectMap*>(
        JSWeakObjectMapCreate(ctx, nullptr, reinterpret_cast<Jsc::WeakMapDestroyedCallback>(&OnDestroyed)));
    Require(first != nullptr && second != nullptr && first != second);
    Require(group->weakMaps.size() == 2);

    JSGlobalContextRelease(ctx);
    JSContextGroupRelease(group);
    Require(destroyed.calls == 2);
}

void TestNullDestructorIsAccepted() {
    destroyed = {};
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef ctx = JSGlobalContextCreateInGroup(group, nullptr);

    auto* map = static_cast<OpaqueJSWeakObjectMap*>(JSWeakObjectMapCreate(ctx, nullptr, nullptr));
    Require(map != nullptr);
    Require(map->destroyedCallback == nullptr);
    Require(Tracked(group, map));

    JSGlobalContextRelease(ctx);
    JSContextGroupRelease(group);
    Require(destroyed.calls == 0);
}

void TestNullContextThrows() {
    bool threw = false;
    try {
        JSWeakObjectMapCreate(nullptr, nullptr, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    TestCreateBindsTheMapToTheGroup();
    TestEntriesOfAFreshMapAreWeak();
    TestMapStaysAliveWhileAContextRetainsItsGroup();
    TestEveryMapIsDestroyedOnce();
    TestNullDestructorIsAccepted();
    TestNullContextThrows();
    std::puts("libSceNKWebKit JSWeakObjectMapCreate tests passed");
    return 0;
}
