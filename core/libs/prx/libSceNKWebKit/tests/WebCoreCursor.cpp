#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <stdexcept>

using NKWebKit::WebCoreAbi::Cursor;
using NKWebKit::WebCoreAbi::CursorType;
using NKWebKit::WebCoreAbi::ImageOrientation;
using NKWebKit::WebCoreAbi::OptionalIntPoint;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore cursor check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static bool IsBlank(const Cursor& cursor) {
    return cursor.image == nullptr && cursor.hotSpot.m_x == 0 && cursor.hotSpot.m_y == 0 && cursor.platformCursor == nullptr;
}

static const Cursor& FromType(std::uint32_t value) {
    return _ZN7WebCore6Cursor8fromTypeENS0_4TypeE(static_cast<CursorType>(value));
}

static void TestNamedSingletons() {
    const Cursor& hand = _ZN7WebCore10handCursorEv();
    const Cursor& iBeam = _ZN7WebCore11iBeamCursorEv();
    Require(&hand == &_ZN7WebCore10handCursorEv());
    Require(&iBeam == &_ZN7WebCore11iBeamCursorEv());
    Require(&hand != &iBeam);
    Require(static_cast<std::uint32_t>(hand.type) == 2);
    Require(static_cast<std::uint32_t>(iBeam.type) == 3);
    Require(IsBlank(hand));
    Require(IsBlank(iBeam));
    Require(&FromType(2) == &hand);
    Require(&FromType(3) == &iBeam);
}

static void TestFromType() {
    std::set<const Cursor*> seen;
    for (std::uint32_t value = 0; value < 43; ++value) {
        const Cursor& cursor = FromType(value);
        Require(static_cast<std::uint32_t>(cursor.type) == value);
        Require(IsBlank(cursor));
        Require(&FromType(value) == &cursor);
        Require(seen.insert(&cursor).second);
    }
    const Cursor& pointer = FromType(0);
    Require(&FromType(43) == &pointer);
    Require(&FromType(44) == &pointer);
    Require(&FromType(63) == &pointer);
}

static void TestOutOfRange() {
    for (std::uint32_t value : {64u, 1000u, 0xFFFFFFFFu}) {
        bool threw = false;
        try {
            FromType(value);
        } catch (const std::out_of_range&) {
            threw = true;
        }
        Require(threw);
    }
}

struct FakeImage {
    const void* vtable;
    std::atomic<std::uint32_t> refCount;
};

struct FakeImageState {
    WebCore::FloatSize size{10.0f, 20.0f};
    bool hotSpotHasValue{false};
    WebCore::IntPoint hotSpotValue{2, 3};
    unsigned sizeCalls{0};
    unsigned hotSpotCalls{0};
    std::int32_t lastOrientation{-1};
};

static FakeImageState* CurrentImageState = nullptr;

static WebCore::FloatSize APS5_VABI FakeSize(const void*, ImageOrientation orientation) {
    CurrentImageState->sizeCalls += 1;
    CurrentImageState->lastOrientation = orientation.orientation;
    return CurrentImageState->size;
}

static OptionalIntPoint APS5_VABI FakeHotSpot(const void*) {
    CurrentImageState->hotSpotCalls += 1;
    OptionalIntPoint result;
    result.hasValue = CurrentImageState->hotSpotHasValue;
    result.value = CurrentImageState->hotSpotValue;
    return result;
}

static const void* const* BuildVtable(const void* sizeEntry, const void* hotSpotEntry) {
    static const void* slots[23] = {};
    slots[19] = sizeEntry;
    slots[22] = hotSpotEntry;
    return slots;
}

static void MakeImage(FakeImage& image, const void* sizeEntry, const void* hotSpotEntry) {
    image.vtable = BuildVtable(sizeEntry, hotSpotEntry);
    image.refCount = 1;
}

static void MakeImageWithoutVtable(FakeImage& image) {
    image.vtable = nullptr;
    image.refCount = 1;
}

template<typename Function>
static bool Throws(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    } catch (...) {
        return true;
    }
    return false;
}

static void TestConstructorWithInsideHotSpot() {
    FakeImageState state;
    CurrentImageState = &state;
    FakeImage image;
    MakeImage(image, reinterpret_cast<const void*>(&FakeSize), reinterpret_cast<const void*>(&FakeHotSpot));
    Cursor cursor{};
    WebCore::IntPoint specified{3, 4};
    _ZN7WebCore6CursorC1EPNS_5ImageERKNS_8IntPointE(&cursor, &image, &specified);
    Require(cursor.type == CursorType::Custom);
    Require(cursor.image == &image);
    Require(cursor.hotSpot.m_x == 3 && cursor.hotSpot.m_y == 4);
    Require(cursor.platformCursor == nullptr);
    Require(image.refCount == 2);
    Require(state.sizeCalls == 2);
    Require(state.hotSpotCalls == 0);
    Require(state.lastOrientation == 0);
}

