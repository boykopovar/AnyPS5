#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

using NKWebKit::DisplayListAbi::DrawGlyphs;
using NKWebKit::DisplayListAbi::DrawingItemVtable;
using NKWebKit::DisplayListAbi::GlyphAdvanceVector;
using NKWebKit::DisplayListAbi::GlyphVector;
using NKWebKit::DisplayListAbi::InlineVector;
using NKWebKit::DisplayListAbi::Item;
using NKWebKit::DisplayListAbi::ItemVtable;
using NKWebKit::DisplayListAbi::OptionalFloatRect;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore display list DrawGlyphs check failed at line %d\n", line);
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

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    return std::malloc(size);
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    return std::calloc(count, size);
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    return std::realloc(pointer, size);
}

void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) {
    return std::malloc(size);
}

void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) {
    return std::realloc(pointer, size);
}

int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

void RegisterHeap() {
    std::array<void*, 10> heapApi{
        reinterpret_cast<void*>(&Allocate_nid_no_patch),
        reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch),
        reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch),
        reinterpret_cast<void*>(&Realign_nid_no_patch),
        reinterpret_cast<void*>(&PosixAlign_nid_no_patch),
    };
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
}

template<typename T, std::size_t inlineCapacity>
void FillInline(InlineVector<T, inlineCapacity>& vector, const std::vector<T>& values) {
    Require(values.size() <= inlineCapacity);
    vector.buffer = reinterpret_cast<T*>(vector.inlineStorage);
    vector.capacity = static_cast<std::uint32_t>(inlineCapacity);
    vector.size = static_cast<std::uint32_t>(values.size());
    if (!values.empty()) std::memcpy(vector.inlineStorage, values.data(), values.size() * sizeof(T));
}

template<typename T, std::size_t inlineCapacity>
void FillHeap(InlineVector<T, inlineCapacity>& vector, const std::vector<T>& values, std::uint32_t capacity) {
    Require(capacity > inlineCapacity && values.size() <= capacity);
    vector.buffer = static_cast<T*>(_ZN3WTF10fastMallocEm(static_cast<std::size_t>(capacity) * sizeof(T)));
    vector.capacity = capacity;
    vector.size = static_cast<std::uint32_t>(values.size());
    if (!values.empty()) std::memcpy(vector.buffer, values.data(), values.size() * sizeof(T));
}

template<typename T, std::size_t inlineCapacity> bool IsEmptyInline(const InlineVector<T, inlineCapacity>& vector) {
    return vector.buffer == reinterpret_cast<const T*>(vector.inlineStorage) && vector.capacity == inlineCapacity && !vector.size;
}

const ItemVtable& VtableOf(const void* item) {
    return *static_cast<const ItemVtable*>(static_cast<const Item*>(item)->vptr);
}

const DrawingItemVtable& DrawingVtableOf(const void* item) {
    return *static_cast<const DrawingItemVtable*>(static_cast<const Item*>(item)->vptr);
}

WebCore::FloatPoint Point(float x, float y) {
    return {x, y};
}

WebCore::FloatSize Size(float width, float height) {
    return {width, height};
}

const std::vector<unsigned short> Glyphs{3, 17, 65535, 0};
const std::vector<WebCore::FloatSize> Advances{Size(1.5f, 2.5f), Size(-3.25f, 4.0f), Size(0.5f, -0.75f), Size(1000.0f, 0.125f)};

struct FontStorage {
    alignas(8) unsigned char bytes[64];
};

