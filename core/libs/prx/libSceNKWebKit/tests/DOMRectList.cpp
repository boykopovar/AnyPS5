#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

using NKWebKit::WebCoreAbi::DOMRect;
using NKWebKit::WebCoreAbi::DOMRectList;
using NKWebKit::WtfAbi::Vector;

using QuadConstructor = void (APS5_VABI*)(DOMRectList*, const Vector<WebCore::FloatQuad>*);
using RectConstructor = void (APS5_VABI*)(DOMRectList*, const Vector<WebCore::FloatRect>*);
using Destructor = void (APS5_VABI*)(DOMRectList*);

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "DOMRectList check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Action>
static bool ThrowsInvalidArgument(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}
#define RequireThrows(action) Require(ThrowsInvalidArgument(action))

static constexpr QuadConstructor kQuadConstructors[] = {
    &_ZN7WebCore11DOMRectListC1ERKN3WTF6VectorINS_9FloatQuadELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE,
    &_ZN7WebCore11DOMRectListC2ERKN3WTF6VectorINS_9FloatQuadELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE,
};

static constexpr RectConstructor kRectConstructors[] = {
    &_ZN7WebCore11DOMRectListC1ERKN3WTF6VectorINS_9FloatRectELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE,
    &_ZN7WebCore11DOMRectListC2ERKN3WTF6VectorINS_9FloatRectELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE,
};

static bool Equal(double a, double b) {
    return std::memcmp(&a, &b, sizeof(double)) == 0;
}

static void ConstructsFromRects() {
    WebCore::FloatRect rects[2] = {
        {{1.5f, -2.25f}, {3.0f, 4.5f}},
        {{0.0f, 0.0f}, {1000.25f, -0.5f}},
    };
    Vector<WebCore::FloatRect> source;
    source.buffer = rects;
    source.capacity = 2;
    source.size = 2;
    for (auto construct : kRectConstructors) {
        alignas(DOMRectList) unsigned char storage[sizeof(DOMRectList)];
        std::memset(storage, 0xAB, sizeof(storage));
        auto* list = reinterpret_cast<DOMRectList*>(storage);
        construct(list, &source);
        Require(list->refCount == 1);
        Require(list->items.size == 2 && list->items.capacity == 2 && list->items.buffer != nullptr);
        const DOMRect* first = list->items.buffer[0];
        const DOMRect* second = list->items.buffer[1];
        Require(first != second);
        Require(first->wrapper == nullptr && second->wrapper == nullptr);
        Require(first->refCount == 1 && second->refCount == 1);
        Require(Equal(first->x, 1.5) && Equal(first->y, -2.25) && Equal(first->width, 3.0) && Equal(first->height, 4.5));
        Require(Equal(second->x, 0.0) && Equal(second->y, 0.0) && Equal(second->width, 1000.25) && Equal(second->height, -0.5));
        _ZN7WebCore11DOMRectListD2Ev(list);
        Require(list->items.buffer == nullptr && list->items.size == 0 && list->items.capacity == 0);
    }
    source.buffer = nullptr;
}

static void ConstructsFromQuadBoundingBox() {
    WebCore::FloatQuad quads[2] = {
        {{3.0f, 1.0f}, {1.0f, 3.0f}, {2.0f, 2.0f}, {2.5f, 2.5f}},
        {{-1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, -1.0f}},
    };
    Vector<WebCore::FloatQuad> source;
    source.buffer = quads;
    source.capacity = 2;
    source.size = 2;
    for (auto construct : kQuadConstructors) {
        alignas(DOMRectList) unsigned char storage[sizeof(DOMRectList)];
        std::memset(storage, 0xAB, sizeof(storage));
        auto* list = reinterpret_cast<DOMRectList*>(storage);
        construct(list, &source);
        Require(list->refCount == 1);
        Require(list->items.size == 2 && list->items.capacity == 2);
        const DOMRect* first = list->items.buffer[0];
        Require(first->wrapper == nullptr && first->refCount == 1);
        Require(Equal(first->x, 1.0) && Equal(first->y, 1.0) && Equal(first->width, 2.0) && Equal(first->height, 2.0));
        const DOMRect* second = list->items.buffer[1];
        Require(Equal(second->x, -1.0) && Equal(second->y, -1.0) && Equal(second->width, 2.0) && Equal(second->height, 2.0));
        _ZN7WebCore11DOMRectListD1Ev(list);
        Require(list->items.buffer == nullptr && list->items.size == 0 && list->items.capacity == 0);
    }
    source.buffer = nullptr;
}

