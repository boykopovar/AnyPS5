#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::NicosiaAbi {

struct PlatformLayer;

struct PlatformLayerVtable {
    void (APS5_VABI* destroy)(PlatformLayer* self);
    void (APS5_VABI* destroyAndFree)(PlatformLayer* self);
};

struct PlatformLayer {
    const PlatformLayerVtable* vtable;
    unsigned refCount;
};

struct SceneState {
    std::uint32_t id;
    bool platformLayerUpdated;
    PlatformLayer** layers;
    PlatformLayer* rootLayer;
};

struct SceneData {
    std::uint8_t lock;
    SceneState state;
};

struct Scene {
    unsigned refCount;
    SceneData scene;
};

struct SceneRef {
    Scene* ptr;
};

struct SceneIntegrationClient;

struct SceneIntegrationClientVtable {
    void (APS5_VABI* destroy)(SceneIntegrationClient* self);
    void (APS5_VABI* destroyAndFree)(SceneIntegrationClient* self);
    void (APS5_VABI* requestUpdate)(SceneIntegrationClient* self);
};

struct SceneIntegrationClient {
    const SceneIntegrationClientVtable* vtable;
};

struct SceneIntegrationData {
    std::uint8_t lock;
    Scene* scene;
    SceneIntegrationClient* object;
};

struct SceneIntegration {
    unsigned refCount;
    SceneIntegrationData client;
};

struct SceneIntegrationRef {
    SceneIntegration* ptr;
};

static_assert(offsetof(PlatformLayer, refCount) == 8);
static_assert(sizeof(SceneState) == 24);
static_assert(offsetof(SceneState, platformLayerUpdated) == 4);
static_assert(offsetof(SceneState, layers) == 8);
static_assert(offsetof(SceneState, rootLayer) == 16);
static_assert(sizeof(Scene) == 40);
static_assert(offsetof(Scene, scene) == 8);
static_assert(offsetof(SceneData, state) == 8);
static_assert(sizeof(SceneIntegration) == 32);
static_assert(offsetof(SceneIntegration, client) == 8);
static_assert(offsetof(SceneIntegrationData, scene) == 8);
static_assert(offsetof(SceneIntegrationData, object) == 16);
static_assert(offsetof(SceneIntegrationClientVtable, requestUpdate) == 16);
static_assert(std::is_trivially_copyable_v<SceneState>);

}  // namespace NKWebKit::NicosiaAbi
