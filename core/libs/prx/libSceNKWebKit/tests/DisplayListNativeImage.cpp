#include "prx/libSceNKWebKit/include/DisplayListNativeImageExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <stdexcept>

namespace {

using NKWebKit::DisplayListAbi::DrawingItemVtable;
using NKWebKit::DisplayListAbi::ItemType;
using NKWebKit::DisplayListNativeImageAbi::DrawNativeImage;
using NKWebKit::DisplayListNativeImageAbi::ImagePaintingOptions;

void Require(const bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "DisplayList::DrawNativeImage check failed at line %u\n", location.line());
        std::abort();
    }
}

void Construct(DrawNativeImage* item, const void* image, const ImagePaintingOptions* options) {
    const WebCore::FloatSize imageSize{20.0f, 10.0f};
    const WebCore::FloatRect destination{{4.0f, 5.0f}, {8.0f, 6.0f}};
    const WebCore::FloatRect source{{1.0f, 2.0f}, {3.0f, 4.0f}};
    _ZN7WebCore11DisplayList15DrawNativeImageC1ERKN3WTF6RefPtrI14_cairo_surfaceNS2_13DumbPtrTraitsIS4_EEEERKNS_9FloatSizeERKNS_9FloatRectESF_RKNS_20ImagePaintingOptionsE(item, image, &imageSize, &destination, &source, options);
}

void TestItem() {
    const void* image = nullptr;
    const ImagePaintingOptions options{2, 2, 0, 0, 1, 0, {0, 0, 0}};
    alignas(8) unsigned char storage[sizeof(DrawNativeImage)];
    auto* item = reinterpret_cast<DrawNativeImage*>(storage);
    Construct(item, &image, &options);
    Require(item->drawingItem.item.type == static_cast<std::uint8_t>(ItemType::DrawNativeImage));
    Require(item->drawingItem.item.refCount == 1);
    Require(item->imageSize.m_width == 20.0f && item->imageSize.m_height == 10.0f);
    Require(item->destinationRect.x() == 4.0f && item->destinationRect.y() == 5.0f);
    Require(item->destinationRect.width() == 8.0f && item->destinationRect.height() == 6.0f);
    Require(item->sourceRect.x() == 1.0f && item->sourceRect.y() == 2.0f);
    Require(item->sourceRect.width() == 3.0f && item->sourceRect.height() == 4.0f);
    Require(item->options.compositeOperator == 2);
    Require(item->options.blendMode == 2);
    Require(item->options.orientation == 1);

    const auto* table = static_cast<const DrawingItemVtable*>(item->drawingItem.item.vptr);
    Require(table->item.isDrawingItem(item));
    table->item.apply(item, nullptr);
    const auto bounds = table->localBounds(item, nullptr);
    Require(bounds.engaged);
    Require(bounds.value.x() == 4.0f && bounds.value.y() == 5.0f);
    Require(bounds.value.width() == 8.0f && bounds.value.height() == 6.0f);
    Require(!table->globalBounds(item).engaged);
    _ZN7WebCore11DisplayList15DrawNativeImageD1Ev(item);

    auto* allocated = static_cast<DrawNativeImage*>(_ZN3WTF10fastMallocEm(sizeof(DrawNativeImage)));
    Construct(allocated, &image, &options);
    _ZN7WebCore11DisplayList15DrawNativeImageD0Ev(allocated);
}

void TestNull() {
    const ImagePaintingOptions options{2, 1, 0, 0, 1, 0, {0, 0, 0}};
    bool threw = false;
    try {
        Construct(nullptr, nullptr, &options);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    alignas(8) unsigned char storage[sizeof(DrawNativeImage)];
    try {
        Construct(reinterpret_cast<DrawNativeImage*>(storage), nullptr, &options);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    try {
        TestItem();
        TestNull();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "DisplayList::DrawNativeImage test failed: %s\n", error.what());
        return 1;
    }
    std::puts("DisplayList::DrawNativeImage tests passed.");
    return 0;
}
