#pragma once

#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

enum class CursorType : std::uint32_t {
    Pointer = 0,
    Cross,
    Hand,
    IBeam,
    Wait,
    Help,
    EastResize,
    NorthResize,
    NorthEastResize,
    NorthWestResize,
    SouthResize,
    SouthEastResize,
    SouthWestResize,
    WestResize,
    NorthSouthResize,
    EastWestResize,
    NorthEastSouthWestResize,
    NorthWestSouthEastResize,
    ColumnResize,
    RowResize,
    MiddlePanning,
    EastPanning,
    NorthPanning,
    NorthEastPanning,
    NorthWestPanning,
    SouthPanning,
    SouthEastPanning,
    SouthWestPanning,
    WestPanning,
    Move,
    VerticalText,
    Cell,
    ContextMenu,
    Alias,
    Progress,
    NoDrop,
    Copy,
    None,
    NotAllowed,
    ZoomIn,
    ZoomOut,
    Grab,
    Grabbing,
    Custom
};

inline constexpr std::uint32_t StandardCursorCount = static_cast<std::uint32_t>(CursorType::Custom);
inline constexpr std::uint32_t CursorTypeValueCount = std::bit_ceil(StandardCursorCount + 1);

struct Cursor {
    CursorType type{static_cast<CursorType>(-1)};
    void* image{nullptr};
    WebCore::IntPoint hotSpot{};
    void* platformCursor{nullptr};
};

static_assert(StandardCursorCount == 43);
static_assert(CursorTypeValueCount == 64);
static_assert(sizeof(CursorType) == 4);
static_assert(sizeof(Cursor) == 32);
static_assert(alignof(Cursor) == 8);
static_assert(offsetof(Cursor, type) == 0);
static_assert(offsetof(Cursor, image) == 8);
static_assert(offsetof(Cursor, hotSpot) == 16);
static_assert(offsetof(Cursor, platformCursor) == 24);
static_assert(std::is_standard_layout_v<Cursor>);

const Cursor& StandardCursor(CursorType type);

}
