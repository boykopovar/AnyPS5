#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ScrollingConstraintsExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::FixedPositionViewportConstraints;
using NKWebKit::WebCoreAbi::StickyPositionViewportConstraints;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore viewport constraints check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

namespace ViewportDriver {

struct Hash {
    std::uint64_t value = 1469598103934665603ull;
    void Mix(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            value ^= (v >> (8 * i)) & 0xff;
            value *= 1099511628211ull;
        }
    }
    void MixFloat(float f) {
        std::uint32_t bits;
        std::memcpy(&bits, &f, sizeof(bits));
        Mix(bits);
    }
};

struct Rect {
    float x, y, width, height;
};

struct Point {
    float x, y;
};

struct Fixed {
    float alignmentWidth, alignmentHeight;
    unsigned anchorEdges;
    Rect viewportRectAtLastLayout;
    Point layerPositionAtLastLayout;
};

struct Sticky {
    float alignmentWidth, alignmentHeight;
    unsigned anchorEdges;
    float leftOffset, rightOffset, topOffset, bottomOffset;
    Rect constrainingRectAtLastLayout;
    Rect containingBlockRect;
    Rect stickyBoxRect;
    float stickyOffsetWidth, stickyOffsetHeight;
    Point layerPositionAtLastLayout;
};

template<typename Random> float Value(Random& random) {
    switch (random(6)) {
    case 0: return 0.0f;
    case 1: return static_cast<float>(random(2001)) - 1000.0f;
    case 2: return (static_cast<float>(random(20001)) - 10000.0f) / 8.0f;
    case 3: return (static_cast<float>(random(1000001)) - 500000.0f) / 997.0f;
    case 4: return static_cast<float>(random(100));
    default: return -static_cast<float>(random(100)) / 3.0f;
    }
}

template<typename Random> Rect MakeRect(Random& random) {
    Rect rect{Value(random), Value(random), Value(random), Value(random)};
    if (random(4)) {
        rect.width = rect.width < 0 ? -rect.width : rect.width;
        rect.height = rect.height < 0 ? -rect.height : rect.height;
    }
    return rect;
}

template<typename Adapter> std::uint64_t Run(Adapter& adapter, std::uint32_t seed, int count) {
    Hash hash;
    std::uint32_t state = seed ? seed : 1;
    auto random = [&](std::uint32_t bound) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<int>(state % bound);
    };
    for (int i = 0; i < count; ++i) {
        if (random(2)) {
            Fixed f{Value(random), Value(random), static_cast<unsigned>(random(16)), MakeRect(random), {Value(random), Value(random)}};
            Rect viewport = MakeRect(random);
            Point p = adapter.FixedPosition(f, viewport);
            hash.Mix(1);
            hash.MixFloat(p.x);
            hash.MixFloat(p.y);
        } else {
            Sticky s{Value(random), Value(random), static_cast<unsigned>(random(16)), Value(random), Value(random), Value(random), Value(random),
                MakeRect(random), MakeRect(random), MakeRect(random), Value(random), Value(random), {Value(random), Value(random)}};
            if (random(3)) {
                s.stickyBoxRect.x = s.containingBlockRect.x + static_cast<float>(random(50));
                s.stickyBoxRect.y = s.containingBlockRect.y + static_cast<float>(random(50));
            }
            Rect constraining = MakeRect(random);
            Point p = adapter.StickyPosition(s, constraining);
            hash.Mix(2);
            hash.MixFloat(p.x);
            hash.MixFloat(p.y);
        }
    }
    return hash.value;
}

} // namespace ViewportDriver

WebCore::FloatRect Convert(const ViewportDriver::Rect& rect) {
    return {{rect.x, rect.y}, {rect.width, rect.height}};
}