static void HandlesEmptyAndLargeVectors() {
    WebCore::FloatRect rect{{1.0f, 2.0f}, {3.0f, 4.0f}};
    Vector<WebCore::FloatRect> empty;
    empty.buffer = nullptr;
    empty.capacity = 0;
    empty.size = 0;
    alignas(DOMRectList) unsigned char storage[sizeof(DOMRectList)];
    auto* list = reinterpret_cast<DOMRectList*>(storage);
    _ZN7WebCore11DOMRectListC1ERKN3WTF6VectorINS_9FloatRectELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(list, &empty);
    Require(list->refCount == 1 && list->items.buffer == nullptr && list->items.size == 0 && list->items.capacity == 0);
    _ZN7WebCore11DOMRectListD2Ev(list);

    constexpr unsigned count = 64;
    static WebCore::FloatRect many[count];
    for (unsigned index = 0; index < count; ++index) {
        many[index] = {{static_cast<float>(index), 0.0f}, {1.0f, 1.0f}};
    }
    Vector<WebCore::FloatRect> source;
    source.buffer = many;
    source.capacity = count;
    source.size = count;
    _ZN7WebCore11DOMRectListC2ERKN3WTF6VectorINS_9FloatRectELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(list, &source);
    Require(list->items.size == count && list->items.capacity == count);
    for (unsigned index = 0; index < count; ++index) {
        Require(Equal(list->items.buffer[index]->x, static_cast<double>(index)));
    }
    _ZN7WebCore11DOMRectListD1Ev(list);
    Require(list->items.buffer == nullptr && list->items.size == 0);
    source.buffer = nullptr;
}

static void PropagatesQuadNaNsLikeMinMax() {
    WebCore::FloatQuad quads[1] = {
        {{std::numeric_limits<float>::quiet_NaN(), 1.0f}, {1.0f, 3.0f}, {2.0f, 0.0f}, {2.5f, 2.5f}},
    };
    Vector<WebCore::FloatQuad> source;
    source.buffer = quads;
    source.capacity = 1;
    source.size = 1;
    alignas(DOMRectList) unsigned char storage[sizeof(DOMRectList)];
    auto* list = reinterpret_cast<DOMRectList*>(storage);
    _ZN7WebCore11DOMRectListC1ERKN3WTF6VectorINS_9FloatQuadELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(list, &source);
    const DOMRect* item = list->items.buffer[0];
    Require(std::isnan(item->x) && std::isnan(item->width));
    Require(Equal(item->y, 0.0) && Equal(item->height, 3.0));
    _ZN7WebCore11DOMRectListD2Ev(list);
    source.buffer = nullptr;
}

static void ThrowsOnInvalidArguments() {
    alignas(DOMRectList) unsigned char storage[sizeof(DOMRectList)];
    auto* list = reinterpret_cast<DOMRectList*>(storage);
    WebCore::FloatRect rect{{1.0f, 2.0f}, {3.0f, 4.0f}};
    Vector<WebCore::FloatRect> source;
    source.buffer = &rect;
    source.capacity = 1;
    source.size = 1;
    Vector<WebCore::FloatRect> broken;
    broken.buffer = nullptr;
    broken.capacity = 1;
    broken.size = 1;
    for (auto construct : kRectConstructors) {
        RequireThrows([&] { construct(nullptr, &source); });
        RequireThrows([&] { construct(list, nullptr); });
        RequireThrows([&] { construct(list, &broken); });
    }
    for (auto destruct : {&_ZN7WebCore11DOMRectListD1Ev, &_ZN7WebCore11DOMRectListD2Ev}) {
        RequireThrows([&] { destruct(nullptr); });
    }
    source.buffer = nullptr;
}

int main() {
    ConstructsFromRects();
    ConstructsFromQuadBoundingBox();
    HandlesEmptyAndLargeVectors();
    PropagatesQuadNaNsLikeMinMax();
    ThrowsOnInvalidArguments();
    std::puts("DOMRectList passed");
    return 0;
}
