#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/PathExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <stdexcept>

namespace {

using NKWebKit::PathAbi::ClipOutToPath;
using NKWebKit::PathAbi::ClipPath;
using NKWebKit::PathAbi::DrawFocusRingPath;
using NKWebKit::PathAbi::DrawPath;
using NKWebKit::PathAbi::FillPath;
using NKWebKit::PathAbi::Path;
using NKWebKit::PathAbi::StrokePath;

void Require(const bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "DisplayList path item check failed at line %u\n", location.line());
        std::abort();
    }
}

Path EmptyPath() {
    Path path{};
    path.m_path = nullptr;
    path.m_elements = {nullptr, 0, 0};
    path.m_elementsEngaged = true;
    return path;
}

Path TwoPointPath() {
    Path path = EmptyPath();
    const WebCore::FloatPoint move{1.0f, 2.0f};
    const WebCore::FloatPoint line{3.0f, 4.0f};
    _ZN7WebCore4Path6moveToERKNS_10FloatPointE(&path, &move);
    _ZN7WebCore4Path9addLineToERKNS_10FloatPointE(&path, &line);
    return path;
}

void TestFillPath() {
    Path source = TwoPointPath();
    alignas(8) unsigned char storage[sizeof(FillPath)] = {};
    auto& item = *reinterpret_cast<FillPath*>(storage);
    _ZN7WebCore11DisplayList8FillPathC1ERKNS_4PathE(&item, &source);
    Require(item.drawingItem.item.type == static_cast<std::uint8_t>(NKWebKit::DisplayListAbi::ItemType::FillPath));
    Require(item.drawingItem.item.refCount == 1);
    Require(item.drawingItem.extent.engaged == false);
    Require(_ZNK7WebCore4Path12elementCountEv(&item.path) == 2);
    const auto* table = reinterpret_cast<const NKWebKit::DisplayListAbi::DrawingItemVtable*>(item.drawingItem.item.vptr);
    const auto bounds = table->localBounds(&item, nullptr);
    Require(bounds.engaged);
    Require(bounds.value.width() == 2.0f && bounds.value.height() == 2.0f);
    const WebCore::FloatPoint extra{5.0f, 6.0f};
    _ZN7WebCore4Path9addLineToERKNS_10FloatPointE(&item.path, &extra);
    Require(_ZNK7WebCore4Path12elementCountEv(&item.path) == 3);
    Require(_ZNK7WebCore4Path12elementCountEv(&source) == 2);
    _ZN7WebCore11DisplayList8FillPathD1Ev(&item);
    _ZN7WebCore4PathD1Ev(&source);
}

void TestStrokePath() {
    Path source = TwoPointPath();
    alignas(8) unsigned char storage[sizeof(StrokePath)] = {};
    auto& item = *reinterpret_cast<StrokePath*>(storage);
    _ZN7WebCore11DisplayList10StrokePathC1ERKNS_4PathE(&item, &source);
    Require(item.drawingItem.item.type == static_cast<std::uint8_t>(NKWebKit::DisplayListAbi::ItemType::StrokePath));
    Require(_ZNK7WebCore4Path12elementCountEv(&item.path) == 2);
    _ZN7WebCore11DisplayList10StrokePathD1Ev(&item);
    _ZN7WebCore4PathD1Ev(&source);
}

void TestDrawPath() {
    Path source = TwoPointPath();
    alignas(8) unsigned char storage[sizeof(DrawPath)] = {};
    auto& item = *reinterpret_cast<DrawPath*>(storage);
    _ZN7WebCore11DisplayList8DrawPathC1ERKNS_4PathE(&item, &source);
    Require(item.drawingItem.item.type == static_cast<std::uint8_t>(NKWebKit::DisplayListAbi::ItemType::DrawPath));
    Require(_ZNK7WebCore4Path12elementCountEv(&item.path) == 2);
    _ZN7WebCore11DisplayList8DrawPathD1Ev(&item);
    _ZN7WebCore4PathD1Ev(&source);
}

void TestClipPath() {
    Path source = TwoPointPath();
    alignas(8) unsigned char storage[sizeof(ClipPath)] = {};
    auto& item = *reinterpret_cast<ClipPath*>(storage);
    _ZN7WebCore11DisplayList8ClipPathC1ERKNS_4PathENS_8WindRuleE(&item, &source, 1);
    Require(item.item.type == static_cast<std::uint8_t>(NKWebKit::DisplayListAbi::ItemType::ClipPath));
    Require(item.windRule == 1);
    Require(_ZNK7WebCore4Path12elementCountEv(&item.path) == 2);
    _ZN7WebCore11DisplayList8ClipPathD1Ev(&item);
    _ZN7WebCore4PathD1Ev(&source);
}

void TestClipOutToPath() {
    Path source = TwoPointPath();
    alignas(8) unsigned char storage[sizeof(ClipOutToPath)] = {};
    auto& item = *reinterpret_cast<ClipOutToPath*>(storage);
    _ZN7WebCore11DisplayList13ClipOutToPathC1ERKNS_4PathE(&item, &source);
    Require(item.item.type == static_cast<std::uint8_t>(NKWebKit::DisplayListAbi::ItemType::ClipOutToPath));
    Require(_ZNK7WebCore4Path12elementCountEv(&item.path) == 2);
    _ZN7WebCore11DisplayList13ClipOutToPathD1Ev(&item);
    _ZN7WebCore4PathD1Ev(&source);
}

void TestDrawFocusRingPath() {
    Path source = TwoPointPath();
    const WebCore::Color color{0x1234567890abcdefULL};
    alignas(8) unsigned char storage[sizeof(DrawFocusRingPath)] = {};
    auto& item = *reinterpret_cast<DrawFocusRingPath*>(storage);
    _ZN7WebCore11DisplayList17DrawFocusRingPathC1ERKNS_4PathEffRKNS_5ColorE(&item, &source, 2.5f, 1.5f, &color);
    Require(item.drawingItem.item.type == static_cast<std::uint8_t>(NKWebKit::DisplayListAbi::ItemType::DrawFocusRingPath));
    Require(item.width == 2.5f && item.offset == 1.5f);
    Require(item.color.m_colorAndFlags == color.m_colorAndFlags);
    Require(_ZNK7WebCore4Path12elementCountEv(&item.path) == 2);
    _ZN7WebCore11DisplayList17DrawFocusRingPathD1Ev(&item);
    _ZN7WebCore4PathD1Ev(&source);
}

void TestDeletingDestructor() {
    Path source = TwoPointPath();
    auto* item = static_cast<FillPath*>(_ZN3WTF10fastMallocEm(sizeof(FillPath)));
    _ZN7WebCore11DisplayList8FillPathC1ERKNS_4PathE(item, &source);
    _ZN7WebCore11DisplayList8FillPathD0Ev(item);
    _ZN7WebCore4PathD1Ev(&source);
}

void TestFaults() {
    bool threw = false;
    try {
        _ZN7WebCore11DisplayList8FillPathC1ERKNS_4PathE(nullptr, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        _ZN7WebCore11DisplayList8ClipPathD1Ev(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    try {
        TestFillPath();
        TestStrokePath();
        TestDrawPath();
        TestClipPath();
        TestClipOutToPath();
        TestDrawFocusRingPath();
        TestDeletingDestructor();
        TestFaults();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "DisplayList path item test failed: %s\n", error.what());
        return 1;
    }
    std::puts("DisplayList path item tests passed.");
    return 0;
}