void TestInlineMove() {
    alignas(16) unsigned char storage[sizeof(DrawGlyphs)];
    std::memset(storage, 0xCD, sizeof(storage));
    auto* item = reinterpret_cast<DrawGlyphs*>(storage);
    FontStorage font;
    GlyphVector glyphs;
    GlyphAdvanceVector advances;
    FillInline(glyphs, Glyphs);
    FillInline(advances, Advances);
    const WebCore::FloatPoint blockLocation = Point(40.0f, -12.5f);
    const WebCore::FloatSize localAnchor = Size(2.0f, 3.0f);
    _ZN7WebCore11DisplayList10DrawGlyphsC1ERKNS_4FontEON3WTF6VectorItLm128ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEEONS6_INS_9FloatSizeELm128ES7_Lm16ES8_EERKNS_10FloatPointERKSB_NS_17FontSmoothingModeE(
        item, reinterpret_cast<const WebCore::Font*>(font.bytes), &glyphs, &advances, &blockLocation, &localAnchor, 1);
    Require(item->item.refCount == 1 && item->item.type == 17);
    Require(!item->extent.engaged);
    Require(item->font == reinterpret_cast<const WebCore::Font*>(font.bytes));
    Require(item->glyphs.size == Glyphs.size() && item->glyphs.capacity == 128);
    Require(item->glyphs.buffer == reinterpret_cast<const unsigned short*>(item->glyphs.inlineStorage));
    Require(std::memcmp(item->glyphs.buffer, Glyphs.data(), Glyphs.size() * sizeof(unsigned short)) == 0);
    Require(item->advances.size == Advances.size() && item->advances.capacity == 128);
    Require(item->advances.buffer == reinterpret_cast<const WebCore::FloatSize*>(item->advances.inlineStorage));
    Require(std::memcmp(item->advances.buffer, Advances.data(), Advances.size() * sizeof(WebCore::FloatSize)) == 0);
    Require(IsEmptyInline(glyphs) && IsEmptyInline(advances));
    Require(item->blockLocation.m_x == 40.0f && item->blockLocation.m_y == -12.5f);
    Require(item->localAnchor.m_width == 2.0f && item->localAnchor.m_height == 3.0f);
    Require(item->smoothingMode == 1);
    Require(item->bounds.x() == 0.0f && item->bounds.y() == 0.0f && item->bounds.width() == 0.0f && item->bounds.height() == 0.0f);
    Require(DrawingVtableOf(item).item.isDrawingItem(item));
    Require(Throws<std::runtime_error>([&] { VtableOf(item).apply(item, nullptr); }));
    OptionalFloatRect bounds = DrawingVtableOf(item).localBounds(item, nullptr);
    Require(bounds.engaged && bounds.value.x() == 40.0f && bounds.value.y() == -12.5f);
    Require(bounds.value.width() == 0.0f && bounds.value.height() == 0.0f);
    Require(!DrawingVtableOf(item).globalBounds(item).engaged);
    _ZN7WebCore11DisplayList10DrawGlyphsD1Ev(item);
    Require(IsEmptyInline(item->glyphs) && IsEmptyInline(item->advances));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList10DrawGlyphsD1Ev(nullptr); }));
}

void TestHeapMove() {
    alignas(16) unsigned char storage[sizeof(DrawGlyphs)];
    std::memset(storage, 0xCD, sizeof(storage));
    auto* item = reinterpret_cast<DrawGlyphs*>(storage);
    FontStorage font;
    GlyphVector glyphs;
    GlyphAdvanceVector advances;
    FillHeap(glyphs, Glyphs, 200);
    FillHeap(advances, Advances, 300);
    const unsigned short* glyphBuffer = glyphs.buffer;
    const WebCore::FloatSize* advanceBuffer = advances.buffer;
    const WebCore::FloatPoint blockLocation = Point(0.0f, 0.0f);
    const WebCore::FloatSize localAnchor = Size(0.0f, 0.0f);
    _ZN7WebCore11DisplayList10DrawGlyphsC2ERKNS_4FontEON3WTF6VectorItLm128ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEEONS6_INS_9FloatSizeELm128ES7_Lm16ES8_EERKNS_10FloatPointERKSB_NS_17FontSmoothingModeE(
        item, reinterpret_cast<const WebCore::Font*>(font.bytes), &glyphs, &advances, &blockLocation, &localAnchor, 0);
    Require(item->glyphs.buffer == glyphBuffer && item->glyphs.capacity == 200 && item->glyphs.size == Glyphs.size());
    Require(item->advances.buffer == advanceBuffer && item->advances.capacity == 300 && item->advances.size == Advances.size());
    Require(std::memcmp(item->glyphs.buffer, Glyphs.data(), Glyphs.size() * sizeof(unsigned short)) == 0);
    Require(std::memcmp(item->advances.buffer, Advances.data(), Advances.size() * sizeof(WebCore::FloatSize)) == 0);
    Require(IsEmptyInline(glyphs) && IsEmptyInline(advances));
    Require(item->smoothingMode == 0);
    _ZN7WebCore11DisplayList10DrawGlyphsD2Ev(item);
    Require(IsEmptyInline(item->glyphs) && IsEmptyInline(item->advances));
}

void TestDeletingDestructor() {
    void* storage = _ZN3WTF10fastMallocEm(sizeof(DrawGlyphs));
    auto* item = static_cast<DrawGlyphs*>(storage);
    FontStorage font;
    GlyphVector glyphs;
    GlyphAdvanceVector advances;
    FillHeap(glyphs, Glyphs, 200);
    FillInline(advances, Advances);
    const WebCore::FloatPoint blockLocation = Point(1.0f, 2.0f);
    const WebCore::FloatSize localAnchor = Size(3.0f, 4.0f);
    _ZN7WebCore11DisplayList10DrawGlyphsC1ERKNS_4FontEON3WTF6VectorItLm128ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEEONS6_INS_9FloatSizeELm128ES7_Lm16ES8_EERKNS_10FloatPointERKSB_NS_17FontSmoothingModeE(
        item, reinterpret_cast<const WebCore::Font*>(font.bytes), &glyphs, &advances, &blockLocation, &localAnchor, 2);
    Require(item->item.type == 17 && item->smoothingMode == 2);
    VtableOf(item).deletingDestructor(item);
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11DisplayList10DrawGlyphsD0Ev(nullptr); }));
}

