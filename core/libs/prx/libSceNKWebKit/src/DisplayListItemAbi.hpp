#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace NKWebKit::DisplayListAbi {

enum class ItemType : std::uint8_t {
    Save,
    Restore,
    Translate,
    Rotate,
    Scale,
    ConcatenateCTM,
    SetCTM,
    SetState,
    SetLineCap,
    SetLineDash,
    SetLineJoin,
    SetMiterLimit,
    ClearShadow,
    Clip,
    ClipOut,
    ClipOutToPath,
    ClipPath,
    DrawGlyphs,
    DrawImage,
    DrawTiledImage,
    DrawTiledScaledImage,
    DrawNativeImage,
    DrawPattern,
    DrawRect,
    DrawLine,
    DrawLinesForText,
    DrawDotsForDocumentMarker,
    DrawEllipse,
    DrawPath,
    DrawFocusRingPath,
    DrawFocusRingRects,
    FillRect,
    FillRectWithColor,
    FillRectWithGradient,
    FillCompositedRect,
    FillRoundedRect,
    FillRectWithRoundedHole,
    FillPath,
    FillEllipse,
    PutImageData,
    StrokeRect,
    StrokePath,
    StrokeEllipse,
    ClearRect,
    BeginTransparencyLayer,
    EndTransparencyLayer,
    ApplyDeviceScaleFactor,
};

template<typename T> struct Vector {
    T* buffer;
    std::uint32_t capacity;
    std::uint32_t size;
};

struct OptionalFloatRect {
    bool engaged;
    std::uint8_t padding[3];
    WebCore::FloatRect value;
};

struct ItemVtable {
    void (APS5_VABI* completeDestructor)(void* self);
    void (APS5_VABI* deletingDestructor)(void* self);
    void (APS5_VABI* apply)(const void* self, void* context);
    bool (APS5_VABI* isDrawingItem)(const void* self);
};

struct DrawingItemVtable {
    ItemVtable item;
    OptionalFloatRect (APS5_VABI* localBounds)(const void* self, const void* context);
    OptionalFloatRect (APS5_VABI* globalBounds)(const void* self);
};

struct Item {
    const void* vptr;
    std::uint32_t refCount;
    std::uint8_t type;
    std::uint8_t padding[3];
};

struct DrawingItem {
    Item item;
    OptionalFloatRect extent;
};

static_assert(std::is_standard_layout_v<Vector<double>>);
static_assert(sizeof(Vector<double>) == 16);
static_assert(std::is_standard_layout_v<OptionalFloatRect>);
static_assert(sizeof(OptionalFloatRect) == 20);
static_assert(offsetof(OptionalFloatRect, value) == 4);
static_assert(std::is_standard_layout_v<Item>);
static_assert(sizeof(Item) == 16);
static_assert(offsetof(Item, refCount) == 8);
static_assert(offsetof(Item, type) == 12);
static_assert(std::is_standard_layout_v<DrawingItem>);
static_assert(sizeof(DrawingItem) == 40);
static_assert(offsetof(DrawingItem, extent) == 16);

template<typename T> T& Checked(T* value, const char* function) {
    if (!value) throw std::invalid_argument(std::string(function) + " requires an object");
    return *value;
}

const ItemVtable* ItemTable();
const DrawingItemVtable* DrawingItemTable();
void ConstructItem(Item& item, const void* vtable, ItemType type);
void ConstructDrawingItem(Item& item, OptionalFloatRect& extent, const void* vtable, ItemType type);
void DestroyItem(Item& item);
void DeleteItem(void* item);
OptionalFloatRect NoBounds();
OptionalFloatRect Bounds(const WebCore::FloatRect& rect);
bool APS5_VABI IsNotDrawingItem(const void* self);
bool APS5_VABI IsDrawingItem(const void* self);
void APS5_VABI ApplyWithoutGraphicsContext(const void* self, void* context);
OptionalFloatRect APS5_VABI DefaultLocalBounds(const void* self, const void* context);
OptionalFloatRect APS5_VABI DefaultGlobalBounds(const void* self);
void CheckVectorStorage(const void* vector, const char* function);
void CopyVectorStorage(void* to, const void* from, std::size_t elementSize, const char* function);
void DestroyVectorStorage(void* vector);

template<typename T> void CheckVector(const Vector<T>* vector, const char* function) {
    CheckVectorStorage(&Checked(vector, function), function);
}

template<typename T> void CopyVector(Vector<T>& to, const Vector<T>* from, const char* function) {
    CopyVectorStorage(&to, &Checked(from, function), sizeof(T), function);
}

template<typename T> void DestroyVector(Vector<T>& vector) {
    DestroyVectorStorage(&vector);
}

}  // namespace NKWebKit::DisplayListAbi
