#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {

using NKWebKit::DisplayListAbi::Item;
using NKWebKit::DisplayListAbi::DrawingItem;
using NKWebKit::DisplayListAbi::ItemVtable;
using NKWebKit::DisplayListAbi::DrawingItemVtable;
using NKWebKit::DisplayListAbi::OptionalFloatRect;
using NKWebKit::DisplayListAbi::Vector;
using NKWebKit::DisplayListAbi::Color;
using NKWebKit::DisplayListAbi::ExtendedColor;
using NKWebKit::DisplayListAbi::Image;
using NKWebKit::DisplayListAbi::ImagePaintingOptions;
using NKWebKit::DisplayListAbi::FillRectWithColor;
using NKWebKit::DisplayListAbi::FillCompositedRect;
using NKWebKit::DisplayListAbi::FillRoundedRect;
using NKWebKit::DisplayListAbi::FillRectWithRoundedHole;
using NKWebKit::DisplayListAbi::DrawFocusRingRects;
using NKWebKit::DisplayListAbi::DrawImage;
using NKWebKit::DisplayListAbi::DrawTiledImage;
using NKWebKit::DisplayListAbi::DrawTiledScaledImage;
using NKWebKit::DisplayListAbi::DrawPattern;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore display list color and image items check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template<typename Exception, typename Action> bool Throws(Action action) {
    try {
        action();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

int guestAllocations;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) guestAllocations++;
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) guestAllocations--;
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    void* pointer = std::calloc(count, size);
    if (pointer) guestAllocations++;
    return pointer;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (!pointer) return Allocate_nid_no_patch(size);
    return std::realloc(pointer, size);
}

void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) {
    return Allocate_nid_no_patch(size);
}

void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) {
    return Reallocate_nid_no_patch(pointer, size);
}

int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

void RegisterHeap() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
}

namespace DisplayListDriver {

struct Hash {
    std::uint64_t value = 1469598103934665603ull;
    void Mix(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            value ^= (v >> (8 * i)) & 0xff;
            value *= 1099511628211ull;
        }
    }
    void MixBytes(const unsigned char* bytes, std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            value ^= bytes[i];
            value *= 1099511628211ull;
        }
    }
    void MixFloat(float f) {
        std::uint32_t bits;
        std::memcpy(&bits, &f, sizeof(bits));
        Mix(bits);
    }
};

struct Random {
    std::uint32_t state;
    int operator()(std::uint32_t bound) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<int>(state % bound);
    }
};

struct RectArg {
    float x, y, width, height;
};

struct ColorArg {
    int kind;
    std::uint64_t bits;
};

struct OptionsArg {
    int compositeOperator, blendMode, decodingMode, orientation, interpolationQuality;
};

struct Reader {
    Random& random;
    float Float() {
        switch (random(7)) {
        case 0: return 0.0f;
        case 1: return static_cast<float>(random(2001)) - 1000.0f;
        case 2: return (static_cast<float>(random(20001)) - 10000.0f) / 8.0f;
        case 3: return (static_cast<float>(random(1000001)) - 500000.0f) / 997.0f;
        case 4: return static_cast<float>(random(100));
        case 5: return -static_cast<float>(random(100)) / 3.0f;
        default: return static_cast<float>(random(65536)) * 1024.0f;
        }
    }
    double Double() {
        return static_cast<double>(Float()) / (1.0 + random(7));
    }
    RectArg Rect() {
        RectArg rect{Float(), Float(), Float(), Float()};
        if (random(4)) {
            rect.width = rect.width < 0 ? -rect.width : rect.width;
            rect.height = rect.height < 0 ? -rect.height : rect.height;
        }
        return rect;
    }
    int Enum(int count, int base = 0) {
        return base + random(count);
    }
    bool Bool() {
        return random(2);
    }
    std::vector<double> Doubles() {
        std::vector<double> values(random(4) ? random(9) : 0);
        for (auto& value : values) value = Double();
        return values;
    }
    std::vector<RectArg> Rects() {
        std::vector<RectArg> values(random(4) ? random(7) : 0);
        for (auto& value : values) value = Rect();
        return values;
    }
    unsigned Capacity(std::size_t size) {
        return static_cast<unsigned>(size + random(3) * random(5));
    }
    ColorArg Color() {
        switch (random(4)) {
        case 0: return {0, 1};
        case 1: return {1, (static_cast<std::uint64_t>(static_cast<std::uint32_t>(random(1 << 16)) << 16 | static_cast<std::uint32_t>(random(1 << 16))) << 32) | 3};
        case 2: return {1, (static_cast<std::uint64_t>(random(1 << 16)) << 32) | 7};
        default: return {2, 0};
        }
    }
    OptionsArg Options() {
        return {Enum(14), Enum(18, 1), Enum(3), Enum(9), Enum(5)};
    }
};

