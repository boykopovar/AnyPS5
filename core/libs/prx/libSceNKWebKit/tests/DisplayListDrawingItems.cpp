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
using NKWebKit::DisplayListAbi::DocumentMarkerLineStyle;
using NKWebKit::DisplayListAbi::BeginTransparencyLayer;
using NKWebKit::DisplayListAbi::EndTransparencyLayer;
using NKWebKit::DisplayListAbi::DrawRect;
using NKWebKit::DisplayListAbi::DrawLine;
using NKWebKit::DisplayListAbi::DrawLinesForText;
using NKWebKit::DisplayListAbi::DrawDotsForDocumentMarker;
using NKWebKit::DisplayListAbi::DrawEllipse;
using NKWebKit::DisplayListAbi::FillRect;
using NKWebKit::DisplayListAbi::FillEllipse;
using NKWebKit::DisplayListAbi::StrokeRect;
using NKWebKit::DisplayListAbi::StrokeEllipse;
using NKWebKit::DisplayListAbi::ClearRect;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore display list drawing items check failed at line %d\n", line);
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


const ItemVtable& VtableOf(const void* item) {
    return *static_cast<const ItemVtable*>(static_cast<const Item*>(item)->vptr);
}

const DrawingItemVtable& DrawingVtableOf(const void* item) {
    return *static_cast<const DrawingItemVtable*>(static_cast<const Item*>(item)->vptr);
}