struct Guest {
    ViewportDriver::Point FixedPosition(const ViewportDriver::Fixed& f, const ViewportDriver::Rect& viewport) {
        FixedPositionViewportConstraints constraints;
        std::memset(&constraints, 0xa5, sizeof(constraints));
        constraints.alignmentOffset = {f.alignmentWidth, f.alignmentHeight};
        constraints.anchorEdges = f.anchorEdges;
        constraints.viewportRectAtLastLayout = Convert(f.viewportRectAtLastLayout);
        constraints.layerPositionAtLastLayout = {f.layerPositionAtLastLayout.x, f.layerPositionAtLastLayout.y};
        const WebCore::FloatRect rect = Convert(viewport);
        WebCore::FloatPoint point = _ZNK7WebCore32FixedPositionViewportConstraints28layerPositionForViewportRectERKNS_9FloatRectE(&constraints, &rect);
        return {point.m_x, point.m_y};
    }
    ViewportDriver::Point StickyPosition(const ViewportDriver::Sticky& s, const ViewportDriver::Rect& constraining) {
        StickyPositionViewportConstraints constraints;
        std::memset(&constraints, 0xa5, sizeof(constraints));
        constraints.alignmentOffset = {s.alignmentWidth, s.alignmentHeight};
        constraints.anchorEdges = s.anchorEdges;
        constraints.leftOffset = s.leftOffset;
        constraints.rightOffset = s.rightOffset;
        constraints.topOffset = s.topOffset;
        constraints.bottomOffset = s.bottomOffset;
        constraints.constrainingRectAtLastLayout = Convert(s.constrainingRectAtLastLayout);
        constraints.containingBlockRect = Convert(s.containingBlockRect);
        constraints.stickyBoxRect = Convert(s.stickyBoxRect);
        constraints.stickyOffsetAtLastLayout = {s.stickyOffsetWidth, s.stickyOffsetHeight};
        constraints.layerPositionAtLastLayout = {s.layerPositionAtLastLayout.x, s.layerPositionAtLastLayout.y};
        const WebCore::FloatRect rect = Convert(constraining);
        WebCore::FloatPoint point = _ZNK7WebCore33StickyPositionViewportConstraints32layerPositionForConstrainingRectERKNS_9FloatRectE(&constraints, &rect);
        return {point.m_x, point.m_y};
    }
};

constexpr std::uint64_t ExpectedHashes[] = {
    0x38bceb151053947cull,
    0x45b3bfd7d7c3df81ull,
    0xa4611bbb040c814aull,
    0x4f90700de558271bull,
    0xf1a8cd77df9f90d2ull,
    0xd981af6c1ca0159cull,
    0xb79dc5c9ddfdc288ull,
    0x103b51429e6c26fdull,
    0x2e56084d5e9b88fcull,
    0xce0ac43405890a9full,
    0x6924cffdafcca713ull,
    0x6d8dc2cddb6f2f79ull,
    0x0e4d85c3f83c90bdull,
    0x7ddefb9f38af0af0ull,
    0xd4d6e360475f7784ull,
    0x6d55bd724b3528e4ull,
};

void TestTranscripts() {
    for (std::uint32_t seed = 1; seed <= std::size(ExpectedHashes); ++seed) {
        Guest guest;
        const std::uint64_t hash = ViewportDriver::Run(guest, seed, 1000);
        if (hash != ExpectedHashes[seed - 1]) std::fprintf(stderr, "seed %u: 0x%016llx\n", seed, static_cast<unsigned long long>(hash));
        Require(hash == ExpectedHashes[seed - 1]);
    }
}

void TestKnownPositions() {
    Guest guest;
    ViewportDriver::Fixed fixed{0, 0, 1 | 4, {10, 20, 100, 50}, {5, 6}};
    ViewportDriver::Point p = guest.FixedPosition(fixed, {15, 30, 100, 50});
    Require(p.x == 10 && p.y == 16);
    fixed.anchorEdges = 2 | 8;
    p = guest.FixedPosition(fixed, {10, 20, 120, 40});
    Require(p.x == 25 && p.y == -4);
    fixed.anchorEdges = 0;
    p = guest.FixedPosition(fixed, {99, 99, 1, 1});
    Require(p.x == 5 && p.y == 6);

    ViewportDriver::Sticky sticky{0, 0, 4, 0, 0, 10, 0, {}, {0, 0, 100, 1000}, {0, 50, 100, 20}, 0, 0, {0, 50}};
    p = guest.StickyPosition(sticky, {0, 100, 100, 400});
    Require(p.x == 0 && p.y == 110);
    p = guest.StickyPosition(sticky, {0, 2000, 100, 400});
    Require(p.x == 0 && p.y == 980);
}

template<typename Function> bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestNullArguments() {
    FixedPositionViewportConstraints fixed{};
    StickyPositionViewportConstraints sticky{};
    WebCore::FloatRect rect{};
    Require(Throws([&] { _ZNK7WebCore32FixedPositionViewportConstraints28layerPositionForViewportRectERKNS_9FloatRectE(nullptr, &rect); }));
    Require(Throws([&] { _ZNK7WebCore32FixedPositionViewportConstraints28layerPositionForViewportRectERKNS_9FloatRectE(&fixed, nullptr); }));
    Require(Throws([&] { _ZNK7WebCore33StickyPositionViewportConstraints32layerPositionForConstrainingRectERKNS_9FloatRectE(nullptr, &rect); }));
    Require(Throws([&] { _ZNK7WebCore33StickyPositionViewportConstraints32layerPositionForConstrainingRectERKNS_9FloatRectE(&sticky, nullptr); }));
}

}  // namespace

int main() {
    TestTranscripts();
    TestKnownPositions();
    TestNullArguments();
    return 0;
}