struct Mask {
    int offset;
    int length;
};

struct KindInfo {
    int kind;
    int size;
    bool drawing;
    bool bounded;
    int vectorOffset;
    int vectorElement;
    int colorOffset;
    int imageOffset;
    std::vector<Mask> masks;
};

struct Bounds {
    bool engaged;
    float x, y, width, height;
};

inline bool Masked(const KindInfo& info, int offset) {
    if (offset < 8) return true;
    for (const auto& mask : info.masks)
        if (offset >= mask.offset && offset < mask.offset + mask.length) return true;
    return false;
}

template<typename Adapter> std::uint64_t Run(Adapter& adapter, const std::vector<KindInfo>& kinds, std::uint32_t seed, int count) {
    Hash hash;
    Random random{seed ? seed : 1};
    alignas(16) unsigned char storage[256];
    for (int i = 0; i < count; ++i) {
        const KindInfo& info = kinds[random(static_cast<std::uint32_t>(kinds.size()))];
        std::memset(storage, 0xCD, sizeof(storage));
        Reader reader{random};
        adapter.Construct(info.kind, reader, storage, i & 1);
        hash.Mix(static_cast<std::uint64_t>(info.kind));
        for (int offset = 0; offset < 256; ++offset)
            if (offset >= info.size ? storage[offset] != 0xCD : !Masked(info, offset)) hash.Mix(static_cast<std::uint64_t>(offset) << 8 | storage[offset]);
        if (info.vectorOffset >= 0) {
            const unsigned char* buffer;
            std::uint32_t capacity;
            std::uint32_t size;
            std::memcpy(&buffer, storage + info.vectorOffset, sizeof(buffer));
            std::memcpy(&capacity, storage + info.vectorOffset + 8, sizeof(capacity));
            std::memcpy(&size, storage + info.vectorOffset + 12, sizeof(size));
            hash.Mix(capacity);
            hash.Mix(size);
            hash.Mix(buffer == nullptr);
            if (size) hash.MixBytes(buffer, static_cast<std::size_t>(size) * info.vectorElement);
        }
        if (info.colorOffset >= 0) {
            std::uint64_t bits;
            std::memcpy(&bits, storage + info.colorOffset, sizeof(bits));
            hash.Mix(bits & 1 ? bits : 2);
            hash.Mix(adapter.ExtendedRefCount());
        }
        if (info.imageOffset >= 0) hash.Mix(adapter.ImageRefCount());
        hash.Mix(adapter.IsDrawing(storage));
        if (info.bounded) {
            Bounds bounds = adapter.LocalBounds(storage);
            hash.Mix(bounds.engaged);
            if (bounds.engaged) {
                hash.MixFloat(bounds.x);
                hash.MixFloat(bounds.y);
                hash.MixFloat(bounds.width);
                hash.MixFloat(bounds.height);
            }
        }
        if (info.drawing) hash.Mix(adapter.GlobalBoundsEngaged(storage));
        adapter.Destroy(storage);
        hash.Mix(adapter.ExtendedRefCount());
        hash.Mix(adapter.ImageRefCount());
    }
    return hash.value;
}

}  // namespace DisplayListDriver

WebCore::FloatRect ToRect(const DisplayListDriver::RectArg& r) {
    return {{r.x, r.y}, {r.width, r.height}};
}