static void TestSecondConstructorSameBehaviour() {
    FakeImageState state;
    CurrentImageState = &state;
    FakeImage image;
    MakeImage(image, reinterpret_cast<const void*>(&FakeSize), reinterpret_cast<const void*>(&FakeHotSpot));
    Cursor cursor{};
    WebCore::IntPoint specified{9, 19};
    _ZN7WebCore6CursorC2EPNS_5ImageERKNS_8IntPointE(&cursor, &image, &specified);
    Require(cursor.type == CursorType::Custom);
    Require(cursor.image == &image);
    Require(cursor.hotSpot.m_x == 9 && cursor.hotSpot.m_y == 19);
    Require(cursor.platformCursor == nullptr);
    Require(image.refCount == 2);
}

static void TestIntrinsicHotSpotUsedWhenSpecifiedOutside() {
    FakeImageState state;
    state.hotSpotHasValue = true;
    state.hotSpotValue = {2, 3};
    CurrentImageState = &state;
    FakeImage image;
    MakeImage(image, reinterpret_cast<const void*>(&FakeSize), reinterpret_cast<const void*>(&FakeHotSpot));
    Cursor cursor{};
    WebCore::IntPoint specified{50, 50};
    _ZN7WebCore6CursorC1EPNS_5ImageERKNS_8IntPointE(&cursor, &image, &specified);
    Require(cursor.hotSpot.m_x == 2 && cursor.hotSpot.m_y == 3);
    Require(state.hotSpotCalls == 1);
}

static void TestDefaultHotSpotWhenNothingContained() {
    FakeImageState state;
    state.hotSpotHasValue = true;
    state.hotSpotValue = {20, 20};
    CurrentImageState = &state;
    FakeImage image;
    MakeImage(image, reinterpret_cast<const void*>(&FakeSize), reinterpret_cast<const void*>(&FakeHotSpot));
    Cursor cursor{};
    WebCore::IntPoint specified{50, 50};
    _ZN7WebCore6CursorC1EPNS_5ImageERKNS_8IntPointE(&cursor, &image, &specified);
    Require(cursor.hotSpot.m_x == 0 && cursor.hotSpot.m_y == 0);
    state.hotSpotHasValue = false;
    Cursor second{};
    _ZN7WebCore6CursorC2EPNS_5ImageERKNS_8IntPointE(&second, &image, &specified);
    Require(second.hotSpot.m_x == 0 && second.hotSpot.m_y == 0);
}

static void TestZeroSizeHasDefaultHotSpotAndSkipsIntrinsic() {
    FakeImageState state;
    state.size = {0.0f, 20.0f};
    state.hotSpotHasValue = true;
    CurrentImageState = &state;
    FakeImage image;
    MakeImage(image, reinterpret_cast<const void*>(&FakeSize), reinterpret_cast<const void*>(&FakeHotSpot));
    Cursor cursor{};
    WebCore::IntPoint specified{3, 4};
    _ZN7WebCore6CursorC1EPNS_5ImageERKNS_8IntPointE(&cursor, &image, &specified);
    Require(cursor.hotSpot.m_x == 0 && cursor.hotSpot.m_y == 0);
    Require(state.sizeCalls == 1);
    Require(state.hotSpotCalls == 0);
    Require(cursor.type == CursorType::Custom);
    Require(image.refCount == 2);
    state.size = {10.0f, -1.0f};
    Cursor second{};
    _ZN7WebCore6CursorC2EPNS_5ImageERKNS_8IntPointE(&second, &image, &specified);
    Require(second.hotSpot.m_x == 0 && second.hotSpot.m_y == 0);
    Require(state.hotSpotCalls == 0);
}

static void TestHotSpotRectangleBoundary() {
    FakeImageState state;
    state.hotSpotHasValue = true;
    state.hotSpotValue = {10, 5};
    CurrentImageState = &state;
    FakeImage image;
    MakeImage(image, reinterpret_cast<const void*>(&FakeSize), reinterpret_cast<const void*>(&FakeHotSpot));
    Cursor cursor{};
    WebCore::IntPoint onEdge{10, 5};
    _ZN7WebCore6CursorC1EPNS_5ImageERKNS_8IntPointE(&cursor, &image, &onEdge);
    Require(cursor.hotSpot.m_x == 0 && cursor.hotSpot.m_y == 0);
    WebCore::IntPoint inside{9, 5};
    _ZN7WebCore6CursorC2EPNS_5ImageERKNS_8IntPointE(&cursor, &image, &inside);
    Require(cursor.hotSpot.m_x == 9 && cursor.hotSpot.m_y == 5);
}

