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
using NKWebKit::DisplayListAbi::Save;
using NKWebKit::DisplayListAbi::Restore;
using NKWebKit::DisplayListAbi::Translate;
using NKWebKit::DisplayListAbi::Rotate;
using NKWebKit::DisplayListAbi::Scale;
using NKWebKit::DisplayListAbi::SetCTM;
using NKWebKit::DisplayListAbi::ConcatenateCTM;
using NKWebKit::DisplayListAbi::SetLineCap;
using NKWebKit::DisplayListAbi::SetLineDash;
using NKWebKit::DisplayListAbi::SetLineJoin;
using NKWebKit::DisplayListAbi::SetMiterLimit;
using NKWebKit::DisplayListAbi::ClearShadow;
using NKWebKit::DisplayListAbi::ApplyDeviceScaleFactor;
using NKWebKit::DisplayListAbi::Clip;
using NKWebKit::DisplayListAbi::ClipOut;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore display list state items check failed at line %d\n", line);
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
        case 0: {
            if (useC1) _ZN7WebCore11DisplayList4SaveC1Ev(static_cast<Save*>(storage));
            else _ZN7WebCore11DisplayList4SaveC2Ev(static_cast<Save*>(storage));
            break;
        }
        case 1: {
            if (useC1) _ZN7WebCore11DisplayList7RestoreC1Ev(static_cast<Restore*>(storage));
            else _ZN7WebCore11DisplayList7RestoreC2Ev(static_cast<Restore*>(storage));
            break;
        }
        case 2: {
            float x = reader.Float();
            float y = reader.Float();
            if (useC1) _ZN7WebCore11DisplayList9TranslateC1Eff(static_cast<Translate*>(storage), x, y);
            else _ZN7WebCore11DisplayList9TranslateC2Eff(static_cast<Translate*>(storage), x, y);
            break;
        }
        case 3: {
            float angle = reader.Float();
            if (useC1) _ZN7WebCore11DisplayList6RotateC1Ef(static_cast<Rotate*>(storage), angle);
            else _ZN7WebCore11DisplayList6RotateC2Ef(static_cast<Rotate*>(storage), angle);
            break;
        }
        case 4: {
            float sizeA = reader.Float();
            float sizeB = reader.Float();
            WebCore::FloatSize sizeValue{sizeA, sizeB};
            if (useC1) _ZN7WebCore11DisplayList5ScaleC1ERKNS_9FloatSizeE(static_cast<Scale*>(storage), &sizeValue);
            else _ZN7WebCore11DisplayList5ScaleC2ERKNS_9FloatSizeE(static_cast<Scale*>(storage), &sizeValue);
            break;
        }
        case 5: {
            double transform[6];
            for (auto& value : transform) value = reader.Double();
            WebCore::AffineTransform transformValue;
            for (int index = 0; index < 6; ++index) transformValue.m_transform[index] = transform[index];
            if (useC1) _ZN7WebCore11DisplayList6SetCTMC1ERKNS_15AffineTransformE(static_cast<SetCTM*>(storage), &transformValue);
            else _ZN7WebCore11DisplayList6SetCTMC2ERKNS_15AffineTransformE(static_cast<SetCTM*>(storage), &transformValue);
            break;
        }
        case 6: {
            double transform[6];
            for (auto& value : transform) value = reader.Double();
            WebCore::AffineTransform transformValue;
            for (int index = 0; index < 6; ++index) transformValue.m_transform[index] = transform[index];
            if (useC1) _ZN7WebCore11DisplayList14ConcatenateCTMC1ERKNS_15AffineTransformE(static_cast<ConcatenateCTM*>(storage), &transformValue);
            else _ZN7WebCore11DisplayList14ConcatenateCTMC2ERKNS_15AffineTransformE(static_cast<ConcatenateCTM*>(storage), &transformValue);
            break;
        }
        case 7: {
            int lineCap = reader.Enum(3);
            if (useC1) _ZN7WebCore11DisplayList10SetLineCapC1ENS_7LineCapE(static_cast<SetLineCap*>(storage), lineCap);
            else _ZN7WebCore11DisplayList10SetLineCapC2ENS_7LineCapE(static_cast<SetLineCap*>(storage), lineCap);
            break;
        }
        case 8: {
            auto dashArray = reader.Doubles();
            unsigned dashArrayCapacity = reader.Capacity(dashArray.size());
            float dashOffset = reader.Float();
            auto dashArrayValue = MakeVector(dashArray, dashArrayCapacity);
            if (useC1) _ZN7WebCore11DisplayList11SetLineDashC1ERKN3WTF6VectorIdLm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEf(static_cast<SetLineDash*>(storage), &dashArrayValue.vector, dashOffset);
            else _ZN7WebCore11DisplayList11SetLineDashC2ERKN3WTF6VectorIdLm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEf(static_cast<SetLineDash*>(storage), &dashArrayValue.vector, dashOffset);
            break;
        }
        case 9: {
            int lineJoin = reader.Enum(3);
            if (useC1) _ZN7WebCore11DisplayList11SetLineJoinC1ENS_8LineJoinE(static_cast<SetLineJoin*>(storage), lineJoin);
            else _ZN7WebCore11DisplayList11SetLineJoinC2ENS_8LineJoinE(static_cast<SetLineJoin*>(storage), lineJoin);
            break;
        }
        case 10: {
            float miterLimit = reader.Float();
            if (useC1) _ZN7WebCore11DisplayList13SetMiterLimitC1Ef(static_cast<SetMiterLimit*>(storage), miterLimit);
            else _ZN7WebCore11DisplayList13SetMiterLimitC2Ef(static_cast<SetMiterLimit*>(storage), miterLimit);
            break;
        }
        case 11: {
            if (useC1) _ZN7WebCore11DisplayList11ClearShadowC1Ev(static_cast<ClearShadow*>(storage));
            else _ZN7WebCore11DisplayList11ClearShadowC2Ev(static_cast<ClearShadow*>(storage));
            break;
        }
        case 12: {
            float scaleFactor = reader.Float();
            if (useC1) _ZN7WebCore11DisplayList22ApplyDeviceScaleFactorC1Ef(static_cast<ApplyDeviceScaleFactor*>(storage), scaleFactor);
            else _ZN7WebCore11DisplayList22ApplyDeviceScaleFactorC2Ef(static_cast<ApplyDeviceScaleFactor*>(storage), scaleFactor);
            break;
        }
        case 13: {
            auto rect = reader.Rect();
            WebCore::FloatRect rectValue = ToRect(rect);
            if (useC1) _ZN7WebCore11DisplayList4ClipC1ERKNS_9FloatRectE(static_cast<Clip*>(storage), &rectValue);
            else _ZN7WebCore11DisplayList4ClipC2ERKNS_9FloatRectE(static_cast<Clip*>(storage), &rectValue);
            break;
        }
        case 14: {
            auto rect = reader.Rect();
            WebCore::FloatRect rectValue = ToRect(rect);
            if (useC1) _ZN7WebCore11DisplayList7ClipOutC1ERKNS_9FloatRectE(static_cast<ClipOut*>(storage), &rectValue);
            else _ZN7WebCore11DisplayList7ClipOutC2ERKNS_9FloatRectE(static_cast<ClipOut*>(storage), &rectValue);
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
    {0, 16, false, false, -1, 0, -1, -1, {}},
    {1, 16, false, false, -1, 0, -1, -1, {}},
    {2, 24, false, false, -1, 0, -1, -1, {}},
    {3, 24, false, false, -1, 0, -1, -1, {}},
    {4, 24, false, false, -1, 0, -1, -1, {}},
    {5, 64, false, false, -1, 0, -1, -1, {}},
    {6, 64, false, false, -1, 0, -1, -1, {}},
    {7, 24, false, false, -1, 0, -1, -1, {}},
    {8, 40, false, false, 16, 8, -1, -1, {{16, 8}}},
    {9, 24, false, false, -1, 0, -1, -1, {}},
    {10, 24, false, false, -1, 0, -1, -1, {}},
    {11, 16, false, false, -1, 0, -1, -1, {}},
    {12, 24, false, false, -1, 0, -1, -1, {}},
    {13, 32, false, false, -1, 0, -1, -1, {}},
    {14, 32, false, false, -1, 0, -1, -1, {}},
};