template<typename T> struct OwnedVector {
    Vector<T> vector{};
    OwnedVector(const OwnedVector&) = delete;
    OwnedVector(const std::vector<T>& values, unsigned capacity) {
        vector.capacity = capacity;
        vector.size = static_cast<std::uint32_t>(values.size());
        if (capacity) {
            vector.buffer = static_cast<T*>(_ZN3WTF10fastMallocEm(capacity * sizeof(T)));
            if (!values.empty()) std::memcpy(vector.buffer, values.data(), values.size() * sizeof(T));
        }
    }
    ~OwnedVector() {
        if (vector.buffer) _ZN3WTF8fastFreeEPv(vector.buffer);
    }
};

template<typename T> OwnedVector<T> MakeVector(const std::vector<T>& values, unsigned capacity) {
    return OwnedVector<T>(values, capacity);
}

int imageDeletes;

void APS5_VABI ImageCompleteDestructor(Image*) {
}

void APS5_VABI ImageDeletingDestructor(Image* image) {
    imageDeletes++;
    image->refCount = 0;
}

const void* const ImageVtable[] = {reinterpret_cast<const void*>(&ImageCompleteDestructor), reinterpret_cast<const void*>(&ImageDeletingDestructor)};

const ItemVtable& VtableOf(const void* item) {
    return *static_cast<const ItemVtable*>(static_cast<const Item*>(item)->vptr);
}

const DrawingItemVtable& DrawingVtableOf(const void* item) {
    return *static_cast<const DrawingItemVtable*>(static_cast<const Item*>(item)->vptr);
}

