#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <stdexcept>

using NKWebKit::WebCoreAbi::Cursor;
using NKWebKit::WebCoreAbi::CursorType;

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

int main() {
    TestNamedSingletons();
    TestFromType();
    TestOutOfRange();
    std::puts("WebCore cursor tests passed");
    return 0;
}
