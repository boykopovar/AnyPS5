#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>

using namespace NKWebKit;

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JscContextSmall check failed at line %u\n", location.line());
    std::abort();
}

struct Fixture {
    JSContextGroupRef group;
    JSGlobalContextRef ctx;

    Fixture() : group(JSContextGroupCreate()), ctx(JSGlobalContextCreateInGroup(group, nullptr)) {
        Require(group != nullptr && ctx != nullptr);
    }
    Fixture(const Fixture&) = delete;
    ~Fixture() {
        JSGlobalContextRelease(ctx);
        JSContextGroupRelease(group);
    }
    OpaqueJSContextGroup* State() const { return const_cast<OpaqueJSContextGroup*>(group); }
};

struct MarkState {
    JSObjectRef kept = nullptr;
    int runs = 0;
    bool nullMarked = false;
    bool keptMarked = false;
};

void APS5_VABI Constraint(JSMarker* marker, void* userData) {
    auto* state = static_cast<MarkState*>(userData);
    ++state->runs;
    state->nullMarked = marker->IsMarked(marker, nullptr);
    state->keptMarked = marker->IsMarked(marker, state->kept);
}

void TestNullObjectIsMarked() {
    Fixture f;
    MarkState state;
    state.kept = JSObjectMake(f.ctx, nullptr, nullptr);
    Require(state.kept != nullptr);
    JSValueProtect(f.ctx, state.kept);
    JSContextGroupAddMarkingConstraint(f.group, &Constraint, &state);
    JSGarbageCollect(f.ctx);
    Require(state.runs >= 1);
    Require(state.nullMarked);
    Require(state.keptMarked);
}

void APS5_VABI Finalizer(JSContextGroupRef group, void* userData) {
    (void)group;
    ++*static_cast<int*>(userData);
}

void TestHeapFinalizerAddedTwice() {
    Fixture f;
    int first = 0;
    int second = 0;
    JSContextGroupAddHeapFinalizer(f.group, &Finalizer, &first);
    JSContextGroupAddHeapFinalizer(f.group, &Finalizer, &first);
    JSContextGroupAddHeapFinalizer(f.group, &Finalizer, &second);
    Require(f.State()->heapFinalizers.size() == 3);
    JSGarbageCollect(f.ctx);
    Require(first == 2 && second == 1);

    JSContextGroupRemoveHeapFinalizer(f.group, &Finalizer, &first);
    Require(f.State()->heapFinalizers.size() == 2);
    JSGarbageCollect(f.ctx);
    Require(first == 3 && second == 2);

    JSContextGroupRemoveHeapFinalizer(f.group, &Finalizer, &first);
    JSGarbageCollect(f.ctx);
    Require(first == 3 && second == 3);

    JSContextGroupRemoveHeapFinalizer(f.group, &Finalizer, &first);
    Require(f.State()->heapFinalizers.size() == 1);
    JSGarbageCollect(f.ctx);
    Require(first == 3 && second == 4);

    JSContextGroupRemoveHeapFinalizer(f.group, &Finalizer, &second);
    Require(f.State()->heapFinalizers.empty());
    JSGarbageCollect(f.ctx);
    Require(first == 3 && second == 4);
}

void TestWeakMapKeepsEntryWhenSetToNull() {
    Fixture f;
    auto* map = new OpaqueJSWeakObjectMap();
    map->group = f.State();
    f.State()->weakMaps.push_back(map);
    JSObjectRef a = JSObjectMake(f.ctx, nullptr, nullptr);
    JSObjectRef b = JSObjectMake(f.ctx, nullptr, nullptr);
    Require(a != nullptr && b != nullptr && a != b);
    JSValueProtect(f.ctx, a);
    JSValueProtect(f.ctx, b);
    int key1 = 0;
    int key2 = 0;
    int key3 = 0;

    JSWeakObjectMapSet(f.ctx, map, &key1, a);
    Require(JSWeakObjectMapGet(f.ctx, map, &key1) == a);
    JSWeakObjectMapSet(f.ctx, map, &key1, nullptr);
    Require(JSWeakObjectMapGet(f.ctx, map, &key1) == a);
    Require(map->entries.size() == 1);

    JSWeakObjectMapSet(f.ctx, map, &key2, nullptr);
    Require(map->entries.size() == 1);
    Require(JSWeakObjectMapGet(f.ctx, map, &key2) == nullptr);
    Require(!JSWeakObjectMapRemove(f.ctx, map, &key2));

    JSWeakObjectMapSet(f.ctx, map, &key1, b);
    Require(JSWeakObjectMapGet(f.ctx, map, &key1) == b);
    JSWeakObjectMapSet(nullptr, map, &key3, a);
    Require(JSWeakObjectMapGet(f.ctx, map, &key3) == nullptr);
    Require(map->entries.size() == 1);

    Require(JSWeakObjectMapRemove(f.ctx, map, &key1));
    Require(JSWeakObjectMapGet(f.ctx, map, &key1) == nullptr);
    Require(map->entries.empty());
    delete map;
}

}  // namespace

int main() {
    TestNullObjectIsMarked();
    TestHeapFinalizerAddedTwice();
    TestWeakMapKeepsEntryWhenSetToNull();
    std::puts("libSceNKWebKit JS context small tests passed");
    return 0;
}