struct Adapter {
    ExtendedColor* extended = nullptr;
    Image image{ImageVtable, 1};
    Adapter() {
        extended = static_cast<ExtendedColor*>(_Znwm_nid_postfix(sizeof(ExtendedColor)));
        std::memset(extended, 0, sizeof(ExtendedColor));
        extended->refCount = 1;
    }
    ~Adapter() {
        _ZdlPv_nid_postfix(extended);
    }
    Color MakeColor(const DisplayListDriver::ColorArg& arg) {
        if (arg.kind == 2) return {static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(extended))};
        return {arg.bits};
    }
    std::uint64_t ExtendedRefCount() { return extended->refCount; }
    std::uint64_t ImageRefCount() { return image.refCount; }
    void Construct(int kind, DisplayListDriver::Reader& reader, void* storage, bool useC1) {
        switch (kind) {
        case 27: {
            auto rect = reader.Rect();
            auto color = reader.Color();
            WebCore::FloatRect rectValue = ToRect(rect);
            Color colorValue = MakeColor(color);
            if (useC1) _ZN7WebCore11DisplayList17FillRectWithColorC1ERKNS_9FloatRectERKNS_5ColorE(static_cast<FillRectWithColor*>(storage), &rectValue, &colorValue);
            else _ZN7WebCore11DisplayList17FillRectWithColorC2ERKNS_9FloatRectERKNS_5ColorE(static_cast<FillRectWithColor*>(storage), &rectValue, &colorValue);
            break;
        }
        case 28: {
            auto rect = reader.Rect();
            auto color = reader.Color();
            int op = reader.Enum(14);
            int blendMode = reader.Enum(18, 1);
            WebCore::FloatRect rectValue = ToRect(rect);
            Color colorValue = MakeColor(color);
            if (useC1) _ZN7WebCore11DisplayList18FillCompositedRectC1ERKNS_9FloatRectERKNS_5ColorENS_17CompositeOperatorENS_9BlendModeE(static_cast<FillCompositedRect*>(storage), &rectValue, &colorValue, static_cast<std::uint8_t>(op), static_cast<std::uint8_t>(blendMode));
            else _ZN7WebCore11DisplayList18FillCompositedRectC2ERKNS_9FloatRectERKNS_5ColorENS_17CompositeOperatorENS_9BlendModeE(static_cast<FillCompositedRect*>(storage), &rectValue, &colorValue, static_cast<std::uint8_t>(op), static_cast<std::uint8_t>(blendMode));
            break;
        }
        case 29: {
            auto rectRect = reader.Rect();
            float rectRadii[8];
            for (auto& value : rectRadii) value = reader.Float();
            auto color = reader.Color();
            int blendMode = reader.Enum(18, 1);
            WebCore::FloatRoundedRect rectValue{ToRect(rectRect), {{rectRadii[0], rectRadii[1]}, {rectRadii[2], rectRadii[3]}, {rectRadii[4], rectRadii[5]}, {rectRadii[6], rectRadii[7]}}};
            Color colorValue = MakeColor(color);
            if (useC1) _ZN7WebCore11DisplayList15FillRoundedRectC1ERKNS_16FloatRoundedRectERKNS_5ColorENS_9BlendModeE(static_cast<FillRoundedRect*>(storage), &rectValue, &colorValue, static_cast<std::uint8_t>(blendMode));
            else _ZN7WebCore11DisplayList15FillRoundedRectC2ERKNS_16FloatRoundedRectERKNS_5ColorENS_9BlendModeE(static_cast<FillRoundedRect*>(storage), &rectValue, &colorValue, static_cast<std::uint8_t>(blendMode));
            break;
        }
        case 30: {
            auto rect = reader.Rect();
            auto roundedHoleRectRect = reader.Rect();
            float roundedHoleRectRadii[8];
            for (auto& value : roundedHoleRectRadii) value = reader.Float();
            auto color = reader.Color();
            WebCore::FloatRect rectValue = ToRect(rect);
            WebCore::FloatRoundedRect roundedHoleRectValue{ToRect(roundedHoleRectRect), {{roundedHoleRectRadii[0], roundedHoleRectRadii[1]}, {roundedHoleRectRadii[2], roundedHoleRectRadii[3]}, {roundedHoleRectRadii[4], roundedHoleRectRadii[5]}, {roundedHoleRectRadii[6], roundedHoleRectRadii[7]}}};
            Color colorValue = MakeColor(color);
            if (useC1) _ZN7WebCore11DisplayList23FillRectWithRoundedHoleC1ERKNS_9FloatRectERKNS_16FloatRoundedRectERKNS_5ColorE(static_cast<FillRectWithRoundedHole*>(storage), &rectValue, &roundedHoleRectValue, &colorValue);
            else _ZN7WebCore11DisplayList23FillRectWithRoundedHoleC2ERKNS_9FloatRectERKNS_16FloatRoundedRectERKNS_5ColorE(static_cast<FillRectWithRoundedHole*>(storage), &rectValue, &roundedHoleRectValue, &colorValue);
            break;
        }
        case 31: {
            auto rects = reader.Rects();
            unsigned rectsCapacity = reader.Capacity(rects.size());
            float width = reader.Float();
            float offset = reader.Float();
            auto color = reader.Color();
            std::vector<WebCore::FloatRect> rectsRects;
            for (const auto& rect : rects) rectsRects.push_back(ToRect(rect));
            auto rectsValue = MakeVector(rectsRects, rectsCapacity);
            Color colorValue = MakeColor(color);
            if (useC1) _ZN7WebCore11DisplayList18DrawFocusRingRectsC1ERKN3WTF6VectorINS_9FloatRectELm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEffRKNS_5ColorE(static_cast<DrawFocusRingRects*>(storage), &rectsValue.vector, width, offset, &colorValue);
            else _ZN7WebCore11DisplayList18DrawFocusRingRectsC2ERKN3WTF6VectorINS_9FloatRectELm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEffRKNS_5ColorE(static_cast<DrawFocusRingRects*>(storage), &rectsValue.vector, width, offset, &colorValue);
            break;
        }
        case 32: {
            auto destination = reader.Rect();
            auto source = reader.Rect();
            auto imagePaintingOptions = reader.Options();
            WebCore::FloatRect destinationValue = ToRect(destination);
            WebCore::FloatRect sourceValue = ToRect(source);
            ImagePaintingOptions imagePaintingOptionsValue{};
            imagePaintingOptionsValue.compositeOperator = static_cast<std::uint8_t>(imagePaintingOptions.compositeOperator);
            imagePaintingOptionsValue.blendMode = static_cast<std::uint8_t>(imagePaintingOptions.blendMode);
            imagePaintingOptionsValue.decodingMode = static_cast<std::uint8_t>(imagePaintingOptions.decodingMode);
            imagePaintingOptionsValue.orientation = imagePaintingOptions.orientation;
            imagePaintingOptionsValue.interpolationQuality = static_cast<std::uint8_t>(imagePaintingOptions.interpolationQuality);
            if (useC1) _ZN7WebCore11DisplayList9DrawImageC1ERNS_5ImageERKNS_9FloatRectES6_RKNS_20ImagePaintingOptionsE(static_cast<DrawImage*>(storage), &image, &destinationValue, &sourceValue, &imagePaintingOptionsValue);
            else _ZN7WebCore11DisplayList9DrawImageC2ERNS_5ImageERKNS_9FloatRectES6_RKNS_20ImagePaintingOptionsE(static_cast<DrawImage*>(storage), &image, &destinationValue, &sourceValue, &imagePaintingOptionsValue);
            break;
        }
        case 33: {
            auto destination = reader.Rect();
            float sourceA = reader.Float();
            float sourceB = reader.Float();
            float tileSizeA = reader.Float();
            float tileSizeB = reader.Float();
            float spacingA = reader.Float();
            float spacingB = reader.Float();
            auto imagePaintingOptions = reader.Options();
            WebCore::FloatRect destinationValue = ToRect(destination);
            WebCore::FloatPoint sourceValue{sourceA, sourceB};
            WebCore::FloatSize tileSizeValue{tileSizeA, tileSizeB};
            WebCore::FloatSize spacingValue{spacingA, spacingB};
            ImagePaintingOptions imagePaintingOptionsValue{};
            imagePaintingOptionsValue.compositeOperator = static_cast<std::uint8_t>(imagePaintingOptions.compositeOperator);
            imagePaintingOptionsValue.blendMode = static_cast<std::uint8_t>(imagePaintingOptions.blendMode);
            imagePaintingOptionsValue.decodingMode = static_cast<std::uint8_t>(imagePaintingOptions.decodingMode);
            imagePaintingOptionsValue.orientation = imagePaintingOptions.orientation;
            imagePaintingOptionsValue.interpolationQuality = static_cast<std::uint8_t>(imagePaintingOptions.interpolationQuality);
            if (useC1) _ZN7WebCore11DisplayList14DrawTiledImageC1ERNS_5ImageERKNS_9FloatRectERKNS_10FloatPointERKNS_9FloatSizeESC_RKNS_20ImagePaintingOptionsE(static_cast<DrawTiledImage*>(storage), &image, &destinationValue, &sourceValue, &tileSizeValue, &spacingValue, &imagePaintingOptionsValue);
            else _ZN7WebCore11DisplayList14DrawTiledImageC2ERNS_5ImageERKNS_9FloatRectERKNS_10FloatPointERKNS_9FloatSizeESC_RKNS_20ImagePaintingOptionsE(static_cast<DrawTiledImage*>(storage), &image, &destinationValue, &sourceValue, &tileSizeValue, &spacingValue, &imagePaintingOptionsValue);
            break;
        }
        case 34: {
            auto destination = reader.Rect();
            auto source = reader.Rect();
            float tileScaleFactorA = reader.Float();
            float tileScaleFactorB = reader.Float();
            int hRule = reader.Enum(4);
            int vRule = reader.Enum(4);
            auto imagePaintingOptions = reader.Options();
            WebCore::FloatRect destinationValue = ToRect(destination);
            WebCore::FloatRect sourceValue = ToRect(source);
            WebCore::FloatSize tileScaleFactorValue{tileScaleFactorA, tileScaleFactorB};
            ImagePaintingOptions imagePaintingOptionsValue{};
            imagePaintingOptionsValue.compositeOperator = static_cast<std::uint8_t>(imagePaintingOptions.compositeOperator);
            imagePaintingOptionsValue.blendMode = static_cast<std::uint8_t>(imagePaintingOptions.blendMode);
            imagePaintingOptionsValue.decodingMode = static_cast<std::uint8_t>(imagePaintingOptions.decodingMode);
            imagePaintingOptionsValue.orientation = imagePaintingOptions.orientation;
            imagePaintingOptionsValue.interpolationQuality = static_cast<std::uint8_t>(imagePaintingOptions.interpolationQuality);
            if (useC1) _ZN7WebCore11DisplayList20DrawTiledScaledImageC1ERNS_5ImageERKNS_9FloatRectES6_RKNS_9FloatSizeENS2_8TileRuleESA_RKNS_20ImagePaintingOptionsE(static_cast<DrawTiledScaledImage*>(storage), &image, &destinationValue, &sourceValue, &tileScaleFactorValue, hRule, vRule, &imagePaintingOptionsValue);
            else _ZN7WebCore11DisplayList20DrawTiledScaledImageC2ERNS_5ImageERKNS_9FloatRectES6_RKNS_9FloatSizeENS2_8TileRuleESA_RKNS_20ImagePaintingOptionsE(static_cast<DrawTiledScaledImage*>(storage), &image, &destinationValue, &sourceValue, &tileScaleFactorValue, hRule, vRule, &imagePaintingOptionsValue);
            break;
        }
        case 35: {
            auto destination = reader.Rect();
            auto tileRect = reader.Rect();
            double patternTransform[6];
            for (auto& value : patternTransform) value = reader.Double();
            float phaseA = reader.Float();
            float phaseB = reader.Float();
            float spacingA = reader.Float();
            float spacingB = reader.Float();
            auto options = reader.Options();
            WebCore::FloatRect destinationValue = ToRect(destination);
            WebCore::FloatRect tileRectValue = ToRect(tileRect);
            WebCore::AffineTransform patternTransformValue;
            for (int index = 0; index < 6; ++index) patternTransformValue.m_transform[index] = patternTransform[index];
            WebCore::FloatPoint phaseValue{phaseA, phaseB};
            WebCore::FloatSize spacingValue{spacingA, spacingB};
            ImagePaintingOptions optionsValue{};
            optionsValue.compositeOperator = static_cast<std::uint8_t>(options.compositeOperator);
            optionsValue.blendMode = static_cast<std::uint8_t>(options.blendMode);
            optionsValue.decodingMode = static_cast<std::uint8_t>(options.decodingMode);
            optionsValue.orientation = options.orientation;
            optionsValue.interpolationQuality = static_cast<std::uint8_t>(options.interpolationQuality);
            if (useC1) _ZN7WebCore11DisplayList11DrawPatternC1ERNS_5ImageERKNS_9FloatRectES6_RKNS_15AffineTransformERKNS_10FloatPointERKNS_9FloatSizeERKNS_20ImagePaintingOptionsE(static_cast<DrawPattern*>(storage), &image, &destinationValue, &tileRectValue, &patternTransformValue, &phaseValue, &spacingValue, &optionsValue);
            else _ZN7WebCore11DisplayList11DrawPatternC2ERNS_5ImageERKNS_9FloatRectES6_RKNS_15AffineTransformERKNS_10FloatPointERKNS_9FloatSizeERKNS_20ImagePaintingOptionsE(static_cast<DrawPattern*>(storage), &image, &destinationValue, &tileRectValue, &patternTransformValue, &phaseValue, &spacingValue, &optionsValue);
            break;
        }
        default: std::abort();
        }
    }
    bool IsDrawing(void* storage) { return VtableOf(storage).isDrawingItem(storage); }
    DisplayListDriver::Bounds LocalBounds(void* storage) {
        OptionalFloatRect bounds = DrawingVtableOf(storage).localBounds(storage, nullptr);
        if (!bounds.engaged) return {false, 0, 0, 0, 0};
        return {true, bounds.value.x(), bounds.value.y(), bounds.value.width(), bounds.value.height()};
    }
    bool GlobalBoundsEngaged(void* storage) { return DrawingVtableOf(storage).globalBounds(storage).engaged; }
    void Destroy(void* storage) { VtableOf(storage).completeDestructor(storage); }
};

