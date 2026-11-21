#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/DisplayListItemAbi.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::PathAbi {

struct PathElement {
    enum class Type : std::uint8_t {
        MoveToPoint,
        AddLineToPoint,
        AddQuadCurveToPoint,
        AddCurveToPoint,
        CloseSubpath,
    };

    WebCore::FloatPoint points[3];
    Type type;
};

struct Path {
    void* m_path;
    DisplayListAbi::Vector<PathElement> m_elements;
    bool m_elementsEngaged;
    unsigned char padding[7];
};

struct PathElementFunctionCallableWrapper;

struct PathElementFunctionCallableWrapperVtable {
    void (APS5_VABI* completeDestructor)(PathElementFunctionCallableWrapper* self);
    void (APS5_VABI* deletingDestructor)(PathElementFunctionCallableWrapper* self);
    void (APS5_VABI* call)(PathElementFunctionCallableWrapper* self, const PathElement* element);
};

struct PathElementFunctionCallableWrapper {
    const PathElementFunctionCallableWrapperVtable* vtable;
};

struct PathElementFunction {
    PathElementFunctionCallableWrapper* callableWrapper;
};

struct FillPath {
    DisplayListAbi::DrawingItem drawingItem;
    Path path;
};

struct StrokePath {
    DisplayListAbi::DrawingItem drawingItem;
    Path path;
};

struct DrawPath {
    DisplayListAbi::DrawingItem drawingItem;
    Path path;
};

struct ClipPath {
    DisplayListAbi::Item item;
    Path path;
    std::uint8_t windRule;
    unsigned char padding[7];
};

struct ClipOutToPath {
    DisplayListAbi::Item item;
    Path path;
};

struct DrawFocusRingPath {
    DisplayListAbi::DrawingItem drawingItem;
    Path path;
    float width;
    float offset;
    WebCore::Color color;
};

static_assert(std::is_standard_layout_v<PathElement>);
static_assert(sizeof(PathElement) == 28);
static_assert(alignof(PathElement) == 4);
static_assert(offsetof(PathElement, type) == 24);

static_assert(std::is_standard_layout_v<Path>);
static_assert(sizeof(Path) == 32);
static_assert(alignof(Path) == 8);
static_assert(offsetof(Path, m_path) == 0);
static_assert(offsetof(Path, m_elements) == 8);
static_assert(offsetof(Path, m_elementsEngaged) == 24);

static_assert(sizeof(PathElementFunctionCallableWrapperVtable) == 24);
static_assert(sizeof(PathElementFunctionCallableWrapper) == 8);
static_assert(sizeof(PathElementFunction) == 8);
static_assert(offsetof(PathElementFunction, callableWrapper) == 0);

static_assert(std::is_standard_layout_v<FillPath>);
static_assert(sizeof(FillPath) == 72);
static_assert(offsetof(FillPath, path) == 40);

static_assert(std::is_standard_layout_v<StrokePath>);
static_assert(sizeof(StrokePath) == 72);
static_assert(offsetof(StrokePath, path) == 40);

static_assert(std::is_standard_layout_v<DrawPath>);
static_assert(sizeof(DrawPath) == 72);
static_assert(offsetof(DrawPath, path) == 40);

static_assert(std::is_standard_layout_v<ClipPath>);
static_assert(sizeof(ClipPath) == 56);
static_assert(offsetof(ClipPath, path) == 16);
static_assert(offsetof(ClipPath, windRule) == 48);

static_assert(std::is_standard_layout_v<ClipOutToPath>);
static_assert(sizeof(ClipOutToPath) == 48);
static_assert(offsetof(ClipOutToPath, path) == 16);

static_assert(std::is_standard_layout_v<DrawFocusRingPath>);
static_assert(sizeof(DrawFocusRingPath) == 88);
static_assert(offsetof(DrawFocusRingPath, path) == 40);
static_assert(offsetof(DrawFocusRingPath, width) == 72);
static_assert(offsetof(DrawFocusRingPath, offset) == 76);
static_assert(offsetof(DrawFocusRingPath, color) == 80);

}  // namespace NKWebKit::PathAbi
