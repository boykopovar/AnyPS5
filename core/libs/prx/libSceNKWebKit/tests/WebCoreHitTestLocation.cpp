#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/HitTestLocationExports.hpp"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "HitTestLocation check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(cond) Check((cond), __LINE__)

template <typename Call>
void ExpectStub(Call call, int line) {
    const auto before = NotImplementedCalls_nid_no_patch();
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Check(threw && NotImplementedCalls_nid_no_patch() == before + 1, line);
}
#define RequireStub(call) ExpectStub([&] { call; }, __LINE__)

bool IsZeroed(const unsigned char* bytes, std::size_t begin, std::size_t end) {
    for (std::size_t i = begin; i < end; ++i) {
        if (bytes[i] != 0) {
            return false;
        }
    }
    return true;
}

using DefaultCtor = void(APS5_VABI*)(WebCore::HitTestLocation*);
using CopyCtor = void(APS5_VABI*)(WebCore::HitTestLocation*, const WebCore::HitTestLocation*);

void TestDefault(DefaultCtor ctor) {
    alignas(WebCore::HitTestLocation) unsigned char storage[sizeof(WebCore::HitTestLocation) + 4];
    std::memset(storage, 0xA5, sizeof(storage));
    ctor(reinterpret_cast<WebCore::HitTestLocation*>(storage));
    Require(IsZeroed(storage, 0, 65));
    Require(storage[65] == 1);
    Require(storage[66] == 0xA5 || storage[66] == 0);
    Require(storage[68] == 0xA5);
    Require(storage[71] == 0xA5);
}

WebCore::HitTestLocation Sample() {
    WebCore::HitTestLocation location;
    location.m_point = {{-65}, {128}};
    location.m_boundingBox = {{-2, 2}, {7, 5}};
    location.m_transformedPoint = {-1.015625f, 2.0f};
    location.m_transformedRect = {{-2.0f, 2.0f}, {5.0f, 2.0f}, {5.0f, 7.0f}, {-2.0f, 7.0f}};
    location.m_isRectBased = true;
    location.m_isRectilinear = false;
    return location;
}

void TestCopy(CopyCtor ctor) {
    const WebCore::HitTestLocation source = Sample();
    alignas(WebCore::HitTestLocation) unsigned char storage[sizeof(WebCore::HitTestLocation) + 4];
    std::memset(storage, 0xA5, sizeof(storage));
    auto* target = reinterpret_cast<WebCore::HitTestLocation*>(storage);
    ctor(target, &source);
    Require(target->m_point.m_x.m_value == -65);
    Require(target->m_point.m_y.m_value == 128);
    Require(target->m_boundingBox.m_location.m_x == -2);
    Require(target->m_boundingBox.m_location.m_y == 2);
    Require(target->m_boundingBox.m_size.m_width == 7);
    Require(target->m_boundingBox.m_size.m_height == 5);
    Require(target->m_transformedPoint.m_x == -1.015625f);
    Require(target->m_transformedPoint.m_y == 2.0f);
    Require(target->m_transformedRect.m_p1.m_x == -2.0f && target->m_transformedRect.m_p1.m_y == 2.0f);
    Require(target->m_transformedRect.m_p2.m_x == 5.0f && target->m_transformedRect.m_p2.m_y == 2.0f);
    Require(target->m_transformedRect.m_p3.m_x == 5.0f && target->m_transformedRect.m_p3.m_y == 7.0f);
    Require(target->m_transformedRect.m_p4.m_x == -2.0f && target->m_transformedRect.m_p4.m_y == 7.0f);
    Require(target->m_isRectBased);
    Require(!target->m_isRectilinear);
    Require(storage[68] == 0xA5);

    WebCore::HitTestLocation fresh;
    std::memset(&fresh, 0x5A, sizeof(fresh));
    _ZN7WebCore15HitTestLocationC1Ev(&fresh);
    WebCore::HitTestLocation copy;
    std::memset(&copy, 0x5A, sizeof(copy));
    ctor(&copy, &fresh);
    Require(copy.m_point.m_x.m_value == 0 && copy.m_point.m_y.m_value == 0);
    Require(copy.m_boundingBox.m_size.m_width == 0);
    Require(copy.m_transformedRect.m_p3.m_y == 0.0f);
    Require(!copy.m_isRectBased);
    Require(copy.m_isRectilinear);
}

using FloatPointCtor = void(APS5_VABI*)(WebCore::HitTestLocation*, const WebCore::FloatPoint*);

