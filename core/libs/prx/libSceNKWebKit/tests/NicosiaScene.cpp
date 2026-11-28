#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <thread>
#include <vector>

using NKWebKit::NicosiaAbi::PlatformLayer;
using NKWebKit::NicosiaAbi::PlatformLayerVtable;
using NKWebKit::NicosiaAbi::Scene;
using NKWebKit::NicosiaAbi::SceneIntegration;
using NKWebKit::NicosiaAbi::SceneIntegrationClient;
using NKWebKit::NicosiaAbi::SceneIntegrationClientVtable;
using NKWebKit::NicosiaAbi::SceneIntegrationRef;
using NKWebKit::NicosiaAbi::SceneRef;
using NKWebKit::NicosiaAbi::SceneState;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "nicosia scene check failed at line %d\n", line);
        std::fflush(stderr);
        std::_Exit(1);
    }
}
#define Require(value) Check((value), __LINE__)

template<typename TFunction>
static bool ThrowsInvalidArgument(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template<typename TFunction>
static bool ThrowsRuntimeError(TFunction&& function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

struct TestLayer {
    PlatformLayer base;
    int name;
};

static int liveLayers = 0;
static std::vector<int> destroyed;
static std::function<void(TestLayer*)> onDestroy;

static void APS5_VABI LayerDestroy(PlatformLayer*) {
    Require(false);
}

static void APS5_VABI LayerDestroyAndFree(PlatformLayer* raw) {
    auto* layer = reinterpret_cast<TestLayer*>(raw);
    Require(layer->base.refCount == 1);
    destroyed.push_back(layer->name);
    if (onDestroy) onDestroy(layer);
    --liveLayers;
    delete layer;
}

static const PlatformLayerVtable layerVtable = {&LayerDestroy, &LayerDestroyAndFree};

static PlatformLayer* NewLayer(int name) {
    ++liveLayers;
    return &(new TestLayer{{&layerVtable, 1}, name})->base;
}

static void Drop(PlatformLayer* layer) {
    if (--layer->refCount) return;
    layer->refCount = 1;
    layer->vtable->destroyAndFree(layer);
}

static PlatformLayer* DeletedBucket() {
    return reinterpret_cast<PlatformLayer*>(~std::uintptr_t{0});
}

static bool Live(const PlatformLayer* layer) {
    return layer && layer != DeletedBucket();
}

struct TableModel {
    std::vector<PlatformLayer*> buckets;
    unsigned keys = 0;
    unsigned deleted = 0;

    static unsigned Hash(const PlatformLayer* layer) {
        std::uint64_t key = reinterpret_cast<std::uintptr_t>(layer);
        key += ~(key << 32);
        key ^= key >> 22;
        key += ~(key << 13);
        key ^= key >> 8;
        key += key << 3;
        key ^= key >> 15;
        key += ~(key << 27);
        key ^= key >> 31;
        return static_cast<unsigned>(key);
    }

    static unsigned Step(unsigned key) {
        key = ~key + (key >> 23);
        key ^= key << 12;
        key ^= key >> 7;
        key ^= key << 2;
        key ^= key >> 20;
        return key | 1;
    }

    static bool Full(std::uint64_t count, std::uint64_t size) {
        return size <= 1024 ? count * 4 >= size * 3 : count * 2 >= size;
    }

    static unsigned Best(unsigned count) {
        unsigned size = 1;
        while (size < count) size *= 2;
        if (Full(count, size)) size *= 2;
        const double maxLoad = size <= 1024 ? 0.75 : 0.5;
        const double average = (maxLoad + 1.0 / 6.0) / 2;
        if (count >= size * ((average + maxLoad) / 2)) size *= 2;
        return size < 8 ? 8 : size;
    }

    void Place(PlatformLayer* layer) {
        const unsigned mask = static_cast<unsigned>(buckets.size()) - 1;
        const unsigned hash = Hash(layer);
        unsigned index = hash & mask;
        while (buckets[index]) index = (index + Step(hash)) & mask;
        buckets[index] = layer;
    }

    void Resize(std::size_t size) {
        const std::vector<PlatformLayer*> old = buckets;
        buckets.assign(size, nullptr);
        deleted = 0;
        for (PlatformLayer* layer : old)
            if (Live(layer)) Place(layer);
    }

    void Add(PlatformLayer* layer) {
        if (buckets.empty()) buckets.assign(8, nullptr);
        Place(layer);
        ++keys;
        if (Full(keys + deleted, buckets.size())) Resize(keys * 6 < buckets.size() * 2 ? buckets.size() : buckets.size() * 2);
    }

    void Remove(PlatformLayer* layer) {
        for (PlatformLayer*& bucket : buckets)
            if (bucket == layer) {
                bucket = DeletedBucket();
                --keys;
                ++deleted;
                return;
            }
        Require(false);
    }

    static TableModel CopyOf(const TableModel& source) {
        TableModel copy;
        if (!source.keys) return copy;
        copy.buckets.assign(Best(source.keys), nullptr);
        copy.keys = source.keys;
        for (PlatformLayer* layer : source.buckets)
            if (Live(layer)) copy.Place(layer);
        return copy;
    }

    PlatformLayer** Materialize() const {
        if (buckets.empty()) return nullptr;
        auto* storage = static_cast<unsigned*>(_ZN3WTF16fastZeroedMallocEm(16 + buckets.size() * sizeof(PlatformLayer*)));
        storage[0] = deleted;
        storage[1] = keys;
        storage[2] = static_cast<unsigned>(buckets.size()) - 1;
        storage[3] = static_cast<unsigned>(buckets.size());
        auto** table = reinterpret_cast<PlatformLayer**>(storage + 4);
        for (std::size_t index = 0; index < buckets.size(); ++index) {
            table[index] = buckets[index];
            if (Live(buckets[index])) ++buckets[index]->refCount;
        }
        return table;
    }

    std::vector<int> Names() const {
        std::vector<int> names;
        for (PlatformLayer* layer : buckets)
            if (Live(layer)) names.push_back(reinterpret_cast<TestLayer*>(layer)->name);
        return names;
    }
};

static unsigned Meta(PlatformLayer** table, int index) {
    return reinterpret_cast<unsigned*>(table)[index];
}

static void RequireSameAs(const TableModel& expected, PlatformLayer** table) {
    if (expected.buckets.empty()) {
        Require(table == nullptr);
        return;
    }
    Require(table != nullptr);
    Require(Meta(table, -1) == expected.buckets.size() && Meta(table, -2) == expected.buckets.size() - 1);
    Require(Meta(table, -3) == expected.keys && Meta(table, -4) == 0);
    for (std::size_t index = 0; index < expected.buckets.size(); ++index) Require(table[index] == expected.buckets[index]);
}

struct StateStorage {
    alignas(8) unsigned char bytes[24];

    StateStorage() {
        std::memset(bytes, 0xA5, sizeof(bytes));
    }

    SceneState* get() {
        return reinterpret_cast<SceneState*>(bytes);
    }
};

struct Source {
    std::vector<PlatformLayer*> layers;
    TableModel model;
    PlatformLayer* root = nullptr;
    StateStorage storage;

    Source(unsigned count, unsigned removed, bool withRoot) {
        for (unsigned index = 0; index < count; ++index) {
            layers.push_back(NewLayer(static_cast<int>(index)));
            model.Add(layers.back());
        }
        for (unsigned index = 0; index < removed; ++index) model.Remove(layers[index]);
        SceneState& state = *storage.get();
        state.id = 77 + count;
        state.platformLayerUpdated = (count % 2) != 0;
        state.layers = model.Materialize();
        if (withRoot) {
            root = NewLayer(-1);
            ++root->refCount;
        }
        state.rootLayer = root;
    }

    bool Kept(std::size_t index) const {
        for (PlatformLayer* layer : model.buckets)
            if (layer == layers[index]) return true;
        return false;
    }

    void RequireCounts(unsigned inTable, unsigned rootCount) const {
        for (std::size_t index = 0; index < layers.size(); ++index) Require(layers[index]->refCount == (Kept(index) ? inTable : 1U));
        if (root) Require(root->refCount == rootCount);
    }

    void Release() {
        for (PlatformLayer* layer : layers) Drop(layer);
        if (root) Drop(root);
    }
};

static const auto stateDefault = {&_ZN7Nicosia5Scene5StateC1Ev, &_ZN7Nicosia5Scene5StateC2Ev};
static const auto stateCopy = {&_ZN7Nicosia5Scene5StateC1ERKS1_, &_ZN7Nicosia5Scene5StateC2ERKS1_};
static const auto stateDestroy = {&_ZN7Nicosia5Scene5StateD1Ev, &_ZN7Nicosia5Scene5StateD2Ev};
static const auto sceneConstruct = {&_ZN7Nicosia5SceneC1Ev, &_ZN7Nicosia5SceneC2Ev};
static const auto sceneDestroy = {&_ZN7Nicosia5SceneD1Ev, &_ZN7Nicosia5SceneD2Ev};
static const auto integrationConstruct = {&_ZN7Nicosia16SceneIntegrationC1ERNS_5SceneERNS0_6ClientE, &_ZN7Nicosia16SceneIntegrationC2ERNS_5SceneERNS0_6ClientE};
static const auto integrationDestroy = {&_ZN7Nicosia16SceneIntegrationD1Ev, &_ZN7Nicosia16SceneIntegrationD2Ev};
constexpr auto assign = &_ZN7Nicosia5Scene5StateaSERKS1_;
constexpr auto setClient = &_ZN7Nicosia16SceneIntegration9setClientERNS0_6ClientE;
constexpr auto invalidate = &_ZN7Nicosia16SceneIntegration10invalidateEv;
constexpr auto requestUpdate = &_ZN7Nicosia16SceneIntegration13requestUpdateEv;

static void TestADefaultState() {
    for (const auto construct : stateDefault)
        for (const auto destroy : stateDestroy) {
            StateStorage storage;
            construct(storage.get());
            const SceneState& state = *storage.get();
            Require(state.id == 0 && storage.bytes[4] == 0 && state.layers == nullptr && state.rootLayer == nullptr);
            destroy(storage.get());
            Require(state.layers == nullptr && state.rootLayer == nullptr);
        }
    Require(destroyed.empty());
}

static void TestAStateOwnsItsLayers() {
    for (const auto destroy : stateDestroy) {
        Source source(7, 2, true);
        SceneState& state = *source.storage.get();
        PlatformLayer** table = state.layers;
        const std::vector<int> order = source.model.Names();
        Require(order.size() == 5 && Meta(table, -4) == 2);
        ++source.layers[3]->refCount;
        for (std::size_t index = 0; index < source.layers.size(); ++index)
            if (index != 3 && source.Kept(index)) Drop(source.layers[index]);
        Drop(source.root);
        Require(destroyed.empty());
        bool rootSeen = false;
        onDestroy = [&](TestLayer* layer) {
            if (layer->name == -1) {
                rootSeen = true;
                Require(state.rootLayer == nullptr && destroyed.size() == 1);
            } else {
                Require(rootSeen);
            }
        };
        destroy(&state);
        onDestroy = nullptr;
        std::vector<int> expected{-1};
        for (const int name : order)
            if (name != 3) expected.push_back(name);
        Require(destroyed == expected && state.rootLayer == nullptr && state.layers == table);
        Require(source.layers[3]->refCount == 2);
        destroyed.clear();
        Drop(source.layers[3]);
        Drop(source.layers[3]);
        Drop(source.layers[0]);
        Drop(source.layers[1]);
        Require(destroyed.size() == 3 && liveLayers == 0);
        destroyed.clear();
    }
}

static void TestCopies() {
    Require(TableModel::Best(1) == 8 && TableModel::Best(4) == 8 && TableModel::Best(5) == 16 && TableModel::Best(6) == 16);
    Require(TableModel::Best(40) == 128 && TableModel::Best(900) == 4096 && TableModel::Best(2000) == 8192);
    struct Case {
        unsigned count;
        unsigned removed;
        bool withRoot;
        unsigned copySize;
    };
    const Case cases[] = {{0, 0, true, 0}, {1, 0, false, 8}, {4, 0, true, 8}, {5, 0, true, 16}, {6, 0, false, 16}, {9, 3, true, 16},
                          {40, 0, true, 128}, {47, 7, false, 128}, {900, 0, true, 4096}, {2000, 0, true, 8192}, {2, 2, true, 0}};
    for (const Case& item : cases)
        for (const auto copy : stateCopy) {
            Source source(item.count, item.removed, item.withRoot);
            SceneState& from = *source.storage.get();
            PlatformLayer** table = from.layers;
            StateStorage storage;
            copy(storage.get(), &from);
            SceneState& state = *storage.get();
            Require(state.id == from.id && state.platformLayerUpdated == from.platformLayerUpdated && state.rootLayer == source.root);
            const TableModel expected = TableModel::CopyOf(source.model);
            Require(expected.buckets.size() == item.copySize);
            RequireSameAs(expected, state.layers);
            Require(from.layers == table && from.rootLayer == source.root && from.id == 77 + item.count);
            if (table) {
                Require(state.layers != table && Meta(table, -3) == source.model.keys && Meta(table, -4) == source.model.deleted);
                for (std::size_t index = 0; index < source.model.buckets.size(); ++index) Require(table[index] == source.model.buckets[index]);
            }
            source.RequireCounts(3, 3);
            _ZN7Nicosia5Scene5StateD1Ev(&state);
            source.RequireCounts(2, 2);
            _ZN7Nicosia5Scene5StateD1Ev(&from);
            source.RequireCounts(1, 1);
            Require(destroyed.empty());
            source.Release();
            Require(liveLayers == 0);
            destroyed.clear();
        }
}

static void TestAssignment() {
    Source source(9, 3, true);
    Source target(6, 0, true);
    SceneState& from = *source.storage.get();
    SceneState& state = *target.storage.get();
    PlatformLayer** oldTable = state.layers;
    PlatformLayer* oldRoot = target.root;
    const std::vector<int> oldOrder = target.model.Names();
    for (PlatformLayer* layer : target.layers) Drop(layer);
    Drop(target.root);
    Require(destroyed.empty());
    const TableModel expected = TableModel::CopyOf(source.model);
    onDestroy = [&](TestLayer* layer) {
        Require(state.layers != oldTable && state.id == from.id);
        RequireSameAs(expected, state.layers);
        if (layer->name == -1)
            Require(state.rootLayer == source.root && source.root->refCount == 3 && destroyed.size() == 7);
        else
            Require(state.rootLayer == oldRoot);
    };
    Require(assign(&state, &from) == &state);
    onDestroy = nullptr;
    std::vector<int> expectedOrder = oldOrder;
    expectedOrder.push_back(-1);
    Require(destroyed == expectedOrder && liveLayers == 10);
    destroyed.clear();
    Require(state.id == from.id && state.platformLayerUpdated == from.platformLayerUpdated && state.rootLayer == source.root);
    RequireSameAs(expected, state.layers);
    source.RequireCounts(3, 3);

    Require(assign(&state, &state) == &state);
    Require(state.layers != nullptr && state.rootLayer == source.root);
    RequireSameAs(TableModel::CopyOf(expected), state.layers);
    source.RequireCounts(3, 3);

    StateStorage emptyStorage;
    _ZN7Nicosia5Scene5StateC1Ev(emptyStorage.get());
    Require(assign(&state, emptyStorage.get()) == &state);
    Require(state.id == 0 && !state.platformLayerUpdated && state.layers == nullptr && state.rootLayer == nullptr);
    source.RequireCounts(2, 2);
    Require(assign(&state, &from) == &state);
    source.RequireCounts(3, 3);
    RequireSameAs(expected, state.layers);
    _ZN7Nicosia5Scene5StateD2Ev(&state);
    _ZN7Nicosia5Scene5StateD2Ev(&from);
    Require(destroyed.empty());
    source.Release();
    target.layers.clear();
    target.root = nullptr;
    Require(liveLayers == 0);
    destroyed.clear();
}

static void GuestDeref(Scene* scene) {
    if (--scene->refCount) return;
    scene->refCount = 1;
    _ZN7Nicosia5SceneD1Ev(scene);
    _ZN3WTF8fastFreeEPv(scene);
}

static void TestTheScene() {
    for (const auto construct : sceneConstruct)
        for (const auto destroy : sceneDestroy) {
            alignas(8) unsigned char bytes[40];
            std::memset(bytes, 0xA5, sizeof(bytes));
            auto* scene = reinterpret_cast<Scene*>(bytes);
            construct(scene);
            Require(scene->refCount == 1 && scene->scene.lock == 0 && scene->scene.state.id == 0 && !scene->scene.state.platformLayerUpdated);
            Require(scene->scene.state.layers == nullptr && scene->scene.state.rootLayer == nullptr);
            Source source(6, 1, true);
            scene->scene.state = *source.storage.get();
            destroy(scene);
            Require(scene->scene.state.rootLayer == nullptr && scene->refCount == 1 && scene->scene.lock == 0);
            source.RequireCounts(1, 1);
            source.Release();
            Require(liveLayers == 0);
            destroyed.clear();
        }
    alignas(8) unsigned char refBytes[8];
    std::memset(refBytes, 0xA5, sizeof(refBytes));
    auto* reference = reinterpret_cast<SceneRef*>(refBytes);
    Require(_ZN7Nicosia5Scene6createEv(reference) == reference);
    Scene* scene = reference->ptr;
    Require(scene && scene->refCount == 1 && scene->scene.lock == 0 && scene->scene.state.id == 0 && !scene->scene.state.platformLayerUpdated);
    Require(scene->scene.state.layers == nullptr && scene->scene.state.rootLayer == nullptr);
    SceneRef second{};
    _ZN7Nicosia5Scene6createEv(&second);
    Require(second.ptr && second.ptr != scene);
    GuestDeref(second.ptr);
    GuestDeref(scene);
}

struct TestClient {
    SceneIntegrationClient base;
    std::atomic<int> updates{0};
    SceneIntegration* integration = nullptr;
    bool lockWasHeld = false;
    std::function<void()> body;
};

static void APS5_VABI ClientDestroy(SceneIntegrationClient*) {
    Require(false);
}

static void APS5_VABI ClientRequestUpdate(SceneIntegrationClient* raw) {
    auto* client = reinterpret_cast<TestClient*>(raw);
    ++client->updates;
    if (client->integration) client->lockWasHeld = client->integration->client.lock == 1;
    if (client->body) client->body();
}

static const SceneIntegrationClientVtable clientVtable = {&ClientDestroy, &ClientDestroy, &ClientRequestUpdate};

static Scene* NewScene() {
    SceneRef reference{};
    _ZN7Nicosia5Scene6createEv(&reference);
    return reference.ptr;
}

static void TestTheIntegration() {
    for (const auto construct : integrationConstruct)
        for (const auto destroy : integrationDestroy) {
            Scene* scene = NewScene();
            TestClient first{{&clientVtable}};
            TestClient second{{&clientVtable}};
            alignas(8) unsigned char bytes[32];
            std::memset(bytes, 0xA5, sizeof(bytes));
            auto* integration = reinterpret_cast<SceneIntegration*>(bytes);
            scene->scene.state.rootLayer = NewLayer(5);
            construct(integration, scene, &first.base);
            Require(integration->refCount == 1 && integration->client.lock == 0);
            Require(integration->client.scene == scene && integration->client.object == &first.base && scene->refCount == 2);
            first.integration = integration;
            requestUpdate(integration);
            Require(first.updates.load() == 1 && first.lockWasHeld && integration->client.lock == 0);
            setClient(integration, &second.base);
            Require(integration->client.object == &second.base && integration->client.lock == 0 && integration->client.scene == scene);
            requestUpdate(integration);
            requestUpdate(integration);
            Require(first.updates.load() == 1 && second.updates.load() == 2);
            destroy(integration);
            Require(integration->client.scene == nullptr && integration->client.object == &second.base && scene->refCount == 1);
            destroy(integration);
            Require(scene->refCount == 1 && destroyed.empty() && liveLayers == 1);
            GuestDeref(scene);
            Require(destroyed == std::vector<int>{5} && liveLayers == 0);
            destroyed.clear();
        }

    Scene* scene = NewScene();
    TestClient client{{&clientVtable}};
    alignas(8) unsigned char refBytes[8];
    std::memset(refBytes, 0xA5, sizeof(refBytes));
    auto* reference = reinterpret_cast<SceneIntegrationRef*>(refBytes);
    Require(_ZN7Nicosia16SceneIntegration6createERNS_5SceneERNS0_6ClientE(reference, scene, &client.base) == reference);
    SceneIntegration* integration = reference->ptr;
    Require(integration && integration->refCount == 1 && integration->client.lock == 0);
    Require(integration->client.scene == scene && integration->client.object == &client.base && scene->refCount == 2);
    invalidate(integration);
    Require(integration->client.scene == nullptr && integration->client.object == nullptr && integration->client.lock == 0 && scene->refCount == 1);
    requestUpdate(integration);
    Require(client.updates.load() == 0);
    invalidate(integration);
    Require(scene->refCount == 1);
    setClient(integration, &client.base);
    requestUpdate(integration);
    Require(client.updates.load() == 1 && integration->client.scene == nullptr);
    _ZN7Nicosia16SceneIntegrationD1Ev(integration);
    _ZN3WTF8fastFreeEPv(integration);
    GuestDeref(scene);
}

static void TestTheLastReferenceToAScene() {
    for (int mode = 0; mode < 2; ++mode) {
        Scene* scene = NewScene();
        Source source(4, 0, true);
        scene->scene.state = *source.storage.get();
        const std::vector<int> order = source.model.Names();
        TestClient client{{&clientVtable}};
        SceneIntegrationRef reference{};
        _ZN7Nicosia16SceneIntegration6createERNS_5SceneERNS0_6ClientE(&reference, scene, &client.base);
        SceneIntegration* integration = reference.ptr;
        --scene->refCount;
        source.Release();
        Require(destroyed.empty() && liveLayers == 5);
        onDestroy = [&](TestLayer*) {
            Require(integration->client.scene == nullptr && scene->refCount == 1);
            Require(integration->client.lock == (mode == 0 ? 1 : 0));
        };
        if (mode == 0)
            invalidate(integration);
        else
            _ZN7Nicosia16SceneIntegrationD2Ev(integration);
        onDestroy = nullptr;
        std::vector<int> expected{-1};
        expected.insert(expected.end(), order.begin(), order.end());
        Require(destroyed == expected && liveLayers == 0 && integration->client.lock == 0);
        destroyed.clear();
        if (mode == 0) _ZN7Nicosia16SceneIntegrationD1Ev(integration);
        _ZN3WTF8fastFreeEPv(integration);
    }
}

static void TestTheLockExcludes() {
    Scene* scene = NewScene();
    TestClient first{{&clientVtable}};
    TestClient second{{&clientVtable}};
    SceneIntegrationRef reference{};
    _ZN7Nicosia16SceneIntegration6createERNS_5SceneERNS0_6ClientE(&reference, scene, &first.base);
    SceneIntegration* integration = reference.ptr;
    for (int round = 0; round < 3; ++round) {
        std::atomic<bool> inside{false};
        std::atomic<bool> release{false};
        std::atomic<bool> finished{false};
        first.updates = 0;
        first.body = [&] {
            if (first.updates.load() != 1) return;
            inside = true;
            while (!release) std::this_thread::yield();
        };
        std::thread holder([&] { requestUpdate(integration); });
        while (!inside) std::this_thread::yield();
        std::thread waiter([&] {
            if (round == 0)
                setClient(integration, &second.base);
            else if (round == 1)
                requestUpdate(integration);
            else
                invalidate(integration);
            finished = true;
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        Require(!finished && integration->client.lock == 1 && integration->client.scene == scene);
        Require(integration->client.object == &first.base && first.updates.load() == 1 && second.updates.load() == 0);
        release = true;
        holder.join();
        waiter.join();
        Require(finished && integration->client.lock == 0);
        if (round == 0) {
            Require(integration->client.object == &second.base);
            setClient(integration, &first.base);
        } else if (round == 1) {
            Require(first.updates.load() == 2);
        } else {
            Require(integration->client.object == nullptr && integration->client.scene == nullptr && scene->refCount == 1);
        }
        first.body = nullptr;
    }
    _ZN7Nicosia16SceneIntegrationD1Ev(integration);
    _ZN3WTF8fastFreeEPv(integration);
    GuestDeref(scene);
}

static void TestParkedThreads() {
    Scene* scene = NewScene();
    TestClient client{{&clientVtable}};
    TestClient other{{&clientVtable}};
    SceneIntegrationRef reference{};
    _ZN7Nicosia16SceneIntegration6createERNS_5SceneERNS0_6ClientE(&reference, scene, &client.base);
    SceneIntegration* integration = reference.ptr;
    integration->client.lock = 2;
    Require(ThrowsRuntimeError([&] { requestUpdate(integration); }));
    Require(ThrowsRuntimeError([&] { setClient(integration, &other.base); }));
    Require(ThrowsRuntimeError([&] { invalidate(integration); }));
    Require(integration->client.lock == 2 && integration->client.object == &client.base && integration->client.scene == scene);
    Require(client.updates.load() == 0 && scene->refCount == 2);
    integration->client.lock = 0;
    client.integration = integration;
    client.body = [&] { integration->client.lock = 3; };
    requestUpdate(integration);
    Require(integration->client.lock == 2 && client.updates.load() == 1);
    integration->client.lock = 0;
    _ZN7Nicosia16SceneIntegrationD1Ev(integration);
    _ZN3WTF8fastFreeEPv(integration);
    GuestDeref(scene);
}

static void TestInvalidArguments() {
    StateStorage storage;
    _ZN7Nicosia5Scene5StateC1Ev(storage.get());
    Scene* scene = NewScene();
    TestClient client{{&clientVtable}};
    alignas(8) unsigned char bytes[32] = {};
    auto* integration = reinterpret_cast<SceneIntegration*>(bytes);
    SceneIntegrationRef reference{};
    for (const auto construct : stateDefault) Require(ThrowsInvalidArgument([&] { construct(nullptr); }));
    for (const auto copy : stateCopy) {
        Require(ThrowsInvalidArgument([&] { copy(nullptr, storage.get()); }));
        Require(ThrowsInvalidArgument([&] { copy(storage.get(), nullptr); }));
    }
    for (const auto destroy : stateDestroy) Require(ThrowsInvalidArgument([&] { destroy(nullptr); }));
    Require(ThrowsInvalidArgument([&] { assign(nullptr, storage.get()); }));
    Require(ThrowsInvalidArgument([&] { assign(storage.get(), nullptr); }));
    for (const auto construct : sceneConstruct) Require(ThrowsInvalidArgument([&] { construct(nullptr); }));
    for (const auto destroy : sceneDestroy) Require(ThrowsInvalidArgument([&] { destroy(nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7Nicosia5Scene6createEv(nullptr); }));
    for (const auto construct : integrationConstruct) {
        Require(ThrowsInvalidArgument([&] { construct(nullptr, scene, &client.base); }));
        Require(ThrowsInvalidArgument([&] { construct(integration, nullptr, &client.base); }));
        Require(ThrowsInvalidArgument([&] { construct(integration, scene, nullptr); }));
    }
    for (const auto destroy : integrationDestroy) Require(ThrowsInvalidArgument([&] { destroy(nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7Nicosia16SceneIntegration6createERNS_5SceneERNS0_6ClientE(nullptr, scene, &client.base); }));
    Require(ThrowsInvalidArgument([&] { _ZN7Nicosia16SceneIntegration6createERNS_5SceneERNS0_6ClientE(&reference, nullptr, &client.base); }));
    Require(ThrowsInvalidArgument([&] { _ZN7Nicosia16SceneIntegration6createERNS_5SceneERNS0_6ClientE(&reference, scene, nullptr); }));
    Require(reference.ptr == nullptr && scene->refCount == 1);
    _ZN7Nicosia16SceneIntegrationC1ERNS_5SceneERNS0_6ClientE(integration, scene, &client.base);
    Require(ThrowsInvalidArgument([&] { setClient(nullptr, &client.base); }));
    Require(ThrowsInvalidArgument([&] { setClient(integration, nullptr); }));
    Require(ThrowsInvalidArgument([&] { invalidate(nullptr); }));
    Require(ThrowsInvalidArgument([&] { requestUpdate(nullptr); }));
    Require(integration->client.object == &client.base && integration->client.lock == 0 && scene->refCount == 2);
    _ZN7Nicosia16SceneIntegrationD1Ev(integration);
    GuestDeref(scene);
}

int main() {
    std::thread([] {
        std::this_thread::sleep_for(std::chrono::seconds(20));
        std::fprintf(stderr, "nicosia scene test timed out\n");
        std::fflush(stderr);
        std::_Exit(3);
    }).detach();
    TestADefaultState();
    TestAStateOwnsItsLayers();
    TestCopies();
    TestAssignment();
    TestTheScene();
    TestTheIntegration();
    TestTheLastReferenceToAScene();
    TestTheLockExcludes();
    TestParkedThreads();
    TestInvalidArguments();
    Require(liveLayers == 0 && destroyed.empty());
    std::puts("libSceNKWebKit nicosia scene tests passed");
    return 0;
}