const std::vector<DisplayListDriver::KindInfo> Kinds = {
    {27, 64, true, true, -1, 0, 56, -1, {{56, 8}}},
    {28, 72, true, true, -1, 0, 56, -1, {{56, 8}}},
    {29, 104, true, true, -1, 0, 88, -1, {{88, 8}}},
    {30, 112, true, true, -1, 0, 104, -1, {{104, 8}}},
    {31, 72, true, true, 40, 16, 64, -1, {{40, 8}, {64, 8}}},
    {32, 96, true, true, -1, 0, -1, 40, {{40, 8}, {83, 1}, {89, 3}}},
    {33, 104, true, true, -1, 0, -1, 40, {{40, 8}, {91, 1}, {97, 3}}},
    {34, 112, true, true, -1, 0, -1, 40, {{40, 8}, {99, 1}, {105, 3}}},
    {35, 160, true, true, -1, 0, -1, 40, {{40, 8}, {147, 1}, {153, 3}}},
};

const std::uint64_t ExpectedHashes[] = {
    0x2933b0bd75ad2635ull,
    0x49b99e1e9dc91c9full,
    0xdf2d922f0d7273c2ull,
    0x3a71ff42013f468dull,
    0x83fdf968fe56d98full,
    0x09355fe23929777cull,
    0x350bbc9abdbce9f3ull,
    0x3d08b3b999f7373cull,
    0x5240fc75410528bbull,
    0x3bc57082936b0968ull,
    0xaf67d490804c2daaull,
    0x7699efdfbad60e09ull,
    0xe20c4125daf33e72ull,
    0x4f63e3effe31af28ull,
    0x14bd2e708ca1f09cull,
    0x313278d1d9820332ull,
};