void TestFloatPoint(FloatPointCtor ctor) {
    alignas(WebCore::HitTestLocation) unsigned char storage[sizeof(WebCore::HitTestLocation) + 4];
    std::memset(storage, 0xA5, sizeof(storage));
    auto* target = reinterpret_cast<WebCore::HitTestLocation*>(storage);
    const WebCore::FloatPoint point{-1.01f, 2.999f};
    ctor(target, &point);
    Require(target->m_point.m_x.m_value == -65);
    Require(target->m_point.m_y.m_value == 191);
    Require(target->m_boundingBox.m_location.m_x == -2);
    Require(target->m_boundingBox.m_location.m_y == 2);
    Require(target->m_boundingBox.m_size.m_width == 1);
    Require(target->m_boundingBox.m_size.m_height == 1);
    Require(target->m_transformedPoint.m_x == -1.015625f);
    Require(target->m_transformedPoint.m_y == 2.984375f);
    Require(target->m_transformedRect.m_p1.m_x == -2.0f && target->m_transformedRect.m_p1.m_y == 2.0f);
    Require(target->m_transformedRect.m_p2.m_x == -1.0f && target->m_transformedRect.m_p2.m_y == 2.0f);
    Require(target->m_transformedRect.m_p3.m_x == -1.0f && target->m_transformedRect.m_p3.m_y == 3.0f);
    Require(target->m_transformedRect.m_p4.m_x == -2.0f && target->m_transformedRect.m_p4.m_y == 3.0f);
    Require(!target->m_isRectBased);
    Require(target->m_isRectilinear);
    Require(storage[68] == 0xA5);
    Require(storage[71] == 0xA5);

    const WebCore::FloatPoint exact{3.0f, -0.5f};
    ctor(target, &exact);
    Require(target->m_point.m_x.m_value == 192);
    Require(target->m_point.m_y.m_value == -32);
    Require(target->m_boundingBox.m_location.m_x == 3);
    Require(target->m_boundingBox.m_location.m_y == -1);
    Require(target->m_transformedPoint.m_x == 3.0f);
    Require(target->m_transformedPoint.m_y == -0.5f);
    Require(target->m_transformedRect.m_p3.m_x == 4.0f && target->m_transformedRect.m_p3.m_y == 0.0f);

    const WebCore::FloatPoint huge{1.0e10f, -1.0e10f};
    ctor(target, &huge);
    Require(target->m_point.m_x.m_value == INT_MAX);
    Require(target->m_point.m_y.m_value == INT_MIN);
    Require(target->m_boundingBox.m_location.m_x == INT_MAX / 64);
    Require(target->m_boundingBox.m_location.m_y == INT_MIN / 64);
    Require(target->m_transformedPoint.m_x == 33554432.0f);
    Require(target->m_transformedPoint.m_y == -33554432.0f);

    const float infinity = std::numeric_limits<float>::infinity();
    const WebCore::FloatPoint infinite{-infinity, infinity};
    ctor(target, &infinite);
    Require(target->m_point.m_x.m_value == INT_MIN);
    Require(target->m_point.m_y.m_value == INT_MAX);

    const WebCore::FloatPoint edge{16777215.0f / 64.0f, -33554432.0f};
    ctor(target, &edge);
    Require(target->m_point.m_x.m_value == 16777215);
    Require(target->m_point.m_y.m_value == INT_MIN);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const WebCore::FloatPoint nanX{nan, 0.0f};
    const WebCore::FloatPoint nanY{0.0f, nan};
    RequireStub(ctor(target, &nanX));
    RequireStub(ctor(target, &nanY));
    RequireStub(ctor(nullptr, &point));
    RequireStub(ctor(target, nullptr));
}

void TestNull() {
    WebCore::HitTestLocation location;
    RequireStub(_ZN7WebCore15HitTestLocationC1Ev(nullptr));
    RequireStub(_ZN7WebCore15HitTestLocationC2Ev(nullptr));
    RequireStub(_ZN7WebCore15HitTestLocationC1ERKS0_(nullptr, &location));
    RequireStub(_ZN7WebCore15HitTestLocationC2ERKS0_(nullptr, &location));
    RequireStub(_ZN7WebCore15HitTestLocationC1ERKS0_(&location, nullptr));
    RequireStub(_ZN7WebCore15HitTestLocationC2ERKS0_(&location, nullptr));
}

}  // namespace

int main() {
    TestDefault(&_ZN7WebCore15HitTestLocationC1Ev);
    TestDefault(&_ZN7WebCore15HitTestLocationC2Ev);
    TestCopy(&_ZN7WebCore15HitTestLocationC1ERKS0_);
    TestCopy(&_ZN7WebCore15HitTestLocationC2ERKS0_);
    TestFloatPoint(&_ZN7WebCore15HitTestLocationC1ERKNS_10FloatPointE);
    TestFloatPoint(&_ZN7WebCore15HitTestLocationC2ERKNS_10FloatPointE);
    TestNull();
    std::puts("nkwebkit hit test location tests passed");
    return 0;
}
