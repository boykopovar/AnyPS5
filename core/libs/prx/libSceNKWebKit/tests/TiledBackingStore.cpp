#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

using NKWebKit::TiledBackingStoreAbi::TiledBackingStore;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "TiledBackingStore check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template<typename TFunction>
bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

TiledBackingStore MakeStore(float contentsScale) {
    TiledBackingStore store{};
    store.tileSize = WebCore::IntSize{512, 512};
    store.coverAreaMultiplier = 4.0f;
    store.trajectoryVector = WebCore::FloatPoint{3.0f, 3.0f};
    store.pendingTrajectoryVector = WebCore::FloatPoint{5.0f, 5.0f};
    store.visibleRect = WebCore::IntRect{{1, 2}, {3, 4}};
    store.coverRect = WebCore::IntRect{{5, 6}, {7, 8}};
    store.keepRect = WebCore::IntRect{{9, 10}, {11, 12}};
    store.rect = WebCore::IntRect{{13, 14}, {15, 16}};
    store.previousRect = WebCore::IntRect{{17, 18}, {19, 20}};
    store.contentsScale = contentsScale;
    store.pendingTileCreation = true;
    return store;
}

bool MapsTo(float contentsScale, int x, int y, int width, int height, int expectedX, int expectedY, int expectedWidth, int expectedHeight) {
    const TiledBackingStore store = MakeStore(contentsScale);
    const WebCore::IntRect input{{x, y}, {width, height}};
    const WebCore::IntRect mapped = _ZNK7WebCore17TiledBackingStore13mapToContentsERKNS_7IntRectE(&store, &input);
    return mapped.m_location.m_x == expectedX && mapped.m_location.m_y == expectedY
        && mapped.m_size.m_width == expectedWidth && mapped.m_size.m_height == expectedHeight;
}

void TestScaleDown() {
    Require(MapsTo(2.0f, 10, 20, 30, 40, 5, 10, 15, 20));
    Require(MapsTo(2.0f, 3, 5, 7, 9, 1, 2, 4, 5));
    Require(MapsTo(2.0f, -3, -1, 1, 1, -2, -1, 1, 1));
    Require(MapsTo(2.0f, 0, 0, 0, 0, 0, 0, 0, 0));
}

void TestScaleUp() {
    Require(MapsTo(0.5f, 1, 2, 3, 4, 2, 4, 6, 8));
    Require(MapsTo(0.25f, -1, 0, 1, 2, -4, 0, 4, 8));
}

void TestFractionalScale() {
    Require(MapsTo(1.5f, 0, 0, 10, 10, 0, 0, 7, 7));
    Require(MapsTo(1.5f, 1, 1, 1, 1, 0, 0, 2, 2));
    Require(MapsTo(1.0f, 7, -7, 8, 9, 7, -7, 8, 9));
}

void TestSinglePrecision() {
    Require(MapsTo(1.0f, 16777217, 0, 1, 1, 16777216, 0, 0, 1));
}

void TestNullArguments() {
    const TiledBackingStore store = MakeStore(2.0f);
    const WebCore::IntRect input{{1, 2}, {3, 4}};
    Require(Throws([&] { _ZNK7WebCore17TiledBackingStore13mapToContentsERKNS_7IntRectE(nullptr, &input); }));
    Require(Throws([&] { _ZNK7WebCore17TiledBackingStore13mapToContentsERKNS_7IntRectE(&store, nullptr); }));
}

}

int main() {
    TestScaleDown();
    TestScaleUp();
    TestFractionalScale();
    TestSinglePrecision();
    TestNullArguments();
    return 0;
}