void TestReuseAfterDestroy() {
    alignas(16) unsigned char storage[sizeof(DrawGlyphs)];
    std::memset(storage, 0xCD, sizeof(storage));
    auto* item = reinterpret_cast<DrawGlyphs*>(storage);
    FontStorage font;
    const WebCore::FloatPoint blockLocation = Point(5.0f, 6.0f);
    const WebCore::FloatSize localAnchor = Size(7.0f, 8.0f);
    GlyphVector first;
    GlyphAdvanceVector firstAdvances;
    FillHeap(first, Glyphs, 150);
    FillInline(firstAdvances, Advances);
    _ZN7WebCore11DisplayList10DrawGlyphsC1ERKNS_4FontEON3WTF6VectorItLm128ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEEONS6_INS_9FloatSizeELm128ES7_Lm16ES8_EERKNS_10FloatPointERKSB_NS_17FontSmoothingModeE(
        item, reinterpret_cast<const WebCore::Font*>(font.bytes), &first, &firstAdvances, &blockLocation, &localAnchor, 0);
    _ZN7WebCore11DisplayList10DrawGlyphsD1Ev(item);
    GlyphVector second;
    GlyphAdvanceVector secondAdvances;
    FillInline(second, Glyphs);
    FillHeap(secondAdvances, Advances, 132);
    _ZN7WebCore11DisplayList10DrawGlyphsC2ERKNS_4FontEON3WTF6VectorItLm128ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEEONS6_INS_9FloatSizeELm128ES7_Lm16ES8_EERKNS_10FloatPointERKSB_NS_17FontSmoothingModeE(
        item, reinterpret_cast<const WebCore::Font*>(font.bytes), &second, &secondAdvances, &blockLocation, &localAnchor, 2);
    Require(item->item.refCount == 1 && item->item.type == 17 && item->smoothingMode == 2);
    Require(item->glyphs.size == Glyphs.size() && item->advances.size == Advances.size());
    Require(std::memcmp(item->glyphs.buffer, Glyphs.data(), Glyphs.size() * sizeof(unsigned short)) == 0);
    Require(std::memcmp(item->advances.buffer, Advances.data(), Advances.size() * sizeof(WebCore::FloatSize)) == 0);
    Require(item->blockLocation.m_x == 5.0f && item->localAnchor.m_height == 8.0f);
    _ZN7WebCore11DisplayList10DrawGlyphsD1Ev(item);
}

void TestNullArguments() {
    alignas(16) unsigned char storage[sizeof(DrawGlyphs)];
    std::memset(storage, 0xCD, sizeof(storage));
    auto* item = reinterpret_cast<DrawGlyphs*>(storage);
    FontStorage font;
    GlyphVector glyphs;
    GlyphAdvanceVector advances;
    FillInline(glyphs, Glyphs);
    FillInline(advances, Advances);
    const WebCore::FloatPoint blockLocation = Point(0.0f, 0.0f);
    const WebCore::FloatSize localAnchor = Size(0.0f, 0.0f);
    const auto* fontPointer = reinterpret_cast<const WebCore::Font*>(font.bytes);
    const auto construct = [&](DrawGlyphs* self, const WebCore::Font* fontArgument, GlyphVector* glyphArgument,
                                GlyphAdvanceVector* advanceArgument, const WebCore::FloatPoint* blockArgument,
                                const WebCore::FloatSize* anchorArgument) {
        _ZN7WebCore11DisplayList10DrawGlyphsC1ERKNS_4FontEON3WTF6VectorItLm128ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEEONS6_INS_9FloatSizeELm128ES7_Lm16ES8_EERKNS_10FloatPointERKSB_NS_17FontSmoothingModeE(
            self, fontArgument, glyphArgument, advanceArgument, blockArgument, anchorArgument, 0);
    };
    Require(Throws<std::invalid_argument>([&] { construct(nullptr, fontPointer, &glyphs, &advances, &blockLocation, &localAnchor); }));
    Require(Throws<std::invalid_argument>([&] { construct(item, nullptr, &glyphs, &advances, &blockLocation, &localAnchor); }));
    Require(Throws<std::invalid_argument>([&] { construct(item, fontPointer, nullptr, &advances, &blockLocation, &localAnchor); }));
    Require(Throws<std::invalid_argument>([&] { construct(item, fontPointer, &glyphs, nullptr, &blockLocation, &localAnchor); }));
    Require(Throws<std::invalid_argument>([&] { construct(item, fontPointer, &glyphs, &advances, nullptr, &localAnchor); }));
    Require(Throws<std::invalid_argument>([&] { construct(item, fontPointer, &glyphs, &advances, &blockLocation, nullptr); }));
    for (unsigned char byte : storage) Require(byte == 0xCD);
    Require(glyphs.size == Glyphs.size() && glyphs.capacity == 128);
    Require(advances.size == Advances.size() && advances.capacity == 128);
}