static void TestLargeSizeClampedToIntMax() {
    FakeImageState state;
    state.size = {5.0e9f, 1.0f};
    CurrentImageState = &state;
    FakeImage image;
    MakeImage(image, reinterpret_cast<const void*>(&FakeSize), reinterpret_cast<const void*>(&FakeHotSpot));
    Cursor cursor{};
    WebCore::IntPoint specified{2000000000, 0};
    _ZN7WebCore6CursorC1EPNS_5ImageERKNS_8IntPointE(&cursor, &image, &specified);
    Require(cursor.hotSpot.m_x == 2000000000 && cursor.hotSpot.m_y == 0);
}

static void TestNullSizeSlotThrows() {
    FakeImageState state;
    CurrentImageState = &state;
    FakeImage image;
    MakeImage(image, nullptr, reinterpret_cast<const void*>(&FakeHotSpot));
    Cursor cursor{};
    WebCore::IntPoint specified{3, 4};
    Require(Throws([&] { _ZN7WebCore6CursorC1EPNS_5ImageERKNS_8IntPointE(&cursor, &image, &specified); }));
    Require(image.refCount == 1);
}

static void TestNullHotSpotSlotThrows() {
    FakeImageState state;
    state.hotSpotHasValue = true;
    CurrentImageState = &state;
    FakeImage image;
    MakeImage(image, reinterpret_cast<const void*>(&FakeSize), nullptr);
    Cursor cursor{};
    WebCore::IntPoint specified{50, 50};
    Require(Throws([&] { _ZN7WebCore6CursorC1EPNS_5ImageERKNS_8IntPointE(&cursor, &image, &specified); }));
    Require(image.refCount == 1);
}

static void TestInvalidArguments() {
    FakeImageState state;
    CurrentImageState = &state;
    FakeImage image;
    MakeImage(image, reinterpret_cast<const void*>(&FakeSize), reinterpret_cast<const void*>(&FakeHotSpot));
    FakeImage badVtable;
    MakeImageWithoutVtable(badVtable);
    Cursor cursor{};
    WebCore::IntPoint specified{3, 4};
    Require(Throws([&] { _ZN7WebCore6CursorC1EPNS_5ImageERKNS_8IntPointE(nullptr, &image, &specified); }));
    Require(Throws([&] { _ZN7WebCore6CursorC1EPNS_5ImageERKNS_8IntPointE(&cursor, nullptr, &specified); }));
    Require(Throws([&] { _ZN7WebCore6CursorC1EPNS_5ImageERKNS_8IntPointE(&cursor, &image, nullptr); }));
    Require(Throws([&] { _ZN7WebCore6CursorC2EPNS_5ImageERKNS_8IntPointE(&cursor, &badVtable, &specified); }));
    Require(image.refCount == 1);
    Require(badVtable.refCount == 1);
    Require(cursor.image == nullptr);
    Require(static_cast<std::uint32_t>(cursor.type) == 0xFFFFFFFFu);
}

static void TestSetAsPlatformCursorDoesNothing() {
    FakeImageState state;
    CurrentImageState = &state;
    FakeImage image;
    MakeImage(image, reinterpret_cast<const void*>(&FakeSize), reinterpret_cast<const void*>(&FakeHotSpot));
    Cursor cursor{};
    WebCore::IntPoint specified{3, 4};
    _ZN7WebCore6CursorC1EPNS_5ImageERKNS_8IntPointE(&cursor, &image, &specified);
    _ZNK7WebCore6Cursor19setAsPlatformCursorEv(&cursor);
    Require(cursor.type == CursorType::Custom);
    Require(cursor.image == &image);
    Require(cursor.hotSpot.m_x == 3 && cursor.hotSpot.m_y == 4);
    Require(cursor.platformCursor == nullptr);
    Require(image.refCount == 2);
    _ZNK7WebCore6Cursor19setAsPlatformCursorEv(nullptr);
}

int main() {
    TestNamedSingletons();
    TestFromType();
    TestOutOfRange();
    TestConstructorWithInsideHotSpot();
    TestSecondConstructorSameBehaviour();
    TestIntrinsicHotSpotUsedWhenSpecifiedOutside();
    TestDefaultHotSpotWhenNothingContained();
    TestZeroSizeHasDefaultHotSpotAndSkipsIntrinsic();
    TestHotSpotRectangleBoundary();
    TestLargeSizeClampedToIntMax();
    TestNullSizeSlotThrows();
    TestNullHotSpotSlotThrows();
    TestInvalidArguments();
    TestSetAsPlatformCursorDoesNothing();
    std::puts("WebCore cursor tests passed");
    return 0;
}