struct Adapter {
    std::uint64_t ExtendedRefCount() { return 1; }
    std::uint64_t ImageRefCount() { return 1; }
    void Construct(int kind, DisplayListDriver::Reader& reader, void* storage, bool useC1) {
        switch (kind) {
        case 15: {
            float opacity = reader.Float();
            if (useC1) _ZN7WebCore11DisplayList22BeginTransparencyLayerC1Ef(static_cast<BeginTransparencyLayer*>(storage), opacity);
            else _ZN7WebCore11DisplayList22BeginTransparencyLayerC2Ef(static_cast<BeginTransparencyLayer*>(storage), opacity);
            break;
        }
        case 16: {
            if (useC1) _ZN7WebCore11DisplayList20EndTransparencyLayerC1Ev(static_cast<EndTransparencyLayer*>(storage));
            else _ZN7WebCore11DisplayList20EndTransparencyLayerC2Ev(static_cast<EndTransparencyLayer*>(storage));
            break;
        }
        case 17: {
            auto rect = reader.Rect();
            float borderThickness = reader.Float();
            WebCore::FloatRect rectValue = ToRect(rect);
            if (useC1) _ZN7WebCore11DisplayList8DrawRectC1ERKNS_9FloatRectEf(static_cast<DrawRect*>(storage), &rectValue, borderThickness);
            else _ZN7WebCore11DisplayList8DrawRectC2ERKNS_9FloatRectEf(static_cast<DrawRect*>(storage), &rectValue, borderThickness);
            break;
        }
        case 18: {
            float point1A = reader.Float();
            float point1B = reader.Float();
            float point2A = reader.Float();
            float point2B = reader.Float();
            WebCore::FloatPoint point1Value{point1A, point1B};
            WebCore::FloatPoint point2Value{point2A, point2B};
            if (useC1) _ZN7WebCore11DisplayList8DrawLineC1ERKNS_10FloatPointES4_(static_cast<DrawLine*>(storage), &point1Value, &point2Value);
            else _ZN7WebCore11DisplayList8DrawLineC2ERKNS_10FloatPointES4_(static_cast<DrawLine*>(storage), &point1Value, &point2Value);
            break;
        }
        case 19: {
            float blockLocationA = reader.Float();
            float blockLocationB = reader.Float();
            float localAnchorA = reader.Float();
            float localAnchorB = reader.Float();
            float thickness = reader.Float();
            auto widths = reader.Doubles();
            unsigned widthsCapacity = reader.Capacity(widths.size());
            bool printing = reader.Bool();
            bool doubleLines = reader.Bool();
            WebCore::FloatPoint blockLocationValue{blockLocationA, blockLocationB};
            WebCore::FloatSize localAnchorValue{localAnchorA, localAnchorB};
            auto widthsValue = MakeVector(widths, widthsCapacity);
            if (useC1) _ZN7WebCore11DisplayList16DrawLinesForTextC1ERKNS_10FloatPointERKNS_9FloatSizeEfRKN3WTF6VectorIdLm0ENS8_15CrashOnOverflowELm16ENS8_10FastMallocEEEbb(static_cast<DrawLinesForText*>(storage), &blockLocationValue, &localAnchorValue, thickness, &widthsValue.vector, printing, doubleLines);
            else _ZN7WebCore11DisplayList16DrawLinesForTextC2ERKNS_10FloatPointERKNS_9FloatSizeEfRKN3WTF6VectorIdLm0ENS8_15CrashOnOverflowELm16ENS8_10FastMallocEEEbb(static_cast<DrawLinesForText*>(storage), &blockLocationValue, &localAnchorValue, thickness, &widthsValue.vector, printing, doubleLines);
            break;
        }
        case 20: {
            auto rect = reader.Rect();
            int styleMode = reader.Enum(5);
            bool styleDark = reader.Bool();
            WebCore::FloatRect rectValue = ToRect(rect);
            DocumentMarkerLineStyle styleValue{static_cast<std::uint8_t>(styleMode), styleDark};
            if (useC1) _ZN7WebCore11DisplayList25DrawDotsForDocumentMarkerC1ERKNS_9FloatRectENS_23DocumentMarkerLineStyleE(static_cast<DrawDotsForDocumentMarker*>(storage), &rectValue, styleValue);
            else _ZN7WebCore11DisplayList25DrawDotsForDocumentMarkerC2ERKNS_9FloatRectENS_23DocumentMarkerLineStyleE(static_cast<DrawDotsForDocumentMarker*>(storage), &rectValue, styleValue);
            break;
        }
        case 21: {
            auto rect = reader.Rect();
            WebCore::FloatRect rectValue = ToRect(rect);
            if (useC1) _ZN7WebCore11DisplayList11DrawEllipseC1ERKNS_9FloatRectE(static_cast<DrawEllipse*>(storage), &rectValue);
            else _ZN7WebCore11DisplayList11DrawEllipseC2ERKNS_9FloatRectE(static_cast<DrawEllipse*>(storage), &rectValue);
            break;
        }
        case 22: {
            auto rect = reader.Rect();
            WebCore::FloatRect rectValue = ToRect(rect);
            if (useC1) _ZN7WebCore11DisplayList8FillRectC1ERKNS_9FloatRectE(static_cast<FillRect*>(storage), &rectValue);
            else _ZN7WebCore11DisplayList8FillRectC2ERKNS_9FloatRectE(static_cast<FillRect*>(storage), &rectValue);
            break;
        }
        case 23: {
            auto rect = reader.Rect();
            WebCore::FloatRect rectValue = ToRect(rect);
            if (useC1) _ZN7WebCore11DisplayList11FillEllipseC1ERKNS_9FloatRectE(static_cast<FillEllipse*>(storage), &rectValue);
            else _ZN7WebCore11DisplayList11FillEllipseC2ERKNS_9FloatRectE(static_cast<FillEllipse*>(storage), &rectValue);
            break;
        }
        case 24: {
            auto rect = reader.Rect();
            float lineWidth = reader.Float();
            WebCore::FloatRect rectValue = ToRect(rect);
            if (useC1) _ZN7WebCore11DisplayList10StrokeRectC1ERKNS_9FloatRectEf(static_cast<StrokeRect*>(storage), &rectValue, lineWidth);
            else _ZN7WebCore11DisplayList10StrokeRectC2ERKNS_9FloatRectEf(static_cast<StrokeRect*>(storage), &rectValue, lineWidth);
            break;
        }
        case 25: {
            auto rect = reader.Rect();
            WebCore::FloatRect rectValue = ToRect(rect);
            if (useC1) _ZN7WebCore11DisplayList13StrokeEllipseC1ERKNS_9FloatRectE(static_cast<StrokeEllipse*>(storage), &rectValue);
            else _ZN7WebCore11DisplayList13StrokeEllipseC2ERKNS_9FloatRectE(static_cast<StrokeEllipse*>(storage), &rectValue);
            break;
        }
        case 26: {
            auto rect = reader.Rect();
            WebCore::FloatRect rectValue = ToRect(rect);
            if (useC1) _ZN7WebCore11DisplayList9ClearRectC1ERKNS_9FloatRectE(static_cast<ClearRect*>(storage), &rectValue);
            else _ZN7WebCore11DisplayList9ClearRectC2ERKNS_9FloatRectE(static_cast<ClearRect*>(storage), &rectValue);
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
    {15, 40, true, true, -1, 0, -1, -1, {}},
    {16, 40, true, true, -1, 0, -1, -1, {}},
    {17, 56, true, true, -1, 0, -1, -1, {}},
    {18, 56, true, true, -1, 0, -1, -1, {}},
    {19, 80, true, true, 56, 8, -1, -1, {{56, 8}}},
    {20, 56, true, true, -1, 0, -1, -1, {}},
    {21, 56, true, true, -1, 0, -1, -1, {}},
    {22, 56, true, true, -1, 0, -1, -1, {}},
    {23, 56, true, true, -1, 0, -1, -1, {}},
    {24, 56, true, true, -1, 0, -1, -1, {}},
    {25, 56, true, false, -1, 0, -1, -1, {}},
    {26, 56, true, true, -1, 0, -1, -1, {}},
};

const std::uint64_t ExpectedHashes[] = {
    0xefb2dade1575ed8bull,
    0x57381b6712ccea79ull,
    0x106504b002fdeb97ull,
    0xd8f447d8ed3d0586ull,
    0x5eb1c0693ffe987bull,
    0x3ae7d3f03f56f3c3ull,
    0x3e9c02adb0fd45c0ull,
    0xc5db6d2a9753139cull,
    0xa56b2cd863e8ccefull,
    0x46146083553545a3ull,
    0xd2e3589f1ab4ff60ull,
    0xf3db27dd54570f0bull,
    0x3ffb9a26d0ee559dull,
    0xadb33c4136a53074ull,
    0xf3f7614a559a7e99ull,
    0x65bf21deb5a862c0ull,
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

void TestStrokeEllipseBoundsNeedGraphicsContext() {
    alignas(16) unsigned char storage[sizeof(StrokeEllipse)];
    auto* item = reinterpret_cast<StrokeEllipse*>(storage);
    WebCore::FloatRect rect{{1, 2}, {3, 4}};
    _ZN7WebCore11DisplayList13StrokeEllipseC1ERKNS_9FloatRectE(item, &rect);
    Require(Throws<std::runtime_error>([&] { DrawingVtableOf(item).localBounds(item, nullptr); }));
    _ZN7WebCore11DisplayList13StrokeEllipseD1Ev(item);
}

void TestLinesForTextBounds() {
    alignas(16) unsigned char storage[sizeof(DrawLinesForText)];
    auto* item = reinterpret_cast<DrawLinesForText*>(storage);
    WebCore::FloatPoint location{10, 20};
    WebCore::FloatSize anchor{1, 2};
    OwnedVector<double> widths({4, 30}, 2);
    _ZN7WebCore11DisplayList16DrawLinesForTextC1ERKNS_10FloatPointERKNS_9FloatSizeEfRKN3WTF6VectorIdLm0ENS8_15CrashOnOverflowELm16ENS8_10FastMallocEEEbb(item, &location, &anchor, 2, &widths.vector, true, false);
    OptionalFloatRect bounds = DrawingVtableOf(item).localBounds(item, nullptr);
    Require(bounds.engaged && bounds.value.x() == 10 && bounds.value.y() == 21 && bounds.value.width() == 32 && bounds.value.height() == 4);
    Require(item->printing && !item->doubleLines && item->thickness == 2);
    _ZN7WebCore11DisplayList16DrawLinesForTextD1Ev(item);
    OwnedVector<double> empty({}, 0);
    _ZN7WebCore11DisplayList16DrawLinesForTextC1ERKNS_10FloatPointERKNS_9FloatSizeEfRKN3WTF6VectorIdLm0ENS8_15CrashOnOverflowELm16ENS8_10FastMallocEEEbb(item, &location, &anchor, 2, &empty.vector, false, true);
    bounds = DrawingVtableOf(item).localBounds(item, nullptr);
    Require(bounds.engaged && bounds.value.x() == 0 && bounds.value.y() == 0 && bounds.value.width() == 0 && bounds.value.height() == 0);
    _ZN7WebCore11DisplayList16DrawLinesForTextD1Ev(item);
}

void TestNullArguments() {
    alignas(16) unsigned char storage[128];
    std::memset(storage, 0xCD, sizeof(storage));
    WebCore::FloatPoint point{};
    WebCore::FloatSize size{};
    WebCore::FloatRect rect{};
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList8DrawLineC1ERKNS_10FloatPointES4_(reinterpret_cast<DrawLine*>(storage), &point, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList16DrawLinesForTextC1ERKNS_10FloatPointERKNS_9FloatSizeEfRKN3WTF6VectorIdLm0ENS8_15CrashOnOverflowELm16ENS8_10FastMallocEEEbb(reinterpret_cast<DrawLinesForText*>(storage), &point, &size, 1, nullptr, false, false); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList8DrawRectC2ERKNS_9FloatRectEf(nullptr, &rect, 1); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList8FillRectC1ERKNS_9FloatRectE(reinterpret_cast<FillRect*>(storage), nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList25DrawDotsForDocumentMarkerC1ERKNS_9FloatRectENS_23DocumentMarkerLineStyleE(reinterpret_cast<DrawDotsForDocumentMarker*>(storage), nullptr, DocumentMarkerLineStyle{}); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList20EndTransparencyLayerD0Ev(nullptr); }));
    for (unsigned char byte : storage) Require(byte == 0xCD);
}

}  // namespace

int main() {
    RegisterHeap();
    TestStrokeEllipseBoundsNeedGraphicsContext();
    TestLinesForTextBounds();
    TestNullArguments();
    TestEveryKind();
    TestOracleTranscripts();
    std::puts("WebCore display list drawing item tests passed");
    return 0;
}
