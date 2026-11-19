#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/HitTestLocationExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    TestNull();
    std::puts("nkwebkit hit test location tests passed");
    return 0;
}