void TestInvalidVectors() {
    alignas(16) unsigned char storage[sizeof(DrawGlyphs)];
    std::memset(storage, 0xCD, sizeof(storage));
    auto* item = reinterpret_cast<DrawGlyphs*>(storage);
    FontStorage font;
    GlyphVector glyphs;
    GlyphAdvanceVector advances;
    FillInline(glyphs, Glyphs);
    FillInline(advances, Advances);
    const WebCore::FloatPoint blockLocation = Point(0.0f, 0.0f);
    const WebCore::FloatSize localAnchor = Size(0.0f, 0.0f);
    const auto* fontPointer = reinterpret_cast<const WebCore::Font*>(font.bytes);
    const auto construct = [&] {
        _ZN7WebCore11DisplayList10DrawGlyphsC1ERKNS_4FontEON3WTF6VectorItLm128ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEEONS6_INS_9FloatSizeELm128ES7_Lm16ES8_EERKNS_10FloatPointERKSB_NS_17FontSmoothingModeE(
            item, fontPointer, &glyphs, &advances, &blockLocation, &localAnchor, 0);
    };
    glyphs.size = 129;
    Require(Throws<std::invalid_argument>(construct));
    glyphs.size = static_cast<std::uint32_t>(Glyphs.size());
    unsigned short* inlineBuffer = glyphs.buffer;
    glyphs.buffer = static_cast<unsigned short*>(_ZN3WTF10fastMallocEm(8));
    Require(Throws<std::invalid_argument>(construct));
    _ZN3WTF8fastFreeEPv(glyphs.buffer);
    glyphs.buffer = nullptr;
    Require(Throws<std::invalid_argument>(construct));
    glyphs.buffer = inlineBuffer;
    glyphs.size = 0;
    glyphs.capacity = 0;
    Require(Throws<std::invalid_argument>(construct));
    glyphs.capacity = 128;
    glyphs.size = static_cast<std::uint32_t>(Glyphs.size());
    advances.capacity = 127;
    Require(Throws<std::invalid_argument>(construct));
    advances.capacity = 200;
    Require(Throws<std::invalid_argument>(construct));
    advances.capacity = 128;
    advances.buffer = nullptr;
    Require(Throws<std::invalid_argument>(construct));
    advances.buffer = reinterpret_cast<WebCore::FloatSize*>(advances.inlineStorage);
    for (unsigned char byte : storage) Require(byte == 0xCD);
    Require(glyphs.size == Glyphs.size() && advances.size == Advances.size());
    construct();
    Require(item->item.type == 17 && item->glyphs.size == Glyphs.size() && item->advances.size == Advances.size());
    _ZN7WebCore11DisplayList10DrawGlyphsD1Ev(item);
}

void TestEmptyVectors() {
    alignas(16) unsigned char storage[sizeof(DrawGlyphs)];
    std::memset(storage, 0xCD, sizeof(storage));
    auto* item = reinterpret_cast<DrawGlyphs*>(storage);
    FontStorage font;
    GlyphVector glyphs;
    GlyphAdvanceVector advances;
    FillInline(glyphs, {});
    FillInline(advances, {});
    const WebCore::FloatPoint blockLocation = Point(0.0f, 0.0f);
    const WebCore::FloatSize localAnchor = Size(0.0f, 0.0f);
    _ZN7WebCore11DisplayList10DrawGlyphsC1ERKNS_4FontEON3WTF6VectorItLm128ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEEONS6_INS_9FloatSizeELm128ES7_Lm16ES8_EERKNS_10FloatPointERKSB_NS_17FontSmoothingModeE(
        item, reinterpret_cast<const WebCore::Font*>(font.bytes), &glyphs, &advances, &blockLocation, &localAnchor, 0);
    Require(!item->glyphs.size && item->glyphs.capacity == 128 && IsEmptyInline(glyphs));
    Require(!item->advances.size && item->advances.capacity == 128 && IsEmptyInline(advances));
    _ZN7WebCore11DisplayList10DrawGlyphsD2Ev(item);
}

}  // namespace

int main() {
    RegisterHeap();
    TestInlineMove();
    TestHeapMove();
    TestDeletingDestructor();
    TestReuseAfterDestroy();
    TestNullArguments();
    TestInvalidVectors();
    TestEmptyVectors();
    std::puts("WebCore display list DrawGlyphs tests passed");
    return 0;
}