void TestOracleTranscripts() {
    for (std::size_t seed = 0; seed < std::size(ExpectedHashes); ++seed) {
        Adapter adapter;
        Require(DisplayListDriver::Run(adapter, Kinds, static_cast<std::uint32_t>(seed + 1), 500) == ExpectedHashes[seed]);
    }
}

const void* ItemTablePointer() {
    alignas(16) unsigned char storage[sizeof(Item)];
    _ZN7WebCore11DisplayList4ItemC2ENS0_8ItemTypeE(reinterpret_cast<Item*>(storage), NKWebKit::DisplayListAbi::ItemType::Save);
    return reinterpret_cast<Item*>(storage)->vptr;
}

void TestEveryKind() {
    const void* itemTable = ItemTablePointer();
    for (const auto& info : Kinds) {
        Adapter adapter;
        DisplayListDriver::Random random{static_cast<std::uint32_t>(info.kind + 1)};
        DisplayListDriver::Reader reader{random};
        const int before = guestAllocations;
        void* storage = _Znwm_nid_postfix(static_cast<std::size_t>(info.size));
        adapter.Construct(info.kind, reader, storage, true);
        const Item& item = *static_cast<Item*>(storage);
        Require(item.refCount == 1);
        Require(VtableOf(storage).isDrawingItem(storage) == info.drawing);
        Require(Throws<std::runtime_error>([&] { VtableOf(storage).apply(storage, nullptr); }));
        if (info.kind % 2) {
            VtableOf(storage).deletingDestructor(storage);
        } else {
            VtableOf(storage).completeDestructor(storage);
            Require(item.vptr == itemTable);
            _ZdlPv_nid_postfix(storage);
        }
        Require(guestAllocations == before);
    }
}

