#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <cstddef>

namespace NKWebKit::TiledBackingStoreAbi {

struct TiledBackingStore {
    void* client;
    void* tiles;
    WebCore::IntSize tileSize;
    float coverAreaMultiplier;
    WebCore::FloatPoint trajectoryVector;
    WebCore::FloatPoint pendingTrajectoryVector;
    WebCore::IntRect visibleRect;
    WebCore::IntRect coverRect;
    WebCore::IntRect keepRect;
    WebCore::IntRect rect;
    WebCore::IntRect previousRect;
    float contentsScale;
    bool pendingTileCreation;
};

static_assert(sizeof(TiledBackingStore) == 136);
static_assert(offsetof(TiledBackingStore, tiles) == 8);
static_assert(offsetof(TiledBackingStore, tileSize) == 16);
static_assert(offsetof(TiledBackingStore, coverAreaMultiplier) == 24);
static_assert(offsetof(TiledBackingStore, trajectoryVector) == 28);
static_assert(offsetof(TiledBackingStore, pendingTrajectoryVector) == 36);
static_assert(offsetof(TiledBackingStore, visibleRect) == 44);
static_assert(offsetof(TiledBackingStore, coverRect) == 60);
static_assert(offsetof(TiledBackingStore, keepRect) == 76);
static_assert(offsetof(TiledBackingStore, rect) == 92);
static_assert(offsetof(TiledBackingStore, previousRect) == 108);
static_assert(offsetof(TiledBackingStore, contentsScale) == 124);
static_assert(offsetof(TiledBackingStore, pendingTileCreation) == 128);

}

extern "C" {

WebCore::IntRect APS5_VABI _ZNK7WebCore17TiledBackingStore13mapToContentsERKNS_7IntRectE(const NKWebKit::TiledBackingStoreAbi::TiledBackingStore* self, const WebCore::IntRect* rect);

}