const std::uint64_t ExpectedHashes[] = {
    0x8435c5cff70b7b02ull,
    0xf0cbae443ed40f8aull,
    0x2e4ab4520628671dull,
    0xe6dd5a3776dd3a15ull,
    0x966c1a8546581a86ull,
    0x47c95769a407d221ull,
    0xac9cd602f5e6bb99ull,
    0x3d4f8161eff4b93full,
    0x0088b90061c61481ull,
    0xbd49e977c0b38ed0ull,
    0xccf0c720868adb17ull,
    0xa558a77b7b7adca2ull,
    0xd29ce5bd4c9ca6f5ull,
    0x706618344803429bull,
    0xec78dc09e11d37f9ull,
    0xbb1e3444d107aa90ull,
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

void TestBaseItems() {
    alignas(16) unsigned char storage[64];
    std::memset(storage, 0xCD, sizeof(storage));
    auto* item = reinterpret_cast<Item*>(storage);
    _ZN7WebCore11DisplayList4ItemC2ENS0_8ItemTypeE(item, NKWebKit::DisplayListAbi::ItemType::ApplyDeviceScaleFactor);
    Require(item->refCount == 1 && item->type == 46 && storage[13] == 0xCD);
    const void* itemTable = item->vptr;
    Require(!VtableOf(item).isDrawingItem(item));
    Require(Throws<std::logic_error>([&] { VtableOf(item).apply(item, nullptr); }));
    _ZN7WebCore11DisplayList4ItemD1Ev(item);
    Require(item->vptr == itemTable);

    std::memset(storage, 0xCD, sizeof(storage));
    auto* drawing = reinterpret_cast<DrawingItem*>(storage);
    _ZN7WebCore11DisplayList11DrawingItemC2ENS0_8ItemTypeE(drawing, NKWebKit::DisplayListAbi::ItemType::FillRect);
    Require(drawing->item.type == 31 && drawing->item.vptr != itemTable);
    for (int offset = 16; offset < 40; ++offset) Require(storage[offset] == (offset == 16 || offset == 20 ? 0 : 0xCD));
    Require(VtableOf(drawing).isDrawingItem(drawing));
    Require(!DrawingVtableOf(drawing).localBounds(drawing, nullptr).engaged);
    Require(!DrawingVtableOf(drawing).globalBounds(drawing).engaged);
    _ZN7WebCore11DisplayList11DrawingItemD2Ev(drawing);
    Require(drawing->item.vptr == itemTable);

    const int before = guestAllocations;
    void* heapItem = _Znwm_nid_postfix(sizeof(DrawingItem));
    _ZN7WebCore11DisplayList11DrawingItemC2ENS0_8ItemTypeE(static_cast<DrawingItem*>(heapItem), NKWebKit::DisplayListAbi::ItemType::Save);
    _ZN7WebCore11DisplayList11DrawingItemD0Ev(static_cast<DrawingItem*>(heapItem));
    heapItem = _Znwm_nid_postfix(sizeof(Item));
    _ZN7WebCore11DisplayList4ItemC2ENS0_8ItemTypeE(static_cast<Item*>(heapItem), NKWebKit::DisplayListAbi::ItemType::Save);
    _ZN7WebCore11DisplayList4ItemD0Ev(static_cast<Item*>(heapItem));
    Require(guestAllocations == before);
}

void TestVectorCopy() {
    alignas(16) unsigned char storage[sizeof(SetLineDash)];
    auto* item = reinterpret_cast<SetLineDash*>(storage);
    OwnedVector<double> dashes({1.5, 2.5}, 5);
    const int before = guestAllocations;
    _ZN7WebCore11DisplayList11SetLineDashC1ERKN3WTF6VectorIdLm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEf(item, &dashes.vector, 0.5f);
    Require(item->dashArray.buffer != dashes.vector.buffer && item->dashArray.capacity == 5 && item->dashArray.size == 2);
    Require(item->dashArray.buffer[0] == 1.5 && item->dashArray.buffer[1] == 2.5 && item->dashOffset == 0.5f);
    _ZN7WebCore11DisplayList11SetLineDashD1Ev(item);
    Require(guestAllocations == before);
    OwnedVector<double> empty({}, 0);
    _ZN7WebCore11DisplayList11SetLineDashC2ERKN3WTF6VectorIdLm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEf(item, &empty.vector, 0);
    Require(!item->dashArray.buffer && !item->dashArray.capacity && !item->dashArray.size);
    _ZN7WebCore11DisplayList11SetLineDashD2Ev(item);
    std::memset(storage, 0xCD, sizeof(storage));
    Vector<double> broken{nullptr, 2, 1};
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList11SetLineDashC1ERKN3WTF6VectorIdLm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEf(item, &broken, 0); }));
    broken = {dashes.vector.buffer, 1, 2};
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList11SetLineDashC1ERKN3WTF6VectorIdLm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEf(item, &broken, 0); }));
    for (unsigned char byte : storage) Require(byte == 0xCD);
}

void TestNullArguments() {
    alignas(16) unsigned char storage[128];
    std::memset(storage, 0xCD, sizeof(storage));
    WebCore::FloatRect rect{};
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList9TranslateC1Eff(nullptr, 1, 2); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList5ScaleC1ERKNS_9FloatSizeE(reinterpret_cast<Scale*>(storage), nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList6SetCTMC2ERKNS_15AffineTransformE(reinterpret_cast<SetCTM*>(storage), nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList11SetLineDashC1ERKN3WTF6VectorIdLm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEf(reinterpret_cast<SetLineDash*>(storage), nullptr, 0); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList4ClipC1ERKNS_9FloatRectE(nullptr, &rect); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList4SaveD1Ev(nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList4ItemC2ENS0_8ItemTypeE(nullptr, NKWebKit::DisplayListAbi::ItemType::Save); }));
    for (unsigned char byte : storage) Require(byte == 0xCD);
}

}  // namespace

int main() {
    RegisterHeap();
    TestBaseItems();
    TestVectorCopy();
    TestNullArguments();
    TestEveryKind();
    TestOracleTranscripts();
    std::puts("WebCore display list state item tests passed");
    return 0;
}