void TestLastReferences() {
    const int before = guestAllocations;
    alignas(16) unsigned char storage[sizeof(DrawImage)];
    auto* colorItem = reinterpret_cast<FillRectWithColor*>(storage);
    auto* extended = static_cast<ExtendedColor*>(_Znwm_nid_postfix(sizeof(ExtendedColor)));
    std::memset(extended, 0, sizeof(ExtendedColor));
    extended->refCount = 1;
    Color color{static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(extended))};
    WebCore::FloatRect rect{{1, 2}, {3, 4}};
    _ZN7WebCore11DisplayList17FillRectWithColorC1ERKNS_9FloatRectERKNS_5ColorE(colorItem, &rect, &color);
    Require(extended->refCount == 2);
    extended->refCount = 1;
    _ZN7WebCore11DisplayList17FillRectWithColorD1Ev(colorItem);
    Require(guestAllocations == before);

    imageDeletes = 0;
    Image image{ImageVtable, 1};
    ImagePaintingOptions options{};
    auto* imageItem = reinterpret_cast<DrawImage*>(storage);
    _ZN7WebCore11DisplayList9DrawImageC1ERNS_5ImageERKNS_9FloatRectES6_RKNS_20ImagePaintingOptionsE(imageItem, &image, &rect, &rect, &options);
    Require(image.refCount == 2 && imageItem->image == &image);
    _ZN7WebCore11DisplayList9DrawImageD2Ev(imageItem);
    Require(image.refCount == 1 && imageDeletes == 0);
    _ZN7WebCore11DisplayList9DrawImageC1ERNS_5ImageERKNS_9FloatRectES6_RKNS_20ImagePaintingOptionsE(imageItem, &image, &rect, &rect, &options);
    image.refCount = 1;
    _ZN7WebCore11DisplayList9DrawImageD1Ev(imageItem);
    Require(imageDeletes == 1);
}

void TestFocusRingBounds() {
    alignas(16) unsigned char storage[sizeof(DrawFocusRingRects)];
    auto* item = reinterpret_cast<DrawFocusRingRects*>(storage);
    OwnedVector<WebCore::FloatRect> rects({WebCore::FloatRect{{0, 0}, {10, 10}}, WebCore::FloatRect{{5, -5}, {0, 40}}, WebCore::FloatRect{{20, 20}, {5, 5}}}, 4);
    Color color{3};
    _ZN7WebCore11DisplayList18DrawFocusRingRectsC1ERKN3WTF6VectorINS_9FloatRectELm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEffRKNS_5ColorE(item, &rects.vector, 1, 2, &color);
    OptionalFloatRect bounds = DrawingVtableOf(item).localBounds(item, nullptr);
    Require(bounds.engaged && bounds.value.x() == -3 && bounds.value.y() == -3 && bounds.value.width() == 31 && bounds.value.height() == 31);
    _ZN7WebCore11DisplayList18DrawFocusRingRectsD1Ev(item);
}

void TestNullArguments() {
    alignas(16) unsigned char storage[192];
    std::memset(storage, 0xCD, sizeof(storage));
    WebCore::FloatRect rect{};
    Color color{1};
    Color nullExtended{0};
    ImagePaintingOptions options{};
    Image image{ImageVtable, 1};
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList17FillRectWithColorC1ERKNS_9FloatRectERKNS_5ColorE(reinterpret_cast<FillRectWithColor*>(storage), &rect, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList17FillRectWithColorC1ERKNS_9FloatRectERKNS_5ColorE(reinterpret_cast<FillRectWithColor*>(storage), &rect, &nullExtended); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList9DrawImageC1ERNS_5ImageERKNS_9FloatRectES6_RKNS_20ImagePaintingOptionsE(reinterpret_cast<DrawImage*>(storage), nullptr, &rect, &rect, &options); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList9DrawImageC1ERNS_5ImageERKNS_9FloatRectES6_RKNS_20ImagePaintingOptionsE(reinterpret_cast<DrawImage*>(storage), &image, &rect, &rect, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList15FillRoundedRectC2ERKNS_16FloatRoundedRectERKNS_5ColorENS_9BlendModeE(reinterpret_cast<FillRoundedRect*>(storage), nullptr, &color, 1); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList18DrawFocusRingRectsC1ERKN3WTF6VectorINS_9FloatRectELm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEffRKNS_5ColorE(reinterpret_cast<DrawFocusRingRects*>(storage), nullptr, 1, 1, &color); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList17FillRectWithColorC1ERKNS_9FloatRectERKNS_5ColorE(nullptr, &rect, &color); }));
    Require(image.refCount == 1);
    for (unsigned char byte : storage) Require(byte == 0xCD);
}

}  // namespace

int main() {
    RegisterHeap();
    TestLastReferences();
    TestFocusRingBounds();
    TestNullArguments();
    TestEveryKind();
    TestOracleTranscripts();
    std::puts("WebCore display list color and image item tests passed");
    return 0;
}
